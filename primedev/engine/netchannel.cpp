#include "netchannel.h"
#include "inetmessage.h"

AUTOHOOK_INIT()

std::vector<std::pair<std::string, int>> g_DebugInfoRegisteredNetMessages;

// clang-format off
AUTOHOOK(CNetChan_RegisterMessage, engine.dll + 0x2129D0, bool, __fastcall, (CNetChan* thisptr, INetMessage* msg))
// clang-format on
{
	if (CNetChan_RegisterMessage(thisptr, msg))
	{
		spdlog::info("Registered netmessage: {} (type: {}, reliable: {}, size: {})",
			msg->GetName(),
			msg->GetType(),
			msg->IsReliable() ? "yes" : "no",
			msg->GetSize());

        auto it = std::find_if(g_DebugInfoRegisteredNetMessages.begin(), g_DebugInfoRegisteredNetMessages.end(),
            [msg](const auto& pair) {
                return pair.first == msg->GetName() && pair.second == msg->GetType();
            });

        if (it == g_DebugInfoRegisteredNetMessages.end())
            g_DebugInfoRegisteredNetMessages.emplace_back(msg->GetName(), msg->GetType());

		return true;
	}

	return false;
}

void ConCommand_ns_dump_registered_netmessages(const CCommand& args)
{
	spdlog::info("Dumping {} registered netmessages:", g_DebugInfoRegisteredNetMessages.size());

	spdlog::info("enum NetMessageType\n{");

	for (const auto& [name, type] : g_DebugInfoRegisteredNetMessages)
		spdlog::info("    {} = {},", name, type);

	spdlog::info("};");
}

ON_DLL_LOAD_RELIESON("engine.dll", NetChan, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();

	RegisterConCommand("ns_dump_registered_netmessages", ConCommand_ns_dump_registered_netmessages, "Dumps registered netmessages", FCVAR_NONE);
}
