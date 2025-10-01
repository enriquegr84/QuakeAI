/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "Subgames.h"

#include "Application/Settings.h"

#include "Core/Logger/Logger.h"
#include "Core/IO/FileSystem.h"
#include "Core/Utility/StringUtil.h"

// The maximum number of identical world names allowed
#define MAX_WORLD_NAMES 100


bool GetGameMinetestConfig(const std::string& gamePath, Settings& conf)
{
	std::string confPath = gamePath + "/minetest.conf";
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

Subgame FindSubgame(const std::string& id)
{
	if (id.empty())
		return Subgame();

	// Get all possible paths fo game
    std::vector<GameFindPath> findPaths;
	std::string share = ToString(FileSystem::Get()->GetWorkingDirectory());

    std::string gameBase = "/../../Assets/Art/Quake/";
	gameBase = gameBase.append("games").append("/").append(id);
    std::string gameSuffixed = gameBase + "_game";
	findPaths.emplace_back(share + gameBase, false);
	findPaths.emplace_back(share + gameSuffixed, false);

	// Find game directory
    std::string gamePath, gameRelativePath;
	bool userGame = true; // Game is in user's directory
	for (const GameFindPath& findPath : findPaths) 
    {
		const std::string& tryPath = findPath.path;
		if (FileSystem::Get()->ExistDirectory(ToWideString(tryPath))) 
        {
			gamePath = tryPath;
			gameRelativePath = "Art/Quake/games/" + id;
			userGame = findPath.userSpecific;
			break;
		}
	}

	if (gamePath.empty())
		return Subgame();

    std::string gameModPath = share + "/../../Assets/Actors/Quake/Mods/" + id;

	// Find mod directories
    std::set<std::string> modsPaths;
	if (!userGame)
		modsPaths.insert(share + "/../../Assets/Actors/Quake/Mods/");

	// Get meta
    std::string confPath = gamePath + "/game.conf";
	Settings conf;
	conf.ReadConfigFile(confPath.c_str());

    std::string gameName;
	if (conf.Exists("name"))
		gameName = conf.Get("name");
	else
		gameName = id;

    std::string gameAuthor;
	if (conf.Exists("author"))
		gameAuthor = conf.Get("author");

	int gameRelease = 0;
	if (conf.Exists("release"))
		gameRelease = conf.GetInt("release");

    std::string menuIconPath = gamePath + "/menu/icon.png";

	return Subgame(
        id, gamePath,gameRelativePath, gameModPath, modsPaths, 
		gameName, menuIconPath, gameAuthor, gameRelease);
}

Subgame FindWorldSubgame(const std::string& gamePath)
{
	std::string worldGameId = GetWorldGameId(gamePath + "/map", true);
	return FindSubgame(worldGameId);
}

std::set<std::string> GetAvailableGameIds()
{
	std::wstring path = 
		FileSystem::Get()->GetWorkingDirectory() + L"/../../Assets/Art/Quake/games/";
	std::set<std::string> gameids;

	const std::set<char> ignore = { '.' };
	std::vector<std::wstring> dirs;
	FileSystem::Get()->GetFileList(dirs, path, true);
	for (const std::wstring& dir : dirs)
	{
		std::string fileName = ToString(FileSystem::Get()->GetFileName(dir));
		if (ignore.count(fileName[0]))
			continue;

		// If configuration file is not found or broken, ignore game
		Settings conf;
		std::string confPath = ToString(dir) + "/game.conf";
		if (!conf.ReadConfigFile(confPath.c_str()))
			continue;

		// Add it to result
		const char* ends[] = { "_game", NULL };
		std::string shorter = StringRemoveEnd(ToString(dir), ends);
		if (!shorter.empty())
			gameids.insert(shorter);
		else
			gameids.insert(ToString(dir));
	}
	return gameids;
}

std::vector<Subgame> GetAvailableGames()
{
    std::vector<Subgame> specs;
    std::set<std::string> gameids = GetAvailableGameIds();
	specs.reserve(gameids.size());
	for (const auto& gameid : gameids)
		specs.push_back(FindSubgame(gameid));
	return specs;
}

#define LEGACY_GAMEID "minetest"

bool GetWorldExists(const std::string& worldPath)
{
	return (FileSystem::Get()->ExistFile(ToWideString(worldPath) + L"/map_meta.txt") ||
			FileSystem::Get()->ExistFile(ToWideString(worldPath) + L"/world.mt"));
}

//! Try to get the displayed name of a world
std::string GetWorldName(const std::string& worldPath, const std::string& defaultName)
{
    std::string confPath = worldPath + "/world.mt";
	Settings conf;
	bool succeeded = conf.ReadConfigFile(confPath.c_str());
	if (!succeeded)
		return defaultName;

	if (!conf.Exists("world_name"))
		return defaultName;
	return conf.Get("world_name");
}

std::string GetWorldGameId(const std::string& worldPath, bool canBeLegacy)
{
    std::string confPath = worldPath + "/world.mt";

	Settings conf;
	bool succeeded = conf.ReadConfigFile(confPath.c_str());
	if (!succeeded) 
    {
		if (canBeLegacy) 
        {
			// If map_meta.txt exists, it is probably an old minetest world
			if (FileSystem::Get()->ExistFile(ToWideString(worldPath) + L"/map_meta.txt"))
				return LEGACY_GAMEID;
		}
		return "";
	}
	if (!conf.Exists("gameid"))
		return "";
	// The "mesetint" gameid has been discarded
	if (conf.Get("gameid") == "mesetint")
		return "minetest";
	return conf.Get("gameid");
}

std::vector<WorldSpecification> GetAvailableWorlds()
{
    std::vector<WorldSpecification> worlds;
    std::set<std::string> worldpaths;

    std::string share = ToString(FileSystem::Get()->GetWorkingDirectory());
	worldpaths.insert(share + "/../../Assets/Art/Quake/maps");
	for (const std::string& worldpath : worldpaths) 
    {
		LogInformation("Searching world in " + worldpath + " : ");

		const std::set<char> ignore = { '.' };
		std::vector<std::wstring> paths;
		FileSystem::Get()->GetFileList(paths, ToWideString(worldpath), true);
		for (const std::wstring& path : paths)
		{
			std::string fileName = ToString(FileSystem::Get()->GetFileName(path));
			if (ignore.count(fileName[0]))
				continue;

			if (FileSystem::Get()->ExistDirectory(path))
			{
				std::string fullpath = ToString(path);
				std::string name = GetWorldName(fullpath, ToString(path));
				// Just allow filling in the gameid always for now
				bool canBeLegacy = true;
				std::string gameid = GetWorldGameId(fullpath, canBeLegacy);
				WorldSpecification spec(fullpath, name, gameid);
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

	GetGameMinetestConfig(game.mPath, *gameSettings);
    for (const auto& keyName : gameSettings->GetNames()) 
    {
        if (keyName.compare(0, 7, "secure.") != 0)
            continue;

        LogWarning("Secure setting " + keyName + " isn't allowed, so was ignored.");
        gameSettings->Remove(keyName);
    }

	LogInformation(L"Initializing world at " + finalPath);

	// Create world.mt if does not already exist
    std::wstring worldPath = finalPath + L"/world.mt";
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
