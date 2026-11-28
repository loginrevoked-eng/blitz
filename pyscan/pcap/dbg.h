#include <stdio.h>
#include <string.h>
#include "logging.h"

typedef enum { SYM_INT, SYM_UINT, SYM_DOUBLE, SYM_STR, SYM_ULL } SymType;

typedef struct {
    const char *name;
    SymType     type;
    const void *ptr;   /* points at the live variable */
} Symbol;

/* picks the type tag automatically (C11). unsupported types fail to compile */
#define SYM(var) { #var, _Generic((var),      \
    int:          SYM_INT,                    \
    unsigned int: SYM_UINT,                   \
    double:       SYM_DOUBLE,                 \
    unsigned long long: SYM_ULL,              \
    char *:       SYM_STR,                    \
    const char *: SYM_STR), &(var) }

#ifndef DBGBRK

/* release build: does nothing, the casts just silence unused warnings */
#define debug_brk(syms, n) ((void)(syms), (void)(n))

#else

static void print_symbol(const Symbol *s)
{
    printf("%s = ", s->name);
    switch (s->type) {
    case SYM_INT:    printf("%d\n", *(const int *)s->ptr); break;
    case SYM_UINT:   printf("%u\n", *(const unsigned int *)s->ptr); break;
    case SYM_DOUBLE: printf("%g\n", *(const double *)s->ptr); break;
    case SYM_ULL: {
        unsigned long long v = *(const unsigned long long *)s->ptr;
        printf("%llu (0x%llX)\n", v, v);
        break;
    }
    case SYM_STR: {
        const char *str = *(const char *const *)s->ptr;
        if (str) printf("\"%s\"\n", str);
        else     puts("NULL");
        break;
    }
    }
}

static const Symbol *find_symbol(const Symbol *syms, size_t n, const char *name)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(syms[i].name, name) == 0)
            return &syms[i];
    return NULL;
}

void debug_brk(const Symbol *syms, size_t n)
{
    enum log_level old_log_level = log_set_off();
    char line[128];

    for (;;) {
        printf(">put symbol here: ");
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin))   /* EOF */
            break;

        size_t len = strcspn(line, "\r\n");
        if (line[len] == '\0' && !feof(stdin)) { /* too long, drop the rest */
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
            puts("input too long");
            continue;
        }
        line[len] = '\0';

        const Symbol *s = find_symbol(syms, n, line);
        if (s)
            print_symbol(s);
        else if (strcmp(line, "c") == 0 || strcmp(line, "q") == 0)
            break;
        else if (strcmp(line, "ls") == 0)
            for (size_t i = 0; i < n; i++) puts(syms[i].name);
        else
            printf("unknown symbol: '%s'\n", line);
    }
    log_set_level(old_log_level);
}

#endif /* DEBUG */
