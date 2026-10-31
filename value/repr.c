#include "repr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map.h"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} Buffer;

static void append(Buffer *buffer, const char *text, size_t len)
{
    if (buffer->failed || len > (size_t)-1 - buffer->len - 1) {
        buffer->failed = 1;
        return;
    }
    size_t needed = buffer->len + len + 1;
    if (needed > buffer->cap) {
        size_t cap = buffer->cap ? buffer->cap : 32;
        while (cap < needed) {
            if (cap > (size_t)-1 / 2) { cap = needed; break; }
            cap *= 2;
        }
        char *data = realloc(buffer->data, cap);
        if (!data) { buffer->failed = 1; return; }
        buffer->data = data;
        buffer->cap = cap;
    }
    memcpy(buffer->data + buffer->len, text, len);
    buffer->len += len;
    buffer->data[buffer->len] = '\0';
}

static void put(Buffer *b, const char *s) { append(b, s, strlen(s)); }

static void repr(Buffer *b, const Value *v, unsigned depth)
{
    if (!v) { put(b, "<null>"); return; }
    if (depth > 64) { put(b, "..."); return; }
    char number[64];
    switch (v->kind) {
    case VALUE_NULL: put(b, "null"); break;
    case VALUE_BOOL: put(b, v->_bool == True ? "true" : "false"); break;
    case VALUE_INT: snprintf(number, sizeof number, "%d", v->_int); put(b, number); break;
    case VALUE_LONG: snprintf(number, sizeof number, "%ld", v->_long); put(b, number); break;
    case VALUE_FLOAT: snprintf(number, sizeof number, "%.9g", (double)v->_float); put(b, number); break;
    case VALUE_DOUBLE: snprintf(number, sizeof number, "%.17g", v->_double); put(b, number); break;
    case VALUE_STRING:
        put(b, "\"");
        if (v->str) for (const unsigned char *p = (const unsigned char *)v->str; *p; ++p) {
            switch (*p) {
            case '\\': put(b, "\\\\"); break;
            case '"': put(b, "\\\""); break;
            case '\n': put(b, "\\n"); break;
            case '\r': put(b, "\\r"); break;
            case '\t': put(b, "\\t"); break;
            default: append(b, (const char *)p, 1); break;
            }
        }
        put(b, "\""); break;
    case VALUE_POINTER: snprintf(number, sizeof number, "%p", v->ptr); put(b, "<pointer "); put(b, number); put(b, ">"); break;
    case VALUE_LIST: {
        put(b, "[");
        const ValueNode *node = v->list;
        while (node) { repr(b, &node->value, depth + 1); node = node->next; if (node) put(b, ", "); }
        put(b, "]"); break;
    }
    case VALUE_ARRAY:
        put(b, "[");
        if (v->array) for (size_t i = 0; i < v->array->len; ++i) {
            if (i) put(b, ", ");
            repr(b, &v->array->values[i], depth + 1);
        }
        put(b, "]"); break;
    case VALUE_MAP: {
        put(b, "{");
        int first = 1;
        if (v->map) for (size_t i = 0; i < v->map->capacity; ++i)
            for (const Entry *e = v->map->buckets[i]; e; e = e->next) {
                if (!first) put(b, ", ");
                first = 0;
                put(b, "\""); put(b, e->key); put(b, "\": ");
                repr(b, &e->value, depth + 1);
            }
        put(b, "}"); break;
    }
    default: put(b, "<value>"); break;
    }
}

char *ValueRepr(const Value *value)
{
    Buffer b = {0};
    repr(&b, value, 0);
    if (b.failed) { free(b.data); return NULL; }
    if (!b.data) {
        b.data = malloc(1);
        if (!b.data) return NULL;
        b.data[0] = '\0';
    }
    return b.data;
}