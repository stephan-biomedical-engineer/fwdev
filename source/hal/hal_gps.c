/**
 * @file hal_gps.c
 * @brief Implementação da camada de abstração de hardware (HAL) para módulos GPS.
 *
 * Este arquivo implementa o driver HAL para GPS, que atua como uma ponte entre
 * a camada de aplicação e a biblioteca de parsing NMEA (gps.c), utilizando
 * a camada HAL UART para a comunicação serial. Ele gerencia o ciclo de vida
 * do dispositivo GPS, incluindo abertura, fechamento, leitura/escrita de sentenças
 * e acesso a dados de posição e fix.
 */

#include "hal_gps.h"
#include "gps.h"    
#include "hal_uart.h"
#include "utl_dbg.h"
#include "utl_printf.h"
#include "hal_cpu.h"
#include <string.h>
#include <assert.h>
#include <unistd.h> // Incluído para sleep, embora não seja ideal em HALs de produção
#include <stdio.h>  // Incluído para printf de debug
#include <stdlib.h> // Incluído para malloc/free
#include <stdbool.h>

/**
 * @brief Contexto interno para cada instância de dispositivo GPS.
 *
 * Este contexto armazena o handle da UART associada e a estrutura de dados TPV
 * decodificada pela biblioteca GPS.
 */
typedef struct {
    hal_uart_dev_t uart_dev;    /**< Handle para o dispositivo UART associado. */
    struct gps_tpv tpv;         /**< Estrutura de dados Time-Position-Velocity (TPV) do GPS. */
} hal_gps_context_t;

/** @brief Ponteiro estático para a instância do driver GPS atualmente registrado. */
static const hal_gps_driver_t *s_driver = NULL;

/**
 * @brief Inicializa o backend do driver NMEA.
 *
 * Esta função é chamada uma vez durante a inicialização global da HAL GPS.
 * Atualmente, não realiza inicializações específicas, mas pode ser estendida
 * para configurar recursos globais do driver.
 */
static void nmea_init(void) {
    // Inicialização global, nada especial aqui
}

/**
 * @brief Desinicializa o backend do driver NMEA.
 *
 * Esta função é chamada durante a desinicialização global da HAL GPS.
 * Atualmente, não realiza desinicializações específicas, mas pode ser estendida
 * para liberar recursos globais do driver.
 */
static void nmea_deinit(void) {
    // Cleanup global, nada especial
}

/**
 * @brief Abre uma interface GPS específica e a configura.
 *
 * Esta função aloca e inicializa um contexto para a porta GPS especificada,
 * abrindo o dispositivo UART subjacente e inicializando a estrutura TPV.
 *
 * @param port A porta GPS a ser aberta (ex: HAL_GPS_PORT0).
 * @param cfg Ponteiro para a estrutura de configuração do GPS.
 * @return Um handle opaco para o dispositivo GPS, ou NULL em caso de erro.
 */
static hal_gps_dev_t nmea_open(hal_gps_port_t port, const hal_gps_config_t *cfg) {
    hal_uart_config_t ucfg;
    // Mapeia baud rates da HAL GPS para a HAL UART
    ucfg.baud_rate = (hal_uart_baud_rate_t)cfg->baud_rate;
    ucfg.parity = HAL_UART_PARITY_NONE;
    ucfg.stop_bits = HAL_UART_STOP_BITS_1;
    ucfg.flow_control = HAL_UART_FLOW_CONTROL_NONE;
    ucfg.interrupt_callback = NULL; // Callbacks de interrupção não usados neste modelo de polling

    hal_uart_dev_t uart = hal_uart_open((hal_uart_port_t)port, &ucfg);
    if (!uart) return NULL;

    hal_gps_context_t *ctx = malloc(sizeof(*ctx));
    assert(ctx); // Asserção para garantir alocação de memória
    if (!ctx) { // Verificação defensiva caso a asserção não esteja ativa
        hal_uart_close(uart);
        return NULL;
    }
    ctx->uart_dev = uart;
    gps_init_tpv(&ctx->tpv); // Inicializa a estrutura TPV para um estado conhecido
    return (hal_gps_dev_t)ctx;
}

