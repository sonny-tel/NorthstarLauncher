#include "squirrel/squirrel.h"
#include "client/origin.h"

//void DecodeJsonTable(HSQUIRRELVM sqvm, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj)
//{
//	g_pSquirrel<context>->newtable(sqvm);
//
//	for (auto itr = obj->MemberBegin(); itr != obj->MemberEnd(); itr++)
//	{
//		switch (itr->value.GetType())
//		{
//		case rapidjson::kObjectType:
//			g_pSquirrel<context>->pushstring(sqvm, itr->name.GetString(), -1);
//			DecodeJsonTable<context>(
//				sqvm, (rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>*)&itr->value);
//			g_pSquirrel<context>->newslot(sqvm, -3, false);
//			break;
//		case rapidjson::kArrayType:
//			g_pSquirrel<context>->pushstring(sqvm, itr->name.GetString(), -1);
//			DecodeJsonArray<context>(
//				sqvm, (rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>*)&itr->value);
//			g_pSquirrel<context>->newslot(sqvm, -3, false);
//			break;
//		case rapidjson::kStringType:
//			g_pSquirrel<context>->pushstring(sqvm, itr->name.GetString(), -1);
//			g_pSquirrel<context>->pushstring(sqvm, itr->value.GetString(), -1);
//
//			g_pSquirrel<context>->newslot(sqvm, -3, false);
//

ADD_SQFUNC("table< string, string >", NSGetFriendSubscriptionMap, "", "", ScriptContext::UI)
{
	g_pSquirrel<context>->newtable(sqvm);

	if (g_IDPartySubMap.empty())
		return SQRESULT_NOTNULL;

	for (std::pair<__int64, std::string> item : g_IDPartySubMap)
	{
		g_pSquirrel<context>->pushstring(sqvm, std::to_string(item.first).c_str(), -1);
		g_pSquirrel<context>->pushstring(sqvm, item.second.c_str(), -1);

		g_pSquirrel<context>->newslot(sqvm, -3, false);
	}

	return SQRESULT_NOTNULL;
}
