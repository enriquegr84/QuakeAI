/*
Minetest
Copyright (C) 2017-2019 vlapsley, Vaughan Lapsley <vlapsley@gmail.com>
Copyright (C) 2017-2019 paramat

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

#ifndef MAPGENERATORCARPATHIAN_H
#define MAPGENERATORCARPATHIAN_H

#include "MapGenerator.h"

#define MGCARPATHIAN_CAVERNS 0x01
#define MGCARPATHIAN_RIVERS  0x02

class BiomeManager;

extern FlagDescription FlagdescMapGeneratorCarpathian[];

struct MapGeneratorCarpathianParams : public MapGeneratorParams
{
	float baseLevel = 12.0f;
	float riverWidth = 0.05f;
	float riverDepth = 24.0f;
	float valleyWidth = 0.25f;

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

	NoiseParams noiseParamsFillerDepth;
	NoiseParams noiseParamsHeight1;
	NoiseParams noiseParamsHeight2;
	NoiseParams noiseParamsHeight3;
	NoiseParams noiseParamsHeight4;
	NoiseParams noiseParamsHillsTerrain;
	NoiseParams noiseParamsRidgeTerrain;
	NoiseParams noiseParamsStepTerrain;
	NoiseParams noiseParamsHills;
	NoiseParams noiseParamsRidgeMnt;
	NoiseParams noiseParamsStepMnt;
	NoiseParams noiseParamsRivers;
	NoiseParams noiseParamsMntVar;
	NoiseParams noiseParamsCave1;
	NoiseParams noiseParamsCave2;
	NoiseParams noiseParamsCavern;
	NoiseParams noiseParamsDungeons;

	MapGeneratorCarpathianParams();
	~MapGeneratorCarpathianParams() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};

class MapGeneratorCarpathian : public MapGeneratorBasic
{
public:
	MapGeneratorCarpathian(MapGeneratorCarpathianParams* params, EmergeParams* emerge);
	~MapGeneratorCarpathian();

	virtual MapGeneratorType GetType() const { return MAPGEN_CARPATHIAN; }

	virtual void MakeChunk(BlockMakeData *data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);

private:
	float mBaseLevel;
	float mRiverWidth;
	float mRiverDepth;
	float mValleyWidth;

	Noise* mNoiseHeight1;
	Noise* mNoiseHeight2;
	Noise* mNoiseHeight3;
	Noise* mNoiseHeight4;
	Noise* mNoiseHillsTerrain;
	Noise* mNoiseRidgeTerrain;
	Noise* mNoiseStepTerrain;
	Noise* mNoiseHills;
	Noise* mNoiseRidgeMnt;
	Noise* mNoiseStepMnt;
	Noise* mNoiseRivers = nullptr;
	Noise* mNoiseMntVar;

	int mGradWL;

	float GetSteps(float noise);
	inline float GetLerp(float noise1, float noise2, float mod);
	int GenerateTerrain();
};

#endif