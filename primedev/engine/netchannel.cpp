#include "netchannel.h"
#include "inetmessage.h"
#include "dedicated/dedicated.h"
#include "r2engine.h"
#include "mods/autodownload/moddownloader.h"

AUTOHOOK_INIT()

std::vector<std::pair<std::string, int>> g_DebugInfoRegisteredNetMessages;

CNetChan__RegisterMessage_t CNetChan__RegisterMessage = nullptr;

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

// clang-format off
AUTOHOOK(CNetChan__SendNetMsg, engine.dll + 0x213270, bool, __fastcall, (CNetChan* thisptr, INetMessage* msg, bool bForceReliable, bool bVoice))
// clang-format on
{
	if( IsDedicatedServer() || *g_pServerState )
		return CNetChan__SendNetMsg(thisptr, msg, bForceReliable, bVoice);

	if(msg->GetType() == static_cast<int>(NetMessageType::net_SignonState))
	{
		if(g_pModDownloader->GetUserAcceptedServerModsState() >= AcceptedServerModState::PENDING)
		{
			if(!g_pModDownloader->m_bHasUserBeenPrompted)
			{
				g_pModDownloader->m_bHasUserBeenPrompted = true;
				std::thread([=]()
				{
					g_pModDownloader->PromptUserConfirmation();
				}).detach();
			}

			return true;
		}
	}

	return CNetChan__SendNetMsg(thisptr, msg, bForceReliable, bVoice);
}

void ConCommand_ns_dump_registered_netmessages(const CCommand& args)
{
	spdlog::info("Dumping {} registered netmessages:", g_DebugInfoRegisteredNetMessages.size());

	spdlog::info("enum NetMessageType");
	spdlog::info("{");

	int largestIndex = -1;

	for (const auto& [name, type] : g_DebugInfoRegisteredNetMessages)
	{
		spdlog::info("    {} = {},", name, type);
		if (type > largestIndex)
			largestIndex = type;
	}

	spdlog::info("    __NEXT_INDEX__ = {},", largestIndex + 1);

	spdlog::info("};");
}

ON_DLL_LOAD_RELIESON("engine.dll", NetChan, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();

	RegisterConCommand("ns_dump_registered_netmessages", ConCommand_ns_dump_registered_netmessages, "Dumps registered netmessages", FCVAR_NONE);

	CNetChan__RegisterMessage = module.Offset(0x2129D0).RCast<CNetChan__RegisterMessage_t>();
}
