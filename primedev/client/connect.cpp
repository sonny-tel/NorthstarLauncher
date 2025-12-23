#include "connect.h"
#include "core/vanilla.h"
#include "client/r2client.h"
#include "masterserver/masterserver.h"
#include "server/auth/serverauthentication.h"
#include "engine/r2engine.h"
#include "core/tier0.h"
#include "squirrel/squirrel.h"

AUTOHOOK_INIT()

void(__fastcall* SCR_BeginLoadingPlaque)(const char* levelName) = nullptr;
void(__fastcall* SCR_EndLoadingPlaque)() = nullptr;

bool g_bConnectingToServer = false;
bool g_bRetryingConnection = false;

ConnectionManager* g_pConnectionManager = nullptr;

void ConnectionManager::Connect(bool useSCRPlaque, std::string mapName)
{
	if(useSCRPlaque)
		SCR_BeginLoadingPlaque(nullptr);

	if(m_bConnecting)
		return;

	m_szMapName = mapName;
	m_bUseSCRPlaque = useSCRPlaque;
	m_bConnecting = true;
	m_eLastMode = m_eCurrentMode;
	m_eCurrentMode = eConnectionMode::LocalServer;

	InvokeConnectionStartCallbacks();

	ConnectToLocalServer();
}

void ConnectionManager::Connect(const std::string& address, const std::string& password, bool useSCRPlaque, std::string mapName)
{
	if(useSCRPlaque)
		SCR_BeginLoadingPlaque(nullptr);

	if(m_bConnecting)
		return;

	m_szMapName = mapName;
	m_bUseSCRPlaque = useSCRPlaque;
	m_bConnecting = true;
	m_eLastMode = m_eCurrentMode;
	m_eCurrentMode = eConnectionMode::RemoteServer;
	m_szLastServerID = address;

	InvokeConnectionStartCallbacks();

	ConnectToRemoteServer(address, password);
}

void ConnectionManager::Connect(const std::string& address, ConnectionManager::eConnectionMode mode, bool useSCRPlaque, std::string mapName)
{
	if(useSCRPlaque)
		SCR_BeginLoadingPlaque(nullptr);

	if(m_bConnecting)
		return;

	m_szMapName = mapName;
	m_bUseSCRPlaque = useSCRPlaque;
	m_bConnecting = true;
	m_eLastMode = m_eCurrentMode;
	m_eCurrentMode = mode;

	if(useSCRPlaque)
		SCR_BeginLoadingPlaque(nullptr);

	InvokeConnectionStartCallbacks();

	std::string ip;
	int port;
	bool isV6;

	if(mode != eConnectionMode::RemoteServer && !ParseAddress(address, ip, port, isV6))
	{
		spdlog::warn("Failed to parse address '{}', aborting connection", address);
		Interrupt("#CONNECTION_FAILED_INVALID_ADDRESS");
		return;
	}

	switch(mode)
	{
	case eConnectionMode::LocalServer:
		ConnectToLocalServer();
		break;
	case eConnectionMode::RemoteServer:
		ConnectToRemoteServer(m_szLastServerID, m_szLastServerPassword);
		break;
	case eConnectionMode::P2P:
		break;
	case eConnectionMode::Direct:
		break;
	default:
		break;
	}
}

void ConnectionManager::InvokeConnectionStartCallbacks()
{
	bool scriptManagedLoading = !m_bUseSCRPlaque;

	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConnectionStarted", scriptManagedLoading);
}

void ConnectionManager::InvokeConnectionMessageCallbacks(const std::string& message)
{
	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConnectionMessage", message.c_str());
}

void ConnectionManager::InvokeConnectionStoppedCallbacks(std::string reason)
{
	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConnectionStopped", reason.c_str());
}

