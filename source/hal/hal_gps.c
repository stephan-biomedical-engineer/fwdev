#include "hal_gps.h"
#include "gps.h"    
#include "hal_uart.h"
#include "utl_dbg.h"
#include "utl_printf.h"
#include "hal_cpu.h"
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Internally, usamos o mesmo handle para UART e GPS
typedef struct {
    hal_uart_dev_t uart_dev;
    struct gps_tpv tpv;
} hal_gps_context_t;

static const hal_gps_driver_t *s_driver = NULL;

// Funções do driver NMEA usando gps.c + HAL UART
static void nmea_init(void) {
    // Inicialização global, nada especial aqui
}

static void nmea_deinit(void) {
    // Cleanup global, nada especial
}

static hal_gps_dev_t nmea_open(hal_gps_port_t port, const hal_gps_config_t *cfg) {
    hal_uart_config_t ucfg;
    // Mapeia baud rates
    ucfg.baud_rate = (hal_uart_baud_rate_t)cfg->baud_rate;
    ucfg.parity = HAL_UART_PARITY_NONE;
    ucfg.stop_bits = HAL_UART_STOP_BITS_1;
    ucfg.flow_control = HAL_UART_FLOW_CONTROL_NONE;
    ucfg.interrupt_callback = NULL;

    hal_uart_dev_t uart = hal_uart_open((hal_uart_port_t)port, &ucfg);
    if (!uart) return NULL;

    hal_gps_context_t *ctx = malloc(sizeof(*ctx));
    assert(ctx);
    ctx->uart_dev = uart;
    gps_init_tpv(&ctx->tpv);
    return (hal_gps_dev_t)ctx;
}

static void nmea_close(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    hal_uart_close(ctx->uart_dev);
    free(ctx);
}

static hal_gps_position_request_t nmea_request_position(hal_gps_dev_t dev,
                                                        hal_gps_position_request_t req) {
    // Exemplo: não implementado, usamos polling de sentenças via read_sentence
    return req;
}

static hal_gps_timestamp_t nmea_get_timestamp(hal_gps_dev_t dev,
                                              hal_gps_timestamp_t type) {
    // Sempre retorna UTC pois gps_decode preenche tpv.time em UTC
    return HAL_GPS_TIMESTAMP_UTC;
}

static hal_gps_fix_quality_t nmea_get_fix_quality(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    switch (ctx->tpv.mode) {
        case GPS_MODE_2D_FIX: return HAL_GPS_FIX_QUALITY_2D;
        case GPS_MODE_3D_FIX: return HAL_GPS_FIX_QUALITY_3D;
        default:              return HAL_GPS_FIX_QUALITY_NONE;
    }
}

static size_t nmea_bytes_available(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    return hal_uart_bytes_available(ctx->uart_dev);
}

static ssize_t nmea_read(hal_gps_dev_t dev, uint8_t *buffer, size_t size) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    return hal_uart_read(ctx->uart_dev, buffer, size);
}

static ssize_t nmea_write(hal_gps_dev_t dev, const uint8_t *buffer, size_t size) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    return hal_uart_write(ctx->uart_dev, (uint8_t*)buffer, size);
}

static void nmea_flush(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    hal_uart_flush(ctx->uart_dev);
}

// static ssize_t nmea_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
//     hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
//     size_t idx = 0;
//     while (idx + 1 < max_len) {
//         uint8_t c;
//         if (hal_uart_read(ctx->uart_dev, &c, 1) != 1) break;
//         sentence[idx++] = c;
//         if (c == '\n') break;
//     }
//     if (idx == 0) return 0;
//     sentence[idx] = '\0';
//     // Decodifica a sentença e atualiza tpv
//     gps_decode(&ctx->tpv, sentence);
//     hal_gps_fix_quality_t fq = hal_gps_get_fix_quality(dev);
//     printf("Fix quality: %d\n", fq);
//     return idx;
// }

// static ssize_t nmea_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
//     hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
//     size_t idx = 0;
//     uint8_t c;

//     // Espera começar com $
//     while (hal_uart_read(ctx->uart_dev, &c, 1) == 1 && c != '$');

//     if (c != '$') return 0;

//     sentence[idx++] = c;

//     // Continua até \n
//     while (idx + 1 < max_len) {
//         if (hal_uart_read(ctx->uart_dev, &c, 1) != 1) break;
//         sentence[idx++] = c;
//         if (c == '\n') break;
//     }

//     sentence[idx] = '\0';
//     gps_decode(&ctx->tpv, sentence);
//     // printf("Fix quality: %d\n", hal_gps_get_fix_quality(dev));
//     UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Fix quality: %d\n", hal_gps_get_fix_quality(dev));
//     return idx;
// }

