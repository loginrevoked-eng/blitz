#include <winsock2.h>
#include <windows.h>

#include "rdp.h"

#include "../../lua/include/lauxlib.h"
#include "net_io.h"

#include <stdint.h>

#define RDP_MAX_CONFIRM_BYTES 4096
#define RDP_NEG_RSP 0x02
#define RDP_NEG_FAILURE 0x03
#define RDP_REQUESTED_PROTOCOLS 0x0000000B

static uint32_t ReadU32LE(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static const char *RdpProtocolName(uint32_t protocol)
{
    switch (protocol) {
    case 0x00000000: return "RDP";
    case 0x00000001: return "SSL";
    case 0x00000002: return "HYBRID";
    case 0x00000004: return "RDSTLS";
    case 0x00000008: return "HYBRID_EX";
    case 0x00000010: return "RDSAAD";
    default: return "unknown";
    }
}

static int WasProtocolRequested(uint32_t protocol)
{
    switch (protocol) {
    case 0x00000001: /* PROTOCOL_SSL */
    case 0x00000002: /* PROTOCOL_HYBRID */
    case 0x00000008: /* PROTOCOL_HYBRID_EX */
        return 1;
    default:
        return 0;
    }
}

static void SetBooleanField(lua_State *state, const char *name, int value)
{
    lua_pushboolean(state, value);
    lua_setfield(state, -2, name);
}

static void SetProtocolFields(lua_State *state, uint32_t protocol)
{
    lua_pushinteger(state, (lua_Integer)protocol);
    lua_setfield(state, -2, "selected_protocol");
    lua_pushstring(state, RdpProtocolName(protocol));
    lua_setfield(state, -2, "selected_protocol_name");
}

int LuaRDP_Confirm(lua_State *state)
{
    const char *host = NULL;
    uint16_t port = 0;
    lua_Integer timeout_value = 0;
    LuaNet_ReadArguments(state, 3389, &host, &port, &timeout_value);
    DWORD timeout_ms = (DWORD)timeout_value;

    char error[160];
    SOCKET socket_handle;
    if (!LuaNet_Connect(host, port, timeout_ms, &socket_handle,
            error, sizeof(error)))
        return LuaNet_PushFailure(state, error);

    static const unsigned char request[] = {
        0x03, 0x00, 0x00, 0x13,             /* TPKT: version, reserved, length */
        0x0e, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, /* X.224 Connection Request */
        0x01, 0x00, 0x08, 0x00,             /* RDP_NEG_REQ header */
        0x0b, 0x00, 0x00, 0x00              /* SSL | HYBRID | HYBRID_EX */
    };
    if (!LuaNet_SendAll(socket_handle, (const char *)request,
            sizeof(request), error, sizeof(error))) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, error);
    }

    unsigned char response[RDP_MAX_CONFIRM_BYTES];
    if (!LuaNet_ReceiveExact(socket_handle, (char *)response, 4,
            error, sizeof(error))) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, error);
    }
    size_t packet_length = ((size_t)response[2] << 8) | response[3];
    if (packet_length < 11 || packet_length > sizeof(response)) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, "invalid RDP TPKT packet length");
    }
    if (!LuaNet_ReceiveExact(socket_handle, (char *)response + 4,
            packet_length - 4, error, sizeof(error))) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, error);
    }
    LuaNet_Close(socket_handle);

    if (response[0] != 0x03 || response[1] != 0x00 ||
        response[4] != packet_length - 5 || response[5] != 0xd0)
        return LuaNet_PushFailure(state,
            "response is not a valid X.224 Connection Confirm");

    size_t negotiation_length = packet_length - 11;
    if (negotiation_length == 0) {
        lua_createtable(state, 0, 4);
        SetBooleanField(state, "confirmed", 1);
        SetBooleanField(state, "negotiated", 0);
        SetBooleanField(state, "legacy", 1);
        lua_pushstring(state, "rdp");
        lua_setfield(state, -2, "protocol");
        SetProtocolFields(state, 0);
        return 1;
    }
    if (negotiation_length != 8)
        return LuaNet_PushFailure(state,
            "invalid RDP negotiation structure length");

    const unsigned char *negotiation = response + 11;
    unsigned structure_length = (unsigned)negotiation[2] |
        ((unsigned)negotiation[3] << 8);
    if (structure_length != 8)
        return LuaNet_PushFailure(state,
            "invalid RDP negotiation structure length field");

    if (negotiation[0] == RDP_NEG_RSP) {
        uint32_t selected_protocol = ReadU32LE(negotiation + 4);
        int selection_offered = WasProtocolRequested(selected_protocol);
        lua_createtable(state, 0, 6);
        SetBooleanField(state, "confirmed", 1);
        SetBooleanField(state, "negotiated", selection_offered);
        SetBooleanField(state, "legacy", 0);
        lua_pushstring(state, "rdp");
        lua_setfield(state, -2, "protocol");
        SetProtocolFields(state, selected_protocol);
        SetBooleanField(state, "selection_offered", selection_offered);
        lua_pushinteger(state, negotiation[1]);
        lua_setfield(state, -2, "response_flags");
        return 1;
    }

    if (negotiation[0] == RDP_NEG_FAILURE) {
        uint32_t failure_code = ReadU32LE(negotiation + 4);
        lua_createtable(state, 0, 4);
        SetBooleanField(state, "confirmed", 1);
        SetBooleanField(state, "negotiated", 0);
        lua_pushstring(state, "rdp");
        lua_setfield(state, -2, "protocol");
        lua_pushinteger(state, (lua_Integer)failure_code);
        lua_setfield(state, -2, "failure_code");
        lua_pushinteger(state, negotiation[1]);
        lua_setfield(state, -2, "response_flags");
        return 1;
    }

    return LuaNet_PushFailure(state,
        "unknown RDP negotiation response type");
}

int luaopen_rdp(lua_State *state)
{
    lua_createtable(state, 0, 1);
    lua_pushcfunction(state, LuaRDP_Confirm);
    lua_setfield(state, -2, "confirm");
    return 1;
}

void LuaRDP_Register(lua_State *state)
{
    if (state == NULL)
        return;
    luaL_requiref(state, "probe.rdp", luaopen_rdp, 0);
    lua_pop(state, 1);
}
