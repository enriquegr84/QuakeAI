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

#include "TreeGenerator.h"
#include "MapGenerator.h"
#include "DungeonGenerator.h"
#include "CaveGenerator.h"
#include "MapGeneratorOre.h"
#include "MapGeneratorDecoration.h"
#include "MapGeneratorV6.h"
#include "MapBlock.h"
#include "MapNode.h"
#include "Map.h"

#include "Emerge.h"
#include "Voxel.h"
#include "VoxelAlgorithms.h"

#include "../../Graphics/Node.h"

#include "../../Utils/Noise.h"

#include "Core/Utility/StringUtil.h"
#include "Application/Settings.h" // For g_settings


FlagDescription FlagdescMapGeneratorV6[] = 
{
	{"jungles",    MGV6_JUNGLES},
	{"biomeblend", MGV6_BIOMEBLEND},
	{"mudflow",    MGV6_MUDFLOW},
	{"snowbiomes", MGV6_SNOWBIOMES},
	{"flat",       MGV6_FLAT},
	{"trees",      MGV6_TREES},
	{NULL,         0}
};


/////////////////////////////////////////////////////////////////////////////


MapGeneratorV6::MapGeneratorV6(MapGeneratorV6Params* params, EmergeParams* emerge)
	: MapGenerator(MAPGEN_V6, params, emerge)
{
	mEmerge = emerge;
	mYstride = mChunkSize[0];

	mHeightmap = new int16_t[mChunkSize[0] * mChunkSize[2]];

	mSPFlags      = params->spFlags;
	mFreqDesert  = params->freqDesert;
	mFreqBeach   = params->freqBeach;
	mDungeonYmin = params->dungeonYmin;
	mDungeonYmax = params->dungeonYmax;

	mNoiseParamsCave = &params->noiseParamsCave;
	mNoiseParamsHumidity = &params->noiseParamsHumidity;
	mNoiseParamsTrees = &params->noiseParamsTrees;
	mNoiseParamsAppleTrees = &params->noiseParamsAppleTrees;

    mNoiseParamsDungeons = NoiseParams(0.9f, 0.5f, Vector3<float>{500.f, 500.f, 500.f}, 0, 2, 0.8f, 2.f);

	//// Create noise objects
	mNoiseTerrainBase = new Noise(&params->noiseParamsTerrainBase, mSeed, mChunkSize[0], mChunkSize[1]);
	mNoiseTerrainHigher = new Noise(&params->noiseParamsTerrainHigher, mSeed, mChunkSize[0], mChunkSize[1]);
	mNoiseSteepness = new Noise(&params->noiseParamsSteepness, mSeed, mChunkSize[0], mChunkSize[1]);
	mNoiseHeightSelect = new Noise(&params->noiseParamsHeightSelect, mSeed, mChunkSize[0], mChunkSize[1]);
	mNoiseMud = new Noise(&params->noiseParamsMud, mSeed, mChunkSize[0], mChunkSize[1]);
	mNoiseBeach = new Noise(&params->noiseParamsBeach, mSeed, mChunkSize[0], mChunkSize[1]);
	mNoiseBiome = new Noise(&params->noiseParamsBiome, mSeed,
			mChunkSize[0] + 2 * MAP_BLOCKSIZE, mChunkSize[1] + 2 * MAP_BLOCKSIZE);
	mNoiseHumidity = new Noise(&params->noiseParamsHumidity, mSeed,
			mChunkSize[0] + 2 * MAP_BLOCKSIZE, mChunkSize[1] + 2 * MAP_BLOCKSIZE);

	//// Resolve nodes to be used
	const NodeManager* nodeMgr = emerge->mNodeMgr;

	mContentStone = nodeMgr->GetId("mapgen_stone");
	mContentDirt = nodeMgr->GetId("mapgen_dirt");
	mContentDirtWithGrass = nodeMgr->GetId("mapgen_dirt_with_grass");
	mContentSand = nodeMgr->GetId("mapgen_sand");
	mContentWaterSource = nodeMgr->GetId("mapgen_water_source");
	mContentLavaSource = nodeMgr->GetId("mapgen_lava_source");
	mContentGravel = nodeMgr->GetId("mapgen_gravel");
	mContentDesertStone = nodeMgr->GetId("mapgen_desert_stone");
	mContentDesertSand = nodeMgr->GetId("mapgen_desert_sand");
	mContentDirtWithSnow = nodeMgr->GetId("mapgen_dirt_with_snow");
	mContentSnow = nodeMgr->GetId("mapgen_snow");
	mContentSnowblock = nodeMgr->GetId("mapgen_snowblock");
	mContentIce = nodeMgr->GetId("mapgen_ice");

	if (mContentGravel == CONTENT_IGNORE)
		mContentGravel = mContentStone;
	if (mContentDesertStone == CONTENT_IGNORE)
		mContentDesertStone = mContentStone;
	if (mContentDesertSand == CONTENT_IGNORE)
		mContentDesertSand = mContentSand;
	if (mContentDirtWithSnow == CONTENT_IGNORE)
		mContentDirtWithSnow = mContentDirtWithGrass;
	if (mContentSnow == CONTENT_IGNORE)
		mContentSnow = CONTENT_AIR;
	if (mContentSnowblock == CONTENT_IGNORE)
		mContentSnowblock = mContentDirtWithGrass;
	if (mContentIce == CONTENT_IGNORE)
		mContentIce = mContentWaterSource;

	mContentCobble = nodeMgr->GetId("mapgen_cobble");
	mContentMossyCobble = nodeMgr->GetId("mapgen_mossycobble");
	mContentStairCobble = nodeMgr->GetId("mapgen_stair_cobble");
	mContentStairDesertStone = nodeMgr->GetId("mapgen_stair_desert_stone");

	if (mContentMossyCobble == CONTENT_IGNORE)
		mContentMossyCobble = mContentCobble;
	if (mContentStairCobble == CONTENT_IGNORE)
		mContentStairCobble = mContentCobble;
	if (mContentStairDesertStone == CONTENT_IGNORE)
		mContentStairDesertStone = mContentDesertStone;

	if (mContentStone == CONTENT_IGNORE)
		LogError("MapGenerator v6: MapGenerator alias 'mapgenodeStone' is invalid!");
	if (mContentDirt == CONTENT_IGNORE)
        LogError("MapGenerator v6: MapGenerator alias 'mapgenodeDirt' is invalid!");
    if (mContentDirtWithGrass == CONTENT_IGNORE)
        LogError("MapGenerator v6: MapGenerator alias 'mapgenodeDirtWithGrass' is invalid!");
	if (mContentSand == CONTENT_IGNORE)
        LogError("MapGenerator v6: MapGenerator alias 'mapgenodeSand' is invalid!");
	if (mContentWaterSource == CONTENT_IGNORE)
        LogError("MapGenerator v6: MapGenerator alias 'mapgen_water_source' is invalid!");
	if (mContentLavaSource == CONTENT_IGNORE)
        LogError("MapGenerator v6: MapGenerator alias 'mapgen_lava_source' is invalid!");
	if (mContentCobble == CONTENT_IGNORE)
        LogError("MapGenerator v6: MapGenerator alias 'mapgen_cobble' is invalid!");
}


