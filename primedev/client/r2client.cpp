#include "r2client.h"

char* g_pLocalPlayerUserID;
char* g_pLocalPlayerOriginToken;
GetBaseLocalClientType GetBaseLocalClient;

static __int64 (*__fastcall ReloadModels)(__int64 a1, unsigned int a2) = nullptr;
static uintptr_t model_loader;

void ConCommand_reload_models(const CCommand& args)
{
	CModule filesystem_dll = CModule("filesystem_stdio.dll");
	char* ptr = filesystem_dll.Offset(0xe5aa9).RCast<char*>();
	char back = *ptr;
	*ptr = 0;
	ReloadModels(model_loader, 2);
	*ptr = back;
};

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", R2EngineClient, ConCommand, (CModule module))
{
	g_pLocalPlayerUserID = module.Offset(0x13F8E688).RCast<char*>();
	g_pLocalPlayerOriginToken = module.Offset(0x13979C80).RCast<char*>();
	RegisterConCommand("reload_models", ConCommand_reload_models, "Send friend request to uid", FCVAR_CLIENTDLL);
	model_loader = module.Offset(0x7c4c20);
	module.Offset(0xCF024).Patch({0xEB, 0x0E});
	ReloadModels = module.Offset(0xCEEF0).RCast<decltype(ReloadModels)>();
	GetBaseLocalClient = module.Offset(0x78200).RCast<GetBaseLocalClientType>();
}
