#include "hashset.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct HashSetEntry {
    int is_u64;
    uint64_t u64;
    Value value;
    HashSetEntry *next;
};

static unsigned long HashBytes(unsigned long hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size; i++)
        hash = ((hash << 5) + hash) + bytes[i];
    return hash;
}

static unsigned long HashValue(Value value)
{
    unsigned long hash = HashBytes(5381, &value.kind, sizeof value.kind);
    switch (value.kind) {
        case VALUE_BOOL: return HashBytes(hash, &value.b, sizeof value.b);
        case VALUE_INT: return HashBytes(hash, &value.i, sizeof value.i);
        case VALUE_LONG: return HashBytes(hash, &value.l, sizeof value.l);
        case VALUE_DOUBLE: {
            double normalized = value.d == 0.0 ? 0.0 : value.d;
            return HashBytes(hash, &normalized, sizeof normalized);
        }
        case VALUE_STRING:
            return value.s ? HashBytes(hash, value.s, strlen(value.s)) : hash;
        case VALUE_POINTER: {
            uintptr_t pointer = (uintptr_t)value.p;
            return HashBytes(hash, &pointer, sizeof pointer);
        }
        case VALUE_LIST:
            for (ValueNode *node = value.list; node; node = node->next)
                hash = HashValue(node->value) + (hash * 33);
            return hash;
        case VALUE_NULL:
        case VALUE_UNKNOWN:
            return hash;
    }
    return hash;
}

static unsigned long HashU64(uint64_t value)
{
    const unsigned char type_tag = 0xFF;
    unsigned long hash = HashBytes(5381, &type_tag, sizeof type_tag);
    return HashBytes(hash, &value, sizeof value);
}

static int ValueEqual(Value left, Value right)
{
    if (left.kind != right.kind)
        return 0;
    switch (left.kind) {
        case VALUE_BOOL: return left.b == right.b;
        case VALUE_INT: return left.i == right.i;
        case VALUE_LONG: return left.l == right.l;
        case VALUE_DOUBLE: return left.d == right.d;
        case VALUE_STRING:
            return left.s == right.s || (left.s && right.s && strcmp(left.s, right.s) == 0);
        case VALUE_POINTER: return left.p == right.p;
        case VALUE_LIST: {
            ValueNode *a = left.list, *b = right.list;
            while (a && b) {
                if (!ValueEqual(a->value, b->value)) return 0;
                a = a->next;
                b = b->next;
            }
            return a == NULL && b == NULL;
        }
        case VALUE_NULL:
        case VALUE_UNKNOWN:
            return 1;
    }
    return 0;
}

static void ValueContentsFree(Value *value)
{
    if (value->kind == VALUE_STRING) {
        free(value->s);
    } else if (value->kind == VALUE_LIST) {
        ValueNode *node = value->list;
        while (node) {
            ValueNode *next = node->next;
            ValueContentsFree(&node->value);
            free(node);
            node = next;
        }
    }
}

static int ValueContentsClone(Value *destination, Value source)
{
    *destination = source;
    destination->free = NULL;
    if (source.kind == VALUE_STRING) {
        destination->s = source.s ? strdup(source.s) : NULL;
        return source.s == NULL || destination->s != NULL;
    }
    if (source.kind == VALUE_LIST) {
        destination->list = NULL;
        ValueNode **tail = &destination->list;
        for (ValueNode *node = source.list; node; node = node->next) {
            ValueNode *copy = calloc(1, sizeof *copy);
            if (copy == NULL || !ValueContentsClone(&copy->value, node->value)) {
                free(copy);
                ValueContentsFree(destination);
                destination->list = NULL;
                return 0;
            }
            *tail = copy;
            tail = &copy->next;
        }
    }
    return 1;
}

HashSet *HashSet_New(size_t capacity)
{
    if (capacity == 0)
        return NULL;
    HashSet *set = malloc(sizeof *set);
    if (set == NULL)
        return NULL;
    set->buckets = calloc(capacity, sizeof *set->buckets);
    set->capacity = capacity;
    if (set->buckets == NULL) {
        free(set);
        return NULL;
    }
    return set;
}

