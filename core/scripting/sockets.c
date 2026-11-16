#include <winsock2.h>
#include <ws2tcpip.h>

#include "sockets.h"

#include "../../lua/include/lauxlib.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOCKET_METATABLE "probe.sockets.handle"
#define SOCKET_DEFAULT_TIMEOUT 1000
#define SOCKET_MAX_TIMEOUT 60000
#define SOCKET_RECV_CHUNK 4096

typedef struct {
    SOCKET handle;
    DWORD timeout_ms;
} LuaSocket;

typedef struct {
    const char *name;
    int level;
    int option;
    int boolean_value;
} SocketOption;

static LuaSocket *CheckSocket(lua_State *state, int index)
{
    return (LuaSocket *)luaL_checkudata(state, index, SOCKET_METATABLE);
}

static int PushSocketError(lua_State *state, const char *operation, int error)
{
    lua_pushnil(state);
    lua_pushfstring(state, "%s failed (Winsock error %d)", operation, error);
    return 2;
}

static int IsOpen(lua_State *state, LuaSocket *socket)
{
    if (socket->handle != INVALID_SOCKET)
        return 1;
    luaL_error(state, "socket is closed");
    return 0;
}

static int ApplyTimeout(LuaSocket *socket, DWORD timeout_ms)
{
    return setsockopt(socket->handle, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout_ms, sizeof(timeout_ms)) != SOCKET_ERROR &&
        setsockopt(socket->handle, SOL_SOCKET, SO_SNDTIMEO,
               (const char *)&timeout_ms, sizeof(timeout_ms)) != SOCKET_ERROR;
}

static int LuaSocketsNew(lua_State *state)
{
    SOCKET handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == INVALID_SOCKET)
        return PushSocketError(state, "socket", WSAGetLastError());

    LuaSocket *socket = lua_newuserdata(state, sizeof(*socket));
    socket->handle = handle;
    socket->timeout_ms = SOCKET_DEFAULT_TIMEOUT;
    luaL_getmetatable(state, SOCKET_METATABLE);
    lua_setmetatable(state, -2);

    if (!ApplyTimeout(socket, socket->timeout_ms)) {
        int error = WSAGetLastError();
        closesocket(handle);
        socket->handle = INVALID_SOCKET;
        lua_pop(state, 1);
        return PushSocketError(state, "setsockopt", error);
    }
    return 1;
}

static int LuaSocketsConnect(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);

    size_t ip_length = 0;
    const char *ip = luaL_checklstring(state, 2, &ip_length);
    if (ip_length == 0 || memchr(ip, '\0', ip_length) != NULL)
        return luaL_argerror(state, 2, "expected an IPv4 address");

    lua_Integer port_value = luaL_checkinteger(state, 3);
    if (port_value < 1 || port_value > UINT16_MAX)
        return luaL_argerror(state, 3, "port must be between 1 and 65535");

    char address_text[INET_ADDRSTRLEN];
    if (ip_length >= sizeof(address_text))
        return luaL_argerror(state, 2, "expected an IPv4 address");
    memcpy(address_text, ip, ip_length);
    address_text[ip_length] = '\0';

    struct sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    address.sin_port = htons((u_short)port_value);
    if (InetPtonA(AF_INET, address_text, &address.sin_addr) != 1)
        return luaL_argerror(state, 2, "expected a valid IPv4 address");

    u_long nonblocking = 1;
    if (ioctlsocket(socket->handle, FIONBIO, &nonblocking) == SOCKET_ERROR)
        return PushSocketError(state, "ioctlsocket", WSAGetLastError());

    int connected = connect(socket->handle, (struct sockaddr *)&address,
        sizeof(address));
    int error = connected == SOCKET_ERROR ? WSAGetLastError() : 0;
    if (connected == SOCKET_ERROR && (error == WSAEWOULDBLOCK ||
            error == WSAEINPROGRESS || error == WSAEALREADY)) {
        fd_set writable;
        fd_set exceptional;
        FD_ZERO(&writable);
        FD_ZERO(&exceptional);
        FD_SET(socket->handle, &writable);
        FD_SET(socket->handle, &exceptional);
        TIMEVAL timeout = {
            (long)(socket->timeout_ms / 1000),
            (long)((socket->timeout_ms % 1000) * 1000)
        };
        int ready = select(0, NULL, &writable, &exceptional, &timeout);
        if (ready == 0)
            error = WSAETIMEDOUT;
        else if (ready == SOCKET_ERROR)
            error = WSAGetLastError();
        else {
            int socket_error = 0;
            int error_length = sizeof(socket_error);
            if (getsockopt(socket->handle, SOL_SOCKET, SO_ERROR,
                    (char *)&socket_error, &error_length) == SOCKET_ERROR)
                error = WSAGetLastError();
            else
                error = socket_error;
        }
    }

    nonblocking = 0;
    if (ioctlsocket(socket->handle, FIONBIO, &nonblocking) == SOCKET_ERROR &&
        error == 0)
        error = WSAGetLastError();
    if (error != 0)
        return PushSocketError(state, "connect", error);

    lua_pushboolean(state, 1);
    return 1;
}

