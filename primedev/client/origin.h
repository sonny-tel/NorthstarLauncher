#pragma once

typedef void (*OriginAuthcodeStrcpyCallbackType)(__int64 a1, __int64* a2);
typedef __int64 (*OriginRequestAuthCodeType)(__int64 userId, const char* game, OriginAuthcodeStrcpyCallbackType callback, __int64* a4, int a5, __int64* a6);
extern OriginRequestAuthCodeType OriginRequestAuthCode;
enum OriginPresenceEnum
{
	UNK,
	IS_OFFLINE,
	IS_ONLINE,
	IN_GAME,
	BUSY,
	AWAY,
	IS_IN_PARTY,
	IS_IN_GAME_PARTY,
	IS_INVITE_ONLY
};
typedef int (*OriginGetPresenceType)(
	__int64 userId, OriginPresenceEnum* presenceData, char* a3, __int64 a4, char* a5, __int64 a6, char* a7, __int64 a8);

extern OriginGetPresenceType OriginGetPresence;

typedef const char* (*OriginGetErrorDescriptionType)(int errorCode);
extern OriginGetErrorDescriptionType OriginGetErrorDescription;
std::string* GetNewOriginToken(int timeoutSeconds);
