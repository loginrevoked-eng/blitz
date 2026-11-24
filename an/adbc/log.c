#include "adbc.h"

#include <stdarg.h>
#include <stdio.h>

static void log_message(const char *level, const char *format, va_list args) {
    fprintf(stderr, "adbc: %s: ", level);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

void adbc_log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("error", format, args);
    va_end(args);
}

void adbc_log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("info", format, args);
    va_end(args);
}
