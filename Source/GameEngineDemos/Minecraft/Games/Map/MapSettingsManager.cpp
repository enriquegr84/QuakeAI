/*
Minetest
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#include "MapGenerator.h"
#include "MapSettingsManager.h"

#include "Core/IO/FileSystem.h"
#include "Core/Logger/Logger.h"

#include "Application/Settings.h"

MapSettingsManager::MapSettingsManager(const std::string& mapMetaPath) :
	mMapMetaPath(mapMetaPath)
{
	mMapSettings = Settings::CreateLayer(SL_MAP);
	MapGenerator::SetDefaultSettings(Settings::GetLayer(SL_DEFAULTS));
}


MapSettingsManager::~MapSettingsManager()
{
	delete mMapSettings;
	delete mMapGenParams;
}


bool MapSettingsManager::GetMapSetting(
	const std::string& name, std::string* valueOut)
{
	// Get from map_meta.txt, then try from all other sources
    try 
    {
        *valueOut = mMapSettings->Get(name);
        return true;
    }
    catch (SettingNotFoundException&) 
    {
        if (name == "seed")
        {
            *valueOut = Settings::GetLayer(SL_GLOBAL)->Get("fixed_map_seed");
            return true;
        }

        return false;
    }
}


bool MapSettingsManager::GetMapSettingNoiseParams(
	const std::string& name, NoiseParams* valueOut)
{
	// TODO: Rename to "getNoiseParams"
    mMapGenParams->GetNoiseParams(mMapSettings, name, *valueOut);
    return true;
}


bool MapSettingsManager::SetMapSetting(
	const std::string& name, const std::string& value, bool overrideMeta)
{
	if (mMapGenParams)
		return false;

	if (overrideMeta)
		mMapSettings->Set(name, value);
	else
		Settings::GetLayer(SL_GLOBAL)->Set(name, value);

	return true;
}


bool MapSettingsManager::SetMapSettingNoiseParams(
	const std::string& name, const NoiseParams* value, bool overrideMeta)
{
    if (mMapGenParams)
        return false;

	if (overrideMeta)
        mMapGenParams->SetNoiseParams(mMapSettings, name, *value);
	else
        mMapGenParams->SetNoiseParams(Settings::GetLayer(SL_GLOBAL), name, *value);

	return true;
}


bool MapSettingsManager::LoadMapMeta()
{
	std::ifstream is(mMapMetaPath, std::ios_base::binary);

	if (!is.good()) 
    {
		LogError("LoadMapMeta: could not open " + mMapMetaPath);
		return false;
	}

	if (!mMapSettings->ParseConfigLines(is)) 
    {
		LogError("LoadMapMeta: Format error. '[end_of_params]' missing?");
		return false;
	}

	return true;
}


bool MapSettingsManager::SaveMapMeta()
{
	// If mapgen params haven't been created yet; abort
	if (!mMapGenParams) 
    {
		LogError("SaveMapMeta: mMapGeneratorParams not present! "
            "Logic Startup was probably interrupted.");
		return false;
	}

	// Paths set up by subgames.cpp, but not in unittests
	if (!FileSystem::Get()->CreateAllDirectories(FileSystem::Get()->RemoveLastPathComponent(mMapMetaPath))) 
    {
		LogError("SaveMapMeta: could not create dirs to " + mMapMetaPath);
		return false;
	}

    mMapGenParams->WriteParams(mMapSettings);

	if (!mMapSettings->UpdateConfigFile(mMapMetaPath.c_str())) 
    {
		LogError("SaveMapMeta: could not write " + mMapMetaPath);
		return false;
	}

	return true;
}


MapGeneratorParams* MapSettingsManager::MakeMapGeneratorParams()
{
	if (mMapGenParams)
		return mMapGenParams;

	LogAssert(mMapSettings != NULL, "invalid setting");

	// At this point, we have (in order of precedence):
	// 1). SL_MAP containing map_meta.txt settings or
	//     explicit overrides from scripts
	// 2). SL_GLOBAL containing all user-specified config file
	//     settings
	// 3). SL_DEFAULTS containing any low-priority settings from
	//     scripts, e.g. mods using scripting as an enhanced config file)

	// Now, get the mapgen type so we can create the appropriate MapGeneratorParams
	std::string mgName;
	MapGeneratorType mgtype = GetMapSetting("mg_name", &mgName) ?
		MapGenerator::GetMapGeneratorType(mgName) : MAPGEN_DEFAULT;

	if (mgtype == MAPGEN_INVALID) 
    {
		LogWarning("EmergeManager: mapgen '" + mgName + "' not valid; falling back to " + 
            MapGenerator::GetMapGeneratorName(MAPGEN_DEFAULT));
		mgtype = MAPGEN_DEFAULT;
	}

	// Create our MapGeneratorParams
	MapGeneratorParams *params = MapGenerator::CreateMapGeneratorParams(mgtype);
	if (!params)
		return nullptr;

	params->mgtype = mgtype;

	// Load the rest of the mapgen params from our active settings
	params->MapGeneratorParams::ReadParams(mMapSettings);
	params->ReadParams(mMapSettings);

	// Hold onto our params
	mMapGenParams = params;

	return params;
}
