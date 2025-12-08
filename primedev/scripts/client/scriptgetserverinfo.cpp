#include "squirrel/squirrel.h"
#include "engine/r2engine.h"
#include "engine/netmessages.h"

ADD_SQFUNC("void", NSRequestServerInfo, "string ipv4, int port, bool requestMods", "", ScriptContext::UI)
{
	const SQChar* ip = g_pSquirrel<context>->getstring(sqvm, 1);
	SQInteger port = g_pSquirrel<context>->getinteger(sqvm, 2);
	bool requestMods = g_pSquirrel<context>->getbool(sqvm, 3);

	netadr_t addr = CNetAdr();
	in6_addr inAddr = {};
	inet_pton(AF_INET, ip, &inAddr);
	addr.SetIP(&inAddr);
	addr.SetPort((uint16_t)port);
	addr.SetType(netadrtype_t::NA_IP);

	char buffer[64];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);

	NET_SendPacket(nullptr, NS_CLIENT, addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, false);

	return SQRESULT_NULL;
}
