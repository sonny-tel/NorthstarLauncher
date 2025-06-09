#include "origin.h"
#include "r2client.h"

OriginRequestAuthCodeType OriginRequestAuthCode;
std::string* tmpTok = nullptr;

void OriginAuthcodeStrcpyCallback(__int64 a1, __int64* a2)
{
	if (a2)
	{
		if (*a2)
		{
			// spdlog::info("Token: {}", (char*)*a2);

			if( tmpTok )
				delete tmpTok;

			tmpTok = new std::string((char*)*a2);
		}
	}
}

// do not use this on the main thread, this is blocking
std::string* GetNewOriginToken(int timeoutSeconds)
{
	std::string* tok = new std::string();

	__int64 userId = 0;
	__int64 res = 0;

	if (g_pLocalPlayerUserID)
    	userId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);

	if (userId > 0)
		res = OriginRequestAuthCode(userId, "TITANFALL2-PC-SERVER", OriginAuthcodeStrcpyCallback, 0, 30000, 0);

	auto start = std::chrono::steady_clock::now();

	for(;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= timeoutSeconds)
        {
			if (tmpTok)
			{
				if( tmpTok->length() >= 0 )
					delete tmpTok;
			}

			break;
		}

		if (!tmpTok)
		    continue;

		if (tmpTok->empty())
			continue;

		if (tmpTok->c_str()[0] == '\0')
			continue;

		if (tmpTok->length() > 0)
		{
			tok->assign(*tmpTok);
			// spdlog::info("Origin token: {}", *tok);
		}
		else
		{
			spdlog::error("Failed to get origin token, tmpTok is empty");
			delete tok;
			return nullptr;
		}

		delete tmpTok;

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
