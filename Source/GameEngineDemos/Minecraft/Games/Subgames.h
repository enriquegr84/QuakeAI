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

#ifndef SUBGAMES_H
#define SUBGAMES_H

#include "GameEngineStd.h"

struct Subgame
{
	std::string mId;
    std::string mName;
    std::string mAuthor;
	int mRelease;
    std::string mPath;
	std::string mRelativePath;
    std::string mGameModsPath;
    std::set<std::string> mAddonModsPaths;
    std::string mMenuIconPath;

	Subgame(const std::string& id = "", 
		const std::string& path = "",
		const std::string& relativePath = "",
		const std::string& gameModsPath = "",
		const std::set<std::string>& addonModsPaths = std::set<std::string>(),
		const std::string& name = "",
		const std::string& menuIconPath = "",
		const std::string& author = "", int release = 0) :
		mId(id), mName(name), mAuthor(author), mRelease(release), mPath(path), mRelativePath(relativePath),
		mGameModsPath(gameModsPath), mAddonModsPaths(addonModsPaths), mMenuIconPath(menuIconPath)
	{
	}

	bool IsValid() const { return (!mId.empty() && !mPath.empty()); }
};

Subgame FindSubgame(const std::string& id);
Subgame FindWorldSubgame(const std::string& gamePath);

std::set<std::string> GetAvailableGameIds();
std::vector<Subgame> GetAvailableGames();

bool GetWorldExists(const std::string& worldPath);

//! Try to get the displayed name of a world
std::string GetWorldName(const std::string& worldPath, const std::string& defaultName);
std::string GetWorldGameId(const std::string& worldPath, bool canBeLegacy = false);

struct WorldSpec
{
    std::string mPath;
    std::string mName;
    std::string mGameId;

	WorldSpec(const std::string& path = "", 
		const std::string& name = "", const std::string& gameid = "") : 
		mPath(path), mName(name), mGameId(gameid)
	{
	}

	bool IsValid() const
	{
		return (!mName.empty() && !mPath.empty() && !mGameId.empty());
	}
};

std::vector<WorldSpec> GetAvailableWorlds();

// loads the subgame's config and creates world directory
// and world.mt if they don't exist
void LoadGameConfAndInitWorld(const std::string& path, 
	const std::string& name, const Subgame& game, bool createWorld);

#endif