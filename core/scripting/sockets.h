#ifndef CORE_SCRIPTING_SOCKETS_H
#define CORE_SCRIPTING_SOCKETS_H

#include "../../lua/include/lua.h"

int luaopen_sockets(lua_State *state);
void LuaSockets_Register(lua_State *state);

#endif
