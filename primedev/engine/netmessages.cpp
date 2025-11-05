#include "netmessages.h"
#include "netchannel.h"
#include "mods/autodownload/svcdownloader.h"

AUTOHOOK_INIT()

// clang-format off
AUTOHOOK(CBaseClient__ConnectionStart, engine.dll + 0x1019C0, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{

	return CBaseClient__ConnectionStart(thisptr, chan);
}

// clang-format off
AUTOHOOK(CBaseClientState__ConnectionStart, engine.dll + 0x8CB40, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	CNetChan__RegisterMessage(
		chan, new SVC_SetModSchema());

	return CBaseClientState__ConnectionStart(thisptr, chan);
}

ON_DLL_LOAD_RELIESON("engine.dll", NetMessages, NetChan, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
