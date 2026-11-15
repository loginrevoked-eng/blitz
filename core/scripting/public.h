#ifndef CORE_SCRIPTING_PUBLIC_H
#define CORE_SCRIPTING_PUBLIC_H

#include "../probe.h"

int LuaInit(lua_State *lua, ProbeConfig *config);
int LuaInvoke(lua_State *lua, HostUpCheck *check);

#endif
