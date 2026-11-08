#ifndef PROBE_INIT_H
#define PROBE_INIT_H

#include "probe.h"

HostUpCheckProbeOptions *HostUpCheckProbeOptions_New(Ipv4 addr,
    DWORD timeout_ms, unsigned int retries);
PortList *PortList_New(size_t len);
IcmpOptions *IcmpOptions_New(const HostUpCheckProbeOptions *probe);
UdpOptions *UdpOptions_New(const HostUpCheckProbeOptions *probe,
    const PortList *ports);
TcpOptions *TcpOptions_New(const HostUpCheckProbeOptions *probe,
    const PortList *ports);
UdpResultWrapper *UdpResultWrapper_New(uint16_t port, UdpResult result,
    unsigned int retries);
TcpResultWrapper *TcpResultWrapper_New(uint16_t port, TcpResult result,
    unsigned int retries);
UdpResultList *UdpResultList_New(size_t len);
TcpResultList *TcpResultList_New(size_t len);
HostUpCheckResult *HostUpCheckResult_New(void);
HostUpCheck *HostUpCheck_New(HostUpCheckMethod kind, Ipv4 addr,
    DWORD timeout_ms, unsigned int retries, const PortList *ports);
void HostUpCheck_Free(HostUpCheck *check);
HostUpCheckThreadHandle *HostUpCheckThreadHandle_New(HANDLE handle);
HostUpCheckThreadContext *HostUpCheckThreadContext_New(void);
HostupCheckHub *HostupCheckHub_New(unsigned int workers);
void HostupCheckHub_Free(HostupCheckHub *hub);
int ProbeInit(ProbeContext *state, HostupCheckHub *hub, IpPicker *picker,
    const struct ProbeConfig *config);

#endif
