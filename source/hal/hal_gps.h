#pragma once  // Garante inclusão única do header

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// Defina os tipos de callback
typedef void (*gps_sentence_callback_t)(const char* sentence, size_t length);

// Defina os códigos de erro
#define HAL_OK                    0
#define HAL_ERR_INVALID_ARG      -1
#define HAL_ERR_NOT_INIT         -2
#define HAL_ERR_ALREADY_INIT     -3
#define HAL_ERR_TIMEOUT          -4
#define HAL_ERR_CHECKSUM         -5
#define HAL_ERR_BUFFER_OVERFLOW  -6

/**
 * @file hal_gps.h
 * @brief Interface HAL para módulo GPS usando a biblioteca gps.c (NMEA 0183).
 *
 * Abstração de alto nível que intermedia a comunicação entre a aplicação e
o driver NMEA (gps_encode / gps_decode), usando UART para transporte.
 */

/** @brief Identificadores das interfaces GPS disponíveis. */
typedef enum hal_gps_port_e {
    HAL_GPS_PORT0 = 0,  /**< Primeira interface GPS */
    HAL_GPS_PORT1,      /**< Segunda interface GPS */
    HAL_GPS_NUM_PORTS   /**< Total de interfaces disponíveis */
} hal_gps_port_t;

/** @brief Baud rates suportados para NMEA 0183. */
typedef enum hal_gps_baud_rate_e {
    HAL_GPS_BAUD_4800 = 0,   /**< 4800 bps (padrão NMEA) */
    HAL_GPS_BAUD_9600,       /**< 9600 bps */
    HAL_GPS_BAUD_19200,      /**< 19200 bps */
    HAL_GPS_BAUD_38400       /**< 38400 bps */
} hal_gps_baud_rate_t;

/** @brief Tipos de requisição de posição. */
typedef enum hal_gps_position_request_e {
    HAL_GPS_POS_REQUEST_NONE = 0,   /**< Sem requisição */
    HAL_GPS_POS_REQUEST_CURRENT,    /**< Posição atual */
    HAL_GPS_POS_REQUEST_HISTORY     /**< Histórico de posições (se suportado) */
} hal_gps_position_request_t;

/** @brief Tipos de timestamp disponíveis. */
typedef enum hal_gps_timestamp_e {
    HAL_GPS_TIMESTAMP_NONE = 0,      /**< Sem timestamp */
    HAL_GPS_TIMESTAMP_UTC,           /**< Timestamp em UTC */
    HAL_GPS_TIMESTAMP_LOCAL,         /**< Timestamp em hora local */
    HAL_GPS_TIMESTAMP_GPS            /**< Timestamp GPS (se suportado) */
} hal_gps_timestamp_t;

/** @brief Qualidade do fix GPS. */
typedef enum hal_gps_fix_quality_e {
    HAL_GPS_FIX_QUALITY_NONE = 0,    /**< Sem fix */
    HAL_GPS_FIX_QUALITY_2D,          /**< Fix 2D */
    HAL_GPS_FIX_QUALITY_3D,          /**< Fix 3D */
    HAL_GPS_FIX_QUALITY_DGPS,        /**< DGPS */
    HAL_GPS_FIX_QUALITY_RTK          /**< RTK */
} hal_gps_fix_quality_t;

/**
 * @brief Estrutura de configuração da HAL GPS.
 *
 * @param baud_rate    Baud rate desejado para NMEA.
 * @param sentence_callback Callback opcional para cada sentença NMEA recebida.
 */
// typedef struct hal_gps_config_s {
//     hal_gps_baud_rate_t baud_rate;
//     void (*sentence_callback)(const char *sentence, size_t len);
// } hal_gps_config_t;

typedef struct {
    hal_gps_baud_rate_t baud_rate;
    uint32_t timeout_ms;  // Adicionar se necessário
    void (*sentence_callback)(const char *sentence, size_t len);
} hal_gps_config_t;

