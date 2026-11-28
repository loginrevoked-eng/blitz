#include "value.h"
#include "map.h"
#include <stdlib.h>


static int AppendList(IpPickerValue *list, IpPickerValue *element);
static int AppendArray(IpPickerValue *array, IpPickerValue *element);

IpPickerValue *IpPickerValue_New(IpPickerValueKind kind, IpPickerValueFreeFunc free_f)
{
    IpPickerValue *value = calloc(1, sizeof(IpPickerValue));
    if (!value)
        return NULL;

    value->kind = kind;
    value->free = free_f;

    switch (kind) {
        case IPPICKER_VALUE_LIST:
            value->list = NULL;   /* empty list */
            break;

        case IPPICKER_VALUE_ARRAY:
            value->array = calloc(1, sizeof(IpPickerValueArray));
            if (!value->array) {
                free(value);
                return NULL;
            }
            break;

        case IPPICKER_VALUE_MAP:
            value->map = IpPickerValue_MapNew(16);
            if (!value->map) {
                free(value);
                return NULL;
            }
            break;

        default:
            break;
    }

    return value;
}

IpPickerValue *IpPickerValue_Append(IpPickerValue *container, IpPickerValue *element)
{
    if (!container)
        return NULL;

    if (container->kind != IPPICKER_VALUE_LIST && container->kind != IPPICKER_VALUE_ARRAY)
        return NULL;

    IpPickerValue *created = NULL;
    if (!element) {
        element = created = IpPickerValue_New(IPPICKER_VALUE_NULL, NULL);
        if (!element)
            return NULL;
    }

    int ok = container->kind == IPPICKER_VALUE_LIST
        ? AppendList(container, element)
        : AppendArray(container, element);

    if (!ok) {
        free(created);   /* free(NULL) is fine */
        return NULL;
    }
    return container;
}

static int AppendList(IpPickerValue *list, IpPickerValue *element)
{
    IpPickerValueNode *node = calloc(1, sizeof(IpPickerValueNode));
    if (!node)
        return 0;

    node->value = *element;

    if (!list->list) {
        list->list = node;
    } else {
        IpPickerValueNode *current = list->list;
        while (current->next)
            current = current->next;
        current->next = node;
    }

    return 1;
}

static int AppendArray(IpPickerValue *array, IpPickerValue *element)
{
    if (!array->array) {
        array->array = calloc(1, sizeof(IpPickerValueArray));
        if (!array->array)
            return 0;
    }

    IpPickerValueArray *a = array->array;

    IpPickerValue *values = realloc(a->values, (a->len + 1) * sizeof(IpPickerValue));
    if (!values)
        return 0;

    a->values = values;
    a->values[a->len++] = *element;
    return 1;
}

size_t IpPickerValue_Len(IpPickerValue *container)
{
    if (!container)
        return 0;

    switch (container->kind) {
        case IPPICKER_VALUE_LIST: {
            size_t len = 0;

            for (IpPickerValueNode *node = container->list; node; node = node->next)
                len++;

            return len;
        }

        case IPPICKER_VALUE_ARRAY:
            return container->array ? container->array->len : 0;

        case IPPICKER_VALUE_MAP:
            return container->map ? IpPickerValue_MapLen(container->map) : 0;

        default:
            return 0;
    }
}

void IpPickerValue_Set(IpPickerValue *map_v, const char *key, IpPickerValue *val)
{
    if (!map_v || map_v->kind != IPPICKER_VALUE_MAP || !map_v->map || !key || !val)
        return;

    IpPickerValue_MapSet(map_v->map, key, *val);
}

IpPickerValue *IpPickerValue_Get(IpPickerValue *map_v, const char *key)
{
    if (!map_v || map_v->kind != IPPICKER_VALUE_MAP || !map_v->map || !key)
        return NULL;

    return IpPickerValue_MapGet(map_v->map, key);
}





IpPickerValue *IpPickerValue_NewInt(int value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_INT, NULL);
    if (v) v->_int = value;
    return v;
}

IpPickerValue *IpPickerValue_NewUnsignedInt(uint32_t value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_UNSIGNED_INT, NULL);
    if (v) v->_unsigned_int = value;
    return v;
}

IpPickerValue *IpPickerValue_NewLong(long value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_LONG, NULL);
    if (v) v->_long = value;
    return v;
}

IpPickerValue *IpPickerValue_NewFloat(float value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_FLOAT, NULL);
    if (v) v->_float = value;
    return v;
}

IpPickerValue *IpPickerValue_NewDouble(double value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_DOUBLE, NULL);
    if (v) v->_double = value;
    return v;
}

IpPickerValue *IpPickerValue_NewBool(IpPickerValueBool value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_BOOL, NULL);
    if (v) v->_bool = value;
    return v;
}

IpPickerValue *IpPickerValue_NewString(char *value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_STRING, NULL);
    if (v) v->str = value;
    return v;
}

IpPickerValue *IpPickerValue_NewPointer(void *value)
{
    IpPickerValue *v = IpPickerValue_New(IPPICKER_VALUE_POINTER, NULL);
    if (v) v->ptr = value;
    return v;
}

IpPickerValue *IpPickerValue_NewNull(void)
{
    return IpPickerValue_New(IPPICKER_VALUE_NULL, NULL);
}


int IpPickerValue_IsMap(IpPickerValue *value)
{
    return value->kind == IPPICKER_VALUE_MAP && value->map;
}


int IpPickerValue_IsArray(IpPickerValue *arr)
{
    return arr->kind == IPPICKER_VALUE_ARRAY && arr->array;
}


IpPickerValue *IpPickerValue_At(IpPickerValue *container, size_t index)
{
    if (!container || container->kind != IPPICKER_VALUE_ARRAY || !container->array)
        return NULL;

    IpPickerValueArray *a = container->array;
    if (index >= a->len)
        return NULL;

    return &a->values[index];
}

void IpPickerValue_Clear(IpPickerValue *value)
{
    if (!value) return;

    switch (value->kind) {
        case IPPICKER_VALUE_ARRAY:
            if (value->array) {
                for (size_t i = 0; i < value->array->len; i++)
                    IpPickerValue_Clear(&value->array->values[i]);
                free(value->array->values);
                free(value->array);
                value->array = NULL;
            }
            break;

        case IPPICKER_VALUE_LIST: {
            IpPickerValueNode *node = value->list;
            while (node) {
                IpPickerValueNode *next = node->next;
                IpPickerValue_Clear(&node->value);
                free(node);
                node = next;
            }
            value->list = NULL;
            break;
        }

        case IPPICKER_VALUE_MAP:
            if (value->map) {
                IpPickerValue_MapFree(value->map);
                value->map = NULL;
            }
            break;

        case IPPICKER_VALUE_STRING:
            free(value->str);
            value->str = NULL;
            break;

        default:
            break;
    }
}

void IpPickerValue_Free(IpPickerValue *value)
{
    if (!value) return;
    if (value->free) {
        value->free(value);
        return;
    }
    IpPickerValue_Clear(value);
    free(value);
}

void IpPickerValue_DeepFree(IpPickerValue *value)
{
    IpPickerValue_Free(value);
}
