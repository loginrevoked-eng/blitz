#include "util.h"
#include "cleanup.h"

#include <stdlib.h>
#include <string.h>

HandleList *HandleListNew(void)
{
    return calloc(1, sizeof(HandleList));
}

int HandleListPush(HandleList *list, HANDLE handle)
{
    if (list == NULL)
        return 0;

    if (list->len == list->cap) {
        uint32_t new_cap = list->cap == 0 ? 8 : list->cap;
        if (list->cap != 0) {
            if (list->cap > UINT32_MAX / 2)
                new_cap = UINT32_MAX;
            else
                new_cap = list->cap * 2;
        }
        if (new_cap <= list->len ||
            (size_t)new_cap > SIZE_MAX / sizeof(*list->handles))
            return 0;

        HANDLE *handles = realloc(list->handles,
            (size_t)new_cap * sizeof(*list->handles));
        if (handles == NULL)
            return 0;
        list->handles = handles;
        list->cap = new_cap;
    }

    list->handles[list->len++] = handle;
    return 1;
}

HANDLE HandleListPop(HandleList *list)
{
    if (list == NULL || list->len == 0)
        return NULL;

    HANDLE handle = list->handles[--list->len];
    list->handles[list->len] = NULL;
    return handle;
}

HANDLE HandleListAt(const HandleList *list, uint32_t index)
{
    if (list == NULL || index >= list->len)
        return NULL;
    return list->handles[index];
}

PortList *PortListCopy(PortList *ports)
{
    if (ports == NULL)
        return NULL;

    PortList *copy = calloc(1, sizeof(*copy));
    if (copy == NULL)
        return NULL;

    if (ports->len != 0) {
        if (ports->items == NULL) {
            ProbeCleanupPortList(copy);
            return NULL;
        }

        copy->items = malloc(ports->len * sizeof(*copy->items));
        if (copy->items == NULL) {
            ProbeCleanupPortList(copy);
            return NULL;
        }
        memcpy(copy->items, ports->items,
            ports->len * sizeof(*copy->items));
    }
    copy->len = ports->len;
    return copy;
}
