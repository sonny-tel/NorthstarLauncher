#include "core/vanilla.h"
#include "engine/r2engine.h"

AUTOHOOK_INIT()

VanillaCompatibility* g_pVanillaCompatibility;

// clang-format off
AUTOHOOK(matchmake, engine.dll + 0xF220, int*, __fastcall, ())
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);

	return matchmake();
}

// clang-format off
AUTOHOOK(connectwithtoken, engine.dll + 0x769C0, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);

	return connectwithtoken(a1);
}

// clang-format off
AUTOHOOK(silentconnect, engine.dll + 0x76F00, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);

	return silentconnect(a1);
}

// vanilla only uses connectwithtoken and silentconnect, this is actually pretty good for us to differentiate
// clang-format off
AUTOHOOK(concommand_connect, engine.dll + 0x76720, __int64, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	return concommand_connect(a1);
}

ON_DLL_LOAD_RELIESON("engine.dll", VanillaCompat, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