bool ConnectionManager::ParseAddress(const std::string& address, std::string& ip, int& port, bool& isV6)
{
    ip.clear();
    port = -1;
    isV6 = false;

    std::string s = address;

    // trim
    const auto begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos)
        return false;
    const auto end = s.find_last_not_of(" \t");
    s = s.substr(begin, end - begin + 1);

    if (s.empty())
        return false;

    // [v6] or [v6]:port
    if (s.front() == '[')
    {
        const auto close = s.find(']');
        if (close == std::string::npos)
            return false;

        ip = s.substr(1, close - 1);

        if (close + 1 < s.size() && s[close + 1] == ':')
        {
            const std::string portStr = s.substr(close + 2);
            if (!portStr.empty())
            {
                try { port = std::stoi(portStr); }
                catch (...) { port = -1; }
            }
        }
    }
    else
    {
        // host:port (single ':') OR bare v6 / v4
        const auto firstColon = s.find(':');
        const auto lastColon  = s.rfind(':');

        if (firstColon != std::string::npos && firstColon == lastColon)
        {
            ip = s.substr(0, firstColon);
            const std::string portStr = s.substr(firstColon + 1);
            if (!portStr.empty())
            {
                try { port = std::stoi(portStr); }
                catch (...) { port = -1; }
            }
        }
        else
        {
            // bare IP (v4 or v6, or v4-mapped)
            ip = s;
        }
    }

    if (ip.empty())
        return false;

    // Decide if it's real IPv6. Treat ::ffff:a.b.c.d as IPv4 (not v6).
    if (ip.find(':') != std::string::npos)
    {
        const std::string prefix = "::ffff:";
        if (ip.rfind(prefix, 0) == 0)
        {
            std::string rest = ip.substr(prefix.size());
            // v4-mapped form ::ffff:a.b.c.d -> NOT v6
            if (!(rest.find('.') != std::string::npos && rest.find(':') == std::string::npos))
                isV6 = true; // some other IPv6, or non‑standard form
        }
        else
        {
            isV6 = true; // normal IPv6
        }
    }

    return true;
}

ConnectionManager::eConnectionMode ConnectionManager::DetermineModeFromAddress(const std::string& address)
{
	if(address.find("localhost") != std::string::npos)
		return eConnectionMode::LocalServer;

	std::string ip;
	int port;
	bool isV6;

	if(!ParseAddress(address, ip, port, isV6))
		return eConnectionMode::Direct;

	if(isV6)
		return eConnectionMode::P2P;

	return eConnectionMode::Direct;
}

void ConnectionManager::AuthenticateToMasterServer()
{
	if (g_pMasterServerManager->m_bOriginAuthWithMasterServerDone || g_pMasterServerManager->m_bOriginAuthWithMasterServerInProgress)
		return;

	int agreedToSendToken = g_pCVar->FindVar("ns_has_agreed_to_send_token")->GetInt();
	if (agreedToSendToken != AGREED_TO_SEND_TOKEN)
		return;

	UpdateMessage("Authenticating with master server.");

	float startTime = Plat_FloatTime();
	float timeOut = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

	g_pMasterServerManager->AuthenticateOriginWithMasterServer();

	while (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone && Plat_FloatTime() - startTime < timeOut && !IsCancelled())
		Sleep(100);

	RETURN_IF_CANCELLED()

	UpdateMessage();

	if (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone)
		Interrupt("Failed to authenticate with master server.");
	else
		spdlog::info("Successfully authenticated with master server for origin auth");
}

void ConnectionManager::SendInfoRequestPacket(const CNetAdr& addr, bool serverAuthUs, bool requestMods)
{
	char buffer[256];
	bf_write msg(buffer, sizeof(buffer));
	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteChar(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);
	msg.WriteByte(serverAuthUs);

	if(serverAuthUs)
	{
		msg.WriteString(g_pLocalPlayerUserID);
		msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);
	}

	NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	float startTime = Plat_FloatTime();
	float timeOut = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

	UpdateMessage("Requesting server info.");

	while(g_bReceivedServerInfo && !IsCancelled() && Plat_FloatTime() - startTime < timeOut)
	{
		int retryInterval = g_pCVar->FindVar("cl_resend_interval")->GetInt();
		NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);
		Sleep(retryInterval);
	}
}

