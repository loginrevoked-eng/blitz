#include "pkt.h"

#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>



static inline void scan_target_normalize(scan_target_t *target)
{
    uint32_t v;
    memcpy(&v, target->ip, 4);
    v = (v >> 24)
      | ((v >> 8) & 0x0000FF00u)
      | ((v << 8) & 0x00FF0000u)
      | (v << 24);
    memcpy(target->ip, &v, 4);
}
static void write_u16_big_endian(uint8_t *buf, uint16_t value)
{
    buf[0] = value >> 8;
    buf[1] = value & 0xFF;
}

static void write_u32_big_endian(uint8_t *buf, uint32_t value)
{
    buf[0] = value >> 24;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

static uint16_t read_u16_big_endian(const uint8_t *buf)
{
    return (uint16_t)(buf[0] << 8 | buf[1]);
}

static uint32_t read_u32_big_endian(const uint8_t *buf)
{
    return (uint32_t)buf[0] << 24 |
           (uint32_t)buf[1] << 16 |
           (uint32_t)buf[2] << 8 |
           buf[3];
}

static uint32_t csum_add(uint32_t sum, const uint8_t *data, size_t len) {
    size_t i;
    for (i = 0; i + 1 < len; i += 2)
        sum += read_u16_big_endian(data + i);
    if (len & 1)
        sum += (uint32_t)data[len - 1] << 8;
    return sum;
}

static uint16_t csum_fold(uint32_t sum) {
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

uint16_t calculate_checksum(const uint8_t *data, size_t len) {
    return csum_fold(csum_add(0, data, len));
}

void tcp_init(tcp_hdr_t *t) {
    memset(t, 0, sizeof *t);
    t->flags = TCP_FLAG_SYN;
    t->window = 65535;
}

void ip_init(ip_hdr_t *ip) {
    memset(ip, 0, sizeof *ip);
    ip->ttl = 64;
    ip->packet_id = 54321;
    tcp_init(&ip->tcp);
}

void eth_init(eth_hdr_t *eth) {
    memset(eth, 0, sizeof *eth);
    eth->ethertype = 0x0800;
    ip_init(&eth->ip);
}

enum
{
    TCP_OFFSET_SRC_PORT = 0,
    TCP_OFFSET_DST_PORT = 2,
    TCP_OFFSET_SEQ = 4,
    TCP_OFFSET_ACK = 8,
    TCP_OFFSET_DATA_OFFSET_AND_FLAGS = 12,
    TCP_OFFSET_WINDOW = 14,
    TCP_OFFSET_CHECKSUM = 16,
    TCP_OFFSET_URGENT_POINTER = 18
};

enum
{
    IP_OFFSET_VERSION_AND_IHL = 0,
    IP_OFFSET_TOS = 1,
    IP_OFFSET_TOTAL_LENGTH = 2,
    IP_OFFSET_PACKET_ID = 4,
    IP_OFFSET_FLAGS_AND_FRAGMENT = 6,
    IP_OFFSET_TTL = 8,
    IP_OFFSET_PROTOCOL = 9,
    IP_OFFSET_CHECKSUM = 10,
    IP_OFFSET_SRC_IP = 12,
    IP_OFFSET_DST_IP = 16
};

enum
{
    ETH_OFFSET_DST_MAC = 0,
    ETH_OFFSET_SRC_MAC = 6,
    ETH_OFFSET_ETHERTYPE = 12
};

#define IP_VERSION_4_IHL_5 0x45
#define IP_PROTOCOL_TCP 6
#define TCP_DATA_OFFSET_5_WORDS (5 << 12)

static void tcp_write_header(const tcp_hdr_t *t, uint8_t *out)
{
    write_u16_big_endian(out + TCP_OFFSET_SRC_PORT, t->src_port);
    write_u16_big_endian(out + TCP_OFFSET_DST_PORT, t->dst_port);
    write_u32_big_endian(out + TCP_OFFSET_SEQ, t->seq);
    write_u32_big_endian(out + TCP_OFFSET_ACK, t->ack);
    write_u16_big_endian(out + TCP_OFFSET_DATA_OFFSET_AND_FLAGS,
                         TCP_DATA_OFFSET_5_WORDS | t->flags);
    write_u16_big_endian(out + TCP_OFFSET_WINDOW, t->window);
    write_u16_big_endian(out + TCP_OFFSET_CHECKSUM, 0);
    write_u16_big_endian(out + TCP_OFFSET_URGENT_POINTER, 0);
}

static void tcp_write_payload(const tcp_hdr_t *t, uint8_t *out)
{
    if (t->payload_len > 0)
    {
        memcpy(out + TCP_HLEN, t->payload, t->payload_len);
    }
}

static uint16_t tcp_compute_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                                     const uint8_t *segment, size_t segment_len)
{
    uint8_t pseudo_header[12];
    memcpy(pseudo_header, src_ip, 4);
    memcpy(pseudo_header + 4, dst_ip, 4);
    pseudo_header[8] = 0;
    pseudo_header[9] = IP_PROTOCOL_TCP;
    write_u16_big_endian(pseudo_header + 10, (uint16_t)segment_len);

    uint32_t sum = csum_add(0, pseudo_header, sizeof pseudo_header);
    sum = csum_add(sum, segment, segment_len);
    return csum_fold(sum);
}

size_t tcp_to_bytes(const tcp_hdr_t *t, const uint8_t src_ip[4], const uint8_t dst_ip[4],
                    uint8_t *out, size_t cap)
{
    size_t segment_len = TCP_HLEN + t->payload_len;
    if (cap < segment_len)
    {
        return 0;
    }

    tcp_write_header(t, out);
    tcp_write_payload(t, out);

    uint16_t checksum = tcp_compute_checksum(src_ip, dst_ip, out, segment_len);
    write_u16_big_endian(out + TCP_OFFSET_CHECKSUM, checksum);
    return segment_len;
}

static void ip_write_header(const ip_hdr_t *ip, size_t total_length, uint8_t *out)
{
    out[IP_OFFSET_VERSION_AND_IHL] = IP_VERSION_4_IHL_5;
    out[IP_OFFSET_TOS] = 0;
    write_u16_big_endian(out + IP_OFFSET_TOTAL_LENGTH, (uint16_t)total_length);
    write_u16_big_endian(out + IP_OFFSET_PACKET_ID, ip->packet_id);
    write_u16_big_endian(out + IP_OFFSET_FLAGS_AND_FRAGMENT, 0);
    out[IP_OFFSET_TTL] = ip->ttl;
    out[IP_OFFSET_PROTOCOL] = IP_PROTOCOL_TCP;
    write_u16_big_endian(out + IP_OFFSET_CHECKSUM, 0);
    memcpy(out + IP_OFFSET_SRC_IP, ip->src_ip, 4);
    memcpy(out + IP_OFFSET_DST_IP, ip->dst_ip, 4);
}

static uint16_t ip_compute_checksum(const uint8_t *header)
{
    return calculate_checksum(header, IP_HLEN);
}

size_t ip_to_bytes(const ip_hdr_t *ip, uint8_t *out, size_t cap)
{
    size_t total_length = IP_HLEN + TCP_HLEN + ip->tcp.payload_len;
    if (total_length > 0xFFFF || cap < total_length)
    {
        return 0;
    }

    ip_write_header(ip, total_length, out);
    write_u16_big_endian(out + IP_OFFSET_CHECKSUM, ip_compute_checksum(out));

    size_t tcp_len = tcp_to_bytes(&ip->tcp, ip->src_ip, ip->dst_ip,
                                  out + IP_HLEN, cap - IP_HLEN);
    if (tcp_len == 0)
    {
        return 0;
    }
    return total_length;
}

static void eth_write_header(const eth_hdr_t *eth, uint8_t *out)
{
    memcpy(out + ETH_OFFSET_DST_MAC, eth->dst_mac, 6);
    memcpy(out + ETH_OFFSET_SRC_MAC, eth->src_mac, 6);
    write_u16_big_endian(out + ETH_OFFSET_ETHERTYPE, eth->ethertype);
}

size_t eth_to_bytes(const eth_hdr_t *eth, uint8_t *out, size_t cap)
{
    if (cap < ETH_HLEN)
    {
        return 0;
    }

    eth_write_header(eth, out);

    size_t ip_len = ip_to_bytes(&eth->ip, out + ETH_HLEN, cap - ETH_HLEN);
    if (ip_len == 0)
    {
        return 0;
    }
    return ETH_HLEN + ip_len;
}

int eth_from_wire(const uint8_t *data, size_t len, eth_hdr_t *out) {
    if (len < 54)
        return -1;
    if (read_u16_big_endian(data + 12) != 0x0800)
        return -1;

    uint8_t ver_ihl = data[14];
    if ((ver_ihl >> 4) != 4)
        return -1;
    size_t ihl = (size_t)(ver_ihl & 0x0F) * 4;
    if (ihl < 20 || len < ETH_HLEN + ihl + 20)
        return -1;
    if (data[23] != 6)
        return -1;

    size_t t = ETH_HLEN + ihl;
    uint16_t offset_flags = read_u16_big_endian(data + t + 12);
    size_t tcp_hlen = (size_t)(offset_flags >> 12) * 4;
    if (tcp_hlen < 20)
        return -1;

    size_t end = ETH_HLEN + (size_t)read_u16_big_endian(data + 16);
    if (end > len)
        end = len;
    size_t pstart = t + tcp_hlen;

    memset(out, 0, sizeof *out);
    memcpy(out->dst_mac, data, 6);
    memcpy(out->src_mac, data + 6, 6);
    out->ethertype = 0x0800;

    ip_hdr_t *ip = &out->ip;
    ip->packet_id = read_u16_big_endian(data + 18);
    ip->ttl = data[22];
    memcpy(ip->src_ip, data + 26, 4);
    memcpy(ip->dst_ip, data + 30, 4);

    tcp_hdr_t *tcp = &ip->tcp;
    tcp->src_port = read_u16_big_endian(data + t);
    tcp->dst_port = read_u16_big_endian(data + t + 2);
    tcp->seq = read_u32_big_endian(data + t + 4);
    tcp->ack = read_u32_big_endian(data + t + 8);
    tcp->flags = offset_flags & 0x01FF;
    tcp->window = read_u16_big_endian(data + t + 14);
    if (pstart < end) {
        tcp->payload = data + pstart;
        tcp->payload_len = end - pstart;
    }
    return 0;
}

uint32_t compute_cookie(const scan_cookie_t cookie, const uint8_t *key, size_t key_len) {
    uint8_t tuple[12];
    memcpy(tuple, cookie.src_ip, 4);
    write_u16_big_endian(tuple + 4, cookie.src_port);
    memcpy(tuple + 6, cookie.dst_ip, 4);
    write_u16_big_endian(tuple + 10, cookie.dst_port);

    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, key, key_len);
    EVP_DigestUpdate(ctx, tuple, sizeof tuple);
    EVP_DigestFinal_ex(ctx, digest, &dlen);
    EVP_MD_CTX_free(ctx);
    return read_u32_big_endian(digest);
}

