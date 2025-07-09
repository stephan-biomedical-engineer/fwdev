#include "hal_gps.h"
#include "gps.h"    
#include "hal_uart.h"
#include <string.h>
#include <assert.h>
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

static ssize_t nmea_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    size_t idx = 0;
    while (idx + 1 < max_len) {
        uint8_t c;
        if (hal_uart_read(ctx->uart_dev, &c, 1) != 1) break;
        sentence[idx++] = c;
        if (c == '\n') break;
    }
    if (idx == 0) return 0;
    sentence[idx] = '\0';
    // Decodifica a sentença e atualiza tpv
    gps_decode(&ctx->tpv, sentence);
    return idx;
}

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
