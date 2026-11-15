#ifndef CORE_SCRIPTING_LUA_UTILS_H
#define CORE_SCRIPTING_LUA_UTILS_H

#include "../../lua/include/lua.h"

int LuaPrintToLog(lua_State *state);
void LuaUtils_RegisterPrint(lua_State *state);

#endif
