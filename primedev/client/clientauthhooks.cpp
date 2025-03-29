#include "client/r2client.h"
#include "core/convar/convar.h"
#include "core/vanilla.h"
#include "masterserver/masterserver.h"

ConVar* Cvar_ns_has_agreed_to_send_token;
char* pDummy3P = const_cast<char*>("Protocol 3: Protect the Pilot");

// mirrored in script
const int NOT_DECIDED_TO_SEND_TOKEN = 0;
const int AGREED_TO_SEND_TOKEN = 1;
const int DISAGREED_TO_SEND_TOKEN = 2;

static void (*__fastcall o_pAuthWithStryder)(void* a1) = nullptr;
static void __fastcall h_AuthWithStryder(void* a1)
{
	if (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone && Cvar_ns_has_agreed_to_send_token->GetInt() != DISAGREED_TO_SEND_TOKEN)
	{
		// if player has agreed to send token and we aren't already authing, try to auth
		if (Cvar_ns_has_agreed_to_send_token->GetInt() == AGREED_TO_SEND_TOKEN &&
			!g_pMasterServerManager->m_bOriginAuthWithMasterServerInProgress)
			g_pMasterServerManager->AuthenticateOriginWithMasterServer(g_pLocalPlayerUserID, g_pLocalPlayerOriginToken);
	}

	o_pAuthWithStryder(a1);
}

static char* (*__fastcall o_pAuth3PToken)() = nullptr;
static char* __fastcall h_Auth3PToken()
{
	// return a dummy token for northstar servers that don't need the session token stuff
	// base it off serverfilter cvar since ns_is_northstar_server could be unset by an evil server
	// we'll get dropped if they're faking it
	if (g_pCVar->FindVar("serverfilter")->GetBool() && g_pMasterServerManager->m_sOwnClientAuthToken[0])
		return pDummy3P;

	return o_pAuth3PToken();
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientAuthHooks, ConVar, (CModule module))
{
	o_pAuthWithStryder = module.Offset(0x1843A0).RCast<decltype(o_pAuthWithStryder)>();
	HookAttach(&(PVOID&)o_pAuthWithStryder, (PVOID)h_AuthWithStryder);

	o_pAuth3PToken = module.Offset(0x183760).RCast<decltype(o_pAuth3PToken)>();
	HookAttach(&(PVOID&)o_pAuth3PToken, (PVOID)h_Auth3PToken);

	// this cvar will save to cfg once initially agreed with
	Cvar_ns_has_agreed_to_send_token = new ConVar(
		"ns_has_agreed_to_send_token",
		"0",
		FCVAR_ARCHIVE_PLAYERPROFILE,
		"whether the user has agreed to send their origin token to the northstar masterserver");
}