void ConnectionManager::DownloadMods(bool remoteServer, RemoteServerInfo* info)
{
	int unverifiedModCount = g_pModDownloader->GetTotalServerRequestedMods();

	if(unverifiedModCount)
		Interrupt("Connection failed: unverified mod downloading not implemented yet.");

	int reloadCount = 0;

	for(const auto& mod : info->requiredMods)
	{
		UpdateMessage(fmt::format("Downloading mod {} version {}.", mod.Name.c_str(), mod.Version.c_str()));

		bool found = false;

		for(auto& existingMod : g_pModManager->m_LoadedMods)
		{
			if(existingMod.Name == mod.Name && existingMod.Version == mod.Version)
			{
				found = true;

				if(!existingMod.m_bEnabled)
				{
					reloadCount++;
					existingMod.m_bEnabled = true;
				}

				break;
			}
		}

		if(found)
			continue;

		g_pModDownloader->DownloadMod(mod.Name, mod.Version);

		while(g_pModDownloader->modState.state == ModDownloader::DOWNLOADING && !IsCancelled())
			Sleep(100);

		RETURN_IF_CANCELLED()

		bool downloadFailed = true;
		bool downloadAborted = false;
		std::string interruptMessage;

		switch(g_pModDownloader->modState.state)
		{
			case ModDownloader::DONE:
				downloadFailed = false;
				break;
			case ModDownloader::ABORTED:
				downloadAborted = true;
				downloadFailed = false;
				break;
			case ModDownloader::FAILED: // Generic error message, should be avoided as much as possible
				interruptMessage = "download failed";
				break;
			case ModDownloader::FAILED_READING_ARCHIVE:
				interruptMessage = "failed reading archive";
				break;
			case ModDownloader::FAILED_WRITING_TO_DISK:
				interruptMessage = "failed writing to disk";
				break;
			case ModDownloader::MOD_FETCHING_FAILED:
				interruptMessage = "mod fetching failed";
				break;
			case ModDownloader::MOD_CORRUPTED: // Downloaded archive checksum does not match verified hash
				interruptMessage = "mod corrupted (checksum mismatch)";
				break;
			case ModDownloader::NO_DISK_SPACE_AVAILABLE:
				interruptMessage = "no disk space available";
				break;
			case ModDownloader::NOT_FOUND: // Mod is not currently being auto-downloaded
				interruptMessage = "mod not found";
				break;
			case ModDownloader::UNKNOWN_PLATFORM:
				interruptMessage = "unknown platform";
				break;
			default:
				break;
		}

		if(downloadAborted)
			Interrupt();

		if(downloadFailed)
			Interrupt(fmt::format("Download for {} v{} failed: {}", mod.Name, mod.Version, interruptMessage));

		RETURN_IF_CANCELLED()
	}

	RETURN_IF_CANCELLED()

	if(reloadCount > 0)
		g_pModManager->LoadMods();
}

void ConnectionManager::ConnectToRemoteServer(const std::string& id, const std::string& password)
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	std::string serverId = id;
	std::string serverPassword = password;

	std::thread authThread([&, serverId, serverPassword]()
		{
			AuthenticateToMasterServer();

			RETURN_IF_CANCELLED();

			g_pMasterServerManager->RequestServerList();

			UpdateMessage("Requesting server list.");

			while(g_pMasterServerManager->m_bScriptRequestingServerList && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			spdlog::info("Authenticating with remote server for uid {}", g_pLocalPlayerUserID);

			m_flConnectionStartTime = Plat_FloatTime();
			float maxTime = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

			RemoteServerInfo* serverInfo = nullptr;

			for (auto& server : g_pMasterServerManager->m_vRemoteServers)
			{
				if (std::string(server.id) == serverId)
				{
					serverInfo = &server;
					break;
				}
			}

			if (!serverInfo)
			{
				spdlog::error("Failed to find server info for id {}", serverId);
				Interrupt("Failed to authenticate with remote server: server not found");
				return;
			}

			g_pMasterServerManager->AuthenticateWithServer(
				g_pLocalPlayerUserID,
				g_pMasterServerManager->m_sOwnClientAuthToken,
				*serverInfo,
				serverPassword.c_str());

			UpdateMessage("Authenticating with server.");

			while (!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
				   Plat_FloatTime() - g_pConnectionManager->m_flConnectionStartTime < maxTime && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			if (!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
			{
				spdlog::error("Timed out authenticating with remote server for uid {}", g_pLocalPlayerUserID);
				Interrupt("Failed to authenticate with remote server: timeout");
				return;
			}

			m_bAuthSucessful = true;

			RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;
			g_pCVar->FindVar("serverfilter")->SetValue(info.authToken);

			std::string ip = fmt::format(
				"{}.{}.{}.{}",
				info.ip.S_un.S_un_b.s_b1,
				info.ip.S_un.S_un_b.s_b2,
				info.ip.S_un.S_un_b.s_b3,
				info.ip.S_un.S_un_b.s_b4);
			int port = ntohs(info.port);

			std::string address = fmt::format("{}:{}", ip, port);

			if(!g_pCVar->FindVar("allow_mod_auto_download")->GetBool())
			{
				FinaliseJoiningServer(address);
				return;
			}

			RETURN_IF_CANCELLED()

			std::string netAdr = fmt::format("[::ffff:{}]:{}", ip, port);
			SendInfoRequestPacket(CNetAdr(netAdr.c_str()), false, true);

			RETURN_IF_CANCELLED()

			UpdateMessage("Fetching verified mods manifest.");
			g_pModDownloader->FetchModsListFromAPI();

			while(g_pModDownloader->modState.state == ModDownloader::MANIFESTO_FETCHING && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			DownloadMods(true, serverInfo);
		});

	authThread.detach();
}

void ConnectionManager::FinaliseJoiningServer(std::string& address)
{
	if(m_bRetrying)
	{
		Retrying(false);
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "retry", cmd_source_t::kCommandSrcCode);
	}
	else
	{
		std::string command;

		Cbuf_AddText(
			Cbuf_GetCurrentPlayer(),
			fmt::format("connect {}", address).c_str(),
		 	cmd_source_t::kCommandSrcCode);
	}
}

void ConnectionManager::FinaliseJoiningLocalServer()
{
	if(m_bRetrying)
	{
		Retrying(false);
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "retry", cmd_source_t::kCommandSrcCode);
	}
	else
	{
		std::string command;

		if(!m_szMapName.empty())
			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				fmt::format("map {}", m_szMapName).c_str(),
				cmd_source_t::kCommandSrcCode);
		else
			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				"map mp_lobby",
				cmd_source_t::kCommandSrcCode);
	}
}

