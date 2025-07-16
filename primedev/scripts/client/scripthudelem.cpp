#include "squirrel/squirrel.h"

using CClientScriptHudElement = void*;
using CUtlString = const char*; 

class CDialogListButton
{
public:
    struct DialogListItem_t
    {
        // CUtlString m_String;      
        // CUtlString m_StringParm1;   
        // CUtlString m_CommandString; 
        // bool m_bEnabled; 
        char pad[72];           
    };
};

// static_assert(sizeof(CDialogListButton::DialogListItem_t) == 72, "Wrong size");

ADD_SQFUNC("void", Hud_DialogList_RemoveListItems, "var elem", "", ScriptContext::UI)
{
    CClientScriptHudElement* hudElement = g_pSquirrel<context>->gethudelement<CClientScriptHudElement>(sqvm, 1);

	if (!hudElement)
	{
		g_pSquirrel<context>->raiseerror(sqvm, "First parameter is not a hud element");
		return SQRESULT_ERROR;
	}

    uintptr_t dialogListButton = *(uintptr_t*)((uintptr_t)hudElement + 40);

    using VFuncType = uintptr_t(__fastcall*)(uintptr_t);
    VFuncType vfunc = *(VFuncType*)(*(uintptr_t*)dialogListButton + 1520);
    uintptr_t isDialogListButton = vfunc(dialogListButton);

    using NameFuncType = const char*(__fastcall*)(uintptr_t);
    NameFuncType nameFunc = *(NameFuncType*)(*(uintptr_t*)dialogListButton + 168);
    const char* name = nameFunc(dialogListButton);

    if (!isDialogListButton)
    {
        g_pSquirrel<context>->raiseerror(sqvm, fmt::format("No DialogListButton element with name '{}'.", name).c_str());
        return SQRESULT_ERROR;
    }

    int count = *(int*)(dialogListButton + 1328);
    CDialogListButton::DialogListItem_t* vectorBase = (CDialogListButton::DialogListItem_t*)(*(uintptr_t*)(dialogListButton + 1304));

    // epic memory leak probably
    *(int*)(dialogListButton + 1328) = 0;

	return SQRESULT_NULL;
}