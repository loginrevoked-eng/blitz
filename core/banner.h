#ifndef CORE_BANNER_H
#define CORE_BANNER_H

#include "../pool/cidr.h"
#include "../ev/tcp/public.h"
#include "probe.h"
#include <stdint.h>

void RegisterTask(Ipv4 ip, uint16_t port);
void BannerConfigureConnect(uint32_t timeout_ms, uint32_t retries);
int BannerBeginHost(Ipv4 ip, uint32_t port_count, ProbeStats *stats,
    int track_reachability);
void BannerEndHost(Ipv4 ip);
BOOL OnConnect(TcpConnectorTask *task);
BOOL BannerPretty(TcpConnectorTask *task);
void BannerWait(void);

#endif