void ConnectionManager::ConnectToLocalServer()
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	std::thread authThread([&]()
		{
			AuthenticateToMasterServer();

			RETURN_IF_CANCELLED();

			spdlog::info("Authenticating with own server for uid {}", g_pLocalPlayerUserID);

			m_flConnectionStartTime = Plat_FloatTime();
			float maxTime = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

			g_pMasterServerManager->AuthenticateWithOwnServer(
				g_pLocalPlayerUserID,
				g_pMasterServerManager->m_sOwnClientAuthToken,
				{});

			UpdateMessage("Getting account data.");

			while(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
				  Plat_FloatTime() - g_pConnectionManager->m_flConnectionStartTime < maxTime && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
			{
				spdlog::error("Timed out authenticating with own server for uid {}", g_pLocalPlayerUserID);
				Interrupt("Failed to authenticate with own server: timeout");
				return;
			}

			if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
				g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

			m_bAuthSucessful = true;

			RETURN_IF_CANCELLED()

			FinaliseJoiningLocalServer();
		});

	authThread.detach();
}

// clang-format off
AUTOHOOK(matchmake, engine.dll + 0xF220, int*, __fastcall, ())
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);
	g_pConnectionManager->SetMatchmaking();

	return matchmake();
}

// clang-format off
AUTOHOOK(silentconnect, engine.dll + 0x76F00, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);
	g_pConnectionManager->SetMatchmaking();

	return silentconnect(a1);
}

//clang-format off
AUTOHOOK(Host_Disconnect, engine.dll + 0x15ABE0, void, __fastcall, (bool bShowMainMenu))
//clang-format on
{
	g_pConnectionManager->Finalise();
	g_pConnectionManager->ResetState();

	Host_Disconnect(bShowMainMenu);
}

// clang-format off
AUTOHOOK(CL_FullyConnected, engine.dll + 0x72D20, int, __fastcall, ())
// clang-format on
{
	g_pConnectionManager->Finalise();
	g_pConnectionManager->ResetState();

	return CL_FullyConnected();
}

// clang-format off
AUTOHOOK(retry, engine.dll + 0x73D10, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_bRetryingConnection = true;
	g_pConnectionManager->Retrying(true);
	return retry(a1);
}

// clang-format off
AUTOHOOK(concommand_connect, engine.dll + 0x76720, __int64, __fastcall, (const CCommand* args))
// clang-format on
{
	return concommand_connect(args);
}

