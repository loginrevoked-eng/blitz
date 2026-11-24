#include "adbc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_unsigned(const char *text, unsigned long min, unsigned long max,
                          unsigned long *value) {
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < min || parsed > max) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s HOST [PORT] [--timeout MS]\n", program);
}

int main(int argc, char **argv) {
    const char *host;
    const char *port = "5555";
    unsigned timeout_ms = 3000;
    unsigned long parsed;
    int index = 2;
    AdbcClient client;

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    host = argv[1];

    if (index < argc && strncmp(argv[index], "--", 2) != 0) {
        if (parse_unsigned(argv[index], 1, 65535, &parsed) != 0) {
            adbc_log_error("invalid port: %s", argv[index]);
            return 2;
        }
        port = argv[index++];
    }
    while (index < argc) {
        if (strcmp(argv[index], "--timeout") == 0 && index + 1 < argc) {
            if (parse_unsigned(argv[index + 1], 1, 600000, &parsed) != 0) {
                adbc_log_error("timeout must be between 1 and 600000 milliseconds");
                return 2;
            }
            timeout_ms = (unsigned)parsed;
            index += 2;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (adbc_socket_probe(host, port, timeout_ms, &client) != 0) {
        return 1;
    }

    switch (client.state) {
        case ADBC_AUTH_REQUIRED:
            printf("ADB transport detected at %s:%s\n", host, port);
            printf("Response: AUTH TOKEN (authentication required)\n");
            printf("No key was sent; device banner is only available after authentication.\n");
            break;
        case ADBC_ONLINE:
            printf("ADB transport is online at %s:%s\n", host, port);
            printf("Device protocol version: 0x%08x\n", client.rx_arg0);
            printf("Device max payload: %u bytes\n", client.rx_arg1);
            printf("Device banner: %s\n", client.banner_size ? client.banner : "<empty>");
            break;
        case ADBC_TLS_REQUIRED:
            printf("ADB transport detected at %s:%s; TLS upgrade requested (STLS).\n",
                   host, port);
            break;
        case ADBC_OTHER_PACKET:
            printf("ADB endpoint replied with %s, not an expected handshake response.\n",
                   adbc_command_name(client.rx_command));
            return 1;
        default:
            adbc_log_error("unexpected client state");
            return 1;
    }
    return 0;
}
