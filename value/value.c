#include "value.h"
#include "map.h"
#include <stdlib.h>


static int AppendList(Value *list, Value *element);
static int AppendArray(Value *array, Value *element);

Value *ValueNew(ValueKind kind, ValueFreeFunc free_f)
{
    Value *value = calloc(1, sizeof(Value));
    if (!value)
        return NULL;

    value->kind = kind;
    value->free = free_f;

    switch (kind) {
        case VALUE_LIST:
            value->list = NULL;   /* empty list */
            break;

        case VALUE_ARRAY:
            value->array = calloc(1, sizeof(ValueArray));
            if (!value->array) {
                free(value);
                return NULL;
            }
            break;

        case VALUE_MAP:
            value->map = MapNew(16);
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

Value *ValueAppend(Value *container, Value *element)
{
    if (!container)
        return NULL;

    if (container->kind != VALUE_LIST && container->kind != VALUE_ARRAY)
        return NULL;

    Value *created = NULL;
    if (!element) {
        element = created = ValueNew(VALUE_NULL, NULL);
        if (!element)
            return NULL;
    }

    int ok = container->kind == VALUE_LIST
        ? AppendList(container, element)
        : AppendArray(container, element);

    if (!ok) {
        free(created);   /* free(NULL) is fine */
        return NULL;
    }
    return container;
}

static int AppendList(Value *list, Value *element)
{
    ValueNode *node = calloc(1, sizeof(ValueNode));
    if (!node)
        return 0;

    node->value = *element;

    if (!list->list) {
        list->list = node;
    } else {
        ValueNode *current = list->list;
        while (current->next)
            current = current->next;
        current->next = node;
    }

    return 1;
}

static int AppendArray(Value *array, Value *element)
{
    if (!array->array) {
        array->array = calloc(1, sizeof(ValueArray));
        if (!array->array)
            return 0;
    }

    ValueArray *a = array->array;

    Value *values = realloc(a->values, (a->len + 1) * sizeof(Value));
    if (!values)
        return 0;

    a->values = values;
    a->values[a->len++] = *element;
    return 1;
}

size_t ValueLen(Value *container)
{
    if (!container)
        return 0;

    switch (container->kind) {
        case VALUE_LIST: {
            size_t len = 0;

            for (ValueNode *node = container->list; node; node = node->next)
                len++;

            return len;
        }

        case VALUE_ARRAY:
            return container->array ? container->array->len : 0;

        case VALUE_MAP:
            return container->map ? MapLen(container->map) : 0;

        default:
            return 0;
    }
}

void ValueSet(Value *map_v, const char *key, Value *val)
{
    if (!map_v || map_v->kind != VALUE_MAP || !map_v->map || !key || !val)
        return;

    MapSet(map_v->map, key, *val);
}

Value *ValueGet(Value *map_v, const char *key)
{
    if (!map_v || map_v->kind != VALUE_MAP || !map_v->map || !key)
        return NULL;

    return MapGet(map_v->map, key);
}




void ValueFree(Value *value)
{
    if (!value)return;
    if (value->free) value->free(value); 
    else free(value);
}




Value *ValueNewInt(int value)
{
    Value *v = ValueNew(VALUE_INT, NULL);
    if (v) v->_int = value;
    return v;
}

Value *ValueNewLong(long value)
{
    Value *v = ValueNew(VALUE_LONG, NULL);
    if (v) v->_long = value;
    return v;
}

Value *ValueNewFloat(float value)
{
    Value *v = ValueNew(VALUE_FLOAT, NULL);
    if (v) v->_float = value;
    return v;
}

Value *ValueNewDouble(double value)
{
    Value *v = ValueNew(VALUE_DOUBLE, NULL);
    if (v) v->_double = value;
    return v;
}

Value *ValueNewBool(Bool value)
{
    Value *v = ValueNew(VALUE_BOOL, NULL);
    if (v) v->_bool = value;
    return v;
}

Value *ValueNewString(char *value)
{
    Value *v = ValueNew(VALUE_STRING, NULL);
    if (v) v->str = value;
    return v;
}

Value *ValueNewPointer(void *value)
{
    Value *v = ValueNew(VALUE_POINTER, NULL);
    if (v) v->ptr = value;
    return v;
}

Value *ValueNewNull(void)
{
    return ValueNew(VALUE_NULL, NULL);
}