#ifndef IPPICKER_VALUE_MAP_H
#define IPPICKER_VALUE_MAP_H

#include <stddef.h>

#include "value.h"

typedef struct IpPickerValueMapEntry {
    char *key;
    IpPickerValue value;
    struct IpPickerValueMapEntry *next;
} IpPickerValueMapEntry;

typedef struct IpPickerValueMap {
    IpPickerValueMapEntry **buckets;
    size_t capacity;
} IpPickerValueMap;


typedef struct {
    const char *start;
    size_t len;
}IpPickerValueStringView;

IpPickerValueMap *IpPickerValue_MapNew(size_t capacity);
int IpPickerValue_MapSet(IpPickerValueMap *map, const char *key, IpPickerValue value);
IpPickerValue *IpPickerValue_MapGet(IpPickerValueMap *map, const char *key);
void IpPickerValue_MapFree(IpPickerValueMap *map);
size_t IpPickerValue_MapLen(IpPickerValueMap *map);

#endif
