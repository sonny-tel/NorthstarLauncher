#include "netmessages.h"
#include "netchannel.h"

AUTOHOOK_INIT()

// clang-format off
AUTOHOOK(CClient__ConnectionStart, engine.dll + 0x1019C0, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	return CClient__ConnectionStart(thisptr, chan);
}

// clang-format off
AUTOHOOK(CClientState__ConnectionStart, engine.dll + 0x8CB40, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	return CClientState__ConnectionStart(thisptr, chan);
}

ON_DLL_LOAD_RELIESON("engine.dll", NetMessages, NetChan, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