static ssize_t nmea_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    size_t idx = 0;
    uint8_t c;
    bool found_start_char = false;

    const uint32_t SENTENCE_READ_TIMEOUT_MS = 500;
    uint32_t start_time_ms = hal_cpu_time_get_ms();

    while (idx + 1 < max_len) {
        if (hal_cpu_time_get_ms() - start_time_ms > SENTENCE_READ_TIMEOUT_MS) {
            printf("nmea_read_sentence: Timeout occurred.\n");
            break; // Exit loop on timeout
        }

        ssize_t bytes_read = hal_uart_read(ctx->uart_dev, &c, 1);

        if (bytes_read == 1) { // Character successfully read
            start_time_ms = hal_cpu_time_get_ms(); // Reset timeout

            if (!found_start_char) {
                if (c == '$') {
                    found_start_char = true;
                    // Add the '$' character and proceed to read the rest of the sentence
                    sentence[idx++] = c;
                }
                // If not '$' and still haven't found '$', just continue (discard char)
                // This 'continue' correctly discards junk BEFORE the '$'
                continue; 
            }

            // If found_start_char is true, we always add the character
            sentence[idx++] = c;

            // If the current character is '\n', the sentence is complete
            if (c == '\n') {
                break; // Sentence complete, exit loop
            }
        } else if (bytes_read == 0) { // No byte read currently
            // Wait a bit if no bytes are available
            hal_cpu_sleep_ms(1);
        } else { // bytes_read < 0 (read error)
            printf("nmea_read_sentence: UART read error.\n");
            break;
        }
    }

    sentence[idx] = '\0'; // Null-terminate the string

    // If we didn't find the start character or read nothing, return 0
    if (!found_start_char || idx == 0) {
        return 0;
    }

    // Faça uma cópia da sentença NMEA antes de passá-la para gps_decode
    // pois gps_decode modifica a string de entrada
    char sentence_copy[max_len]; 
    strncpy(sentence_copy, sentence, max_len);
    sentence_copy[max_len - 1] = '\0'; // Garantir nul-terminação

    // Decodifica a sentença usando a biblioteca GPS
    // Passe a cópia para gps_decode

    int decode_result = gps_decode(&ctx->tpv, sentence_copy); // <--- Use a cópia aqui
    
    // Decode the sentence using the GPS library
    // int decode_result = gps_decode(&ctx->tpv, sentence);
    
    printf("Fix quality: %d\n", hal_gps_get_fix_quality(dev));
    printf("GPS Decode Result: %d (%s)\n", decode_result);
    
    return idx; // Return the number of bytes read
}

    // sentence[idx] = '\0';

    // // Se não encontramos o caractere de início ou nada foi lido, retorna 0
    // if (!found_start_char || idx == 0) {
    //     return 0;
    // }

    // // NENHUMA CÓPIA TEMPORÁRIA AQUI. 'sentence' é a string lida e NUL-terminada.
    // // gps_decode vai modificar 'sentence' in-place.
    // int decode_result = gps_decode(&ctx->tpv, sentence); // <--- Passe 'sentence' diretamente

    // printf("Fix quality: %d\n", hal_gps_get_fix_quality(dev));
    // printf("GPS Decode Result: %d (%s)\n", decode_result, gps_error_string(decode_result));

    // return idx;
// }


// Driver instância
static const hal_gps_driver_t nmea_driver = {
    .init            = nmea_init,
    .deinit          = nmea_deinit,
    .open            = nmea_open,
    .close           = nmea_close,
    .request_position= nmea_request_position,
    .get_timestamp   = nmea_get_timestamp,
    .get_fix_quality = nmea_get_fix_quality,
    .bytes_available = nmea_bytes_available,
    .read            = nmea_read,
    .write           = nmea_write,
    .flush           = nmea_flush,
    .read_sentence   = nmea_read_sentence
};

// API pública
void hal_gps_init(void) {
    s_driver = &nmea_driver;
    s_driver->init();
}

void hal_gps_deinit(void) {
    s_driver->deinit();
    s_driver = NULL;
}

hal_gps_dev_t hal_gps_open(hal_gps_port_t port, const hal_gps_config_t *cfg) {
    assert(s_driver);
    return s_driver->open(port, cfg);
}

void hal_gps_close(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    s_driver->close(dev);
}

size_t hal_gps_bytes_available(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    return s_driver->bytes_available(dev);
}

ssize_t hal_gps_read(hal_gps_dev_t dev, uint8_t *buffer, size_t size) {
    assert(s_driver && dev);
    return s_driver->read(dev, buffer, size);
}

ssize_t hal_gps_write(hal_gps_dev_t dev, const uint8_t *buffer, size_t size) {
    assert(s_driver && dev);
    return s_driver->write(dev, buffer, size);
}

void hal_gps_flush(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    s_driver->flush(dev);
}

ssize_t hal_gps_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
    assert(s_driver && dev);
    return s_driver->read_sentence(dev, sentence, max_len);
}

hal_gps_fix_quality_t hal_gps_get_fix_quality(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    return s_driver->get_fix_quality(dev);
}