MapGeneratorV6::~MapGeneratorV6()
{
	delete mNoiseTerrainBase;
	delete mNoiseTerrainHigher;
	delete mNoiseSteepness;
	delete mNoiseHeightSelect;
	delete mNoiseMud;
	delete mNoiseBeach;
	delete mNoiseBiome;
	delete mNoiseHumidity;

	delete[] mHeightmap;

	delete mEmerge; // our responsibility
}


MapGeneratorV6Params::MapGeneratorV6Params():
    noiseParamsTerrainBase(-4, 20.f, Vector3<float>{250, 250, 250}, 82341, 5, 0.6f, 2.f),
    noiseParamsTerrainHigher(20, 16.f, Vector3<float>{500, 500, 500}, 85039, 5, 0.6f, 2.f),
    noiseParamsSteepness(0.85f, 0.5f, Vector3<float>{125, 125, 125}, -932, 5, 0.7f, 2.f),
    noiseParamsHeightSelect(0, 1.f, Vector3<float>{250, 250, 250}, 4213, 5, 0.69f, 2.f),
    noiseParamsMud(4, 2.f, Vector3<float>{200, 200, 200}, 91013, 3, 0.55f, 2.f),
    noiseParamsBeach(0, 1.f, Vector3<float>{250, 250, 250}, 59420, 3, 0.5f, 2.f),
    noiseParamsBiome(0, 1.f, Vector3<float>{500, 500, 500}, 9130, 3, 0.5f, 2.f),
    noiseParamsCave(6, 6.f, Vector3<float>{250, 250, 250}, 34329, 3, 0.5f, 2.f),
    noiseParamsHumidity(0.5f, 0.5f, Vector3<float>{500, 500, 500.0}, 72384, 3, 0.5f, 2.f),
    noiseParamsTrees(0, 1.f, Vector3<float>{125, 125, 125}, 2, 4, 0.66f, 2.f),
    noiseParamsAppleTrees(0, 1.f, Vector3<float>{100, 100, 100}, 342902, 3, 0.45f, 2.f)
{
}


void MapGeneratorV6Params::ReadParams(const Settings* settings)
{
    try 
    {
        settings->GetFlagString("mgv6_spflags", FlagdescMapGeneratorV6, &spFlags);
        freqDesert = settings->GetFloat("mgv6_freq_desert");
        freqBeach = settings->GetFloat("mgv6_freq_beach");
        dungeonYmin = settings->GetInt16("mgv6_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgv6_dungeon_ymax");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgv6_np_terrain_base", noiseParamsTerrainBase);
    GetNoiseParams(settings, "mgv6_np_terrain_higher", noiseParamsTerrainHigher);
    GetNoiseParams(settings, "mgv6_np_steepness", noiseParamsSteepness);
    GetNoiseParams(settings, "mgv6_np_height_select", noiseParamsHeightSelect);
    GetNoiseParams(settings, "mgv6_np_mud", noiseParamsMud);
    GetNoiseParams(settings, "mgv6_np_beach", noiseParamsBeach);
    GetNoiseParams(settings, "mgv6_np_biome", noiseParamsBiome);
    GetNoiseParams(settings, "mgv6_np_cave", noiseParamsCave);
    GetNoiseParams(settings, "mgv6_np_humidity", noiseParamsHumidity);
    GetNoiseParams(settings, "mgv6_np_trees", noiseParamsTrees);
    GetNoiseParams(settings, "mgv6_np_apple_trees", noiseParamsAppleTrees);
}


void MapGeneratorV6Params::WriteParams(Settings* settings)
{
	settings->SetFlagString("mgv6_spflags", spFlags, FlagdescMapGeneratorV6, spFlags);
	settings->SetFloat("mgv6_freq_desert", freqDesert);
	settings->SetFloat("mgv6_freq_beach", freqBeach);
	settings->SetInt16("mgv6_dungeon_ymin", dungeonYmin);
	settings->SetInt16("mgv6_dungeon_ymax", dungeonYmax);

    SetNoiseParams(settings, "mgv6_np_terrain_base", noiseParamsTerrainBase);
    SetNoiseParams(settings, "mgv6_np_terrain_higher", noiseParamsTerrainHigher);
    SetNoiseParams(settings, "mgv6_np_steepness", noiseParamsSteepness);
    SetNoiseParams(settings, "mgv6_np_height_select", noiseParamsHeightSelect);
    SetNoiseParams(settings, "mgv6_np_mud", noiseParamsMud);
    SetNoiseParams(settings, "mgv6_np_beach", noiseParamsBeach);
    SetNoiseParams(settings, "mgv6_np_biome", noiseParamsBiome);
    SetNoiseParams(settings, "mgv6_np_cave", noiseParamsCave);
    SetNoiseParams(settings, "mgv6_np_humidity", noiseParamsHumidity);
    SetNoiseParams(settings, "mgv6_np_trees", noiseParamsTrees);
    SetNoiseParams(settings, "mgv6_np_apple_trees", noiseParamsAppleTrees);
}


void MapGeneratorV6Params::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgv6_mSPFlags", FlagdescMapGeneratorV6, MGV6_JUNGLES |
		MGV6_SNOWBIOMES | MGV6_TREES | MGV6_BIOMEBLEND | MGV6_MUDFLOW);
}


