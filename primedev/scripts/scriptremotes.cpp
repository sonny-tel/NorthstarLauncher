#include "squirrel/squirrel.h"
#include "core/vanilla.h"

REPLACE_SQFUNC(Remote_RegisterFunction, ScriptContext::CLIENT)
{
	SQStackInfos si;

	if( g_pVanillaCompatibility->GetVanillaCompatibility() )
	{
		g_pSquirrel<context>->sq_stackinfos(sqvm, 1, si);

		// honestly, it would be better to try and figure out how you get disconnected for being out of sync
		if( strcmp(si._name, "RemoteFunctions_Init") == 0
		 && strcmp(si._sourceName, "_remote_functions_mp.gnut") == 0 )
			return g_pSquirrel<context>->m_funcOriginals["Remote_RegisterFunction"](sqvm);
		else
			return SQRESULT_NULL;
	}

	return g_pSquirrel<context>->m_funcOriginals["Remote_RegisterFunction"](sqvm);
}
