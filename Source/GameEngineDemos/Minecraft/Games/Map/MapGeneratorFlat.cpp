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

#include "MapGenerator.h"
#include "DungeonGenerator.h"
#include "CaveGenerator.h"
#include "MapGeneratorFlat.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorOre.h"
#include "MapGeneratorDecoration.h"
#include "MapBlock.h"
#include "MapNode.h"
#include "Map.h"

#include "Emerge.h"
#include "Voxel.h"
#include "VoxelAlgorithms.h"

#include "../../Utils/Noise.h"

#include "Core/Utility/StringUtil.h"
#include "Application/Settings.h" // For g_settings


FlagDescription FlagdescMapGeneratorFlat[] = 
{
	{"lakes",   MGFLAT_LAKES},
	{"hills",   MGFLAT_HILLS},
	{"caverns", MGFLAT_CAVERNS},
	{NULL,    0}
};

///////////////////////////////////////////////////////////////////////////////////////


MapGeneratorFlat::MapGeneratorFlat(MapGeneratorFlatParams* params, EmergeParams* emerge)
	: MapGeneratorBasic(MAPGEN_FLAT, params, emerge)
{
	mSPFlags            = params->spFlags;
	mGroundLevel       = params->groundLevel;
	mLakeThreshold     = params->lakeThreshold;
	mLakeSteepness     = params->lakeSteepness;
	mHillThreshold     = params->hillThreshold;
	mHillSteepness     = params->hillSteepness;

	mCaveWidth         = params->caveWidth;
	mSmallCaveNumMin = params->smallCaveNumMin;
	mSmallCaveNumMax = params->smallCaveNumMax;
	mLargeCaveNumMin = params->largeCaveNumMin;
	mLargeCaveNumMax = params->largeCaveNumMax;
	mLargeCaveDepth   = params->largeCaveDepth;
	mLargeCaveFlooded = params->largeCaveFlooded;
	mCavernLimit       = params->cavernLimit;
	mCavernTaper       = params->cavernTaper;
	mCavernThreshold   = params->cavernThreshold;
	mDungeonYmin       = params->dungeonYmin;
	mDungeonYmax       = params->dungeonYmax;

	// 2D noise
	mNoiseFillerDepth = new Noise(&params->noiseParamsFillerDepth, mSeed, mChunkSize[0], mChunkSize[2]);

	if ((mSPFlags & MGFLAT_LAKES) || (mSPFlags & MGFLAT_HILLS))
		mNoiseTerrain = new Noise(&params->noiseParamsTerrain, mSeed, mChunkSize[0], mChunkSize[2]);

	// 3D noise
	MapGeneratorBasic::mNoiseParamsCave1    = params->noiseParamsCave1;
	MapGeneratorBasic::mNoiseParamsCave2    = params->noiseParamsCave2;
	MapGeneratorBasic::mNoiseParamsCavern   = params->noiseParamsCavern;
	MapGeneratorBasic::mNoiseParamsDungeons = params->noiseParamsDungeons;
}


MapGeneratorFlat::~MapGeneratorFlat()
{
	delete mNoiseFillerDepth;

	if ((mSPFlags & MGFLAT_LAKES) || (mSPFlags & MGFLAT_HILLS))
		delete mNoiseTerrain;
}


MapGeneratorFlatParams::MapGeneratorFlatParams():
    noiseParamsTerrain(0, 1, Vector3<float>{600, 600, 600}, 7244, 5, 0.6f, 2.f),
    noiseParamsFillerDepth(0, 1.2f, Vector3<float>{150, 150, 150}, 261, 3, 0.7f, 2.f),
    noiseParamsCavern(0.f, 1.f, Vector3<float>{384, 128, 384}, 723, 5, 0.63f, 2.f),
    noiseParamsCave1(0, 12, Vector3<float>{61, 61, 61}, 52534, 3, 0.5f, 2.f),
    noiseParamsCave2(0, 12, Vector3<float>{67, 67, 67}, 10325, 3, 0.5f, 2.f),
    noiseParamsDungeons(0.9f, 0.5f, Vector3<float>{500, 500, 500}, 0, 2, 0.8f, 2.f)
{
}


