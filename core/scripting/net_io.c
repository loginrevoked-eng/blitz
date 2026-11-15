#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include "net_io.h"

#include "../../lua/include/lauxlib.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define LUA_NET_DEFAULT_TIMEOUT_MS 1000
#define LUA_NET_MAX_TIMEOUT_MS 60000

static void SetSocketError(char *error, size_t capacity, const char *operation,
    int code)
{
    if (capacity != 0)
        snprintf(error, capacity, "%s failed (Winsock error %d)", operation, code);
}

int LuaNet_ReadArguments(lua_State *state, unsigned default_port,
    const char **host, uint16_t *port, lua_Integer *timeout_ms)
{
    size_t host_length = 0;
    luaL_checktype(state, 1, LUA_TSTRING);
    *host = luaL_checklstring(state, 1, &host_length);
    if (host_length == 0 || memchr(*host, '\0', host_length) != NULL)
        return luaL_error(state, "host must be a non-empty string without NUL bytes");

    lua_Integer port_value = luaL_optinteger(state, 2, (lua_Integer)default_port);
    if (port_value < 1 || port_value > UINT16_MAX)
        return luaL_error(state, "port must be between 1 and 65535");
    *port = (uint16_t)port_value;

    lua_Integer timeout_value = luaL_optinteger(state, 3,
        LUA_NET_DEFAULT_TIMEOUT_MS);
    if (timeout_value < 1 || timeout_value > LUA_NET_MAX_TIMEOUT_MS)
        return luaL_error(state, "timeout_ms must be between 1 and 60000");
    *timeout_ms = timeout_value;
    return 1;
}

int LuaNet_Connect(const char *host, uint16_t port, DWORD timeout_ms,
    SOCKET *socket_out, char *error, size_t error_capacity)
{
    *socket_out = INVALID_SOCKET;
    char service[6];
    snprintf(service, sizeof(service), "%u", (unsigned)port);

    struct addrinfo hints = { 0 };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addresses = NULL;
    int lookup = getaddrinfo(host, service, &hints, &addresses);
    if (lookup != 0) {
        if (error_capacity != 0)
            snprintf(error, error_capacity, "address lookup failed (%d)", lookup);
        return 0;
    }

    int last_error = WSAETIMEDOUT;
    for (struct addrinfo *address = addresses; address != NULL;
         address = address->ai_next) {
        SOCKET socket_handle = socket(address->ai_family,
            address->ai_socktype, address->ai_protocol);
        if (socket_handle == INVALID_SOCKET) {
            last_error = WSAGetLastError();
            continue;
        }

        u_long nonblocking = 1;
        if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            last_error = WSAGetLastError();
            closesocket(socket_handle);
            continue;
        }

        int connected = connect(socket_handle, address->ai_addr,
            (int)address->ai_addrlen);
        if (connected == SOCKET_ERROR) {
            int connect_error = WSAGetLastError();
            if (connect_error != WSAEWOULDBLOCK &&
                connect_error != WSAEINPROGRESS &&
                connect_error != WSAEALREADY) {
                last_error = connect_error;
                closesocket(socket_handle);
                continue;
            }

            fd_set writable;
            fd_set exceptional;
            FD_ZERO(&writable);
            FD_ZERO(&exceptional);
            FD_SET(socket_handle, &writable);
            FD_SET(socket_handle, &exceptional);
            TIMEVAL timeout = {
                (long)(timeout_ms / 1000),
                (long)((timeout_ms % 1000) * 1000)
            };
            int ready = select(0, NULL, &writable, &exceptional, &timeout);
            if (ready <= 0) {
                last_error = ready == 0 ? WSAETIMEDOUT : WSAGetLastError();
                closesocket(socket_handle);
                continue;
            }

            int socket_error = 0;
            int error_length = sizeof(socket_error);
            if (getsockopt(socket_handle, SOL_SOCKET, SO_ERROR,
                    (char *)&socket_error, &error_length) == SOCKET_ERROR) {
                last_error = WSAGetLastError();
                closesocket(socket_handle);
                continue;
            }
            if (socket_error != 0) {
                last_error = socket_error;
                closesocket(socket_handle);
                continue;
            }
        }

        nonblocking = 0;
        if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            last_error = WSAGetLastError();
            closesocket(socket_handle);
            continue;
        }

        DWORD socket_timeout = timeout_ms;
        if (setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO,
                (const char *)&socket_timeout, sizeof(socket_timeout)) == SOCKET_ERROR ||
            setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO,
                (const char *)&socket_timeout, sizeof(socket_timeout)) == SOCKET_ERROR) {
            last_error = WSAGetLastError();
            closesocket(socket_handle);
            continue;
        }

        *socket_out = socket_handle;
        freeaddrinfo(addresses);
        return 1;
    }

    freeaddrinfo(addresses);
    SetSocketError(error, error_capacity, "TCP connect", last_error);
    return 0;
}

