#include "core/convar/convar.h"
#include "squirrel/squirrel.h"

AUTOHOOK_INIT()

#define CInputSystem__PostEvent_SQFunc "CInputSystem__PostEvent"

ConVar* Cvar_cl_dump_input_events;

// clang-format off
AUTOHOOK(CInputSystem__PostEvent, inputsystem.dll + 0x7EC0, void, __fastcall, (void* self, int nType, int nTick, int nData, int nData2, int nData3))
// clang-format on
{
	if (Cvar_cl_dump_input_events->GetBool())
		spdlog::info("CInputSystem::PostEvent - Type: {}, Tick: {}, Data: {}, Data2: {}, Data3: {}",
			nType, nTick, nData, nData2, nData3);

	HSQUIRRELVM cl_sqvm = g_pSquirrel<ScriptContext::CLIENT>->m_pSQVM ? g_pSquirrel<ScriptContext::CLIENT>->m_pSQVM->sqvm : nullptr;

	if( g_pSquirrel<ScriptContext::CLIENT>->m_pSQVM && g_pSquirrel<ScriptContext::CLIENT>->m_pSQVM->sqvm )
	{
		HSQUIRRELVM sqvm = g_pSquirrel<ScriptContext::CLIENT>->m_pSQVM->sqvm;
		SQObject functionobj {};

		if( g_pSquirrel<ScriptContext::CLIENT>->sq_getfunction(sqvm, CInputSystem__PostEvent_SQFunc, &functionobj, 0) == 0)
			g_pSquirrel<ScriptContext::CLIENT>->Call(
				CInputSystem__PostEvent_SQFunc, nType, nTick, nData, nData2, nData3);
	}

	if( g_pSquirrel<ScriptContext::UI>->m_pSQVM && g_pSquirrel<ScriptContext::UI>->m_pSQVM->sqvm )
	{
		HSQUIRRELVM sqvm = g_pSquirrel<ScriptContext::UI>->m_pSQVM->sqvm;
		SQObject functionobj {};

		if( g_pSquirrel<ScriptContext::UI>->sq_getfunction(sqvm, CInputSystem__PostEvent_SQFunc, &functionobj, 0) == 0)
			g_pSquirrel<ScriptContext::UI>->Call(
				CInputSystem__PostEvent_SQFunc, nType, nTick, nData, nData2, nData3);
	}

	CInputSystem__PostEvent(self, nType, nTick, nData, nData2, nData3);
	return;
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