int HashSet_Add(HashSet *set, Value value)
{
    if (set == NULL || set->capacity == 0)
        return 0;
    size_t index = HashValue(value) % set->capacity;
    for (HashSetEntry *entry = set->buckets[index]; entry; entry = entry->next)
        if (!entry->is_u64 && ValueEqual(entry->value, value)) return 0;

    HashSetEntry *entry = calloc(1, sizeof *entry);
    if (entry == NULL || !ValueContentsClone(&entry->value, value)) {
        free(entry);
        return 0;
    }
    entry->next = set->buckets[index];
    set->buckets[index] = entry;
    return 1;
}

int HashSet_Contains(const HashSet *set, Value value)
{
    if (set == NULL || set->capacity == 0)
        return 0;
    size_t index = HashValue(value) % set->capacity;
    for (HashSetEntry *entry = set->buckets[index]; entry; entry = entry->next)
        if (!entry->is_u64 && ValueEqual(entry->value, value)) return 1;
    return 0;
}

int HashSet_Remove(HashSet *set, Value value)
{
    if (set == NULL || set->capacity == 0)
        return 0;
    size_t index = HashValue(value) % set->capacity;
    HashSetEntry **link = &set->buckets[index];
    while (*link) {
        HashSetEntry *entry = *link;
        if (!entry->is_u64 && ValueEqual(entry->value, value)) {
            *link = entry->next;
            ValueContentsFree(&entry->value);
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

int HashSet_PutString(HashSet *set, const char *value)
{
    if (value == NULL)
        return 0;
    Value key = { .kind = VALUE_STRING, .s = (char *)value };
    return HashSet_Add(set, key);
}

int HashSet_ContainsString(const HashSet *set, const char *value)
{
    if (value == NULL)
        return 0;
    Value key = { .kind = VALUE_STRING, .s = (char *)value };
    return HashSet_Contains(set, key);
}

int HashSet_RemoveString(HashSet *set, const char *value)
{
    if (value == NULL)
        return 0;
    Value key = { .kind = VALUE_STRING, .s = (char *)value };
    return HashSet_Remove(set, key);
}

int HashSet_PutU64(HashSet *set, uint64_t value)
{
    if (set == NULL || set->capacity == 0)
        return 0;
    size_t index = HashU64(value) % set->capacity;
    for (HashSetEntry *entry = set->buckets[index]; entry; entry = entry->next)
        if (entry->is_u64 && entry->u64 == value) return 0;

    HashSetEntry *entry = calloc(1, sizeof *entry);
    if (entry == NULL)
        return 0;
    entry->is_u64 = 1;
    entry->u64 = value;
    entry->next = set->buckets[index];
    set->buckets[index] = entry;
    return 1;
}

int HashSet_ContainsU64(const HashSet *set, uint64_t value)
{
    if (set == NULL || set->capacity == 0)
        return 0;
    size_t index = HashU64(value) % set->capacity;
    for (HashSetEntry *entry = set->buckets[index]; entry; entry = entry->next)
        if (entry->is_u64 && entry->u64 == value) return 1;
    return 0;
}

int HashSet_RemoveU64(HashSet *set, uint64_t value)
{
    if (set == NULL || set->capacity == 0)
        return 0;
    size_t index = HashU64(value) % set->capacity;
    HashSetEntry **link = &set->buckets[index];
    while (*link) {
        HashSetEntry *entry = *link;
        if (entry->is_u64 && entry->u64 == value) {
            *link = entry->next;
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

void HashSet_ForEachU64(const HashSet *set, HashSetU64Callback callback, void *context)
{
    if (set == NULL || callback == NULL)
        return;
    for (size_t i = 0; i < set->capacity; i++) {
        for (HashSetEntry *entry = set->buckets[i]; entry; entry = entry->next) {
            if (entry->is_u64)
                callback(entry->u64, context);
        }
    }
}

void HashSet_Free(HashSet *set)
{
    if (set == NULL)
        return;
    for (size_t i = 0; i < set->capacity; i++) {
        HashSetEntry *entry = set->buckets[i];
        while (entry) {
            HashSetEntry *next = entry->next;
            if (!entry->is_u64)
                ValueContentsFree(&entry->value);
            free(entry);
            entry = next;
        }
    }
    free(set->buckets);
    free(set);
}
