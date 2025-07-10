// // test/hal/gps/test_gps.c
// #include "hal.h"
// #include "utl_printf.h"
// #include <assert.h>
// #include <string.h>
// #include <stdio.h>

// // Sentença de teste (RMC)
// static const char *rmc_test =
//   "$GPRMC,023044.00,A,3907.3840,N,12102.4692,W,0.0,156.1,131102,15.3,E,A*37\n";

// int main(void) {
//     // 1) Inicializa toda a HAL
//     hal_init();

//     // 2) Inicializa a parte GPS
//     // hal_gps_init();

//     // 3) Prepara configuração e abre
//     hal_gps_config_t cfg = {
//         .baud_rate       = HAL_GPS_BAUD_4800,
//         .sentence_callback = NULL
//     };
//     hal_gps_dev_t dev = hal_gps_open(HAL_GPS_PORT0, &cfg);
//     assert(dev && "Não conseguiu abrir HAL GPS");

//     sleep(1); // <=== dá tempo para o GPS thread ficar pront
    
//     uint8_t tmp;
//     while (hal_uart_read((hal_uart_dev_t)dev, &tmp, 1) == 1);


//     // 4) Injeta a sentença na camada UART por trás dos panos
//     //    O port_uart do Linux imprime no stdout algo como "[UART0] virtual port: /dev/pts/X"
//     //    Aqui simplificamos: como o port_uart usa um pseudo‐terminal, 
//     //    você pode abrir aquele /dev/pts/X e escrever diretamente nele.
//     //
//     //    Exemplo rápido (em C) de como descobrir e escrever:
//     extern char *utl_cbf_base_path; // supondo que o driver salve o nome do pts em algum lugar
//     // Alternativamente, no seu run.sh você pode fazer:
//     //    echo "$rmc_test" > /dev/pts/X
//     //
//     // Para manter tudo em C, vamos usar hal_uart_write direto:
//     // (ele escreverá no pts que o port_uart abriu)

//     sleep(1); // <=== espera mais um pouco para garantir que o GPS thread esteja pronto
//     hal_uart_write((hal_uart_dev_t)dev, (uint8_t*)rmc_test, strlen(rmc_test));
//     hal_uart_flush((hal_uart_dev_t)dev);
    
//     // 5) Agora leia via HAL GPS
//     char buf[128];
//     ssize_t n = hal_gps_read_sentence(dev, buf, sizeof(buf));
//     // char sentence[192] = {0};
//     // size_t offset = 0;
//     // while (!strstr(sentence, "\r\n")) {
//     //     ssize_t n = hal_gps_read_sentence(dev, sentence + offset, sizeof(sentence) - offset);
//     //     if (n <= 0) {
//     //         // timeout, erro ou nada disponível
//     //         break;
//     //     }
//     //     offset += n;
//     // }
//     // // agora `sentence` tem a linha completa
//     // assert(strcmp(sentence, rmc_test) == 0);

//     printf(">>> Bytes lidos: %zd (esperado: %zu)\n", n, strlen(rmc_test));
//     printf(">>> Conteúdo lido: '%s'\n", buf);
//     assert(n > 0 && "Nada foi lido da UART");
//     buf[n] = '\0';

//     // 6) Verificações básicas
//     printf("Sentença lida pelo HAL: %s", buf);
//     // assert(strncmp(buf, rmc_test, strlen(rmc_test)) == 0);

//     // Exemplo: checar se houve fix 3D (o parser interno já pôs o mode em GPS_MODE_3D_FIX para RMC)
//     hal_gps_fix_quality_t fq = hal_gps_get_fix_quality(dev);
//     assert(fq == HAL_GPS_FIX_QUALITY_3D);

//     // 7) Fecha e deinit
//     hal_gps_close(dev);
//     hal_gps_deinit();
//     hal_deinit();

//     printf(">>> Teste HAL‑GPS passou!\n");
//     return 0;
// }


