#include "client.h"

void (*CClient__Disconnect)(void* self, uint32_t unknownButAlways1, const char* reason, ...);
void (*CClient__SendDataBlock)(void* self, bf_write* msg);
CClient* g_pClientArray;

void CClient::Disconnect(const Reputation_t nRepLevel, const char* reason, ...)
{
	if (m_Signon != eSignonState::NONE)
	{
		char szBuf[1024];
		{
			va_list vArgs;
			va_start(vArgs, reason);

			const int ret = snprintf(szBuf, sizeof(szBuf), reason, vArgs);

			if (ret < 0)
				szBuf[0] = '\0';

			va_end(vArgs);
		}
	}
}


ON_DLL_LOAD("engine.dll", CClient, (CModule module))
{
	CClient__Disconnect = module.Offset(0x1012C0).RCast<void (*)(void*, uint32_t, const char*, ...)>();
	CClient__SendDataBlock = module.Offset(0x104870).RCast<void (*)(void*, bf_write*)>();
	g_pClientArray = module.Offset(0x12A53F90).RCast<CClient*>();
}
