#include "worker.h"

#include "probe.h"
#include "cleanup.h"
#include "icmp.h"
#include "scripting/public.h"
#include "tcp.h"
#include "udp.h"
#include "../lua/include/lauxlib.h"
#include "../logging/logging.h"

#include <stdatomic.h>
#include <stdlib.h>

static void LogScriptLoadFailure(const char *path, const char *reason)
{
    static atomic_flag logged = ATOMIC_FLAG_INIT;
    if (!atomic_flag_test_and_set(&logged))
        Error("failed to load Lua script '%s': %s", path,
            reason != NULL ? reason : "unknown Lua error");
}

static int LoadScript(lua_State *lua, ProbeConfig *config)
{
    if (lua == NULL || config == NULL)
        return 0;

    const char *path = config->script != NULL ? config->script : "main.lua";
    if (luaL_loadfile(lua, path) != LUA_OK) {
        LogScriptLoadFailure(path, lua_tostring(lua, -1));
        lua_pop(lua, 1);
        return 0;
    }
    if (lua_pcall(lua, 0, 1, 0) != LUA_OK) {
        LogScriptLoadFailure(path, lua_tostring(lua, -1));
        lua_pop(lua, 1);
        return 0;
    }

    if (lua_istable(lua, -1)) {
        lua_getfield(lua, -1, "main");
        if (lua_isfunction(lua, -1))
            lua_setglobal(lua, "main");
        else
            lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    return 1;
}

int CheckHost(HostUpCheck *check, HostUpCheckMethod method, Ipv4 ip)
{
    if (check == NULL)
        return 0;

    HostupCheckOptions *options = &check->options;
    HostUpCheckResult *result = &check->result;
    options->method = method;
    if (method & HOST_CHECK_METHOD_ICMP)
        options->icmp_options.probe.addr = ip;
    if (method & HOST_CHECK_METHOD_TCP)
        options->tcp_options.probe.addr = ip;
    if (method & HOST_CHECK_METHOD_UDP)
        options->udp_options.probe.addr = ip;

    int responsive = 0;
    int encountered_error = 0;
    result->status = HOST_CHECK_STATUS_PENDING;

    if (options->method & HOST_CHECK_METHOD_ICMP)
    {
        if (result->iresult == NULL)
            result->iresult = malloc(sizeof(*result->iresult));
        if (result->iresult == NULL) {
            result->status = HOST_CHECK_STATUS_ERROR;
            return 0;
        }

        *result->iresult = ProbeIcmp(&options->icmp_options.probe);
        responsive |= *result->iresult == ICMP_RESULT_ECHO_REPLY;
        encountered_error |= *result->iresult == ICMP_RESULT_ERROR;
    }

    if (options->method & HOST_CHECK_METHOD_TCP)
    {
        NonIcmpOptions *tcp = &options->tcp_options;
        if (result->tresult == NULL)
        {
            result->tresult = calloc(1, sizeof(*result->tresult));
            if (result->tresult != NULL && tcp->ports.len != 0)
            {
                result->tresult->results = calloc(tcp->ports.len, sizeof(*result->tresult->results));
            }

            if (result->tresult != NULL && tcp->ports.len != 0 && result->tresult->results == NULL)
            {
                ProbeCleanupTcpResultList(result->tresult);
                result->tresult = NULL;
            }
            if (result->tresult == NULL)
            {
                result->status = HOST_CHECK_STATUS_ERROR;
                return 0;
            }
            result->tresult->len = tcp->ports.len;
        }

        for (size_t i = 0; i < tcp->ports.len; ++i)
        {
            uint16_t port = tcp->ports.items[i];
            TcpResult probe_result = ProbeTcp(&tcp->probe, port);
            result->tresult->results[i] = (TcpResultWrapper){ port, probe_result, 0 };
            responsive |= probe_result == TCP_RESULT_SYN_ACK_RECEIVED || probe_result == TCP_RESULT_RESET_RECEIVED;
            encountered_error |= probe_result == TCP_RESULT_ERROR;
        }
    }

    if (options->method & HOST_CHECK_METHOD_UDP)
    {
        NonIcmpOptions *udp = &options->udp_options;
        if (result->uresult == NULL)
        {
            result->uresult = calloc(1, sizeof(*result->uresult));
            if (result->uresult != NULL && udp->ports.len != 0)
                result->uresult->results = calloc(udp->ports.len, sizeof(*result->uresult->results));
            if (result->uresult != NULL && udp->ports.len != 0 && result->uresult->results == NULL)
            {
                ProbeCleanupUdpResultList(result->uresult);
                result->uresult = NULL;
            }
            if (result->uresult == NULL)
            {
                result->status = HOST_CHECK_STATUS_ERROR;
                return 0;
            }
            result->uresult->len = udp->ports.len;
        }

        for (size_t i = 0; i < udp->ports.len; ++i)
        {
            uint16_t port = udp->ports.items[i];
            UdpResult probe_result = ProbeUdp(&udp->probe, port);
            result->uresult->results[i] = (UdpResultWrapper){ port, probe_result, 0 };
            responsive |= probe_result == UDP_RESULT_RESPONSE_RECEIVED;
            encountered_error |= probe_result == UDP_RESULT_ERROR;
        }
    }

    if (responsive) {
        result->status = HOST_CHECK_STATUS_SUCCEEDED;
        return 1;
    }
    result->status = encountered_error ? HOST_CHECK_STATUS_ERROR : HOST_CHECK_STATUS_RETRIES_EXHAUSTED;
    return 0;
}

DWORD WINAPI ProbeWorker(LPVOID argument)
{
    WorkerContext *ctx = argument;
    if (ctx == NULL)
        return 0;



    ProbeStats *stats = ctx->stats;
    HostUpCheck *check = &ctx->check;
    ProbeConfig *config = ctx->config;


    lua_State *lua = ctx->lua;
    if (!LuaInit(lua, config) || !LoadScript(lua, config)) {
        ProbeCleanupWorkerContext(ctx);
        return 0;
    }


    Ipv4 ip;
    while (IpPicker_Pick(ctx->picker, &ip, NULL) && atomic_load(&stats->checked) < config->limit)
    {
        if (stats) atomic_fetch_add(&stats->claimed, 1);

        int up = CheckHost(check, config->method, ip);
        if (stats != NULL)
        {
            atomic_fetch_add(&stats->checked, 1);
            if(up)
            {
                atomic_fetch_add(&stats->up, 1);
            }
            else{
                atomic_fetch_add(&stats->down, 1);
            }
        }

        if (!up) continue;
        if (LuaInvoke(lua, check) && stats != NULL)
            atomic_fetch_add(&stats->script_confirmed_hosts, 1);
    }

    ProbeCleanupWorkerContext(ctx);
    return 0;
}
