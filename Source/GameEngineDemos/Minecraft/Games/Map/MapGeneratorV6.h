/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2014-2018 paramat

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

#ifndef MAPGENERATORV6_H
#define MAPGENERATORV6_H

#include "MapGenerator.h"

#include "../../Utils/Noise.h"

#define MGV6_AVERAGE_MUD_AMOUNT 4
#define MGV6_DESERT_STONE_BASE -32
#define MGV6_ICE_BASE 0
#define MGV6_FREQ_HOT 0.4
#define MGV6_FREQ_SNOW -0.4
#define MGV6_FREQ_TAIGA 0.5
#define MGV6_FREQ_JUNGLE 0.5

//////////// MapGenerator V6 flags
#define MGV6_JUNGLES    0x01
#define MGV6_BIOMEBLEND 0x02
#define MGV6_MUDFLOW    0x04
#define MGV6_SNOWBIOMES 0x08
#define MGV6_FLAT       0x10
#define MGV6_TREES      0x20


extern FlagDescription FlagdescMapGeneratorV6[];


enum BiomeV6Type
{
	BT_NORMAL,
	BT_DESERT,
	BT_JUNGLE,
	BT_TUNDRA,
	BT_TAIGA,
};


struct MapGeneratorV6Params : public MapGeneratorParams 
{
	float freqDesert = 0.45f;
	float freqBeach = 0.15f;
	int16_t dungeonYmin = -31000;
	int16_t dungeonYmax = 31000;

	NoiseParams noiseParamsTerrainBase;
	NoiseParams noiseParamsTerrainHigher;
	NoiseParams noiseParamsSteepness;
	NoiseParams noiseParamsHeightSelect;
	NoiseParams noiseParamsMud;
	NoiseParams noiseParamsBeach;
	NoiseParams noiseParamsBiome;
	NoiseParams noiseParamsCave;
	NoiseParams noiseParamsHumidity;
	NoiseParams noiseParamsTrees;
	NoiseParams noiseParamsAppleTrees;

	MapGeneratorV6Params();
	~MapGeneratorV6Params() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};


class MapGeneratorV6 : public MapGenerator 
{
public:
	EmergeParams* mEmerge;

	int mYstride;
	unsigned int mSPFlags;

	Vector3<short> mNodeMin;
	Vector3<short> mNodeMax;
	Vector3<short> mFullNodeMin;
	Vector3<short> mFullNodeMax;
	Vector3<short> mCentralAreaSize;

	Noise* mNoiseTerrainBase;
	Noise* mNoiseTerrainHigher;
	Noise* mNoiseSteepness;
	Noise* mNoiseHeightSelect;
	Noise* mNoiseMud;
	Noise* mNoiseBeach;
	Noise* mNoiseBiome;
	Noise* mNoiseHumidity;
	NoiseParams* mNoiseParamsCave;
	NoiseParams* mNoiseParamsHumidity;
	NoiseParams* mNoiseParamsTrees;
	NoiseParams* mNoiseParamsAppleTrees;

	NoiseParams mNoiseParamsDungeons;

	float mFreqDesert;
	float mFreqBeach;
	int16_t mDungeonYmin;
	int16_t mDungeonYmax;

	uint16_t mContentStone;
	uint16_t mContentDirt;
	uint16_t mContentDirtWithGrass;
	uint16_t mContentSand;
	uint16_t mContentWaterSource;
	uint16_t mContentLavaSource;
	uint16_t mContentGravel;
	uint16_t mContentDesertStone;
	uint16_t mContentDesertSand;
	uint16_t mContentDirtWithSnow;
	uint16_t mContentSnow;
	uint16_t mContentSnowblock;
	uint16_t mContentIce;

	uint16_t mContentCobble;
	uint16_t mContentMossyCobble;
	uint16_t mContentStairCobble;
	uint16_t mContentStairDesertStone;

	MapGeneratorV6(MapGeneratorV6Params* params, EmergeParams* emerge);
	~MapGeneratorV6();

	virtual MapGeneratorType GetType() const { return MAPGEN_V6; }

	void MakeChunk(BlockMakeData* data);
	int GetGroundLevelAtPoint(Vector2<short> pos);
	int GetSpawnLevelAtPoint(Vector2<short> pos);

	float BaseTerrainLevel(float terrainBase, float terrainHigher,
		float steepness, float heightSelect);
	virtual float BaseTerrainLevelFromNoise(Vector2<short> pos);
	virtual float BaseTerrainLevelFromMap(Vector2<short> pos);
	virtual float BaseTerrainLevelFromMap(int index);

	int16_t FindStoneLevel(Vector2<short> p2d);
	bool BlockIsUnderground(uint64_t seed, Vector3<short> blockpos);

	float GetHumidity(Vector2<short> pos);
	float GetTreeAmount(Vector2<short> pos);
	bool GetHaveAppleTree(Vector2<short> pos);
	float GetMudAmount(Vector2<short> pos);
	virtual float GetMudAmount(int index);
	bool GetHaveBeach(Vector2<short> pos);
	bool GetHaveBeach(int index);
	BiomeV6Type GetBiome(Vector2<short> pos);
	BiomeV6Type GetBiome(int index, Vector2<short> pos);

	unsigned int GetBlockSeed(uint64_t seed, Vector3<short> pos);

	virtual void CalculateNoise();
	int GenerateGround();
	void AddMud();
	void FlowMud(int16_t& mudFlowMinPos, int16_t& mudFlowMaxPos);
	void MoveMud(unsigned int removeIndex, unsigned int placeIndex,
		unsigned int aboveRemoveIndex, Vector2<short> pos, Vector3<short> em);
	void GrowGrass();
	void PlaceTreesAndJungleGrass();
	virtual void GenerateCaves(int maxStoneY);

private:

    PcgRandom mPcgRand;
};

#endif