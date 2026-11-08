#include "value.h"
#include <stdlib.h>

#define DEEPFREE 0

Value *MoolahValueNew(ValueKind kind)
{
    Value *value = malloc(sizeof(Value));

    if (!value)
        return NULL;

    value->kind = kind;
    value->free = NULL;

    if (kind == VALUE_LIST)
        value->list = NULL;

    return value;
}

Value *MoolahValueNewEx(ValueKind kind, void (*free)(Value *value))
{
    Value *value = MoolahValueNew(kind);

    if (!value)
        return NULL;

    value->free = free ? free : (DEEPFREE ? MoolahValueDeepFree : MoolahValueFree);

    return value;
}

Value *MoolahValueAppend(Value *container, Value *element)
{
    if (container->kind != VALUE_LIST)
        return NULL;
    container = container ? container : MoolahValueNew(VALUE_LIST);
    element = element ?  element : MoolahValueNew(VALUE_NULL);
    ValueNode *node = malloc(sizeof(ValueNode));

    if (!node)
        return NULL;

    node->value = *element;
    node->next = NULL;

    if (!container->list)
    {
        container->list = node;
        return container;
    }

    ValueNode *current = container->list;

    while (current->next)
        current = current->next;

    current->next = node;
    return container;
}

size_t MoolahValueLen(Value *container)
{
    if (!container || container->kind != VALUE_LIST)
        return 0;

    size_t len = 0;

    for (ValueNode *node = container->list; node; node = node->next)
        len++;

    return len;
}

static void ValueClearContents(Value *value)
{
    if (!value) return;
    if (value->kind == VALUE_LIST)
    {
        ValueNode *node = value->list;
        while (node)
        {
            ValueNode *next = node->next;
            ValueClearContents(&node->value);
            free(node);
            node = next;
        }
        value->list = NULL;
    }
    else if (value->kind == VALUE_STRING)
    {
        free(value->s);
        value->s = NULL;
    }
}

void MoolahValueDeepFree(Value *value)
{
    if (!value) return;
    ValueClearContents(value);
    free(value);
}

void MoolahValueFree(Value *value)
{
    if (!value) return;
    if (value->free) value->free(value);
    else MoolahValueDeepFree(value);
}
