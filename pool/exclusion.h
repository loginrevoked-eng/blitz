#ifndef POOL_EXCLUSION_H
#define POOL_EXCLUSION_H

#include <stdint.h>
#include <stddef.h>

#include "cidr.h"

typedef struct {
    uint32_t net;
    uint32_t mask;
} CidrMask;

typedef int (*ExcludeRangeCallback)(uint64_t start, uint64_t end, void *context);

/* Always excludes Cloudflare ranges; optionally excludes private/reserved ranges. */
int Exclude(Ipv4 ip, int exclude_private_reserved);

/* Visits the portions of [start, end] that are not excluded. */
int ExcludeRanges(uint64_t start, uint64_t end, int exclude_private_reserved,
    ExcludeRangeCallback callback, void *context);

#endif
