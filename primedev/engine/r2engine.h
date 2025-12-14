#pragma once
#include "shared/keyvalues.h"
#include "engine/bitbuf.h"
#include "engine/net.h"
#include "engine/netmessages.h"

// Cbuf
enum class ECommandTarget_t
{
	CBUF_FIRST_PLAYER = 0,
	CBUF_LAST_PLAYER = 1, // MAX_SPLITSCREEN_CLIENTS - 1, MAX_SPLITSCREEN_CLIENTS = 2
	CBUF_SERVER = CBUF_LAST_PLAYER + 1,

	CBUF_COUNT,
};

enum class cmd_source_t
{
	// Added to the console buffer by gameplay code.  Generally unrestricted.
	kCommandSrcCode,

	// Sent from code via engine->ClientCmd, which is restricted to commands visible
	// via FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS.
	kCommandSrcClientCmd,

	// Typed in at the console or via a user key-bind.  Generally unrestricted, although
	// the client will throttle commands sent to the server this way to 16 per second.
	kCommandSrcUserInput,

	// Came in over a net connection as a clc_stringcmd
	// host_client will be valid during this state.
	//
	// Restricted to FCVAR_GAMEDLL commands (but not convars) and special non-ConCommand
	// server commands hardcoded into gameplay code (e.g. "joingame")
	kCommandSrcNetClient,

	// Received from the server as the client
	//
	// Restricted to commands with FCVAR_SERVER_CAN_EXECUTE
	kCommandSrcNetServer,

	// Being played back from a demo file
	//
	// Not currently restricted by convar flag, but some commands manually ignore calls
	// from this source.  FIXME: Should be heavily restricted as demo commands can come
	// from untrusted sources.
	kCommandSrcDemoFile,

	// Invalid value used when cleared
	kCommandSrcInvalid = -1
};

typedef ECommandTarget_t (*Cbuf_GetCurrentPlayerType)();
extern Cbuf_GetCurrentPlayerType Cbuf_GetCurrentPlayer;

typedef void (*Cbuf_AddTextType)(ECommandTarget_t eTarget, const char* text, cmd_source_t source);
extern Cbuf_AddTextType Cbuf_AddText;

typedef void (*Cbuf_ExecuteType)();
extern Cbuf_ExecuteType Cbuf_Execute;

extern bool (*CCommand__Tokenize)(CCommand& self, const char* pCommandString, cmd_source_t commandSource);

// CEngine

enum EngineQuitState
{
	QUIT_NOTQUITTING = 0,
	QUIT_TODESKTOP,
	QUIT_RESTART
};

enum class EngineState_t
{
	DLL_INACTIVE = 0, // no dll
	DLL_ACTIVE, // engine is focused
	DLL_CLOSE, // closing down dll
	DLL_RESTART, // engine is shutting down but will restart right away
	DLL_PAUSED, // engine is paused, can become active from this state
};

class CEngine
{
public:
	virtual void unknown() = 0; // unsure if this is where
	virtual bool Load(bool dedicated, const char* baseDir) = 0;
	virtual void Unload() = 0;
	virtual void SetNextState(EngineState_t iNextState) = 0;
	virtual EngineState_t GetState() = 0;
	virtual void Frame() = 0;
	virtual double GetFrameTime() = 0;
	virtual float GetCurTime() = 0;

	EngineQuitState m_nQuitting;
	EngineState_t m_nDllState;
	EngineState_t m_nNextDllState;
	double m_flCurrentTime;
	float m_flFrameTime;
	double m_flPreviousTime;
	float m_flFilteredTime;
	float m_flMinFrameTime; // Expected duration of a frame, or zero if it is unlimited.
};

extern CEngine* g_pEngine;

extern void (*CClient__Disconnect)(void* self, uint32_t unknownButAlways1, const char* reason, ...);

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

