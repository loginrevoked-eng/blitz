#include "json.h"

#include "jsmn.h"
#include "map.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} JsonBuffer;

static int append(JsonBuffer *buffer, const char *text, size_t length)
{
    if (length > (size_t)-1 - buffer->length - 1)
        return 0;

    size_t needed = buffer->length + length + 1;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 64;
        while (capacity < needed) {
            if (capacity > (size_t)-1 / 2) {
                capacity = needed;
                break;
            }
            capacity *= 2;
        }
        char *data = realloc(buffer->data, capacity);
        if (!data)
            return 0;
        buffer->data = data;
        buffer->capacity = capacity;
    }

    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int put(JsonBuffer *buffer, const char *text)
{
    return append(buffer, text, strlen(text));
}

static int append_string(JsonBuffer *buffer, const char *string)
{
    static const char hex[] = "0123456789abcdef";
    if (!put(buffer, "\""))
        return 0;
    if (string) {
        for (const unsigned char *p = (const unsigned char *)string; *p; ++p) {
            char escaped[6];
            switch (*p) {
            case '"': if (!put(buffer, "\\\"")) return 0; break;
            case '\\': if (!put(buffer, "\\\\")) return 0; break;
            case '\b': if (!put(buffer, "\\b")) return 0; break;
            case '\f': if (!put(buffer, "\\f")) return 0; break;
            case '\n': if (!put(buffer, "\\n")) return 0; break;
            case '\r': if (!put(buffer, "\\r")) return 0; break;
            case '\t': if (!put(buffer, "\\t")) return 0; break;
            default:
                if (*p < 0x20) {
                    escaped[0] = '\\'; escaped[1] = 'u';
                    escaped[2] = '0'; escaped[3] = '0';
                    escaped[4] = hex[*p >> 4]; escaped[5] = hex[*p & 0x0f];
                    if (!append(buffer, escaped, sizeof escaped)) return 0;
                } else if (!append(buffer, (const char *)p, 1)) {
                    return 0;
                }
            }
        }
    }
    return put(buffer, "\"");
}

static int serialize(JsonBuffer *buffer, const Value *value, unsigned depth)
{
    if (!value || depth > 64)
        return 0;

    char number[64];
    switch (value->kind) {
    case VALUE_NULL:
        return put(buffer, "null");
    case VALUE_BOOL:
        return put(buffer, value->_bool == True ? "true" : "false");
    case VALUE_INT:
        snprintf(number, sizeof number, "%d", value->_int);
        return put(buffer, number);
    case VALUE_UNSIGNED_INT:
        snprintf(number, sizeof number, "%u", value->_unsigned_int);
        return put(buffer, number);
    case VALUE_LONG:
        snprintf(number, sizeof number, "%ld", value->_long);
        return put(buffer, number);
    case VALUE_FLOAT:
        if (!isfinite(value->_float)) return 0;
        snprintf(number, sizeof number, "%.9g", (double)value->_float);
        return put(buffer, number);
    case VALUE_DOUBLE:
        if (!isfinite(value->_double)) return 0;
        snprintf(number, sizeof number, "%.17g", value->_double);
        return put(buffer, number);
    case VALUE_STRING:
        return value->str ? append_string(buffer, value->str) : put(buffer, "null");
    case VALUE_LIST: {
        if (!put(buffer, "[")) return 0;
        for (const ValueNode *node = value->list; node; node = node->next) {
            if (node != value->list && !put(buffer, ",")) return 0;
            if (!serialize(buffer, &node->value, depth + 1)) return 0;
        }
        return put(buffer, "]");
    }
    case VALUE_ARRAY:
        if (!put(buffer, "[")) return 0;
        if (value->array) {
            for (size_t i = 0; i < value->array->len; ++i) {
                if (i && !put(buffer, ",")) return 0;
                if (!serialize(buffer, &value->array->values[i], depth + 1)) return 0;
            }
        }
        return put(buffer, "]");
    case VALUE_MAP: {
        if (!put(buffer, "{")) return 0;
        int first = 1;
        if (value->map) {
            for (size_t i = 0; i < value->map->capacity; ++i) {
                for (const Entry *entry = value->map->buckets[i]; entry; entry = entry->next) {
                    if (!first && !put(buffer, ",")) return 0;
                    first = 0;
                    if (!append_string(buffer, entry->key) || !put(buffer, ":") ||
                        !serialize(buffer, &entry->value, depth + 1)) return 0;
                }
            }
        }
        return put(buffer, "}");
    }
    case VALUE_POINTER:
    default:
        return 0;
    }
}