//////////////////////// Some helper functions for the map generator


// Returns Y one under area minimum if not found
int16_t MapGeneratorV6::FindStoneLevel(Vector2<short> p2d)
{
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	int16_t yNodesMax = mMMVManip->mArea.mMaxEdge[1];
	int16_t yNodesMin = mMMVManip->mArea.mMinEdge[1];
	unsigned int i = mMMVManip->mArea.Index(p2d[0], yNodesMax, p2d[1]);
	int16_t y;

	for (y = yNodesMax; y >= yNodesMin; y--) 
    {
		uint16_t c = mMMVManip->mData[i].GetContent();
		if (c != CONTENT_IGNORE && (c == mContentStone || c == mContentDesertStone))
			break;

		VoxelArea::AddY(em, i, -1);
	}
	return (y >= yNodesMin) ? y : yNodesMin - 1;
}


// Required by mapgen.h
bool MapGeneratorV6::BlockIsUnderground(uint64_t mSeed, Vector3<short> blockpos)
{
	/*int16_t minimumGroundlevel = (int16_t)get_sector_minimumGroundlevel(
			mSeed, Vector2<short>(blockpos[0], blockpos[2]));*/
	// Nah, this is just a heuristic, just return something
	int16_t minimumGroundlevel = mWaterLevel;

	if(blockpos[1] * MAP_BLOCKSIZE + MAP_BLOCKSIZE <= minimumGroundlevel)
		return true;

	return false;
}


//////////////////////// Base terrain height functions

float MapGeneratorV6::BaseTerrainLevel(float terrainBase, float terrainHigher, float steepness, float heightSelect)
{
	float base = 1 + terrainBase;
	float higher = 1 + terrainHigher;

	// Limit higher ground level to at least base
	if(higher < base)
		higher = base;

	// Steepness factor of cliffs
	float b = steepness;
	b = std::clamp(b, 0.f, 1000.f);
	b = 5 * b * b * b * b * b * b * b;
	b = std::clamp(b, 0.5f, 1000.f);

	// Values 1.5...100 give quite horrible looking slopes
	if (b > 1.5f && b < 100.f)
		b = (b < 10.f) ? 1.5f : 100.f;

	float aOff = -0.2F; // Offset to more low
	float a = 0.5F + b * (aOff + heightSelect);
	a = std::clamp(a, 0.f, 1.f); // Limit

	return base * (1.f - a) + higher * a;
}


float MapGeneratorV6::BaseTerrainLevelFromNoise(Vector2<short> pos)
{
	if (mSPFlags & MGV6_FLAT)
		return (float)mWaterLevel;

	float terrainBase = NoisePerlin2DPO(&mNoiseTerrainBase->mNoiseParams, pos[0], 0.5, pos[1], 0.5, mSeed);
	float terrainHigher = NoisePerlin2DPO(&mNoiseTerrainHigher->mNoiseParams, pos[0], 0.5, pos[1], 0.5, mSeed);
	float steepness = NoisePerlin2DPO(&mNoiseSteepness->mNoiseParams, pos[0], 0.5, pos[1], 0.5, mSeed);
	float heightSelect = NoisePerlin2DPO(&mNoiseHeightSelect->mNoiseParams, pos[0], 0.5, pos[1], 0.5, mSeed);

	return BaseTerrainLevel(terrainBase, terrainHigher, steepness, heightSelect);
}


float MapGeneratorV6::BaseTerrainLevelFromMap(Vector2<short> pos)
{
	int index = (pos[1] - mNodeMin[2]) * mYstride + (pos[0] - mNodeMin[0]);
	return BaseTerrainLevelFromMap(index);
}


float MapGeneratorV6::BaseTerrainLevelFromMap(int index)
{
	if (mSPFlags & MGV6_FLAT)
		return (float)mWaterLevel;

	float terrainBase = mNoiseTerrainBase->mResult[index];
	float terrainHigher = mNoiseTerrainHigher->mResult[index];
	float steepness = mNoiseSteepness->mResult[index];
	float heightSelect = mNoiseHeightSelect->mResult[index];

	return BaseTerrainLevel(terrainBase, terrainHigher, steepness, heightSelect);
}


int MapGeneratorV6::GetGroundLevelAtPoint(Vector2<short> pos)
{
	return (int)BaseTerrainLevelFromNoise(pos) + MGV6_AVERAGE_MUD_AMOUNT;
}


int MapGeneratorV6::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	int16_t levelAtPoint = (int)BaseTerrainLevelFromNoise(pos) + MGV6_AVERAGE_MUD_AMOUNT;
	if (levelAtPoint <= mWaterLevel || levelAtPoint > mWaterLevel + 16)
		return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point

	return levelAtPoint;
}


//////////////////////// Noise functions

float MapGeneratorV6::GetMudAmount(Vector2<short> pos)
{
	int index = (pos[1] - mNodeMin[2]) * mYstride + (pos[0] - mNodeMin[0]);
	return GetMudAmount(index);
}


bool MapGeneratorV6::GetHaveBeach(Vector2<short> pos)
{
	int index = (pos[1] - mNodeMin[2]) * mYstride + (pos[0] - mNodeMin[0]);
	return GetHaveBeach(index);
}


BiomeV6Type MapGeneratorV6::GetBiome(Vector2<short> pos)
{
	int index = (pos[1] - mFullNodeMin[2]) * (mYstride + 2 * MAP_BLOCKSIZE) + (pos[0] - mFullNodeMin[0]);
	return GetBiome(index, pos);
}


float MapGeneratorV6::GetHumidity(Vector2<short> pos)
{
	/*double noise = Noise2dPerlin(
		0.5+(float)p[0]/500, 0.5+(float)p[1]/500,
		mSeed+72384, 4, 0.66);
	noise = (noise + 1.0)/2.0;*/

	int index = (pos[1] - mFullNodeMin[2]) * (mYstride + 2 * MAP_BLOCKSIZE) + (pos[0] - mFullNodeMin[0]);
	float noise = mNoiseHumidity->mResult[index];

	if (noise < 0.0)
		noise = 0.0;
	if (noise > 1.0)
		noise = 1.0;
	return noise;
}


