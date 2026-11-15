#ifndef CORE_SCRIPTING_NET_IO_H
#define CORE_SCRIPTING_NET_IO_H

#include <winsock2.h>
#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include "../../lua/include/lua.h"

int LuaNet_ReadArguments(lua_State *state, unsigned default_port,
    const char **host, uint16_t *port, lua_Integer *timeout_ms);
int LuaNet_Connect(const char *host, uint16_t port, DWORD timeout_ms,
    SOCKET *socket_out, char *error, size_t error_capacity);
int LuaNet_SendAll(SOCKET socket_handle, const char *data, size_t length,
    char *error, size_t error_capacity);
int LuaNet_ReceiveExact(SOCKET socket_handle, char *data, size_t length,
    char *error, size_t error_capacity);
int LuaNet_ReceiveSome(SOCKET socket_handle, char *data, size_t capacity,
    size_t *received, char *error, size_t error_capacity);
int LuaNet_ReceiveUntil(SOCKET socket_handle, char *data, size_t capacity,
    const char *terminator, size_t terminator_length, size_t *received,
    char *error, size_t error_capacity);
void LuaNet_Close(SOCKET socket_handle);
int LuaNet_PushFailure(lua_State *state, const char *reason);

#endif