static int CloseSocket(LuaSocket *socket)
{
    if (socket->handle == INVALID_SOCKET)
        return 0;
    SOCKET handle = socket->handle;
    socket->handle = INVALID_SOCKET;
    return closesocket(handle);
}

static int LuaSocketsClose(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    if (CloseSocket(socket) == SOCKET_ERROR)
        return PushSocketError(state, "closesocket", WSAGetLastError());
    lua_pushboolean(state, 1);
    return 1;
}

static int LuaSocketsGc(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    CloseSocket(socket);
    return 0;
}

static int LuaSocketsSetTimeout(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);
    lua_Integer timeout_value = luaL_checkinteger(state, 2);
    if (timeout_value < 1 || timeout_value > SOCKET_MAX_TIMEOUT)
        return luaL_argerror(state, 2, "timeout must be between 1 and 60000 ms");

    DWORD timeout_ms = (DWORD)timeout_value;
    if (!ApplyTimeout(socket, timeout_ms))
        return PushSocketError(state, "setsockopt", WSAGetLastError());
    socket->timeout_ms = timeout_ms;
    lua_pushboolean(state, 1);
    return 1;
}

static int LuaSocketsSetOption(lua_State *state)
{
    static const SocketOption options[] = {
        { "keepalive", SOL_SOCKET, SO_KEEPALIVE, 1 },
        { "reuseaddr", SOL_SOCKET, SO_REUSEADDR, 1 },
        { "recvbuf", SOL_SOCKET, SO_RCVBUF, 0 },
        { "sendbuf", SOL_SOCKET, SO_SNDBUF, 0 },
        { "nodelay", IPPROTO_TCP, TCP_NODELAY, 1 }
    };

    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);
    const char *name = luaL_checkstring(state, 2);
    const SocketOption *selected = NULL;
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (strcmp(name, options[i].name) == 0) {
            selected = &options[i];
            break;
        }
    }
    if (selected == NULL)
        return luaL_argerror(state, 2, "unknown socket option");

    int option_value;
    if (selected->boolean_value && lua_isboolean(state, 3)) {
        option_value = lua_toboolean(state, 3) ? 1 : 0;
    } else {
        lua_Integer value = luaL_checkinteger(state, 3);
        if (value < INT_MIN || value > INT_MAX)
            return luaL_argerror(state, 3, "option value must fit an int");
        option_value = (int)value;
    }

    if (setsockopt(socket->handle, selected->level, selected->option,
            (const char *)&option_value, sizeof(option_value)) == SOCKET_ERROR)
        return PushSocketError(state, "setsockopt", WSAGetLastError());
    lua_pushboolean(state, 1);
    return 1;
}

static int LuaSocketsRecv(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);
    lua_Integer size_value = luaL_checkinteger(state, 2);
    if (size_value < 1 || size_value > INT_MAX)
        return luaL_argerror(state, 2, "size must be between 1 and INT_MAX");

    size_t capacity = (size_t)size_value;
    char *buffer = malloc(capacity);
    if (buffer == NULL)
        return luaL_error(state, "out of memory allocating receive buffer");

    int received = recv(socket->handle, buffer, (int)capacity, 0);
    if (received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        free(buffer);
        return PushSocketError(state, "recv", error);
    }
    lua_pushlstring(state, buffer, (size_t)received);
    free(buffer);
    return 1;
}

