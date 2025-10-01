//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "MinecraftLevel.h"
#include "MinecraftLevelManager.h"

#include "Core/IO/FileSystem.h"
#include "Core/Logger/Logger.h"

MinecraftLevelManager::MinecraftLevelManager() : LevelManager()
{

}   // MinecraftLevelManager

//-----------------------------------------------------------------------------
/** Delete all miencraft levels.
 */
MinecraftLevelManager::~MinecraftLevelManager()
{

}   // ~MinecraftLevelManager

// ----------------------------------------------------------------------------
/** Tries to load a level from a single directory. Returns true if a level was
*  successfully loaded.
*  \param dirname Name of the directory to load the level from.
*/
bool MinecraftLevelManager::LoadLevel(const std::wstring& levelname)
{
	if (!FileSystem::Get()->ExistFile(levelname))
		return false;

	MinecraftLevel* level;

	try
	{
		level = new MinecraftLevel(levelname);
	}
	catch (std::exception)
	{
		LogError(L"Cannot load level " + levelname);
		return false;
	}

	mAllLevelDirs.push_back(FileSystem::Get()->GetFileDirectory(levelname));
	mLevels.push_back(level);
	mLevelAvailables.push_back(true);
	return true;
}   // LoadLevel