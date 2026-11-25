#include "exclusion.h"

#include <stdlib.h>
#include <string.h>

static const CidrMask cf_masks[] = {
    { 0xADF53000, 0xFFFFF000 }, /* 173.245.48.0/20 */
    { 0x6715F400, 0xFFFFFC00 }, /* 103.21.244.0/22 */
    { 0x6716C800, 0xFFFFFC00 }, /* 103.22.200.0/22 */
    { 0x671F0400, 0xFFFFFC00 }, /* 103.31.4.0/22 */
    { 0x8D654000, 0xFFFFC000 }, /* 141.101.64.0/18 */
    { 0x6CA2C000, 0xFFFFC000 }, /* 108.162.192.0/18 */
    { 0xBE5DF000, 0xFFFFF000 }, /* 190.93.240.0/20 */
    { 0xBC726000, 0xFFFFF000 }, /* 188.114.96.0/20 */
    { 0xC5EAF000, 0xFFFFFC00 }, /* 197.234.240.0/22 */
    { 0xC6298000, 0xFFFF8000 }, /* 198.41.128.0/17 */
    { 0xA29E0000, 0xFFFE0000 }, /* 162.158.0.0/15 */
    { 0x68100000, 0xFFF80000 }, /* 104.16.0.0/13 */
    { 0x68180000, 0xFFFC0000 }, /* 104.24.0.0/14 */
    { 0xAC400000, 0xFFF80000 }, /* 172.64.0.0/13 */
    { 0x83004800, 0xFFFFFC00 }, /* 131.0.72.0/22 */
};

static const CidrMask private_reserved_masks[] = {
    { 0x00000000, 0xFF000000 }, /* 0.0.0.0/8 */
    { 0x0A000000, 0xFF000000 }, /* 10.0.0.0/8 */
    { 0x64400000, 0xFFC00000 }, /* 100.64.0.0/10 */
    { 0x7F000000, 0xFF000000 }, /* 127.0.0.0/8 */
    { 0xA9FE0000, 0xFFFF0000 }, /* 169.254.0.0/16 */
    { 0xAC100000, 0xFFF00000 }, /* 172.16.0.0/12 */
    { 0xC0000000, 0xFFFFFF00 }, /* 192.0.0.0/24 */
    { 0xC0000200, 0xFFFFFF00 }, /* 192.0.2.0/24 */
    { 0xC0A80000, 0xFFFF0000 }, /* 192.168.0.0/16 */
    { 0xC6120000, 0xFFFE0000 }, /* 198.18.0.0/15 */
    { 0xC6336400, 0xFFFFFF00 }, /* 198.51.100.0/24 */
    { 0xCB007100, 0xFFFFFF00 }, /* 203.0.113.0/24 */
    { 0xE0000000, 0xF0000000 }, /* 224.0.0.0/4 */
    { 0xF0000000, 0xF0000000 }, /* 240.0.0.0/4 */
};

static int MaskMatches(uint32_t ip, const CidrMask *masks, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if ((ip & masks[i].mask) == masks[i].net)
            return 1;
    }
    return 0;
}

int Exclude(Ipv4 ip, int exclude_private_reserved)
{
    uint32_t address = (uint32_t)ip;
    if (MaskMatches(address, cf_masks, sizeof(cf_masks) / sizeof(cf_masks[0])))
        return 1;
    return exclude_private_reserved &&
        MaskMatches(address, private_reserved_masks,
            sizeof(private_reserved_masks) / sizeof(private_reserved_masks[0]));
}

static int CompareMasks(const void *left, const void *right)
{
    const CidrMask *a = left;
    const CidrMask *b = right;
    if (a->net < b->net) return -1;
    if (a->net > b->net) return 1;
    return 0;
}

int ExcludeRanges(uint64_t start, uint64_t end, int exclude_private_reserved,
    ExcludeRangeCallback callback, void *context)
{
    if (callback == NULL || start > end || end > UINT32_MAX)
        return 0;

    CidrMask masks[(sizeof(cf_masks) / sizeof(cf_masks[0])) +
        (sizeof(private_reserved_masks) / sizeof(private_reserved_masks[0]))];
    size_t count = sizeof(cf_masks) / sizeof(cf_masks[0]);
    memcpy(masks, cf_masks, sizeof(cf_masks));
    if (exclude_private_reserved) {
        memcpy(masks + count, private_reserved_masks, sizeof(private_reserved_masks));
        count += sizeof(private_reserved_masks) / sizeof(private_reserved_masks[0]);
    }
    qsort(masks, count, sizeof(*masks), CompareMasks);

    uint64_t cursor = start;
    for (size_t i = 0; i < count && cursor <= end; ++i) {
        uint64_t range_start = masks[i].net;
        uint64_t range_end = range_start | (uint32_t)~masks[i].mask;
        if (range_end < cursor || range_start > end)
            continue;
        if (range_start > cursor &&
            !callback(cursor, range_start - 1, context))
            return 0;
        if (range_end >= end)
            return 1;
        if (range_end >= cursor)
            cursor = range_end + 1;
    }

    return cursor > end || callback(cursor, end, context);
}
