#include "squirrel/squirrel.h"
#include "client/r2client.h"
#include "engine/r2engine.h"

// asset function StringToAsset( string assetName )
ADD_SQFUNC(
	"asset",
	StringToAsset,
	"string assetName",
	"converts a given string to an asset",
	ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	g_pSquirrel<context>->pushasset(sqvm, g_pSquirrel<context>->getstring(sqvm, 1), -1);
	return SQRESULT_NOTNULL;
}

// string function NSGetLocalPlayerUID()
ADD_SQFUNC(
	"string", NSGetLocalPlayerUID, "", "Returns the local player's uid.", ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	if (g_pLocalPlayerUserID)
	{
		g_pSquirrel<context>->pushstring(sqvm, g_pLocalPlayerUserID);
		return SQRESULT_NOTNULL;
	}

	return SQRESULT_NULL;
}

ADD_SQFUNC(
	"string",
	GetConVarDefaultString,
	"string convarName",
	"Returns convar's default value as a string",
	ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	const char* convarName = g_pSquirrel<context>->getstring(sqvm, 1);
	if (!convarName)
	{
		g_pSquirrel<context>->raiseerror(sqvm, "ConVar name is null");
		return SQRESULT_ERROR;
	}

	auto convar = g_pCVar->FindVar(convarName);
	if (!convar)
	{
		g_pSquirrel<context>->raiseerror(sqvm, fmt::format("ConVar '{}' not found", convarName).c_str());
		return SQRESULT_ERROR;
	}

	const char* defaultValue = convar->m_pszDefaultValue;

	g_pSquirrel<context>->pushstring(sqvm, defaultValue);

	return SQRESULT_NOTNULL;
}