#ifndef ROUTE_H
#define ROUTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mac_ip_t {
    uint8_t  mac[6];
    uint32_t ipv4;
};

struct route_t {
    char            if_name[128];
    struct mac_ip_t if_addr;
    struct mac_ip_t gw_addr;
};

bool route_init(struct route_t *r);
int  route_sprintf(const struct route_t *r, char *out, size_t len);

#endif // ROUTE_H