float MapGeneratorV6::GetTreeAmount(Vector2<short> pos)
{
	/*double noise = Noise2dPerlin(
			0.5+(float)p[0]/125, 0.5+(float)p[1]/125,
			mSeed+2, 4, 0.66);*/

	float noise = NoisePerlin2D(mNoiseParamsTrees, pos[0], pos[1], mSeed);
	float zeroval = -0.39f;
	if (noise < zeroval)
		return 0;

	return 0.04f * (noise - zeroval) / (1.f - zeroval);
}


bool MapGeneratorV6::GetHaveAppleTree(Vector2<short> pos)
{
	/*isAppleTree = Noise2dPerlin(
		0.5+(float)p[0]/100, 0.5+(float)p[2]/100,
		data->mSeed+342902, 3, 0.45) > 0.2;*/

	float noise = NoisePerlin2D(mNoiseParamsAppleTrees, pos[0], pos[1], mSeed);

	return noise > 0.2;
}


float MapGeneratorV6::GetMudAmount(int index)
{
	if (mSPFlags & MGV6_FLAT)
		return MGV6_AVERAGE_MUD_AMOUNT;

	/*return ((float)AVERAGE_MUD_AMOUNT + 2.0 * Noise2dPerlin(
			0.5+(float)p[0]/200, 0.5+(float)p[1]/200,
			mSeed+91013, 3, 0.55));*/

	return mNoiseMud->mResult[index];
}


bool MapGeneratorV6::GetHaveBeach(int index)
{
	// Determine whether to have sand here
	/*double sandnoise = Noise2dPerlin(
			0.2+(float)p2d[0]/250, 0.7+(float)p2d[1]/250,
			mSeed+59420, 3, 0.50);*/

	float sandnoise = mNoiseBeach->mResult[index];
	return (sandnoise > mFreqBeach);
}


BiomeV6Type MapGeneratorV6::GetBiome(int index, Vector2<short> pos)
{
	// Just do something very simple as for now
	/*double d = Noise2dPerlin(
			0.6+(float)p2d[0]/250, 0.2+(float)p2d[1]/250,
			mSeed+9130, 3, 0.50);*/

	float d = mNoiseBiome->mResult[index];
	float h = mNoiseHumidity->mResult[index];

	if (mSPFlags & MGV6_SNOWBIOMES)
    {
		float blend = (mSPFlags & MGV6_BIOMEBLEND) ? Noise2d(pos[0], pos[1], mSeed) / 40 : 0;

		if (d > MGV6_FREQ_HOT + blend)
        {
			if (h > MGV6_FREQ_JUNGLE + blend)
				return BT_JUNGLE;

			return BT_DESERT;
		}

		if (d < MGV6_FREQ_SNOW + blend) 
        {
			if (h > MGV6_FREQ_TAIGA + blend)
				return BT_TAIGA;

			return BT_TUNDRA;
		}

		return BT_NORMAL;
	}

	if (d > mFreqDesert)
		return BT_DESERT;

	if ((mSPFlags & MGV6_BIOMEBLEND) && (d > mFreqDesert - 0.10) &&
        ((Noise2d(pos[0], pos[1], mSeed) + 1.0) > (mFreqDesert - d) * 20.0))
		return BT_DESERT;

	if ((mSPFlags & MGV6_JUNGLES) && h > 0.75)
		return BT_JUNGLE;

	return BT_NORMAL;

}


unsigned int MapGeneratorV6::GetBlockSeed(uint64_t seed, Vector3<short> pos)
{
	int x = pos[0], y = pos[1], z = pos[2];
	return (unsigned int)(mSeed % 0x100000000ULL) + z * 38134234 + y * 42123 + x * 23;
}


//////////////////////// Map generator

