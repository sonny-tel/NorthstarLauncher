#include "squirrel/squirrel.h"

using CClientScriptHudElement = void*;
using CUtlString = const char*; 
using CGameUIConVarRef__Init_t = __int64(*)(__int64, const char*, bool);

CGameUIConVarRef__Init_t CGameUIConVarRef__Init = nullptr;

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

ADD_SQFUNC("void", Hud_ChangeDialogListConVar, "var elem, string convarName", "", ScriptContext::UI)
{
    CClientScriptHudElement* hudElement = g_pSquirrel<context>->gethudelement<CClientScriptHudElement>(sqvm, 1);
    const char* convarName = g_pSquirrel<context>->getstring(sqvm, 2);

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

    void* convarRef = malloc(0x10);
    __int64 result = CGameUIConVarRef__Init(
        (uintptr_t)convarRef, convarName, false);

    *(uintptr_t**)(dialogListButton + 1256) = (uintptr_t*)convarRef;


    return SQRESULT_NULL;
}

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", CGameUIConVarRef, ConVar, (CModule module))
{
    CGameUIConVarRef__Init = module.Offset(0x4A34A0).RCast<CGameUIConVarRef__Init_t>();
}
