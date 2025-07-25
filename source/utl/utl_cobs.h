#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#define COBS_OVERHEAD_SIZE(max_len) ((max_len) + ((max_len) / 254) + 1)
#define COBS_MAX_DATA_LEN(encoded_len) ((encoded_len) - 2 - ((encoded_len - 1) / 255))

/** COBS encode data to buffer
    @param input Pointer to input data to encode
    @param output Pointer to encoded output buffer
    @param len Number of bytes to encode
    @return Encoded output len in bytes
    @note Does not output delimiter byte
*/
size_t cobs_encode(const void* input, uint8_t* output, size_t len);

/** COBS decode data from buffer
    @param input Pointer to encoded input bytes
    @param len Number of bytes to decode
    @param output Pointer to decoded output data
    @return Number of bytes successfully decoded
    @note Stops decoding if delimiter byte is found
*/
size_t cobs_decode(const uint8_t* input, void* output, size_t len);

#ifdef __cplusplus
}
#endif
