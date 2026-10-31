#include "map.h"
#include <stdlib.h>
#include <string.h>

static unsigned long hash(const char *key)
{
    unsigned long h = 5381;
    int c;

    while ((c = *key++))
        h = ((h << 5) + h) + c;

    return h;
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