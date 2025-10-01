/*
Minetest
Copyright (C) 2016-2019 Duane Robertson <duane@duanerobertson.com>
Copyright (C) 2016-2019 paramat

Based on Valleys MapGenerator by Gael de Sailly
(https://forum.minetest.net/viewtopic.php?f=9&t=11430)
and mapgen_v7, mapgen_flat by kwolekr and paramat.

Licensing changed by permission of Gael de Sailly.

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

#ifndef MAPGENERATORVALLEYS_H
#define MAPGENERATORVALLEYS_H

#include "MapGenerator.h"

#define MGVALLEYS_ALT_CHILL        0x01
#define MGVALLEYS_HUMID_RIVERS     0x02
#define MGVALLEYS_VARY_RIVER_DEPTH 0x04
#define MGVALLEYS_ALT_DRY          0x08

class BiomeManager;
class BiomeGeneratorOriginal;

extern FlagDescription FlagdescMapGeneratorValleys[];


struct MapGeneratorValleysParams : public MapGeneratorParams 
{
	uint16_t altitudeChill = 90;
	uint16_t riverDepth = 4;
	uint16_t riverSize = 5;

	float caveWidth = 0.09f;
	int16_t largeCaveDepth = -33;
	uint16_t smallCaveNumMin = 0;
	uint16_t smallCaveNumMax = 0;
	uint16_t largeCaveNumMin = 0;
	uint16_t largeCaveNumMax = 2;
	float largeCaveFlooded = 0.5f;
	int16_t cavernLimit = -256;
	int16_t cavernTaper = 192;
	float cavernThreshold = 0.6f;
	int16_t dungeonYmin = -31000;
	int16_t dungeonYmax = 63;

	NoiseParams noiseParamsFillerDepth;
	NoiseParams noiseParamsInterValleyFill;
	NoiseParams noiseParamsInterValleySlope;
	NoiseParams noiseParamsRivers;
	NoiseParams noiseParamsTerrainHeight;
	NoiseParams noiseParamsValleyDepth;
	NoiseParams noiseParamsValleyProfile;

	NoiseParams noiseParamsCave1;
	NoiseParams noiseParamsCave2;
	NoiseParams noiseParamsCavern;
	NoiseParams noiseParamsDungeons;

	MapGeneratorValleysParams();
	~MapGeneratorValleysParams() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};


class MapGeneratorValleys : public MapGeneratorBasic 
{
public:

	MapGeneratorValleys(MapGeneratorValleysParams* params, EmergeParams* emerge);
	~MapGeneratorValleys();

	virtual MapGeneratorType GetType() const { return MAPGEN_VALLEYS; }

	virtual void MakeChunk(BlockMakeData* data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);

private:
	BiomeGeneratorOriginal* mBiomeGen;

	float mAltitudeChill;
	float mRiverDepthBed;
	float mRiverSizeFactor;

	Noise* mNoiseInterValleyFill;
	Noise* mNoiseInterValleySlope;
	Noise* mNoiseRivers;
	Noise* mNoiseTerrainHeight;
	Noise* mNoiseValleyDepth;
	Noise* mNoiseValleyProfile;

	virtual int GenerateTerrain();
};

#endif