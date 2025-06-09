#pragma once

typedef void (*OriginAuthcodeStrcpyCallbackType)(__int64 a1, __int64* a2);
typedef __int64 (*OriginRequestAuthCodeType)(__int64 userId, const char* game, OriginAuthcodeStrcpyCallbackType callback, __int64* a4, int a5, __int64* a6);
extern OriginRequestAuthCodeType OriginRequestAuthCode;

std::string* GetNewOriginToken(int timeoutSeconds);