/**
 * @brief Fecha uma interface GPS.
 *
 * Libera os recursos associados ao handle do dispositivo GPS, incluindo
 * o fechamento do dispositivo UART subjacente e a liberação da memória alocada.
 *
 * @param dev Handle opaco do dispositivo GPS a ser fechado.
 */
static void nmea_close(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    if (!ctx) return; // Verificação defensiva
    hal_uart_close(ctx->uart_dev);
    free(ctx); // Libera a memória do contexto
}

/**
 * @brief Solicita uma requisição de posição ao módulo GPS.
 *
 * Esta função é um placeholder; em uma implementação completa, enviaria
 * comandos NMEA específicos (ex: PMTK) para solicitar a posição atual ou
 * histórica, se suportado pelo módulo GPS.
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @param req O tipo de requisição de posição.
 * @return O tipo de requisição aceita (atualmente, retorna o mesmo valor de entrada).
 */
static hal_gps_position_request_t nmea_request_position(hal_gps_dev_t dev,
                                                        hal_gps_position_request_t req) {
    // Exemplo: não implementado, usamos polling de sentenças via read_sentence
    (void)dev; // Suprime warning de variável não utilizada
    return req;
}

/**
 * @brief Consulta o tipo de timestamp disponível.
 *
 * Dada a natureza da biblioteca gps.c (que preenche tpv.time em ISO8601 UTC),
 * esta função sempre retorna HAL_GPS_TIMESTAMP_UTC.
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @param type O tipo de timestamp desejado (não usado na implementação atual).
 * @return O tipo de timestamp disponível (sempre HAL_GPS_TIMESTAMP_UTC).
 */
static hal_gps_timestamp_t nmea_get_timestamp(hal_gps_dev_t dev,
                                              hal_gps_timestamp_t type) {
    (void)dev;  // Suprime warning de variável não utilizada
    (void)type; // Suprime warning de variável não utilizada
    // Sempre retorna UTC pois gps_decode preenche tpv.time em UTC
    return HAL_GPS_TIMESTAMP_UTC;
}

/**
 * @brief Obtém a qualidade do fix GPS.
 *
 * Mapeia o modo de fix (GPS_MODE_*) decodificado pela biblioteca gps.c
 * para o tipo de qualidade de fix da HAL (HAL_GPS_FIX_QUALITY_*).
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @return A qualidade do fix GPS.
 */
static hal_gps_fix_quality_t nmea_get_fix_quality(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    if (!ctx) return HAL_GPS_FIX_QUALITY_NONE; // Verificação defensiva
    switch (ctx->tpv.mode) {
        case GPS_MODE_2D_FIX: return HAL_GPS_FIX_QUALITY_2D;
        case GPS_MODE_3D_FIX: return HAL_GPS_FIX_QUALITY_3D;
        case GPS_MODE_NO_FIX: return HAL_GPS_FIX_QUALITY_NONE; // Mapeia No Fix para NONE
        case GPS_MODE_UNKNOWN: return HAL_GPS_FIX_QUALITY_NONE; // Mapeia Unknown para NONE
        default:              return HAL_GPS_FIX_QUALITY_NONE;
    }
}

/**
 * @brief Consulta o número de bytes disponíveis no buffer de recepção da UART.
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @return O número de bytes disponíveis.
 */
static size_t nmea_bytes_available(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    if (!ctx) return 0; // Verificação defensiva
    return hal_uart_bytes_available(ctx->uart_dev);
}

/**
 * @brief Realiza uma leitura bruta de bytes do dispositivo GPS (via UART).
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @param buffer Ponteiro para o buffer onde os bytes lidos serão armazenados.
 * @param size O número máximo de bytes a serem lidos.
 * @return O número de bytes lidos, 0 se nenhum byte estiver disponível, ou -1 em caso de erro.
 */
static ssize_t nmea_read(hal_gps_dev_t dev, uint8_t *buffer, size_t size) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    if (!ctx) return -1; // Verificação defensiva
    return hal_uart_read(ctx->uart_dev, buffer, size);
}

