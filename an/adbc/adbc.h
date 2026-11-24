#ifndef ADBC_H
#define ADBC_H

#include <stddef.h>
#include <stdint.h>

#define ADBC_VERSION 0x01000000u
#define ADBC_MAX_DATA (256u * 1024u)
#define ADBC_BANNER_CAP 8192u

typedef enum {
    ADBC_WAITING = 0,
    ADBC_AUTH_REQUIRED,
    ADBC_ONLINE,
    ADBC_TLS_REQUIRED,
    ADBC_OTHER_PACKET,
    ADBC_PROTOCOL_ERROR
} AdbcState;

typedef struct {
    AdbcState state;
    uint8_t rx_header[24];
    size_t rx_header_used;
    uint32_t rx_command;
    uint32_t rx_arg0;
    uint32_t rx_arg1;
    uint32_t rx_payload_size;
    uint32_t rx_payload_used;
    char banner[ADBC_BANNER_CAP];
    size_t banner_size;
} AdbcClient;

/* Sans-I/O protocol client: encode output and feed received bytes into state. */
void adbc_client_init(AdbcClient *client);
size_t adbc_client_start(AdbcClient *client, uint8_t *out, size_t out_cap);
size_t adbc_client_feed(AdbcClient *client, const uint8_t *data, size_t size);
const char *adbc_command_name(uint32_t command);

/* TCP adapter. Returns 0 when an ADB response was received, -1 on failure. */
int adbc_socket_probe(const char *host, const char *port, unsigned timeout_ms,
                      AdbcClient *client);

void adbc_log_error(const char *format, ...);
void adbc_log_info(const char *format, ...);

#endif
