#ifndef MAP_H
#define MAP_H

#include <stddef.h>
#include "value.h"

typedef struct Entry {
    char *key;
    Value value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    size_t capacity;
} HashMap;

HashMap *MapNew(size_t capacity);
int MapSet(HashMap *map, const char *key, Value value);
Value *MapGet(HashMap *map, const char *key);
void MapFree(HashMap *map);

#endif