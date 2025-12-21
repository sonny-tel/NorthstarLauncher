#pragma once

#include "engine/net.h"
#include "masterserver/masterserver.h"
#include "core/tier0.h"

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
	bool m_bFinished = false;
	bool m_bFailed = false;
	bool m_bUseSCRPlaque = false; // whether we're expecting script to show progress or use LoadingProgress from BaseModUI
	bool m_bConnecting = false;
	float m_flConnectionStartTime = 0.0f;
	bool m_bAuthSucessful = false;
	std::string m_szMapName;

	void ConnectToLocalServer();
	void ConnectToNorthstarServer(const std::string& address);
	void ConnectToVanillaMatchmakingServer(const std::string& address);
	void ConnectToP2PServer(const std::string& address);
	void SendInfoRequestPacket(const CNetAdr& addr, bool serverAuthUs, bool requestMods);
	void HandleModDownloads();
	bool IsCancelled() { return !m_bConnecting;}
	void Cancel() { InvokeCancelCallbacks(); }

	void InvokeConnectionStartCallbacks();
	void InvokeCancelCallbacks() {}

	void AuthenticateToMasterServer();

public:
	void Connect(const std::string& address, eConnectionMode mode, bool useSCRPlaque = true, std::string mapName = "");
	void Connect(eConnectionMode mode, bool useSCRPlaque = true, std::string mapName = "");
	void Interrupt(const char* reason = "#CODE_CONNECTION_INTERRUPTED_BY_USER")
	{
		m_bFailed = true;
		m_bConnecting = false;
		m_eCurrentMode = m_eLastMode;
		m_szFailReason = reason;
	}

	void Finalise() { m_bConnecting = false; }

	void ResetState()
	{
		m_bFailed = false;
		m_szFailReason.clear();
		m_szProgressMessage.clear();
		m_bConnecting = false;
		m_flConnectionStartTime = 0.0f;
		m_bAuthSucessful = false;
	}

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
