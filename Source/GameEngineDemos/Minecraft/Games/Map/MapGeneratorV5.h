/*
Minetest
Copyright (C) 2014-2018 paramat
Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#ifndef MAPGENERATORV5_H
#define MAPGENERATORV5_H

#include "MapGenerator.h"

///////// MapGenerator V5 flags
#define MGV5_CAVERNS 0x01

class BiomeManager;

extern FlagDescription FlagdescMapGeneratorV5[];

struct MapGeneratorV5Params : public MapGeneratorParams
{
	float caveWidth = 0.09f;
	int16_t largeCaveDepth = -256;
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

	NoiseParams noiseParamsFillerDepth;
	NoiseParams noiseParamsFactor;
	NoiseParams noiseParamsHeight;
	NoiseParams noiseParamsGround;
	NoiseParams noiseParamsCave1;
	NoiseParams noiseParamsCave2;
	NoiseParams noiseParamsCavern;
	NoiseParams noiseParamsDungeons;

	MapGeneratorV5Params();
	~MapGeneratorV5Params() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};

class MapGeneratorV5 : public MapGeneratorBasic
{
public:
	MapGeneratorV5(MapGeneratorV5Params* params, EmergeParams* emerge);
	~MapGeneratorV5();

	virtual MapGeneratorType GetType() const { return MAPGEN_V5; }

	virtual void MakeChunk(BlockMakeData* data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);
	int GenerateBaseTerrain();

private:
	Noise* mNoiseFactor;
	Noise* mNoiseHeight;
	Noise* mNoiseGround;
};

#endif