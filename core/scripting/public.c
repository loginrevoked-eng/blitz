#include "public.h"

#include "../cleanup.h"
#include "lua_utils.h"
#include "rdp.h"
#include "rtsp.h"
#include "ssh.h"
#include "telnet.h"

#include "../../lua/include/lauxlib.h"
#include "../../lua/include/lualib.h"

static int HostupCheckToLuaTable(lua_State *lua, const HostUpCheck *check)
{
    if (lua == NULL || check == NULL)
        return 0;

    Ipv4 address;
    if (check->options.method & HOST_CHECK_METHOD_ICMP)
        address = check->options.icmp_options.probe.addr;
    else if (check->options.method & HOST_CHECK_METHOD_TCP)
        address = check->options.tcp_options.probe.addr;
    else if (check->options.method & HOST_CHECK_METHOD_UDP)
        address = check->options.udp_options.probe.addr;
    else
        return 0;

    char *ip = Ipv4Repr(address);
    if (ip == NULL)
        return 0;

    lua_createtable(lua, 0, 5);
    lua_pushstring(lua, ip);
    lua_setfield(lua, -2, "ip");
    ProbeCleanupString(ip);

    lua_pushinteger(lua, (lua_Integer)check->result.status);
    lua_setfield(lua, -2, "status");
    lua_pushinteger(lua, (lua_Integer)check->options.method);
    lua_setfield(lua, -2, "method");
    DWORD timeout_ms = (check->options.method & HOST_CHECK_METHOD_TCP) ?
        check->options.tcp_options.probe.timeout_ms :
        (check->options.method & HOST_CHECK_METHOD_UDP) ?
        check->options.udp_options.probe.timeout_ms :
        check->options.icmp_options.probe.timeout_ms;
    lua_pushinteger(lua, (lua_Integer)timeout_ms);
    lua_setfield(lua, -2, "timeout_ms");

    if (check->result.iresult != NULL) {
        lua_pushinteger(lua, (lua_Integer)*check->result.iresult);
        lua_setfield(lua, -2, "icmp");
    }

    if (check->result.tresult != NULL) {
        lua_createtable(lua, (int)check->result.tresult->len, 0);
        for (size_t i = 0; i < check->result.tresult->len; ++i) {
            const TcpResultWrapper *item = &check->result.tresult->results[i];
            lua_createtable(lua, 0, 3);
            lua_pushinteger(lua, item->port);
            lua_setfield(lua, -2, "port");
            lua_pushinteger(lua, (lua_Integer)item->result);
            lua_setfield(lua, -2, "result");
            lua_pushinteger(lua, item->retries);
            lua_setfield(lua, -2, "retries");
            lua_rawseti(lua, -2, (lua_Integer)i + 1);
        }
        lua_setfield(lua, -2, "tcp");
    }

    if (check->result.uresult != NULL) {
        lua_createtable(lua, (int)check->result.uresult->len, 0);
        for (size_t i = 0; i < check->result.uresult->len; ++i) {
            const UdpResultWrapper *item = &check->result.uresult->results[i];
            lua_createtable(lua, 0, 3);
            lua_pushinteger(lua, item->port);
            lua_setfield(lua, -2, "port");
            lua_pushinteger(lua, (lua_Integer)item->result);
            lua_setfield(lua, -2, "result");
            lua_pushinteger(lua, item->retries);
            lua_setfield(lua, -2, "retries");
            lua_rawseti(lua, -2, (lua_Integer)i + 1);
        }
        lua_setfield(lua, -2, "udp");
    }

    return 1;
}

int LuaInit(lua_State *lua, ProbeConfig *config)
{
    if (lua == NULL || config == NULL)
        return 0;

    (void)config;
    luaL_openlibs(lua);

    //register our stdlib
    LuaUtils_RegisterPrint(lua);
    LuaSSH_Register(lua);
    LuaTelnet_Register(lua);
    LuaRTSP_Register(lua);
    LuaRDP_Register(lua);
    return 1;
}

int LuaInvoke(lua_State *lua, HostUpCheck *check)
{
    if (lua == NULL || check == NULL)
        return 0;

    if (!HostupCheckToLuaTable(lua, check))
        return 0;

    if (lua_getglobal(lua, "main") != LUA_TFUNCTION) {
        lua_pop(lua, 1);
        lua_pop(lua, 1);
        return 0;
    }
    lua_insert(lua, -2);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK) {
        lua_pop(lua, 1);
        return 0;
    }

    int confirmed = lua_toboolean(lua, -1);
    lua_pop(lua, 1);
    return confirmed;
}
