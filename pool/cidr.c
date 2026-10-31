#include "cidr.h"
#include "str/str.h"
#include <stdlib.h>

static int ParseDecimal(const char **p, int max_digits, unsigned long max_val, unsigned long *out)
{
    const char *s = *p;
    unsigned long val = 0;
    int digits = 0;

    while (*s >= '0' && *s <= '9') {
        if (++digits > max_digits)
            return 0;
        val = val * 10 + (unsigned long)(*s - '0');
        s++;
    }
    if (digits == 0 || val > max_val)
        return 0;

    *out = val;
    *p = s;
    return 1;
}

static Ipv4 PrefixToMask(unsigned long prefix)
{
    return (0xFFFFFFFFULL << (32 - prefix)) & 0xFFFFFFFFULL;
}

CidrRange *CidrRangeInit(Ipv4 start, Ipv4 end)
{
    if (start > end)
        return NULL;

    CidrRange *range = malloc(sizeof *range);
    if (range == NULL)
        return NULL;
    range->start = start;
    range->end = end;
    return range;
}

uint64_t CidrRangeLen(const CidrRange *range)
{
    if (range == NULL || range->start > range->end)
        return 0;
    return range->end - range->start + 1;
}

size_t BatchIndexFromIp(CidrRange *range, uint32_t spacing, Ipv4 ip)
{
    if (range == NULL || range->start > range->end || ip < range->start || ip > range->end)
        return SIZE_MAX;

    uint64_t batch_width = (uint64_t)spacing + 1;
    return (size_t)((ip - range->start) / batch_width);
}

CidrRange *CidrRangeNew(char *text)
{
    if (text == NULL)
        return NULL;

    const char *p = text;
    Ipv4 ip = 0;
    unsigned long octet, prefix = 32;
    for (int i = 0; i < 4; i++) {
        if (!ParseDecimal(&p, 3, 255, &octet))
            return NULL;
        ip = (ip << 8) | octet;
        if (i < 3) {
            if (*p != '.')
                return NULL;
            p++;
        }
    }

    if (*p == '/') {
        p++;
        if (!ParseDecimal(&p, 2, 32, &prefix))
            return NULL;
    }
    if (*p != '\0')
        return NULL;

    Ipv4 mask = PrefixToMask(prefix);
    Ipv4 start = ip & mask;
    Ipv4 end = start | (~mask & 0xFFFFFFFFULL);
    return CidrRangeInit(start, end);
}

char *Ipv4Repr(Ipv4 ip)
{
    return strf("%u.%u.%u.%u",
                (unsigned)((ip >> 24) & 0xFF),
                (unsigned)((ip >> 16) & 0xFF),
                (unsigned)((ip >> 8) & 0xFF),
                (unsigned)(ip & 0xFF));
}

char *CidrRangeRepr(CidrRange *range)
{
    if (range == NULL)
        return NULL;

    return strf("%u.%u.%u.%u-%u.%u.%u.%u",
                (unsigned)((range->start >> 24) & 0xFF),
                (unsigned)((range->start >> 16) & 0xFF),
                (unsigned)((range->start >> 8) & 0xFF),
                (unsigned)(range->start & 0xFF),
                (unsigned)((range->end >> 24) & 0xFF),
                (unsigned)((range->end >> 16) & 0xFF),
                (unsigned)((range->end >> 8) & 0xFF),
                (unsigned)(range->end & 0xFF));
}

char *CidrEnumeratorRepr(CidrEnumerator *enumerator)
{
    if (enumerator == NULL)
        return NULL;

    if (!enumerator->started)
        return strf("CidrEnumerator(not started)");
    if (enumerator->finished)
        return strf("CidrEnumerator(finished)");

    char *next_ip = Ipv4Repr(enumerator->next);
    if (next_ip == NULL)
        return NULL;
    char *repr = strf("CidrEnumerator(next=%s)", next_ip);
    free(next_ip);
    return repr;
}

void CidrEnumeratorInit(CidrEnumerator *enumerator)
{
    if (enumerator == NULL)
        return;
    enumerator->next = 0;
    enumerator->started = 0;
    enumerator->finished = 0;
}

int EnumerateCidr(CidrEnumerator *enumerator, const CidrRange *range, Ipv4 *ip)
{
    if (enumerator == NULL || range == NULL || ip == NULL || range->start > range->end || enumerator->finished)
        return 0;

    if (!enumerator->started) {
        enumerator->next = range->start;
        enumerator->started = 1;
    }
    if (enumerator->next < range->start || enumerator->next > range->end) {
        enumerator->finished = 1;
        return 0;
    }

    *ip = enumerator->next;
    if (enumerator->next == range->end)
        enumerator->finished = 1;
    else
        enumerator->next++;
    return 1;
}