void MapGeneratorV6::MakeChunk(BlockMakeData *data)
{
	// Pre-conditions
	LogAssert(data->vmanip, "invalid vmanip");
	LogAssert(data->nodeMgr, "invalid node manager");

	this->mGenerating = true;
	this->mMMVManip = data->vmanip;
	this->mNodeMgr = data->nodeMgr;

	// Hack: use minimum block coords for old code that assumes a single block
	Vector3<short> blockPosMin = data->blockPosMin;
	Vector3<short> blockPosMax = data->blockPosMax;

	// Area of central chunk
	mNodeMin = blockPosMin * (short)MAP_BLOCKSIZE;
    mNodeMax = (blockPosMax + Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};

	// Full allocated area
    mFullNodeMin = (blockPosMin - Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE;
    mFullNodeMax = (blockPosMax + Vector3<short>{2, 2, 2}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};

    mCentralAreaSize = mNodeMax - mNodeMin + Vector3<short>{1, 1, 1};
	LogAssert(mCentralAreaSize[0] == mCentralAreaSize[2], "invalid area size");

	// Create a block-specific mSeed
	mBlockSeed = GetBlockSeed(data->seed, mFullNodeMin);

	// Make some noise
	CalculateNoise();

	// Maximum height of the stone surface and obstacles.
	// This is used to guide the cave generation
	int16_t stoneSurfaceMaxY;

	// Generate general ground level to full area
	stoneSurfaceMaxY = GenerateGround();

	// Create initial heightmap to limit caves
	UpdateHeightmap(mNodeMin, mNodeMax);

	const int16_t maxSpreadAmount = MAP_BLOCKSIZE;
	// Limit dirt flow area by 1 because mud is flowed into neighbors
	int16_t mudFlowMinPos = -maxSpreadAmount + 1;
	int16_t mudFlowMaxPos = mCentralAreaSize[0] + maxSpreadAmount - 2;

	// Loop this part, it will make stuff look older and newer nicely
	const unsigned int ageLoops = 2;
	for (unsigned int iAge = 0; iAge < ageLoops; iAge++) 
    { // Aging loop
		// Make caves (this code is relatively horrible)
		if (mFlags & MG_CAVES)
			GenerateCaves(stoneSurfaceMaxY);

		// Add mud to the central chunk
		AddMud();

		// Flow mud away from steep edges
		if (mSPFlags & MGV6_MUDFLOW)
			FlowMud(mudFlowMinPos, mudFlowMaxPos);
	}

	// Update heightmap after mudflow
	UpdateHeightmap(mNodeMin, mNodeMax);

	// Add dungeons
	if ((mFlags & MG_DUNGEONS) && stoneSurfaceMaxY >= mNodeMin[1] &&
        mFullNodeMin[1] >= mDungeonYmin && mFullNodeMax[1] <= mDungeonYmax) 
    {
		uint16_t numDungeons = (uint16_t)std::fmax(std::floor(
            NoisePerlin3D(&mNoiseParamsDungeons, mNodeMin[0], mNodeMin[1], mNodeMin[2], mSeed)), 0.0f);

		if (numDungeons >= 1) 
        {
			PseudoRandom ps(mBlockSeed + 4713);

			DungeonParams dp;
			dp.seed = mSeed;
			dp.numDungeons = numDungeons;
			dp.onlyInGround = true;
			dp.corridorLengthMin = 1;
			dp.corridorLengthMax = 13;
			dp.numRooms = ps.Range(2, 16);
			dp.largeRoomChance = (ps.Range(1, 4) == 1) ? 1 : 0;

            dp.npAltWall = NoiseParams(-0.4f, 1.f, 
                Vector3<float>{40.f, 40.f, 40.f}, 32474, 6, 1.1f, 2.f);

            if (GetBiome(0, Vector2<short>{mNodeMin[0], mNodeMin[2]}) == BT_DESERT) 
            {
				dp.contentWall = mContentDesertStone;
				dp.contentAltWall = CONTENT_IGNORE;
				dp.contentStair = mContentStairDesertStone;

				dp.diagonalDirections = true;
                dp.holeSize = Vector3<short>{ 2, 3, 2 };
                dp.roomSizeMin = Vector3<short>{ 6, 9, 6 };
                dp.roomSizeMax = Vector3<short>{ 10, 11, 10 };
                dp.roomSizeLargeMin = Vector3<short>{ 10, 13, 10 };
                dp.roomSizeLargeMax = Vector3<short>{ 18, 21, 18 };
				dp.notifyType = GENNOTIFY_TEMPLE;
			} 
            else 
            {
				dp.contentWall = mContentCobble;
				dp.contentAltWall = mContentMossyCobble;
				dp.contentStair = mContentStairCobble;

				dp.diagonalDirections = false;
                dp.holeSize = Vector3<short>{ 1, 2, 1 };
                dp.roomSizeMin = Vector3<short>{ 4, 4, 4 };
                dp.roomSizeMax = Vector3<short>{ 8, 6, 8 };
                dp.roomSizeLargeMin = Vector3<short>{ 8, 8, 8 };
                dp.roomSizeLargeMax = Vector3<short>{ 16, 16, 16 };
				dp.notifyType = GENNOTIFY_DUNGEON;
			}

			DungeonGen dgen(mNodeMgr, &mGenNotify, &dp);
			dgen.Generate(mMMVManip, mBlockSeed, mFullNodeMin, mFullNodeMax);
		}
	}

	// Add top and bottom side of water to transforming_liquid queue
	UpdateLiquid(&data->transformingLiquid, mFullNodeMin, mFullNodeMax);

	// Add surface nodes
	GrowGrass();

	// Generate some trees, and add grass, if a jungle
	if (mSPFlags & MGV6_TREES)
		PlaceTreesAndJungleGrass();

	// Generate the registered decorations
	if (mFlags & MG_DECORATIONS)
		mEmerge->mDecoMgr->PlaceAllDecos(this, mBlockSeed, mNodeMin, mNodeMax);

	// Generate the registered ores
	if (mFlags & MG_ORES)
		mEmerge->mOreMgr->PlaceAllOres(this, mBlockSeed, mNodeMin, mNodeMax);

	// Calculate lighting
	if (mFlags & MG_LIGHT)
        CalculateLighting(mNodeMin - Vector3<short>{1, 1, 1} * (short)MAP_BLOCKSIZE,
            mNodeMax + Vector3<short>{1, 0, 1} * (short)MAP_BLOCKSIZE, mFullNodeMin, mFullNodeMax);

	this->mGenerating = false;
}


void MapGeneratorV6::CalculateNoise()
{
	float x = mNodeMin[0];
    float z = mNodeMin[2];
    float fx = mFullNodeMin[0];
    float fz = mFullNodeMin[2];

	if (!(mSPFlags & MGV6_FLAT)) 
    {
		mNoiseTerrainBase->PerlinMap2DPO(x, 0.5f, z, 0.5f);
		mNoiseTerrainHigher->PerlinMap2DPO(x, 0.5f, z, 0.5f);
		mNoiseSteepness->PerlinMap2DPO(x, 0.5f, z, 0.5f);
		mNoiseHeightSelect->PerlinMap2DPO(x, 0.5f, z, 0.5f);
		mNoiseMud->PerlinMap2DPO(x, 0.5f, z, 0.5f);
	}

	mNoiseBeach->PerlinMap2DPO(x, 0.2f, z, 0.7f);

	mNoiseBiome->PerlinMap2DPO(fx, 0.6f, fz, 0.2f);
	mNoiseHumidity->PerlinMap2DPO(fx, 0.f, fz, 0.f);
	// Humidity map does not need range limiting 0 to 1,
	// only humidity at point does
}


int MapGeneratorV6::GenerateGround()
{
	//TimeTaker timer1("Generating ground level");
	MapNode nodeAir(CONTENT_AIR), nodeWaterSource(mContentWaterSource);
	MapNode nodeStone(mContentStone), nodeDesertStone(mContentDesertStone);
	MapNode nodeIce(mContentIce);
	int stoneSurfaceMaxY = -MAX_MAP_GENERATION_LIMIT;

	unsigned int index = 0;
    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, index++)
        {
            // Surface height
            int16_t surfaceY = (int16_t)BaseTerrainLevelFromMap(index);

            // Log it
            if (surfaceY > stoneSurfaceMaxY)
                stoneSurfaceMaxY = surfaceY;

            BiomeV6Type bt = GetBiome(Vector2<short>{x, z});

            // Fill ground with stone
            const Vector3<short>& em = mMMVManip->mArea.GetExtent();
            unsigned int i = mMMVManip->mArea.Index(x, mNodeMin[1], z);
            for (int16_t y = mNodeMin[1]; y <= mNodeMax[1]; y++) 
            {
                if (mMMVManip->mData[i].GetContent() == CONTENT_IGNORE) 
                {
                    if (y <= surfaceY) 
                    {
                        mMMVManip->mData[i] = (y >= MGV6_DESERT_STONE_BASE && bt == BT_DESERT) ?
                            nodeDesertStone : nodeStone;
                    }
                    else if (y <= mWaterLevel) 
                    {
                        mMMVManip->mData[i] = (y >= MGV6_ICE_BASE && bt == BT_TUNDRA) ?
                            nodeIce : nodeWaterSource;
                    }
                    else 
                    {
                        mMMVManip->mData[i] = nodeAir;
                    }
                }
                VoxelArea::AddY(em, i, 1);
            }
        }
    }

	return stoneSurfaceMaxY;
}


