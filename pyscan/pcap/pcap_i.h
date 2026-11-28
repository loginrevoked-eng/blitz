#ifndef PCAP_I_H
#define PCAP_I_H

#include <pcap/pcap.h>
#include <stdint.h>
#include "pkt.h"

#include "route.h"

void    require_wpcap(void);
pcap_t *open_handle(const struct route_t *r, char *err, size_t le);
void    set_filters(pcap_t *hndl, uint16_t src_port);
int send_pkt_pcap(pcap_t *hndl, const eth_hdr_t *pkt);
int recv_pcap_pkt(const syn_scan_config_t *cfg, pcap_t *hndl, scan_target_t *alive_host, int *open);
#endif // PCAP_I_H
