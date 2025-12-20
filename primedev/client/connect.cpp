#include "connect.h"
#include "core/vanilla.h"
#include "client/r2client.h"
#include "masterserver/masterserver.h"
#include "server/auth/serverauthentication.h"
#include "engine/r2engine.h"
#include "core/tier0.h"

AUTOHOOK_INIT()

bool g_bConnectingToServer = false;

// clang-format off
AUTOHOOK(matchmake, engine.dll + 0xF220, int*, __fastcall, ())
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);
	g_pCVar->FindVar("ns_server_auth_mode")->SetValue("matchmaking");

	return matchmake();
}

// clang-format off
AUTOHOOK(silentconnect, engine.dll + 0x76F00, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);
	g_pCVar->FindVar("ns_server_auth_mode")->SetValue("matchmaking");

	return silentconnect(a1);
}

// vanilla only uses connectwithtoken and silentconnect, this is actually pretty good for us to differentiate
// clang-format off
AUTOHOOK(concommand_connect, engine.dll + 0x76720, __int64, __fastcall, (const CCommand* args))
// clang-format on
{
	spdlog::info("Connect command called");

	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);
	std::string mode = g_pCVar->FindVar("ns_server_auth_mode")->GetString();

	if(mode == "matchmaking")
		return concommand_connect(args);

	std::string localUid = g_pLocalPlayerUserID;

	CCommand copyArgs = *args;

	if(mode == localUid && !g_bConnectingToServer)
	{
		std::thread authThread(
			[localUid, copyArgs]()
			{
				float startTime = Plat_FloatTime();

				spdlog::info("Authenticating with server for uid {}", localUid);
				g_pMasterServerManager->AuthenticateWithOwnServer(
					g_pLocalPlayerUserID,
					g_pMasterServerManager->m_sOwnClientAuthToken,
					{});

				while(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
					  Plat_FloatTime() - startTime < 10.0f)
					Sleep(100);

				if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
				{
					spdlog::error("Timed out authenticating with own server for uid {}", localUid);
					return;
				}

				spdlog::info("Successfully authenticated with own server for uid {}", localUid);

				if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
					g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

				std::string arg1 = copyArgs.Arg(1);
				g_bConnectingToServer = true;

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					fmt::format("connect {}", arg1).c_str(), cmd_source_t::kCommandSrcCode);
			});

		authThread.detach();
		return 0;
	}

	int index = -1;

	for(int i = 0; i < g_pMasterServerManager->m_vRemoteServers.size(); i++)
	{
		if(std::string(g_pMasterServerManager->m_vRemoteServers[i].id) == mode)
		{
			index = i;
			break;
		}
	}

	if(!mode.empty() && index != -1 && !g_bConnectingToServer)
	{
		std::thread authThread(
			[mode, copyArgs, index]()
			{
				float startTime = Plat_FloatTime();

                g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer = false;
                g_pMasterServerManager->m_bHasPendingConnectionInfo = false;

				g_pMasterServerManager->AuthenticateWithServer(
					g_pLocalPlayerUserID,
					g_pMasterServerManager->m_sOwnClientAuthToken,
					g_pMasterServerManager->m_vRemoteServers[index],
					"");

				while(!g_pMasterServerManager->m_bHasPendingConnectionInfo && Plat_FloatTime() - startTime < 10.0f)
					Sleep(100);

				if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
				{
					spdlog::error("Timed out authenticating with server for uid {}", mode);
					return;
				}

				spdlog::info("Successfully authenticated with server for uid {}", mode);

				RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;
				g_pCVar->FindVar("serverfilter")->SetValue(info.authToken);

				g_bConnectingToServer = true;

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					fmt::format(
						"connect {}.{}.{}.{}:{}",
						info.ip.S_un.S_un_b.s_b1,
						info.ip.S_un.S_un_b.s_b2,
						info.ip.S_un.S_un_b.s_b3,
						info.ip.S_un.S_un_b.s_b4,
						info.port)
						.c_str(),
					cmd_source_t::kCommandSrcCode);
			});

		authThread.detach();
		return 0;
	}

	if(!mode.empty() && !g_bConnectingToServer)
	{
		std::thread authThread(
			[mode, copyArgs]()
			{
			char buffer[256];

			bf_write msg(buffer, sizeof(buffer));
			msg.WriteLong(CONNECTIONLESS_HEADER);
			msg.WriteChar(A2S_REQUESTCUSTOMSERVERINFO);
			msg.WriteLong(CUSTOMSERVERINFO_VERSION);
			msg.WriteByte(false);
			msg.WriteLong(MODDOWNLOADINFO_VERSION);
			msg.WriteByte(1);
			msg.WriteString(g_pLocalPlayerUserID);
			msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);

			std::string arg1 = copyArgs.Arg(1);
			netadr_t addr;

			if(arg1.find(']') != std::string::npos)
				addr = CNetAdr(arg1.c_str());
			else if(arg1.find(':') != std::string::npos)
			{
				std::string ip = arg1.substr(0, arg1.find(':'));
				std::string portStr = arg1.substr(arg1.find(':') + 1);
				int port = std::stoi(portStr);
				std::string address = fmt::format("[::ffff:{}]:{}", ip, port);
				addr = CNetAdr(address.c_str());
			}

			g_bNextServerAuthUs = true;

			NET_SendPacket(
				nullptr,
				NS_CLIENT,
				&addr,
				msg.GetData(),
				msg.GetNumBytesWritten(),
				nullptr,
				false,
				0,
				true);

			float startListeningTime = Plat_FloatTime();

			while(g_LastReceivedServerInfoTime > startListeningTime && Plat_FloatTime() - startListeningTime < 5.0f)
				Sleep(100);

			g_bNextServerAuthUs = false;

			if(!g_bNextServerAllowingAuthUs)
			{
				spdlog::warn("Timed out waiting for server info response from server");

				g_bConnectingToServer = true;

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					fmt::format("connect {}", arg1).c_str(), cmd_source_t::kCommandSrcCode);
				return;
			}

			startListeningTime = Plat_FloatTime();
			float notifyTime = -1.0f;

			while(notifyTime > startListeningTime && Plat_FloatTime() - startListeningTime < 5.0f)
			{
				auto it = g_LastNotifyTimes.find(NOTIFY_AUTHENTICATED);
				if(it != g_LastNotifyTimes.end())
					notifyTime = it->second;

				Sleep(100);
			}

			if(notifyTime < startListeningTime)
				spdlog::warn("Timed out waiting for authentication notify from server");

			g_bConnectingToServer = true;

			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				fmt::format("connect {}", arg1).c_str(), cmd_source_t::kCommandSrcCode);
			});
		authThread.detach();
		return 0;
	}

	g_bConnectingToServer = false;

	return concommand_connect(args);
}