enum class eSignonState : int
{
	NONE = 0, // no state yet; about to connect
	CHALLENGE = 1, // client challenging server; all OOB packets
	CONNECTED = 2, // client is connected to server; netchans ready
	NEW = 3, // just got serverinfo and string tables
	PRESPAWN = 4, // received signon buffers
	GETTINGDATA = 5, // respawn-defined signonstate, assumedly this is for persistence
	SPAWN = 6, // ready to receive entity packets
	FIRSTSNAP = 7, // another respawn-defined one
	FULL = 8, // we are fully connected; first non-delta packet received
	CHANGELEVEL = 9, // server is changing level; please wait
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
	virtual void* ConnectionStart(INetChannelHandler* chan) = 0;
	virtual void ConnectionClosing(const char* reason, int unk1) = 0;
	virtual void ConnectionCrashed(const char* reason) = 0;
	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) = 0;
	virtual void PacketEnd(void) = 0;

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

enum server_state_t
{
	ss_dead = 0, // Dead
	ss_loading, // Spawning
	ss_active, // Running
	ss_paused, // Running, but paused
};

extern server_state_t* g_pServerState;

extern char* g_pModName;

enum class GameMode_t : int
{
	NO_MODE = 0,
	MP_MODE,
	SP_MODE,
};

class CGlobalVars
{
public:
	// Absolute time (per frame still - Use Plat_FloatTime() for a high precision real time
	//  perf clock, but not that it doesn't obey host_timescale/host_framerate)
	double m_flRealTime; // 0x0 ( Size: 8 )

	// Absolute frame counter - continues to increase even if game is paused
	int m_nFrameCount; // 0x8 ( Size: 4 )

	// Non-paused frametime
	float m_flAbsoluteFrameTime; // 0xc ( Size: 4 )

	// Current time
	//
	// On the client, this (along with tickcount) takes a different meaning based on what
	// piece of code you're in:
	//
	//   - While receiving network packets (like in PreDataUpdate/PostDataUpdate and proxies),
	//     this is set to the SERVER TICKCOUNT for that packet. There is no interval between
	//     the server ticks.
	//     [server_current_Tick * tick_interval]
	//
	//   - While rendering, this is the exact client clock
	//     [client_current_tick * tick_interval + interpolation_amount]
	//
	//   - During prediction, this is based on the client's current tick:
	//     [client_current_tick * tick_interval]
	float m_flCurTime; // 0x10 ( Size: 4 )

	char _unk_0x14[28]; // 0x14 ( Size: 28 )

	// Time spent on last server or client frame (has nothing to do with think intervals)
	float m_flFrameTime; // 0x30 ( Size: 4 )

	// current maxplayers setting
	int m_nMaxClients; // 0x34 ( Size: 4 )
	GameMode_t m_nGameMode; // 0x38 ( Size: 4 )

	// Simulation ticks - does not increase when game is paused
	//   this is weird and doesn't seem to increase once per frame?
	uint32_t m_nTickCount; // 0x3c ( Size: 4 )

	// Simulation tick interval
	float m_flTickInterval; // 0x40 ( Size: 4 )
	char _unk_0x44[28]; // 0x44 ( Size: 28 )

	const char* m_pMapName; // 0x60 ( Size: 8 )
	int m_nMapVersion; // 0x68 ( Size: 4 )
};
static_assert(offsetof(CGlobalVars, m_flRealTime) == 0x0);
static_assert(offsetof(CGlobalVars, m_nFrameCount) == 0x8);
static_assert(offsetof(CGlobalVars, m_flAbsoluteFrameTime) == 0xc);
static_assert(offsetof(CGlobalVars, m_flCurTime) == 0x10);
static_assert(offsetof(CGlobalVars, m_flFrameTime) == 0x30);
static_assert(offsetof(CGlobalVars, m_nMaxClients) == 0x34);
static_assert(offsetof(CGlobalVars, m_nGameMode) == 0x38);
static_assert(offsetof(CGlobalVars, m_nTickCount) == 0x3c);
static_assert(offsetof(CGlobalVars, m_flTickInterval) == 0x40);
static_assert(offsetof(CGlobalVars, m_pMapName) == 0x60);
static_assert(offsetof(CGlobalVars, m_nMapVersion) == 0x68);

extern CGlobalVars* g_pGlobals;
