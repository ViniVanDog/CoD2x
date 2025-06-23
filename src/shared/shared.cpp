#include "shared.h"

#include <cstdio>

void escape_string(char* buffer, size_t bufferSize, const void* data, size_t length) {
    // Loop through the data char by char and escape invisible characters into buffer
    size_t buffer_index = 0;
    for (size_t i = 0; i < length && buffer_index < (size_t)(bufferSize - 5); i++) { // Reserve 5 bytes for worst-case scenario
        char c = ((char*)data)[i]; // Process data directly without skipping
        switch (c) {
            case '\n':
                buffer[buffer_index++] = '\\';
                buffer[buffer_index++] = 'n';
                break;
            case '\r':
                buffer[buffer_index++] = '\\';
                buffer[buffer_index++] = 'r';
                break;
            case '\t':
                buffer[buffer_index++] = '\\';
                buffer[buffer_index++] = 't';
                break;
            default:
                if (c < 32 || c > 126) { // Non-printable characters
                    if (buffer_index + 4 < bufferSize) { // Ensure enough space for "\\xNN"
                        snprintf(&buffer[buffer_index], 5, "\\x%02X", (unsigned char)c);
                        buffer_index += 4;
                    } else {
                        break; // Exit loop if there isn't enough space
                    }
                } else {
                    buffer[buffer_index++] = c;
                }
                break;
        }
    }
    buffer[buffer_index] = '\0'; // Ensure null termination
}

// CRC16-CCITT (poly 0x1021, initial value 0xFFFF)
uint16_t crc16_ccitt(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}


int base64_encode(const uint8_t* input, size_t len, char* output, size_t out_size)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (!input || !output) return -1;

    size_t i = 0, j = 0;
    while (i < len) {
        size_t remain = len - i;
        uint32_t octet_a = i < len ? input[i++] : 0;
        uint32_t octet_b = i < len ? input[i++] : 0;
        uint32_t octet_c = i < len ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        if (j + 4 >= out_size) return -2; // Not enough space

        output[j++] = alphabet[(triple >> 18) & 0x3F];
        output[j++] = alphabet[(triple >> 12) & 0x3F];
        output[j++] = (remain > 1) ? alphabet[(triple >> 6) & 0x3F] : '=';
        output[j++] = (remain > 2) ? alphabet[triple & 0x3F] : '=';
    }
    if (j < out_size)
        output[j] = '\0';
    else
        return -2; // Not enough space for null terminator

    return (int)j; // Return encoded length
}

int base64_decode(const char* input, uint8_t* output, size_t out_size)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (!input || !output) return -1;

    // Build reverse lookup table
    int8_t reverse_table[256];
    memset(reverse_table, -1, sizeof(reverse_table));
    for (int i = 0; i < 64; ++i) {
        reverse_table[(unsigned char)alphabet[i]] = i;
    }

    size_t in_len = strlen(input);
    size_t i = 0, j = 0;
    while (i < in_len) {
        int vals[4] = {0, 0, 0, 0};
        int pad = 0;
        for (int k = 0; k < 4 && i < in_len; ++k, ++i) {
            char c = input[i];
            if (c == '=') {
                vals[k] = 0;
                pad++;
            } else {
                int8_t v = reverse_table[(unsigned char)c];
                if (v == -1) return -2; // Invalid character
                vals[k] = v;
            }
        }
        if (j + 3 > out_size) return -3; // Not enough space

        output[j++] = (vals[0] << 2) | (vals[1] >> 4);
        if (pad < 2) output[j++] = ((vals[1] & 0xF) << 4) | (vals[2] >> 2);
        if (pad < 1) output[j++] = ((vals[2] & 0x3) << 6) | vals[3];
    }
    return (int)j;
}