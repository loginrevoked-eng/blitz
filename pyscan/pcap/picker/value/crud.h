#ifndef IPPICKER_VALUE_CRUD_H
#define IPPICKER_VALUE_CRUD_H

#include "value.h"

/* Typed accessors return the stored field when kinds match, or a zero value
 * otherwise. IpPickerValue_AsNull reports whether the value has IPPICKER_VALUE_NULL kind. */
IpPickerValueBool IpPickerValue_AsNull(const IpPickerValue *value);
IpPickerValueBool IpPickerValue_AsBool(const IpPickerValue *value);
int IpPickerValue_AsInt(const IpPickerValue *value);
uint32_t IpPickerValue_AsUnsignedInt(const IpPickerValue *value);
long IpPickerValue_AsLong(const IpPickerValue *value);
float IpPickerValue_AsFloat(const IpPickerValue *value);
double IpPickerValue_AsDouble(const IpPickerValue *value);
const char *IpPickerValue_AsString(const IpPickerValue *value);
void *IpPickerValue_AsPointer(const IpPickerValue *value);
IpPickerValueNode *IpPickerValue_AsList(const IpPickerValue *value);
IpPickerValueArray *IpPickerValue_AsArray(const IpPickerValue *value);
IpPickerValueMap *IpPickerValue_AsMap(const IpPickerValue *value);

#endif
