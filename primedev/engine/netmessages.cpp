#include "netmessages.h"
#include "netchannel.h"

ExternalNetMessageHandler* g_pExternalNetMessageHandler = nullptr;

AUTOHOOK_INIT()

// clang-format off
AUTOHOOK(CBaseClient__ConnectionStart, engine.dll + 0x1019C0, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	if (g_pExternalNetMessageHandler)
	{
		for (auto& msg : g_pExternalNetMessageHandler->m_CLCMessages)
			CNetChan__RegisterMessage(chan, msg);
	}

	return CBaseClient__ConnectionStart(thisptr, chan);
}

// clang-format off
AUTOHOOK(CBaseClientState__ConnectionStart, engine.dll + 0x8CB40, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	if (g_pExternalNetMessageHandler)
	{
		for (auto& msg : g_pExternalNetMessageHandler->m_SVCMessages)
			CNetChan__RegisterMessage(chan, msg);
	}

	return CBaseClientState__ConnectionStart(thisptr, chan);
}

ON_DLL_LOAD_RELIESON("engine.dll", NetMessages, NetChan, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
