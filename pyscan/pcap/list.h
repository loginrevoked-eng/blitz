#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdint.h>

#include "pkt.h"

/* --- scan_target_t list --- */

typedef struct {
    scan_target_t *targets;
    uint32_t       len;
    uint32_t       cap;
} scan_target_list_t;

int  list_init(scan_target_list_t **list, uint32_t initial_cap);
int  list_inits(scan_target_list_t *list, uint32_t initial_cap);

int  list_ensure_cap(scan_target_list_t **list, size_t more);
int  list_push(scan_target_list_t **list, scan_target_t out);
int  list_pop(scan_target_list_t *list, scan_target_t *out);

void list_free(scan_target_list_t **list);
void list_frees(scan_target_list_t *list);

/* --- uint16_t list --- */

typedef struct {
    uint16_t *items;
    uint32_t  len;
    uint32_t  cap;
} u16_list_t;

int  u16_list_init(u16_list_t **list, uint32_t initial_cap);
int  u16_list_inits(u16_list_t *list, uint32_t initial_cap);

int  u16_list_ensure_cap(u16_list_t **list, size_t more);
int  u16_list_push(u16_list_t **list, uint16_t val);
int  u16_list_pop(u16_list_t *list, uint16_t *out);

void u16_list_free(u16_list_t **list);
void u16_list_frees(u16_list_t *list);

#endif // LIST_H
