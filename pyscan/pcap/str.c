#include "str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



char *strfv(const char *fmt, va_list ap) {
    if (!fmt) {
        return NULL;
    }

    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (len < 0) {
        return NULL;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        return NULL;
    }

    vsnprintf(buf, (size_t)len + 1, fmt, ap);
    return buf;
}

char *strf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *res = strfv(fmt, ap);
    va_end(ap);
    return res;
}

char *strappf(char **dst, const char *fmt, ...) {
    if (!dst || !fmt) {
        return NULL;
    }

    va_list args;
    va_start(args, fmt);
    int add_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (add_len < 0) {
        return NULL;
    }

    size_t old_len = (*dst != NULL) ? strlen(*dst) : 0;
    size_t new_size = old_len + (size_t)add_len + 1;

    char *resized = (char *)realloc(*dst, new_size);
    if (!resized) {
        return NULL;
    }

    va_start(args, fmt);
    int written = vsnprintf(resized + old_len, (size_t)add_len + 1, fmt, args);
    va_end(args);

    if (written < 0) {
        // Keep resized buffer valid at old_len if formatting fails
        resized[old_len] = '\0';
        *dst = resized;
        return NULL;
    }

    *dst = resized;
    return resized;
}
