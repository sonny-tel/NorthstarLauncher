#include "origin.h"
#include "r2client.h"

OriginRequestAuthCodeType OriginRequestAuthCode;
OriginGetPresenceType OriginGetPresence;
OriginGetPresenceType OriginQueryPresence;
OriginGetErrorDescriptionType OriginGetErrorDescription;
OriginReadEnumerationSyncType OriginReadEnumerationSync;
OriginRequestFriendType OriginRequestFriend;

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
	auto localUserId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);
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
	uintptr_t some;
	OriginRequestFriend(localUserId, userId, 2, some, 0);
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

class FriendPresence
{
public:
	int64_t uid; // 0x0000
	int32_t state; // 0x0008
	char pad_000C[4]; // 0x000C
	char* Title_ID; // 0x0010
	char* MP_ID; // 0x0018
	char* Title; // 0x0020
	char* Presence; // 0x0028
	char* GamePresence; // 0x0030
	char* empty; // 0x0038
	char* empty2; // 0x0040
}; // Size: 0x0880

//0x185AE0

static __int64 (*__fastcall o_185AE0)(__int64 a1, unsigned int a2, FriendPresence* p) = nullptr;
static __int64 __fastcall sub_185AE0(__int64 uid, unsigned int a2, FriendPresence* pFriendPresence)
{
	if (pFriendPresence->GamePresence)
	{
		auto match_sub = *(const char**)&pFriendPresence->GamePresence[0x20];
		if (match_sub && match_sub[0] != '\0')
		{
			spdlog::info("GamePresence for uid {}: {}", uid, match_sub);
		}
		else
		{
			spdlog::info("GamePresence for uid {} is empty", uid);
		}
	}
	return o_185AE0(uid, a2,pFriendPresence);
}




static __int64 (*__fastcall o_UpdateFriendsList)(__int64 a1, __int64 a2, unsigned __int64 a3, __int64 a4, int a5) = nullptr;

static __int64 __fastcall UpdateFriendsListHook(__int64 a1, __int64 a2, unsigned __int64 a3, __int64 a4, int a5) {
	spdlog::info("UpdateFriendsListHook called with a1: {}, a2: {}, a3: {}, a4: {}, a5: {}", a1, a2, a3, a4, a5);
	return o_UpdateFriendsList(a1, a2, a3, a4, a5);
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientOrigin, ConCommand, (CModule module))
{
	RegisterConCommand("ns_fish", ConCommand_fish_origin, "does stuff idk", 0);
	o_UpdateFriendsList = module.Offset(0x184000).RCast<decltype(o_UpdateFriendsList)>();
	HookAttach(&(PVOID&)o_UpdateFriendsList, (PVOID)UpdateFriendsListHook);

	o_185AE0 = module.Offset(0x185AE0).RCast<decltype(o_185AE0)>();
	HookAttach(&(PVOID&)o_185AE0, (PVOID)sub_185AE0);

}

static int OriginReadEnumerationSyncHook(__int64 a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5, __int64 a6) {
	// print all the parameters in hex format
	

	spdlog::info("OriginReadEnumerationSyncHook called with a1: {}, a2: {}, a3: {}, a4: {}, a5: {}, a6: {}", a1, a2, a3, a4, a5,a6);
	return OriginReadEnumerationSync(a1, a2, a3, a4, a5,a6);
}

ON_DLL_LOAD("OriginSDK.dll", OriginSDK,(CModule module))
{
	// takes 5 params: user_id (g_pLocalPlayerUserID), the game ("TITANFALL2-PC-SERVER"), callback func to strcpy the token to which looks like void(*)(__int64 a1, char* a2),
	// and 3 ints which idk what they are but are 0, 30000 and 0 by default. probably some token ttl stuff
	OriginRequestAuthCode = module.GetExportedFunction("OriginRequestAuthCode").RCast<OriginRequestAuthCodeType>();
	OriginGetPresence = module.GetExportedFunction("OriginGetPresence").RCast<OriginGetPresenceType>();
	OriginQueryPresence = module.GetExportedFunction("OriginQueryPresence").RCast<OriginGetPresenceType>();
	OriginGetErrorDescription = module.GetExportedFunction("OriginGetErrorDescription").RCast<OriginGetErrorDescriptionType>();
	OriginReadEnumerationSync = module.GetExportedFunction("OriginReadEnumerationSync").RCast<OriginReadEnumerationSyncType>();
	OriginRequestFriend = module.GetExportedFunction("OriginRequestFriend").RCast<OriginRequestFriendType>();
	//HookAttach(&(PVOID&)OriginReadEnumerationSync, (PVOID)OriginReadEnumerationSyncHook);
}
