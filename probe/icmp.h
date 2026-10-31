#ifndef PROBE_ICMP_INTERNAL_H
#define PROBE_ICMP_INTERNAL_H

#include "probe.h"

IcmpResult IcmpDo(const IcmpConfig *icmp, struct in_addr addr,
                 ProbeManager *manager);

#endif
