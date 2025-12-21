#pragma once

#include "engine/net.h"
#include "masterserver/masterserver.h"

extern bool g_bConnectingToServer;
extern bool g_bRetryingConnection;

// mirrored in script
const int NOT_DECIDED_TO_SEND_TOKEN = 0;
const int AGREED_TO_SEND_TOKEN = 1;
const int DISAGREED_TO_SEND_TOKEN = 2;

class ConnectionManager
{
public:
	enum class eConnectionMode
	{
		Direct = 0, // unauthenticated direct connect
		Matchmaking = 'V', // vanilla matchmaking
		LocalServer = 'L', // listen server AuthWithOwnServer
		ServerBrowser = 'N', // server browser AuthWithServer
		P2P = 'P', // connectionless self auth hack only for p2p
	};

private:
	eConnectionMode m_eCurrentMode = eConnectionMode::Direct;
	eConnectionMode m_eLastMode = eConnectionMode::Direct;
	bool m_bRetrying = false; // if we're retrying we need to check if we should use connectWithKey
	int m_nAttemptCount = 0;
	bool m_bIncomingP2PServerAuthenticating = false;
	std::string m_szFailReason;
	std::string m_szProgressMessage;
	bool m_bFailed = false;
	bool m_bUseSCRPlaque = false; // whether we're expecting script to show progress or use LoadingProgress from BaseModUI
	bool m_bConnecting = false;
	float m_flConnectionStartTime = 0.0f;
	bool m_bAuthSucessful = false;

	void ConnectToLocalServer();
	void ConnectToNorthstarServer(const std::string& address);
	void ConnectToVanillaMatchmakingServer(const std::string& address);
	void ConnectToP2PServer(const std::string& address);
	void SendInfoRequestPacket(const CNetAdr& addr, bool serverAuthUs, bool requestMods);
	void HandleModDownloads();
	void ResetState()
	{
		m_bFailed = false;
		m_szFailReason.clear();
		m_szProgressMessage.clear();
		m_bConnecting = false;
		m_flConnectionStartTime = 0.0f;
		m_bAuthSucessful = false;
	}

	void EnsureAuthenticatedToMasterServer()
	{
		if (g_pMasterServerManager->m_bOriginAuthWithMasterServerDone
		&& g_pCVar->FindVar("ns_has_agreed_to_send_token")->GetInt() == AGREED_TO_SEND_TOKEN)
			g_pMasterServerManager->AuthenticateOriginWithMasterServer();
	}

public:
	bool Connect(const std::string& address, const std::string& mode, bool useSCRPlaque);
	void Cancel() { m_bConnecting = false; m_eCurrentMode = m_eLastMode; }
	bool ParseAddress(const std::string& address, std::string& ip, int& port, bool& isV6);
	void SetFailed(const std::string& reason)
	{
		m_bFailed = true;
		m_szFailReason = reason;
	}
	std::string& GetFailReason() { return m_szFailReason; }
	void SetProgressMessage(const std::string& message) { m_szProgressMessage = message; }
	std::string& GetProgressMessage() { return m_szProgressMessage; }
	bool IsFailed() { return m_bFailed; }
	bool IsConnecting() { return m_bConnecting; }
	eConnectionMode GetCurrentMode() { return m_eCurrentMode; }
};

extern ConnectionManager* g_pConnectionManager;
