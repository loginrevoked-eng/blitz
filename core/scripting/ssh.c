#include "ssh.h"

#include "../../lua/include/lauxlib.h"

#include <stddef.h>
#include <string.h>

static int IsProtocolVersion(const char *text, size_t length)
{
    size_t separator = length;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == '.') {
            if (separator != length)
                return 0;
            separator = i;
        } else if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
    }
    return separator > 0 && separator + 1 < length;
}

static int IsSoftwareVersion(const char *text, size_t length)
{
    if (length == 0)
        return 0;

    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 33 || ch > 126 || ch == '-')
            return 0;
    }
    return 1;
}

int LuaParseSSHIdentification(lua_State *state)
{
    size_t length = 0;
    luaL_checktype(state, 1, LUA_TSTRING);
    const char *identification = luaL_checklstring(state, 1, &length);
    if (length > 255)
        return luaL_error(state, "SSH identification exceeds 255 bytes");
    if (length < 10 || identification[length - 2] != '\r' ||
        identification[length - 1] != '\n')
        return luaL_error(state,
            "expected an SSH identification line terminated by CRLF");

    size_t body_length = length - 2;
    for (size_t i = 0; i < body_length; ++i) {
        if (identification[i] == '\0' || identification[i] == '\r' ||
            identification[i] == '\n')
            return luaL_error(state, "SSH identification contains an invalid control byte");
    }

    if (memcmp(identification, "SSH-", 4) != 0)
        return luaL_error(state, "SSH identification must begin with 'SSH-'");

    const char *protocol = identification + 4;
    size_t remaining = body_length - 4;
    const char *software_separator = memchr(protocol, '-', remaining);
    if (software_separator == NULL)
        return luaL_error(state, "SSH identification is missing its software version");

    size_t protocol_length = (size_t)(software_separator - protocol);
    if (!IsProtocolVersion(protocol, protocol_length))
        return luaL_error(state, "invalid SSH protocol version");

    const char *software = software_separator + 1;
    const char *body_end = identification + body_length;
    const char *comments_separator = memchr(software, ' ',
        (size_t)(body_end - software));
    const char *software_end = comments_separator != NULL ?
        comments_separator : body_end;
    size_t software_length = (size_t)(software_end - software);
    if (!IsSoftwareVersion(software, software_length))
        return luaL_error(state, "invalid SSH software version");

    const char *comments = NULL;
    size_t comments_length = 0;
    if (comments_separator != NULL) {
        comments = comments_separator + 1;
        comments_length = (size_t)(body_end - comments);
        if (comments_length == 0)
            return luaL_error(state, "SSH comments cannot be empty after a separator");
    }

    lua_createtable(state, 0, comments != NULL ? 4 : 3);
    lua_pushlstring(state, identification, length);
    lua_setfield(state, -2, "identification");
    lua_pushlstring(state, protocol, protocol_length);
    lua_setfield(state, -2, "protocol_version");
    lua_pushlstring(state, software, software_length);
    lua_setfield(state, -2, "software_version");
    if (comments != NULL) {
        lua_pushlstring(state, comments, comments_length);
        lua_setfield(state, -2, "comments");
    }
    return 1;
}

int luaopen_ssh(lua_State *state)
{
    lua_createtable(state, 0, 1);
    lua_pushcfunction(state, LuaParseSSHIdentification);
    lua_setfield(state, -2, "parse_identification");
    return 1;
}

void LuaSSH_Register(lua_State *state)
{
    if (state == NULL)
        return;

    luaL_requiref(state, "probe.ssh", luaopen_ssh, 0);
    lua_pop(state, 1);
}
