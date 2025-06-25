#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>   // for size_t
#include <time.h>     // for time_t

#define LOG_CAPACITY  50      // max entries
#define LOG_MSG_LEN   512     // max chars per message (excl. NUL)

// Add a message (truncated to LOG_MSG_LEN) with current timestamp.
void logger_add(const char *fmt, ...);

// Write up to max_chars of the most recent logs into buffer,
// each line formatted as "<offset>s: message\n", where offset is
// seconds since the newest log (newest is always “0s”).
// Returns number of chars written (excluding terminating NUL).
size_t logger_get_recent(char *buffer, size_t max_chars);

// Must call once before any logging.
void logger_init(void);

#endif // LOGGER_H
