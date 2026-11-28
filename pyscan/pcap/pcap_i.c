#include "pcap_i.h"

#include <windows.h>
#include <stdio.h>

#include "logging.h"

#pragma comment(lib, "wpcap.lib")

void require_wpcap(void) {
    SetDllDirectoryA("C:\\Windows\\System32\\Npcap");

    HMODULE mod = LoadLibraryA("wpcap.dll");
    if (!mod) {
        panic(1, "wpcap.dll is missing in the system.");
    }

    FreeLibrary(mod);
}

pcap_t *open_handle(const struct route_t *r, char *err, size_t len)
{
    if (err && len > 0) {
        err[0] = '\0';
    }

    if (!r || r->if_name[0] == '\0') {
        const char *msg = "Cannot open handle on invalid route";
        error("%s", msg);
        if (err && len > 0) {
            snprintf(err, len, "%s", msg);
        }
        return NULL;
    }

    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    debug("Opening live handle on %s", r->if_name);

    pcap_t *handle = pcap_open_live(r->if_name, 65535, 1, 1000, errbuf);
    if (!handle) {
        error("pcap_open_live failed for %s: %s", r->if_name, errbuf);
        if (err && len > 0) {
            snprintf(err, len, "%s", errbuf);
        }
        return NULL;
    }

    return handle;
}


void set_filters(pcap_t *hndl, uint16_t src_port) {
    if (!hndl) {
        error("Cannot set filter on NULL handle");
        return;
    }

    char expr[32];
    snprintf(expr, sizeof(expr), "tcp dst port %u", src_port);

    struct bpf_program filter = {0};
    const int optimize = 1;
    const bpf_u_int32 netmask = PCAP_NETMASK_UNKNOWN;

    if (pcap_compile(hndl, &filter, expr, optimize, netmask) == -1) {
        error("pcap_compile failed for '%s': %s", expr, pcap_geterr(hndl));
        return;
    }

    if (pcap_setfilter(hndl, &filter) == -1) {
        error("pcap_setfilter failed for '%s': %s", expr, pcap_geterr(hndl));
        pcap_freecode(&filter);
        return;
    }

    debug("BPF filter applied: %s", expr);
    pcap_freecode(&filter);
}


int send_pkt_pcap(pcap_t *hndl, const eth_hdr_t *pkt)
{
    uint8_t bytes[SYN_ETH_PKT_LEN];
    size_t len = eth_to_bytes(pkt, bytes, sizeof(bytes));
    if (len == 0) {
        error("failed to serialize packet");
        return -1;
    }

    int ret = pcap_sendpacket(hndl, bytes, (int)len);
    if (ret != 0) {
        error("pcap failed to send: %s", pcap_geterr(hndl));
        return ret;
    }

    return 0;
}


int recv_pcap_pkt(const syn_scan_config_t *cfg, pcap_t *hndl, scan_target_t *alive_host, int *open)
{
    int local_open = 0;
    open = open ? open : &local_open;
    struct pcap_pkthdr *header;
    const u_char *pkt_data;
    int res = pcap_next_ex(hndl, &header, &pkt_data);

    if (res == 0) {
        //info("Timed out waiting for reply.");
        return -2;
    }

    if (res < 0) {
        error("pcap_next_ex error: %s", pcap_geterr(hndl));
        return -3;
    }

    eth_hdr_t eth;
    if (eth_from_wire(pkt_data, header->caplen, &eth) != 0) {
        return -1;
    }

    if (!verify_incoming(&eth, cfg->seq_key, cfg->seq_key_len)) {
        return 1;
    }

    uint8_t f = eth.ip.tcp.flags;
    if (f & (TCP_FLAG_SYN | TCP_FLAG_RST))
    {
        if (alive_host)
        {
            memcpy(alive_host->ip, eth.ip.src_ip, 4);
            alive_host->port = eth.ip.tcp.src_port;
        }

        *open = !(f & TCP_FLAG_RST);

        return 0;
    }

    return 1;//neither ack nor rst
}
