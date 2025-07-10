#include "origin.h"
#include "r2client.h"
#include "util/utils.h"

OriginRequestAuthCodeType OriginRequestAuthCode;
OriginGetPresenceType OriginGetPresence;
OriginGetPresenceType OriginQueryPresence;
OriginGetErrorDescriptionType OriginGetErrorDescription;
OriginReadEnumerationSyncType OriginReadEnumerationSync;
OriginRequestFriendType OriginRequestFriend;

std::string* tmpTok = nullptr;

std::unordered_map<__int64, std::string> g_IDPartySubMap;

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

		if (!IsBadReadPtr2((void*)match_sub))
		{
			if (match_sub && match_sub[0] != '\0' && strncmp(match_sub, "p_PC_", 5) == 0)
			{
				g_IDPartySubMap.insert(std::make_pair(pFriendPresence->uid, std::string(match_sub)));
				spdlog::info("GamePresence for uid {}: {}", uid, match_sub);
			}
			else
			{
				if (g_IDPartySubMap.contains(pFriendPresence->uid))
					g_IDPartySubMap.erase(pFriendPresence->uid);

				spdlog::info("GamePresence for uid {} is empty", uid);
			}
		}
		else
		{
			if (g_IDPartySubMap.contains(pFriendPresence->uid))
				g_IDPartySubMap.erase(pFriendPresence->uid);

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
	o_UpdateFriendsList = module.Offset(0x184000).RCast<decltype(o_UpdateFriendsList)>();
	HookAttach(&(PVOID&)o_UpdateFriendsList, (PVOID)UpdateFriendsListHook);

	o_185AE0 = module.Offset(0x185AE0).RCast<decltype(o_185AE0)>();
	HookAttach(&(PVOID&)o_185AE0, (PVOID)sub_185AE0);

}

// static int OriginReadEnumerationSyncHook(__int64 a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5, __int64 a6) {
// 	// print all the parameters in hex format


// 	spdlog::info("OriginReadEnumerationSyncHook called with a1: {}, a2: {}, a3: {}, a4: {}, a5: {}, a6: {}", a1, a2, a3, a4, a5,a6);
// 	return OriginReadEnumerationSync(a1, a2, a3, a4, a5,a6);
// }

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
