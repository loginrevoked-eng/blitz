#include "crud.h"

HashSetValueBool HashSetValue_AsNull(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_NULL
        ? HASHSET_VALUE_TRUE : HASHSET_VALUE_FALSE;
}

HashSetValueBool HashSetValue_AsBool(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_BOOL
        ? value->_bool : HASHSET_VALUE_FALSE;
}

int HashSetValue_AsInt(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_INT ? value->_int : 0;
}

uint32_t HashSetValue_AsUnsignedInt(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_UNSIGNED_INT
        ? value->_unsigned_int : 0;
}

uint64_t HashSetValue_AsUInt64(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_UINT64 ? value->_uint64 : 0;
}

long HashSetValue_AsLong(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_LONG ? value->_long : 0;
}

float HashSetValue_AsFloat(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_FLOAT ? value->_float : 0.0f;
}

double HashSetValue_AsDouble(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_DOUBLE ? value->_double : 0.0;
}

const char *HashSetValue_AsString(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_STRING ? value->str : NULL;
}

void *HashSetValue_AsPointer(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_POINTER ? value->ptr : NULL;
}

HashSetValueNode *HashSetValue_AsList(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_LIST ? value->list : NULL;
}

HashSetValueArray *HashSetValue_AsArray(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_ARRAY ? value->array : NULL;
}

HashSetValueMap *HashSetValue_AsMap(const HashSetValue *value)
{
    return value && value->kind == HASHSET_VALUE_MAP ? value->map : NULL;
}
