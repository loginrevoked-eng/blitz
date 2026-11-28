#ifndef HASHSET_VALUE_CRUD_H
#define HASHSET_VALUE_CRUD_H

#include "value.h"

/* Typed accessors return the stored field when kinds match, or a zero value
 * otherwise. HashSetValue_AsNull reports whether the value has HASHSET_VALUE_NULL kind. */
HashSetValueBool HashSetValue_AsNull(const HashSetValue *value);
HashSetValueBool HashSetValue_AsBool(const HashSetValue *value);
int HashSetValue_AsInt(const HashSetValue *value);
uint32_t HashSetValue_AsUnsignedInt(const HashSetValue *value);
uint64_t HashSetValue_AsUInt64(const HashSetValue *value);
long HashSetValue_AsLong(const HashSetValue *value);
float HashSetValue_AsFloat(const HashSetValue *value);
double HashSetValue_AsDouble(const HashSetValue *value);
const char *HashSetValue_AsString(const HashSetValue *value);
void *HashSetValue_AsPointer(const HashSetValue *value);
HashSetValueNode *HashSetValue_AsList(const HashSetValue *value);
HashSetValueArray *HashSetValue_AsArray(const HashSetValue *value);
HashSetValueMap *HashSetValue_AsMap(const HashSetValue *value);

#endif
