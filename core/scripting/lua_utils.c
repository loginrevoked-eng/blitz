#include "lua_utils.h"

#include "../../logging/logging.h"
#include "../../lua/include/lauxlib.h"

#include <limits.h>
#include <string.h>

typedef enum {
    LUA_LOG_RAW,
    LUA_LOG_DEBUG,
    LUA_LOG_INFO,
    LUA_LOG_WARN,
    LUA_LOG_ERROR,
    LUA_LOG_FATAL
} LuaLogKind;

static const char *FormatArguments(lua_State *state)
{
    int argument_count = lua_gettop(state);
    luaL_Buffer buffer;
    luaL_buffinit(state, &buffer);

    for (int i = 1; i <= argument_count; ++i) {
        size_t length = 0;
        const char *text = luaL_tolstring(state, i, &length);
        int text_ref = luaL_ref(state, LUA_REGISTRYINDEX);
        int has_null = 0;
        for (size_t j = 0; j < length; ++j) {
            if (text[j] == '\0') {
                has_null = 1;
                break;
            }
        }

        if (has_null) {
            luaL_Buffer escaped;
            luaL_buffinit(state, &escaped);
            size_t start = 0;
            for (size_t j = 0; j < length; ++j) {
                if (text[j] != '\0')
                    continue;
                luaL_addlstring(&escaped, text + start, j - start);
                luaL_addlstring(&escaped, "\\0", 2);
                start = j + 1;
            }
            luaL_addlstring(&escaped, text + start, length - start);
            luaL_pushresult(&escaped);
            luaL_addvalue(&buffer);
        } else {
            lua_rawgeti(state, LUA_REGISTRYINDEX, text_ref);
            luaL_addvalue(&buffer);
        }
        luaL_unref(state, LUA_REGISTRYINDEX, text_ref);
        if (i != argument_count)
            luaL_addchar(&buffer, '\t');
    }

    luaL_addchar(&buffer, '\n');
    luaL_pushresult(&buffer);
    return lua_tostring(state, -1);
}

static int LogArguments(lua_State *state, LuaLogKind kind)
{
    const char *message = FormatArguments(state);
    if (message == NULL)
        message = "\n";

    switch (kind) {
    case LUA_LOG_RAW:   Log("%s", message); break;
    case LUA_LOG_DEBUG: Debug("%s", message); break;
    case LUA_LOG_INFO:  Info("%s", message); break;
    case LUA_LOG_WARN:  Warn("%s", message); break;
    case LUA_LOG_ERROR: Error("%s", message); break;
    case LUA_LOG_FATAL: Panic("%s", message); break;
    }
    lua_pop(state, 1);
    return 0;
}

int LuaPrintToLog(lua_State *state)
{
    return LogArguments(state, LUA_LOG_RAW);
}

static int LuaDebug(lua_State *state)
{
    return LogArguments(state, LUA_LOG_DEBUG);
}

static int LuaLog(lua_State *state)
{
    return LogArguments(state, LUA_LOG_RAW);
}

static int LuaInfo(lua_State *state)
{
    return LogArguments(state, LUA_LOG_INFO);
}

static int LuaWarn(lua_State *state)
{
    return LogArguments(state, LUA_LOG_WARN);
}

static int LuaError(lua_State *state)
{
    return LogArguments(state, LUA_LOG_ERROR);
}

static int LuaFatal(lua_State *state)
{
    return LogArguments(state, LUA_LOG_FATAL);
}

static int LuaSetLogLevel(lua_State *state)
{
    static const struct {
        const char *name;
        LogLevel level;
    } levels[] = {
        { "debug", LOG_LEVEL_DEBUG },
        { "log", LOG_LEVEL_LOG },
        { "info", LOG_LEVEL_INFO },
        { "warn", LOG_LEVEL_WARN },
        { "error", LOG_LEVEL_ERROR },
        { "fatal", LOG_LEVEL_FATAL },
        { "quiet", LOG_LEVEL_QUIET }
    };
    const char *name = luaL_checkstring(state, 1);
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        if (strcmp(name, levels[i].name) == 0) {
            SetLogLevel(levels[i].level);
            return 0;
        }
    }
    return luaL_error(state, "unknown log level '%s'", name);
}

static int LuaSetLoggerFlags(lua_State *state)
{
    lua_Integer flags = luaL_checkinteger(state, 1);
    if (flags < 0 || (lua_Unsigned)flags > UINT_MAX)
        return luaL_argerror(state, 1, "flags must fit an unsigned int");
    SetLoggerFlags((unsigned int)flags);
    return 0;
}

static int LuaSuspendLogs(lua_State *state)
{
    (void)state;
    SuspendLogs();
    return 0;
}

static int LuaResumeLogs(lua_State *state)
{
    (void)state;
    ResumeLogs();
    return 0;
}

static void SetLoggingFunction(lua_State *state, const char *name,
    lua_CFunction function)
{
    lua_pushcfunction(state, function);
    lua_setfield(state, -2, name);
}

static int LuaOpenLogging(lua_State *state)
{
    lua_createtable(state, 0, 11);
    SetLoggingFunction(state, "log", LuaLog);
    SetLoggingFunction(state, "debug", LuaDebug);
    SetLoggingFunction(state, "info", LuaInfo);
    SetLoggingFunction(state, "warn", LuaWarn);
    SetLoggingFunction(state, "error", LuaError);
    SetLoggingFunction(state, "fatal", LuaFatal);
    SetLoggingFunction(state, "panic", LuaFatal);
    SetLoggingFunction(state, "set_level", LuaSetLogLevel);
    SetLoggingFunction(state, "set_flags", LuaSetLoggerFlags);
    SetLoggingFunction(state, "suspend", LuaSuspendLogs);
    SetLoggingFunction(state, "resume", LuaResumeLogs);
    return 1;
}

void LuaUtils_RegisterPrint(lua_State *state)
{
    if (state == NULL)
        return;

    lua_pushcfunction(state, LuaPrintToLog);
    lua_setglobal(state, "print");

    luaL_requiref(state, "logging", LuaOpenLogging, 0);
    lua_pop(state, 1);
}
