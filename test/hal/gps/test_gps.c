#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "gps.h"

int main(void) {
    struct gps_tpv tpv;
    gps_init_tpv(&tpv);

    // Sentença NMEA RMC de teste (13 Nov 2002 às 02:30:44 UTC)
    const char *rmc =
      "$GPRMC,023044,A,3907.3840,N,12102.4692,W,0.0,156.1,131102,15.3,E,A*37\r\n";

    // Copia para buffer mutável, pois gps_decode altera a string
    char buf[128];
    size_t len = strlen(rmc);
    assert(len < sizeof(buf));
    memcpy(buf, rmc, len + 1);

    // Decodifica
    int res = gps_decode(&tpv, buf);
    assert(res == GPS_OK);

    // Verifica campos parseados
    assert(strcmp(tpv.talker_id, "GP") == 0);
    assert(strcmp(tpv.time,      "2002-11-13T02:30:44.000Z") == 0);

    // Aqui estão os valores corretos:
    assert(tpv.latitude  ==  39123066);   // 39°07.3840′ N
    assert(tpv.longitude == -121041153);  // 121°02.4692′ W
    assert(tpv.track     ==    156100);   // 156.1° ×10³
    assert(tpv.speed     ==         0);   // 0 nós

    printf("Teste gps_decode RMC passou com sucesso.\n");
    return 0;
}
