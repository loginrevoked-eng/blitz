#include <winsock2.h>
#include <windows.h>

#include "telnet.h"

#include "../../lua/include/lauxlib.h"
#include "net_io.h"

#include <string.h>

static const char *TelnetCommandName(unsigned char command)
{
    switch (command) {
    case 251: return "WILL";
    case 252: return "WONT";
    case 253: return "DO";
    case 254: return "DONT";
    default: return "UNKNOWN";
    }
}

static int FindTelnetNegotiation(const char *response, size_t response_length,
    unsigned char *command_out, unsigned char *option_out)
{
    for (size_t i = 0; i + 2 < response_length; ++i) {
        if ((unsigned char)response[i] != 255)
            continue;

        unsigned char command = (unsigned char)response[i + 1];
        if (command >= 251 && command <= 254) {
            *command_out = command;
            *option_out = (unsigned char)response[i + 2];
            return 1;
        }
        if (command == 255)
            ++i;
    }
    return 0;
}

int LuaTelnet_Confirm(lua_State *state)
{
    const char *host = NULL;
    uint16_t port = 0;
    lua_Integer timeout_value = 0;
    LuaNet_ReadArguments(state, 23, &host, &port, &timeout_value);
    DWORD timeout_ms = (DWORD)timeout_value;

    char error[160];
    SOCKET socket_handle;
    if (!LuaNet_Connect(host, port, timeout_ms, &socket_handle,
            error, sizeof(error)))
        return LuaNet_PushFailure(state, error);

    static const char request[] = {
        (char)255, (char)253, (char)24 /* IAC DO TERMINAL-TYPE */
    };
    if (!LuaNet_SendAll(socket_handle, request, sizeof(request),
            error, sizeof(error))) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, error);
    }

    char response[512];
    size_t response_length = 0;
    unsigned char command = 0;
    unsigned char option = 0;
    while (response_length < sizeof(response)) {
        size_t received = 0;
        if (!LuaNet_ReceiveSome(socket_handle, response + response_length,
                sizeof(response) - response_length, &received,
                error, sizeof(error))) {
            LuaNet_Close(socket_handle);
            return LuaNet_PushFailure(state, error);
        }
        response_length += received;
        if (FindTelnetNegotiation(response, response_length, &command, &option)) {
            LuaNet_Close(socket_handle);
            lua_createtable(state, 0, 4);
            lua_pushboolean(state, 1);
            lua_setfield(state, -2, "confirmed");
            lua_pushstring(state, "telnet");
            lua_setfield(state, -2, "protocol");
            lua_pushstring(state, TelnetCommandName(command));
            lua_setfield(state, -2, "command");
            lua_pushinteger(state, option);
            lua_setfield(state, -2, "option");
            return 1;
        }
    }

    LuaNet_Close(socket_handle);

    return LuaNet_PushFailure(state,
        "no valid Telnet option negotiation received");
}

int luaopen_telnet(lua_State *state)
{
    lua_createtable(state, 0, 1);
    lua_pushcfunction(state, LuaTelnet_Confirm);
    lua_setfield(state, -2, "confirm");
    return 1;
}

void LuaTelnet_Register(lua_State *state)
{
    if (state == NULL)
        return;
    luaL_requiref(state, "probe.telnet", luaopen_telnet, 0);
    lua_pop(state, 1);
}
