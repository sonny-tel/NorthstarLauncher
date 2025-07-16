#include "squirrel/squirrel.h"

using CClientScriptHudElement = void*;
using CUtlString = const char*; 

class CDialogListButton
{
public:
	struct DialogListItem_t
	{
	    CUtlString m_String;
	    CUtlString m_StringParm1;
	    CUtlString m_CommandString; // unused pretty sure but the size looks the same maybe
	    bool m_bEnabled;
	};
};

ADD_SQFUNC("void", Hud_DialogList_RemoveListItem, "var elem, string name, string enum", "", ScriptContext::UI)
{
    CClientScriptHudElement* hudElement = g_pSquirrel<context>->gethudelement<CClientScriptHudElement>(sqvm, 1);

	if (!hudElement)
	{
		g_pSquirrel<context>->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

    uintptr_t v7 = *(uintptr_t*)((uintptr_t)hudElement + 40);

    using VFuncType = uintptr_t(__fastcall*)(uintptr_t);
    VFuncType vfunc = *(VFuncType*)(*(uintptr_t*)v7 + 1520);
    uintptr_t dialogListButton = vfunc(v7);

    using NameFuncType = const char*(__fastcall*)(uintptr_t);
    NameFuncType nameFunc = *(NameFuncType*)(*(uintptr_t*)v7 + 168);
    const char* name = nameFunc(v7);

    if (!dialogListButton)
    {
        g_pSquirrel<context>->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
        return SQRESULT_ERROR;
    }

	return SQRESULT_NULL;
}