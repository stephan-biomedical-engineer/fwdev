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

static uint8_t build_hex_byte(const char c0, const char c1)
{
    uint8_t val;
    val  = (hex_char_to_uint8(c0) << 4) & 0xF0;
    val |=  hex_char_to_uint8(c1)       & 0x0F;
    return val;
}

static bool match_sentence_id(const char *str, const char *id)
{
    return strncmp(str, id, SENTENCE_ID_SIZE) == 0;
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

static void parse_gga(struct gps_tpv *tpv, const char **token)
{
    parse_time(tpv->time, token[0]);
    tpv->latitude = parse_angular_distance(token[1], token[2][0]);
    tpv->longitude = parse_angular_distance(token[3], token[4][0]);
    tpv->altitude = parse_altitude(token[8], token[9][0]);
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

static void parse_gsa(struct gps_tpv *tpv, const char **token)
{
    tpv->mode = parse_mode(token[1][0]);
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

int gps_decode(struct gps_tpv *tpv, char *nmea)
{
    assert(tpv != NULL);
    assert(nmea != NULL);

    char debug_nmea_copy[256];
    strncpy(debug_nmea_copy, nmea, sizeof(debug_nmea_copy) - 1);
    debug_nmea_copy[sizeof(debug_nmea_copy) - 1] = '\0';
    printf("DEBUG gps_decode: Received NMEA (len %zu): '%s'\n", strlen(debug_nmea_copy), debug_nmea_copy);

    parse_function parse;
    char *token[NMEA_MAX_FIELDS];
    uint8_t checksum = 0;
    uint8_t i = 0;
    char c0_val, c1_val;
    char *nmea_current_ptr = nmea; // Inicialmente aponta para '$'

    /* Check if the first character is the header */
    if (*nmea_current_ptr != '$') return GPS_ERROR_HEAD;
    nmea_current_ptr++; // <--- CORREÇÃO AQUI: AVANÇAR O PONTEIRO ALÉM DO '$'

    // Find the '*' character to delineate the payload for checksum calculation
    char *checksum_delimiter_ptr = strchr(nmea_current_ptr, '*'); // strchr agora procura a partir de 'G'
    if (!checksum_delimiter_ptr) return GPS_ERROR_TRUNCATED;

    // Calculate checksum for the payload (characters *between* '$' and '*')
    // O loop agora começa de 'G' e vai até o '*'
    for (char *p = nmea_current_ptr; p < checksum_delimiter_ptr; p++) {
        checksum ^= *p;
    }

    // Now, advance nmea_current_ptr to the '*'
    nmea_current_ptr = checksum_delimiter_ptr;

    // Get the expected checksum digits from the string
    c0_val = *(nmea_current_ptr + 1); // First hex digit after '*'
    c1_val = *(nmea_current_ptr + 2); // Second hex digit after '*'

    // Validate the checksum
    if (checksum != build_hex_byte(c0_val, c1_val)) {
        printf("DEBUG: Calculated Checksum: 0x%02X, Expected: 0x%02X\n", checksum, build_hex_byte(c0_val, c1_val));
        return GPS_ERROR_CHECKSUM;
    }

    // Now, handle the footer. Advance nmea_current_ptr past the checksum digits.
    nmea_current_ptr += 3; // nmea_current_ptr now points to where '\r' or '\n' should be

    // Check for the message footer: allow either "\r\n" or just "\n"
    if (*nmea_current_ptr == '\r') {
        nmea_current_ptr++; // Consume '\r' if present
    }
    if (*nmea_current_ptr != '\n') { // Must have a '\n'
        return GPS_ERROR_FOOT;
    }
    nmea_current_ptr++; // Consume '\n'

    // Ensure it's the end of the string after the footer
    if (*nmea_current_ptr != '\0') {
        return GPS_ERROR_FOOT; // Extra characters after footer
    }

    // --- TOKENIZATION ---
    // At this point, checksum and footer are validated. Now tokenize the string in-place.
    // We need to use the original 'nmea' pointer (which points to '$') for this.

    // Restore 'nmea' to its original starting position for tokenization logic
    nmea_current_ptr = nmea;

    // Set Talker ID (e.g., 'GP')
    tpv->talker_id[0] = *(nmea_current_ptr + 1);
    tpv->talker_id[1] = *(nmea_current_ptr + 2);

    // Determine the parsing function based on sentence ID (e.g., 'RMC')
    if (match_sentence_id(nmea_current_ptr + 3, "GGA"))
        parse = parse_gga;
    else if (match_sentence_id(nmea_current_ptr + 3, "GLL"))
        parse = parse_gll;
    else if (match_sentence_id(nmea_current_ptr + 3, "GSA"))
        parse = parse_gsa;
    else if (match_sentence_id(nmea_current_ptr + 3, "RMC"))
        parse = parse_rmc;
    else if (match_sentence_id(nmea_current_ptr + 3, "VTG"))
        parse = parse_vtg;
    else if (match_sentence_id(nmea_current_ptr + 3, "ZDA"))
        parse = parse_zda;
    else
        return GPS_ERROR_UNSUPPORTED; // Unsupported NMEA sentence

    // Prepare tokens array. The original code's tokenization logic was intertwined
    // with checksum calculation and pointer advancement. This new approach separates it.

    // Tokenize the string in-place: iterate from start of sentence ID + first comma
    i = 0;
    char *current_char_for_tokenizing = nmea_current_ptr + 3 + SENTENCE_ID_SIZE; // Points to the first comma after sentence ID
    
    // The first token starts immediately after the first comma (e.g., '0' in 023044.00)
    token[i++] = current_char_for_tokenizing + 1;

    // Iterate up to the NUL character that replaced '*' (delimiter_ptr)
    while (*current_char_for_tokenizing != '\0') { // Loop until the string end (where '*' was replaced)
        if (*current_char_for_tokenizing == ',') {
            *current_char_for_tokenizing = '\0'; // Replace comma with NUL
            // Add the next token if it's not empty and we haven't hit the end
            if (*(current_char_for_tokenizing + 1) != '\0') {
                token[i++] = current_char_for_tokenizing + 1;
            }
        }
        current_char_for_tokenizing++;
    }

    /* Parse the NMEA sentence tokens */
    parse(tpv, (const char **)token);

    return GPS_OK;
}
