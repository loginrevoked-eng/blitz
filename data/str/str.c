#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "str.h"

char *appendf(char **old, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len < 0)
    {
        va_end(args);
        return NULL;
    }

    size_t old_len = *old ? strlen(*old) : 0;

    char *new = realloc(*old, old_len + len + 1);
    if (!new)
    {
        va_end(args);
        return NULL;
    }

    vsnprintf(new + old_len, len + 1, fmt, args);
    va_end(args);

    *old = new;
    return new;
}



char *strf(const char *fmt, ...)
{
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);

    int len = vsnprintf(NULL, 0, fmt, copy);   /* pass 1: measure */
    va_end(copy);

    if (len < 0)
    {
        va_end(args);
        return NULL;
    }

    char *out = malloc((size_t)len + 1);
    if (out)
        vsnprintf(out, (size_t)len + 1, fmt, args);   /* pass 2: write */
    va_end(args);

    return out;
}