#ifndef VALUE_JSON_H
#define VALUE_JSON_H

#include "value.h"

/* Serializes a top-level array, list, or map. Returns an allocated string;
 * the caller must free it. Returns NULL for invalid/unsupported values. */
char *ValueToJson(const Value *value);

/* Parse into a fresh ValueNew(VALUE_NULL, NULL) destination. Returns 1 on
 * success. The JSON literal null is stored as VALUE_NULL. Release it with
 * ValueFree(json). */
int ParseJson(Value *json, char *jsonstr);

#endif
