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

void ConnectionManager::Connect(ConnectionManager::eConnectionMode mode, bool useSCRPlaque, std::string mapName)
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

	InvokeConnectionStartCallbacks();

	ConnectToLocalServer();
}

void ConnectionManager::InvokeConnectionStartCallbacks()
{
	bool scriptManagedLoading = !m_bUseSCRPlaque;

	g_pSquirrel[ScriptContext::UI]->Call("NSUICodeCallback_ConnectionStarted", scriptManagedLoading);
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

void ConnectionManager::AuthenticateToMasterServer()
{
	if (g_pMasterServerManager->m_bOriginAuthWithMasterServerDone || g_pMasterServerManager->m_bOriginAuthWithMasterServerInProgress)
		return;

	int agreedToSendToken = g_pCVar->FindVar("ns_has_agreed_to_send_token")->GetInt();
	if (agreedToSendToken != AGREED_TO_SEND_TOKEN)
		return;

	float startTime = Plat_FloatTime();
	float timeOut = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

	g_pMasterServerManager->AuthenticateOriginWithMasterServer();

	while (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone && Plat_FloatTime() - startTime < timeOut && !IsCancelled())
		Sleep(100);

	if(IsCancelled())
		return Cancel();

	if (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone)
		spdlog::error("Timed out authenticating with master server for origin auth");
	else
		spdlog::info("Successfully authenticated with master server for origin auth");
}

void ConnectionManager::ConnectToLocalServer()
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	std::thread authThread([&]()
		{
			AuthenticateToMasterServer();

			if(IsCancelled())
				return Cancel();

			spdlog::info("Authenticating with own server for uid {}", g_pLocalPlayerUserID);

			m_flConnectionStartTime = Plat_FloatTime();
			float maxTime = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

			g_pMasterServerManager->AuthenticateWithOwnServer(
				g_pLocalPlayerUserID,
				g_pMasterServerManager->m_sOwnClientAuthToken,
				{});

			while(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
				  Plat_FloatTime() - g_pConnectionManager->m_flConnectionStartTime < maxTime && !IsCancelled())
				Sleep(100);

			if(IsCancelled())
				return Cancel();

			if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
			{
				spdlog::error("Timed out authenticating with own server for uid {}", g_pLocalPlayerUserID);
				SetFailed("Failed to authenticate with own server: timeout");
				return;
			}

			if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
				g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

			m_bAuthSucessful = true;

			if(m_bRetrying)
				Cbuf_AddText(Cbuf_GetCurrentPlayer(), "retry", cmd_source_t::kCommandSrcCode);
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
		});

	authThread.detach();
}

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

// clang-format off
AUTOHOOK(retry, engine.dll + 0x73D10, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_bRetryingConnection = true;
	return retry(a1);
}

// vanilla only uses connectwithtoken and silentconnect, this is actually pretty good for us to differentiate
// clang-format off
AUTOHOOK(concommand_connect, engine.dll + 0x76720, __int64, __fastcall, (const CCommand* args))
// clang-format on
{
	return concommand_connect(args);

	// spdlog::info("Connect command called");

	// g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);
	// std::string mode = g_pCVar->FindVar("ns_server_auth_mode")->GetString();

	// std::string address = args->Arg(1);

	// std::string ipPart;
	// int port;
	// bool isV6 = false;

	// if(!g_pConnectionManager->ParseAddress(address, ipPart, port, isV6))
	// 	return concommand_connect(args);

	// spdlog::info("Parsed connect address: ip='{}', port={}, isV6={}", ipPart, port, isV6 ? "true" : "false");

	// if(mode == "matchmaking" || !g_bRetryingConnection)
	// {
	// 	g_pCVar->FindVar("ns_server_auth_mode")->SetValue("direct");
	// 	return concommand_connect(args);
	// }

	// g_bRetryingConnection = false;

	// std::string localUid = g_pLocalPlayerUserID;

	// CCommand copyArgs = *args;

	// if(mode == localUid && !g_bConnectingToServer)
	// {
	// 	SCR_BeginLoadingPlaque(nullptr);
	// 	std::thread authThread(
	// 		[localUid, copyArgs]()
	// 		{
	// 			float startTime = Plat_FloatTime();

	// 			spdlog::info("Authenticating with server for uid {}", localUid);
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
	// 			g_bConnectingToServer = true;

	// 			Cbuf_AddText(
	// 				Cbuf_GetCurrentPlayer(),
	// 				fmt::format("connect {}", arg1).c_str(), cmd_source_t::kCommandSrcCode);
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

	// 			while(!g_pMasterServerManager->m_bHasPendingConnectionInfo && Plat_FloatTime() - startTime < 10.0f)
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

	// if(!mode.empty() && !g_bConnectingToServer)
	// {
	// 	SCR_BeginLoadingPlaque(nullptr);
	// 	std::thread authThread(
	// 		[mode, copyArgs]()
	// 		{

	// 		char dummyBuf[32];
	// 		bf_write pkt(dummyBuf, sizeof(dummyBuf));
	// 		pkt.WriteLong(CONNECTIONLESS_HEADER);
	// 		// send

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

	// 		while(!g_bReceivedServerInfo && Plat_FloatTime() - startListeningTime < 5.0f)
	// 			Sleep(100);

	// 		g_bNextServerAuthUs = false;

	// 		if(!g_bNextServerAllowingAuthUs)
	// 		{
	// 			spdlog::warn("Timed out waiting for server info response from server");

	// 			g_bConnectingToServer = true;

	// 			Cbuf_AddText(
	// 				Cbuf_GetCurrentPlayer(),
	// 				fmt::format("connect {}", arg1).c_str(), cmd_source_t::kCommandSrcCode);
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
	// 			fmt::format("connect {}", arg1).c_str(), cmd_source_t::kCommandSrcCode);
	// 		});
	// 	authThread.detach();
	// 	return 0;
	// }

	// g_bConnectingToServer = false;

	// return concommand_connect(args);
}

// clang-format off
AUTOHOOK(connectWithKey, engine.dll + 0x768C0, int*, __fastcall, (const CCommand* args))
// clang-format on
{
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

ON_DLL_LOAD_RELIESON("engine.dll", ConnectHooks, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();

	g_pConnectionManager = new ConnectionManager();

	SCR_BeginLoadingPlaque = module.Offset(0xB92E0).RCast<decltype(SCR_BeginLoadingPlaque)>();
	SCR_EndLoadingPlaque = module.Offset(0xB9470).RCast<decltype(SCR_EndLoadingPlaque)>();
}
