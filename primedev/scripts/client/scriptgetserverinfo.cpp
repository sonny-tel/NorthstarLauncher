#include "squirrel/squirrel.h"
#include "engine/r2engine.h"
#include "engine/netmessages.h"

ADD_SQFUNC("void", NSRequestServerInfo, "string ip, int port, bool requestMods", "", ScriptContext::UI)
{
	const SQChar* ip = g_pSquirrel<context>->getstring(sqvm, 1);
	SQInteger port = g_pSquirrel<context>->getinteger(sqvm, 2);
	bool requestMods = g_pSquirrel<context>->getbool(sqvm, 3);
	std::string address = fmt::format("[{}]:{}", ip, port);
	netadr_t addr = CNetAdr(address.c_str());

	char buffer[64];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);

	NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	return SQRESULT_NULL;
}
