#include "engine/r2engine.h"
#include "client/r2client.h"
#include "engine/netmessages.h"
#include "mods/autodownload/moddownloader.h"

AUTOHOOK_INIT()

AUTOHOOK(CClientState__ProcessConnectionlessPacket, engine.dll + 0x19F400, bool, __fastcall, (CClientState* self, netpacket_t* packet))
{
	bf_read msg(packet->data, packet->size);
	unsigned int header = msg.ReadLong();

	if (header == CONNECTIONLESS_HEADER)
	{
		char packetType = msg.ReadChar();

		switch(packetType)
		{
			case S2C_MODDOWNLOADINFO:
				return g_pModDownloader->RecvModInfoConnectionlessPacket(msg);
			case S2A_CUSTOMSERVERINFO:
				return true;
			default:
				break;
		}
	}

	return CClientState__ProcessConnectionlessPacket(self, packet);
}

ON_DLL_LOAD_RELIESON("engine.dll", ClientNetHooks, R2Engine, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
