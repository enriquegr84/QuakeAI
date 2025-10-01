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

#ifndef MODS_H
#define MODS_H

#include "../Data/Metadata.h"

#define MODNAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz0123456789_"

struct Mod
{
	std::string name;
    std::string author;
    std::string path;
    std::string desc;
	int release = 0;

	// if normal mod:
    std::unordered_set<std::string> depends;
    std::unordered_set<std::string> optDepends;
    std::unordered_set<std::string> unsatisfiedDepends;

	bool partOfModpack = false;
	bool isModpack = false;

	// if modpack:
	std::map<std::string, Mod> modpackContent;
	Mod(const std::string& name = "", const std::string& path = "") :
        name(name), path(path)
	{
	}

	Mod(const std::string& name, const std::string& path, bool partOfModpack) : 
        name(name), path(path), partOfModpack(partOfModpack)
	{
	}
};

// Retrieves depends, optDepends, isModpack and modpackContent
void ParseModContents(Mod& mod);

std::map<std::string, Mod> GetModsInPath(
		const std::string& path, bool partOfModpack = false);

// replaces modpack Modspecs with their content
std::vector<Mod> FlattenMods(const std::map<std::string, Mod>& mods);

// a ModConfiguration is a subset of installed mods, expected to have
// all dependencies fullfilled, so it can be used as a list of mods to
// load when the game starts.
class ModConfiguration
{
public:
	// checks if all dependencies are fullfilled.
	bool IsConsistent() const { return mUnsatisfiedMods.empty(); }

	const std::vector<Mod>& GetMods() const { return mSortedMods; }

	const std::vector<Mod>& GetUnsatisfiedMods() const { return mUnsatisfiedMods; }

	void PrintUnsatisfiedModsError() const;

protected:

	ModConfiguration(const std::string& gamePath);
	// adds all mods in the given path. used for games, modpacks
	// and world-specific mods (worldmods-folders)
	void AddModsInPath(const std::string& path);

	// adds all mods in the set.
	void AddMods(const std::vector<Mod>& newMods);

	void AddModsFromConfig(const std::string& settingsPath, const std::set<std::string>& mods);

	void CheckConflictsAndDeps();

protected:
	// list of mods sorted such that they can be loaded in the
	// given order with all dependencies being fullfilled. I.e.,
	// every mod in this list has only dependencies on mods which
	// appear earlier in the vector.
	std::vector<Mod> mSortedMods;

private:
	// move mods from mUnsatisfiedMods to mSortedMods
	// in an order that satisfies dependencies
	void ResolveDependencies();

	// mods with unmet dependencies. Before dependencies are resolved,
	// this is where all mods are stored. Afterwards this contains
	// only the ones with really unsatisfied dependencies.
	std::vector<Mod> mUnsatisfiedMods;

	// set of mod names for which an unresolved name conflict
	// exists. A name conflict happens when two or more mods
	// at the same level have the same name but different paths.
	// Levels (mods in higher levels override mods in lower levels):
	// 1. game mod in modpack; 2. game mod;
	// 3. world mod in modpack; 4. world mod;
	// 5. addon mod in modpack; 6. addon mod.
    std::unordered_set<std::string> mNameConflicts;

	// Deleted default constructor
	ModConfiguration() = default;
};


class ModMetadata : public Metadata
{
public:
    ModMetadata() = delete;
    ModMetadata(const std::string& modName);
    ~ModMetadata() = default;

    virtual void Clear();

    bool Save(const std::string& rootPath);
    bool Load(const std::string& rootPath);

    bool IsModified() const { return mModified; }
    const std::string& GetModName() const { return mModName; }

    virtual bool SetString(const std::string& name, const std::string& var);

private:
    std::string mModName;
    bool mModified = false;
};



/**
 * Manage logic mods
 */
class ModManager : public ModConfiguration
{
public:
    /**
     * Creates a ModManager which targets gamePath
     * @param gamePath
     */
    ModManager(const std::string& gamePath);

    void LoadMods();
    void LoadMod(const std::wstring& modPath, const std::string& modName);

    const bool ModsLoaded() const { return mModsLoaded; }

    void GetModNames(std::vector<std::string>& modlist) const;
    const Mod* GetMod(const std::string& modname) const;
    /**
     * Recursively gets all paths of mod folders that can contain media files.
     *
     * Result is ordered in descending priority, ie. files from an earlier path
     * should not be replaced by files from a latter one.
     *
     * @param paths result vector
     */
    void GetModsMediaPaths(std::vector<std::wstring>& paths) const;

private:

    bool mModsLoaded = false;

};

#endif