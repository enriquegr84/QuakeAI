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


#include "MapGenerator.h"
#include "DungeonGenerator.h"
#include "CaveGenerator.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorOre.h"
#include "MapGeneratorDecoration.h"
#include "MapGeneratorCarpathian.h"
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


FlagDescription FlagdescMapGeneratorCarpathian[] = 
{
	{"caverns", MGCARPATHIAN_CAVERNS},
	{"rivers",  MGCARPATHIAN_RIVERS},
	{NULL,      0}
};


///////////////////////////////////////////////////////////////////////////////


MapGeneratorCarpathian::MapGeneratorCarpathian(
    MapGeneratorCarpathianParams *params, EmergeParams *emerge)
	: MapGeneratorBasic(MAPGEN_CARPATHIAN, params, emerge)
{
	mBaseLevel = params->baseLevel;
	mRiverWidth = params->riverWidth;
	mRiverDepth = params->riverDepth;
	mValleyWidth = params->valleyWidth;

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

	mGradWL = 1 - mWaterLevel;

	//// 2D Terrain noise
	mNoiseFillerDepth  = new Noise(&params->noiseParamsFillerDepth, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeight1 = new Noise(&params->noiseParamsHeight1, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeight2 = new Noise(&params->noiseParamsHeight2, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeight3 = new Noise(&params->noiseParamsHeight3, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeight4 = new Noise(&params->noiseParamsHeight4, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHillsTerrain = new Noise(&params->noiseParamsHillsTerrain, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseRidgeTerrain = new Noise(&params->noiseParamsRidgeTerrain, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseStepTerrain  = new Noise(&params->noiseParamsStepTerrain,  mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseHills = new Noise(&params->noiseParamsHills, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseRidgeMnt = new Noise(&params->noiseParamsRidgeMnt, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseStepMnt = new Noise(&params->noiseParamsStepMnt, mSeed, mChunkSize[0], mChunkSize[2]);
	if (mSPFlags & MGCARPATHIAN_RIVERS)
		mNoiseRivers = new Noise(&params->noiseParamsRivers, mSeed, mChunkSize[0], mChunkSize[2]);

	//// 3D terrain noise
	// 1 up 1 down overgeneration
	mNoiseMntVar = new Noise(&params->noiseParamsMntVar, mSeed, mChunkSize[0], mChunkSize[1] + 2, mChunkSize[2]);

	//// Cave noise
	MapGeneratorBasic::mNoiseParamsCave1 = params->noiseParamsCave1;
	MapGeneratorBasic::mNoiseParamsCave2 = params->noiseParamsCave2;
	MapGeneratorBasic::mNoiseParamsCavern = params->noiseParamsCavern;
	MapGeneratorBasic::mNoiseParamsDungeons = params->noiseParamsDungeons;
}


MapGeneratorCarpathian::~MapGeneratorCarpathian()
{
	delete mNoiseFillerDepth;
	delete mNoiseHeight1;
	delete mNoiseHeight2;
	delete mNoiseHeight3;
	delete mNoiseHeight4;
	delete mNoiseHillsTerrain;
	delete mNoiseRidgeTerrain;
	delete mNoiseStepTerrain;
	delete mNoiseHills;
	delete mNoiseRidgeMnt;
	delete mNoiseStepMnt;
	if (mSPFlags & MGCARPATHIAN_RIVERS)
		delete mNoiseRivers;

	delete mNoiseMntVar;
}


MapGeneratorCarpathianParams::MapGeneratorCarpathianParams():
    noiseParamsFillerDepth(0, 1, Vector3<float>{128, 128, 128}, 261, 3, 0.7f, 2.f),
    noiseParamsHeight1(0, 5, Vector3<float>{251, 251, 251}, 9613, 5, 0.5f, 2.f),
    noiseParamsHeight2(0, 5, Vector3<float>{383, 383, 383}, 1949, 5, 0.5f, 2.f),
    noiseParamsHeight3(0, 5, Vector3<float>{509, 509, 509}, 3211, 5, 0.5f, 2.f),
    noiseParamsHeight4(0, 5, Vector3<float>{631, 631, 631}, 1583, 5, 0.5f, 2.f),
    noiseParamsHillsTerrain(1, 1, Vector3<float>{1301, 1301, 1301}, 1692, 5, 0.5f, 2.f),
    noiseParamsRidgeTerrain(1, 1, Vector3<float>{1889, 1889, 1889}, 3568, 5, 0.5f, 2.f),
    noiseParamsStepTerrain(1, 1, Vector3<float>{1889, 1889, 1889}, 4157, 5, 0.5f, 2.f),
    noiseParamsHills(0, 3, Vector3<float>{257, 257, 257}, 6604, 6, 0.5f, 2.f),
    noiseParamsRidgeMnt(0, 12, Vector3<float>{743, 743, 743}, 5520, 6, 0.7f, 2.f),
    noiseParamsStepMnt(0, 8, Vector3<float>{509, 509, 509}, 2590, 6, 0.6f, 2.f),
    noiseParamsRivers(0, 1, Vector3<float>{1000, 1000, 1000}, 85039, 5, 0.6f, 2.f),
    noiseParamsMntVar(0, 1, Vector3<float>{499, 499, 499}, 2490, 5, 0.55f, 2.f),
    noiseParamsCave1(0, 12, Vector3<float>{61, 61, 61}, 52534, 3, 0.5f, 2.f),
    noiseParamsCave2(0, 12, Vector3<float>{67, 67, 67}, 10325, 3, 0.5f, 2.f),
    noiseParamsCavern(0, 1, Vector3<float>{384, 128, 384}, 723, 5, 0.63f, 2.f),
    noiseParamsDungeons(0.9f, 0.5f, Vector3<float>{500, 500, 500}, 0, 2, 0.8f, 2.f)
{
}


void MapGeneratorCarpathianParams::ReadParams(const Settings* settings)
{
    try {
        settings->GetFlagString("mgcarpathian_spflags", FlagdescMapGeneratorCarpathian, &spFlags);

        baseLevel = settings->GetFloat("mgcarpathian_base_level");
        riverWidth = settings->GetFloat("mgcarpathian_river_width");
        riverDepth = settings->GetFloat("mgcarpathian_river_depth");
        valleyWidth = settings->GetFloat("mgcarpathian_valley_width");

        caveWidth = settings->GetFloat("mgcarpathian_cave_width");
        largeCaveDepth = settings->GetInt16("mgcarpathian_large_cave_depth");
        smallCaveNumMin = settings->GetUInt16("mgcarpathian_small_cave_num_min");
        smallCaveNumMax = settings->GetUInt16("mgcarpathian_small_cave_num_max");
        largeCaveNumMin = settings->GetUInt16("mgcarpathian_large_cave_num_min");
        largeCaveNumMax = settings->GetUInt16("mgcarpathian_large_cave_num_max");
        largeCaveFlooded = settings->GetFloat("mgcarpathian_large_cave_flooded");
        cavernLimit = settings->GetInt16("mgcarpathian_cavern_limit");
        cavernTaper = settings->GetInt16("mgcarpathian_cavern_taper");
        cavernThreshold = settings->GetFloat("mgcarpathian_cavern_threshold");
        dungeonYmin = settings->GetInt16("mgcarpathian_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgcarpathian_dungeon_ymax");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgcarpathian_np_filler_depth", noiseParamsFillerDepth);
    GetNoiseParams(settings, "mgcarpathian_np_height1", noiseParamsHeight1);
    GetNoiseParams(settings, "mgcarpathian_np_height2", noiseParamsHeight2);
    GetNoiseParams(settings, "mgcarpathian_np_height3", noiseParamsHeight3);
    GetNoiseParams(settings, "mgcarpathian_np_height4", noiseParamsHeight4);
    GetNoiseParams(settings, "mgcarpathian_np_hills_terrain", noiseParamsHillsTerrain);
    GetNoiseParams(settings, "mgcarpathian_np_ridge_terrain", noiseParamsRidgeTerrain);
    GetNoiseParams(settings, "mgcarpathian_np_step_terrain", noiseParamsStepTerrain);
    GetNoiseParams(settings, "mgcarpathian_np_hills", noiseParamsHills);
    GetNoiseParams(settings, "mgcarpathian_np_ridgeMnt", noiseParamsRidgeMnt);
    GetNoiseParams(settings, "mgcarpathian_np_stepMnt", noiseParamsStepMnt);
    GetNoiseParams(settings, "mgcarpathian_np_rivers", noiseParamsRivers);
    GetNoiseParams(settings, "mgcarpathian_np_mntVar", noiseParamsMntVar);
    GetNoiseParams(settings, "mgcarpathian_np_cave1", noiseParamsCave1);
    GetNoiseParams(settings, "mgcarpathian_np_cave2", noiseParamsCave2);
    GetNoiseParams(settings, "mgcarpathian_np_cavern", noiseParamsCavern);
    GetNoiseParams(settings, "mgcarpathian_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorCarpathianParams::WriteParams(Settings* settings)
{
	settings->SetFlagString("mgcarpathian_mSPFlags", spFlags, FlagdescMapGeneratorCarpathian, spFlags);

	settings->SetFloat("mgcarpathian_base_level",   baseLevel);
	settings->SetFloat("mgcarpathian_river_width",  riverWidth);
	settings->SetFloat("mgcarpathian_river_depth",  riverDepth);
	settings->SetFloat("mgcarpathian_valley_width", valleyWidth);

	settings->SetFloat("mgcarpathian_cave_width", caveWidth);
	settings->SetInt("mgcarpathian_large_cave_depth", largeCaveDepth);
	settings->SetUInt16("mgcarpathian_small_cave_num_min", smallCaveNumMin);
	settings->SetUInt16("mgcarpathian_small_cave_num_max", smallCaveNumMax);
	settings->SetUInt16("mgcarpathian_large_cave_num_min", largeCaveNumMin);
	settings->SetUInt16("mgcarpathian_large_cave_num_max", largeCaveNumMax);
	settings->SetFloat("mgcarpathian_large_cave_flooded", largeCaveFlooded);
	settings->SetInt16("mgcarpathian_cavern_limit", cavernLimit);
	settings->SetInt16("mgcarpathian_cavern_taper", cavernTaper);
	settings->SetFloat("mgcarpathian_cavern_threshold", cavernThreshold);
	settings->SetInt16("mgcarpathian_dungeon_ymin", dungeonYmin);
	settings->SetInt16("mgcarpathian_dungeon_ymax", dungeonYmax);

    SetNoiseParams(settings, "mgcarpathian_np_filler_depth", noiseParamsFillerDepth);
    SetNoiseParams(settings, "mgcarpathian_np_height1", noiseParamsHeight1);
    SetNoiseParams(settings, "mgcarpathian_np_height2", noiseParamsHeight2);
    SetNoiseParams(settings, "mgcarpathian_np_height3", noiseParamsHeight3);
    SetNoiseParams(settings, "mgcarpathian_np_height4", noiseParamsHeight4);
    SetNoiseParams(settings, "mgcarpathian_np_hills_terrain", noiseParamsHillsTerrain);
    SetNoiseParams(settings, "mgcarpathian_np_ridge_terrain", noiseParamsRidgeTerrain);
    SetNoiseParams(settings, "mgcarpathian_np_step_terrain", noiseParamsStepTerrain);
    SetNoiseParams(settings, "mgcarpathian_np_hills", noiseParamsHills);
    SetNoiseParams(settings, "mgcarpathian_np_ridgeMnt", noiseParamsRidgeMnt);
    SetNoiseParams(settings, "mgcarpathian_np_stepMnt", noiseParamsStepMnt);
    SetNoiseParams(settings, "mgcarpathian_np_rivers", noiseParamsRivers);
    SetNoiseParams(settings, "mgcarpathian_np_mntVar", noiseParamsMntVar);
    SetNoiseParams(settings, "mgcarpathian_np_cave1", noiseParamsCave1);
    SetNoiseParams(settings, "mgcarpathian_np_cave2", noiseParamsCave2);
    SetNoiseParams(settings, "mgcarpathian_np_cavern", noiseParamsCavern);
    SetNoiseParams(settings, "mgcarpathian_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorCarpathianParams::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgcarpathian_mSPFlags", FlagdescMapGeneratorCarpathian, MGCARPATHIAN_CAVERNS);
}

////////////////////////////////////////////////////////////////////////////////


// Lerp function
inline float MapGeneratorCarpathian::GetLerp(float noise1, float noise2, float mod)
{
	return noise1 + mod * (noise2 - noise1);
}

// Steps function
float MapGeneratorCarpathian::GetSteps(float noise)
{
	float w = 0.5f;
	float k = std::floor(noise / w);
	float f = (noise - k * w) / w;
	float s = std::fmin(2.f * f, 1.f);
	return (k + s) * w;
}


////////////////////////////////////////////////////////////////////////////////


void MapGeneratorCarpathian::MakeChunk(BlockMakeData *data)
{
	// Pre-conditions
	LogAssert(data->vmanip, "invalid vmanip");
	LogAssert(data->nodeMgr, "invalid node manager");

	this->mGenerating = true;
	this->mMMVManip = data->vmanip;
	this->mNodeMgr = data->nodeMgr;

	Vector3<short> blockPosMin = data->blockPosMin;
	Vector3<short> blockPosMax = data->blockPosMax;
	mNodeMin = blockPosMin * (short)MAP_BLOCKSIZE;
    mNodeMax = (blockPosMax + Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};
    mFullNodeMin = (blockPosMin - Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE;
    mFullNodeMax = (blockPosMax + Vector3<short>{2, 2, 2}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};

	// Create a block-specific mSeed
	mBlockSeed = GetBlockSeed2(mFullNodeMin, mSeed);

	// Generate terrain
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
		if (mSPFlags & MGCARPATHIAN_CAVERNS)
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
	if (mFlags & MG_LIGHT) 
    {
        CalculateLighting(mNodeMin - Vector3<short>{0, 1, 0}, 
            mNodeMax + Vector3<short>{0, 1, 0}, mFullNodeMin, mFullNodeMax);
	}

	this->mGenerating = false;
}


////////////////////////////////////////////////////////////////////////////////


int MapGeneratorCarpathian::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	// If rivers are enabled, first check if in a river channel
	if (mSPFlags & MGCARPATHIAN_RIVERS) 
    {
		float river = std::fabs(NoisePerlin2D(&mNoiseRivers->mNoiseParams, pos[0], pos[1], mSeed)) - mRiverWidth;
		if (river < 0.0f)
			return MAX_MAP_GENERATION_LIMIT; // Unsuitable spawn point
	}

	float height1 = NoisePerlin2D(&mNoiseHeight1->mNoiseParams, pos[0], pos[1], mSeed);
	float height2 = NoisePerlin2D(&mNoiseHeight2->mNoiseParams, pos[0], pos[1], mSeed);
	float height3 = NoisePerlin2D(&mNoiseHeight3->mNoiseParams, pos[0], pos[1], mSeed);
	float height4 = NoisePerlin2D(&mNoiseHeight4->mNoiseParams, pos[0], pos[1], mSeed);

	float hterabs = std::fabs(NoisePerlin2D(&mNoiseHillsTerrain->mNoiseParams, pos[0], pos[1], mSeed));
	float noiseHills = NoisePerlin2D(&mNoiseHills->mNoiseParams, pos[0], pos[1], mSeed);
	float hillMnt = hterabs * hterabs * hterabs * noiseHills * noiseHills;

	float rterabs = std::fabs(NoisePerlin2D(&mNoiseRidgeTerrain->mNoiseParams, pos[0], pos[1], mSeed));
	float noiseRidgeMnt = NoisePerlin2D(&mNoiseRidgeMnt->mNoiseParams, pos[0], pos[1], mSeed);
	float ridgeMnt = rterabs * rterabs * rterabs * (1.0f - std::fabs(noiseRidgeMnt));

	float sterabs = std::fabs(NoisePerlin2D(&mNoiseStepTerrain->mNoiseParams, pos[0], pos[1], mSeed));
	float noiseStepMnt = NoisePerlin2D(&mNoiseStepMnt->mNoiseParams, pos[0], pos[1], mSeed);
	float stepMnt = sterabs * sterabs * sterabs * GetSteps(noiseStepMnt);

	float valley = 1.0f;
	float river = 0.0f;

	if ((mSPFlags & MGCARPATHIAN_RIVERS) && mNodeMax[1] >= mWaterLevel - 16) 
    {
		river = std::fabs(NoisePerlin2D(&mNoiseRivers->mNoiseParams, pos[0], pos[1], mSeed)) - mRiverWidth;
		if (river <= mValleyWidth) 
        {
			// Within river valley
			if (river < 0.0f) 
            {
				// River channel
				valley = river;
			} 
            else 
            {
				// Valley slopes.
				// 0 at river edge, 1 at valley edge.
				float riversc = river / mValleyWidth;
				// Smoothstep
				valley = riversc * riversc * (3.0f - 2.0f * riversc);
			}
		}
	}

	bool solidBelow = false;
	unsigned char consNonSolid = 0; // consecutive non-solid nodes

	for (int16_t y = mWaterLevel; y <= mWaterLevel + 32; y++) 
    {
		float mntVar = NoisePerlin3D(&mNoiseMntVar->mNoiseParams, pos[0], y, pos[1], mSeed);
		float hill1 = GetLerp(height1, height2, mntVar);
		float hill2 = GetLerp(height3, height4, mntVar);
		float hill3 = GetLerp(height3, height2, mntVar);
		float hill4 = GetLerp(height1, height4, mntVar);

		float hilliness = std::fmax(std::fmin(hill1, hill2), std::fmin(hill3, hill4));
		float hills = hillMnt * hilliness;
		float ridgedMountains = ridgeMnt * hilliness;
		float stepMountains = stepMnt * hilliness;

		int grad = 1 - y;

		float mountains = hills + ridgedMountains + stepMountains;
		float surfaceLevel = mBaseLevel + mountains + grad;

		if ((mSPFlags & MGCARPATHIAN_RIVERS) && river <= mValleyWidth) 
        {
			if (valley < 0.0f) 
            {
				// River channel
				surfaceLevel = std::fmin(surfaceLevel, mWaterLevel - std::sqrt(-valley) * mRiverDepth);
			} 
            else if (surfaceLevel > mWaterLevel)
            {
				// Valley slopes
				surfaceLevel = mWaterLevel + (surfaceLevel - mWaterLevel) * valley;
			}
		}

		if (y < surfaceLevel) 
        { //TODO '<=' fix from GenerateTerrain()
			// solid node
			solidBelow = true;
			consNonSolid = 0;
		} 
        else 
        {
			// non-solid node
			consNonSolid++;
			if (consNonSolid == 3 && solidBelow)
				return y - 1;
		}
	}

	return MAX_MAP_GENERATION_LIMIT; // No suitable spawn point found
}


////////////////////////////////////////////////////////////////////////////////


int MapGeneratorCarpathian::GenerateTerrain()
{
	MapNode mnodeAir(CONTENT_AIR);
	MapNode mnStone(mContentStone);
	MapNode mnWater(mContentWaterSource);

	// Calculate noise for terrain generation
	mNoiseHeight1->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseHeight2->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseHeight3->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseHeight4->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseHillsTerrain->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseRidgeTerrain->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseStepTerrain->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseHills->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseRidgeMnt->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseStepMnt->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseMntVar->PerlinMap3D(mNodeMin[0], (float)(mNodeMin[1] - 1), mNodeMin[2]);

	if (mSPFlags & MGCARPATHIAN_RIVERS)
		mNoiseRivers->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
    
	//// Place nodes
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	int16_t stoneSurfaceMaxY = -MAX_MAP_GENERATION_LIMIT;
	unsigned int index2d = 0;

    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, index2d++)
        {
            // Hill/Mountain height (hilliness)
            float height1 = mNoiseHeight1->mResult[index2d];
            float height2 = mNoiseHeight2->mResult[index2d];
            float height3 = mNoiseHeight3->mResult[index2d];
            float height4 = mNoiseHeight4->mResult[index2d];

            // Rolling hills
            float hterabs = std::fabs(mNoiseHillsTerrain->mResult[index2d]);
            float noiseHills = mNoiseHills->mResult[index2d];
            float hillMnt = hterabs * hterabs * hterabs * noiseHills * noiseHills;

            // Ridged mountains
            float rterabs = std::fabs(mNoiseRidgeTerrain->mResult[index2d]);
            float noiseRidgeMnt = mNoiseRidgeMnt->mResult[index2d];
            float ridgeMnt = rterabs * rterabs * rterabs *
                (1.0f - std::fabs(noiseRidgeMnt));

            // Step (terraced) mountains
            float sterabs = std::fabs(mNoiseStepTerrain->mResult[index2d]);
            float noiseStepMnt = mNoiseStepMnt->mResult[index2d];
            float stepMnt = sterabs * sterabs * sterabs * GetSteps(noiseStepMnt);

            // Rivers
            float valley = 1.0f;
            float river = 0.0f;

            if ((mSPFlags & MGCARPATHIAN_RIVERS) && mNodeMax[1] >= mWaterLevel - 16) 
            {
                river = std::fabs(mNoiseRivers->mResult[index2d]) - mRiverWidth;
                if (river <= mValleyWidth)
                {
                    // Within river valley
                    if (river < 0.0f) 
                    {
                        // River channel
                        valley = river;
                    }
                    else
                    {
                        // Valley slopes.
                        // 0 at river edge, 1 at valley edge.
                        float riversc = river / mValleyWidth;
                        // Smoothstep
                        valley = riversc * riversc * (3.0f - 2.0f * riversc);
                    }
                }
            }

            // Initialise 3D noise index and voxelmanip index to column base
            unsigned int index3d = (z - mNodeMin[2]) * mZStride1u1d + (x - mNodeMin[0]);
            unsigned int vi = mMMVManip->mArea.Index(x, mNodeMin[1] - 1, z);
            
            for (int16_t y = mNodeMin[1] - 1; y <= mNodeMax[1] + 1; y++, index3d += mYStride, VoxelArea::AddY(em, vi, 1)) 
            {
                if (mMMVManip->mData[vi].GetContent() != CONTENT_IGNORE)
                    continue;

                // Combine height noises and apply 3D variation
                float mntVar = mNoiseMntVar->mResult[index3d];
                float hill1 = GetLerp(height1, height2, mntVar);
                float hill2 = GetLerp(height3, height4, mntVar);
                float hill3 = GetLerp(height3, height2, mntVar);
                float hill4 = GetLerp(height1, height4, mntVar);

                // 'hilliness' determines whether hills/mountains are
                // small or large
                float hilliness = std::fmax(std::fmin(hill1, hill2), std::fmin(hill3, hill4));
                float hills = hillMnt * hilliness;
                float ridgedMountains = ridgeMnt * hilliness;
                float stepMountains = stepMnt * hilliness;

                // Gradient & shallow seabed
                int grad = (y < mWaterLevel) ? mGradWL + (mWaterLevel - y) * 3 : 1 - y;

                // Final terrain level
                float mountains = hills + ridgedMountains + stepMountains;
                float surfaceLevel = mBaseLevel + mountains + grad;

                // Rivers
                if ((mSPFlags & MGCARPATHIAN_RIVERS) && mNodeMax[1] >= mWaterLevel - 16 && river <= mValleyWidth) 
                {
                    if (valley < 0.0f) 
                    {
                        // River channel
                        surfaceLevel = std::fmin(surfaceLevel,
                            mWaterLevel - std::sqrt(-valley) * mRiverDepth);
                    }
                    else if (surfaceLevel > mWaterLevel) 
                    {
                        // Valley slopes
                        surfaceLevel = mWaterLevel + (surfaceLevel - mWaterLevel) * valley;
                    }
                }

                if (y < surfaceLevel) 
                { //TODO '<='
                    mMMVManip->mData[vi] = mnStone; // Stone
                    if (y > stoneSurfaceMaxY)
                        stoneSurfaceMaxY = y;
                }
                else if (y <= mWaterLevel) 
                {
                    mMMVManip->mData[vi] = mnWater; // Sea water
                }
                else 
                {
                    mMMVManip->mData[vi] = mnodeAir; // Air
                }
            }
        }
    }

	return stoneSurfaceMaxY;
}