void MapGeneratorV6::AddMud()
{
	// 15ms @cs=8
	//TimeTaker timer1("add mud");
	MapNode nodeDirt(mContentDirt), nodeGravel(mContentGravel);
	MapNode nodeSand(mContentSand), nodeDesertSand(mContentDesertSand);
	MapNode addnode;

	unsigned int index = 0;
    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, index++)
        {
            // Randomize mud amount
            int16_t mudAddAmount = (int16_t)(GetMudAmount(index) / 2.0 + 0.5);

            // Find ground level
            int16_t surfaceY = FindStoneLevel(Vector2<short>{x, z}); /////////////////optimize this!

            // Handle area not found
            if (surfaceY == mMMVManip->mArea.mMinEdge[1] - 1)
                continue;

            BiomeV6Type bt = GetBiome(Vector2<short>{x, z});
            addnode = (bt == BT_DESERT) ? nodeDesertSand : nodeDirt;

            if (bt == BT_DESERT && surfaceY + mudAddAmount <= mWaterLevel + 1)
            {
                addnode = nodeSand;
            }
            else if (mudAddAmount <= 0)
            {
                mudAddAmount = 1 - mudAddAmount;
                addnode = nodeGravel;
            }
            else if (bt != BT_DESERT && GetHaveBeach(index) &&
                surfaceY + mudAddAmount <= mWaterLevel + 2)
            {
                addnode = nodeSand;
            }

            if ((bt == BT_DESERT || bt == BT_TUNDRA) && surfaceY > 20)
                mudAddAmount = std::max(0, mudAddAmount - (surfaceY - 20) / 5);

            /* If topmost node is grass, change it to mud.  It might be if it was
            // flown to there from a neighboring chunk and then converted.
            unsigned int i = vm->mArea.index(x, surfaceY, z);
            if (vm->mData[i].GetContent() == mContentDirtWithGrass)
                vm->mData[i] = nodeDirt;*/

                // Add mud on ground
            int16_t mudcount = 0;
            const Vector3<short> &em = mMMVManip->mArea.GetExtent();
            int16_t yStart = surfaceY + 1;
            unsigned int i = mMMVManip->mArea.Index(x, yStart, z);
            for (int16_t y = yStart; y <= mNodeMax[1]; y++)
            {
                if (mudcount >= mudAddAmount)
                    break;

                mMMVManip->mData[i] = addnode;
                mudcount++;

                VoxelArea::AddY(em, i, 1);
            }
        }
    }
}


void MapGeneratorV6::FlowMud(int16_t& mudFlowMinPos, int16_t& mudFlowMaxPos)
{
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	static const Vector3<short> dirs4[4] = {
        Vector3<short>{0, 0, 1}, // Back
        Vector3<short>{1, 0, 0}, // Right
        Vector3<short>{0, 0, -1}, // Front
        Vector3<short>{-1, 0, 0}, // Left
	};

	// Iterate twice
	for (int16_t k = 0; k < 2; k++) 
    {
        for (int16_t z = mudFlowMinPos; z <= mudFlowMaxPos; z++)
        {
            for (int16_t x = mudFlowMinPos; x <= mudFlowMaxPos; x++) 
            {
                // Node column position
                Vector2<short> p2d;
                // Invert coordinates on second iteration to process columns in
                // opposite order, to avoid a directional bias.
                if (k == 1)
                    p2d = Vector2<short>{ mNodeMax[0], mNodeMax[2] } - Vector2<short>{x, z};
                else
                    p2d = Vector2<short>{ mNodeMin[0], mNodeMin[2] } + Vector2<short>{x, z};

                int16_t y = mNodeMax[1];

                while (y >= mNodeMin[1]) 
                {
                    for (;; y--) 
                    {
                        unsigned int i = mMMVManip->mArea.Index(p2d[0], y, p2d[1]);
                        MapNode* node = nullptr;

                        // Find next mud node in mapchunk column
                        for (; y >= mNodeMin[1]; y--) 
                        {
                            node = &mMMVManip->mData[i];
                            if (node->GetContent() == mContentDirt ||
                                node->GetContent() == mContentDirtWithGrass ||
                                node->GetContent() == mContentGravel)
                                break;

                            VoxelArea::AddY(em, i, -1);
                        }
                        if (y < mNodeMin[1])
                            // No mud found in mapchunk column, process the next column
                            break;

                        if (node->GetContent() == mContentDirt || 
                            node->GetContent() == mContentDirtWithGrass) 
                        {
                            // Convert dirt_with_grass to dirt
                            node->SetContent(mContentDirt);
                            // Don't flow mud if the stuff under it is not mud,
                            // to leave at least 1 node of mud.
                            unsigned int i2 = i;
                            VoxelArea::AddY(em, i2, -1);
                            MapNode* node2 = &mMMVManip->mData[i2];
                            if (node2->GetContent() != mContentDirt &&
                                node2->GetContent() != mContentDirtWithGrass)
                                // Find next mud node in column
                                continue;
                        }

                        // Check if node above is walkable. If so, cancel
                        // flowing as if node above keeps it in place.
                        unsigned int i3 = i;
                        VoxelArea::AddY(em, i3, 1);
                        MapNode* node3 = &mMMVManip->mData[i3];
                        if (mNodeMgr->Get(*node3).walkable)
                        {
                            // Find next mud node in column
                            continue;
                        }

                        // Drop mud on one side
                        for (const Vector3<short>& dirp : dirs4) 
                        {
                            unsigned int i2 = i;
                            // Move to side
                            VoxelArea::AddP(em, i2, dirp);
                            // Check that side is air
                            MapNode* node2 = &mMMVManip->mData[i2];
                            if (mNodeMgr->Get(*node2).walkable)
                                continue;

                            // Check that under side is air
                            VoxelArea::AddY(em, i2, -1);
                            node2 = &mMMVManip->mData[i2];
                            if (mNodeMgr->Get(*node2).walkable)
                                continue;

                            // Loop further down until not air
                            int16_t y2 = y - 1; // y of i2
                            bool droppedToUnknown = false;
                            do 
                            {
                                y2--;
                                VoxelArea::AddY(em, i2, -1);
                                node2 = &mMMVManip->mData[i2];
                                // If out of area or in ungenerated world
                                if (y2 < mFullNodeMin[1] || node2->GetContent() == CONTENT_IGNORE) 
                                {
                                    droppedToUnknown = true;
                                    break;
                                }
                            } while (!mNodeMgr->Get(*node2).walkable);

                            if (!droppedToUnknown) 
                            {
                                // Move up one so that we're in air
                                VoxelArea::AddY(em, i2, 1);
                                // Move mud to new place, and if outside mapchunk remove
                                // any decorations above removed or placed mud.
                                MoveMud(i, i2, i3, p2d, em);
                            }
                            // Done, find next mud node in column
                            break;
                        }
                    }
                }
            }
        }
	}
}