// clang-format off
AUTOHOOK(connectWithKey, engine.dll + 0x768C0, int*, __fastcall, (const CCommand* args))
// clang-format on
{
	std::string mode = g_pCVar->FindVar("ns_server_auth_mode")->GetString();

	if(mode == "matchmaking")
		return connectWithKey(args);

	std::string localUid = g_pLocalPlayerUserID;

	CCommand copyArgs = CCommand(*args);

	if(mode == localUid && !g_bConnectingToServer)
	{
		std::thread authThread(
			[localUid, copyArgs]()
			{
				float startTime = Plat_FloatTime();

				g_pMasterServerManager->AuthenticateWithOwnServer(
					g_pLocalPlayerUserID,
					g_pMasterServerManager->m_sOwnClientAuthToken,
					{});

				while(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
					  Plat_FloatTime() - startTime < 10.0f)
					Sleep(100);

				if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
				{
					spdlog::error("Timed out authenticating with own server for uid {}", localUid);
					return;
				}

				spdlog::info("Successfully authenticated with own server for uid {}", localUid);

				if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
					g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

				std::string arg1 = copyArgs.Arg(1);
				std::string arg2 = copyArgs.Arg(2);
				g_bConnectingToServer = true;

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					fmt::format("connectWithKey {} {}", arg1, arg2).c_str(), cmd_source_t::kCommandSrcCode);
			});

		authThread.detach();
		return 0;
	}

	int index = -1;

	for(int i = 0; i < g_pMasterServerManager->m_vRemoteServers.size(); i++)
	{
		if(std::string(g_pMasterServerManager->m_vRemoteServers[i].id) == mode)
		{
			index = i;
			break;
		}
	}

	if(!mode.empty() && index != -1 && !g_bConnectingToServer)
	{
		std::thread authThread(
			[mode, copyArgs, index]()
			{
				float startTime = Plat_FloatTime();

                g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer = false;
                g_pMasterServerManager->m_bHasPendingConnectionInfo = false;

				g_pMasterServerManager->AuthenticateWithServer(
					g_pLocalPlayerUserID,
					g_pMasterServerManager->m_sOwnClientAuthToken,
					g_pMasterServerManager->m_vRemoteServers[index],
					"");

				while(!g_pMasterServerManager->m_bHasPendingConnectionInfo &&
					  Plat_FloatTime() - startTime < 10.0f)
					Sleep(100);

				if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
				{
					spdlog::error("Timed out authenticating with server for uid {}", mode);
					return;
				}

				spdlog::info("Successfully authenticated with server for uid {}", mode);

				RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;
				g_pCVar->FindVar("serverfilter")->SetValue(info.authToken);

				g_bConnectingToServer = true;

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					fmt::format(
						"connect {}.{}.{}.{}:{}",
						info.ip.S_un.S_un_b.s_b1,
						info.ip.S_un.S_un_b.s_b2,
						info.ip.S_un.S_un_b.s_b3,
						info.ip.S_un.S_un_b.s_b4,
						info.port)
						.c_str(),
					cmd_source_t::kCommandSrcCode);
			});

		authThread.detach();
		return 0;
	}

	if(!mode.empty() && !g_bConnectingToServer)
	{
		std::thread authThread(
			[mode, copyArgs]()
			{
			char buffer[256];

			bf_write msg(buffer, sizeof(buffer));
			msg.WriteLong(CONNECTIONLESS_HEADER);
			msg.WriteChar(A2S_REQUESTCUSTOMSERVERINFO);
			msg.WriteLong(CUSTOMSERVERINFO_VERSION);
			msg.WriteByte(false);
			msg.WriteLong(MODDOWNLOADINFO_VERSION);
			msg.WriteByte(1);
			msg.WriteString(g_pLocalPlayerUserID);
			msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);

			std::string arg1 = copyArgs.Arg(1);
			std::string arg2 = copyArgs.Arg(2);
			netadr_t addr;

			if(arg1.find(']') != std::string::npos)
				addr = CNetAdr(arg1.c_str());
			else if(arg1.find(':') != std::string::npos)
			{
				std::string ip = arg1.substr(0, arg1.find(':'));
				std::string portStr = arg1.substr(arg1.find(':') + 1);
				int port = std::stoi(portStr);
				std::string address = fmt::format("[::ffff:{}]:{}", ip, port);
				addr = CNetAdr(address.c_str());
			}

			g_bNextServerAuthUs = true;

			NET_SendPacket(
				nullptr,
				NS_CLIENT,
				&addr,
				msg.GetData(),
				msg.GetNumBytesWritten(),
				nullptr,
				false,
				0,
				true);

			float startListeningTime = Plat_FloatTime();

			while(g_LastReceivedServerInfoTime > startListeningTime && Plat_FloatTime() - startListeningTime < 5.0f)
				Sleep(100);

			g_bNextServerAuthUs = false;

			if(!g_bNextServerAllowingAuthUs)
			{
				spdlog::warn("Timed out waiting for server info response from server");

				g_bConnectingToServer = true;

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					fmt::format("connectWithKey {} {}", arg1, arg2).c_str(), cmd_source_t::kCommandSrcCode);
				return;
			}

			startListeningTime = Plat_FloatTime();
			float notifyTime = -1.0f;

			while(notifyTime > startListeningTime && Plat_FloatTime() - startListeningTime < 5.0f)
			{
				auto it = g_LastNotifyTimes.find(NOTIFY_AUTHENTICATED);
				if(it != g_LastNotifyTimes.end())
					notifyTime = it->second;

				Sleep(100);
			}

			if(notifyTime < startListeningTime)
				spdlog::warn("Timed out waiting for authentication notify from server");

			g_bConnectingToServer = true;

			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				fmt::format("connectWithKey {} {}", arg1, arg2).c_str(), cmd_source_t::kCommandSrcCode);
			});
		authThread.detach();
		return 0;
	}

	g_bConnectingToServer = false;

	return connectWithKey(args);
}

ON_DLL_LOAD_RELIESON("engine.dll", ConnectHooks, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
