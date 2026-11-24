#include "adbc.h"

#include <string.h>

#define ADB_CMD(name) ((uint32_t)(uint8_t)(name)[0] | \
                       ((uint32_t)(uint8_t)(name)[1] << 8) | \
                       ((uint32_t)(uint8_t)(name)[2] << 16) | \
                       ((uint32_t)(uint8_t)(name)[3] << 24))

static uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void finish_packet(AdbcClient *client) {
    client->banner[client->banner_size] = '\0';
    if (client->rx_command == ADB_CMD("AUTH")) {
        client->state = client->rx_arg0 == 1u ? ADBC_AUTH_REQUIRED : ADBC_OTHER_PACKET;
    } else if (client->rx_command == ADB_CMD("CNXN")) {
        client->state = ADBC_ONLINE;
    } else if (client->rx_command == ADB_CMD("STLS")) {
        client->state = ADBC_TLS_REQUIRED;
    } else {
        client->state = ADBC_OTHER_PACKET;
    }
}

void adbc_client_init(AdbcClient *client) {
    memset(client, 0, sizeof(*client));
    client->state = ADBC_WAITING;
}

size_t adbc_client_start(AdbcClient *client, uint8_t *out, size_t out_cap) {
    static const uint8_t banner[] = "host::\0";
    const size_t payload_size = sizeof(banner) - 1u;
    const size_t packet_size = 24u + payload_size;
    const uint32_t command = ADB_CMD("CNXN");
    uint32_t checksum = 0;
    size_t i;

    if (client == NULL || out == NULL || client->state != ADBC_WAITING ||
        out_cap < packet_size) {
        return 0;
    }

    for (i = 0; i < payload_size; ++i) {
        checksum += banner[i];
    }
    write_u32le(out + 0, command);
    write_u32le(out + 4, ADBC_VERSION);
    write_u32le(out + 8, ADBC_MAX_DATA);
    write_u32le(out + 12, (uint32_t)payload_size);
    write_u32le(out + 16, checksum);
    write_u32le(out + 20, command ^ 0xffffffffu);
    memcpy(out + 24, banner, payload_size);
    return packet_size;
}

size_t adbc_client_feed(AdbcClient *client, const uint8_t *data, size_t size) {
    size_t consumed = 0;

    if (client == NULL || data == NULL || client->state != ADBC_WAITING) {
        return 0;
    }

    while (consumed < size && client->state == ADBC_WAITING) {
        if (client->rx_header_used < sizeof(client->rx_header)) {
            size_t need = sizeof(client->rx_header) - client->rx_header_used;
            size_t take = size - consumed < need ? size - consumed : need;
            memcpy(client->rx_header + client->rx_header_used, data + consumed, take);
            client->rx_header_used += take;
            consumed += take;
            if (client->rx_header_used < sizeof(client->rx_header)) {
                continue;
            }

            client->rx_command = read_u32le(client->rx_header + 0);
            client->rx_arg0 = read_u32le(client->rx_header + 4);
            client->rx_arg1 = read_u32le(client->rx_header + 8);
            client->rx_payload_size = read_u32le(client->rx_header + 12);
            if (read_u32le(client->rx_header + 20) !=
                (client->rx_command ^ 0xffffffffu) ||
                client->rx_payload_size > ADBC_MAX_DATA) {
                client->state = ADBC_PROTOCOL_ERROR;
                break;
            }
            if (client->rx_payload_size == 0) {
                finish_packet(client);
            }
        } else {
            size_t remaining = (size_t)client->rx_payload_size - client->rx_payload_used;
            size_t take = size - consumed < remaining ? size - consumed : remaining;
            if (client->rx_command == ADB_CMD("CNXN") &&
                client->banner_size < sizeof(client->banner) - 1u) {
                size_t room = sizeof(client->banner) - 1u - client->banner_size;
                size_t copy_size = take < room ? take : room;
                memcpy(client->banner + client->banner_size, data + consumed, copy_size);
                client->banner_size += copy_size;
            }
            client->rx_payload_used += (uint32_t)take;
            consumed += take;
            if (client->rx_payload_used == client->rx_payload_size) {
                finish_packet(client);
            }
        }
    }
    return consumed;
}

const char *adbc_command_name(uint32_t command) {
    static char name[5];
    name[0] = (char)(command & 0xffu);
    name[1] = (char)((command >> 8) & 0xffu);
    name[2] = (char)((command >> 16) & 0xffu);
    name[3] = (char)((command >> 24) & 0xffu);
    name[4] = '\0';
    return name;
}
