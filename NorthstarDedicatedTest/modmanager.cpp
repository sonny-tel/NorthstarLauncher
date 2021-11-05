#include "pch.h"
#include "modmanager.h"
#include "convar.h"
#include "concommand.h"

#include "rapidjson/error/en.h"
#include "rapidjson/document.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/writer.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "filesystem.h"

ModManager* g_ModManager;

Mod::Mod(fs::path modDir, char* jsonBuf)
{
	wasReadSuccessfully = false;

	ModDirectory = modDir;

	rapidjson::Document modJson;
	modJson.Parse<rapidjson::ParseFlag::kParseCommentsFlag | rapidjson::ParseFlag::kParseTrailingCommasFlag>(jsonBuf);

	// fail if parse error
	if (modJson.HasParseError())
	{
		spdlog::error("Failed reading mod file {}: encountered parse error \"{}\" at offset {}", (modDir / "mod.json").string(), GetParseError_En(modJson.GetParseError()), modJson.GetErrorOffset());
		return;
	}

	// fail if it's not a json obj (could be an array, string, etc)
	if (!modJson.IsObject())
	{
		spdlog::error("Failed reading mod file {}: file is not a JSON object", (modDir / "mod.json").string());
		return;
	}

	// basic mod info
	// name is required
	if (!modJson.HasMember("Name"))
	{
		spdlog::error("Failed reading mod file {}: missing required member \"Name\"", (modDir / "mod.json").string());
		return;
	}

	Name = modJson["Name"].GetString();

	if (modJson.HasMember("Description"))
		Description = modJson["Description"].GetString();
	else
		Description = "";

	if (modJson.HasMember("Version"))
		Version = modJson["Version"].GetString();
	else
	{
		Version = "0.0.0";
		spdlog::warn("Mod file {} is missing a version, consider adding a version", (modDir / "mod.json").string());
	}

	if (modJson.HasMember("DownloadLink"))
		DownloadLink = modJson["DownloadLink"].GetString();
	else
		DownloadLink = "";
	
	if (modJson.HasMember("RequiredOnClient"))
		RequiredOnClient = modJson["RequiredOnClient"].GetBool();
	else
		RequiredOnClient = false;

	if (modJson.HasMember("LoadPriority"))
		LoadPriority = modJson["LoadPriority"].GetInt();
	else
	{
		spdlog::info("Mod file {} is missing a LoadPriority, consider adding one", (modDir / "mod.json").string());
		LoadPriority = 0;
	}

	// mod convars
	if (modJson.HasMember("ConVars") && modJson["ConVars"].IsArray())
	{
		for (auto& convarObj : modJson["ConVars"].GetArray())
		{
			if (!convarObj.IsObject() || !convarObj.HasMember("Name") || !convarObj.HasMember("DefaultValue"))
				continue;

			ModConVar* convar = new ModConVar;
			convar->Name = convarObj["Name"].GetString();
			convar->DefaultValue = convarObj["DefaultValue"].GetString();

			if (convarObj.HasMember("HelpString"))
				convar->HelpString = convarObj["HelpString"].GetString();
			else
				convar->HelpString = "";

			// todo: could possibly parse FCVAR names here instead
			if (convarObj.HasMember("Flags"))
				convar->Flags = convarObj["Flags"].GetInt();
			else
				convar->Flags = FCVAR_NONE;

			ConVars.push_back(convar);
		}
	}

	// mod scripts
	if (modJson.HasMember("Scripts") && modJson["Scripts"].IsArray())
	{
		for (auto& scriptObj : modJson["Scripts"].GetArray())
		{
			if (!scriptObj.IsObject() || !scriptObj.HasMember("Path") || !scriptObj.HasMember("RunOn"))
				continue;
			
			ModScript* script = new ModScript;

			script->Path = scriptObj["Path"].GetString();
			script->RsonRunOn = scriptObj["RunOn"].GetString();

			if (scriptObj.HasMember("ServerCallback") && scriptObj["ServerCallback"].IsObject())
			{
				ModScriptCallback* callback = new ModScriptCallback;
				callback->Context = SERVER;

				if (scriptObj["ServerCallback"].HasMember("Before") && scriptObj["ServerCallback"]["Before"].IsString())
					callback->BeforeCallback = scriptObj["ServerCallback"]["Before"].GetString();

				if (scriptObj["ServerCallback"].HasMember("After") && scriptObj["ServerCallback"]["After"].IsString())
					callback->AfterCallback = scriptObj["ServerCallback"]["After"].GetString();
			
				script->Callbacks.push_back(callback);
			}

			if (scriptObj.HasMember("ClientCallback") && scriptObj["ClientCallback"].IsObject())
			{
				ModScriptCallback* callback = new ModScriptCallback;
				callback->Context = CLIENT;

				if (scriptObj["ClientCallback"].HasMember("Before") && scriptObj["ClientCallback"]["Before"].IsString())
					callback->BeforeCallback = scriptObj["ClientCallback"]["Before"].GetString();

				if (scriptObj["ClientCallback"].HasMember("After") && scriptObj["ClientCallback"]["After"].IsString())
					callback->AfterCallback = scriptObj["ClientCallback"]["After"].GetString();

				script->Callbacks.push_back(callback);
			}

			if (scriptObj.HasMember("UICallback") && scriptObj["UICallback"].IsObject())
			{
				ModScriptCallback* callback = new ModScriptCallback;
				callback->Context = UI;

				if (scriptObj["UICallback"].HasMember("Before") && scriptObj["UICallback"]["Before"].IsString())
					callback->BeforeCallback = scriptObj["UICallback"]["Before"].GetString();

				if (scriptObj["UICallback"].HasMember("After") && scriptObj["UICallback"]["After"].IsString())
					callback->AfterCallback = scriptObj["UICallback"]["After"].GetString();

				script->Callbacks.push_back(callback);
			}

			Scripts.push_back(script);
		}
	}

	if (modJson.HasMember("Localisation") && modJson["Localisation"].IsArray())
	{
		for (auto& localisationStr : modJson["Localisation"].GetArray())
		{
			if (!localisationStr.IsString())
				continue;

			LocalisationFiles.push_back(localisationStr.GetString());
		}
	}

	wasReadSuccessfully = true;
}

