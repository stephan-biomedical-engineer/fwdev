#pragma once  // Garante inclusão única do header

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * @file hal_uart.h
 * @brief Interface da HAL para comunicação UART.
 *
 * Este arquivo define a API abstrata de UART usada no projeto, permitindo a portabilidade
 * da camada de hardware para diferentes sistemas operacionais e plataformas.
 */

/** @brief Identificadores das portas UART disponíveis. */
typedef enum hal_uart_port_e {
    HAL_UART_PORT0 = 0, /**< Primeira porta UART */
    HAL_UART_PORT1,     /**< Segunda porta UART */
    HAL_UART_NUM_PORTS  /**< Número total de portas UART disponíveis */
} hal_uart_port_t;

/** @brief Taxas de transmissão suportadas (baud rate). */
typedef enum hal_uart_baud_rate_e {
    HAL_UART_BAUD_RATE_9600 = 0,    /**< 9600 bps */
    HAL_UART_BAUD_RATE_19200,       /**< 19200 bps */
    HAL_UART_BAUD_RATE_38400,       /**< 38400 bps */
    HAL_UART_BAUD_RATE_57600,       /**< 57600 bps */
    HAL_UART_BAUD_RATE_115200       /**< 115200 bps */
} hal_uart_baud_rate_t;

/** @brief Número de bits de parada. */
typedef enum hal_uart_stop_bits_e {
    HAL_UART_STOP_BITS_1 = 0,       /**< 1 bit de parada */
    HAL_UART_STOP_BITS_2            /**< 2 bits de parada */
} hal_uart_stop_bits_t;

/** @brief Tipos de paridade. */
typedef enum hal_uart_parity_e {
    HAL_UART_PARITY_NONE = 0,       /**< Sem paridade */
    HAL_UART_PARITY_ODD,            /**< Paridade ímpar */
    HAL_UART_PARITY_EVEN            /**< Paridade par */
} hal_uart_parity_t;

/** @brief Tipos de controle de fluxo. */
typedef enum hal_uart_flow_control_e {
    HAL_UART_FLOW_CONTROL_NONE = 0,     /**< Sem controle de fluxo */
    HAL_UART_FLOW_CONTROL_CTS_RTS       /**< Controle de fluxo CTS/RTS */
} hal_uart_flow_control_t;

/** @brief Tipo opaco para instância de dispositivo UART. */
typedef struct hal_uart_dev_s* hal_uart_dev_t;

/** @brief Tipo de função de callback para interrupção de recepção. */
typedef void (*hal_uart_interrupt_t)(uint8_t c);

/**
 * @brief Estrutura de configuração UART.
 */
typedef struct hal_uart_config_s {
    hal_uart_baud_rate_t     baud_rate;         /**< Velocidade de transmissão */
    hal_uart_parity_t        parity;            /**< Paridade */
    hal_uart_stop_bits_t     stop_bits;         /**< Bits de parada */
    hal_uart_flow_control_t  flow_control;      /**< Controle de fluxo */
    hal_uart_interrupt_t     interrupt_callback;/**< Callback para RX assíncrono (opcional) */
} hal_uart_config_t;

/**
 * @brief Estrutura de driver UART que deve ser implementada para cada backend (Linux, STM32, etc).
 */
typedef struct hal_uart_driver_s {
    void      (*init)(void);    /**< Inicializa o driver UART */
    void      (*deinit)(void);  /**< Desativa e libera o driver UART */

    hal_uart_dev_t (*open)(hal_uart_port_t dev, hal_uart_config_t* cfg); /**< Abre uma porta UART */
    void      (*close)(hal_uart_dev_t dev);                              /**< Fecha a UART */

    size_t    (*bytes_available)(hal_uart_dev_t dev);                    /**< Verifica bytes disponíveis para leitura */
    ssize_t   (*read)(hal_uart_dev_t dev, uint8_t* buffer, size_t size); /**< Lê dados da UART */
    ssize_t   (*write)(hal_uart_dev_t dev, uint8_t* buffer, size_t size);/**< Escreve dados na UART */
    void      (*flush)(hal_uart_dev_t dev);                              /**< Garante envio de todos os dados */
} hal_uart_driver_t;

// ─────────────────────────────────────────────
// API PÚBLICA
// ─────────────────────────────────────────────

/** @brief Inicializa a HAL UART. */
void hal_uart_init(void);

/** @brief Finaliza a HAL UART. */
void hal_uart_deinit(void);

/**
 * @brief Abre uma porta UART.
 * @param dev Identificador da porta.
 * @param cfg Configuração desejada.
 * @return Handle para dispositivo UART, ou NULL em caso de erro.
 */
hal_uart_dev_t hal_uart_open(hal_uart_port_t dev, hal_uart_config_t* cfg);

/**
 * @brief Fecha uma porta UART.
 * @param dev Handle da UART.
 */
void hal_uart_close(hal_uart_dev_t dev);

/**
 * @brief Retorna número de bytes disponíveis para leitura.
 * @param dev Handle da UART.
 * @return Quantidade de bytes disponíveis.
 */
size_t hal_uart_bytes_available(hal_uart_dev_t dev);

/**
 * @brief Lê dados da UART.
 * @param dev Handle da UART.
 * @param buffer Buffer de destino.
 * @param size Tamanho máximo a ser lido.
 * @return Número de bytes lidos, ou -1 em caso de erro.
 */
ssize_t hal_uart_read(hal_uart_dev_t dev, uint8_t* buffer, size_t size);

/**
 * @brief Escreve dados na UART.
 * @param dev Handle da UART.
 * @param buffer Dados a serem enviados.
 * @param size Número de bytes a enviar.
 * @return Número de bytes enviados, ou -1 em caso de erro.
 */
ssize_t hal_uart_write(hal_uart_dev_t dev, uint8_t* buffer, size_t size);

/**
 * @brief Aguarda o envio de todos os dados pendentes na UART.
 * @param dev Handle da UART.
 */
void hal_uart_flush(hal_uart_dev_t dev);

/**
 * @brief Lê um único byte da UART.
 * @param dev Handle da UART.
 * @param c Ponteiro para onde o byte será armazenado.
 * @return 1 se sucesso, 0 se nada a ler, -1 se erro.
 */
ssize_t hal_uart_byte_read(hal_uart_dev_t dev, uint8_t* c);

/**
 * @brief Escreve um único byte na UART.
 * @param dev Handle da UART.
 * @param c Byte a ser transmitido.
 * @return 1 se sucesso, -1 se erro.
 */
ssize_t hal_uart_byte_write(hal_uart_dev_t dev, uint8_t c);

#ifdef __cplusplus
}
#endif
