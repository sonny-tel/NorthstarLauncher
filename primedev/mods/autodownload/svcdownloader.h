#pragma once

#include "engine/netmessages.h"
#include "moddownloader.h"

#include <rapidjson/document.h>

class SVC_SetModSchema : public CNetMessage
{
  public:
	SVC_SetModSchema() = default;
	SVC_SetModSchema(const rapidjson::Document& document)
	{
    	if (!document.IsObject())
    	{
    	    spdlog::error("SVC_SetModSchema: JSON document is not an object");
    	    return;
    	}

    	for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it)
    	{
    	    modentry_s entry;
    	    entry.name = it->name.GetString();

    	    const rapidjson::Value& modData = it->value;

    	    if (!modData.IsObject())
    	    {
    	        spdlog::warn("SVC_SetModSchema: Mod '{}' data is not an object, skipping", entry.name);
    	        continue;
    	    }

    	    // Parse DownloadLink
    	    if (modData.HasMember("DownloadLink") && modData["DownloadLink"].IsString())
    	    {
    	        entry.url = modData["DownloadLink"].GetString();
    	    }
    	    else
    	    {
    	        spdlog::warn("SVC_SetModSchema: Mod '{}' missing DownloadLink, skipping", entry.name);
    	        continue;
    	    }

        	// Parse Version
        	if (modData.HasMember("Version") && modData["Version"].IsString())
        	{
        	    entry.version = modData["Version"].GetString();
        	}
        	else
        	{
        	    spdlog::warn("SVC_SetModSchema: Mod '{}' missing Version, skipping", entry.name);
        	    continue;
        	}

    	    // Parse Checksum
    	    if (modData.HasMember("Checksum") && modData["Checksum"].IsString())
    	    {
    	        entry.checksum = modData["Checksum"].GetString();
    	    }
    	    else
    	    {
    	        spdlog::warn("SVC_SetModSchema: Mod '{}' missing Checksum, skipping", entry.name);
    	        continue;
    	    }

    	    m_ModEntries.push_back(entry);
    	}

    	spdlog::info("SVC_SetModSchema: Loaded {} mod entries", m_ModEntries.size());
	}

	virtual bool ReadFromBuffer(bf_read* buffer);
	virtual bool WriteToBuffer(bf_write* buffer);
	virtual bool Process(void);

	virtual int GetType(void) const { return static_cast<int>(NSCustomNetMessages::svc_SetModSchema); }
	virtual const char* GetName(void) const { return "svc_SetModSchema"; }

	virtual const char* ToString(void) const { return "svc_SetModSchema: unimplemented"; }

	virtual size_t GetSize(void) const { return sizeof(SVC_SetModSchema); }

	std::vector<modentry_s> m_ModEntries;
};
