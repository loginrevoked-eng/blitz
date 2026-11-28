#include "route.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

static PIP_ADAPTER_ADDRESSES fetch_adapters(void) {
    ULONG buf_len = 15000;
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
    if (!adapters) {
        error("Failed to allocate adapter buffer");
        return NULL;
    }

    ULONG flags = GAA_FLAG_SKIP_ANYCAST |
                  GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER |
                  GAA_FLAG_INCLUDE_GATEWAYS;

    ULONG ret = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(adapters);
        adapters = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
        if (!adapters) {
            error("Failed to reallocate adapter buffer");
            return NULL;
        }
        ret = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    }

    if (ret != ERROR_SUCCESS) {
        error("GetAdaptersAddresses failed with error code %lu", ret);
        free(adapters);
        return NULL;
    }

    return adapters;
}

static bool get_eth(PIP_ADAPTER_ADDRESSES adapters, struct route_t *r) {
    for (PIP_ADAPTER_ADDRESSES curr = adapters; curr != NULL; curr = curr->Next) {
        if (curr->IfType != IF_TYPE_ETHERNET_CSMACD || curr->OperStatus != IfOperStatusUp) {
            continue;
        }

        if (!curr->FirstGatewayAddress) {
            continue;
        }

        uint32_t self_ipv4 = 0;
        for (PIP_ADAPTER_UNICAST_ADDRESS ua = curr->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *sa_in = (struct sockaddr_in *)ua->Address.lpSockaddr;
                self_ipv4 = sa_in->sin_addr.s_addr;
                break;
            }
        }

        if (self_ipv4 == 0) {
            continue;
        }

        snprintf(r->if_name, sizeof(r->if_name), "\\Device\\NPF_%s", curr->AdapterName);
        memcpy(r->if_addr.mac, curr->PhysicalAddress, sizeof(r->if_addr.mac));
        r->if_addr.ipv4 = self_ipv4;
        return true;
    }

    return false;
}

static bool get_gw(PIP_ADAPTER_ADDRESSES adapters, struct route_t *r) {
    const char *guid = strchr(r->if_name, '{');
    if (!guid) {
        return false;
    }

    for (PIP_ADAPTER_ADDRESSES curr = adapters; curr != NULL; curr = curr->Next) {
        if (_stricmp(curr->AdapterName, guid) != 0) {
            continue;
        }

        uint32_t gw_ipv4 = 0;
        for (PIP_ADAPTER_GATEWAY_ADDRESS_LH ga = curr->FirstGatewayAddress; ga != NULL; ga = ga->Next) {
            if (ga->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *sa_in = (struct sockaddr_in *)ga->Address.lpSockaddr;
                gw_ipv4 = sa_in->sin_addr.s_addr;
                break;
            }
        }

        if (gw_ipv4 == 0) {
            return false;
        }

        ULONG gw_mac[2] = {0};
        ULONG mac_len = 6;
        if (SendARP((IPAddr)gw_ipv4, (IPAddr)r->if_addr.ipv4, gw_mac, &mac_len) != NO_ERROR || mac_len != 6) {
            error("SendARP failed for gateway on %s", r->if_name);
            return false;
        }

        r->gw_addr.ipv4 = gw_ipv4;
        memcpy(r->gw_addr.mac, gw_mac, sizeof(r->gw_addr.mac));
        return true;
    }

    return false;
}

bool route_init(struct route_t *r) {
    if (!r) {
        return false;
    }
    memset(r, 0, sizeof(*r));

    PIP_ADAPTER_ADDRESSES adapters = fetch_adapters();
    if (!adapters) {
        return false;
    }

    bool ok = get_eth(adapters, r) && get_gw(adapters, r);

    free(adapters);
    return ok;
}

int route_sprintf(const struct route_t *r, char *out, size_t len) {
    if (!out || len == 0) {
        return -1;
    }

    if (!r || r->if_name[0] == '\0') {
        return snprintf(out, len, "Route: (invalid or empty)");
    }

    char if_ip_str[INET_ADDRSTRLEN] = {0};
    struct in_addr if_ip = { .s_addr = r->if_addr.ipv4 };
    inet_ntop(AF_INET, &if_ip, if_ip_str, sizeof(if_ip_str));

    char gw_ip_str[INET_ADDRSTRLEN] = {0};
    struct in_addr gw_ip = { .s_addr = r->gw_addr.ipv4 };
    inet_ntop(AF_INET, &gw_ip, gw_ip_str, sizeof(gw_ip_str));

    return snprintf(
        out, len,
        "Interface: %s\n"
        "Self MAC:  %02X:%02X:%02X:%02X:%02X:%02X | IPv4: %s\n"
        "GW   MAC:  %02X:%02X:%02X:%02X:%02X:%02X | IPv4: %s",
        r->if_name,
        r->if_addr.mac[0], r->if_addr.mac[1], r->if_addr.mac[2],
        r->if_addr.mac[3], r->if_addr.mac[4], r->if_addr.mac[5],
        if_ip_str,
        r->gw_addr.mac[0], r->gw_addr.mac[1], r->gw_addr.mac[2],
        r->gw_addr.mac[3], r->gw_addr.mac[4], r->gw_addr.mac[5],
        gw_ip_str
    );
}
