#include "origin.h"
#include "r2client.h"

OriginRequestAuthCodeType OriginRequestAuthCode;
OriginGetPresenceType OriginGetPresence;
OriginGetPresenceType OriginQueryPresence;
OriginGetErrorDescriptionType OriginGetErrorDescription;
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

			delete tok;

			spdlog::error("Failed to get origin token: timeout after {} seconds", timeoutSeconds);

			return nullptr;
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

		delete tmpTok;

		break;
	}

	return tok;
}

std::string PresenceToString(OriginPresenceEnum presence)
{
	switch (presence)
	{
	case OriginPresenceEnum::IS_ONLINE:
		return "Online";
	case OriginPresenceEnum::IS_OFFLINE:
		return "Offline";
	case OriginPresenceEnum::AWAY:
		return "Away";
	case OriginPresenceEnum::IS_IN_PARTY:
		return "In Party";
	case OriginPresenceEnum::BUSY:
		return "Busy";
	case OriginPresenceEnum::IN_GAME:
		return "In Game";
	case OriginPresenceEnum::IS_IN_GAME_PARTY:
		return "In Game Party";
	default:
		return "Unknown";
	}
}

ADD_SQFUNC("PresenceData", NSGetOriginPresence, "string uid", "", ScriptContext::UI)
{

	std::string uid = g_pSquirrel<context>->getstring(sqvm, 1);
	__int64 userId = _strtoi64(uid.c_str(), nullptr, 10);
	OriginPresenceEnum presence;
	char* sessionId = new char[256];
	char* p1 = new char[256]; // unused, but required by the function signature
	char* p2 = new char[256]; // unused, but required by the function signature
	int ret = OriginGetPresence(userId, &presence, p1, 256, p2, 256, sessionId, 256);
	spdlog::info("Origin presence for {}: Presence: {},Session: {}, p1: {}, p2: {}", uid, presence, sessionId,p1,p2);
	auto result = OriginGetErrorDescription(ret);
	spdlog::info("Origin presence for {}: {}", uid, result);
	g_pSquirrel<context>->pushnewstructinstance(sqvm, 2);
	g_pSquirrel<context>->pushinteger(sqvm, presence);
	g_pSquirrel<context>->sealstructslot(sqvm, 0);

	g_pSquirrel<context>->pushstring(sqvm, sessionId,-1);
	g_pSquirrel<context>->sealstructslot(sqvm, 1);
	return SQRESULT_NOTNULL;
}

void ConCommand_fish_origin(const CCommand& args)
{
	__int64 userId = 0;
	if (g_pLocalPlayerUserID)
		userId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);
	OriginPresenceEnum presence;
	char* sessionId = new char[256];
	OriginGetPresence(userId, &presence, nullptr, 0, nullptr, 0, sessionId, 256);
	spdlog::info("Origin presence: {},{},{}", presence,PresenceToString(presence), sessionId);
}
ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientOrigin, ConCommand, (CModule module))
{
	RegisterConCommand("ns_fish", ConCommand_fish_origin, "does stuff idk", 0);
}

ON_DLL_LOAD("OriginSDK.dll", OriginSDK,(CModule module))
{
	// takes 5 params: user_id (g_pLocalPlayerUserID), the game ("TITANFALL2-PC-SERVER"), callback func to strcpy the token to which looks like void(*)(__int64 a1, char* a2),
	// and 3 ints which idk what they are but are 0, 30000 and 0 by default. probably some token ttl stuff
	OriginRequestAuthCode = module.GetExportedFunction("OriginRequestAuthCode").RCast<OriginRequestAuthCodeType>();
	OriginGetPresence = module.GetExportedFunction("OriginGetPresence").RCast<OriginGetPresenceType>();
	OriginQueryPresence = module.GetExportedFunction("OriginQueryPresence").RCast<OriginGetPresenceType>();
	OriginGetErrorDescription = module.GetExportedFunction("OriginGetErrorDescription").RCast<OriginGetErrorDescriptionType>();
}