static void syn_set_id(eth_hdr_t *eth) {
    uint8_t id_bytes[2];
    if (RAND_bytes(id_bytes, sizeof id_bytes) == 1)
        eth->ip.packet_id = read_u16_big_endian(id_bytes);
}

void sign_packet(eth_hdr_t *eth, const uint8_t *key, size_t key_len) {
    const ip_hdr_t *ip = &eth->ip;
    scan_cookie_t cookie;
    memcpy(cookie.src_ip, ip->src_ip, 4);
    cookie.src_port = ip->tcp.src_port;
    memcpy(cookie.dst_ip, ip->dst_ip, 4);
    cookie.dst_port = ip->tcp.dst_port;
    eth->ip.tcp.seq = compute_cookie(cookie, key, key_len);
}

int verify_incoming(const eth_hdr_t *reply, const uint8_t *key, size_t key_len) {
    const ip_hdr_t *ip = &reply->ip;

    scan_cookie_t cookie;
    memcpy(cookie.src_ip, ip->dst_ip, 4);
    cookie.src_port = ip->tcp.dst_port;
    memcpy(cookie.dst_ip, ip->src_ip, 4);
    cookie.dst_port = ip->tcp.src_port;

    uint32_t expected = compute_cookie(cookie, key, key_len);
    uint32_t received = ip->tcp.ack - 1;

    uint8_t a[4], b[4];
    write_u32_big_endian(a, expected);
    write_u32_big_endian(b, received);
    return CRYPTO_memcmp(a, b, 4) == 0;
}