char *ValueToJson(const Value *value)
{
    if (!value || (value->kind != VALUE_ARRAY &&
                   value->kind != VALUE_LIST &&
                   value->kind != VALUE_MAP))
        return NULL;

    JsonBuffer buffer = {0};
    if (!serialize(&buffer, value, 0)) {
        free(buffer.data);
        return NULL;
    }
    return buffer.data;
}

static void parsed_contents_free(Value *value)
{
    if (!value)
        return;

    switch (value->kind) {
    case VALUE_STRING:
        free(value->str);
        break;
    case VALUE_LIST: {
        ValueNode *node = value->list;
        while (node) {
            ValueNode *next = node->next;
            parsed_contents_free(&node->value);
            free(node);
            node = next;
        }
        break;
    }
    case VALUE_ARRAY:
        if (value->array) {
            for (size_t i = 0; i < value->array->len; ++i)
                parsed_contents_free(&value->array->values[i]);
            free(value->array->values);
            free(value->array);
        }
        break;
    case VALUE_MAP:
        if (value->map) {
            for (size_t i = 0; i < value->map->capacity; ++i) {
                Entry *entry = value->map->buckets[i];
                while (entry) {
                    Entry *next = entry->next;
                    free(entry->key);
                    parsed_contents_free(&entry->value);
                    free(entry);
                    entry = next;
                }
            }
            free(value->map->buckets);
            free(value->map);
        }
        break;
    default:
        break;
    }
}

