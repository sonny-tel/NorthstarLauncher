#include "squirrel/squirrel.h"

ADD_SQFUNC("void", Hud_DialogList_RemoveListItem, "var elem, string name, string enum", "", ScriptContext::UI)
{
	void** pData;
	uint64_t typeId;
    
    g_pSquirrel<context>->getuserdata(sqvm, 2, &pData, &typeId);

}