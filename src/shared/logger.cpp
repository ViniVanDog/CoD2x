#include "logger.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <windows.h> // For CRITICAL_SECTION

static struct {
    size_t head;      // next write index
    size_t count;     // number of valid entries (≤ LOG_CAPACITY)
    time_t timestamps[LOG_CAPACITY];
    char   messages[LOG_CAPACITY][LOG_MSG_LEN + 1];
    CRITICAL_SECTION cs;
} lg;

void logger_add(const char *fmt, ...) {
    char message[LOG_MSG_LEN + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    time_t now = time(NULL);

    EnterCriticalSection(&lg.cs);

    lg.timestamps[lg.head] = now;
    strncpy(lg.messages[lg.head], message, LOG_MSG_LEN);
    lg.messages[lg.head][LOG_MSG_LEN] = '\0';
    lg.head = (lg.head + 1) % LOG_CAPACITY;
    if (lg.count < LOG_CAPACITY) lg.count++;

    LeaveCriticalSection(&lg.cs);
}

size_t logger_get_recent(char *buffer, size_t max_chars) {
    if (max_chars == 0) return 0;
    size_t written = 0;
    buffer[0] = '\0';

    EnterCriticalSection(&lg.cs);

    if (lg.count == 0) {
        LeaveCriticalSection(&lg.cs);
        return 0;
    }

    ssize_t newest_idx = (ssize_t)lg.head - 1;
    if (newest_idx < 0) newest_idx += LOG_CAPACITY;
    time_t t0 = lg.timestamps[newest_idx];

    ssize_t idx = newest_idx;
    for (size_t n = 0; n < lg.count; n++) {
        time_t t = lg.timestamps[idx];
        long offset = (long)(t0 - t);

        char line[32 + LOG_MSG_LEN + 4];
        int len = snprintf(line, sizeof(line), "%lds: %s\n", offset, lg.messages[idx]);
        if (len < 0) break;
        if (written + (size_t)len > max_chars) break;

        memcpy(buffer + written, line, len);
        written += len;
        buffer[written] = '\0';

        idx = (idx - 1 + LOG_CAPACITY) % LOG_CAPACITY;
    }

    LeaveCriticalSection(&lg.cs);

    return written;
}

void logger_init(void) {
    lg.head  = 0;
    lg.count = 0;
    InitializeCriticalSection(&lg.cs);
}

void logger_destroy(void) {
    DeleteCriticalSection(&lg.cs);
}