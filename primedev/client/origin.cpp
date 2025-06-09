#include "origin.h"
#include "r2client.h"

OriginRequestAuthCodeType OriginRequestAuthCode;
char* tmpTok = nullptr;

void OriginAuthcodeStrcpyCallback(__int64 a1, char* a2)
{
	if (tmpTok)
		spdlog::error("OriginAuthcodeStrcpyCallback called while tmpTok is not null, do not use this in parallel!");

	tmpTok = new char[256];

	if (a2)
		strcpy(tmpTok, a2);
}

// do not use this on the main thread, this is blocking
char* GetNewOriginToken(int timeoutSeconds)
{
	char* tok = new char[256];

	if( tmpTok )
		delete[] tmpTok;

    __int64 userId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);

	__int64 res = OriginRequestAuthCode(userId, "TITANFALL2-PC-SERVER", OriginAuthcodeStrcpyCallback, 0, 30000, 0);

	auto start = std::chrono::steady_clock::now();

	for(;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= timeoutSeconds)
        {
			if( tmpTok )
				delete[] tmpTok;
		}

		if (!tmpTok)
			continue;

		if (tmpTok[0] == '\0')
			continue;

		strcpy(tok, tmpTok);

		delete[] tmpTok;

		break;
	}

	return tok;
}

ON_DLL_LOAD("OriginSDK.dll", OriginSDK, (CModule module))
{
	// takes 5 params: user_id (g_pLocalPlayerUserID), the game ("TITANFALL2-PC-SERVER"), callback func to strcpy the token to which looks like void(*)(__int64 a1, char* a2),
	// and 3 ints which idk what they are but are 0, 30000 and 0 by default. probably some token ttl stuff
	OriginRequestAuthCode = module.GetExportedFunction("OriginRequestAuthCode").RCast<OriginRequestAuthCodeType>();
}
