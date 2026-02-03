#include "Subgames.h"

#include "Application/Settings.h"

#include "Core/Logger/Logger.h"
#include "Core/IO/FileSystem.h"
#include "Core/Utility/StringUtil.h"

// The maximum number of identical world names allowed
#define MAX_WORLD_NAMES 100


bool GetGameQuakeConfig(const std::string& gamePath, Settings& conf)
{
	std::string confPath = gamePath + "/quake.conf";
	return conf.ReadConfigFile(confPath.c_str());
}

struct GameFindPath
{
	std::string path;
	bool userSpecific;

	GameFindPath(const std::string& path, bool userSpecific) :
        path(path), userSpecific(userSpecific)
	{

	}
};

Subgame FindSubgame(const std::string& gamePath)
{
	if (gamePath.empty())
		return Subgame();

	// Get meta
    std::string confPath = gamePath + "/game.conf";
	Settings conf;
	conf.ReadConfigFile(confPath.c_str());

	std::string gameId = conf.Get("id");
    std::string gameName = conf.Get("name");
	std::string gameRelativePath = 
		"art/quake/games/" + conf.Get("map") + "/" + gameId;

    std::string gameAuthor;
	if (conf.Exists("author"))
		gameAuthor = conf.Get("author");

	int gameRelease = 0;
	if (conf.Exists("release"))
		gameRelease = conf.GetInt("release");

	bool gameModding = false;
	if (conf.Exists("modding"))
		gameModding = conf.GetBool("modding");

    std::string iconPath = gameRelativePath + "/menu/icon.png";

	return Subgame(gameId, gamePath, gameRelativePath, 
		gameName, iconPath, gameAuthor, gameRelease, gameModding);
}

std::set<std::string> GetAvailableGamePaths()
{
	std::wstring gameDir =
		FileSystem::Get()->GetWorkingDirectory() + L"/../../Assets/Art/Quake/games";
	std::set<std::string> gamePaths;

	const std::set<char> ignore = { '.' };
	for (const auto& path : FileSystem::Get()->GetRecursiveDirectories(gameDir))
	{
		std::string gameName = ToString(FileSystem::Get()->GetFileName(path));
		if (ignore.count(gameName[0]))
			continue;

		if (FileSystem::Get()->ExistDirectory(path))
		{
			// If configuration file is not found or broken, ignore game
			Settings conf;
			std::string confPath = ToString(path.c_str()) + "/game.conf";
			if (!conf.ReadConfigFile(confPath.c_str()))
				continue;

			// Add it to result
			gamePaths.insert(ToString(path));
		}
	}
	return gamePaths;
}

std::vector<Subgame> GetAvailableGames()
{
    std::vector<Subgame> specs;
    std::set<std::string> gamePaths = GetAvailableGamePaths();
	specs.reserve(gamePaths.size());
	for (const auto& gamePath : gamePaths)
		specs.push_back(FindSubgame(gamePath));
	return specs;
}

#define LEGACY_GAMEID "quake"

bool GetWorldExists(const std::string& worldPath)
{
	return (FileSystem::Get()->ExistFile(ToWideString(worldPath) + L"/map_meta.txt") ||
			FileSystem::Get()->ExistFile(ToWideString(worldPath) + L"/world.qk"));
}

//! Try to get the displayed name of a world
std::string GetWorldName(const std::string& worldPath, const std::string& defaultName)
{
	Settings conf;
	std::string confPath = worldPath + "/world.qk";
	bool succeeded = conf.ReadConfigFile(confPath.c_str());
	if (!succeeded)
		return defaultName;

	if (!conf.Exists("world_name"))
		return defaultName;
	return conf.Get("world_name");
}


std::vector<WorldSpec> GetAvailableWorlds()
{
	std::wstring gameDir =
		FileSystem::Get()->GetWorkingDirectory() + L"/../../Assets/Art/Quake/games";
	std::vector<WorldSpec> worlds;

	const std::set<char> ignore = { '.' };
	for (const auto& path : FileSystem::Get()->GetRecursiveDirectories(gameDir))
	{
		std::string pathName = ToString(FileSystem::Get()->GetFileName(path));
		if (ignore.count(pathName[0]))
			continue;

		if (FileSystem::Get()->ExistDirectory(path))
		{
			// If world configuration file is not found or broken, ignore game
			Settings conf;
			std::string confPath = ToString(path.c_str()) + "/world.qk";
			if (!conf.ReadConfigFile(confPath.c_str()))
				continue;

			std::string fullpath = ToString(path);
			std::string name = GetWorldName(fullpath, ToString(path));
			// Just allow filling in the gameid always for now
			bool canBeLegacy = true;
			WorldSpec spec(fullpath, name);
			if (spec.IsValid())
			{
				LogInformation(name + " ");
				worlds.push_back(spec);
			}
			else
			{
				LogInformation("(invalid: " + name + ") ");
			}
		}
	}

	LogInformation(std::to_string(worlds.size()) + " found.");
	return worlds;
}

void LoadGameConfAndInitWorld(const std::string& path, 
    const std::string& name, const Subgame& game, bool createWorld)
{
	std::wstring finalPath = ToWideString(path);

	// If we're creating a new world, ensure that the path isn't already taken
	if (createWorld) 
    {
		int counter = 1;
		while (FileSystem::Get()->ExistDirectory(finalPath) && counter < MAX_WORLD_NAMES) 
        {
            finalPath += L"_" + std::to_wstring(counter);
			counter++;
		}

		if (FileSystem::Get()->ExistDirectory(finalPath)) {
			throw BaseException("Too many similar filenames");
		}
	}

	Settings* gameSettings = Settings::GetLayer(SL_GAME);
	const bool newSettingss = (gameSettings == nullptr);
	if (newSettingss) 
    {
		// Called by main-menu without a Logic instance running -> create and free manually
		gameSettings = Settings::CreateLayer(SL_GAME);
	}

	GetGameQuakeConfig(game.mPath, *gameSettings);
    for (const auto& keyName : gameSettings->GetNames()) 
    {
        if (keyName.compare(0, 7, "secure.") != 0)
            continue;

        LogWarning("Secure setting " + keyName + " isn't allowed, so was ignored.");
        gameSettings->Remove(keyName);
    }

	LogInformation(L"Initializing world at " + finalPath);

	// Create world.mt if does not already exist
    std::wstring worldPath = finalPath + L"/world.qk";
	if (!FileSystem::Get()->ExistFile(worldPath))
    {
		Settings conf;

		conf.Set("world_name", name);
		conf.Set("gameid", game.mId);
		conf.Set("backend", "bin");
		conf.Set("player_backend", "bin");
		conf.Set("auth_backend", "bin");

		if (!conf.UpdateConfigFile(ToString(worldPath).c_str()))
        {
			throw BaseException("Failed to update the config file");
		}
	}

	// Create map_meta.txt if does not already exist
	std::wstring mapMetaPath = finalPath + L"/map_meta.txt";
	if (!FileSystem::Get()->ExistFile(mapMetaPath)) 
    {
		LogInformation(L"Creating map_meta.txt (" + mapMetaPath + L")");
		std::ostringstream oss(std::ios_base::binary);

		Settings conf;

		conf.WriteLines(oss);
		oss << "[end_of_params]\n";

		FileSystem::Get()->SafeWriteToFile(ToString(mapMetaPath), oss.str());
	}

	// The Settings object is no longer needed for created worlds
	if (newSettingss)
		delete gameSettings;
}
