#ifndef CORE_SCRIPTING_RTSP_H
#define CORE_SCRIPTING_RTSP_H

#include "../../lua/include/lua.h"

int LuaRTSP_Confirm(lua_State *state);
int luaopen_rtsp(lua_State *state);
void LuaRTSP_Register(lua_State *state);

#endif