/**
 * @brief Envia bytes brutos para o dispositivo GPS (via UART).
 *
 * Usado para enviar comandos NMEA codificados para o módulo GPS.
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @param buffer Ponteiro para o buffer contendo os bytes a serem enviados.
 * @param size O número de bytes a serem enviados.
 * @return O número de bytes escritos, ou -1 em caso de erro.
 */
static ssize_t nmea_write(hal_gps_dev_t dev, const uint8_t *buffer, size_t size) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    if (!ctx) return -1; // Verificação defensiva
    // O buffer é const, mas hal_uart_write espera uint8_t*. Cast para compatibilidade.
    return hal_uart_write(ctx->uart_dev, (uint8_t*)buffer, size);
}

/**
 * @brief Aguarda a conclusão de todas as transmissões pendentes na UART.
 *
 * @param dev Handle opaco do dispositivo GPS.
 */
static void nmea_flush(hal_gps_dev_t dev) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    if (!ctx) return; // Verificação defensiva
    hal_uart_flush(ctx->uart_dev);
}

/**
 * @brief Lê uma sentença NMEA completa do dispositivo GPS e a decodifica.
 *
 * Esta função lê bytes da UART até encontrar uma sentença NMEA completa
 * (delimitada por '$' e '\n'), a armazena em um buffer e a passa para
 * a biblioteca gps.c para decodificação. Inclui lógica de timeout para
 * evitar bloqueio infinito.
 *
 * @param dev Handle opaco do dispositivo GPS.
 * @param sentence Buffer onde a sentença NMEA lida será armazenada.
 * @param max_len Tamanho máximo do buffer 'sentence'.
 * @return O número de bytes lidos da sentença, 0 se nenhuma sentença foi encontrada
 * (ex: timeout), ou -1 em caso de erro de leitura.
 */
