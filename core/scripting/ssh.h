#ifndef CORE_SCRIPTING_SSH_H
#define CORE_SCRIPTING_SSH_H

#include "../../lua/include/lua.h"

int LuaParseSSHIdentification(lua_State *state);
int luaopen_ssh(lua_State *state);
void LuaSSH_Register(lua_State *state);

#endif
