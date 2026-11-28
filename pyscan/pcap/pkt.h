#ifndef SYNPKT_H
#define SYNPKT_H

#include <stddef.h>
#include <stdint.h>

#define ETH_HLEN 14
#define IP_HLEN 20
#define TCP_HLEN 20
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_ACK 0x10



/* Individual TCP Flags (1-byte flags field at TCP header offset 13) */
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20
#define TCP_FLAG_ECE 0x40
#define TCP_FLAG_CWR 0x80

/* Common Flag Combinations */
#define TCP_FLAG_SYN_ACK (TCP_FLAG_SYN | TCP_FLAG_ACK) /* 0x12 - Open port reply */
#define TCP_FLAG_RST_ACK (TCP_FLAG_RST | TCP_FLAG_ACK) /* 0x14 - Closed port reply */
#define TCP_FLAG_FIN_ACK (TCP_FLAG_FIN | TCP_FLAG_ACK) /* 0x11 */



#define SYN_ETH_PKT_LEN 54

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t flags;
    uint16_t window;
    const uint8_t *payload;
    size_t payload_len;
} tcp_hdr_t;

typedef struct {
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint8_t ttl;
    uint16_t packet_id;
    tcp_hdr_t tcp;
} ip_hdr_t;

typedef struct {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    ip_hdr_t ip;
} eth_hdr_t;




typedef struct {
    uint8_t if_mac[6];
    uint8_t if_ipv4[4];
    uint8_t gateway_mac[6];
    uint16_t src_port;
    const uint8_t *seq_key;
    size_t seq_key_len;
} syn_scan_config_t;

typedef struct {
    uint8_t ip[4];
    uint16_t port;
} scan_target_t;


typedef struct {
    uint8_t src_ip[4];
    uint16_t src_port;
    uint8_t dst_ip[4];
    uint16_t dst_port;
}scan_cookie_t;

void tcp_init(tcp_hdr_t *t);
void ip_init(ip_hdr_t *ip);
void eth_init(eth_hdr_t *eth);

uint16_t calculate_checksum(const uint8_t *data, size_t len);


size_t tcp_to_bytes(const tcp_hdr_t *t, const uint8_t src_ip[4], const uint8_t dst_ip[4], uint8_t *out, size_t cap);
size_t ip_to_bytes(const ip_hdr_t *ip, uint8_t *out, size_t cap);
size_t eth_to_bytes(const eth_hdr_t *eth, uint8_t *out, size_t cap);

int eth_from_wire(const uint8_t *data, size_t len, eth_hdr_t *out);

uint32_t compute_cookie(const scan_cookie_t cookie, const uint8_t *key, size_t key_len);

void sign_packet(eth_hdr_t *eth, const uint8_t *key, size_t key_len);
int verify_incoming(const eth_hdr_t *reply, const uint8_t *key, size_t key_len);

int eth_printf(const eth_hdr_t *hdr, char *out, size_t len);

void syn_init(eth_hdr_t *out, const syn_scan_config_t *cfg, const scan_target_t *target);
uint64_t scan_target_u64_cannonical_key(scan_target_t *t);
#endif