void MapGeneratorV6::MoveMud(unsigned int removeIndex, unsigned int placeIndex,
	unsigned int aboveRemoveIndex, Vector2<short> pos, Vector3<short> em)
{
	MapNode nodeAir(CONTENT_AIR);
	// Copy mud from old place to new place
	mMMVManip->mData[placeIndex] = mMMVManip->mData[removeIndex];
	// Set old place to be air
    mMMVManip->mData[removeIndex] = nodeAir;
	// Outside the mapchunk decorations may need to be removed if above removed
	// mud or if half-buried in placed mud. Placed mud is to the side of pos so
	// use 'pos[0] >= mNodeMax[0]' etc.
	if (pos[0] >= mNodeMax[0] || pos[0] <= mNodeMin[0] ||
        pos[1] >= mNodeMax[2] || pos[1] <= mNodeMin[2]) 
    {
		// 'above remove' node is above removed mud. If it is not air, water or
		// 'ignore' it is a decoration that needs removing. Also search upwards
		// to remove a possible stacked decoration.
		// Check for 'ignore' because stacked decorations can penetrate into
		// 'ignore' nodes above the mapchunk.
		while (mMMVManip->mArea.Contains(aboveRemoveIndex) &&
            mMMVManip->mData[aboveRemoveIndex].GetContent() != CONTENT_AIR &&
            mMMVManip->mData[aboveRemoveIndex].GetContent() != mContentWaterSource &&
            mMMVManip->mData[aboveRemoveIndex].GetContent() != CONTENT_IGNORE) {
            mMMVManip->mData[aboveRemoveIndex] = nodeAir;
			VoxelArea::AddY(em, aboveRemoveIndex, 1);
		}
		// Mud placed may have partially-buried a stacked decoration, search
		// above and remove.
		VoxelArea::AddY(em, placeIndex, 1);
		while (mMMVManip->mArea.Contains(placeIndex) &&
            mMMVManip->mData[placeIndex].GetContent() != CONTENT_AIR &&
            mMMVManip->mData[placeIndex].GetContent() != mContentWaterSource &&
            mMMVManip->mData[placeIndex].GetContent() != CONTENT_IGNORE) {
            mMMVManip->mData[placeIndex] = nodeAir;
			VoxelArea::AddY(em, placeIndex, 1);
		}
	}
}


void MapGeneratorV6::PlaceTreesAndJungleGrass()
{
	//TimeTaker t("placeTrees");
	if (mNodeMax[1] < mWaterLevel)
		return;

	PseudoRandom grassrandom(mBlockSeed + 53);
	uint16_t contentJunglegrass = mNodeMgr->GetId("mapgen_junglegrass");
	// if we don't have junglegrass, don't place cignore... that's bad
	if (contentJunglegrass == CONTENT_IGNORE)
		contentJunglegrass = CONTENT_AIR;
	MapNode nodeJunglegrass(contentJunglegrass);
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();

	// Divide area into parts
	int16_t div = 8;
	int16_t mSideLen = mCentralAreaSize[0] / div;
	double area = mSideLen * mSideLen;

	// N.B.  We must add jungle grass first, since tree leaves will
	// obstruct the ground, giving us a false ground level
    for (int16_t z0 = 0; z0 < div; z0++)
    {
        for (int16_t x0 = 0; x0 < div; x0++)
        {
            // Center position of part of division
            Vector2<short> p2dCenter{
                (short)(mNodeMin[0] + mSideLen / 2 + mSideLen * x0),
                (short)(mNodeMin[2] + mSideLen / 2 + mSideLen * z0)};
                // Minimum edge of part of division
            Vector2<short> p2dMin{
                (short)(mNodeMin[0] + mSideLen * x0),
                (short)(mNodeMin[2] + mSideLen * z0)};
                // Maximum edge of part of division
            Vector2<short> p2dMax{
                (short)(mNodeMin[0] + mSideLen + mSideLen * x0 - 1),
                (short)(mNodeMin[2] + mSideLen + mSideLen * z0 - 1)};

            // Get biome at center position of part of division
            BiomeV6Type bt = GetBiome(p2dCenter);

            // Amount of trees
            unsigned int treeCount;
            if (bt == BT_JUNGLE || bt == BT_TAIGA || bt == BT_NORMAL) 
            {
                treeCount = (unsigned int)(area * GetTreeAmount(p2dCenter));
                if (bt == BT_JUNGLE)
                    treeCount *= 4;
            }
            else 
            {
                treeCount = 0;
            }

            // Add jungle grass
            if (bt == BT_JUNGLE) 
            {
                float humidity = GetHumidity(p2dCenter);
                unsigned int grassCount = (unsigned int)(5 * humidity * treeCount);
                for (unsigned int i = 0; i < grassCount; i++) 
                {
                    int16_t x = grassrandom.Range(p2dMin[0], p2dMax[0]);
                    int16_t z = grassrandom.Range(p2dMin[1], p2dMax[1]);
                    int mapindex = mCentralAreaSize[0] * (z - mNodeMin[2])
                        + (x - mNodeMin[0]);
                    int16_t y = mHeightmap[mapindex];
                    if (y < mWaterLevel)
                        continue;

                    unsigned int vi = mMMVManip->mArea.Index(x, y, z);
                    // place on dirt_with_grass, since we know it is exposed to sunlight
                    if (mMMVManip->mData[vi].GetContent() == mContentDirtWithGrass)
                    {
                        VoxelArea::AddY(em, vi, 1);
                        mMMVManip->mData[vi] = nodeJunglegrass;
                    }
                }
            }

            // Put trees in random places on part of division
            for (unsigned int i = 0; i < treeCount; i++) 
            {
                int16_t x = mPcgRand.Range(p2dMin[0], p2dMax[0]);
                int16_t z = mPcgRand.Range(p2dMin[1], p2dMax[1]);
                int mapindex = mCentralAreaSize[0] * (z - mNodeMin[2]) + (x - mNodeMin[0]);
                int16_t y = mHeightmap[mapindex];
                // Don't make a tree under water level
                // Don't make a tree so high that it doesn't fit
                if (y < mWaterLevel || y > mNodeMax[1] - 6)
                    continue;

                Vector3<short> p{ x, y, z };
                // Trees grow only on mud and grass
                {
                    unsigned int i = mMMVManip->mArea.Index(p);
                    uint16_t c = mMMVManip->mData[i].GetContent();
                    if (c != mContentDirt &&
                        c != mContentDirtWithGrass &&
                        c != mContentDirtWithSnow)
                        continue;
                }
                p[1]++;

                // Make a tree
                if (bt == BT_JUNGLE) 
                {
                    MakeJungleTree(*mMMVManip, p, mNodeMgr, mPcgRand.Next());
                }
                else if (bt == BT_TAIGA) 
                {
                    MakePineTree(*mMMVManip, p - Vector3<short>{0, 1, 0}, mNodeMgr, mPcgRand.Next());
                }
                else if (bt == BT_NORMAL) 
                {
                    bool isAppleTree = (mPcgRand.Range(0, 3) == 0) && GetHaveAppleTree(Vector2<short>{x, z});
                    MakeTree(*mMMVManip, p, isAppleTree, mNodeMgr, mPcgRand.Next());
                }
            }
        }
    }
	//printf("PlaceTreesAndJungleGrass: %dms\n", t.stop());
}