static int LuaSocketsRecvAll(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);
    size_t length = 0;
    size_t capacity = SOCKET_RECV_CHUNK;
    char *buffer = malloc(capacity);
    if (buffer == NULL)
        return luaL_error(state, "out of memory allocating receive buffer");

    for (;;) {
        if (capacity - length < SOCKET_RECV_CHUNK) {
            if (capacity > SIZE_MAX / 2) {
                free(buffer);
                return luaL_error(state, "received data is too large");
            }
            size_t new_capacity = capacity * 2;
            char *new_buffer = realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                free(buffer);
                return luaL_error(state, "out of memory growing receive buffer");
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }

        int received = recv(socket->handle, buffer + length,
            SOCKET_RECV_CHUNK, 0);
        if (received == 0)
            break;
        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            free(buffer);
            return PushSocketError(state, "recv", error);
        }
        length += (size_t)received;
    }

    lua_pushlstring(state, buffer, length);
    free(buffer);
    return 1;
}

static int LuaSocketsSend(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);
    size_t length = 0;
    const char *bytes = luaL_checklstring(state, 2, &length);
    size_t sent = 0;
    while (sent < length) {
        int chunk = (int)((length - sent) > INT_MAX ? INT_MAX : length - sent);
        int result = send(socket->handle, bytes + sent, chunk, 0);
        if (result == SOCKET_ERROR)
            return PushSocketError(state, "send", WSAGetLastError());
        if (result == 0)
            return PushSocketError(state, "send", WSAECONNRESET);
        sent += (size_t)result;
    }
    lua_pushinteger(state, (lua_Integer)sent);
    return 1;
}

static int LuaSocketsPeerInfo(lua_State *state)
{
    LuaSocket *socket = CheckSocket(state, 1);
    IsOpen(state, socket);
    struct sockaddr_in peer = { 0 };
    int peer_length = sizeof(peer);
    if (getpeername(socket->handle, (struct sockaddr *)&peer,
            &peer_length) == SOCKET_ERROR)
        return PushSocketError(state, "getpeername", WSAGetLastError());

    char ip[INET_ADDRSTRLEN];
    if (InetNtopA(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == NULL)
        return PushSocketError(state, "InetNtopA", WSAGetLastError());

    lua_pushinteger(state, ntohs(peer.sin_port));
    lua_pushstring(state, ip);
    return 2;
}

static void SetFunction(lua_State *state, const char *name,
    lua_CFunction function)
{
    lua_pushcfunction(state, function);
    lua_setfield(state, -2, name);
}

static void SetString(lua_State *state, const char *name, const char *value)
{
    lua_pushstring(state, value);
    lua_setfield(state, -2, name);
}

int luaopen_sockets(lua_State *state)
{
    if (luaL_newmetatable(state, SOCKET_METATABLE)) {
        lua_pushcfunction(state, LuaSocketsGc);
        lua_setfield(state, -2, "__gc");
    }
    lua_pop(state, 1);

    lua_createtable(state, 0, 13);
    SetFunction(state, "new", LuaSocketsNew);
    SetFunction(state, "connect", LuaSocketsConnect);
    SetFunction(state, "close", LuaSocketsClose);
    SetFunction(state, "recv", LuaSocketsRecv);
    SetFunction(state, "recv_all", LuaSocketsRecvAll);
    SetFunction(state, "send", LuaSocketsSend);
    SetFunction(state, "set_timeout", LuaSocketsSetTimeout);
    SetFunction(state, "set_sock_option", LuaSocketsSetOption);
    SetFunction(state, "peer_info", LuaSocketsPeerInfo);
    SetString(state, "SO_KEEPALIVE", "keepalive");
    SetString(state, "SO_REUSEADDR", "reuseaddr");
    SetString(state, "SO_RCVBUF", "recvbuf");
    SetString(state, "SO_SNDBUF", "sendbuf");
    SetString(state, "TCP_NODELAY", "nodelay");
    return 1;
}

void LuaSockets_Register(lua_State *state)
{
    if (state == NULL)
        return;
    luaL_requiref(state, "sockets", luaopen_sockets, 0);
    lua_pop(state, 1);
}
