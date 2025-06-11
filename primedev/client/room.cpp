#include "room.h"

typedef void* (*JoinPlayerGameRoom_t)(const char* uid);
JoinPlayerGameRoom_t JoinPlayerGameRoom;
char* room1;
char* room2;

void ConCommand_ns_join_room(const CCommand& args)
{
    // 187C70
    auto roomId = args.Arg(1);
    JoinPlayerGameRoom(roomId);
}

void ConCommand_ns_print_room(const CCommand& args)
{
	NOTE_UNUSED(args);

	if (room1)
	{
		// not it
		//spdlog::info("Origin Player Room: {}", room1);
	}
	if (room2)
	{
		spdlog::info("Community Room: {}", room2);
	}
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", PartyRoom, ConCommand, (CModule module))
{
    JoinPlayerGameRoom = module.Offset(0x187C70).RCast<JoinPlayerGameRoom_t>();
	RegisterConCommand("ns_join_room", ConCommand_ns_join_room, "Join a server by its room ID", FCVAR_CLIENTDLL);

	room1 = module.Offset(0x1314E538).RCast<char*>();
	room2 = module.Offset(0x13F8E2D0).RCast<char*>();
	RegisterConCommand("ns_print_room", ConCommand_ns_print_room, "Dump room info", FCVAR_CLIENTDLL);

	// module.Offset(0x8c3e9).Patch("0x09");
}