// clang-format off
AUTOHOOK(connectWithKey, engine.dll + 0x768C0, int*, __fastcall, (const CCommand* args))
// clang-format on
{
	if(!g_pConnectionManager->IsConnecting())
	{
		if(g_pVanillaCompatibility->GetVanillaCompatibility())
		{
			g_pConnectionManager->SetMatchmaking();
			return connectWithKey(args);
		}

		const char* address = args->Arg(1);

		auto mode = g_pConnectionManager->DetermineModeFromAddress(address);
		if(g_pConnectionManager->IsRetrying())
			mode = g_pConnectionManager->GetCurrentMode();
		else
			spdlog::info("Determined connection mode from address '{}': {}", address, static_cast<int>(mode));

		int argCount = args->ArgC();
		bool useSCRPlaque = true;

		if(argCount == 4)
			atoi(args->Arg(3)) == 0 ? useSCRPlaque = false : useSCRPlaque = true;

		g_pConnectionManager->Connect(address, mode, useSCRPlaque);

		return 0;
	}

	return connectWithKey(args);

	// std::string mode = g_pCVar->FindVar("ns_server_auth_mode")->GetString();

	// std::string address = args->Arg(1);

	// std::string ipPart;
	// int port;
	// bool isV6 = false;

	// if(!g_pConnectionManager->ParseAddress(address, ipPart, port, isV6))
	// 	return connectWithKey(args);

	// spdlog::info("Parsed connect address: ip='{}', port={}, isV6={}", ipPart, port, isV6 ? "true" : "false");


	// if(mode == "matchmaking" || !g_bRetryingConnection)
	// {
	// 	g_pCVar->FindVar("ns_server_auth_mode")->SetValue("direct");
	// 	return connectWithKey(args);
	// }

	// g_bRetryingConnection = false;

	// std::string localUid = g_pLocalPlayerUserID;

	// CCommand copyArgs = CCommand(*args);

	// if(mode == localUid && !g_bConnectingToServer)
	// {
	// 	SCR_BeginLoadingPlaque(nullptr);
	// 	std::thread authThread(
	// 		[localUid, copyArgs]()
	// 		{
	// 			float startTime = Plat_FloatTime();

	// 			g_pMasterServerManager->AuthenticateWithOwnServer(
	// 				g_pLocalPlayerUserID,
	// 				g_pMasterServerManager->m_sOwnClientAuthToken,
	// 				{});

	// 			while(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
	// 				  Plat_FloatTime() - startTime < 10.0f)
	// 				Sleep(100);

	// 			if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
	// 			{
	// 				spdlog::error("Timed out authenticating with own server for uid {}", localUid);
	// 				return;
	// 			}

	// 			spdlog::info("Successfully authenticated with own server for uid {}", localUid);

	// 			if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
	// 				g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

	// 			std::string arg1 = copyArgs.Arg(1);
	// 			std::string arg2 = copyArgs.Arg(2);
	// 			g_bConnectingToServer = true;

	// 			Cbuf_AddText(
	// 				Cbuf_GetCurrentPlayer(),
	// 				fmt::format("connectWithKey {} {}", arg1, arg2).c_str(), cmd_source_t::kCommandSrcCode);
	// 		});

	// 	authThread.detach();
	// 	return 0;
	// }

	// int index = -1;

	// for(int i = 0; i < g_pMasterServerManager->m_vRemoteServers.size(); i++)
	// {
	// 	if(std::string(g_pMasterServerManager->m_vRemoteServers[i].id) == mode)
	// 	{
	// 		index = i;
	// 		break;
	// 	}
	// }

	// if(!mode.empty() && index != -1 && !g_bConnectingToServer)
	// {
	// 	SCR_BeginLoadingPlaque(nullptr);
	// 	std::thread authThread(
	// 		[mode, copyArgs, index]()
	// 		{
	// 			float startTime = Plat_FloatTime();

    //             g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer = false;
    //             g_pMasterServerManager->m_bHasPendingConnectionInfo = false;

	// 			const char* password = g_pCVar->FindVar("ns_last_server_password")->GetString();

	// 			g_pMasterServerManager->AuthenticateWithServer(
	// 				g_pLocalPlayerUserID,
	// 				g_pMasterServerManager->m_sOwnClientAuthToken,
	// 				g_pMasterServerManager->m_vRemoteServers[index],
	// 				password);

	// 			while(!g_pMasterServerManager->m_bHasPendingConnectionInfo &&
	// 				  Plat_FloatTime() - startTime < 10.0f)
	// 				Sleep(100);

	// 			if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
	// 			{
	// 				spdlog::error("Timed out authenticating with server for uid {}", mode);
	// 				return;
	// 			}

	// 			spdlog::info("Successfully authenticated with server for uid {}", mode);

	// 			RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;
	// 			g_pCVar->FindVar("serverfilter")->SetValue(info.authToken);

	// 			g_bConnectingToServer = true;

	// 			Cbuf_AddText(
	// 				Cbuf_GetCurrentPlayer(),
	// 				fmt::format(
	// 					"connect {}.{}.{}.{}:{}",
	// 					info.ip.S_un.S_un_b.s_b1,
	// 					info.ip.S_un.S_un_b.s_b2,
	// 					info.ip.S_un.S_un_b.s_b3,
	// 					info.ip.S_un.S_un_b.s_b4,
	// 					info.port)
	// 					.c_str(),
	// 				cmd_source_t::kCommandSrcCode);
	// 		});

	// 	authThread.detach();
	// 	return 0;
	// }

	// if(!g_bConnectingToServer)
	// {
	// 	SCR_BeginLoadingPlaque(nullptr);
	// 	std::thread authThread(
	// 		[mode, copyArgs]()
	// 		{
	// 		char buffer[256];

	// 		bf_write msg(buffer, sizeof(buffer));
	// 		msg.WriteLong(CONNECTIONLESS_HEADER);
	// 		msg.WriteChar(A2S_REQUESTCUSTOMSERVERINFO);
	// 		msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	// 		msg.WriteByte(false);
	// 		msg.WriteLong(MODDOWNLOADINFO_VERSION);
	// 		msg.WriteByte(1);
	// 		msg.WriteString(g_pLocalPlayerUserID);
	// 		msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);

	// 		std::string arg1 = copyArgs.Arg(1);
	// 		std::string arg2 = copyArgs.Arg(2);
	// 		netadr_t addr;

	// 		if(arg1.find(']') != std::string::npos)
	// 			addr = CNetAdr(arg1.c_str());
	// 		else if(arg1.find(':') != std::string::npos)
	// 		{
	// 			std::string ip = arg1.substr(0, arg1.find(':'));
	// 			std::string portStr = arg1.substr(arg1.find(':') + 1);
	// 			int port = std::stoi(portStr);
	// 			std::string address = fmt::format("[::ffff:{}]:{}", ip, port);
	// 			addr = CNetAdr(address.c_str());
	// 		}

	// 		g_bNextServerAuthUs = true;

	// 		NET_SendPacket(
	// 			nullptr,
	// 			NS_CLIENT,
	// 			&addr,
	// 			msg.GetData(),
	// 			msg.GetNumBytesWritten(),
	// 			nullptr,
	// 			false,
	// 			0,
	// 			true);

	// 		float startListeningTime = Plat_FloatTime();

	// 		while(!g_bReceivedServerInfo && Plat_FloatTime() - startListeningTime < 2.5f)
	// 			Sleep(100);

	// 		g_bNextServerAuthUs = false;

	// 		if(!g_bNextServerAllowingAuthUs)
	// 		{
	// 			spdlog::warn("Timed out waiting for server info response from server");

	// 			g_bConnectingToServer = true;

	// 			Cbuf_AddText(
	// 				Cbuf_GetCurrentPlayer(),
	// 				fmt::format("connectWithKey {} {}", arg1, arg2).c_str(), cmd_source_t::kCommandSrcCode);
	// 			return;
	// 		}

	// 		startListeningTime = Plat_FloatTime();

	// 		while(!g_bReceivedAuthNotify && Plat_FloatTime() - startListeningTime < 5.0f)
	// 			Sleep(100);

	// 		if(!g_bReceivedAuthNotify)
	// 			spdlog::warn("Timed out waiting for authentication notify from server");

	// 		g_bConnectingToServer = true;

	// 		Cbuf_AddText(
	// 			Cbuf_GetCurrentPlayer(),
	// 			fmt::format("connectWithKey {} {}", arg1, arg2).c_str(), cmd_source_t::kCommandSrcCode);
	// 		});
	// 	authThread.detach();
	// 	return 0;
	// }

	// g_bConnectingToServer = false;

	// return connectWithKey(args);
}

void ConCommand_connectWithRemoteId(const CCommand& args)
{
	if(args.ArgC() < 3)
	{
		spdlog::warn("connectWithRemoteId called with insufficient arguments");
		return;
	}

	std::string remoteId = args.Arg(1);
	std::string password = args.Arg(2);

	bool useSCRPlaque = true;

	if(args.ArgC() == 4)
		atoi(args.Arg(3)) == 0 ? useSCRPlaque = false : useSCRPlaque = true;

	if(password == "0")
		password = "";

	g_pConnectionManager->Connect(remoteId, password, useSCRPlaque);
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ConnectHooks, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();

	g_pConnectionManager = new ConnectionManager();

	RegisterConCommand("connectWithRemoteId", ConCommand_connectWithRemoteId, "Connects to a server using its remote ID from the master server", FCVAR_CLIENTDLL);

	SCR_BeginLoadingPlaque = module.Offset(0xB92E0).RCast<decltype(SCR_BeginLoadingPlaque)>();
	SCR_EndLoadingPlaque = module.Offset(0xB9470).RCast<decltype(SCR_EndLoadingPlaque)>();
}
