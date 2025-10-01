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

#include "Mods.h"
#include "Subgames.h"

#include "Core/OS/OS.h"
#include "Core/Logger/Logger.h"
#include "Core/IO/FileSystem.h"

#include "Application/Settings.h"

bool ParseDependsString(std::string& dep, std::unordered_set<char>& symbols)
{
	dep = Trim(dep);
	symbols.clear();
	size_t pos = dep.size();
	while (pos > 0 && !StringAllowed(dep.substr(pos - 1, 1), MODNAME_ALLOWED_CHARS)) 
    {
		// last character is a symbol, not part of the modname
		symbols.insert(dep[pos - 1]);
		--pos;
	}
	dep = Trim(dep.substr(0, pos));
	return !dep.empty();
}

void ParseModContents(Mod& mod)
{
	// NOTE: this function works in mutual recursion with GetModsInPath
	mod.depends.clear();
	mod.optDepends.clear();
	mod.isModpack = false;
	mod.modpackContent.clear();

	// Handle modpacks (defined by containing modpack.txt)
	std::ifstream modpackIs((mod.path + "\\modpack.txt"));
	std::ifstream modpack2Is((mod.path + "\\modpack.conf"));
	if (modpackIs.good() || modpack2Is.good()) 
    {
		if (modpackIs.good())
			modpackIs.close();

		if (modpack2Is.good())
			modpack2Is.close();

		mod.isModpack = true;
		mod.modpackContent = GetModsInPath(mod.path, true);

	} 
    else 
    {
		Settings info;
		info.ReadConfigFile((mod.path + "\\mod.conf").c_str());

		if (info.Exists("name"))
			mod.name = info.Get("name");

		if (info.Exists("author"))
			mod.author = info.Get("author");

		if (info.Exists("release"))
			mod.release = info.GetInt("release");

		// Attempt to load dependencies from mod.conf
		bool modConfHasDepends = false;
		if (info.Exists("depends")) 
        {
			modConfHasDepends = true;
			std::string dep = info.Get("depends");
			// clang-format off
			dep.erase(std::remove_if(dep.begin(), dep.end(),
					static_cast<int (*)(int)>(&std::isspace)), dep.end());
			// clang-format on
			for (const auto& dependency : StringSplit(dep, ','))
				mod.depends.insert(dependency);
		}

		if (info.Exists("optional_depends")) 
        {
			modConfHasDepends = true;
			std::string dep = info.Get("optional_depends");
			// clang-format off
			dep.erase(std::remove_if(dep.begin(), dep.end(),
					static_cast<int (*)(int)>(&std::isspace)), dep.end());
			// clang-format on
			for (const auto& dependency : StringSplit(dep, ','))
				mod.optDepends.insert(dependency);
		}

		// Fallback to depends.txt
		if (!modConfHasDepends) 
        {
			std::vector<std::string> dependencies;

            std::ifstream is((mod.path + "\\depends.txt"));

			while (is.good()) 
            {
				std::string dep;
				std::getline(is, dep);
				dependencies.push_back(dep);
			}

			for (auto& dependency : dependencies) 
            {
				std::unordered_set<char> symbols;
				if (ParseDependsString(dependency, symbols)) 
                {
					if (symbols.count('?') != 0)
						mod.optDepends.insert(dependency);
                    else
						mod.depends.insert(dependency);
				}
			}
		}

		if (info.Exists("description"))
			mod.desc = info.Get("description");
		else if (FileSystem::Get()->ExistFile(ToWideString(mod.path) + L"\\description.txt"))
			LogWarning("description.txt is deprecated, please use mod.conf instead.");
	}
}

std::map<std::string, Mod> GetModsInPath(const std::string& path, bool partOfModpack)
{
	// NOTE: this function works in mutual recursion with ParseModContents
    std::map<std::string, Mod> result;

    std::vector<std::wstring> fileList;
    FileSystem::Get()->GetFileList(fileList, ToWideString(path));
    
	std::string modPath;
	for (const std::wstring& file : fileList)
    {
		const std::string& modName = ToString(file);
		// Ignore all directories beginning with a ".", emodially
		// VCS directories like ".git" or ".svn"
		if (modName[0] == '.')
			continue;

		modPath.clear();
		modPath.append(path).append("\\").append(modName);

		Mod mod(modName, modPath, partOfModpack);
		ParseModContents(mod);
		result.insert(std::make_pair(modName, mod));
	}
	return result;
}

