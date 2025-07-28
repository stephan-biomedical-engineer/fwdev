/*
The MIT License (MIT)

Copyright (c) 2015 Jacob McGladdery

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "gps.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h> // Para toupper()
#include "utl_dbg.h" // Assuming you need this to use UTL_DBG_PRINTF
#include "utl_printf.h" // Assuming you need this for UTL_DBG_PRINTF

#define NMEA_MAX_FIELDS  (32)
#define SENTENCE_ID_SIZE (3)

#define is_char_in_range(c, start, end) \
    (((uint_fast8_t)(c - start)) < ((uint_fast8_t)(end - start + 1)))

#define is_digit_0_to_1(c) is_char_in_range(c, '0', '1')
#define is_digit_0_to_2(c) is_char_in_range(c, '0', '2')
#define is_digit_0_to_3(c) is_char_in_range(c, '0', '3')
#define is_digit_0_to_5(c) is_char_in_range(c, '0', '5')
#define is_digit_0_to_9(c) is_char_in_range(c, '0', '9')

typedef void (*parse_function)(struct gps_tpv *, const char **);

static const char NULL_TIME[] = "0000-00-00T00:00:00.000Z";

static char uint8_to_hex_char(const uint8_t n)
{
    static const char hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    if ((0 <= n) && (n <= 0x0F)) return hex[n];
    return hex[0];
}

static uint8_t hex_char_to_uint8(const char c)
{
    if (('0' <= c) && (c <= '9'))
        return c - '0';
    else if (('A' <= c) && (c <= 'F'))
        return c - 'A' + 10;
    else if (('a' <= c) && (c <= 'f'))
        return c - 'a' + 10;

    return 0;
}

// static uint8_t build_hex_byte(const char c0, const char c1)
// {
//     uint8_t val;
//     val  = (hex_char_to_uint8(c0) << 4) & 0xF0;
//     val |=  hex_char_to_uint8(c1)       & 0x0F;
//     return val;
// }

// Função 100% testada para converter hex
static uint8_t build_hex_byte(const char c0, const char c1) {
    uint8_t val = 0;
    
    if (c0 >= '0' && c0 <= '9') {
        val = (c0 - '0') << 4;
    } else if (c0 >= 'A' && c0 <= 'F') {
        val = (c0 - 'A' + 10) << 4;
    } else if (c0 >= 'a' && c0 <= 'f') {
        val = (c0 - 'a' + 10) << 4;
    }
    
    if (c1 >= '0' && c1 <= '9') {
        val |= (c1 - '0');
    } else if (c1 >= 'A' && c1 <= 'F') {
        val |= (c1 - 'A' + 10);
    } else if (c1 >= 'a' && c1 <= 'f') {
        val |= (c1 - 'a' + 10);
    }
    
    return val;
}

static bool match_sentence_id(const char *str, const char *id)
{
    return strncmp(str, id, SENTENCE_ID_SIZE) == 0;
}

// Função para verificar o checksum de uma sentença NMEA
static bool verify_nmea_checksum(const char* sentence) {
    // Encontra o asterisco
    const char* asterisk = strchr(sentence, '*');
    if (!asterisk || asterisk - sentence < 6) // Mínimo: $XXXXX*CS
        return false;
    
    // Calcula o checksum
    uint8_t calculated = 0;
    // Começa após o '$', até antes do '*'
    for (const char* p = sentence + 1; p < asterisk; p++) {
        calculated ^= *p;
    }
    
    // Converte o checksum esperado para byte
    char hex1 = toupper(*(asterisk + 1));
    char hex2 = toupper(*(asterisk + 2));
    
    uint8_t expected = 0;
    expected |= (hex1 >= 'A') ? (hex1 - 'A' + 10) << 4 : (hex1 - '0') << 4;
    expected |= (hex2 >= 'A') ? (hex2 - 'A' + 10) : (hex2 - '0');
    
    return (calculated == expected);
}

static int32_t parse_number(const char *str)
{
    int32_t value = 0;
    int32_t factor;
    int32_t sign = 1;
    char c0 = *str++;

    /* Check if the string is an empty token field */
    if (!c0) return GPS_INVALID_VALUE;

    /* Number : -?[0-9]+(\.[0-9]{1,3}) */

    /* Check if the first character is a minus sign
     * Regex : -?
     */
    if ('-' == c0)
    {
        sign = -1;
        c0 = *str++;
    }

    /* Store one or more decimal digits
     * Regex : [0-9]+
     */
    if (is_digit_0_to_9(c0))
    {
        do
        {
            value = (value * 10) + (c0 - '0');
            c0 = *str++;
        }
        while (is_digit_0_to_9(c0));
    }
    else
    {
        return GPS_INVALID_VALUE;
    }

    /* Check for a decimal point. If one is present, then store up to 3 digits
     * past it.
     * Regex : (\.[0-9]{1,3})
     */
    factor = GPS_VALUE_FACTOR;
    if ('.' == c0)
    {
        c0 = *str++;
        if (is_digit_0_to_9(c0))
        {
            uint_fast8_t i = 3;

            do
            {
                value = (value * 10) + (c0 - '0');
                factor /= 10;
                c0 = *str++;
            }
            while (is_digit_0_to_9(c0) && --i);
        }
    }

    return value * factor * sign;
}

