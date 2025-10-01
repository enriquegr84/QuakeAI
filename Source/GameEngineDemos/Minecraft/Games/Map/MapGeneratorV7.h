/*
Minetest
Copyright (C) 2014-2020 paramat
Copyright (C) 2013-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#ifndef MAPGENERATORV7_H
#define MAPGENERATORV7_H

#include "MapGenerator.h"

///////////// MapGenerator V7 flags
#define MGV7_MOUNTAINS   0x01
#define MGV7_RIDGES      0x02
#define MGV7_FLOATLANDS  0x04
#define MGV7_CAVERNS     0x08
#define MGV7_BIOMEREPEAT 0x10 // Now unused

class BiomeManager;

extern FlagDescription FlagdescMapGeneratorV7[];


struct MapGeneratorV7Params : public MapGeneratorParams 
{
	int16_t mountZeroLevel = 0;
	int16_t floatLandYmin = 1024;
	int16_t floatLandYmax = 4096;
	int16_t floatLandTaper = 256;
	float floatTaperExp = 2.0f;
	float floatLandDensity = -0.6f;
	int16_t floatLandYwater = -31000;

	float caveWidth = 0.09f;
	int16_t largeCaveDepth = -33;
	uint16_t smallCaveNumMin = 0;
	uint16_t smallCaveNumMax = 0;
	uint16_t largeCaveNumMin = 0;
	uint16_t largeCaveNumMax = 2;
	float largeCaveFlooded = 0.5f;
	int16_t cavernLimit = -256;
	int16_t cavernTaper = 256;
	float cavernThreshold = 0.7f;
	int16_t dungeonYmin = -31000;
	int16_t dungeonYmax = 31000;

	NoiseParams noiseParamsTerrainBase;
	NoiseParams noiseParamsTerrainAlt;
	NoiseParams noiseParamsTerrainPersist;
	NoiseParams noiseParamsHeightSelect;
	NoiseParams noiseParamsFillerDepth;
	NoiseParams noiseParamsMountHeight;
	NoiseParams noiseParamsRidgeUWater;
	NoiseParams noiseParamsMountain;
	NoiseParams noiseParamsRidge;
	NoiseParams noiseParamsFloatLand;
	NoiseParams noiseParamsCavern;
	NoiseParams noiseParamsCave1;
	NoiseParams noiseParamsCave2;
	NoiseParams noiseParamsDungeons;

	MapGeneratorV7Params();
	~MapGeneratorV7Params() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};


class MapGeneratorV7 : public MapGeneratorBasic 
{
public:
	MapGeneratorV7(MapGeneratorV7Params* params, EmergeParams* emerge);
	~MapGeneratorV7();

	virtual MapGeneratorType GetType() const { return MAPGEN_V7; }

	virtual void MakeChunk(BlockMakeData* data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);

	float BaseTerrainLevelAtPoint(int16_t x, int16_t z);
	float BaseTerrainLevelFromMap(int index);
	bool GetMountainTerrainAtPoint(int16_t x, int16_t y, int16_t z);
	bool GetMountainTerrainFromMap(int idx_xyz, int idx_xz, int16_t y);
	bool GetRiverChannelFromMap(int idx_xyz, int idx_xz, int16_t y);
	bool GetFloatlandTerrainFromMap(int idx_xyz, float floatOffset);

	int GenerateTerrain();

private:
	int16_t mMountZeroLevel;
	int16_t mFloatLandYmin;
	int16_t mFloatLandYmax;
	int16_t mFloatLandTaper;
	float mFloatTaperExp;
	float mFloatLandDensity;
	int16_t mFloatLandYwater;

	float* mFloatOffsetCache = nullptr;

	Noise* mNoiseTerrainBase;
	Noise* mNoiseTerrainAlt;
	Noise* mNoiseTerrainPersist;
	Noise* mNoiseHeightSelect;
	Noise* mNoiseMountHeight;
	Noise* mNoiseRidgeUWater;
	Noise* mNoiseMountain;
	Noise* mNoiseRidge;
	Noise* mNoiseFloatLand;
};

#endif