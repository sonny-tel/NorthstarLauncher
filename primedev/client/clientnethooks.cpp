#include "engine/r2engine.h"
#include "client/r2client.h"
#include "engine/netmessages.h"

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
			{
				spdlog::info("Received S2C_MODDOWNLOADINFO packet from server");

				packet->message->ReadLong();
				packet->message->ReadChar();

				return true;
			}
			case S2C_CUSTOMSERVERINFO:
			{
				spdlog::info("Received S2C_CUSTOMSERVERINFO packet from server");

				packet->message->ReadLong();
				packet->message->ReadChar();

				return true;
			}
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
