#ifndef LOGGING_H
#define LOGGING_H

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_LOG,
    LOG_LEVEL_QUIET
} LogLevel;

void LoggingOutputLock(void);
void LoggingOutputUnlock(void);
void SetLogLevel(LogLevel new_level);
void Log(const char *fmt, ...);
void Debug(const char *fmt, ...);
void Info(const char *fmt, ...);
void Warn(const char *fmt, ...);
void Panic(const char *fmt, ...);

#endif
