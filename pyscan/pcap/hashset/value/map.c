#include "map.h"
#include <stdlib.h>
#include <string.h>




HashSetValueMapStringView HashSetValueMap_StringViewNew(const char *start, size_t len)
{
    HashSetValueMapStringView view;
    view.start = start;
    view.len = len;
    return view;
}

static unsigned long hash(const char *key)
{
    unsigned long h = 5381;
    int c;

    while ((c = *key++))
        h = ((h << 5) + h) + c;

    return h;
}


static unsigned long hashV(HashSetValueMapStringView key)
{
    unsigned long h = 5381;
    for (size_t i = 0; i < key.len; i++)
        h = ((h << 5) + h) + (unsigned char)key.start[i];
    return h;
}

static int key_equalV(const char *key, HashSetValueMapStringView view)
{
    return strlen(key) == view.len && memcmp(key, view.start, view.len) == 0;
}

HashSetValue *HashSetValueMap_GetV(HashSetValueMap *map, HashSetValueMapStringView key)
{
    if (!map || !key.start || !map->capacity)
        return NULL;

    size_t index = hashV(key) % map->capacity;

    for (HashSetValueMapEntry *entry = map->buckets[index]; entry; entry = entry->next)
        if (key_equalV(entry->key, key))
            return &entry->value;

    return NULL;
}


HashSetValueMap *HashSetValueMap_New(size_t capacity)
{
    HashSetValueMap *map = malloc(sizeof(HashSetValueMap));

    if (!capacity)
        return NULL;

    if (!map)
        return NULL;

    map->buckets = calloc(capacity, sizeof(HashSetValueMapEntry *));
    map->capacity = capacity;

    if (!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}

size_t HashSetValueMap_Len(HashSetValueMap *map)
{
    if (!map)
        return 0;

    size_t len = 0;

    for (size_t i = 0; i < map->capacity; i++)
        for (HashSetValueMapEntry *entry = map->buckets[i]; entry; entry = entry->next)
            len++;

    return len;
}

int HashSetValueMap_Set(HashSetValueMap *map, const char *key, HashSetValue value)
{
    if (!map || !key || !map->capacity) return 0;
    size_t index = hash(key) % map->capacity;
    HashSetValueMapEntry *entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return 1;
        }

        entry = entry->next;
    }

    entry = malloc(sizeof(HashSetValueMapEntry));

    if (!entry)
        return 0;

    entry->key = strdup(key);
    if (!entry->key) { free(entry); return 0; }
    entry->value = value;
    entry->next = map->buckets[index];

    map->buckets[index] = entry;
    return 1;
}

HashSetValue *HashSetValueMap_Get(HashSetValueMap *map, const char *key)
{
    size_t index = hash(key) % map->capacity;
    HashSetValueMapEntry *entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0)
            return &entry->value;

        entry = entry->next;
    }

    return NULL;
}

void HashSetValueMap_Free(HashSetValueMap *map)
{
    if (!map)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        HashSetValueMapEntry *entry = map->buckets[i];

        while (entry) {
            HashSetValueMapEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}