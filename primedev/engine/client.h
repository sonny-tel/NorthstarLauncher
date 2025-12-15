#pragma once

#include "shared/keyvalues.h"
#include "shared/signonstate.h"
#include "engine/inetmessage.h"
#include "engine/inetchannel.h"

extern void (*CClient__Disconnect)(void* self, uint32_t unknownButAlways1, const char* reason, ...);
extern void (*CClient__SendDataBlock)(void* self, bf_write* msg);

enum Reputation_t
{
	REP_NONE = 0,
	REP_REMOVE_ONLY,
	REP_MARK_BAD
};

// #56169 $DB69 PData size
// #512   $200	Trailing data
// #100	  $64	Safety buffer
const int PERSISTENCE_MAX_SIZE = 0xDDCD;

// note: NOT_READY and READY are the only entries we have here that are defined by the vanilla game
// entries after this are custom and used to determine the source of persistence, e.g. whether it is local or remote
enum class ePersistenceReady : char
{
	NOT_READY,
	READY = 3,
	READY_INSECURE = 3,
	READY_REMOTE
};

class IClientMessageHandler
{
public:
	virtual ~IClientMessageHandler(void) {};
	virtual void* ProcessStringCmd(void) = 0;
	virtual void* ProcessScriptMessage(void) = 0;
	virtual void* ProcessSetConVar(void) = 0;
	virtual bool nullsub_0(void) = 0;
	virtual char ProcessSignonState(void* msg) = 0; // NET_SignonState
	virtual void* ProcessMove(void) = 0;
	virtual void* ProcessVoiceData(void) = 0;
	virtual void* ProcessDurangoVoiceData(void) = 0;
	virtual bool nullsub_1(void) = 0;
	virtual void* ProcessLoadingProgress(void) = 0;
	virtual void* ProcessPersistenceRequestSave(void) = 0;
	virtual bool nullsub_2(void) = 0;
	virtual bool nullsub_3(void) = 0;
	virtual void* ProcessSetPlaylistVarOverride(void) = 0;
	virtual void* ProcessClaimClientSidePickup(void) = 0;
	virtual void* ProcessCmdKeyValues(void) = 0;
	virtual void* ProcessClientTick(void) = 0;
	virtual void* ProcessClientSayText(void) = 0;
	virtual bool nullsub_4(void) = 0;
	virtual bool nullsub_5(void) = 0;
	virtual bool nullsub_6(void) = 0;
	virtual void* ProcessScriptMessageChecksum(void) = 0;
};

class CClient : public IClientMessageHandler, public INetMessageHandler
{
public:
	char unk0[6];
	char m_Name[64]; // 0x16 ( Size: 64 )
	char _unk_0x56[514]; // 0x56 ( Size: 514 )
	KeyValues* m_ConVars; // 0x258 ( Size: 8 )
	char _unk_0x260[64]; // 0x260 ( Size: 64 )
	eSignonState m_Signon; // 0x2a0 ( Size: 4 )
	int32_t m_nDeltaTick; // 0x2a4 ( Size: 4 )
	uint64_t m_nOriginID; // 0x2a8 ( Size: 8 )
	int32_t m_nStringTableAckTick; // 0x2b0 ( Size: 4 )
	int32_t m_nSignonTick; // 0x2b4 ( Size: 4 )
	char _unk_0x2b8[160]; // 0x2b8 ( Size: 180 )
	char m_ClanTag[16]; // 0x358 ( Size: 16 )
	char _unk_0x368[284]; // 0x368 ( Size: 284 )
	bool m_bFakePlayer; // 0x484 ( Size: 1 )
	bool m_bReceivedPacket; // 0x485 ( Size: 1 )
	bool m_bLowViolence; // 0x486 ( Size: 1 )
	bool m_bFullyAuthenticated; // 0x487 ( Size: 1 )
	char _unk_0x488[24]; // 0x488 ( Size: 24 )
	ePersistenceReady m_iPersistenceReady; // 0x4a0 ( Size: 1 )
	char _unk_0x4a1[89]; // 0x4a1 ( Size: 89 )
	char m_PersistenceBuffer[PERSISTENCE_MAX_SIZE]; // 0x4fa ( Size: 56781 )
	char _unk_0xe2c7[4665]; // 0xe2c7 ( Size: 4665 )
	char m_UID[32]; // 0xf500 ( Size: 32 )
	char _unk_0xf520[123400]; // 0xf520 ( Size: 123400 )

	void Disconnect(const Reputation_t nRepLevel, const char* reason, ...);

	inline void SendDataBlock(bf_write* msg)
	{
		CClient__SendDataBlock(this, msg);
	}

	virtual void* ConnectionStart(INetChannelHandler* chan) = 0;
	virtual void ConnectionClosing(const char* reason, int unk1) = 0;
	virtual void ConnectionCrashed(const char* reason) = 0;
	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) = 0;
	virtual void PacketEnd(void) = 0;
};

static_assert(sizeof(CClient) == 0x2D728);
static_assert(offsetof(CClient, m_Name) == 0x16);
static_assert(offsetof(CClient, m_ConVars) == 0x258);
static_assert(offsetof(CClient, m_Signon) == 0x2A0);
static_assert(offsetof(CClient, m_nDeltaTick) == 0x2A4);
static_assert(offsetof(CClient, m_nOriginID) == 0x2A8);
static_assert(offsetof(CClient, m_nStringTableAckTick) == 0x2B0);
static_assert(offsetof(CClient, m_nSignonTick) == 0x2B4);
static_assert(offsetof(CClient, m_ClanTag) == 0x358);
static_assert(offsetof(CClient, m_bFakePlayer) == 0x484);
static_assert(offsetof(CClient, m_bReceivedPacket) == 0x485);
static_assert(offsetof(CClient, m_bLowViolence) == 0x486);
static_assert(offsetof(CClient, m_bFullyAuthenticated) == 0x487);
static_assert(offsetof(CClient, m_iPersistenceReady) == 0x4A0);
static_assert(offsetof(CClient, m_PersistenceBuffer) == 0x4FA);
static_assert(offsetof(CClient, m_UID) == 0xF500);

extern CClient* g_pClientArray;