ModManager::ModManager()
{
	LoadMods();
}

void ModManager::LoadMods()
{
	if (m_hasLoadedMods)
		UnloadMods();

	std::vector<fs::path> modDirs;

	// ensure dirs exist
	fs::remove_all(COMPILED_ASSETS_PATH);
	fs::create_directories(MOD_FOLDER_PATH);

	// read enabled mods cfg
	std::ifstream enabledModsStream("R2Northstar/enabledmods.json");
	std::stringstream enabledModsStringStream;

	if (!enabledModsStream.fail())
	{
		while (enabledModsStream.peek() != EOF)
			enabledModsStringStream << (char)enabledModsStream.get();

		enabledModsStream.close();
		m_enabledModsCfg.Parse<rapidjson::ParseFlag::kParseCommentsFlag | rapidjson::ParseFlag::kParseTrailingCommasFlag>(enabledModsStringStream.str().c_str());
		
		m_hasEnabledModsCfg = m_enabledModsCfg.IsObject();
	}

	// get mod directories
	for (fs::directory_entry dir : fs::directory_iterator(MOD_FOLDER_PATH))
		if (fs::exists(dir.path() / "mod.json"))
			modDirs.push_back(dir.path());

	for (fs::path modDir : modDirs)
	{
		// read mod json file
		std::ifstream jsonStream(modDir / "mod.json");
		std::stringstream jsonStringStream;
		
		// fail if no mod json
		if (jsonStream.fail())
		{
			spdlog::warn("Mod {} has a directory but no mod.json", modDir.string());
			continue;
		}

		while (jsonStream.peek() != EOF)
			jsonStringStream << (char)jsonStream.get();

		jsonStream.close();
	
		Mod* mod = new Mod(modDir, (char*)jsonStringStream.str().c_str());

		if (m_hasEnabledModsCfg && m_enabledModsCfg.HasMember(mod->Name.c_str()))
			mod->Enabled = m_enabledModsCfg[mod->Name.c_str()].IsTrue();
		else
			mod->Enabled = true;

		if (mod->wasReadSuccessfully)
		{
			spdlog::info("Loaded mod {} successfully", mod->Name);
			if (mod->Enabled)
				spdlog::info("Mod {} is enabled", mod->Name);
			else
				spdlog::info("Mod {} is disabled", mod->Name);

			m_loadedMods.push_back(mod);
		}
		else
		{
			spdlog::warn("Skipping loading mod file {}", (modDir / "mod.json").string());
			delete mod;
		}
	}

	// sort by load prio, lowest-highest
	std::sort(m_loadedMods.begin(), m_loadedMods.end(), [](Mod* a, Mod* b) {
		return a->LoadPriority < b->LoadPriority;
	});

	for (Mod* mod : m_loadedMods)
	{
		if (!mod->Enabled)
			continue;

		// register convars
		// for reloads, this is sorta barebones, when we have a good findconvar method, we could probably reset flags and stuff on preexisting convars
		// potentially it might also be good to unregister convars if they get removed on a reload, but unsure if necessary
		for (ModConVar* convar : mod->ConVars)
			if (g_CustomConvars.find(convar->Name) == g_CustomConvars.end()) // make sure convar isn't registered yet, unsure if necessary but idk what behaviour is for defining same convar multiple times
				RegisterConVar(convar->Name.c_str(), convar->DefaultValue.c_str(), convar->Flags, convar->HelpString.c_str());

		// read vpk paths
		if (fs::exists(mod->ModDirectory / "vpk"))
		{
			for (fs::directory_entry file : fs::directory_iterator(mod->ModDirectory / "vpk"))
			{
				// a bunch of checks to make sure we're only adding dir vpks and their paths are good
				// note: the game will literally only load vpks with the english prefix
				if (fs::is_regular_file(file) && file.path().extension() == ".vpk" && 
					file.path().string().find("english") != std::string::npos && file.path().string().find(".bsp.pak000_dir") != std::string::npos)
				{
					std::string formattedPath = file.path().filename().string();

					// this really fucking sucks but it'll work
					std::string vpkName = (file.path().parent_path() / formattedPath.substr(strlen("english"), formattedPath.find(".bsp") - 3)).string();
					mod->Vpks.push_back(vpkName);
				
					if (m_hasLoadedMods)
						(*g_Filesystem)->m_vtable->MountVPK(*g_Filesystem, vpkName.c_str());
				}
			}
		}
			

		// read keyvalues paths
		if (fs::exists(mod->ModDirectory / "keyvalues"))
		{
			for (fs::directory_entry file : fs::recursive_directory_iterator(mod->ModDirectory / "keyvalues"))
			{
				if (fs::is_regular_file(file))
				{
					std::string kvStr = file.path().lexically_relative(mod->ModDirectory / "keyvalues").lexically_normal().string();
					mod->KeyValuesHash.push_back(std::hash<std::string>{}(kvStr));
					mod->KeyValues.push_back(kvStr);
				}
			}
		}
	}

	// in a seperate loop because we register mod files in reverse order, since mods loaded later should have their files prioritised
	for (int i = m_loadedMods.size() - 1; i > -1; i--)
	{
		if (!m_loadedMods[i]->Enabled)
			continue;

		if (fs::exists(m_loadedMods[i]->ModDirectory / MOD_OVERRIDE_DIR))
		{
			for (fs::directory_entry file : fs::recursive_directory_iterator(m_loadedMods[i]->ModDirectory / MOD_OVERRIDE_DIR))
			{
				fs::path path = file.path().lexically_relative(m_loadedMods[i]->ModDirectory / MOD_OVERRIDE_DIR).lexically_normal();

				if (file.is_regular_file() && m_modFiles.find(path.string()) == m_modFiles.end())
				{
					ModOverrideFile* modFile = new ModOverrideFile;
					modFile->owningMod = m_loadedMods[i];
					modFile->path = path;
					m_modFiles.insert(std::make_pair(path.string(), modFile));
				}
			}
		}
	}

	m_hasLoadedMods = true;
}

