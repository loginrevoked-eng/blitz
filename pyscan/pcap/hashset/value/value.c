#include "value.h"
#include "map.h"
#include <stdlib.h>


static int AppendList(HashSetValue *list, HashSetValue *element);
static int AppendArray(HashSetValue *array, HashSetValue *element);

HashSetValue *HashSetValue_New(HashSetValueKind kind, HashSetValueFreeFunc free_f)
{
    HashSetValue *value = calloc(1, sizeof(HashSetValue));
    if (!value)
        return NULL;

    value->kind = kind;
    value->free = free_f;

    switch (kind) {
        case HASHSET_VALUE_LIST:
            value->list = NULL;   /* empty list */
            break;

        case HASHSET_VALUE_ARRAY:
            value->array = calloc(1, sizeof(HashSetValueArray));
            if (!value->array) {
                free(value);
                return NULL;
            }
            break;

        case HASHSET_VALUE_MAP:
            value->map = HashSetValueMap_New(16);
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

HashSetValue *HashSetValue_Append(HashSetValue *container, HashSetValue *element)
{
    if (!container)
        return NULL;

    if (container->kind != HASHSET_VALUE_LIST && container->kind != HASHSET_VALUE_ARRAY)
        return NULL;

    HashSetValue *created = NULL;
    if (!element) {
        element = created = HashSetValue_New(HASHSET_VALUE_NULL, NULL);
        if (!element)
            return NULL;
    }

    int ok = container->kind == HASHSET_VALUE_LIST
        ? AppendList(container, element)
        : AppendArray(container, element);

    if (!ok) {
        free(created);   /* free(NULL) is fine */
        return NULL;
    }
    return container;
}

static int AppendList(HashSetValue *list, HashSetValue *element)
{
    HashSetValueNode *node = calloc(1, sizeof(HashSetValueNode));
    if (!node)
        return 0;

    node->value = *element;

    if (!list->list) {
        list->list = node;
    } else {
        HashSetValueNode *current = list->list;
        while (current->next)
            current = current->next;
        current->next = node;
    }

    return 1;
}

static int AppendArray(HashSetValue *array, HashSetValue *element)
{
    if (!array->array) {
        array->array = calloc(1, sizeof(HashSetValueArray));
        if (!array->array)
            return 0;
    }

    HashSetValueArray *a = array->array;

    HashSetValue *values = realloc(a->values, (a->len + 1) * sizeof(HashSetValue));
    if (!values)
        return 0;

    a->values = values;
    a->values[a->len++] = *element;
    return 1;
}

size_t HashSetValue_Len(HashSetValue *container)
{
    if (!container)
        return 0;

    switch (container->kind) {
        case HASHSET_VALUE_LIST: {
            size_t len = 0;

            for (HashSetValueNode *node = container->list; node; node = node->next)
                len++;

            return len;
        }

        case HASHSET_VALUE_ARRAY:
            return container->array ? container->array->len : 0;

        case HASHSET_VALUE_MAP:
            return container->map ? HashSetValueMap_Len(container->map) : 0;

        default:
            return 0;
    }
}

void HashSetValue_Set(HashSetValue *map_v, const char *key, HashSetValue *val)
{
    if (!map_v || map_v->kind != HASHSET_VALUE_MAP || !map_v->map || !key || !val)
        return;

    HashSetValueMap_Set(map_v->map, key, *val);
}

HashSetValue *HashSetValue_Get(HashSetValue *map_v, const char *key)
{
    if (!map_v || map_v->kind != HASHSET_VALUE_MAP || !map_v->map || !key)
        return NULL;

    return HashSetValueMap_Get(map_v->map, key);
}





HashSetValue *HashSetValue_NewInt(int value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_INT, NULL);
    if (v) v->_int = value;
    return v;
}

HashSetValue *HashSetValue_NewUnsignedInt(uint32_t value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_UNSIGNED_INT, NULL);
    if (v) v->_unsigned_int = value;
    return v;
}

HashSetValue *HashSetValue_NewUInt64(uint64_t value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_UINT64, NULL);
    if (v) v->_uint64 = value;
    return v;
}

HashSetValue *HashSetValue_NewLong(long value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_LONG, NULL);
    if (v) v->_long = value;
    return v;
}

HashSetValue *HashSetValue_NewFloat(float value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_FLOAT, NULL);
    if (v) v->_float = value;
    return v;
}

HashSetValue *HashSetValue_NewDouble(double value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_DOUBLE, NULL);
    if (v) v->_double = value;
    return v;
}

HashSetValue *HashSetValue_NewBool(HashSetValueBool value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_BOOL, NULL);
    if (v) v->_bool = value;
    return v;
}

HashSetValue *HashSetValue_NewString(char *value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_STRING, NULL);
    if (v) v->str = value;
    return v;
}

HashSetValue *HashSetValue_NewPointer(void *value)
{
    HashSetValue *v = HashSetValue_New(HASHSET_VALUE_POINTER, NULL);
    if (v) v->ptr = value;
    return v;
}

HashSetValue *HashSetValue_NewNull(void)
{
    return HashSetValue_New(HASHSET_VALUE_NULL, NULL);
}


int HashSetValue_IsMap(HashSetValue *value)
{
    return value->kind == HASHSET_VALUE_MAP && value->map;
}


int HashSetValue_IsArray(HashSetValue *arr)
{
    return arr->kind == HASHSET_VALUE_ARRAY && arr->array;
}


HashSetValue *HashSetValue_At(HashSetValue *container, size_t index)
{
    if (!container || container->kind != HASHSET_VALUE_ARRAY || !container->array)
        return NULL;

    HashSetValueArray *a = container->array;
    if (index >= a->len)
        return NULL;

    return &a->values[index];
}

void HashSetValue_Clear(HashSetValue *value)
{
    if (!value) return;

    switch (value->kind) {
        case HASHSET_VALUE_ARRAY:
            if (value->array) {
                for (size_t i = 0; i < value->array->len; i++)
                    HashSetValue_Clear(&value->array->values[i]);
                free(value->array->values);
                free(value->array);
                value->array = NULL;
            }
            break;

        case HASHSET_VALUE_LIST: {
            HashSetValueNode *node = value->list;
            while (node) {
                HashSetValueNode *next = node->next;
                HashSetValue_Clear(&node->value);
                free(node);
                node = next;
            }
            value->list = NULL;
            break;
        }

        case HASHSET_VALUE_MAP:
            if (value->map) {
                HashSetValueMap_Free(value->map);
                value->map = NULL;
            }
            break;

        case HASHSET_VALUE_STRING:
            free(value->str);
            value->str = NULL;
            break;

        default:
            break;
    }
}

void HashSetValue_DeepFree(HashSetValue *value)
{
    if (!value)
        return;
    HashSetValue_Clear(value);
    free(value);
}

void HashSetValue_Free(HashSetValue *value)
{
    if (!value) return;
    if (value->free) {
        value->free(value);
        return;
    }
    HashSetValue_Clear(value);
    free(value);
}
