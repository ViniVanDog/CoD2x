#include "logger.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#if _WIN32 == 1
  #include <windows.h> // For CRITICAL_SECTION
#else
  #include <pthread.h>
#endif

static struct {
    size_t head;      // next write index
    size_t count;     // number of valid entries (≤ LOG_CAPACITY)
    time_t timestamps[LOG_CAPACITY];
    char   messages[LOG_CAPACITY][LOG_MSG_LEN + 1];
    #if _WIN32 == 1
        CRITICAL_SECTION cs;
    #else
        pthread_mutex_t cs;
    #endif
} lg;

void logger_add(const char *fmt, ...) {
    char message[LOG_MSG_LEN + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    time_t now = time(NULL);

    #if _WIN32 == 1
        EnterCriticalSection(&lg.cs);
    #else
        pthread_mutex_lock(&lg.cs);
    #endif

    lg.timestamps[lg.head] = now;
    strncpy(lg.messages[lg.head], message, LOG_MSG_LEN);
    lg.messages[lg.head][LOG_MSG_LEN] = '\0';
    lg.head = (lg.head + 1) % LOG_CAPACITY;
    if (lg.count < LOG_CAPACITY) lg.count++;

    #if _WIN32 == 1
        LeaveCriticalSection(&lg.cs);
    #else
        pthread_mutex_unlock(&lg.cs);
    #endif
}

size_t logger_get_recent(char *buffer, size_t max_chars) {
    if (max_chars == 0) return 0;
    size_t written = 0;
    buffer[0] = '\0';

    #if _WIN32 == 1
        EnterCriticalSection(&lg.cs);
    #else
        pthread_mutex_lock(&lg.cs);
    #endif

    if (lg.count == 0) {
        #if _WIN32 == 1
                LeaveCriticalSection(&lg.cs);
        #else
                pthread_mutex_unlock(&lg.cs);
        #endif
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

    #if _WIN32 == 1
        LeaveCriticalSection(&lg.cs);
    #else
        pthread_mutex_unlock(&lg.cs);
    #endif

    return written;
}

void logger_init(void) {
    lg.head  = 0;
    lg.count = 0;
    #if _WIN32 == 1
        InitializeCriticalSection(&lg.cs);
    #else
        pthread_mutex_init(&lg.cs, NULL);
    #endif
}

void logger_destroy(void) {
    #if _WIN32 == 1
        DeleteCriticalSection(&lg.cs);
    #else
        pthread_mutex_destroy(&lg.cs);
    #endif
}