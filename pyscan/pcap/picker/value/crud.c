#include "crud.h"

IpPickerValueBool IpPickerValue_AsNull(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_NULL ? IPPICKER_VALUE_TRUE : IPPICKER_VALUE_FALSE;
}

IpPickerValueBool IpPickerValue_AsBool(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_BOOL ? value->_bool : IPPICKER_VALUE_FALSE;
}

int IpPickerValue_AsInt(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_INT ? value->_int : 0;
}

uint32_t IpPickerValue_AsUnsignedInt(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_UNSIGNED_INT ? value->_unsigned_int : 0;
}

long IpPickerValue_AsLong(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_LONG ? value->_long : 0;
}

float IpPickerValue_AsFloat(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_FLOAT ? value->_float : 0.0f;
}

double IpPickerValue_AsDouble(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_DOUBLE ? value->_double : 0.0;
}

const char *IpPickerValue_AsString(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_STRING ? value->str : NULL;
}

void *IpPickerValue_AsPointer(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_POINTER ? value->ptr : NULL;
}

IpPickerValueNode *IpPickerValue_AsList(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_LIST ? value->list : NULL;
}

IpPickerValueArray *IpPickerValue_AsArray(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_ARRAY ? value->array : NULL;
}

IpPickerValueMap *IpPickerValue_AsMap(const IpPickerValue *value)
{
    return value && value->kind == IPPICKER_VALUE_MAP ? value->map : NULL;
}

