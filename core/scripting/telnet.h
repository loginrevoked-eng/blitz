#ifndef CORE_SCRIPTING_TELNET_H
#define CORE_SCRIPTING_TELNET_H

#include "../../lua/include/lua.h"

int LuaTelnet_Confirm(lua_State *state);
int luaopen_telnet(lua_State *state);
void LuaTelnet_Register(lua_State *state);

#endif
