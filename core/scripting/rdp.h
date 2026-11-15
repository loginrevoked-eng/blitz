#ifndef CORE_SCRIPTING_RDP_H
#define CORE_SCRIPTING_RDP_H

#include "../../lua/include/lua.h"

int LuaRDP_Confirm(lua_State *state);
int luaopen_rdp(lua_State *state);
void LuaRDP_Register(lua_State *state);

#endif