static void parsed_value_free(Value *value)
{
    parsed_contents_free(value);
    free(value);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex4(const char *text, unsigned *result)
{
    unsigned value = 0;
    for (int i = 0; i < 4; ++i) {
        int digit = hex_value(text[i]);
        if (digit < 0)
            return 0;
        value = (value << 4) | (unsigned)digit;
    }
    *result = value;
    return 1;
}

static int append_utf8(char *out, size_t *length, unsigned cp)
{
    if (cp == 0) /* Value strings are NUL-terminated. */
        return 0;
    if (cp <= 0x7f) {
        out[(*length)++] = (char)cp;
    } else if (cp <= 0x7ff) {
        out[(*length)++] = (char)(0xc0 | (cp >> 6));
        out[(*length)++] = (char)(0x80 | (cp & 0x3f));
    } else if (cp <= 0xffff) {
        out[(*length)++] = (char)(0xe0 | (cp >> 12));
        out[(*length)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[(*length)++] = (char)(0x80 | (cp & 0x3f));
    } else if (cp <= 0x10ffff) {
        out[(*length)++] = (char)(0xf0 | (cp >> 18));
        out[(*length)++] = (char)(0x80 | ((cp >> 12) & 0x3f));
        out[(*length)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[(*length)++] = (char)(0x80 | (cp & 0x3f));
    } else {
        return 0;
    }
    return 1;
}

static char *decode_string(const char *json, const jsmntok_t *token)
{
    size_t source_length = (size_t)(token->end - token->start);
    char *out = malloc(source_length + 1);
    if (!out)
        return NULL;

    size_t written = 0;
    for (size_t i = 0; i < source_length; ++i) {
        unsigned char c = (unsigned char)json[token->start + (int)i];
        if (c != '\\') {
            if (c < 0x20) { free(out); return NULL; }
            out[written++] = (char)c;
            continue;
        }

        if (++i >= source_length) { free(out); return NULL; }
        c = (unsigned char)json[token->start + (int)i];
        switch (c) {
        case '"': out[written++] = '"'; break;
        case '\\': out[written++] = '\\'; break;
        case '/': out[written++] = '/'; break;
        case 'b': out[written++] = '\b'; break;
        case 'f': out[written++] = '\f'; break;
        case 'n': out[written++] = '\n'; break;
        case 'r': out[written++] = '\r'; break;
        case 't': out[written++] = '\t'; break;
        case 'u': {
            if (i + 4 >= source_length) { free(out); return NULL; }
            unsigned cp;
            if (!parse_hex4(json + token->start + (int)i + 1, &cp)) {
                free(out); return NULL;
            }
            i += 4;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 6 >= source_length ||
                    json[token->start + (int)i + 1] != '\\' ||
                    json[token->start + (int)i + 2] != 'u') {
                    free(out); return NULL;
                }
                unsigned low;
                if (!parse_hex4(json + token->start + (int)i + 3, &low) ||
                    low < 0xdc00 || low > 0xdfff) {
                    free(out); return NULL;
                }
                cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                i += 6;
            } else if (cp >= 0xdc00 && cp <= 0xdfff) {
                free(out); return NULL;
            }
            if (!append_utf8(out, &written, cp)) { free(out); return NULL; }
            break;
        }
        default:
            free(out);
            return NULL;
        }
    }
    out[written] = '\0';
    return out;
}

static Value *parse_json_value(const char *json, const jsmntok_t *tokens,
                               int token_count, int *cursor, unsigned depth);

static Value *parse_json_number(const char *json, const jsmntok_t *token)
{
    size_t length = (size_t)(token->end - token->start);
    char *text = malloc(length + 1);
    if (!text)
        return NULL;
    memcpy(text, json + token->start, length);
    text[length] = '\0';

    char *p = text;
    if (*p == '-') ++p;
    if (*p == '0') {
        ++p;
        if (*p >= '0' && *p <= '9') { free(text); return NULL; }
    } else {
        if (*p < '1' || *p > '9') { free(text); return NULL; }
        while (*p >= '0' && *p <= '9') ++p;
    }
    int is_integer = 1;
    if (*p == '.') {
        is_integer = 0;
        ++p;
        if (*p < '0' || *p > '9') { free(text); return NULL; }
        while (*p >= '0' && *p <= '9') ++p;
    }
    if (*p == 'e' || *p == 'E') {
        is_integer = 0;
        ++p;
        if (*p == '+' || *p == '-') ++p;
        if (*p < '0' || *p > '9') { free(text); return NULL; }
        while (*p >= '0' && *p <= '9') ++p;
    }
    if (*p != '\0') { free(text); return NULL; }

    Value *value = NULL;
    if (is_integer) {
        if (text[0] != '-') {
            errno = 0;
            char *unsigned_end;
            unsigned long long unsigned_number = strtoull(text, &unsigned_end, 10);
            if (errno == ERANGE || *unsigned_end != '\0' || unsigned_number > UINT32_MAX) {
                free(text);
                return NULL;
            }
            value = ValueNewUnsignedInt(unsigned_number);
            free(text);
            return value;
        }
        errno = 0;
        char *end;
        long number = strtol(text, &end, 10);
        if (errno != ERANGE && *end == '\0') {
            if (number >= INT_MIN && number <= INT_MAX)
                value = ValueNewInt((int)number);
            else
                value = ValueNewLong(number);
        }
    }
    if (!value) {
        errno = 0;
        char *end;
        double number = strtod(text, &end);
        if (errno != ERANGE && *end == '\0' && isfinite(number))
            value = ValueNewDouble(number);
    }
    free(text);
    return value;
}

static Value *parse_json_value(const char *json, const jsmntok_t *tokens,
                               int token_count, int *cursor, unsigned depth)
{
    if (depth > 64 || *cursor >= token_count)
        return NULL;

    const jsmntok_t *token = &tokens[(*cursor)++];
    if (token->type == JSMN_STRING) {
        char *text = decode_string(json, token);
        if (!text)
            return NULL;
        Value *value = ValueNewString(text);
        if (!value) free(text);
        return value;
    }
    if (token->type == JSMN_PRIMITIVE) {
        size_t length = (size_t)(token->end - token->start);
        const char *text = json + token->start;
        if (length == 4 && memcmp(text, "true", 4) == 0)
            return ValueNewBool(True);
        if (length == 5 && memcmp(text, "false", 5) == 0)
            return ValueNewBool(False);
        if (length == 4 && memcmp(text, "null", 4) == 0)
            return ValueNewNull();
        return parse_json_number(json, token);
    }

    if (token->type == JSMN_ARRAY) {
        Value *array = ValueNew(VALUE_ARRAY, NULL);
        if (!array)
            return NULL;
        for (int i = 0; i < token->size; ++i) {
            Value *item = parse_json_value(json, tokens, token_count, cursor, depth + 1);
            if (!item || !ValueAppend(array, item)) {
                if (item) { parsed_contents_free(item); free(item); }
                parsed_contents_free(array);
                free(array);
                return NULL;
            }
            free(item); /* The array now owns the copied value contents. */
        }
        return array;
    }

    if (token->type == JSMN_OBJECT) {
        Value *object = ValueNew(VALUE_MAP, NULL);
        if (!object)
            return NULL;
        for (int i = 0; i < token->size; ++i) {
            if (*cursor >= token_count || tokens[*cursor].type != JSMN_STRING) {
                parsed_contents_free(object);
                free(object);
                return NULL;
            }
            char *key = decode_string(json, &tokens[(*cursor)++]);
            Value *item = key ? parse_json_value(json, tokens, token_count, cursor, depth + 1) : NULL;
            if (!key || !item) {
                free(key);
                if (item) { parsed_contents_free(item); free(item); }
                parsed_contents_free(object);
                free(object);
                return NULL;
            }

            Value *old = MapGet(object->map, key);
            if (old)
                parsed_contents_free(old);
            int stored = MapSet(object->map, key, *item);
            free(key);
            if (!stored) {
                parsed_contents_free(item);
                free(item);
                parsed_contents_free(object);
                free(object);
                return NULL;
            }
            free(item); /* The map now owns the copied value contents. */
        }
        return object;
    }
    return NULL;
}

static Value *parse_json_document(const char *json)
{
    if (!json)
        return NULL;
    size_t length = strlen(json);
    if (length > INT_MAX)
        return NULL;

    jsmn_parser parser;
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json, length, NULL, 0);
    if (token_count <= 0)
        return NULL;

    jsmntok_t *tokens = malloc((size_t)token_count * sizeof(*tokens));
    if (!tokens)
        return NULL;
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, length, tokens, (unsigned int)token_count);
    if (parsed != token_count) {
        free(tokens);
        return NULL;
    }

    int cursor = 0;
    Value *value = parse_json_value(json, tokens, token_count, &cursor, 0);
    free(tokens);
    if (!value || cursor != token_count) {
        if (value) { parsed_contents_free(value); free(value); }
        return NULL;
    }
    value->free = parsed_value_free;
    return value;
}

int ParseJson(Value *json, char *jsonstr)
{
    if (!json || !jsonstr || json->kind != VALUE_NULL || json->free != NULL)
        return 0;

    Value *parsed = parse_json_document(jsonstr);
    if (!parsed)
        return 0;

    *json = *parsed;
    free(parsed); /* Transfer the parsed tree into the caller's Value. */
    return 1;
}