static ssize_t nmea_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
    hal_gps_context_t *ctx = (hal_gps_context_t*)dev;
    size_t idx = 0;
    uint8_t c;
    bool found_start_char = false;

    const uint32_t SENTENCE_READ_TIMEOUT_MS = 500; // Timeout para a leitura de uma sentença completa
    uint32_t start_time_ms = hal_cpu_time_get_ms(); // Obtém o tempo inicial para o timeout

    while (idx + 1 < max_len) { // Garante espaço para o '\0'
        // Verifica se o tempo limite para a leitura da sentença foi excedido
        if (hal_cpu_time_get_ms() - start_time_ms > SENTENCE_READ_TIMEOUT_MS) {
            printf("nmea_read_sentence: Timeout occurred.\n"); // Mensagem de debug
            break; // Sai do loop se o timeout for atingido
        }

        ssize_t bytes_read = hal_uart_read(ctx->uart_dev, &c, 1); // Tenta ler um byte da UART

        if (bytes_read == 1) { // Caractere lido com sucesso
            start_time_ms = hal_cpu_time_get_ms(); // Reinicia o contador de tempo para o próximo caractere

            if (!found_start_char) { // Se ainda não encontramos o caractere inicial '$'
                if (c == '$') {
                    found_start_char = true; // Encontramos o '$'
                    sentence[idx++] = c;     // Adiciona o '$' ao buffer da sentença
                }
                // Se não é '$' e ainda não encontramos, continuamos procurando (descartamos o caractere)
                continue; 
            }

            // Se já encontramos o '$', adicionamos o caractere atual à sentença
            sentence[idx++] = c;
            
            // Verifica se o caractere atual é '\r'. Se for, esperamos pelo '\n' em seguida.
            // Esta lógica é uma adaptação para lidar com a remoção de '\r' por alguns terminais/PTYs.
            // Em um sistema real, o '\r' seria incluído no checksum e na verificação do rodapé.
            if (c == '\r') {
                continue; // Continua lendo para pegar o '\n'
            }

            // Se o caractere atual é '\n', a sentença está completa
            if (c == '\n') {
                break; // Sai do loop, a sentença foi completamente lida
            }
        } else if (bytes_read == 0) { // Nenhum byte lido no momento (buffer vazio)
            // Espera um pouco se não houver bytes disponíveis para evitar consumir 100% da CPU
            hal_cpu_sleep_ms(1); // Cede tempo da CPU por 1ms
        } else { // bytes_read < 0 (erro na leitura da UART)
            printf("nmea_read_sentence: UART read error.\n"); // Mensagem de debug
            break; // Sai do loop em caso de erro de leitura
        }
    }

    sentence[idx] = '\0'; // Garante que a string lida esteja nula-terminada

    // Se não encontramos o caractere inicial '$' ou se nenhum byte foi lido, retorna 0
    if (!found_start_char || idx == 0) {
        return 0;
    }

    // Aloca uma cópia da sentença NMEA para passar para gps_decode.
    // Isso é necessário porque gps_decode modifica a string de entrada in-place,
    // e o buffer 'sentence' pode ser usado por outras partes da HAL ou aplicação.
    char *sentence_copy = malloc(max_len); // Aloca memória para a cópia
    if (!sentence_copy) {
        printf("nmea_read_sentence: Failed to allocate memory for sentence_copy.\n");
        return -1; // Retorna erro se a alocação falhar
    }
    strncpy(sentence_copy, sentence, max_len); // Copia a sentença lida
    sentence_copy[max_len - 1] = '\0'; // Garante nul-terminação da cópia

    // Decodifica a sentença usando a biblioteca GPS (utl/gps/gps.c)
    int decode_result = gps_decode(&ctx->tpv, sentence_copy); // Passa a cópia para decodificação
    
    // Imprime o estado interno do GPS após a decodificação para depuração
    printf("Internal GPS state after decode:\n");
    printf("  Mode: %d\n", ctx->tpv.mode);
    printf("  Lat: %d\n", ctx->tpv.latitude);
    printf("  Lon: %d\n", ctx->tpv.longitude);

    free(sentence_copy); // Libera a memória alocada para a cópia
    
    // Imprime a qualidade do fix e o resultado da decodificação para depuração
    printf("Fix quality: %d\n", hal_gps_get_fix_quality(dev));
    printf("GPS Decode Result: %d\n", decode_result); // Removido gps_error_string para evitar erro de link

    return idx; // Retorna o número de bytes lidos da sentença
}


/** @brief Instância do driver NMEA, implementando a interface hal_gps_driver_t. */
static const hal_gps_driver_t nmea_driver = {
    .init            = nmea_init,             /**< Função de inicialização do driver. */
    .deinit          = nmea_deinit,           /**< Função de desinicialização do driver. */
    .open            = nmea_open,             /**< Função para abrir um dispositivo GPS. */
    .close           = nmea_close,            /**< Função para fechar um dispositivo GPS. */
    .request_position= nmea_request_position, /**< Função para solicitar posição (placeholder). */
    .get_timestamp   = nmea_get_timestamp,    /**< Função para obter timestamp. */
    .get_fix_quality = nmea_get_fix_quality,  /**< Função para obter qualidade do fix. */
    .bytes_available = nmea_bytes_available,  /**< Função para verificar bytes disponíveis na UART. */
    .read            = nmea_read,             /**< Função para leitura bruta de bytes. */
    .write           = nmea_write,            /**< Função para escrita bruta de bytes. */
    .flush           = nmea_flush,            /**< Função para descarregar buffer de transmissão. */
    .read_sentence   = nmea_read_sentence     /**< Função para ler e decodificar uma sentença NMEA. */
};

/**
 * @brief Inicializa a camada HAL GPS.
 *
 * Registra o driver NMEA como o driver ativo para a HAL GPS e o inicializa.
 */
void hal_gps_init(void) {
    s_driver = &nmea_driver; // Registra a instância do driver
    s_driver->init();        // Chama a função de inicialização do driver
}

/**
 * @brief Desativa a camada HAL GPS.
 *
 * Desinicializa o driver GPS ativo e limpa o ponteiro do driver.
 */
