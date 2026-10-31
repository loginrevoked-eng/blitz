#include "logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define ANSI_RED "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

static LogLevel level = LOG_LEVEL_ERROR;
static SRWLOCK output_lock = SRWLOCK_INIT;

void LoggingOutputLock(void)
{
    AcquireSRWLockExclusive(&output_lock);
}

void LoggingOutputUnlock(void)
{
    ReleaseSRWLockExclusive(&output_lock);
}

static void LogV(int fd, LogLevel message_level, const char *fmt, ...);

static void LogArgs(int fd, LogLevel message_level, const char *fmt, va_list args)
{
    if (message_level != LOG_LEVEL_FATAL && message_level < level)
        return;

    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        LogV(fd, message_level, "(failed to format log message)\n");
        return;
    }

    char *message = malloc((size_t)length + 1);
    if (message == NULL) {
        LogV(fd, message_level, "(failed to allocate log message)\n");
        return;
    }

    vsnprintf(message, (size_t)length + 1, fmt, args);
    LogV(fd, message_level, "%s", message);
    free(message);
}

static void LogV(int fd, LogLevel message_level, const char *fmt, ...)
{
    if (message_level != LOG_LEVEL_FATAL && message_level < level)
        return;

    FILE *stream = fd == 1 ? stdout : stderr;
    const char *label;
    switch (message_level) {
    case LOG_LEVEL_DEBUG: label = "debug"; break;
    case LOG_LEVEL_INFO:  label = "info"; break;
    case LOG_LEVEL_WARN:  label = "warn"; break;
    case LOG_LEVEL_ERROR: label = "error"; break;
    case LOG_LEVEL_FATAL: label = "fatal"; break;
    case LOG_LEVEL_LOG:   label = "log"; break;
    default:              label = "log"; break;
    }

    /* The marquee shares stdout and leaves its cursor on the active line. */
    if (fd == 1)
        fputs("\r\x1b[2K", stream);

    if (message_level == LOG_LEVEL_FATAL)
        fprintf(stream, "%s(fatal):%s ", ANSI_RED, ANSI_RESET);
    else
        fprintf(stream, "%s: ", label);

    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
    fflush(stream);
}

void SetLogLevel(LogLevel new_level)
{
    LoggingOutputLock();
    if (new_level >= LOG_LEVEL_DEBUG && new_level <= LOG_LEVEL_QUIET)
        level = new_level;
    LoggingOutputUnlock();
}

void Log(const char *fmt, ...)
{
    LoggingOutputLock();
    va_list args;
    va_start(args, fmt);
    LogArgs(1, LOG_LEVEL_LOG, fmt, args);
    va_end(args);
    LoggingOutputUnlock();
}

void Debug(const char *fmt, ...)
{
    LoggingOutputLock();
    va_list args;
    va_start(args, fmt);
    LogArgs(2, LOG_LEVEL_DEBUG, fmt, args);
    va_end(args);
    LoggingOutputUnlock();
}

void Info(const char *fmt, ...)
{
    LoggingOutputLock();
    va_list args;
    va_start(args, fmt);
    LogArgs(1, LOG_LEVEL_INFO, fmt, args);
    va_end(args);
    LoggingOutputUnlock();
}

void Warn(const char *fmt, ...)
{
    LoggingOutputLock();
    va_list args;
    va_start(args, fmt);
    LogArgs(2, LOG_LEVEL_WARN, fmt, args);
    va_end(args);
    LoggingOutputUnlock();
}

void Panic(const char *fmt, ...)
{
    LoggingOutputLock();
    va_list args;
    va_start(args, fmt);
    LogArgs(2, LOG_LEVEL_FATAL, fmt, args);
    va_end(args);
    LoggingOutputUnlock();
    exit(EXIT_FAILURE);
}
