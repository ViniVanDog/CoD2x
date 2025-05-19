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
