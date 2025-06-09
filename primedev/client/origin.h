#pragma once

typedef void (*OriginAuthcodeStrcpyCallbackType)(__int64 a1, char* a2);
typedef __int64 (*OriginRequestAuthCodeType)(__int64 userId, const char* game, OriginAuthcodeStrcpyCallbackType callback, int a4, int a5, int a6);
extern OriginRequestAuthCodeType OriginRequestAuthCode;

char* GetNewOriginToken(int timeoutSeconds);
