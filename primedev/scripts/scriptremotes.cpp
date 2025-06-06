#include "squirrel/squirrel.h"
#include "core/vanilla.h"

REPLACE_SQFUNC(Remote_RegisterFunction, ScriptContext::CLIENT)
{
	SQStackInfos si;

	if( !g_pSquirrel<context>->m_pSQVM || !g_pSquirrel<context>->m_pSQVM->sqvm )
		return SQRESULT_NULL;

	if( g_pVanillaCompatibility->GetVanillaCompatibility() )
	{
		g_pSquirrel<context>->sq_stackinfos(sqvm, 1, si);

		if( si._name == nullptr || si._sourceName == nullptr )
		{
			g_pSquirrel<context>->raiseerror(sqvm, "No source name or function name found in stack infos. Can't register remote.");
			return SQRESULT_ERROR;
		}

		// honestly, it would be better to try and figure out how you get disconnected for being out of sync
		if( strcmp(si._name, "RemoteFunctions_Init") == 0
		 && strcmp(si._sourceName, "_remote_functions_mp.gnut") == 0 )
			return g_pSquirrel<context>->m_funcOriginals["Remote_RegisterFunction"](sqvm);
		else
			return SQRESULT_NULL;
	}

	return g_pSquirrel<context>->m_funcOriginals["Remote_RegisterFunction"](sqvm);
}