static int32_t parse_angular_distance(const char *nmea, const char direction)
{
    int32_t angular_distance;
    int32_t minutes;
    int32_t factor;
    int32_t sign;
    uint_fast8_t i;
    char c0 = *nmea++;

    /* First check for an empty token field */
    if (!c0) return GPS_INVALID_VALUE;

    /* Latitude  : ([0-9]{2})([0-9]{2}\.)[0-9]{1,6}
     * Longitude : ([0-9]{3})([0-9]{2}\.)[0-9]{1,6}
     *
     * Northing  : N = +, S = -
     * Easting   : E = +, W = -
     */

    /* Convert the direction to a sign. Also set the number of digits to treat
     * as decimal degrees. For N or S, we know we are parsing latitude and
     * that begins with 2 degree digits. For E or W, we know we are parsing
     * longitude and that begins with 3 degree digits.
     */
    switch (direction)
    {
    case 'N':
        sign = 1;
        i = 2;
        break;
    case 'S':
        sign = -1;
        i = 2;
        break;
    case 'E':
        sign = 1;
        i = 3;
        break;
    case 'W':
        sign = -1;
        i = 3;
        break;
    default:
        return GPS_INVALID_VALUE;
    }

    /* Store the first 3 digits which are already in decimal degrees
     * Regex : ([0-9]{3})
     */
    angular_distance = 0;
    while (i--)
    {
        if (is_digit_0_to_9(c0))
        {
            angular_distance = (angular_distance * 10) + (c0 - '0');
            c0 = *nmea++;
        }
        else
        {
            return GPS_INVALID_VALUE;
        }
    }
    angular_distance *= GPS_LAT_LON_FACTOR;

    /* Store the next two digits which are part of the arc minutes
     * Regex : ([0-9]{2}\.)
     */
    minutes = 0;
    i = 2;
    while (i--)
    {
        if (is_digit_0_to_9(c0))
        {
            minutes = (minutes * 10) + (c0 - '0');
            c0 = *nmea++;
        }
        else
        {
            return GPS_INVALID_VALUE;
        }
    }

    /* Check if a decimal point is the current character */
    if (c0 != '.') return GPS_INVALID_VALUE;
    c0 = *nmea++;

    /* Store up to 6 more arc minute fraction values
     * Regex : [0-9]{1,6}
     */
    factor = GPS_LAT_LON_FACTOR;
    i = 6;
    if (is_digit_0_to_9(c0))
    {
        do
        {
            minutes = (minutes * 10) + (c0 - '0');
            factor /= 10;
            c0 = *nmea++;
        }
        while (is_digit_0_to_9(c0) && --i);
    }
    else
    {
        return GPS_INVALID_VALUE;
    }
    minutes *= factor;

    /* Divide the arc minutes by 60 to get the decimal degrees fraction */
    minutes /= 60;

    /* Add the decimal degrees to the fraction component. We now how an
     * integer value which represents latitude in decimal degrees times
     * 10^6.
     */
    angular_distance += minutes;

    return angular_distance * sign;
}

