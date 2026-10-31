#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "panic.h"


static char* PROGRAM_NAME_INDENT = "         ";
static char *PROGRAM_NAME = NULL;
static char *RED_ANSI = "\x1b[31m";
static char *RESET_ANSI = "\x1b[0m";
static char* UNDERLINE_ANSI = "\x1b[4m";


void SetPrefix(char *prefix)
{
    PROGRAM_NAME = prefix;
}
static char *GetPrefix(void)
{
    if (PROGRAM_NAME) return PROGRAM_NAME;

    return "Argument Parsing Error";
}

static void MoolahCliPanicV(const char *fmt, va_list args)
{
    fprintf(stderr, "%s%s%s%s\n%serror: %s", PROGRAM_NAME_INDENT, UNDERLINE_ANSI, GetPrefix(), RESET_ANSI, RED_ANSI, RESET_ANSI);
    vfprintf(stderr, fmt, args);
    exit(1);
}

void MoolahCliPanic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    MoolahCliPanicV(fmt, args);
    va_end(args);
}

void MoolahCliPanicIf(int condition, const char *fmt, ...)
{
    if (condition)
    {
        va_list args;
        va_start(args, fmt);
        MoolahCliPanicV(fmt, args);
        va_end(args);
    }
}
