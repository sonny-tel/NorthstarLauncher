#include "core/vanilla.h"
#include "engine/r2engine.h"
#include "tracy/Tracy.hpp"

VanillaCompatibility* g_pVanillaCompatibility;

ConVar* Cvar_ns_skip_vanilla_integrity_check;

bool VanillaCompatibility::GetVanillaCompatibility()
{
	ZoneScoped;

	return !(g_pCVar->FindVar("ns_is_northstar_server")->GetBool() || g_pCVar->FindVar("serverfilter")->GetBool());
}

ON_DLL_LOAD_RELIESON("engine.dll", VanillaCompat, ConVar, (CModule module))
{
	Cvar_ns_skip_vanilla_integrity_check =
		new ConVar("ns_skip_vanilla_integrity_check", "0", FCVAR_NONE, "Skip vanilla integrity check for compatibility with older servers");
}