void MapGeneratorFlatParams::ReadParams(const Settings* settings)
{
    try {
	    settings->GetFlagString("mgflat_spflags", FlagdescMapGeneratorFlat, &spFlags);
        
        groundLevel = settings->GetInt16("mgflat_ground_level");
        largeCaveDepth = settings->GetInt16("mgflat_large_cave_depth");
        smallCaveNumMin = settings->GetUInt16("mgflat_small_cave_num_min");
        smallCaveNumMax = settings->GetUInt16("mgflat_small_cave_num_max");
        largeCaveNumMin = settings->GetUInt16("mgflat_large_cave_num_min");
        largeCaveNumMax = settings->GetUInt16("mgflat_large_cave_num_max");
        largeCaveFlooded = settings->GetFloat("mgflat_large_cave_flooded");
        caveWidth = settings->GetFloat("mgflat_cave_width");
        lakeThreshold = settings->GetFloat("mgflat_lake_threshold");
        lakeSteepness = settings->GetFloat("mgflat_lake_steepness");
        hillThreshold = settings->GetFloat("mgflat_hillThreshold");
        hillSteepness = settings->GetFloat("mgflat_hillSteepness");
        cavernLimit = settings->GetInt16("mgflat_cavern_limit");
        cavernTaper = settings->GetInt16("mgflat_cavern_taper");
        cavernThreshold = settings->GetFloat("mgflat_cavern_threshold");
        dungeonYmin = settings->GetInt16("mgflat_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgflat_dungeon_ymax");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgflat_np_terrain", noiseParamsTerrain);
    GetNoiseParams(settings, "mgflat_np_filler_depth", noiseParamsFillerDepth);
    GetNoiseParams(settings, "mgflat_np_cavern", noiseParamsCavern);
    GetNoiseParams(settings, "mgflat_np_cave1", noiseParamsCave1);
    GetNoiseParams(settings, "mgflat_np_cave2", noiseParamsCave2);
    GetNoiseParams(settings, "mgflat_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorFlatParams::WriteParams(Settings* settings)
{
	settings->SetFlagString("mgflat_spflags", spFlags, FlagdescMapGeneratorFlat, spFlags);

	settings->SetInt16("mgflat_ground_level", groundLevel);
	settings->SetInt16("mgflat_large_cave_depth", largeCaveDepth);
	settings->SetUInt16("mgflat_small_cave_num_min", smallCaveNumMin);
	settings->SetUInt16("mgflat_small_cave_num_max", smallCaveNumMax);
	settings->SetUInt16("mgflat_large_cave_num_min", largeCaveNumMin);
	settings->SetUInt16("mgflat_large_cave_num_max", largeCaveNumMax);
	settings->SetFloat("mgflat_large_cave_flooded", largeCaveFlooded);
	settings->SetFloat("mgflat_cave_width", caveWidth);
	settings->SetFloat("mgflat_lake_threshold", lakeThreshold);
	settings->SetFloat("mgflat_lake_steepness", lakeSteepness);
	settings->SetFloat("mgflat_hill_threshold", hillThreshold);
	settings->SetFloat("mgflat_hill_steepness", hillSteepness);
	settings->SetInt16("mgflat_cavern_limit", cavernLimit);
	settings->SetInt16("mgflat_cavern_taper", cavernTaper);
	settings->SetFloat("mgflat_cavern_threshold", cavernThreshold);
	settings->SetInt16("mgflat_dungeon_ymin", dungeonYmin);
	settings->SetInt16("mgflat_dungeon_ymax", dungeonYmax);

    SetNoiseParams(settings, "mgflat_np_terrain", noiseParamsTerrain);
    SetNoiseParams(settings, "mgflat_np_filler_depth", noiseParamsFillerDepth);
    SetNoiseParams(settings, "mgflat_np_cavern", noiseParamsCavern);
    SetNoiseParams(settings, "mgflat_np_cave1", noiseParamsCave1);
    SetNoiseParams(settings, "mgflat_np_cave2", noiseParamsCave2);
    SetNoiseParams(settings, "mgflat_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorFlatParams::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgflat_spflags", FlagdescMapGeneratorFlat, 0);
}


/////////////////////////////////////////////////////////////////


int MapGeneratorFlat::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	int16_t stoneLevel = mGroundLevel;
	float noiseTerrain = 
		((mSPFlags & MGFLAT_LAKES) || (mSPFlags & MGFLAT_HILLS)) ?
		NoisePerlin2D(&mNoiseTerrain->mNoiseParams, pos[0], pos[1], mSeed) : 0.0f;

	if ((mSPFlags & MGFLAT_LAKES) && noiseTerrain < mLakeThreshold) 
    {
		int16_t depress = (int16_t)((mLakeThreshold - noiseTerrain) * mLakeSteepness);
		stoneLevel = mGroundLevel - depress;
	} 
    else if ((mSPFlags & MGFLAT_HILLS) && noiseTerrain > mHillThreshold) 
    {
		int16_t rise = (int16_t)((noiseTerrain - mHillThreshold) * mHillSteepness);
	 	stoneLevel = mGroundLevel + rise;
	}

    if (mGroundLevel < mWaterLevel)
    {
        // Ocean world, may not have islands so allow spawn in water
        return std::max(stoneLevel + 2, mWaterLevel);
    }

	if (stoneLevel >= mWaterLevel)
		// Spawn on land
		// + 2 not + 1, to spawn above biome 'dust' nodes
		return stoneLevel + 2;

	// Unsuitable spawn point
	return MAX_MAP_GENERATION_LIMIT;
}


void MapGeneratorFlat::MakeChunk(BlockMakeData *data)
{
	// Pre-conditions
	LogAssert(data->vmanip, "invalid vmanip");
    LogAssert(data->nodeMgr, "invalid node manager");

	this->mGenerating = true;
	this->mMMVManip = data->vmanip;
	this->mNodeMgr = data->nodeMgr;
	//TimeTaker t("MakeChunk");

	Vector3<short> blockPosMin = data->blockPosMin;
	Vector3<short> blockPosMax = data->blockPosMax;
	mNodeMin = blockPosMin * (short)MAP_BLOCKSIZE;
    mNodeMax = (blockPosMax + Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};
    mFullNodeMin = (blockPosMin - Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE;
    mFullNodeMax = (blockPosMax + Vector3<short>{2, 2, 2}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};

	mBlockSeed = GetBlockSeed2(mFullNodeMin, mSeed);

	// Generate base terrain, mountains, and ridges with initial heightmaps
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
		if (mSPFlags & MGFLAT_CAVERNS)
			nearCavern = GenerateCavernsNoise(stoneSurfaceMaxY);

		// Generate large randomwalk caves
		if (nearCavern)
			// Disable large randomwalk caves in this mapchunk by setting
			// 'large cave depth' to world base. Avoids excessive liquid in
			// large caverns and floating blobs of overgenerated liquid.
			GenerateCavesRandomWalk(stoneSurfaceMaxY, -MAX_MAP_GENERATION_LIMIT);
		else
			GenerateCavesRandomWalk(stoneSurfaceMaxY, mLargeCaveDepth);
	}

	// Generate the registered ores
	if (mFlags & MG_ORES)
		mEmerge->mOreMgr->PlaceAllOres(this, mBlockSeed, mNodeMin, mNodeMax);

	if (mFlags & MG_DUNGEONS)
		GenerateDungeons(stoneSurfaceMaxY);

	// Generate the registered decorations
	if (mFlags & MG_DECORATIONS)
		mEmerge->mDecoMgr->PlaceAllDecos(this, mBlockSeed, mNodeMin, mNodeMax);

	// Sprinkle some dust on top after everything else was generated
	if (mFlags & MG_BIOMES)
		DustTopNodes();

	//printf("MakeChunk: %dms\n", t.stop());

	UpdateLiquid(&data->transformingLiquid, mFullNodeMin, mFullNodeMax);

    if (mFlags & MG_LIGHT)
    {
        CalculateLighting(mNodeMin - Vector3<short>{0, 1, 0},
            mNodeMax + Vector3<short>{0, 1, 0}, mFullNodeMin, mFullNodeMax);
    }

	//SetLighting(mNodeMin - Vector3<short>(1, 0, 1) * MAP_BLOCKSIZE,
	//			mNodeMax + Vector3<short>(1, 0, 1) * MAP_BLOCKSIZE, 0xFF);

	this->mGenerating = false;
}


int16_t MapGeneratorFlat::GenerateTerrain()
{
	MapNode nodeAir(CONTENT_AIR);
	MapNode nodeStone(mContentStone);
	MapNode nodeWater(mContentWaterSource);

	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	int16_t stoneSurfaceMaxY = -MAX_MAP_GENERATION_LIMIT;
	unsigned int ni2d = 0;

	bool useNoise = (mSPFlags & MGFLAT_LAKES) || (mSPFlags & MGFLAT_HILLS);
	if (useNoise)
		mNoiseTerrain->PerlinMap2D(mNodeMin[0], mNodeMin[2]);

    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, ni2d++)
        {
            int16_t stoneLevel = mGroundLevel;
            float noiseTerrain = useNoise ? mNoiseTerrain->mResult[ni2d] : 0.0f;

            if ((mSPFlags & MGFLAT_LAKES) && noiseTerrain < mLakeThreshold) 
            {
                int16_t depress = (int16_t)((mLakeThreshold - noiseTerrain) * mLakeSteepness);
                stoneLevel = mGroundLevel - depress;
            }
            else if ((mSPFlags & MGFLAT_HILLS) && noiseTerrain > mHillThreshold) 
            {
                int16_t rise = (int16_t)((noiseTerrain - mHillThreshold) * mHillSteepness);
                stoneLevel = mGroundLevel + rise;
            }

            unsigned int vi = mMMVManip->mArea.Index(x, mNodeMin[1] - 1, z);
            for (int16_t y = mNodeMin[1] - 1; y <= mNodeMax[1] + 1; y++) 
            {
                if (mMMVManip->mData[vi].GetContent() == CONTENT_IGNORE) 
                {
                    if (y <= stoneLevel) 
                    {
                        mMMVManip->mData[vi] = nodeStone;
                        if (y > stoneSurfaceMaxY)
                            stoneSurfaceMaxY = y;
                    }
                    else if (y <= mWaterLevel) 
                    {
                        mMMVManip->mData[vi] = nodeWater;
                    }
                    else 
                    {
                        mMMVManip->mData[vi] = nodeAir;
                    }
                }
                VoxelArea::AddY(em, vi, 1);
            }
        }
    }

	return stoneSurfaceMaxY;
}
