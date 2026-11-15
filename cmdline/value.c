#include "value.h"
#include <stdlib.h>

MoolahValue *MoolahValueAppend(MoolahValue *container, MoolahValue *element)
{
    if (container->kind != MOOLAH_VALUE_LIST)
        return NULL;
    container = container ? container : MoolahValueNew(MOOLAH_VALUE_LIST);
    element = element ?  element : MoolahValueNew(MOOLAH_VALUE_NULL);
    MoolahValueNode *node = malloc(sizeof(MoolahValueNode));

    if (!node)
        return NULL;

    node->value = *element;
    node->next = NULL;

    if (!container->list)
    {
        container->list = node;
        return container;
    }

    MoolahValueNode *current = container->list;

    while (current->next)
        current = current->next;

    current->next = node;
    return container;
}

size_t MoolahValueLen(MoolahValue *container)
{
    if (!container || container->kind != MOOLAH_VALUE_LIST)
        return 0;

    size_t len = 0;

    for (MoolahValueNode *node = container->list; node; node = node->next)
        len++;

    return len;
}

static void MoolahValueClearContents(MoolahValue *value)
{
    if (!value) return;
    if (value->kind == MOOLAH_VALUE_LIST)
    {
        MoolahValueNode *node = value->list;
        while (node)
        {
            MoolahValueNode *next = node->next;
            MoolahValueClearContents(&node->value);
            free(node);
            node = next;
        }
        value->list = NULL;
    }
    else if (value->kind == MOOLAH_VALUE_STRING)
    {
        free(value->s);
        value->s = NULL;
    }
}

void MoolahValueDeepFree(MoolahValue *value)
{
    if (!value) return;
    MoolahValueClearContents(value);
    free(value);
}

void MoolahValueFree(MoolahValue *value)
{
    if (!value) return;
    if (value->free) value->free(value);
    else MoolahValueDeepFree(value);
}
