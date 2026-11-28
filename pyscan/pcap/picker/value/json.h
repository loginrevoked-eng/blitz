#ifndef IPPICKER_VALUE_JSON_H
#define IPPICKER_VALUE_JSON_H

#include "value.h"

/* Serializes a top-level array, list, or map. Returns an allocated string;
 * the caller must free it. Returns NULL for invalid/unsupported values. */
char *IpPickerValue_ToJson(const IpPickerValue *value);

/* Parse into a fresh IpPickerValue_New(IPPICKER_VALUE_NULL, NULL) destination. Returns 1 on
 * success. The JSON literal null is stored as IPPICKER_VALUE_NULL. Release it with
 * IpPickerValue_Free(json). */
int IpPickerValue_ParseJson(IpPickerValue *json, const char *jsonstr);

#endif