void syn_init(eth_hdr_t *out, const syn_scan_config_t *cfg, const scan_target_t *target) {
    eth_init(out);
    memcpy(out->dst_mac, cfg->gateway_mac, 6);
    memcpy(out->src_mac, cfg->if_mac, 6);
    memcpy(out->ip.src_ip, cfg->if_ipv4, 4);

    scan_target_t t = *target;
    scan_target_normalize(&t);
    memcpy(out->ip.dst_ip, t.ip, 4);

    out->ip.tcp.src_port = cfg->src_port;
    out->ip.tcp.dst_port = t.port;
    syn_set_id(out);
    sign_packet(out, cfg->seq_key, cfg->seq_key_len);
}

static void tcp_flags_to_string(uint16_t flags, char *out, size_t len)
{
    static const struct
    {
        uint16_t bit;
        const char *name;
    } names[] = {
        { 0x001, "FIN" }, { 0x002, "SYN" }, { 0x004, "RST" },
        { 0x008, "PSH" }, { 0x010, "ACK" }, { 0x020, "URG" },
        { 0x040, "ECE" }, { 0x080, "CWR" }, { 0x100, "NS" }
    };

    size_t used = 0;
    out[0] = '\0';

    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
    {
        if (!(flags & names[i].bit))
        {
            continue;
        }
        if (used >= len)
        {
            break;
        }
        used += (size_t)snprintf(out + used, len - used, "%s%s", used ? "|" : "", names[i].name);
    }

    if (used == 0)
    {
        snprintf(out, len, "none");
    }
}