void ModManager::UnloadMods()
{
	// clean up stuff from mods before we unload

	// do we need to dealloc individual entries in m_modFiles? idk, rework
	m_modFiles.clear();
	fs::remove_all(COMPILED_ASSETS_PATH);

	if (!m_hasEnabledModsCfg)
		m_enabledModsCfg.SetObject();

	for (Mod* mod : m_loadedMods)
	{	
		// remove all built kvs
		for (std::string kvPaths : mod->KeyValues)
			fs::remove(COMPILED_ASSETS_PATH / fs::path(kvPaths).lexically_relative(mod->ModDirectory));

		mod->KeyValuesHash.clear();
		mod->KeyValues.clear();

		// write to m_enabledModsCfg
		if (!m_enabledModsCfg.HasMember(mod->Name.c_str()))
			m_enabledModsCfg.AddMember(rapidjson::StringRef(mod->Name.c_str()), rapidjson::Value(false), m_enabledModsCfg.GetAllocator());

		m_enabledModsCfg[mod->Name.c_str()].SetBool(mod->Enabled);
	}

	std::ofstream writeStream("R2Northstar/enabledmods.json");
	rapidjson::OStreamWrapper writeStreamWrapper(writeStream);
	rapidjson::Writer<rapidjson::OStreamWrapper> writer(writeStreamWrapper);
	m_enabledModsCfg.Accept(writer);

	// do we need to dealloc individual entries in m_loadedMods? idk, rework
	m_loadedMods.clear();
}

void ModManager::CompileAssetsForFile(const char* filename)
{
	fs::path path(filename);

	if (!path.filename().compare("scripts.rson"))
		BuildScriptsRson();
	else //if (!strcmp((filename + strlen(filename)) - 3, "txt")) // check if it's a .txt
	{
		// check if we should build keyvalues, depending on whether any of our mods have patch kvs for this file
		for (Mod* mod : m_loadedMods)
		{
			if (!mod->Enabled)
				continue;

			size_t fileHash = std::hash<std::string>{}(fs::path(filename).lexically_normal().string());
			if (std::find(mod->KeyValuesHash.begin(), mod->KeyValuesHash.end(), fileHash) != mod->KeyValuesHash.end())
			{
				TryBuildKeyValues(filename);
				return;
			}
		}
	}
}

void ReloadModsCommand(const CCommand& args)
{
	g_ModManager->LoadMods();
}

void InitialiseModManager(HMODULE baseAddress)
{
	g_ModManager = new ModManager();

	RegisterConCommand("reload_mods", ReloadModsCommand, "idk", FCVAR_NONE);
}