static void parse_time(char *destination, const char *nmea)
{
    char c0 = *nmea++;

    /* ISO8601 : YYYY-MM-DDTHH:MM:SS.SSSZ
     * NMEA    : HHMMSS.SSS
     * Regex   : ([0-2][0-9])([0-5][0-9])([0-5][0-9])(\.[0-9]{1,3})?
     */

    /* ([0-2][0-9]) */
    if (is_digit_0_to_2(c0))
    {
        destination[11] = c0; /* H */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[12] = c0; /* H */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    /* ([0-5][0-9]) */
    if (is_digit_0_to_5(c0))
    {
        destination[14] = c0; /* M */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[15] = c0; /* M */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    /* ([0-5][0-9]) */
    if (is_digit_0_to_5(c0))
    {
        destination[17] = c0; /* S */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[18] = c0; /* S */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    /* (\.[0-9]{1,3})? */
    if ('.' == c0)
    {
        uint_fast8_t i;

        /* Advance the destination pointer past the decimal point */
        destination += 20;
        c0 = *nmea++;
        for (i = 0; is_digit_0_to_9(c0) && (i < 3); ++i)
        {
            destination[i] = c0;
            c0 = *nmea++;
        }
    }
    else
    {
        destination[20] = '0';
        destination[21] = '0';
        destination[22] = '0';
    }
}

static void parse_date(char *destination, const char *nmea)
{
    char c0 = *nmea++;

    /* ISO8601 : YYYY-MM-DDTHH:MM:SS.SSSZ
     * NMEA    : DDMMYY
     * Regex   : ([0-3][0-9])([0-1][0-9])([0-9][0-9])
     */

    /* ([0-3][0-9]) */
    if (is_digit_0_to_3(c0))
    {
        destination[8] = c0; /* D */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[9] = c0; /* D */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    /* ([0-1][0-9]) */
    if (is_digit_0_to_1(c0))
    {
        destination[5] = c0; /* M */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[6] = c0; /* M */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    /* ([0-9][0-9]) */
    if (is_digit_0_to_9(c0))
    {
        destination[2] = c0; /* Y */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[3] = c0; /* Y */
        c0 = *nmea++;
    }
    else
    {
        return;
    }

    /* Prepend "20" to the year. Hopefully by the year 2100 there will be a
     * better standard than NMEA 0183 and we will never need to change this.
     */
    destination[0] = '2';
    destination[1] = '0';
}

static void parse_extended_date(char *destination, const char *day, const char *month, const char *year)
{
    uint_fast8_t i;
    char c0;

    /* ISO8601 : YYYY-MM-DDTHH:MM:SS.SSSZ
     * Day     : [0-3][0-9]
     * Month   : [0-1][0-9]
     * Year    : [0-9]{4}
     */

    /* Day */
    c0 = *day++;
    if (is_digit_0_to_3(c0))
    {
        destination[8] = c0;
        c0 = *day++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[9] = c0;
    }
    else
    {
        return;
    }

    /* Month */
    c0 = *month++;
    if (is_digit_0_to_1(c0))
    {
        destination[5] = c0;
        c0 = *month++;
    }
    else
    {
        return;
    }

    if (is_digit_0_to_9(c0))
    {
        destination[6] = c0;
    }
    else
    {
        return;
    }

    /* Year */
    c0 = *year++;
    for (i = 0; is_digit_0_to_9(c0) && (i < 4); ++i)
    {
        destination[i] = c0;
        c0 = *year++;
    }
}

static int32_t parse_altitude(const char *nmea, const char unit)
{
    /* The unit should always be meters */
    if (unit != 'M') return GPS_INVALID_VALUE;

    return parse_number(nmea);
}

static int32_t parse_track(const char *nmea, const char type)
{
    /* Make sure the track type is true */
    if (type != 'T') return GPS_INVALID_VALUE;

    return parse_number(nmea);
}

static int32_t parse_speed(const char *nmea, const char unit)
{
    int32_t speed;

    /* Process speed differently based on the unit. In all cases, we are
     * converting from some unit to the SI unit of meters per second.
     * K = Kilometers per hour, N = Knots
     */
    switch (unit)
    {
    case 'K':
        speed = parse_number(nmea);
        return (speed * 10) / 36;
    case 'N':
        speed = parse_number(nmea);
        return (speed * 1000) / 1944;
    default:
        break;
    }

    return GPS_INVALID_VALUE;
}

static enum gps_mode parse_mode(const char mode)
{
    switch (mode)
    {
    case '1': return GPS_MODE_NO_FIX;
    case '2': return GPS_MODE_2D_FIX;
    case '3': return GPS_MODE_3D_FIX;
    default: break;
    }

    return GPS_MODE_UNKNOWN;
}

static bool is_status_valid(const char status)
{
    if ('A' == status) return true;
    return false;
}

// static void parse_gga(struct gps_tpv *tpv, const char **token)
// {
//     parse_time(tpv->time, token[0]);
//     tpv->latitude = parse_angular_distance(token[1], token[2][0]);
//     tpv->longitude = parse_angular_distance(token[3], token[4][0]);
//     tpv->altitude = parse_altitude(token[8], token[9][0]);
// }

static void parse_gga(struct gps_tpv *tpv, const char **token) {
    // Token 0: Tempo UTC
    if (token[0] && token[0][0]) {
        parse_time(tpv->time, token[0]);
    }
    
    // Token 1: Latitude
    // Token 2: Direção latitude
    if (token[1] && token[1][0] && token[2] && token[2][0]) {
        tpv->latitude = parse_angular_distance(token[1], token[2][0]);
    }
    
    // Token 3: Longitude
    // Token 4: Direção longitude
    if (token[3] && token[3][0] && token[4] && token[4][0]) {
        tpv->longitude = parse_angular_distance(token[3], token[4][0]);
    }
    
    // Token 8: Altitude
    // Token 9: Unidade altitude
    if (token[8] && token[8][0] && token[9] && token[9][0]) {
        tpv->altitude = parse_altitude(token[8], token[9][0]);
    }
}

static void parse_gll(struct gps_tpv *tpv, const char **token)
{
    if (is_status_valid(token[5][0]))
    {
        tpv->latitude = parse_angular_distance(token[0], token[1][0]);
        tpv->longitude = parse_angular_distance(token[2], token[3][0]);
        parse_time(tpv->time, token[4]);
    }
}

static void parse_gsa(struct gps_tpv *tpv, const char **token) {
    // Campo 2: Modo de operação (1=Manual, 2=Automático)
    printf("Parsing GSA - Mode of operation: %s\n", token[1] ? token[1] : "NULL");
    
    if (token[1] && token[1][0]) {
        printf("Parsing GSA - Mode char: %c\n", token[1][0]);
        tpv->mode = parse_mode(token[1][0]);
    } else {
        tpv->mode = GPS_MODE_UNKNOWN;
    }
    
    // Campo 3: Modo de fix (1=Sem fix, 2=2D, 3=3D)
    if (token[2] && token[2][0]) {
        tpv->mode = parse_mode(token[2][0]);
    } else {
        tpv->mode = GPS_MODE_UNKNOWN;
    }
    
    // Campos 4-15: IDs dos satélites (podem estar vazios)
    
    // Campo 16: PDOP
    // Campo 17: HDOP
    // Campo 18: VDOP
    // (Não usamos estes campos no exemplo)
}

static void parse_rmc(struct gps_tpv *tpv, const char **token)
{
    if (is_status_valid(token[1][0]))
    {
        parse_time(tpv->time, token[0]);
        tpv->latitude = parse_angular_distance(token[2], token[3][0]);
        tpv->longitude = parse_angular_distance(token[4], token[5][0]);
        tpv->track = parse_track(token[7], 'T');
        tpv->speed = parse_speed(token[6], 'N');
        parse_date(tpv->time, token[8]);
    }
}

static void parse_vtg(struct gps_tpv *tpv, const char **token)
{
    tpv->track = parse_track(token[0], token[1][0]);
    tpv->speed = parse_speed(token[6], token[7][0]);
}

static void parse_zda(struct gps_tpv *tpv, const char **token)
{
    parse_time(tpv->time, token[0]);
    parse_extended_date(tpv->time, token[1], token[2], token[3]);
}

void gps_init_tpv(struct gps_tpv *tpv)
{
    assert(tpv != NULL);

    tpv->mode      = GPS_MODE_UNKNOWN;
    tpv->altitude  = GPS_INVALID_VALUE;
    tpv->latitude  = GPS_INVALID_VALUE;
    tpv->longitude = GPS_INVALID_VALUE;
    tpv->track     = GPS_INVALID_VALUE;
    tpv->speed     = GPS_INVALID_VALUE;
    strcpy(tpv->time, NULL_TIME);
    memset(tpv->talker_id, '\0', GPS_TALKER_ID_SIZE);
}

char *gps_encode(char *destination, const char *message)
{
    assert(destination != NULL);
    assert(message != NULL);

    uint8_t checksum = 0;
    char c0 = *message++;

    /* Add the header character */
    *destination++ = '$';

    /* Add and compute the checksum for all of the message contents */
    while (c0)
    {
        checksum ^= c0;
        *destination++ = c0;
        c0 = *message++;
    }

    /* Add the end of sentence marker */
    *destination++ = '*';

    /* Add the checksum characters */
    *destination++ = uint8_to_hex_char((checksum & 0xF0) >> 4);
    *destination++ = uint8_to_hex_char( checksum & 0x0F);

    /* Add the footer characters */
    *destination++ = '\r';
    *destination++ = '\n';

    /* NUL terminate the string */
    *destination = '\0';

    return destination;
}

// int gps_decode(struct gps_tpv *tpv, char *nmea)
// {
//     assert(tpv != NULL);
//     assert(nmea != NULL);

//     char debug_nmea_copy[256];
//     strncpy(debug_nmea_copy, nmea, sizeof(debug_nmea_copy) - 1);
//     debug_nmea_copy[sizeof(debug_nmea_copy) - 1] = '\0';
//     printf("DEBUG gps_decode: Received NMEA (len %zu): '%s'\n", strlen(debug_nmea_copy), debug_nmea_copy);

//     parse_function parse;
//     char *token[NMEA_MAX_FIELDS];
//     uint8_t checksum = 0;
//     uint8_t i = 0;
//     char c0_val, c1_val;
//     char *nmea_current_ptr = nmea; // Inicialmente aponta para '$'

//     /* Check if the first character is the header */
//     if (*nmea_current_ptr != '$') return GPS_ERROR_HEAD;
//     nmea_current_ptr++; // <--- CORREÇÃO AQUI: AVANÇAR O PONTEIRO ALÉM DO '$'

//     // Find the '*' character to delineate the payload for checksum calculation
//     char *checksum_delimiter_ptr = strchr(nmea_current_ptr, '*'); // strchr agora procura a partir de 'G'
//     if (!checksum_delimiter_ptr) return GPS_ERROR_TRUNCATED;

//     // Calculate checksum for the payload (characters *between* '$' and '*')
//     // O loop agora começa de 'G' e vai até o '*'
//     for (char *p = nmea_current_ptr; p < checksum_delimiter_ptr; p++) {
//         checksum ^= *p;
//     }

//     // Now, advance nmea_current_ptr to the '*'
//     nmea_current_ptr = checksum_delimiter_ptr;

//     // Get the expected checksum digits from the string
//     c0_val = *(nmea_current_ptr + 1); // First hex digit after '*'
//     c1_val = *(nmea_current_ptr + 2); // Second hex digit after '*'

//     // Validate the checksum
//     if (checksum != build_hex_byte(c0_val, c1_val)) {
//         printf("DEBUG: Calculated Checksum: 0x%02X, Expected: 0x%02X\n", checksum, build_hex_byte(c0_val, c1_val));
//         return GPS_ERROR_CHECKSUM;
//     }

//     // Now, handle the footer. Advance nmea_current_ptr past the checksum digits.
//     nmea_current_ptr += 3; // nmea_current_ptr now points to where '\r' or '\n' should be

//     // Check for the message footer: allow either "\r\n" or just "\n"
//     if (*nmea_current_ptr == '\r') {
//         nmea_current_ptr++; // Consume '\r' if present
//     }
//     if (*nmea_current_ptr != '\n') { // Must have a '\n'
//         return GPS_ERROR_FOOT;
//     }
//     nmea_current_ptr++; // Consume '\n'

//     // Ensure it's the end of the string after the footer
//     if (*nmea_current_ptr != '\0') {
//         return GPS_ERROR_FOOT; // Extra characters after footer
//     }

//     // --- TOKENIZATION ---
//     // At this point, checksum and footer are validated. Now tokenize the string in-place.
//     // We need to use the original 'nmea' pointer (which points to '$') for this.

//     // Restore 'nmea' to its original starting position for tokenization logic
//     nmea_current_ptr = nmea;

//     // Set Talker ID (e.g., 'GP')
//     tpv->talker_id[0] = *(nmea_current_ptr + 1);
//     tpv->talker_id[1] = *(nmea_current_ptr + 2);

//     // Determine the parsing function based on sentence ID (e.g., 'RMC')
//     if (match_sentence_id(nmea_current_ptr + 3, "GGA"))
//         parse = parse_gga;
//     else if (match_sentence_id(nmea_current_ptr + 3, "GLL"))
//         parse = parse_gll;
//     else if (match_sentence_id(nmea_current_ptr + 3, "GSA"))
//         parse = parse_gsa;
//     else if (match_sentence_id(nmea_current_ptr + 3, "RMC"))
//         parse = parse_rmc;
//     else if (match_sentence_id(nmea_current_ptr + 3, "VTG"))
//         parse = parse_vtg;
//     else if (match_sentence_id(nmea_current_ptr + 3, "ZDA"))
//         parse = parse_zda;
//     else
//         return GPS_ERROR_UNSUPPORTED; // Unsupported NMEA sentence

//     // Prepare tokens array. The original code's tokenization logic was intertwined
//     // with checksum calculation and pointer advancement. This new approach separates it.

//     // Tokenize the string in-place: iterate from start of sentence ID + first comma
//     i = 0;
//     char *current_char_for_tokenizing = nmea_current_ptr + 3 + SENTENCE_ID_SIZE; // Points to the first comma after sentence ID
    
//     // The first token starts immediately after the first comma (e.g., '0' in 023044.00)
//     token[i++] = current_char_for_tokenizing + 1;

//     // Iterate up to the NUL character that replaced '*' (delimiter_ptr)
//     while (*current_char_for_tokenizing != '\0') { // Loop until the string end (where '*' was replaced)
//         if (*current_char_for_tokenizing == ',') {
//             *current_char_for_tokenizing = '\0'; // Replace comma with NUL
//             // Add the next token if it's not empty and we haven't hit the end
//             if (*(current_char_for_tokenizing + 1) != '\0') {
//                 token[i++] = current_char_for_tokenizing + 1;
//             }
//         }
//         current_char_for_tokenizing++;
//     }

//     /* Parse the NMEA sentence tokens */
//     parse(tpv, (const char **)token);

//     return GPS_OK;
// }

// int gps_decode(struct gps_tpv *tpv, char *nmea) {
//     assert(tpv != NULL);
//     assert(nmea != NULL);

//     /* Debug: Print received NMEA sentence */
//     printf("DEBUG gps_decode: Received NMEA: '%s'\n", nmea);

//     /* 1. Verificação inicial do cabeçalho */
//     if (nmea[0] != '$') {
//         return GPS_ERROR_HEAD;
//     }

//     /* 2. Encontrar posição do checksum */
//     char *asterisk_ptr = strchr(nmea, '*');
//     if (!asterisk_ptr) {
//         return GPS_ERROR_TRUNCATED;
//     }

//     /* 3. Calcular checksum CORRETAMENTE (caracteres ENTRE '$' e '*') */
//     uint8_t calculated_checksum = 0;
//     char *p = nmea + 1;  // Começa APÓS o '$'

//     printf("Checksum calculation data: \"");
//     for (char *tmp = p; tmp < asterisk_ptr; tmp++) {
//         printf("%c", *tmp);
//     }
//     printf("\"\n");
    
//     while (p < asterisk_ptr) {
//         calculated_checksum ^= *p;
//         p++;
//     }

//     /* 4. Extrair checksum esperado (2 caracteres hex após '*') */
//     uint8_t expected_checksum = build_hex_byte(asterisk_ptr[1], asterisk_ptr[2]);

//     /* 5. Validar checksum */
//     if (calculated_checksum != expected_checksum) {
//         printf("Checksum error: Calc=0x%02X, Expected=0x%02X\n",
//                calculated_checksum, expected_checksum);
//         return GPS_ERROR_CHECKSUM;
//     }

//     /* 6. Verificar terminador (pode ser \r\n ou \n) */
//     char *footer_ptr = asterisk_ptr + 3; // Pula '*' e 2 dígitos do checksum
//     if (*footer_ptr == '\r') {
//         printf("DEBUG gps_decode: Found CR before LF\n");
//         footer_ptr++; // Ignora \r se existir
//     }
//     if (*footer_ptr != '\n') {
//         return GPS_ERROR_FOOT;
//     }

//     /* 7. Extrair Talker ID (2 caracteres após '$') */
//     tpv->talker_id[0] = nmea[1];
//     tpv->talker_id[1] = nmea[2];
//     tpv->talker_id[2] = '\0'; // Terminador nulo

//     /* 8. Determinar função de parse baseada no tipo de mensagem */
//     parse_function parse = NULL;
//     const char *sentence_id = nmea + 3; // Pula '$' e Talker ID (2 chars)
    
//     if (match_sentence_id(sentence_id, "GGA")) parse = parse_gga;
//     else if (match_sentence_id(sentence_id, "GLL")) parse = parse_gll;
//     else if (match_sentence_id(sentence_id, "GSA")) parse = parse_gsa;
//     else if (match_sentence_id(sentence_id, "RMC")) parse = parse_rmc;
//     else if (match_sentence_id(sentence_id, "VTG")) parse = parse_vtg;
//     else if (match_sentence_id(sentence_id, "ZDA")) parse = parse_zda;
//     else return GPS_ERROR_UNSUPPORTED;

//     /* 9. Tokenização in-place (substitui vírgulas por '\0') */
//     char *tokens[NMEA_MAX_FIELDS] = {0};
//     int token_count = 0;
//     // Primeiro token começa após o tipo de mensagem (GGA, RMC, etc.)
//     char *token_start = nmea + 3 + 3; // Pula '$', Talker ID (2) e tipo (3)
    
//     // ADICIONE VERIFICAÇÃO DE LIMITES
//     if (token_start >= asterisk_ptr) {
//         printf("ERROR: Token start beyond asterisk!\n");
//         return GPS_ERROR_TRUNCATED;
//     }

//     tokens[token_count++] = token_start;

//     for (char *p = token_start; *p && p < asterisk_ptr; p++) {
//         if (*p == ',') {
//             *p = '\0'; // Substitui vírgula por terminador
//             if (token_count < NMEA_MAX_FIELDS) {
//                 tokens[token_count++] = p + 1; // Próximo token começa após a vírgula
//             }
//         }
//     }

//     // ADICIONE LOG DOS TOKENS
//     printf("Tokens count: %d\n", token_count);
//     for (int i = 0; i < token_count; i++) {
//         printf("Token %d: '%s'\n", i, tokens[i]);
//     }

//     /* 10. Chamar a função de parse específica */
//     parse(tpv, (const char **)tokens);

//     return GPS_OK;
// }

void print_tokens(int token_count, char **tokens) {
    printf("Token count: %d\n", token_count);
    for (int i = 0; i < token_count; i++) {
        printf("Token %d: '%s'\n", i, tokens[i]);
    }
}

// int gps_decode(struct gps_tpv *tpv, char *nmea) {
//     assert(tpv != NULL);
//     assert(nmea != NULL);

//     printf("DEBUG gps_decode: Received NMEA: '%s'\n", nmea);

//     /* 1. Verificação inicial do cabeçalho */
//     if (nmea[0] != '$') {
//         return GPS_ERROR_HEAD;
//     }

//     /* 2. Encontrar posição do checksum */
//     char *asterisk_ptr = strchr(nmea, '*');
//     if (!asterisk_ptr) {
//         return GPS_ERROR_TRUNCATED;
//     }

//     /* 3. Calcular checksum com unsigned char */
//     uint8_t calculated_checksum = 0;
//     unsigned char *p = (unsigned char*)(nmea + 1);
    
//     printf("Checksum calculation data: \"");
//     while (p < (unsigned char*)asterisk_ptr) {
//         printf("%c", *p);
//         calculated_checksum ^= *p;
//         p++;
//     }
//     printf("\"\n");

//     /* 4. Extrair checksum esperado */
//     if ((unsigned)(asterisk_ptr - nmea) + 3 > strlen(nmea)) {
//         return GPS_ERROR_TRUNCATED;
//     }
    
//     uint8_t expected_checksum = build_hex_byte(asterisk_ptr[1], asterisk_ptr[2]);
//     printf("Checksum: Calc=0x%02X, Expected=0x%02X\n", calculated_checksum, expected_checksum);

//     if (calculated_checksum != expected_checksum) {
//         return GPS_ERROR_CHECKSUM;
//     }

//     /* 5. Verificar terminador */
//     char *footer_ptr = asterisk_ptr + 3;
//     if (*footer_ptr == '\r') {
//         footer_ptr++;
//     }
//     if (*footer_ptr != '\n') {
//         return GPS_ERROR_FOOT;
//     }

//     /* 6. Extrair Talker ID */
//     tpv->talker_id[0] = nmea[1];
//     tpv->talker_id[1] = nmea[2];
//     tpv->talker_id[2] = '\0';

//     /* 7. Determinar função de parse */
//     parse_function parse = NULL;
//     const char *sentence_id = nmea + 3;
    
//     if (match_sentence_id(sentence_id, "GGA")) parse = parse_gga;
//     else if (match_sentence_id(sentence_id, "GLL")) parse = parse_gll;
//     else if (match_sentence_id(sentence_id, "GSA")) parse = parse_gsa;
//     else if (match_sentence_id(sentence_id, "RMC")) parse = parse_rmc;
//     else if (match_sentence_id(sentence_id, "VTG")) parse = parse_vtg;
//     else if (match_sentence_id(sentence_id, "ZDA")) parse = parse_zda;
//     else return GPS_ERROR_UNSUPPORTED;

//     char *tokens[NMEA_MAX_FIELDS] = {0};
//     int token_count = 0;

//     // Encontrar primeira vírgula após o tipo da sentença
//     char *first_comma = strchr(nmea + 3, ',');
//     if (!first_comma || first_comma >= asterisk_ptr) {
//         return GPS_ERROR_TRUNCATED;
//     }

//     char *token_start = first_comma + 1;
//     tokens[token_count++] = token_start;

//     for (char *p = token_start; *p && p < asterisk_ptr; p++) {
//         if (*p == ',') {
//             *p = '\0';
//             if (token_count < NMEA_MAX_FIELDS) {
//                 tokens[token_count++] = p + 1;
//             }
//         }
//     }

//     // CORREÇÃO CRÍTICA: Remover checksum do último token
//     if (token_count > 0) {
//         char *last_token = tokens[token_count - 1];
//         char *asterisk_in_token = strchr(last_token, '*');
//         if (asterisk_in_token) {
//             *asterisk_in_token = '\0'; // Remove checksum do token
//         }
//     }

//     /* 9. Log dos tokens para depuração */
//     // printf("Token count: %d\n", token_count);
//     // for (int i = 0; i < token_count; i++) {
//     //     printf("Token %d: '%s'\n", i, tokens[i]);
//     // }
//     print_tokens(token_count, tokens);

//     /* 10. Chamar parser */
//     parse(tpv, (const char **)tokens);

//     return GPS_OK;
// }

int gps_decode(struct gps_tpv* tpv, char* nmea) {
    // Passo 1: Verificar o header
    if (nmea[0] != '$') {
        return GPS_ERROR_HEAD;
    }

    // Passo 2: Verificar checksum
    if (!verify_nmea_checksum(nmea)) {
        return GPS_ERROR_CHECKSUM;
    }

    // Passo 3: Encontrar fim da sentença
    char* end = strstr(nmea, "\r\n");
    if (!end) end = strchr(nmea, '\n');
    if (!end) end = strchr(nmea, '\0');
    
    // Passo 4: Extrair talker ID
    tpv->talker_id[0] = nmea[1];
    tpv->talker_id[1] = nmea[2];
    tpv->talker_id[2] = '\0';
    
    // Passo 5: Determinar o parser
    const char* sentence_id = nmea + 3;
    parse_function parse = NULL;
    
    if (strncmp(sentence_id, "GGA", 3) == 0) parse = parse_gga;
    else if (strncmp(sentence_id, "GSA", 3) == 0) parse = parse_gsa;
    else if (strncmp(sentence_id, "RMC", 3) == 0) parse = parse_rmc;
    else if (strncmp(sentence_id, "VTG", 3) == 0) parse = parse_vtg;
    else if (strncmp(sentence_id, "GLL", 3) == 0) parse = parse_gll;
    else if (strncmp(sentence_id, "ZDA", 3) == 0) parse = parse_zda;
    else return GPS_ERROR_UNSUPPORTED;
    
    // Passo 6: Tokenização
    char* tokens[32] = {0};
    int token_count = 0;
    
    char* start = (char*)(nmea + 6); // Após o tipo (ex: $GPGGA,)
    char* current = start;
    
    while (*current && current < end && token_count < 31) {
        if (*current == ',' || *current == '*') {
            *current = '\0';
            tokens[token_count++] = start;
            start = current + 1;
        }
        current++;
    }
    
    // Último token
    if (start < end) {
        tokens[token_count++] = start;
    }
    
    // Passo 7: Chamar o parser
    parse(tpv, (const char**)tokens);
    
    return GPS_OK;
}