/** @brief Tipo opaco de dispositivo GPS. */
typedef struct hal_gps_dev_s *hal_gps_dev_t;

/**
 * @brief Driver GPS que deve ser implementado por cada plataforma.
 */
typedef struct hal_gps_driver_s {
    /** Inicializa o backend (ex: UART, DMA). */
    void (*init)(void);
    /** Desativa o backend e libera recursos. */
    void (*deinit)(void);

    /**
     * Abre a interface GPS na porta especificada.
     * @return Handle opaco, ou NULL em caso de erro.
     */
    hal_gps_dev_t (*open)(hal_gps_port_t port, const hal_gps_config_t *cfg);

    /**
     * Fecha a interface GPS.
     */
    void (*close)(hal_gps_dev_t dev);

    /**
     * Solicita posição via comando NMEA (ex: PMTK).
     * @return Tipo de requisição aceita.
     */
    hal_gps_position_request_t (*request_position)(hal_gps_dev_t dev,
                                                   hal_gps_position_request_t request);

    /**
     * Consulta timestamp atual.
     * @return Tipo de timestamp.
     */
    hal_gps_timestamp_t (*get_timestamp)(hal_gps_dev_t dev,
                                         hal_gps_timestamp_t type);

    /**
     * Obtém qualidade do fix.
     */
    hal_gps_fix_quality_t (*get_fix_quality)(hal_gps_dev_t dev);

    /**
     * Quantos bytes estão disponíveis no buffer de recepção.
     */
    size_t (*bytes_available)(hal_gps_dev_t dev);

    /**
     * Leitura bruta de bytes (para parsing NMEA).
     */
    ssize_t (*read)(hal_gps_dev_t dev, uint8_t *buffer, size_t size);

    /**
     * Envio de bytes (para comandos NMEA codificados).
     */
    ssize_t (*write)(hal_gps_dev_t dev, const uint8_t *buffer, size_t size);

    /**
     * Aguarda finalização da transmissão.
     */
    void (*flush)(hal_gps_dev_t dev);

    /**
     * Lê uma sentença NMEA completa (até CR/LF).
     * @return bytes lidos, 0 se vazio, -1 em caso de erro.
     */
    ssize_t (*read_sentence)(hal_gps_dev_t dev, char *sentence, size_t max_len);
} hal_gps_driver_t;

// ─────────────────────────────────────────────
// API PÚBLICA
// ─────────────────────────────────────────────

/**
 * @brief Inicializa a camada HAL GPS.
 * Deve registrar o driver via hal_gps_driver.
 */
void hal_gps_init(void);

/** @brief Desativa a camada HAL GPS. */
void hal_gps_deinit(void);

/**
 * @brief Abre a interface GPS.
 * @return Handle para operações subsequentes.
 */
hal_gps_dev_t hal_gps_open(hal_gps_port_t port, const hal_gps_config_t *cfg);

/** @brief Fecha a interface GPS. */
void hal_gps_close(hal_gps_dev_t dev);

/** @brief Retorna bytes disponíveis no RX. */
size_t hal_gps_bytes_available(hal_gps_dev_t dev);

/**
 * @brief Leitura bruta de bytes do GPS.
 */
ssize_t hal_gps_read(hal_gps_dev_t dev, uint8_t *buffer, size_t size);

/**
 * @brief Envia comandos codificados NMEA para o GPS.
 */
ssize_t hal_gps_write(hal_gps_dev_t dev, const uint8_t *buffer, size_t size);

/** @brief Aguarda transmissão pendente. */
void hal_gps_flush(hal_gps_dev_t dev);

/**
 * @brief Lê sentença NMEA completa.
 */
ssize_t hal_gps_read_sentence(hal_gps_dev_t dev, char *sentence, size_t max_len);

hal_gps_fix_quality_t hal_gps_get_fix_quality(hal_gps_dev_t dev);

#ifdef __cplusplus
}
#endif

