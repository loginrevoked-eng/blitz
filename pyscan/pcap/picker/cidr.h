#ifndef CIDR_H
#define CIDR_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t Ipv4;

typedef struct {
    Ipv4 start;
    Ipv4 end;
} CidrRange;

typedef struct {
    Ipv4 next;
    int started;
    int finished;
} CidrEnumerator;

CidrRange *CidrRangeInit(Ipv4 start, Ipv4 end);
CidrRange *CidrRangeNew(const char *text);
uint64_t CidrRangeLen(const CidrRange *range);
size_t BatchIndexFromIp(CidrRange *range, uint32_t spacing, Ipv4 ip);
char *Ipv4Repr(Ipv4 ip);
char *CidrRangeRepr(CidrRange *range);
char *CidrEnumeratorRepr(CidrEnumerator *enumerator);
void CidrEnumeratorInit(CidrEnumerator *enumerator);
int EnumerateCidr(CidrEnumerator *enumerator, const CidrRange *range, Ipv4 *ip);

#endif
