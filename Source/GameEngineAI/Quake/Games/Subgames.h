#ifndef SUBGAMES_H
#define SUBGAMES_H

#include "GameEngineStd.h"

struct Subgame
{
	std::string mId;
    std::string mName;
    std::string mAuthor;
	int mRelease;
	bool mModding;
    std::string mPath;
	std::string mRelativePath;
    std::string mIconPath;

	Subgame(const std::string& id = "", 
		const std::string& path = "",
		const std::string& relativePath = "",
		const std::string& name = "",
		const std::string& iconPath = "",
		const std::string& author = "", 
		int release = 0, bool modding = false) :
		mId(id), mName(name), mAuthor(author), mRelease(release), 
		mPath(path), mRelativePath(relativePath), mIconPath(iconPath), 
		mModding(modding)
	{
	}

	bool IsValid() const { return (!mId.empty() && !mPath.empty()); }
};

Subgame FindSubgame(const std::string& gamePath);

std::set<std::string> GetAvailableGamePaths();
std::vector<Subgame> GetAvailableGames();

bool GetWorldExists(const std::string& worldPath);

//! Try to get the displayed name of a world
std::string GetWorldName(const std::string& worldPath, const std::string& defaultName);

struct WorldSpec
{
    std::string mPath;
    std::string mName;

	WorldSpec(const std::string& path = "",  const std::string& name = "") :  mPath(path), mName(name)
	{
	}

	bool IsValid() const
	{
		return (!mName.empty() && !mPath.empty());
	}
};

std::vector<WorldSpec> GetAvailableWorlds();

// loads the subgame's config and creates world directory
// and world.mt if they don't exist
void LoadGameConfAndInitWorld(const std::string& path, 
	const std::string& name, const Subgame& game, bool createWorld);

#endif