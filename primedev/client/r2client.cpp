#include "r2client.h"

char* g_pLocalPlayerUserID;
char* g_pLocalPlayerOriginToken;
GetBaseLocalClientType GetBaseLocalClient;

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", R2EngineClient, ConCommand, (CModule module))
{
	g_pLocalPlayerUserID = module.Offset(0x13F8E688).RCast<char*>();
	g_pLocalPlayerOriginToken = module.Offset(0x13979C80).RCast<char*>();
	GetBaseLocalClient = module.Offset(0x78200).RCast<GetBaseLocalClientType>();
}
