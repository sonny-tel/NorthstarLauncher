#include "squirrel/squirrel.h"
#include "client/r2client.h"
#include "client/origin.h"

char g_szNorthstarToken[256] = {};

void OriginAuthcodeStrcpyCallback2(__int64 a1, __int64* a2)
{
	if (a2)
	{
		if (*a2)
		{
			spdlog::info("Token: {}", (char*)*a2);

			strncpy_s(g_szNorthstarToken, sizeof(g_szNorthstarToken), (char*)*a2, sizeof(g_szNorthstarToken) - 1);
		}
	}
}

ADD_SQFUNC("int", NSGetSignonState, "", "Returns the current signon state of the client.", ScriptContext::CLIENT | ScriptContext::UI)
{
	CClientState* client = GetBaseLocalClient();
	if (!client)
	{
		g_pSquirrel[context]->pushinteger(sqvm, -1);
		return SQRESULT_NOTNULL;
	}

	__int64 userId = 0;
	__int64 res = 0;
	if(g_szNorthstarToken[0] == '\0')
	{
		if (g_pLocalPlayerUserID)
			userId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);
		if (userId > 0)
			res = OriginRequestAuthCode(userId, "TITANFALL3-PC-CLIENT", OriginAuthcodeStrcpyCallback2, 0, 30000, 0);
	}

	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(client->m_nSignonState));
	return SQRESULT_NOTNULL;
}
