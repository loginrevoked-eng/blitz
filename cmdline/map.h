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

HashMap *MoolahMapNew(size_t capacity);
int MoolahMapSet(HashMap *map, const char *key, Value value);
Value *MoolahMapGet(HashMap *map, const char *key);
void MoolahMapFree(HashMap *map);

#endif
