#pragma once

#include "player.h"
#include "engine/inetchannel.h"

class CBaseEntity;
extern CBaseEntity* (*Server_GetEntityByIndex)(int index);

class CServer : public IConnectionlessPacketHandler
{

};
