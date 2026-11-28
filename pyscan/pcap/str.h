#ifndef STR_H
#define STR_H

#include <stdarg.h>

// Allocates and formats a new string from a va_list (caller must free). Returns NULL on error.
char *strfv(const char *fmt, va_list ap);

// Allocates and formats a new string (caller must free). Returns NULL on error.
char *strf(const char *fmt, ...);

// Appends formatted text to *dst (reallocating as needed).
// If *dst is NULL, allocates a new string.
// Returns *dst on success, or NULL on error (leaving *dst intact).
char *strappf(char **dst, const char *fmt, ...);

#endif // STR_H
