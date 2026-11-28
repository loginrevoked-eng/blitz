#ifndef SCAN_H
#define SCAN_H

#include <pcap/pcap.h>
#include <stddef.h>
#include <stdint.h>

#include "pkt.h"
#include "route.h"

void syn_cfg_init(syn_scan_config_t *cfg, const struct route_t *route, uint16_t port, const uint8_t *key, size_t keylen);
int  send_pkt(pcap_t *hndl, const syn_scan_config_t *cfg, uint32_t ip, uint16_t port);

#endif // SCAN_H
