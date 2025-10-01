/*
Minetest
Copyright (C) 2015-2020 paramat
Copyright (C) 2015-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#ifndef MAPGENERATORFLAT_H
#define MAPGENERATORFLAT_H

#include "MapGenerator.h"

/////// MapGenerator Flat flags
#define MGFLAT_LAKES 0x01
#define MGFLAT_HILLS 0x02
#define MGFLAT_CAVERNS 0x04

class BiomeManager;

extern FlagDescription FlagdescMapGeneratorFlat[];

struct MapGeneratorFlatParams : public MapGeneratorParams
{
	int16_t groundLevel = 8;
	float lakeThreshold = -0.45f;
	float lakeSteepness = 48.0f;
	float hillThreshold = 0.45f;
	float hillSteepness = 64.0f;

	float caveWidth = 0.09f;
	uint16_t smallCaveNumMin = 0;
	uint16_t smallCaveNumMax = 0;
	uint16_t largeCaveNumMin = 0;
	uint16_t largeCaveNumMax = 2;
	int16_t largeCaveDepth = -33;
	float largeCaveFlooded = 0.5f;
	int16_t cavernLimit = -256;
	int16_t cavernTaper = 256;
	float cavernThreshold = 0.7f;
	int16_t dungeonYmin = -31000;
	int16_t dungeonYmax = 31000;

	NoiseParams noiseParamsTerrain;
	NoiseParams noiseParamsFillerDepth;
	NoiseParams noiseParamsCavern;
	NoiseParams noiseParamsCave1;
	NoiseParams noiseParamsCave2;
	NoiseParams noiseParamsDungeons;

	MapGeneratorFlatParams();
	~MapGeneratorFlatParams() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};

class MapGeneratorFlat : public MapGeneratorBasic
{
public:
	MapGeneratorFlat(MapGeneratorFlatParams* params, EmergeParams* emerge);
	~MapGeneratorFlat();

	virtual MapGeneratorType GetType() const { return MAPGEN_FLAT; }

	virtual void MakeChunk(BlockMakeData *data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);
	int16_t GenerateTerrain();

private:
	int16_t mGroundLevel;
	float mLakeThreshold;
	float mLakeSteepness;
	float mHillThreshold;
	float mHillSteepness;

	Noise* mNoiseTerrain;
};

#endif