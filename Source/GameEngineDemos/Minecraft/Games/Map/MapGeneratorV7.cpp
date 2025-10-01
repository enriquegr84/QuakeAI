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

#include "MapGenerator.h"
#include "DungeonGenerator.h"
#include "CaveGenerator.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorOre.h"
#include "MapGeneratorDecoration.h"
#include "MapGeneratorV7.h"
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


FlagDescription FlagdescMapGeneratorV7[] = 
{
	{"mountains",   MGV7_MOUNTAINS},
	{"ridges",      MGV7_RIDGES},
	{"floatlands",  MGV7_FLOATLANDS},
	{"caverns",     MGV7_CAVERNS},
	{NULL,          0}
};


////////////////////////////////////////////////////////////////////////////////


MapGeneratorV7::MapGeneratorV7(MapGeneratorV7Params* params, EmergeParams* emerge)
	: MapGeneratorBasic(MAPGEN_V7, params, emerge)
{
	mSPFlags = params->spFlags;
	mMountZeroLevel = params->mountZeroLevel;
	mFloatLandYmin = params->floatLandYmin;
	mFloatLandYmax = params->floatLandYmax;
	mFloatLandTaper = params->floatLandTaper;
	mFloatTaperExp = params->floatTaperExp;
	mFloatLandDensity = params->floatLandDensity;
	mFloatLandYwater = params->floatLandYwater;

	mCaveWidth = params->caveWidth;
	mLargeCaveDepth = params->largeCaveDepth;
	mSmallCaveNumMin = params->smallCaveNumMin;
	mSmallCaveNumMax = params->smallCaveNumMax;
	mLargeCaveNumMin = params->largeCaveNumMin;
	mLargeCaveNumMax = params->largeCaveNumMax;
	mLargeCaveFlooded = params->largeCaveFlooded;
	mCavernLimit = params->cavernLimit;
	mCavernTaper = params->cavernTaper;
	mCavernThreshold = params->cavernThreshold;
	mDungeonYmin = params->dungeonYmin;
	mDungeonYmax = params->dungeonYmax;

	// Allocate floatland noise offset cache
	this->mFloatOffsetCache = new float[mChunkSize[1] + 2];

	// 2D noise
	mNoiseTerrainBase = new Noise(&params->noiseParamsTerrainBase, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseTerrainAlt = new Noise(&params->noiseParamsTerrainAlt, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseTerrainPersist = new Noise(&params->noiseParamsTerrainPersist, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeightSelect = new Noise(&params->noiseParamsHeightSelect, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseFillerDepth = new Noise(&params->noiseParamsFillerDepth, mSeed, mChunkSize[0], mChunkSize[2]);

	if (mSPFlags & MGV7_MOUNTAINS) 
    {
		// 2D noise
		mNoiseMountHeight = new Noise(&params->noiseParamsMountHeight, mSeed, mChunkSize[0], mChunkSize[2]);
		// 3D noise, 1 up, 1 down overgeneration
		mNoiseMountain = new Noise(&params->noiseParamsMountain, mSeed, mChunkSize[0], mChunkSize[1] + 2, mChunkSize[2]);
	}

	if (mSPFlags & MGV7_RIDGES) 
    {
		// 2D noise
		mNoiseRidgeUWater = new Noise(&params->noiseParamsRidgeUWater, mSeed, mChunkSize[0], mChunkSize[2]);
		// 3D noise, 1 up, 1 down overgeneration
		mNoiseRidge = new Noise(&params->noiseParamsRidge, mSeed, mChunkSize[0], mChunkSize[1] + 2, mChunkSize[2]);
	}

	if (mSPFlags & MGV7_FLOATLANDS) 
    {
		// 3D noise, 1 up, 1 down overgeneration
		mNoiseFloatLand = new Noise(&params->noiseParamsFloatLand, mSeed, mChunkSize[0], mChunkSize[1] + 2, mChunkSize[2]);
	}

	// 3D noise, 1 down overgeneration
	MapGeneratorBasic::mNoiseParamsCave1 = params->noiseParamsCave1;
	MapGeneratorBasic::mNoiseParamsCave2 = params->noiseParamsCave2;
	MapGeneratorBasic::mNoiseParamsCavern = params->noiseParamsCavern;
	// 3D noise
	MapGeneratorBasic::mNoiseParamsDungeons = params->noiseParamsDungeons;
}


MapGeneratorV7::~MapGeneratorV7()
{
	delete mNoiseTerrainBase;
	delete mNoiseTerrainAlt;
	delete mNoiseTerrainPersist;
	delete mNoiseHeightSelect;
	delete mNoiseFillerDepth;

	if (mSPFlags & MGV7_MOUNTAINS) 
    {
		delete mNoiseMountHeight;
		delete mNoiseMountain;
	}

	if (mSPFlags & MGV7_RIDGES) 
    {
		delete mNoiseRidgeUWater;
		delete mNoiseRidge;
	}

	if (mSPFlags & MGV7_FLOATLANDS) 
    {
		delete mNoiseFloatLand;
	}

	delete []mFloatOffsetCache;
}


MapGeneratorV7Params::MapGeneratorV7Params():
    noiseParamsTerrainBase(4.f, 70.f, Vector3<float>{600, 600, 600}, 82341, 5, 0.6f, 2.f),
    noiseParamsTerrainAlt(4.f, 25.f, Vector3<float>{600, 600, 600}, 5934, 5, 0.6f, 2.f),
    noiseParamsTerrainPersist(0.6f, 0.1f, Vector3<float>{2000, 2000, 2000}, 539, 3, 0.6f, 2.f),
    noiseParamsHeightSelect(-8.f, 16.f, Vector3<float>{500, 500, 500}, 4213, 6, 0.7f, 2.f),
    noiseParamsFillerDepth(0.f, 1.2f, Vector3<float>{150, 150, 150}, 261, 3, 0.7f, 2.f),
    noiseParamsMountHeight(256.f, 112.f, Vector3<float>{1000, 1000, 1000}, 72449, 3, 0.6f, 2.f),
    noiseParamsRidgeUWater(0.f, 1.f, Vector3<float>{1000, 1000, 1000}, 85039, 5, 0.6f, 2.f),
    noiseParamsMountain(-0.6f, 1.f, Vector3<float>{250, 350, 250}, 5333, 5, 0.63f, 2.f),
    noiseParamsRidge(0.f, 1.f, Vector3<float>{100, 100, 100}, 6467, 4, 0.75f, 2.f),
    noiseParamsFloatLand(0.f, 0.7f, Vector3<float>{384, 96, 384}, 1009, 4, 0.75f, 1.618f),
    noiseParamsCavern(0.f, 1.f, Vector3<float>{384, 128, 384}, 723, 5, 0.63f, 2.f),
    noiseParamsCave1(0.f, 12.f, Vector3<float>{61, 61, 61}, 52534, 3, 0.5f, 2.f),
    noiseParamsCave2(0.f, 12.f, Vector3<float>{67, 67, 67}, 10325, 3, 0.5f, 2.f),
    noiseParamsDungeons(0.9f, 0.5f, Vector3<float>{500, 500, 500}, 0, 2, 0.8f, 2.f)
{
}


void MapGeneratorV7Params::ReadParams(const Settings* settings)
{
    try {
        settings->GetFlagString("mgv7_spflags", FlagdescMapGeneratorV7, &spFlags);
        mountZeroLevel = settings->GetInt16("mgv7_mount_zero_level");
        floatLandYmin = settings->GetInt16("mgv7_floatland_ymin");
        floatLandYmax = settings->GetInt16("mgv7_floatland_ymax");
        floatLandTaper = settings->GetInt16("mgv7_floatland_taper");
        floatTaperExp = settings->GetFloat("mgv7_float_taper_exp");
        floatLandDensity = settings->GetFloat("mgv7_floatland_density");
        floatLandYwater = settings->GetInt16("mgv7_floatland_ywater");

        caveWidth = settings->GetFloat("mgv7_cave_width");
        largeCaveDepth = settings->GetInt16("mgv7_large_cave_depth");
        smallCaveNumMin = settings->GetUInt16("mgv7_small_cave_num_min");
        smallCaveNumMax = settings->GetUInt16("mgv7_small_cave_num_max");
        largeCaveNumMin = settings->GetUInt16("mgv7_large_cave_num_min");
        largeCaveNumMax = settings->GetUInt16("mgv7_large_cave_num_max");
        largeCaveFlooded = settings->GetFloat("mgv7_large_cave_flooded");
        cavernLimit = settings->GetInt16("mgv7_cavern_limit");
        cavernTaper = settings->GetInt16("mgv7_cavern_taper");
        cavernThreshold = settings->GetFloat("mgv7_cavern_threshold");
        dungeonYmin = settings->GetInt16("mgv7_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgv7_dungeon_ymax");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgv7_np_terrain_base", noiseParamsTerrainBase);
    GetNoiseParams(settings, "mgv7_np_terrain_alt", noiseParamsTerrainAlt);
    GetNoiseParams(settings, "mgv7_np_terrain_persist", noiseParamsTerrainPersist);
    GetNoiseParams(settings, "mgv7_np_height_select", noiseParamsHeightSelect);
    GetNoiseParams(settings, "mgv7_np_filler_depth", noiseParamsFillerDepth);
    GetNoiseParams(settings, "mgv7_np_mount_height", noiseParamsMountHeight);
    GetNoiseParams(settings, "mgv7_np_ridge_uwater", noiseParamsRidgeUWater);
    GetNoiseParams(settings, "mgv7_np_mountain", noiseParamsMountain);
    GetNoiseParams(settings, "mgv7_np_ridge", noiseParamsRidge);
    GetNoiseParams(settings, "mgv7_np_floatland", noiseParamsFloatLand);
    GetNoiseParams(settings, "mgv7_np_cavern", noiseParamsCavern);
    GetNoiseParams(settings, "mgv7_np_cave1", noiseParamsCave1);
    GetNoiseParams(settings, "mgv7_np_cave2", noiseParamsCave2);
    GetNoiseParams(settings, "mgv7_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorV7Params::WriteParams(Settings* settings)
{
	settings->SetFlagString("mgv7_spflags", spFlags, FlagdescMapGeneratorV7, spFlags);
	settings->SetInt16("mgv7_mount_zero_level", mountZeroLevel);
	settings->SetInt16("mgv7_floatland_ymin", floatLandYmin);
	settings->SetInt16("mgv7_floatland_ymax", floatLandYmax);
	settings->SetInt16("mgv7_floatland_taper", floatLandTaper);
	settings->SetFloat("mgv7_float_taper_exp", floatTaperExp);
	settings->SetFloat("mgv7_floatland_density", floatLandDensity);
	settings->SetInt16("mgv7_floatland_ywater", floatLandYwater);

	settings->SetFloat("mgv7_cave_width", caveWidth);
	settings->SetInt16("mgv7_large_cave_depth", largeCaveDepth);
	settings->SetUInt16("mgv7_small_cave_num_min", smallCaveNumMin);
	settings->SetUInt16("mgv7_small_cave_num_max", smallCaveNumMax);
	settings->SetUInt16("mgv7_large_cave_num_min", largeCaveNumMin);
	settings->SetUInt16("mgv7_large_cave_num_max", largeCaveNumMax);
	settings->SetFloat("mgv7_large_cave_flooded", largeCaveFlooded);
	settings->SetInt16("mgv7_cavern_limit", cavernLimit);
	settings->SetInt16("mgv7_cavern_taper", cavernTaper);
	settings->SetFloat("mgv7_cavern_threshold", cavernThreshold);
	settings->SetInt16("mgv7_dungeon_ymin", dungeonYmin);
	settings->SetInt16("mgv7_dungeon_ymax", dungeonYmax);

    SetNoiseParams(settings, "mgv7_np_terrain_base", noiseParamsTerrainBase);
    SetNoiseParams(settings, "mgv7_np_terrain_alt", noiseParamsTerrainAlt);
    SetNoiseParams(settings, "mgv7_np_terrain_persist", noiseParamsTerrainPersist);
    SetNoiseParams(settings, "mgv7_np_height_select", noiseParamsHeightSelect);
    SetNoiseParams(settings, "mgv7_np_filler_depth", noiseParamsFillerDepth);
    SetNoiseParams(settings, "mgv7_np_mount_height", noiseParamsMountHeight);
    SetNoiseParams(settings, "mgv7_np_ridge_uwater", noiseParamsRidgeUWater);
    SetNoiseParams(settings, "mgv7_np_mountain", noiseParamsMountain);
    SetNoiseParams(settings, "mgv7_np_ridge", noiseParamsRidge);
    SetNoiseParams(settings, "mgv7_np_floatland", noiseParamsFloatLand);
    SetNoiseParams(settings, "mgv7_np_cavern", noiseParamsCavern);
    SetNoiseParams(settings, "mgv7_np_cave1", noiseParamsCave1);
    SetNoiseParams(settings, "mgv7_np_cave2", noiseParamsCave2);
    SetNoiseParams(settings, "mgv7_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorV7Params::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgv7_spflags", FlagdescMapGeneratorV7,
		MGV7_MOUNTAINS | MGV7_RIDGES | MGV7_CAVERNS);
}


////////////////////////////////////////////////////////////////////////////////


int MapGeneratorV7::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	// If rivers are enabled, first check if in a river
	if (mSPFlags & MGV7_RIDGES) 
    {
		float width = 0.2f;
		float uwatern = NoisePerlin2D(&mNoiseRidgeUWater->mNoiseParams, pos[0], pos[1], mSeed) * 2.0f;
		if (std::fabs(uwatern) <= width)
			return MAX_MAP_GENERATION_LIMIT; // Unsuitable spawn point
	}

	// Terrain noise 'offset' is the average level of that terrain.
	// At least 50% of terrain will be below the higher of base and alt terrain
	// 'offset's.
	// Raising the maximum spawn level above 'mWaterLevel + 16' is necessary
	// for when terrain 'offset's are set much higher than WaterLevel.
	int16_t maxSpawnY = (int16_t)std::fmax(std::fmax(mNoiseTerrainAlt->mNoiseParams.offset,
			mNoiseTerrainBase->mNoiseParams.offset), mWaterLevel + 16);
	// Base terrain calculation
	int16_t y = (int16_t)BaseTerrainLevelAtPoint(pos[0], pos[1]);

	// If mountains are disabled, terrain level is base terrain level.
	// Avoids mid-air spawn where mountain terrain would have been.
	if (!(mSPFlags & MGV7_MOUNTAINS)) 
    {
		if (y < mWaterLevel || y > maxSpawnY)
			return MAX_MAP_GENERATION_LIMIT; // Unsuitable spawn point

		// y + 2 because y is surface level and due to biome 'dust'
		return y + 2;
	}

	// Search upwards for first node without mountain terrain
	int iters = 256;
	while (iters > 0 && y <= maxSpawnY) 
    {
		if (!GetMountainTerrainAtPoint(pos[0], y + 1, pos[1]))
        {
			if (y <= mWaterLevel)
				return MAX_MAP_GENERATION_LIMIT; // Unsuitable spawn point

			// y + 1 due to biome 'dust'
			return y + 1;
		}
		y++;
		iters--;
	}

	// Unsuitable spawn point
	return MAX_MAP_GENERATION_LIMIT;
}


void MapGeneratorV7::MakeChunk(BlockMakeData *data)
{
	// Pre-conditions
	LogAssert(data->vmanip, "invalid vmanip");
	LogAssert(data->nodeMgr, "invalid node manager");

	//TimeTaker t("MakeChunk");

	this->mGenerating = true;
	this->mMMVManip = data->vmanip;
	this->mNodeMgr = data->nodeMgr;

	Vector3<short> blockPosMin = data->blockPosMin;
	Vector3<short> blockPosMax = data->blockPosMax;
	mNodeMin = blockPosMin * (short)MAP_BLOCKSIZE;
    mNodeMax = (blockPosMax + Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};
    mFullNodeMin = (blockPosMin - Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE;
    mFullNodeMax = (blockPosMax + Vector3<short>{2, 2, 2}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};

	mBlockSeed = GetBlockSeed2(mFullNodeMin, mSeed);

	// Generate base and mountain terrain
	int16_t stoneSurfaceMaxY = GenerateTerrain();

	// Create heightmap
	UpdateHeightmap(mNodeMin, mNodeMax);

	// Init biome generator, place biome-specific nodes, and build mBiomeMap
	if (mFlags & MG_BIOMES) 
    {
		mBiomeGenerator->CalculateBiomeNoise(mNodeMin);
		GenerateBiomes();
	}

	// Generate tunnels, caverns and large randomwalk caves
	if (mFlags & MG_CAVES) 
    {
		// Generate tunnels first as caverns confuse them
		GenerateCavesNoiseIntersection(stoneSurfaceMaxY);

		// Generate caverns
		bool nearCavern = false;
		if (mSPFlags & MGV7_CAVERNS)
			nearCavern = GenerateCavernsNoise(stoneSurfaceMaxY);

		// Generate large randomwalk caves
        if (nearCavern)
        {
            // Disable large randomwalk caves in this mapchunk by setting
            // 'large cave depth' to world base. Avoids excessive liquid in
            // large caverns and floating blobs of overgenerated liquid.
            GenerateCavesRandomWalk(stoneSurfaceMaxY, -MAX_MAP_GENERATION_LIMIT);
        }
		else
			GenerateCavesRandomWalk(stoneSurfaceMaxY, mLargeCaveDepth);
	}

	// Generate the registered ores
	if (mFlags & MG_ORES)
		mEmerge->mOreMgr->PlaceAllOres(this, mBlockSeed, mNodeMin, mNodeMax);

	// Generate dungeons
	if (mFlags & MG_DUNGEONS)
		GenerateDungeons(stoneSurfaceMaxY);

	// Generate the registered decorations
	if (mFlags & MG_DECORATIONS)
		mEmerge->mDecoMgr->PlaceAllDecos(this, mBlockSeed, mNodeMin, mNodeMax);

	// Sprinkle some dust on top after everything else was generated
	if (mFlags & MG_BIOMES)
		DustTopNodes();

	// Update liquids
	UpdateLiquid(&data->transformingLiquid, mFullNodeMin, mFullNodeMax);

	// Calculate lighting
	// Limit floatland shadows
	bool propagateShadow = !((mSPFlags & MGV7_FLOATLANDS) &&
		mNodeMax[1] >= mFloatLandYmin - mChunkSize[1] * 2 && mNodeMin[1] <= mFloatLandYmax);

    if (mFlags & MG_LIGHT)
    {
        CalculateLighting(mNodeMin - Vector3<short>{0, 1, 0}, mNodeMax + Vector3<short>{0, 1, 0},
            mFullNodeMin, mFullNodeMax, propagateShadow);
    }

	this->mGenerating = false;

	//printf("MakeChunk: %lums\n", t.stop());
}


////////////////////////////////////////////////////////////////////////////////


float MapGeneratorV7::BaseTerrainLevelAtPoint(int16_t x, int16_t z)
{
	float hselect = NoisePerlin2D(&mNoiseHeightSelect->mNoiseParams, x, z, mSeed);
	hselect = std::clamp(hselect, 0.0f, 1.0f);

	float persist = NoisePerlin2D(&mNoiseTerrainPersist->mNoiseParams, x, z, mSeed);

	mNoiseTerrainBase->mNoiseParams.persist = persist;
	float heightBase = NoisePerlin2D(&mNoiseTerrainBase->mNoiseParams, x, z, mSeed);

	mNoiseTerrainAlt->mNoiseParams.persist = persist;
	float heightAlt = NoisePerlin2D(&mNoiseTerrainAlt->mNoiseParams, x, z, mSeed);

	if (heightAlt > heightBase)
		return heightAlt;

	return (heightBase * hselect) + (heightAlt * (1.0f - hselect));
}


float MapGeneratorV7::BaseTerrainLevelFromMap(int index)
{
	float hselect = std::clamp(mNoiseHeightSelect->mResult[index], 0.0f, 1.0f);
	float heightBase = mNoiseTerrainBase->mResult[index];
	float heightAlt = mNoiseTerrainAlt->mResult[index];

	if (heightAlt > heightBase)
		return heightAlt;

	return (heightBase * hselect) + (heightAlt * (1.0f - hselect));
}


bool MapGeneratorV7::GetMountainTerrainAtPoint(int16_t x, int16_t y, int16_t z)
{
	float mntHeightNoise =
		std::fmax(NoisePerlin2D(&mNoiseMountHeight->mNoiseParams, x, z, mSeed), 1.0f);
	float densityGradient = -((float)(y - mMountZeroLevel) / mntHeightNoise);
	float mntNoise = NoisePerlin3D(&mNoiseMountain->mNoiseParams, x, y, z, mSeed);

	return mntNoise + densityGradient >= 0.0f;
}


bool MapGeneratorV7::GetMountainTerrainFromMap(int idx_xyz, int idx_xz, int16_t y)
{
	float mounthn = std::fmax(mNoiseMountHeight->mResult[idx_xz], 1.0f);
	float densityGradient = -((float)(y - mMountZeroLevel) / mounthn);
	float mountn = mNoiseMountain->mResult[idx_xyz];

	return mountn + densityGradient >= 0.0f;
}


bool MapGeneratorV7::GetRiverChannelFromMap(int idx_xyz, int idx_xz, int16_t y)
{
	// Maximum width of river channel. Creates the vertical canyon walls
	float width = 0.2f;
	float absuwatern = std::fabs(mNoiseRidgeUWater->mResult[idx_xz]) * 2.0f;
	if (absuwatern > width)
		return false;

	float altitude = (float)(y - mWaterLevel);
	float heightMod = (altitude + 17.0f) / 2.5f;
	float widthMod = width - absuwatern;
	float nridge = mNoiseRidge->mResult[idx_xyz] * std::fmax(altitude, 0.0f) / 7.0f;

	return nridge + widthMod * heightMod >= 0.6f;
}


bool MapGeneratorV7::GetFloatlandTerrainFromMap(int idx_xyz, float floatOffset)
{
	return mNoiseFloatLand->mResult[idx_xyz] + mFloatLandDensity - floatOffset >= 0.0f;
}


int MapGeneratorV7::GenerateTerrain()
{
	MapNode nodeAir(CONTENT_AIR);
	MapNode nodeStone(mContentStone);
	MapNode nodeWater(mContentWaterSource);

	//// Calculate noise for terrain generation
	mNoiseTerrainPersist->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	float* persistmap = mNoiseTerrainPersist->mResult;

	mNoiseTerrainBase->PerlinMap2D(mNodeMin[0], mNodeMin[2], persistmap);
	mNoiseTerrainAlt->PerlinMap2D(mNodeMin[0], mNodeMin[2], persistmap);
	mNoiseHeightSelect->PerlinMap2D(mNodeMin[0], mNodeMin[2]);

	if (mSPFlags & MGV7_MOUNTAINS) 
    {
		mNoiseMountHeight->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
		mNoiseMountain->PerlinMap3D(mNodeMin[0], (float)(mNodeMin[1] - 1), mNodeMin[2]);
	}

	//// Floatlands
	// 'Generate floatlands in this mapchunk' bool for
	// simplification of condition checks in y-loop.
	bool genFloatLands = false;
	unsigned char cacheIndex = 0;
	// Y values where floatland tapering starts
	int16_t floatTaperYmax = mFloatLandYmax - mFloatLandTaper;
	int16_t floatTaperYmin = mFloatLandYmin + mFloatLandTaper;

	if ((mSPFlags & MGV7_FLOATLANDS) && mNodeMax[1] >= mFloatLandYmin && mNodeMin[1] <= mFloatLandYmax) 
    {
		genFloatLands = true;
		// Calculate noise for floatland generation
		mNoiseFloatLand->PerlinMap3D(mNodeMin[0], (float)(mNodeMin[1] - 1), mNodeMin[2]);

		// Cache floatland noise offset values, for floatland tapering
		for (int16_t y = mNodeMin[1] - 1; y <= mNodeMax[1] + 1; y++, cacheIndex++) 
        {
			float floatOffset = 0.0f;
			if (y > floatTaperYmax) 
            {
				floatOffset = std::pow((y - floatTaperYmax) / (float)mFloatLandTaper, mFloatTaperExp) * 4.0f;
			} 
            else if (y < floatTaperYmin) 
            {
				floatOffset = std::pow((floatTaperYmin - y) / (float)mFloatLandTaper, mFloatTaperExp) * 4.0f;
			}
			mFloatOffsetCache[cacheIndex] = floatOffset;
		}
	}

	// 'Generate rivers in this mapchunk' bool for
	// simplification of condition checks in y-loop.
	bool genRivers = (mSPFlags & MGV7_RIDGES) && mNodeMax[1] >= mWaterLevel - 16 && !genFloatLands;
	if (genRivers) 
    {
		mNoiseRidge->PerlinMap3D(mNodeMin[0], (float)(mNodeMin[1] - 1), mNodeMin[2]);
		mNoiseRidgeUWater->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	}

	//// Place nodes
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	int16_t stoneSurfaceMaxY = -MAX_MAP_GENERATION_LIMIT;
	unsigned int index2d = 0;

    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, index2d++)
        {
            int16_t surfaceY = (int16_t)BaseTerrainLevelFromMap(index2d);
            if (surfaceY > stoneSurfaceMaxY)
                stoneSurfaceMaxY = surfaceY;

            cacheIndex = 0;
            unsigned int vi = mMMVManip->mArea.Index(x, mNodeMin[1] - 1, z);
            unsigned int index3d = (z - mNodeMin[2]) * mZStride1u1d + (x - mNodeMin[0]);

            for (int16_t y = mNodeMin[1] - 1; y <= mNodeMax[1] + 1;
                y++, index3d += mYStride, VoxelArea::AddY(em, vi, 1), cacheIndex++) 
            {
                if (mMMVManip->mData[vi].GetContent() != CONTENT_IGNORE)
                    continue;

                bool isRiverChannel = genRivers &&
                    GetRiverChannelFromMap(index3d, index2d, y);
                if (y <= surfaceY && !isRiverChannel) 
                {
                    mMMVManip->mData[vi] = nodeStone; // Base terrain
                }
                else if ((mSPFlags & MGV7_MOUNTAINS) &&
                    GetMountainTerrainFromMap(index3d, index2d, y) && !isRiverChannel) 
                {
                    mMMVManip->mData[vi] = nodeStone; // Mountain terrain
                    if (y > stoneSurfaceMaxY)
                        stoneSurfaceMaxY = y;
                }
                else if (genFloatLands &&
                    GetFloatlandTerrainFromMap(index3d, mFloatOffsetCache[cacheIndex])) 
                {
                    mMMVManip->mData[vi] = nodeStone; // Floatland terrain
                    if (y > stoneSurfaceMaxY)
                        stoneSurfaceMaxY = y;
                }
                else if (y <= mWaterLevel) 
                { // Surface water
                    mMMVManip->mData[vi] = nodeWater;
                }
                else if (genFloatLands && y >= floatTaperYmax && y <= mFloatLandYwater) 
                {
                    mMMVManip->mData[vi] = nodeWater; // Water for solid floatland layer only
                }
                else 
                {
                    mMMVManip->mData[vi] = nodeAir; // Air
                }
            }
        }
    }

	return stoneSurfaceMaxY;
}
