#include "svcdownloader.h"
#include "moddownloader.h"

AUTOHOOK_INIT()

bool SVC_SetModSchema::ReadFromBuffer(BFRead* buffer)
{
	if(buffer->IsOverflowed())
	{
		spdlog::error("SVC_SetModSchema::WriteToBuffer: Buffer overflow occurred while writing mod entries.");
		return false;
	}

	unsigned int numEntries = buffer->ReadUBitLong(16); // Read number of mod entries
	spdlog::info("Reading {} mod entries from SVC_SetModSchema message buffer.", numEntries);
	m_ModEntries.clear();
	for (unsigned int i = 0; i < numEntries; ++i)
	{
		modentry_s entry;
		char* buff = new char[256];
		// Read mod name
		if(buffer->ReadString(buff, 128))
			entry.name = buff;
		else
			return false;
		delete[] buff;

		// Read download URL
		buff = new char[256];
		if(buffer->ReadString(buff, 224))
			entry.url = buff;
		else
			return false;
		delete[] buff;

		// Read checksum
		buff = new char[256];
		if(buffer->ReadString(buff, 128))
			entry.checksum = buff;
		else
			return false;
		delete[] buff;

		// Read version
		buff = new char[64];
		if(buffer->ReadString(buff, 32))
			entry.version = buff;
		else
			return false;
		delete[] buff;

		m_ModEntries.push_back(entry);
	}

    return true;
}

bool SVC_SetModSchema::WriteToBuffer(BFWrite* buffer)
{
	spdlog::info("Writing {} mod entries to SVC_SetModSchema message buffer.", m_ModEntries.size());

	buffer->WriteUBitLong(static_cast<unsigned int>(m_ModEntries.size()), 16); // Write number of mod entries

	for (const auto& entry : m_ModEntries)
	{
		spdlog::info("Writing mod entry: Name='{}', URL='{}', Checksum='{}', Version='{}'",
			entry.name, entry.url, entry.checksum, entry.version);
		// Write mod name
		if(entry.name.length() < 128 )
			buffer->WriteString(entry.name.c_str());
		else
			return false;

		// Write download URL
		if(entry.url.length() < 224 )
			buffer->WriteString(entry.url.c_str());
		else
			return false;

		// Write checksum
		if(entry.checksum.length() < 128 )
			buffer->WriteString(entry.checksum.c_str());
		else
			return false;

		// Write version
		if(entry.version.length() < 32 )
			buffer->WriteString(entry.version.c_str());
		else
			return false;
	}

	if(buffer->IsOverflowed())
	{
		spdlog::error("SVC_SetModSchema::WriteToBuffer: Buffer overflow occurred while writing mod entries.");
		return false;
	}

	return true;
}

bool SVC_SetModSchema::Process(void)
{
	for(const auto& entry : m_ModEntries)
	{
		spdlog::info("Mod Name: {}", entry.name);
		spdlog::info("Download URL: {}", entry.url);
		spdlog::info("Checksum: {}", entry.checksum);
		spdlog::info("Version: {}", entry.version);
	}

	return true;
}

bool g_bIsGettingServerData = false;

// clang-format off
AUTOHOOK(CClient__GetServerData, engine.dll + 0x105760, bool, __fastcall, (void* thisptr))
// clang-format on
{
	g_bIsGettingServerData = true;
	spdlog::info("CClient::GetServerData called, preparing to inject SVC_SetModSchema message.");
	bool res = CClient__GetServerData(thisptr);
	g_bIsGettingServerData = false;
	return res;
}

// clang-format off
AUTOHOOK(CClient__SendDataBlock, engine.dll + 0x104870, bool, __fastcall, (void* thisptr, BFWrite* buffer))
// clang-format on
{
	if (g_bIsGettingServerData)
	{
		SVC_SetModSchema modSchemaMessage = SVC_SetModSchema(g_pModDownloader->GetServerModSchemaDocument());
		spdlog::info("Num bytes written to buf a {}", buffer->GetNumBytesWritten());
		modSchemaMessage.WriteToBuffer(buffer);
		spdlog::info("Num bytes written to buf b {}", buffer->GetNumBytesWritten());
	}

	return CClient__SendDataBlock(thisptr, buffer);
}

ON_DLL_LOAD_RELIESON("engine.dll", SVCModDownloader, NetMessages, (CModule module))
{
	AUTOHOOK_DISPATCH()
}
