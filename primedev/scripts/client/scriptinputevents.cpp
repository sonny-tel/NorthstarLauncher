#include "core/convar/convar.h"
#include "squirrel/squirrel.h"

AUTOHOOK_INIT()

#define CInputSystem__PostEvent_SQFunc "CInputSystem__ProcessPostEvent"

#define CALL_INPUTSYS_SQ_FUNC(context) \
	if( g_pSquirrel<ScriptContext::context>->m_pSQVM && g_pSquirrel<ScriptContext::context>->m_pSQVM->sqvm ) \
	{ \
		HSQUIRRELVM sqvm = g_pSquirrel<ScriptContext::context>->m_pSQVM->sqvm; \
		SQObject functionobj {}; \
		if( g_pSquirrel<ScriptContext::context>->sq_getfunction(sqvm, CInputSystem__PostEvent_SQFunc, &functionobj, 0) == 0) \
			g_pSquirrel<ScriptContext::context>->AsyncCall( \
				CInputSystem__PostEvent_SQFunc, nType, nTick, nData, nData2, nData3); \
	}

ConVar* Cvar_cl_dump_input_events;

// clang-format off
AUTOHOOK(CInputSystem__PostEvent, inputsystem.dll + 0x7EC0, void, __fastcall, (void* self, int nType, int nTick, int nData, int nData2, int nData3))
// clang-format on
{
	CInputSystem__PostEvent(self, nType, nTick, nData, nData2, nData3);

	if (Cvar_cl_dump_input_events->GetBool())
		spdlog::info("CInputSystem::PostEvent - Type: {}, Tick: {}, Data: {}, Data2: {}, Data3: {}",
			nType, nTick, nData, nData2, nData3);

	CALL_INPUTSYS_SQ_FUNC(CLIENT);
	CALL_INPUTSYS_SQ_FUNC(UI);
}

ON_DLL_LOAD_RELIESON("inputsystem.dll", FastCallbacks, ConVar, (CModule module))
{
	Cvar_cl_dump_input_events = new ConVar(
		"cl_dump_input_events",
		"0",
		FCVAR_NONE,
		"Dump input events to console for debugging purposes.");

	AUTOHOOK_DISPATCH();
}