int LuaNet_SendAll(SOCKET socket_handle, const char *data, size_t length,
    char *error, size_t error_capacity)
{
    size_t sent = 0;
    while (sent < length) {
        int chunk = (int)((length - sent) > INT_MAX ? INT_MAX : length - sent);
        int result = send(socket_handle, data + sent, chunk, 0);
        if (result == SOCKET_ERROR) {
            SetSocketError(error, error_capacity, "TCP send", WSAGetLastError());
            return 0;
        }
        if (result == 0) {
            if (error_capacity != 0)
                snprintf(error, error_capacity, "connection closed during send");
            return 0;
        }
        sent += (size_t)result;
    }
    return 1;
}

int LuaNet_ReceiveExact(SOCKET socket_handle, char *data, size_t length,
    char *error, size_t error_capacity)
{
    size_t received = 0;
    while (received < length) {
        int chunk = (int)((length - received) > INT_MAX ? INT_MAX : length - received);
        int result = recv(socket_handle, data + received, chunk, 0);
        if (result == SOCKET_ERROR) {
            SetSocketError(error, error_capacity, "TCP receive", WSAGetLastError());
            return 0;
        }
        if (result == 0) {
            if (error_capacity != 0)
                snprintf(error, error_capacity, "connection closed before the response was complete");
            return 0;
        }
        received += (size_t)result;
    }
    return 1;
}

int LuaNet_ReceiveSome(SOCKET socket_handle, char *data, size_t capacity,
    size_t *received, char *error, size_t error_capacity)
{
    int chunk = (int)(capacity > INT_MAX ? INT_MAX : capacity);
    int result = recv(socket_handle, data, chunk, 0);
    if (result == SOCKET_ERROR) {
        SetSocketError(error, error_capacity, "TCP receive", WSAGetLastError());
        return 0;
    }
    if (result == 0) {
        if (error_capacity != 0)
            snprintf(error, error_capacity, "connection closed without a response");
        return 0;
    }
    *received = (size_t)result;
    return 1;
}

int LuaNet_ReceiveUntil(SOCKET socket_handle, char *data, size_t capacity,
    const char *terminator, size_t terminator_length, size_t *received,
    char *error, size_t error_capacity)
{
    size_t used = 0;
    while (used < capacity) {
        int result = recv(socket_handle, data + used, 1, 0);
        if (result == SOCKET_ERROR) {
            SetSocketError(error, error_capacity, "TCP receive", WSAGetLastError());
            return 0;
        }
        if (result == 0) {
            if (error_capacity != 0)
                snprintf(error, error_capacity, "connection closed before the response was complete");
            return 0;
        }
        ++used;
        if (used >= terminator_length &&
            memcmp(data + used - terminator_length, terminator,
                terminator_length) == 0) {
            *received = used;
            return 1;
        }
    }

    if (error_capacity != 0)
        snprintf(error, error_capacity, "response exceeded the %zu-byte limit", capacity);
    return 0;
}

void LuaNet_Close(SOCKET socket_handle)
{
    if (socket_handle != INVALID_SOCKET)
        closesocket(socket_handle);
}

int LuaNet_PushFailure(lua_State *state, const char *reason)
{
    lua_pushboolean(state, 0);
    lua_pushstring(state, reason != NULL ? reason : "protocol handshake failed");
    return 2;
}
