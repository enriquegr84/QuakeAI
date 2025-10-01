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


#include "MapGenerator.h"
#include "DungeonGenerator.h"
#include "CaveGenerator.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorOre.h"
#include "MapGeneratorDecoration.h"
#include "MapGeneratorV5.h"
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




FlagDescription FlagdescMapGeneratorV5[] = 
{
	{"caverns", MGV5_CAVERNS},
	{NULL,      0}
};


MapGeneratorV5::MapGeneratorV5(MapGeneratorV5Params* params, EmergeParams* emerge)
	: MapGeneratorBasic(MAPGEN_V5, params, emerge)
{
	mSPFlags = params->spFlags;
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

	// Terrain noise
	mNoiseFillerDepth = new Noise(&params->noiseParamsFillerDepth, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseFactor = new Noise(&params->noiseParamsFactor, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeight = new Noise(&params->noiseParamsHeight, mSeed, mChunkSize[0], mChunkSize[2]);

	// 3D terrain noise
	// 1-up 1-down overgeneration
	mNoiseGround = new Noise(&params->noiseParamsGround, mSeed, mChunkSize[0], mChunkSize[1] + 2, mChunkSize[2]);
	// 1 down overgeneration
	MapGeneratorBasic::mNoiseParamsCave1 = params->noiseParamsCave1;
	MapGeneratorBasic::mNoiseParamsCave2 = params->noiseParamsCave2;
	MapGeneratorBasic::mNoiseParamsCavern = params->noiseParamsCavern;
	MapGeneratorBasic::mNoiseParamsDungeons = params->noiseParamsDungeons;
}


MapGeneratorV5::~MapGeneratorV5()
{
	delete mNoiseFillerDepth;
	delete mNoiseFactor;
	delete mNoiseHeight;
	delete mNoiseGround;
}


MapGeneratorV5Params::MapGeneratorV5Params():
    noiseParamsFillerDepth(0, 1, Vector3<float>{150, 150, 150}, 261, 4, 0.7f, 2.f),
    noiseParamsFactor(0, 1, Vector3<float>{250, 250, 250}, 920381, 3, 0.45f, 2.f),
    noiseParamsHeight(0, 10, Vector3<float>{250, 250, 250}, 84174, 4, 0.5f, 2.f),
    noiseParamsGround(0, 40, Vector3<float>{80, 80, 80}, 983240, 4, 0.55f, 2.f, NOISE_FLAG_EASED),
    noiseParamsCave1(0, 12, Vector3<float>{61, 61, 61}, 52534, 3, 0.5f, 2.f),
    noiseParamsCave2(0, 12, Vector3<float>{67, 67, 67}, 10325, 3, 0.5f, 2.f),
    noiseParamsCavern(0, 1, Vector3<float>{384, 128, 384}, 723, 5, 0.63f, 2.f),
    noiseParamsDungeons(0.9f, 0.5f, Vector3<float>{500, 500, 500}, 0, 2, 0.8f, 2.f)
{
}

void MapGeneratorV5Params::ReadParams(const Settings* settings)
{

    try {

        settings->GetFlagString("mgv5_spflags", FlagdescMapGeneratorV5, &spFlags);

        caveWidth = settings->GetFloat("mgv5_cave_width");
        largeCaveDepth = settings->GetInt16("mgv5_large_cave_depth");
        smallCaveNumMin = settings->GetUInt16("mgv5_small_cave_num_min");
        smallCaveNumMax = settings->GetUInt16("mgv5_small_cave_num_max");
        largeCaveNumMin = settings->GetUInt16("mgv5_large_cave_num_min");
        largeCaveNumMax = settings->GetUInt16("mgv5_large_cave_num_max");
        largeCaveFlooded = settings->GetFloat("mgv5_large_cave_flooded");
        cavernLimit = settings->GetInt16("mgv5_cavern_limit");
        cavernTaper = settings->GetInt16("mgv5_cavern_taper");
        cavernThreshold = settings->GetFloat("mgv5_cavern_threshold");
        dungeonYmin = settings->GetInt16("mgv5_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgv5_dungeon_ymax");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgv5_np_filler_depth", noiseParamsFillerDepth);
    GetNoiseParams(settings, "mgv5_np_factor", noiseParamsFactor);
    GetNoiseParams(settings, "mgv5_np_height", noiseParamsHeight);
    GetNoiseParams(settings, "mgv5_np_ground", noiseParamsGround);
    GetNoiseParams(settings, "mgv5_np_cave1", noiseParamsCave1);
    GetNoiseParams(settings, "mgv5_np_cave2", noiseParamsCave2);
    GetNoiseParams(settings, "mgv5_np_cavern", noiseParamsCavern);
    GetNoiseParams(settings, "mgv5_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorV5Params::WriteParams(Settings* settings)
{
    settings->SetFlagString("mgv5_spflags", spFlags, FlagdescMapGeneratorV5, spFlags);
	settings->SetFloat("mgv5_cave_width", caveWidth);
	settings->SetInt16("mgv5_large_cave_depth", largeCaveDepth);
	settings->SetUInt16("mgv5_small_cave_num_min", smallCaveNumMin);
	settings->SetUInt16("mgv5_small_cave_num_max", smallCaveNumMax);
	settings->SetUInt16("mgv5_large_cave_num_min", largeCaveNumMin);
	settings->SetUInt16("mgv5_large_cave_num_max", largeCaveNumMax);
	settings->SetFloat("mgv5_large_cave_flooded", largeCaveFlooded);
	settings->SetInt16("mgv5_cavern_limit", cavernLimit);
	settings->SetInt16("mgv5_cavern_taper", cavernTaper);
	settings->SetFloat("mgv5_cavern_threshold", cavernThreshold);
	settings->SetInt16("mgv5_dungeon_ymin", dungeonYmin);
	settings->SetInt16("mgv5_dungeon_ymax", dungeonYmax);

    SetNoiseParams(settings, "mgv5_np_filler_depth", noiseParamsFillerDepth);
    SetNoiseParams(settings, "mgv5_np_factor", noiseParamsFactor);
    SetNoiseParams(settings, "mgv5_np_height", noiseParamsHeight);
    SetNoiseParams(settings, "mgv5_np_ground", noiseParamsGround);
    SetNoiseParams(settings, "mgv5_np_cave1", noiseParamsCave1);
    SetNoiseParams(settings, "mgv5_np_cave2", noiseParamsCave2);
    SetNoiseParams(settings, "mgv5_np_cavern", noiseParamsCavern);
    SetNoiseParams(settings, "mgv5_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorV5Params::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgv5_spflags", FlagdescMapGeneratorV5, MGV5_CAVERNS);
}


/////////////////////////////////////////////////////////////////


int MapGeneratorV5::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	float f = 0.55f + NoisePerlin2D(&mNoiseFactor->mNoiseParams, (float)pos[0], (float)pos[1], mSeed);
	if (f < 0.01f)
		f = 0.01f;
	else if (f >= 1.f)
		f *= 1.6f;
	float h = NoisePerlin2D(&mNoiseHeight->mNoiseParams, (float)pos[0], (float)pos[1], mSeed);

	// mNoiseHeight 'offset' is the average level of terrain. At least 50% of
	// terrain will be below this.
	// Raising the maximum spawn level above 'mWaterLevel + 16' is necessary
	// for when mNoiseHeight 'offset' is set much higher than mWaterLevel.
	int16_t maxSpawnY = std::max((short)mNoiseHeight->mNoiseParams.offset, (short)(mWaterLevel + 16));

	// Starting spawn search at maxSpawnY + 128 ensures 128 nodes of open
	// space above spawn position. Avoids spawning in possibly sealed voids.
	for (int16_t y = maxSpawnY + 128; y >= mWaterLevel; y--) 
    {
		float noiseGround = NoisePerlin3D(&mNoiseGround->mNoiseParams, (float)pos[0], (float)y, (float)pos[1], mSeed);

		if (noiseGround * f > y - h) 
        {  // If solid
			if (y < mWaterLevel || y > maxSpawnY)
				return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point

			// y + 2 because y is surface and due to biome 'dust' nodes.
			return y + 2;
		}
	}
	// Unsuitable spawn position, no ground found
	return MAX_MAP_GENERATION_LIMIT;
}


void MapGeneratorV5::MakeChunk(BlockMakeData* data)
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

	// Create a block-specific mSeed
	mBlockSeed = GetBlockSeed2(mFullNodeMin, mSeed);

	// Generate base terrain
	int16_t stoneSurfaceMaxY = GenerateBaseTerrain();

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
		if (mSPFlags & MGV5_CAVERNS)
			nearCavern = GenerateCavernsNoise(stoneSurfaceMaxY);

		// Generate large randomwalk caves
		if (nearCavern)
			// Disable large randomwalk caves in this mapchunk by setting
			// 'large cave depth' to world base. Avoids excessive liquid in
			// large caverns and floating blobs of overgenerated liquid.
			GenerateCavesRandomWalk(stoneSurfaceMaxY,
				-MAX_MAP_GENERATION_LIMIT);
		else
			GenerateCavesRandomWalk(stoneSurfaceMaxY, mLargeCaveDepth);
	}

	// Generate the registered ores
	if (mFlags & MG_ORES)
		mEmerge->mOreMgr->PlaceAllOres(this, mBlockSeed, mNodeMin, mNodeMax);

	// Generate dungeons and desert temples
	if (mFlags & MG_DUNGEONS)
		GenerateDungeons(stoneSurfaceMaxY);

	// Generate the registered decorations
	if (mFlags & MG_DECORATIONS)
		mEmerge->mDecoMgr->PlaceAllDecos(this, mBlockSeed, mNodeMin, mNodeMax);

	// Sprinkle some dust on top after everything else was generated
	if (mFlags & MG_BIOMES)
		DustTopNodes();

	//printf("MakeChunk: %dms\n", t.stop());

	// Add top and bottom side of water to transforming_liquid queue
	UpdateLiquid(&data->transformingLiquid, mFullNodeMin, mFullNodeMax);

	// Calculate lighting
	if (mFlags & MG_LIGHT) 
    {
        CalculateLighting(mNodeMin - Vector3<short>{0, 1, 0}, 
            mNodeMax + Vector3<short>{0, 1, 0}, mFullNodeMin, mFullNodeMax);
	}

	this->mGenerating = false;
}


int MapGeneratorV5::GenerateBaseTerrain()
{
	unsigned int index = 0;
	unsigned int index2d = 0;
	int stoneSurfaceMaxY = -MAX_MAP_GENERATION_LIMIT;

	mNoiseFactor->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseHeight->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseGround->PerlinMap3D(mNodeMin[0], (float)(mNodeMin[1] - 1), mNodeMin[2]);

	for (int16_t z=mNodeMin[2]; z<=mNodeMax[2]; z++) 
    {
		for (int16_t y=mNodeMin[1] - 1; y<=mNodeMax[1] + 1; y++) 
        {
			unsigned int vi = mMMVManip->mArea.Index(mNodeMin[0], y, z);
			for (int16_t x=mNodeMin[0]; x<=mNodeMax[0]; x++, vi++, index++, index2d++) 
            {
				if (mMMVManip->mData[vi].GetContent() != CONTENT_IGNORE)
					continue;

				float f = 0.55f + mNoiseFactor->mResult[index2d];
				if (f < 0.01f)
					f = 0.01f;
				else if (f >= 1.f)
					f *= 1.6f;
				float h = mNoiseHeight->mResult[index2d];

				if (mNoiseGround->mResult[index] * f < y - h) 
                {
					if (y <= mWaterLevel)
                        mMMVManip->mData[vi] = MapNode(mContentWaterSource);
					else
                        mMMVManip->mData[vi] = MapNode(CONTENT_AIR);
				}
                else 
                {
                    mMMVManip->mData[vi] = MapNode(mContentStone);
					if (y > stoneSurfaceMaxY)
						stoneSurfaceMaxY = y;
				}
			}
			index2d -= mYStride;
		}
		index2d += mYStride;
	}

	return stoneSurfaceMaxY;
}
