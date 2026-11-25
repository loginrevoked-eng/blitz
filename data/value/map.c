#include "map.h"
#include <stdlib.h>
#include <string.h>




StringView StringViewNew(const char *start, size_t len)
{
    StringView view;
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


static unsigned long hashV(StringView key)
{
    unsigned long h = 5381;
    for (size_t i = 0; i < key.len; i++)
        h = ((h << 5) + h) + (unsigned char)key.start[i];
    return h;
}

static int key_equalV(const char *key, StringView view)
{
    return strlen(key) == view.len && memcmp(key, view.start, view.len) == 0;
}

Value *MapGetV(HashMap *map, StringView key)
{
    if (!map || !key.start || !map->capacity)
        return NULL;

    size_t index = hashV(key) % map->capacity;

    for (Entry *entry = map->buckets[index]; entry; entry = entry->next)
        if (key_equalV(entry->key, key))
            return &entry->value;

    return NULL;
}


HashMap *MapNew(size_t capacity)
{
    HashMap *map = malloc(sizeof(HashMap));

    if (!capacity)
        return NULL;

    if (!map)
        return NULL;

    map->buckets = calloc(capacity, sizeof(Entry *));
    map->capacity = capacity;

    if (!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}

size_t MapLen(HashMap *map)
{
    if (!map)
        return 0;

    size_t len = 0;

    for (size_t i = 0; i < map->capacity; i++)
        for (Entry *entry = map->buckets[i]; entry; entry = entry->next)
            len++;

    return len;
}

int MapSet(HashMap *map, const char *key, Value value)
{
    if (!map || !key || !map->capacity) return 0;
    size_t index = hash(key) % map->capacity;
    Entry *entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return 1;
        }

        entry = entry->next;
    }

    entry = malloc(sizeof(Entry));

    if (!entry)
        return 0;

    entry->key = strdup(key);
    if (!entry->key) { free(entry); return 0; }
    entry->value = value;
    entry->next = map->buckets[index];

    map->buckets[index] = entry;
    return 1;
}

Value *MapGet(HashMap *map, const char *key)
{
    size_t index = hash(key) % map->capacity;
    Entry *entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0)
            return &entry->value;

        entry = entry->next;
    }

    return NULL;
}

void MapFree(HashMap *map)
{
    if (!map)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        Entry *entry = map->buckets[i];

        while (entry) {
            Entry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}