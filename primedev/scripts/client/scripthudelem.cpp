#include "squirrel/squirrel.h"

using CClientHudElement = void*;

ADD_SQFUNC("void", Hud_DialogList_RemoveListItem, "var elem, string name, string enum", "", ScriptContext::UI)
{
    CClientHudElement* hudElement = g_pSquirrel<context>->gethudelement<CClientHudElement>(sqvm, 1);

	if (!hudElement)
	{
		g_pSquirrel<context>->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

	return SQRESULT_NULL;
}