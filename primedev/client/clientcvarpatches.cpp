#include "core/convar/convar.h"

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ClientCVARPatches, ConVar, (CModule module))
{
	ConVar* Cvar_openInvites_filterByLanguage = g_pCVar->FindVar("openInvites_filterByLanguage");
	Cvar_openInvites_filterByLanguage->AddFlags(FCVAR_ARCHIVE_PLAYERPROFILE);

	ConVar* Cvar_openInvites_filterByRegion = g_pCVar->FindVar("openInvites_filterByRegion");
	Cvar_openInvites_filterByRegion->AddFlags(FCVAR_ARCHIVE_PLAYERPROFILE);
}
