#include "r2server.h"

CBaseEntity* (*Server_GetEntityByIndex)(int index);

ON_DLL_LOAD("server.dll", R2GameServer, (CModule module))
{
	Server_GetEntityByIndex = module.Offset(0xFB820).RCast<CBaseEntity* (*)(int)>();
}