void MapGeneratorV6::GrowGrass() // Add surface nodes
{
	MapNode nodeDirtWithGrass(mContentDirtWithGrass);
	MapNode nodeDirtWithSnow(mContentDirtWithSnow);
	MapNode n_snowblock(mContentSnowblock);
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();

	unsigned int index = 0;
    for (int16_t z = mFullNodeMin[2]; z <= mFullNodeMax[2]; z++)
    {
        for (int16_t x = mFullNodeMin[0]; x <= mFullNodeMax[0]; x++, index++) 
        {
            // Find the lowest surface to which enough light ends up to make
            // grass grow.  Basically just wait until not air and not leaves.
            int16_t surfaceY = 0;
            {
                unsigned int i = mMMVManip->mArea.Index(x, mNodeMax[1], z);
                int16_t y;
                // Go to ground level
                for (y = mNodeMax[1]; y >= mFullNodeMin[1]; y--) 
                {
                    MapNode& n = mMMVManip->mData[i];
                    if (mNodeMgr->Get(n).paramType != CPT_LIGHT ||
                        mNodeMgr->Get(n).liquidType != LIQUID_NONE ||
                        n.GetContent() == mContentIce)
                        break;
                    VoxelArea::AddY(em, i, -1);
                }
                surfaceY = (y >= mFullNodeMin[1]) ? y : mFullNodeMin[1];
            }

            BiomeV6Type bt = GetBiome(index, Vector2<short>{x, z});
            unsigned int i = mMMVManip->mArea.Index(x, surfaceY, z);
            uint16_t c = mMMVManip->mData[i].GetContent();
            if (surfaceY >= mWaterLevel - 20) 
            {
                if (bt == BT_TAIGA && c == mContentDirt) 
                {
                    mMMVManip->mData[i] = nodeDirtWithSnow;
                }
                else if (bt == BT_TUNDRA) 
                {
                    if (c == mContentDirt) 
                    {
                        mMMVManip->mData[i] = n_snowblock;
                        VoxelArea::AddY(em, i, -1);
                        mMMVManip->mData[i] = nodeDirtWithSnow;
                    }
                    else if (c == mContentStone && surfaceY < mNodeMax[1]) 
                    {
                        VoxelArea::AddY(em, i, 1);
                        mMMVManip->mData[i] = n_snowblock;
                    }
                }
                else if (c == mContentDirt) 
                {
                    mMMVManip->mData[i] = nodeDirtWithGrass;
                }
            }
        }
    }
}


void MapGeneratorV6::GenerateCaves(int maxStoneY)
{
	float caveAmount = NoisePerlin2D(mNoiseParamsCave, mNodeMin[0], mNodeMin[1], mSeed);
	int volumeNodes = (mNodeMax[0] - mNodeMin[0] + 1) *
        (mNodeMax[1] - mNodeMin[1] + 1) * MAP_BLOCKSIZE;
	caveAmount = std::max(0.f, caveAmount);
	unsigned int cavesCount = (unsigned int)(caveAmount * volumeNodes / 50000);
	unsigned int bruisesCount = 1;
	PseudoRandom ps(mBlockSeed + 21343);
	PseudoRandom ps2(mBlockSeed + 1032);

	if (ps.Range(1, 6) == 1)
		bruisesCount = ps.Range(0, ps.Range(0, 2));

    if (GetBiome(Vector2<short>{mNodeMin[0], mNodeMin[2]}) == BT_DESERT) 
    {
		cavesCount   /= 3;
		bruisesCount /= 3;
	}

	for (unsigned int i = 0; i < cavesCount + bruisesCount; i++) 
    {
		CavesV6 cave(mNodeMgr, &mGenNotify, mWaterLevel, mContentWaterSource, mContentLavaSource);

		bool mLargeCave = (i >= cavesCount);
		cave.MakeCave(mMMVManip, mNodeMin, mNodeMax, &ps, &ps2, mLargeCave, maxStoneY, mHeightmap);
	}
}
