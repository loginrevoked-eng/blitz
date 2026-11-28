#include "map.h"
#include <stdlib.h>
#include <string.h>




IpPickerValueStringView IpPickerValue_StringViewNew(const char *start, size_t len)
{
    IpPickerValueStringView view;
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


static unsigned long hashV(IpPickerValueStringView key)
{
    unsigned long h = 5381;
    for (size_t i = 0; i < key.len; i++)
        h = ((h << 5) + h) + (unsigned char)key.start[i];
    return h;
}

static int key_equalV(const char *key, IpPickerValueStringView view)
{
    return strlen(key) == view.len && memcmp(key, view.start, view.len) == 0;
}

IpPickerValue *IpPickerValue_MapGetView(IpPickerValueMap *map, IpPickerValueStringView key)
{
    if (!map || !key.start || !map->capacity)
        return NULL;

    size_t index = hashV(key) % map->capacity;

    for (IpPickerValueMapEntry *entry = map->buckets[index]; entry; entry = entry->next)
        if (key_equalV(entry->key, key))
            return &entry->value;

    return NULL;
}


IpPickerValueMap *IpPickerValue_MapNew(size_t capacity)
{
    IpPickerValueMap *map = malloc(sizeof(IpPickerValueMap));

    if (!capacity)
        return NULL;

    if (!map)
        return NULL;

    map->buckets = calloc(capacity, sizeof(IpPickerValueMapEntry *));
    map->capacity = capacity;

    if (!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}

size_t IpPickerValue_MapLen(IpPickerValueMap *map)
{
    if (!map)
        return 0;

    size_t len = 0;

    for (size_t i = 0; i < map->capacity; i++)
        for (IpPickerValueMapEntry *entry = map->buckets[i]; entry; entry = entry->next)
            len++;

    return len;
}

int IpPickerValue_MapSet(IpPickerValueMap *map, const char *key, IpPickerValue value)
{
    if (!map || !key || !map->capacity) return 0;
    size_t index = hash(key) % map->capacity;
    IpPickerValueMapEntry *entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return 1;
        }

        entry = entry->next;
    }

    entry = malloc(sizeof(IpPickerValueMapEntry));

    if (!entry)
        return 0;

    entry->key = strdup(key);
    if (!entry->key) { free(entry); return 0; }
    entry->value = value;
    entry->next = map->buckets[index];

    map->buckets[index] = entry;
    return 1;
}

IpPickerValue *IpPickerValue_MapGet(IpPickerValueMap *map, const char *key)
{
    size_t index = hash(key) % map->capacity;
    IpPickerValueMapEntry *entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0)
            return &entry->value;

        entry = entry->next;
    }

    return NULL;
}

void IpPickerValue_MapFree(IpPickerValueMap *map)
{
    if (!map)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        IpPickerValueMapEntry *entry = map->buckets[i];

        while (entry) {
            IpPickerValueMapEntry *next = entry->next;
            free(entry->key);
            IpPickerValue_Clear(&entry->value);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}
