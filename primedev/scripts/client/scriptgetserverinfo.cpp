#include "squirrel/squirrel.h"
#include "engine/r2engine.h"
#include "engine/netmessages.h"
#include "masterserver/masterserver.h"
#include "client/r2client.h"
#include "core/tier0.h"

ADD_SQFUNC("void", NSRequestServerInfo, "string ip, int port, bool requestMods, bool serverAuthUs", "", ScriptContext::UI)
{
	const SQChar* ip = g_pSquirrel<context>->getstring(sqvm, 1);
	SQInteger port = g_pSquirrel<context>->getinteger(sqvm, 2);
	bool requestMods = g_pSquirrel<context>->getbool(sqvm, 3);
	bool serverAuthUs = g_pSquirrel<context>->getbool(sqvm, 4);
	std::string address = fmt::format("[{}]:{}", ip, port);
	netadr_t addr = CNetAdr(address.c_str());

	g_bNextServerAllowingAuthUs = false;
	g_bNextServerAuthUs = false;

	char buffer[256];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);

	if(serverAuthUs)
	{
		g_bNextServerAuthUs = true;
		msg.WriteByte(1);
		msg.WriteString(g_pLocalPlayerUserID);
		msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);
	}
	else
	{
		msg.WriteByte(0);
	}

	g_bListeningforCustomServerInfoPacket = true;

	NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsServerAuthingUs, "", "Returns true if the last requested server is authenticating us.", ScriptContext::UI)
{
	g_pSquirrel<context>->pushbool(sqvm, g_bNextServerAllowingAuthUs);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("string", NSGetNameFromServerInfo, "", "Returns the name from the last received server info packet.", ScriptContext::UI)
{
	g_pSquirrel<context>->pushstring(sqvm, g_szLastServerInfoName);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("float", NSGetLastAuthNotifyTime, "", "Returns the time when the last auth notify was received from the server.", ScriptContext::UI)
{
	auto it = g_LastNotifyTimes.find(NOTIFY_AUTHENTICATED);
	if(it == g_LastNotifyTimes.end())
	{
		g_pSquirrel<context>->pushfloat(sqvm, -1.0f);
		return SQRESULT_NOTNULL;
	}

	g_pSquirrel<context>->pushfloat(sqvm, it->second);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("float", NSGetLastServerInfoTime, "", "Returns the time when the last server info packet was received.", ScriptContext::UI)
{
	float currentTime = Plat_FloatTime();
	g_pSquirrel<context>->pushfloat(sqvm, g_LastReceivedServerInfoTime);
	return SQRESULT_NOTNULL;
}
