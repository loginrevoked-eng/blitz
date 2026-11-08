#ifndef VALUE_CRUD_H
#define VALUE_CRUD_H

#include "value.h"

/* Typed accessors return the stored field when kinds match, or a zero value
 * otherwise. ValueAsNull reports whether the value has VALUE_NULL kind. */
Bool ValueAsNull(const Value *value);
Bool ValueAsBool(const Value *value);
int ValueAsInt(const Value *value);
uint32_t ValueAsUnsignedInt(const Value *value);
long ValueAsLong(const Value *value);
float ValueAsFloat(const Value *value);
double ValueAsDouble(const Value *value);
const char *ValueAsString(const Value *value);
void *ValueAsPointer(const Value *value);
ValueNode *ValueAsList(const Value *value);
ValueArray *ValueAsArray(const Value *value);
HashMap *ValueAsMap(const Value *value);

#endif