// test/hal/gps/test_gps.c
#include "hal.h"
#include "utl_printf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> // Para sleep

// Sentença de teste (RMC) - Ajustada para ter apenas '\n'
static const char *rmc_test =
  "$GPRMC,023044.00,A,3907.3840,N,12102.4692,W,0.0,156.1,131102,15.3,E,A*37\n";

// Sentença de teste (GSA) - Para testar o fix 3D. Ajustada para ter apenas '\n'
static const char *gsa_test =
  "$GPGSA,A,3,01,04,07,16,20,,,,,,,,3.6,2.2,2.7*35\n"; // Checksum for this payload is 0x35

int main(void) {
    // 1) Inicializa toda a HAL
    hal_init();

    // 2) Prepara configuração e abre a interface GPS
    hal_gps_config_t cfg = {
        .baud_rate       = HAL_GPS_BAUD_4800,
        .sentence_callback = NULL // Sem callback por enquanto
    };
    hal_gps_dev_t dev = hal_gps_open(HAL_GPS_PORT0, &cfg);
    assert(dev && "Não conseguiu abrir HAL GPS");

    // Dá um tempo para a thread do GPS ou pseudo-terminal ficar pronta
    sleep(1);
    
    // Limpa quaisquer dados residuais no buffer da UART
    uint8_t tmp;
    while (hal_uart_read((hal_uart_dev_t)dev, &tmp, 1) == 1);

    // 4) Injeta a sentença RMC na camada UART simulada
    sleep(1); // Espera um pouco mais para garantir que a thread esteja pronta
    hal_uart_write((hal_uart_dev_t)dev, (uint8_t*)rmc_test, strlen(rmc_test));
    hal_uart_flush((hal_uart_dev_t)dev);
    
    // 5) Lê e processa a sentença RMC via HAL GPS
    char buf[128]; // Buffer para a sentença lida
    ssize_t n_rmc = hal_gps_read_sentence(dev, buf, sizeof(buf));
    
    printf(">>> RMC Bytes lidos: %zd (esperado: %zu)\n", n_rmc, strlen(rmc_test));
    printf(">>> RMC Conteúdo lido: '%s'\n", buf); // Conteúdo pode ser truncado devido à tokenização in-place
    assert(n_rmc > 0 && "Nada foi lido da UART para RMC");
    buf[n_rmc] = '\0'; // Garante terminação nula para o print

    // 6) Injeta a sentença GSA na camada UART simulada
    sleep(1); // Espera um pouco para a próxima injeção
    hal_uart_write((hal_uart_dev_t)dev, (uint8_t*)gsa_test, strlen(gsa_test));
    hal_uart_flush((hal_uart_dev_t)dev);

    // 7) Lê e processa a sentença GSA via HAL GPS
    ssize_t n_gsa = hal_gps_read_sentence(dev, buf, sizeof(buf)); // Reutiliza buf
    
    printf(">>> GSA Bytes lidos: %zd (esperado: %zu)\n", n_gsa, strlen(gsa_test));
    printf(">>> GSA Conteúdo lido: '%s'\n", buf); // Conteúdo pode ser truncado

    assert(n_gsa > 0 && "Nada foi lido da UART para GSA");
    buf[n_gsa] = '\0';

    // 8) Agora, verifica a qualidade do fix, que deve vir da GSA
    hal_gps_fix_quality_t fq = hal_gps_get_fix_quality(dev);
    // Para a sentença GSA fornecida ($GPGSA,A,3,...), o modo de fix é '3' (3D fix)
    assert(fq == HAL_GPS_FIX_QUALITY_3D); // Esta asserção agora deve passar!

    // Você também pode adicionar asserções para os dados decodificados (Lat, Lon, etc.)
    // se tiver funções hal_gps_get_latitude, etc.

    // 9) Fecha e desinicializa a HAL
    hal_gps_close(dev);
    hal_gps_deinit();
    hal_deinit();

    printf(">>> Teste HAL‑GPS passou com sucesso!\n");
    return 0;
}