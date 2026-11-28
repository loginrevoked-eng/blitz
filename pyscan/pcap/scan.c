#include "scan.h"

#include <string.h>

#include "logging.h"
#include "pcap_i.h"

static int build_pkt(eth_hdr_t *pkt, const syn_scan_config_t *cfg, uint32_t ip, uint16_t port) {
    scan_target_t t;
    memcpy(t.ip, &ip, 4);
    t.port = port;

    syn_init(pkt, cfg, &t);

    char eth_r[1024];
    eth_printf(pkt, eth_r, sizeof(eth_r));
    info("initialized the following packet...\n%s", eth_r);
    return 0;
}

int send_pkt(pcap_t *hndl, const syn_scan_config_t *cfg, uint32_t ip, uint16_t port) {
    eth_hdr_t pkt;
    int err;
    if ((err = build_pkt(&pkt, cfg, ip, port)) != 0) {
        return err;
    }

    return send_pkt_pcap(hndl, &pkt);
}

void syn_cfg_init(syn_scan_config_t *cfg, const struct route_t *route, uint16_t port, const uint8_t *key, size_t keylen) {
    memset(cfg, 0, sizeof(*cfg));
    memcpy(cfg->if_mac, route->if_addr.mac, 6);
    memcpy(cfg->if_ipv4, &route->if_addr.ipv4, 4);
    memcpy(cfg->gateway_mac, route->gw_addr.mac, 6);
    cfg->src_port = port;
    cfg->seq_key = key;
    cfg->seq_key_len = keylen;
}
