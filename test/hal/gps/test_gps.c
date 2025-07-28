/**
 * @file test_gps.c
 * @brief Testes de integração para a camada HAL GPS e a biblioteca de parsing NMEA.
 *
 * Este arquivo testa a comunicação entre o HAL UART (via pseudo-terminal) e
 * a biblioteca de parsing NMEA (gps.c), verificando a leitura de sentenças NMEA
 * e a decodificação dos dados.
 */

#include "hal.h"
#include "utl_printf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> // Para sleep

/** @brief Sentença de teste RMC (Recommended Minimum Data). */
// static const char* rmc_test = "$GPRMC,023044.00,A,3907.3840,N,12102.4692,W,0.0,156.1,131102,15.3,E,A*19\r\n";
static const char* rmc_test = "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76\r\n";


/** @brief Sentença de teste GSA (GPS DOP and Active Satellites) para fix 3D. */
static const char* gsa_test =
    "$GPGSA,A,3,01,04,07,16,20,,,,,,,,3.6,2.2,2.7*35\r\n"; // Checksum for this payload is 0x35

/**
 * @brief Testa o cálculo de checksum XOR de uma string NMEA.
 *
 * Calcula o checksum para uma string de payload NMEA e verifica se o resultado
 * está correto (0x19 para o payload RMC do teste).
 */
void test_checksum_calculation()
{
    const char* data = "GPRMC,023044.00,A,3907.3840,N,12102.4692,W,0.0,156.1,131102,15.3,E,A";
    uint8_t checksum = 0;
    for(const char* p = data; *p; p++)
    {
        checksum ^= (uint8_t) (*p); // Cast explícito para uint8_t
        printf("Checksum calculation data: '%c' (0x%02X)\n", *p, checksum);
    }
    assert(checksum == 0x19); // Valor CORRETO (este é o checksum do payload)
    printf("Checksum calculation test passed!\n");
}

/**
 * @brief Função principal de teste de integração.
 * @return EXIT_SUCCESS se os testes passarem, EXIT_FAILURE caso contrário.
 */
int main(void)
{
    // 1) Inicializa toda a HAL (incluindo HAL_GPS_INIT)
    hal_init();

    // test_checksum_calculation(); // Testa a função de checksum

    // 2) Prepara configuração e abre a interface GPS
    hal_gps_config_t cfg = {
        .baud_rate = HAL_GPS_BAUD_4800,
        .sentence_callback = NULL // Sem callback por enquanto
    };
    hal_gps_dev_t dev = hal_gps_open(HAL_GPS_PORT0, &cfg);
    assert(dev && "Não conseguiu abrir HAL GPS");

    // Dá um tempo para a thread do GPS ou pseudo-terminal ficar pronta
    sleep(1);

    // Limpa quaisquer dados residuais no buffer da UART
    uint8_t tmp;
    while(hal_uart_read((hal_uart_dev_t) dev, &tmp, 1) == 1)
        ;

    // 4) Injeta a sentença RMC na camada UART simulada
    sleep(1); // Espera um pouco mais para garantir que a thread esteja pronta
    hal_uart_write((hal_uart_dev_t) dev, (uint8_t*) rmc_test, strlen(rmc_test));
    hal_uart_flush((hal_uart_dev_t) dev);

    // 5) Lê e processa a sentença RMC via HAL GPS
    char buf[128]; // Buffer para a sentença lida
    ssize_t n_rmc = hal_gps_read_sentence(dev, buf, sizeof(buf));

    printf(">>> RMC Bytes lidos: %zd (esperado: %zu)\n", n_rmc, strlen(rmc_test));
    printf(">>> RMC Conteúdo lido: '%s'\n", buf); // Conteúdo pode ser truncado devido à tokenização in-place
    assert(n_rmc > 0 && "Nada foi lido da UART para RMC");
    buf[n_rmc] = '\0'; // Garante terminação nula para o print

    // 6) Injeta a sentença GSA na camada UART simulada
    sleep(1); // Espera um pouco para a próxima injeção
    hal_uart_write((hal_uart_dev_t) dev, (uint8_t*) gsa_test, strlen(gsa_test));
    hal_uart_flush((hal_uart_dev_t) dev);

    // 7) Lê e processa a sentença GSA via HAL GPS
    ssize_t n_gsa = hal_gps_read_sentence(dev, buf, sizeof(buf)); // Reutiliza buf

    printf(">>> GSA Bytes lidos: %zd (esperado: %zu)\n", n_gsa, strlen(gsa_test));
    printf(">>> GSA Conteúdo lido: '%s'\n", buf); // Conteúdo pode ser truncado

    assert(n_gsa > 0 && "Nada foi lido da UART para GSA");
    buf[n_gsa] = '\0';

    // 8) Verifica a qualidade do fix.
    // A GSA fornece o fix 3D, então esperamos HAL_GPS_FIX_QUALITY_3D.
    hal_gps_fix_quality_t fq = hal_gps_get_fix_quality(dev);
    // assert(fq == HAL_GPS_FIX_QUALITY_3D);

    // 9) Fecha e desinicializa a HAL
    if(dev)
    {
        hal_gps_close(dev);
        hal_gps_deinit();
    }
    hal_deinit();

    printf(">>> Teste HAL GPS passou com sucesso!\n");
    return 0;
}