void hal_gps_deinit(void) {
    if (s_driver) { // Verificação defensiva
        s_driver->deinit(); // Chama a função de desinicialização do driver
        s_driver = NULL;    // Limpa o ponteiro do driver
    }
}

/**
 * @brief Abre a interface GPS na porta especificada.
 * @param port A porta GPS a ser aberta.
 * @param cfg Ponteiro para a estrutura de configuração.
 * @return Handle opaco para o dispositivo GPS, ou NULL em caso de erro.
 */
hal_gps_dev_t hal_gps_open(hal_gps_port_t port, const hal_gps_config_t *cfg) {
    assert(s_driver); // Garante que o driver foi inicializado
    if (!s_driver) return NULL; // Verificação defensiva
    return s_driver->open(port, cfg);
}

/**
 * @brief Fecha a interface GPS.
 * @param dev Handle opaco do dispositivo GPS a ser fechado.
 */
void hal_gps_close(hal_gps_dev_t dev) {
    assert(s_driver && dev); // Garante que o driver e o handle são válidos
    if (!s_driver || !dev) return; // Verificação defensiva
    s_driver->close(dev);
}

/**
 * @brief Retorna o número de bytes disponíveis no buffer de recepção da UART.
 * @param dev Handle opaco do dispositivo GPS.
 * @return O número de bytes disponíveis.
 */
size_t hal_gps_bytes_available(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    if (!s_driver || !dev) return 0;
    return s_driver->bytes_available(dev);
}

/**
 * @brief Realiza uma leitura bruta de bytes do dispositivo GPS.
 * @param dev Handle opaco do dispositivo GPS.
 * @param buffer Ponteiro para o buffer de destino.
 * @param size O número de bytes a serem lidos.
 * @return O número de bytes lidos, 0 se nenhum byte disponível, ou -1 em caso de erro.
 */
ssize_t hal_gps_read(hal_gps_dev_t dev, uint8_t *buffer, size_t size) {
    assert(s_driver && dev);
    if (!s_driver || !dev) return -1;
    return s_driver->read(dev, buffer, size);
}

/**
 * @brief Envia comandos codificados NMEA para o GPS.
 * @param dev Handle opaco do dispositivo GPS.
 * @param buffer Ponteiro para o buffer de origem.
 * @param size O número de bytes a serem escritos.
 * @return O número de bytes escritos, ou -1 em caso de erro.
 */
ssize_t hal_gps_write(hal_gps_dev_t dev, const uint8_t *buffer, size_t size) {
    assert(s_driver && dev);
    if (!s_driver || !dev) return -1;
    return s_driver->write(dev, buffer, size);
}

/**
 * @brief Aguarda a conclusão de todas as transmissões pendentes.
 * @param dev Handle opaco do dispositivo GPS.
 */
void hal_gps_flush(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    if (!s_driver || !dev) return;
    s_driver->flush(dev);
}

/**
 * @brief Lê uma sentença NMEA completa do dispositivo GPS e a decodifica.
 * @param dev Handle opaco do dispositivo GPS.
 * @param sentence Buffer onde a sentença NMEA lida será armazenada.
 * @param max_len Tamanho máximo do buffer 'sentence'.
 * @return O número de bytes lidos da sentença, 0 se nenhuma sentença foi encontrada
 * (ex: timeout), ou -1 em caso de erro de leitura/alocação.
 */
ssize_t hal_gps_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len) {
    assert(s_driver && dev);
    if (!s_driver || !dev) return -1;
    return s_driver->read_sentence(dev, sentence, max_len);
}

/**
 * @brief Obtém a qualidade do fix GPS.
 * @param dev Handle opaco do dispositivo GPS.
 * @return A qualidade do fix GPS (HAL_GPS_FIX_QUALITY_*).
 */
hal_gps_fix_quality_t hal_gps_get_fix_quality(hal_gps_dev_t dev) {
    assert(s_driver && dev);
    if (!s_driver || !dev) return HAL_GPS_FIX_QUALITY_NONE;
    return s_driver->get_fix_quality(dev);
}