std::vector<Mod> FlattenMods(const std::map<std::string, Mod>& mods)
{
    std::vector<Mod> result;
	for (const auto& it : mods) 
    {
		const Mod& mod = it.second;
		if (mod.isModpack) 
        {
            std::vector<Mod> content = FlattenMods(mod.modpackContent);
			result.reserve(result.size() + content.size());
			result.insert(result.end(), content.begin(), content.end());
		}
        else // not a modpack
		{
			result.push_back(mod);
		}
	}
	return result;
}

ModConfiguration::ModConfiguration(const std::string& gamePath)
{

}

void ModConfiguration::PrintUnsatisfiedModsError() const
{
	for (const Mod& mod : mUnsatisfiedMods) 
    {
		LogError("mod \"" + mod.name + "\" has unsatisfied dependencies: ");
		for (const std::string& unsatisfiedDepend : mod.unsatisfiedDepends)
			LogError(" \"" + unsatisfiedDepend + "\"");
	}
}

void ModConfiguration::AddModsInPath(const std::string& path)
{
	AddMods(FlattenMods(GetModsInPath(path)));
}

void ModConfiguration::AddMods(const std::vector<Mod>& newMods)
{
	// Maintain a map of all existing mUnsatisfiedMods.
	// Keys are mod names and values are indices into mUnsatisfiedMods.
    std::map<std::string, unsigned int> existingMods;
	for (unsigned int i = 0; i < mUnsatisfiedMods.size(); ++i)
		existingMods[mUnsatisfiedMods[i].name] = i;

	// Add new mods
	for (int wantFromModpack = 1; wantFromModpack >= 0; --wantFromModpack) 
    {
		// First iteration:
		// Add all the mods that come from modpacks
		// Second iteration:
		// Add all the mods that didn't come from modpacks
        std::set<std::string> seenThisIteration;

		for (const Mod& mod : newMods) 
        {
			if (mod.partOfModpack != (bool)wantFromModpack)
				continue;

			if (existingMods.count(mod.name) == 0)
            {
				// GOOD CASE: completely new mod.
				mUnsatisfiedMods.push_back(mod);
				existingMods[mod.name] = (unsigned int)mUnsatisfiedMods.size() - 1;
			} 
            else if (seenThisIteration.count(mod.name) == 0) 
            {
				// BAD CASE: name conflict in different levels.
				unsigned int oldindex = existingMods[mod.name];
				const Mod& oldmod = mUnsatisfiedMods[oldindex];
				LogWarning("Mod name conflict detected: \"" + mod.name + "\"" + 
                    "\nWill not load: " + oldmod.path + "\nOverridden by: " + mod.path);
				mUnsatisfiedMods[oldindex] = mod;

				// If there was a "VERY BAD CASE" name conflict
				// in an earlier level, ignore it.
				mNameConflicts.erase(mod.name);
			} 
            else 
            {
				// VERY BAD CASE: name conflict in the same level.
				unsigned int oldindex = existingMods[mod.name];
				const Mod& oldmod = mUnsatisfiedMods[oldindex];
                LogWarning("Mod name conflict detected: \"" + mod.name + "\"" +
                    "\nWill not load: " + oldmod.path + "\nOverridden by: " + mod.path);
				mUnsatisfiedMods[oldindex] = mod;
				mNameConflicts.insert(mod.name);
			}

			seenThisIteration.insert(mod.name);
		}
	}
}

