#ifndef HASHSET_VALUE_JSON_H
#define HASHSET_VALUE_JSON_H

#include "value.h"

/* Serializes a top-level array, list, or map. Returns an allocated string;
 * the caller must free it. Returns NULL for invalid/unsupported values. */
char *HashSetValue_ToJson(const HashSetValue *value);

/* Parse into a fresh HashSetValue_New(HASHSET_VALUE_NULL, NULL) destination. Returns 1 on
 * success. The JSON literal null is stored as HASHSET_VALUE_NULL. Release it with
 * HashSetValue_Free(json). */
int HashSetValue_ParseJson(HashSetValue *json, char *jsonstr);

#endif
