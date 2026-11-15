#include <winsock2.h>
#include <windows.h>

#include "rtsp.h"

#include "../../lua/include/lauxlib.h"
#include "net_io.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#define RTSP_MAX_HEADER_BYTES 8192

static int ParseStatusLine(const char *line, size_t length,
    char version[4], unsigned *status, const char **reason,
    size_t *reason_length)
{
    if (length < 13 || memcmp(line, "RTSP/", 5) != 0 ||
        line[8] != ' ' || line[12] != ' ')
        return 0;
    if ((memcmp(line + 5, "1.0", 3) != 0) &&
        (memcmp(line + 5, "2.0", 3) != 0))
        return 0;
    if (!isdigit((unsigned char)line[9]) ||
        !isdigit((unsigned char)line[10]) ||
        !isdigit((unsigned char)line[11]))
        return 0;

    memcpy(version, line + 5, 3);
    version[3] = '\0';
    *status = (unsigned)(line[9] - '0') * 100u +
        (unsigned)(line[10] - '0') * 10u + (unsigned)(line[11] - '0');
    if (*status < 100 || *status > 699)
        return 0;
    *reason = line + 13;
    *reason_length = length - 13;
    return 1;
}

static int HasMatchingCSeq(const char *headers, size_t length)
{
    const char *end = headers + length;
    const char *line_end = memchr(headers, '\n', length);
    if (line_end == NULL)
        return 0;
    const char *line = line_end + 1;
    int found = 0;

    while (line < end) {
        line_end = memchr(line, '\n', (size_t)(end - line));
        if (line_end == NULL)
            return 0;
        const char *content_end = line_end;
        if (content_end > line && content_end[-1] == '\r')
            --content_end;
        if (content_end == line)
            break;

        const char *colon = memchr(line, ':', (size_t)(content_end - line));
        if (colon != NULL && (size_t)(colon - line) == 4 &&
            _strnicmp(line, "CSeq", 4) == 0) {
            if (found)
                return 0;
            found = 1;
            const char *value = colon + 1;
            while (value < content_end && (*value == ' ' || *value == '\t'))
                ++value;
            if (value == content_end || *value++ != '1')
                return 0;
            while (value < content_end && (*value == ' ' || *value == '\t'))
                ++value;
            if (value != content_end)
                return 0;
        }
        line = line_end + 1;
    }
    return found;
}

int LuaRTSP_Confirm(lua_State *state)
{
    const char *host = NULL;
    uint16_t port = 0;
    lua_Integer timeout_value = 0;
    LuaNet_ReadArguments(state, 554, &host, &port, &timeout_value);
    DWORD timeout_ms = (DWORD)timeout_value;

    char error[160];
    SOCKET socket_handle;
    if (!LuaNet_Connect(host, port, timeout_ms, &socket_handle,
            error, sizeof(error)))
        return LuaNet_PushFailure(state, error);

    static const char request[] =
        "OPTIONS * RTSP/1.0\r\n"
        "CSeq: 1\r\n"
        "User-Agent: core-probe/1.0\r\n"
        "\r\n";
    if (!LuaNet_SendAll(socket_handle, request, sizeof(request) - 1,
            error, sizeof(error))) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, error);
    }

    char response[RTSP_MAX_HEADER_BYTES];
    size_t response_length = 0;
    static const char terminator[] = "\r\n\r\n";
    if (!LuaNet_ReceiveUntil(socket_handle, response, sizeof(response),
            terminator, sizeof(terminator) - 1, &response_length,
            error, sizeof(error))) {
        LuaNet_Close(socket_handle);
        return LuaNet_PushFailure(state, error);
    }
    LuaNet_Close(socket_handle);

    const char *status_end = memchr(response, '\n', response_length);
    if (status_end == NULL || status_end == response || status_end[-1] != '\r')
        return LuaNet_PushFailure(state, "invalid RTSP status line");

    char version[4];
    unsigned status = 0;
    const char *reason = NULL;
    size_t reason_length = 0;
    size_t status_length = (size_t)(status_end - response) - 1;
    if (!ParseStatusLine(response, status_length, version, &status,
            &reason, &reason_length))
        return LuaNet_PushFailure(state, "response is not a valid RTSP status line");
    if (!HasMatchingCSeq(response, response_length))
        return LuaNet_PushFailure(state, "RTSP response has no matching CSeq header");

    lua_createtable(state, 0, 5);
    lua_pushboolean(state, 1);
    lua_setfield(state, -2, "confirmed");
    lua_pushstring(state, "rtsp");
    lua_setfield(state, -2, "protocol");
    lua_pushstring(state, version);
    lua_setfield(state, -2, "version");
    lua_pushinteger(state, (lua_Integer)status);
    lua_setfield(state, -2, "status_code");
    lua_pushlstring(state, reason, reason_length);
    lua_setfield(state, -2, "reason");
    return 1;
}

int luaopen_rtsp(lua_State *state)
{
    lua_createtable(state, 0, 1);
    lua_pushcfunction(state, LuaRTSP_Confirm);
    lua_setfield(state, -2, "confirm");
    return 1;
}

void LuaRTSP_Register(lua_State *state)
{
    if (state == NULL)
        return;
    luaL_requiref(state, "probe.rtsp", luaopen_rtsp, 0);
    lua_pop(state, 1);
}