void ModConfiguration::AddModsFromConfig(
		const std::string& settingsPath, const std::set<std::string>& mods)
{
	Settings conf;
    std::set<std::string> loadModNames;

	conf.ReadConfigFile(settingsPath.c_str());
    std::vector<std::string> names = conf.GetNames();
	for (const std::string& name : names) 
    {
		if (name.compare(0, 9, "load_mod_") == 0 && 
            conf.Get(name) != "false" &&
            conf.Get(name) != "nil")
			loadModNames.insert(name.substr(9));
	}

    std::vector<Mod> addonMods;
	for (const std::string& mod : mods) 
    {
        std::vector<Mod> addonModsInPath = FlattenMods(GetModsInPath(mod));
        std::vector<Mod>::const_iterator it;
		for (it = addonModsInPath.begin(); it != addonModsInPath.end(); ++it) 
        {
			const Mod& mod = *it;
			if (loadModNames.count(mod.name) != 0)
				addonMods.push_back(mod);
			else
				conf.SetBool("load_mod_" + mod.name, false);
		}
	}
	conf.UpdateConfigFile(settingsPath.c_str());

	AddMods(addonMods);
	CheckConflictsAndDeps();

	// complain about mods declared to be loaded, but not found
	for (const Mod &addon_mod : addonMods)
		loadModNames.erase(addon_mod.name);

    std::vector<Mod> unsatisfiedMods = GetUnsatisfiedMods();

	for (const Mod &unsatisfiedMod : unsatisfiedMods)
		loadModNames.erase(unsatisfiedMod.name);

	if (!loadModNames.empty()) 
    {
		LogError("The following mods could not be found:");
		for (const std::string& mod : loadModNames)
			LogError(" \"" + mod + "\"");
	}
}

void ModConfiguration::CheckConflictsAndDeps()
{
	// report on name conflicts
	if (!mNameConflicts.empty()) 
    {
        std::string s = "Unresolved name conflicts for mods ";
        std::unordered_set<std::string>::const_iterator it;
		for (it = mNameConflicts.begin(); it != mNameConflicts.end(); ++it) 
        {
			if (it != mNameConflicts.begin())
				s += ", ";
			s += std::string("\"") + (*it) + "\"";
		}
		s += ".";
		throw ModError(s);
	}

	// get the mods in order
	ResolveDependencies();
}

void ModConfiguration::ResolveDependencies()
{
	// Step 1: Compile a list of the mod names we're working with
    std::set<std::string> modnames;
	for (const Mod& mod : mUnsatisfiedMods)
		modnames.insert(mod.name);

	// Step 2: get dependencies (including optional dependencies)
	// of each mod, split mods into satisfied and unsatisfied
    std::list<Mod> satisfied;
    std::list<Mod> unsatisfied;
	for (Mod mod : mUnsatisfiedMods) 
    {
		mod.unsatisfiedDepends = mod.depends;
		// check which optional dependencies actually exist
		for (const std::string& optdep : mod.optDepends) 
			if (modnames.count(optdep) != 0)
				mod.unsatisfiedDepends.insert(optdep);

		// if a mod has no depends it is initially satisfied
		if (mod.unsatisfiedDepends.empty())
			satisfied.push_back(mod);
		else
			unsatisfied.push_back(mod);
	}

	// Step 3: mods without unmet dependencies can be appended to
	// the sorted list.
	while (!satisfied.empty()) 
    {
		Mod mod = satisfied.back();
		mSortedMods.push_back(mod);
		satisfied.pop_back();
		for (auto it = unsatisfied.begin(); it != unsatisfied.end();) 
        {
			Mod &mod2 = *it;
			mod2.unsatisfiedDepends.erase(mod.name);
			if (mod2.unsatisfiedDepends.empty()) 
            {
				satisfied.push_back(mod2);
				it = unsatisfied.erase(it);
			} 
            else ++it;
		}
	}

	// Step 4: write back list of unsatisfied mods
	mUnsatisfiedMods.assign(unsatisfied.begin(), unsatisfied.end());
}

/**
 * Manage logic mods
 */

 /**
  * Creates a ModManager which targets gamePath
  * @param gamePath
  */
ModManager::ModManager(const std::string& gamePath) : ModConfiguration(gamePath)
{
    Subgame game = FindWorldSubgame(gamePath);

    // Add all game mods and all world mods
	/*
    AddModsInPath(game.mGameModsPath);
    AddModsInPath(gamePath + "/mod");

    // Load normal mods
    std::string worldmt = gamePath + "/map/world.mt";
    AddModsFromConfig(worldmt, game.mAddonModsPaths);
	*/
}

