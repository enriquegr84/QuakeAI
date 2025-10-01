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

#ifndef MAPSETTINGSMANAGER_H
#define MAPSETTINGSMANAGER_H

#include "GameEngineStd.h"

class Settings;
struct NoiseParams;
struct MapGeneratorParams;

/*
	MapSettingsManager is a centralized object for management (creating,
	loading, storing, saving, etc.) of config settings related to the Map.

	It has two phases: the initial r/w "gather and modify settings" state, and
	the final r/o "read and save settings" state.

	The typical use case is, in order, as follows:
	- Create a MapSettingsManager object
	- Try to load map metadata into it from the metadata file
	- Manually view and modify the current configuration as desired through a
	  Settings-like interface
	- When all modifications are finished, create a 'Parameters' object
	  containing the finalized, active parameters.  This could be passed along
	  to whichever Map-related objects that may require it.
	- Save these active settings to the metadata file when requested
*/
class MapSettingsManager 
{
public:
	MapSettingsManager(const std::string& mapMetaPath);
	~MapSettingsManager();

	// Finalized map generation parameters
	MapGeneratorParams* mMapGenParams = nullptr;

	bool GetMapSetting(const std::string& name, std::string* valueOut);
	bool GetMapSettingNoiseParams(const std::string& name, NoiseParams* valueOut);

	// Note: Map config becomes read-only after MakeMapGeneratorParams() gets called
	// (i.e. mapGenParams is non-NULL).  Attempts to set map config after
	// params have been finalized will result in failure.
	bool SetMapSetting(const std::string& name,
		const std::string& value, bool overrideMeta = false);

	bool SetMapSettingNoiseParams(const std::string& name,
		const NoiseParams* value, bool overrideMeta = false);

	bool LoadMapMeta();
	bool SaveMapMeta();
	MapGeneratorParams* MakeMapGeneratorParams();

private:
	std::string mMapMetaPath;
	// TODO: Rename to "m_settings"
	Settings* mMapSettings;
};

#endif