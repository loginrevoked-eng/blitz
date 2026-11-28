#include "logging.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>


static enum log_level current_level = LOG_INFO;

static const char *level_names[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

enum log_level log_set_off(void)
{
    enum log_level p = current_level;
    current_level = LOG_OFF;
    return p;
}

void log_set_level(enum log_level level) {
    current_level = level;
}

enum log_level log_get_level(void) {
    return current_level;
}

static void __log(enum log_level level, const char *fmt, va_list args) {
    if (level < current_level || level >= LOG_OFF) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);

    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    FILE *stream = (level >= LOG_WARN) ? stderr : stdout;

    fprintf(stream, "[%s] [%-5s] ", time_buf, level_names[level]);
    vfprintf(stream, fmt, args);
    fprintf(stream, "\n");
    fflush(stream);
}

void debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __log(LOG_DEBUG, fmt, args);
    va_end(args);
}

void info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __log(LOG_INFO, fmt, args);
    va_end(args);
}

void warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __log(LOG_WARN, fmt, args);
    va_end(args);
}

void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __log(LOG_ERROR, fmt, args);
    va_end(args);
}

void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __log(LOG_FATAL, fmt, args);
    va_end(args);
}


void panic(int err, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    __log(LOG_FATAL, fmt, args);
    va_end(args);
    exit(err);
}