// clang-format off
// This function cannot be currenctly easily tested but it should be ASAP
void ModManager::LoadMods()
{
    // Don't load mods twice.
    if (mModsLoaded || !Settings::Get()->GetBool("enable_modding"))
        return;

    // Print mods
    std::string modInfo = "Loading mods: ";
    for (const Mod& mod : mSortedMods)
        modInfo += mod.name + " ";
    LogInformation(modInfo);

    // Load and run "mod" scripts
    for (const Mod& mod : mSortedMods) 
    {
        if (mod.name.find_first_not_of(MODNAME_ALLOWED_CHARS) == mod.name.npos)
        {
            LogError("Error loading mod \"" + mod.name +
                "\": Mod name does not follow naming conventions: "
                "Only characters [a-z0-9_] are allowed.");
        }

        std::vector<std::wstring> fileList;
        FileSystem::Get()->GetFileList(fileList, ToWideString(mod.path), true);
        for (const std::wstring& filePath : fileList)
        {
            unsigned int time = Timer::GetRealTime();
            LoadMod(filePath, mod.name);
            LogInformation("Mod \"" + mod.name + "\" loaded after " +
                std::to_string(Timer::GetRealTime() - time) + " ms");
        }
    }

    // Mods are done loading. Unlock callbacks
    mModsLoaded = true;

    // Run a callback when mods are loaded
    //script->on_mods_loaded();
}


void ModManager::LoadMod(const std::wstring& modPath, const std::string& modName)
{

}

// clang-format on
const Mod* ModManager::GetMod(const std::string& modname) const
{
    std::vector<Mod>::const_iterator it;
    for (it = mSortedMods.begin(); it != mSortedMods.end(); ++it) 
    {
        const Mod& mod = *it;
        if (mod.name == modname)
            return &mod;
    }
    return NULL;
}

void ModManager::GetModNames(std::vector<std::string>& modlist) const
{
    for (const Mod& mod : mSortedMods)
        modlist.push_back(mod.name);
}

void ModManager::GetModsMediaPaths(std::vector<std::wstring>& paths) const
{

}

ModMetadata::ModMetadata(const std::string& modName) : mModName(modName)
{
}

void ModMetadata::Clear()
{
	Metadata::Clear();
	mModified = true;
}

bool ModMetadata::Save(const std::string& rootPath)
{
	json object;
	for (StringMap::const_iterator it = mStringVars.begin(); it != mStringVars.end(); ++it)
		object[it->first.c_str()] = it->second.c_str();

	if (!FileSystem::Get()->ExistFile(ToWideString(rootPath)))
    {
		if (!FileSystem::Get()->CreateAllDirectories(rootPath))
        {
			LogWarning("ModMetadata[" + mModName
				    + "]: Unable to save. '" + rootPath
				    + "' tree cannot be created.");
			return false;
		}
	} 
    else if (!FileSystem::Get()->ExistDirectory(ToWideString(rootPath)))
    {
		LogError("ModMetadata[" + mModName + "]: Unable to save. '"
			    + rootPath + "' is not a directory.");
		return false;
	}

    bool writeOk = FileSystem::Get()->SafeWriteToFile(rootPath + "\\" + mModName, object.dump());

	if (writeOk)
		mModified = false;
    else
		LogError("ModMetadata[" + mModName + "]: failed write file.");

	return writeOk;
}

bool ModMetadata::Load(const std::string& rootPath)
{
	mStringVars.clear();

	std::ifstream is(rootPath + "\\" + mModName, std::ios_base::binary);
	if (!is.good())
		return false;

	json root;
	root["collectComments"] = false;
	std::string errs;

	is >> root;
	/*
    {
		LogError("ModMetadata[" + mModName + 
            "]: failed read data (Json decoding failure). Message: " + errs);
		return false;
	}
	*/
	for (const auto& item : root.items()) 
		mStringVars[item.key()] = item.value();

	return true;
}

bool ModMetadata::SetString(const std::string& name, const std::string& var)
{
	mModified = Metadata::SetString(name, var);
	return mModified;
}