int eth_printf(const eth_hdr_t *hdr, char *out, size_t len)
{
    const ip_hdr_t *ip = &hdr->ip;
    const tcp_hdr_t *tcp = &ip->tcp;

    char flags[48];
    tcp_flags_to_string(tcp->flags, flags, sizeof flags);

    return snprintf(out, len,
                    "ETH  dst=%02x:%02x:%02x:%02x:%02x:%02x src=%02x:%02x:%02x:%02x:%02x:%02x type=0x%04x\n"
                    "IP   src=%u.%u.%u.%u dst=%u.%u.%u.%u ttl=%u id=%u\n"
                    "TCP  sport=%u dport=%u seq=%u ack=%u flags=%s window=%u payload=%zu\n",
                    hdr->dst_mac[0], hdr->dst_mac[1], hdr->dst_mac[2],
                    hdr->dst_mac[3], hdr->dst_mac[4], hdr->dst_mac[5],
                    hdr->src_mac[0], hdr->src_mac[1], hdr->src_mac[2],
                    hdr->src_mac[3], hdr->src_mac[4], hdr->src_mac[5],
                    hdr->ethertype,
                    ip->src_ip[0], ip->src_ip[1], ip->src_ip[2], ip->src_ip[3],
                    ip->dst_ip[0], ip->dst_ip[1], ip->dst_ip[2], ip->dst_ip[3],
                    ip->ttl, ip->packet_id,
                    tcp->src_port, tcp->dst_port, tcp->seq, tcp->ack,
                    flags, tcp->window, tcp->payload_len);
}
uint64_t scan_target_u64_cannonical_key(scan_target_t *t)
{
    return ((uint64_t)t->ip[0] << 40) | ((uint64_t)t->ip[1] << 32) |
           ((uint64_t)t->ip[2] << 24) | ((uint64_t)t->ip[3] << 16) |
           t->port;
}
