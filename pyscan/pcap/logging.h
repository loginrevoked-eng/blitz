#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>

enum log_level {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_OFF
};

void log_set_level(enum log_level level);
enum log_level log_get_level(void);
enum log_level log_set_off(void);
void debug(const char *fmt, ...);
void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
void fatal(const char *fmt, ...);
void panic(int err, const char *fmt, ...);

#endif // LOGGING_H
