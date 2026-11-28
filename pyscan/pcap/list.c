#include "list.h"

#include <stdlib.h>

/* --- scan_target_t list --- */

int list_init(scan_target_list_t **list, uint32_t initial_cap) {
    if (!list) {
        return 1;
    }

    if (initial_cap == 0) {
        initial_cap = 16;
    }

    scan_target_list_t *l = (scan_target_list_t *)malloc(sizeof(scan_target_list_t));
    if (!l) {
        *list = NULL;
        return 1;
    }

    l->targets = (scan_target_t *)malloc(initial_cap * sizeof(scan_target_t));
    if (!l->targets) {
        free(l);
        *list = NULL;
        return 1;
    }

    l->len = 0;
    l->cap = initial_cap;
    *list = l;
    return 0;
}

int list_inits(scan_target_list_t *list, uint32_t initial_cap) {
    if (!list) {
        return 1;
    }

    if (initial_cap == 0) {
        initial_cap = 16;
    }

    list->targets = (scan_target_t *)malloc(initial_cap * sizeof(scan_target_t));
    if (!list->targets) {
        list->len = 0;
        list->cap = 0;
        return 1;
    }

    list->len = 0;
    list->cap = initial_cap;
    return 0;
}

int list_ensure_cap(scan_target_list_t **list, size_t more) {
    if (!list || !*list) {
        return 1;
    }

    scan_target_list_t *l = *list;
    if ((size_t)l->len + more <= (size_t)l->cap) {
        return 0;
    }

    size_t new_cap = l->cap ? (size_t)l->cap * 2 : 16;
    while (new_cap < (size_t)l->len + more) {
        new_cap *= 2;
    }

    scan_target_t *new_buf = (scan_target_t *)realloc(l->targets, new_cap * sizeof(scan_target_t));
    if (!new_buf) {
        return 1;
    }

    l->targets = new_buf;
    l->cap = (uint32_t)new_cap;
    return 0;
}

int list_push(scan_target_list_t **list, scan_target_t out) {
    int r = list_ensure_cap(list, 1);
    if (r) return r;

    scan_target_list_t *l = *list;
    l->targets[l->len++] = out;
    return 0;
}

int list_pop(scan_target_list_t *list, scan_target_t *out) {
    if (!list || list->len == 0 || !out) {
        return 1;
    }

    *out = list->targets[--list->len];
    return 0;
}

void list_free(scan_target_list_t **list) {
    if (!list || !*list) {
        return;
    }

    free((*list)->targets);
    free(*list);
    *list = NULL;
}

void list_frees(scan_target_list_t *list) {
    if (!list) {
        return;
    }

    free(list->targets);
    list->targets = NULL;
    list->len = 0;
    list->cap = 0;
}

/* --- uint16_t list --- */

int u16_list_init(u16_list_t **list, uint32_t initial_cap) {
    if (!list) {
        return 1;
    }

    if (initial_cap == 0) {
        initial_cap = 16;
    }

    u16_list_t *l = (u16_list_t *)malloc(sizeof(u16_list_t));
    if (!l) {
        *list = NULL;
        return 1;
    }

    l->items = (uint16_t *)malloc(initial_cap * sizeof(uint16_t));
    if (!l->items) {
        free(l);
        *list = NULL;
        return 1;
    }

    l->len = 0;
    l->cap = initial_cap;
    *list = l;
    return 0;
}

int u16_list_inits(u16_list_t *list, uint32_t initial_cap) {
    if (!list) {
        return 1;
    }

    if (initial_cap == 0) {
        initial_cap = 16;
    }

    list->items = (uint16_t *)malloc(initial_cap * sizeof(uint16_t));
    if (!list->items) {
        list->len = 0;
        list->cap = 0;
        return 1;
    }

    list->len = 0;
    list->cap = initial_cap;
    return 0;
}

int u16_list_ensure_cap(u16_list_t **list, size_t more) {
    if (!list || !*list) {
        return 1;
    }

    u16_list_t *l = *list;
    if ((size_t)l->len + more <= (size_t)l->cap) {
        return 0;
    }

    size_t new_cap = l->cap ? (size_t)l->cap * 2 : 16;
    while (new_cap < (size_t)l->len + more) {
        new_cap *= 2;
    }

    uint16_t *new_buf = (uint16_t *)realloc(l->items, new_cap * sizeof(uint16_t));
    if (!new_buf) {
        return 1;
    }

    l->items = new_buf;
    l->cap = (uint32_t)new_cap;
    return 0;
}

int u16_list_push(u16_list_t **list, uint16_t val) {
    int r = u16_list_ensure_cap(list, 1);
    if (r) return r;

    u16_list_t *l = *list;
    l->items[l->len++] = val;
    return 0;
}

int u16_list_pop(u16_list_t *list, uint16_t *out) {
    if (!list || list->len == 0 || !out) {
        return 1;
    }

    *out = list->items[--list->len];
    return 0;
}

void u16_list_free(u16_list_t **list) {
    if (!list || !*list) {
        return;
    }

    free((*list)->items);
    free(*list);
    *list = NULL;
}

void u16_list_frees(u16_list_t *list) {
    if (!list) {
        return;
    }

    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}
