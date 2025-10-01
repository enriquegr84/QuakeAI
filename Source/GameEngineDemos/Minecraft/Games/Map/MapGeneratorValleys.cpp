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

#include "MapGenerator.h"
#include "DungeonGenerator.h"
#include "CaveGenerator.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorOre.h"
#include "MapGeneratorDecoration.h"
#include "MapGeneratorValleys.h"
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


FlagDescription FlagdescMapGeneratorValleys[] = 
{
	{"altitudeChill",   MGVALLEYS_ALT_CHILL},
	{"humid_rivers",     MGVALLEYS_HUMID_RIVERS},
	{"vary_riverDepth", MGVALLEYS_VARY_RIVER_DEPTH},
	{"altitude_dry",     MGVALLEYS_ALT_DRY},
	{NULL,               0}
};


MapGeneratorValleys::MapGeneratorValleys(MapGeneratorValleysParams* params, EmergeParams* emerge)
	: MapGeneratorBasic(MAPGEN_VALLEYS, params, emerge)
{
	LogAssert(mBiomeGenerator->GetType() != BIOMEGEN_ORIGINAL,
		"MapGeneratorValleys has a hard dependency on BiomeGeneratorOriginal");
	mBiomeGen = (BiomeGeneratorOriginal *)mBiomeGenerator;

	mSPFlags = params->spFlags;
	mAltitudeChill = params->altitudeChill;
	mRiverDepthBed = params->riverDepth + 1.0f;
	mRiverSizeFactor = params->riverSize / 100.0f;

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

	//// 2D Terrain noise
	mNoiseFillerDepth = new Noise(&params->noiseParamsFillerDepth, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseInterValleySlope = new Noise(&params->noiseParamsInterValleySlope, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseRivers = new Noise(&params->noiseParamsRivers, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseTerrainHeight = new Noise(&params->noiseParamsTerrainHeight, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseValleyDepth = new Noise(&params->noiseParamsValleyDepth, mSeed, mChunkSize[0], mChunkSize[2]);
	mNoiseValleyProfile = new Noise(&params->noiseParamsValleyProfile, mSeed, mChunkSize[0], mChunkSize[2]);

	//// 3D Terrain noise
	// 1-up 1-down overgeneration
	mNoiseInterValleyFill = new Noise(&params->noiseParamsInterValleyFill,
		mSeed, mChunkSize[0], mChunkSize[1] + 2, mChunkSize[2]);
	// 1-down overgeneraion
	MapGeneratorBasic::mNoiseParamsCave1 = params->noiseParamsCave1;
	MapGeneratorBasic::mNoiseParamsCave2 = params->noiseParamsCave2;
	MapGeneratorBasic::mNoiseParamsCavern = params->noiseParamsCavern;
	MapGeneratorBasic::mNoiseParamsDungeons = params->noiseParamsDungeons;
}


MapGeneratorValleys::~MapGeneratorValleys()
{
	delete mNoiseFillerDepth;
	delete mNoiseInterValleyFill;
	delete mNoiseInterValleySlope;
	delete mNoiseRivers;
	delete mNoiseTerrainHeight;
	delete mNoiseValleyDepth;
	delete mNoiseValleyProfile;
}


MapGeneratorValleysParams::MapGeneratorValleysParams():
    noiseParamsFillerDepth(0.f, 1.2f, Vector3<float>{256, 256, 256}, 1605, 3, 0.5f, 2.f),
    noiseParamsInterValleyFill(0.f, 1.f, Vector3<float>{256, 512, 256}, 1993, 6, 0.8f, 2.f),
    noiseParamsInterValleySlope(0.5f, 0.5f, Vector3<float>{128, 128, 128}, 746, 1, 1.f, 2.f),
    noiseParamsRivers(0.f, 1.f, Vector3<float>{256, 256, 256}, -6050, 5, 0.6f, 2.f),
    noiseParamsTerrainHeight(-10.f, 50.f, Vector3<float>{1024, 1024, 1024}, 5202, 6, 0.4f, 2.f),
    noiseParamsValleyDepth(5.f, 4.f, Vector3<float>{512, 512, 512}, -1914, 1, 1.f, 2.f),
    noiseParamsValleyProfile(0.6f, 0.5f, Vector3<float>{512, 512, 512}, 777, 1, 1.f, 2.f),
    noiseParamsCave1(0.f, 12.f, Vector3<float>{61, 61, 61}, 52534, 3, 0.5f, 2.f),
    noiseParamsCave2(0.f, 12.f, Vector3<float>{67, 67, 67}, 10325, 3, 0.5f, 2.f),
    noiseParamsCavern(0.f, 1.f, Vector3<float>{768, 256, 768}, 59033, 6, 0.63f, 2.f),
    noiseParamsDungeons(0.9f, 0.5f, Vector3<float>{500, 500, 500}, 0, 2, 0.8f, 2.f)
{
}


void MapGeneratorValleysParams::ReadParams(const Settings* settings)
{
    try {
        settings->GetFlagString("mgvalleys_spflags", FlagdescMapGeneratorValleys, &spFlags);
        altitudeChill = settings->GetUInt16("mgvalleys_altitude_chill");
        largeCaveDepth = settings->GetInt16("mgvalleys_large_cave_depth");
        smallCaveNumMin = settings->GetUInt16("mgvalleys_small_cave_num_min");
        smallCaveNumMax = settings->GetUInt16("mgvalleys_small_cave_num_max");
        largeCaveNumMin = settings->GetUInt16("mgvalleys_large_cave_num_min");
        largeCaveNumMax = settings->GetUInt16("mgvalleys_large_cave_num_max");
        largeCaveFlooded = settings->GetFloat("mgvalleys_large_cave_flooded");
        riverDepth = settings->GetUInt16("mgvalleys_river_depth");
        riverSize = settings->GetUInt16("mgvalleys_river_size");
        caveWidth = settings->GetFloat("mgvalleys_cave_width");
        cavernLimit = settings->GetInt16("mgvalleys_cavern_limit");
        cavernTaper = settings->GetInt16("mgvalleys_cavern_taper");
        cavernThreshold = settings->GetFloat("mgvalleys_cavern_threshold");
        dungeonYmin = settings->GetInt16("mgvalleys_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgvalleys_dungeon_ymax");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgvalleys_np_filler_depth", noiseParamsFillerDepth);
    GetNoiseParams(settings, "mgvalleys_np_inter_valley_fill", noiseParamsInterValleyFill);
    GetNoiseParams(settings, "mgvalleys_np_inter_valley_slope", noiseParamsInterValleySlope);
    GetNoiseParams(settings, "mgvalleys_np_rivers", noiseParamsRivers);
    GetNoiseParams(settings, "mgvalleys_np_terrain_height", noiseParamsTerrainHeight);
    GetNoiseParams(settings, "mgvalleys_np_valleyDistepth", noiseParamsValleyDepth);
    GetNoiseParams(settings, "mgvalleys_np_valley_profile", noiseParamsValleyProfile);

    GetNoiseParams(settings, "mgvalleys_np_cave1", noiseParamsCave1);
    GetNoiseParams(settings, "mgvalleys_np_cave2", noiseParamsCave2);
    GetNoiseParams(settings, "mgvalleys_np_cavern", noiseParamsCavern);
    GetNoiseParams(settings, "mgvalleys_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorValleysParams::WriteParams(Settings* settings)
{
	settings->SetFlagString("mgvalleys_spflags", spFlags, FlagdescMapGeneratorValleys, spFlags);
	settings->SetUInt16("mgvalleys_altitude_chill", altitudeChill);
	settings->SetInt16("mgvalleys_large_cave_depth", largeCaveDepth);
	settings->SetUInt16("mgvalleys_small_cave_num_min", smallCaveNumMin);
	settings->SetUInt16("mgvalleys_small_cave_num_max", smallCaveNumMax);
	settings->SetUInt16("mgvalleys_large_cave_num_min", largeCaveNumMin);
	settings->SetUInt16("mgvalleys_large_cave_num_max", largeCaveNumMax);
	settings->SetFloat("mgvalleys_large_cave_flooded", largeCaveFlooded);
	settings->SetUInt16("mgvalleys_river_depth", riverDepth);
	settings->SetUInt16("mgvalleys_river_size", riverSize);
	settings->SetFloat("mgvalleys_cave_width", caveWidth);
	settings->SetUInt16("mgvalleys_cavern_limit", cavernLimit);
	settings->SetUInt16("mgvalleys_cavern_taper", cavernTaper);
	settings->SetFloat("mgvalleys_cavern_threshold", cavernThreshold);
	settings->SetUInt16("mgvalleys_dungeon_ymin", dungeonYmin);
	settings->SetUInt16("mgvalleys_dungeon_ymax", dungeonYmax);

    SetNoiseParams(settings, "mgvalleys_np_filler_depth", noiseParamsFillerDepth);
    SetNoiseParams(settings, "mgvalleys_np_inter_valley_fill", noiseParamsInterValleyFill);
    SetNoiseParams(settings, "mgvalleys_np_inter_valley_slope", noiseParamsInterValleySlope);
    SetNoiseParams(settings, "mgvalleys_np_rivers", noiseParamsRivers);
    SetNoiseParams(settings, "mgvalleys_np_terrain_height", noiseParamsTerrainHeight);
    SetNoiseParams(settings, "mgvalleys_np_valleyDistepth", noiseParamsValleyDepth);
    SetNoiseParams(settings, "mgvalleys_np_valley_profile", noiseParamsValleyProfile);

    SetNoiseParams(settings, "mgvalleys_np_cave1", noiseParamsCave1);
    SetNoiseParams(settings, "mgvalleys_np_cave2", noiseParamsCave2);
    SetNoiseParams(settings, "mgvalleys_np_cavern", noiseParamsCavern);
    SetNoiseParams(settings, "mgvalleys_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorValleysParams::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgvalleys_spflags", FlagdescMapGeneratorValleys,
		MGVALLEYS_ALT_CHILL | MGVALLEYS_HUMID_RIVERS |
		MGVALLEYS_VARY_RIVER_DEPTH | MGVALLEYS_ALT_DRY);
}


/////////////////////////////////////////////////////////////////


void MapGeneratorValleys::MakeChunk(BlockMakeData *data)
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

	// Generate biome noises. Note this must be executed strictly before
	// GenerateTerrain, because GenerateTerrain depends on intermediate
	// biome-related noises.
	mBiomeGen->CalculateBiomeNoise(mNodeMin);

	// Generate terrain
	int16_t stoneSurfaceMaxY = GenerateTerrain();

	// Create heightmap
	UpdateHeightmap(mNodeMin, mNodeMax);

	// Place biome-specific nodes and build mBiomeMap
	if (mFlags & MG_BIOMES)
		GenerateBiomes();

	// Generate tunnels, caverns and large randomwalk caves
	if (mFlags & MG_CAVES) 
    {
		// Generate tunnels first as caverns confuse them
		GenerateCavesNoiseIntersection(stoneSurfaceMaxY);

		// Generate caverns
		bool nearCavern = GenerateCavernsNoise(stoneSurfaceMaxY);

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

	// Dungeon creation
	if (mFlags & MG_DUNGEONS)
		GenerateDungeons(stoneSurfaceMaxY);

	// Generate the registered decorations
	if (mFlags & MG_DECORATIONS)
		mEmerge->mDecoMgr->PlaceAllDecos(this, mBlockSeed, mNodeMin, mNodeMax);

	// Sprinkle some dust on top after everything else was generated
	if (mFlags & MG_BIOMES)
		DustTopNodes();

	UpdateLiquid(&data->transformingLiquid, mFullNodeMin, mFullNodeMax);

	if (mFlags & MG_LIGHT)
        CalculateLighting(mNodeMin - Vector3<short>{0, 1, 0}, mNodeMax + Vector3<short>{0, 1, 0},
			mFullNodeMin, mFullNodeMax);

	this->mGenerating = false;

	//printf("MakeChunk: %lums\n", t.stop());
}


int MapGeneratorValleys::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	// Check if in a river channel
	float noiseRivers = NoisePerlin2D(&mNoiseRivers->mNoiseParams, pos[0], pos[1], mSeed);
    if (std::fabs(noiseRivers) <= mRiverSizeFactor)
    {
        // Unsuitable spawn point
        return MAX_MAP_GENERATION_LIMIT;
    }

	float noiseSlope = NoisePerlin2D(&mNoiseInterValleySlope->mNoiseParams, pos[0], pos[1], mSeed);
	float noiseTerrainHeight = NoisePerlin2D(&mNoiseTerrainHeight->mNoiseParams, pos[0], pos[1], mSeed);
	float noiseValley = NoisePerlin2D(&mNoiseValleyDepth->mNoiseParams, pos[0], pos[1], mSeed);
	float noiseValleyProfile = NoisePerlin2D(&mNoiseValleyProfile->mNoiseParams, pos[0], pos[1], mSeed);

	float valleyDist = noiseValley * noiseValley;
	float base = noiseTerrainHeight + valleyDist;
	float river = std::fabs(noiseRivers) - mRiverSizeFactor;
	float tv = std::fmax(river / noiseValleyProfile, 0.0f);
	float valleyHeight = valleyDist * (1.0f - std::exp(-tv * tv));
	float surfaceY = base + valleyHeight;
	float slope = noiseSlope * valleyHeight;
	float riverY = base - 1.0f;

	// Raising the maximum spawn level above 'mWaterLevel + 16' is necessary for custom
	// parameters that set average terrain level much higher than mWaterLevel.
	int16_t maxSpawnY = (int16_t)std::fmax(mNoiseTerrainHeight->mNoiseParams.offset +
		mNoiseValleyDepth->mNoiseParams.offset * mNoiseValleyDepth->mNoiseParams.offset, mWaterLevel + 16);

	// Starting spawn search at maxSpawnY + 128 ensures 128 nodes of open
	// space above spawn position. Avoids spawning in possibly sealed voids.
	for (int16_t y = maxSpawnY + 128; y >= mWaterLevel; y--) 
    {
		float noiseFill = NoisePerlin3D(&mNoiseInterValleyFill->mNoiseParams, pos[0], y, pos[1], mSeed);
		float surfaceDelta = (float)y - surfaceY;
		float density = slope * noiseFill - surfaceDelta;

		if (density > 0.0f) 
        {  // If solid
			// Sometimes surface level is below river water level in places that are not
			// river channels.
			if (y < mWaterLevel || y > maxSpawnY || y < (int16_t)riverY)
				// Unsuitable spawn point
				return MAX_MAP_GENERATION_LIMIT;

			// y + 2 because y is surface and due to biome 'dust' nodes.
			return y + 2;
		}
	}
	// Unsuitable spawn position, no ground found
	return MAX_MAP_GENERATION_LIMIT;
}


int MapGeneratorValleys::GenerateTerrain()
{
	MapNode nodeAir(CONTENT_AIR);
	MapNode nodeRiverWater(mContentRiverWaterSource);
	MapNode nodeStone(mContentStone);
	MapNode nodeWater(mContentWaterSource);

	mNoiseInterValleySlope->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseRivers->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseTerrainHeight->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseValleyDepth->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseValleyProfile->PerlinMap2D(mNodeMin[0], mNodeMin[2]);
	mNoiseInterValleyFill->PerlinMap3D(mNodeMin[0], (float)(mNodeMin[1] - 1), mNodeMin[2]);

	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	int16_t surfaceMaxY = -MAX_MAP_GENERATION_LIMIT;
	unsigned int index2d = 0;

    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, index2d++)
        {
            float noiseSlope = mNoiseInterValleySlope->mResult[index2d];
            float noiseRivers = mNoiseRivers->mResult[index2d];
            float noiseTerrainHeight = mNoiseTerrainHeight->mResult[index2d];
            float noiseValley = mNoiseValleyDepth->mResult[index2d];
            float noiseValleyProfile = mNoiseValleyProfile->mResult[index2d];

            float valleyDist = noiseValley * noiseValley;
            // 'base' represents the level of the river banks
            float base = noiseTerrainHeight + valleyDist;
            // 'river' represents the distance from the river edge
            float river = std::fabs(noiseRivers) - mRiverSizeFactor;
            // Use the curve of the function 1-exp(-(x/a)^2) to model valleys.
            // 'valleyHeight' represents the height of the terrain, from the rivers.
            float tv = std::fmax(river / noiseValleyProfile, 0.0f);
            float valleyHeight = valleyDist * (1.0f - std::exp(-tv * tv));
            // Approximate height of the terrain
            float surfaceY = base + valleyHeight;
            float slope = noiseSlope * valleyHeight;
            // River water surface is 1 node below river banks
            float riverY = base - 1.0f;

            // Rivers are placed where 'river' is negative
            if (river < 0.0f) 
            {
                // Use the function -sqrt(1-x^2) which models a circle
                float tr = river / mRiverSizeFactor + 1.0f;
                float depth = (mRiverDepthBed * std::sqrt(std::fmax(0.0f, 1.0f - tr * tr)));
                // There is no logical equivalent to this using rangelim
                surfaceY = std::fmin(std::fmax(base - depth, (float)(mWaterLevel - 3)), surfaceY);
                slope = 0.0f;
            }

            // Optionally vary river depth according to heat and humidity
            if (mSPFlags & MGVALLEYS_VARY_RIVER_DEPTH) 
            {
                float tempHeat = mBiomeGen->mHeatMap[index2d];
                float heat = (mSPFlags & MGVALLEYS_ALT_CHILL) ?
                    // Match heat value calculated below in
                    // 'Optionally decrease heat with altitude'.
                    // In rivers, 'ground height ignoring riverbeds' is 'base'.
                    // As this only affects river water we can assume y > mWaterLevel.
                    tempHeat + 5.0f - (base - mWaterLevel) * 20.0f / mAltitudeChill : tempHeat;
                float delta = mBiomeGen->mHumIdMap[index2d] - 50.0f;
                if (delta < 0.0f) 
                {
                    float tempEvap = (heat - 32.0f) / 300.0f;
                    riverY += delta * std::fmax(tempEvap, 0.08f);
                }
            }

            // Highest solid node in column
            int16_t columnMaxY = (int16_t)surfaceY;
            unsigned int index3d = (z - mNodeMin[2]) * mZStride1u1d + (x - mNodeMin[0]);
            unsigned int indexData = mMMVManip->mArea.Index(x, mNodeMin[1] - 1, z);

            for (int16_t y = mNodeMin[1] - 1; y <= mNodeMax[1] + 1; y++) 
            {
                if (mMMVManip->mData[indexData].GetContent() == CONTENT_IGNORE) 
                {
                    float noiseFill = mNoiseInterValleyFill->mResult[index3d];
                    float surfaceDelta = (float)y - surfaceY;
                    // Density = density noise + density gradient
                    float density = slope * noiseFill - surfaceDelta;

                    if (density > 0.0f) 
                    {
                        mMMVManip->mData[indexData] = nodeStone; // Stone
                        if (y > surfaceMaxY)
                            surfaceMaxY = y;
                        if (y > columnMaxY)
                            columnMaxY = y;
                    }
                    else if (y <= mWaterLevel) 
                    {
                        mMMVManip->mData[indexData] = nodeWater; // Water
                    }
                    else if (y <= (int16_t)riverY) 
                    {
                        mMMVManip->mData[indexData] = nodeRiverWater; // River water
                    }
                    else 
                    {
                        mMMVManip->mData[indexData] = nodeAir; // Air
                    }
                }

                VoxelArea::AddY(em, indexData, 1);
                index3d += mYStride;
            }

            // Optionally increase humidity around rivers
            if (mSPFlags & MGVALLEYS_HUMID_RIVERS) 
            {
                // Compensate to avoid increasing average humidity
                mBiomeGen->mHumIdMap[index2d] *= 0.8f;
                // Ground height ignoring riverbeds
                float tempAlt = std::fmax(base, (float)columnMaxY);
                float waterDepth = (tempAlt - base) / 4.0f;
                mBiomeGen->mHumIdMap[index2d] *=
                    1.0f + std::pow(0.5f, std::fmax(waterDepth, 1.0f));
            }

            // Optionally decrease humidity with altitude
            if (mSPFlags & MGVALLEYS_ALT_DRY) 
            {
                // Ground height ignoring riverbeds
                float tempAlt = std::fmax(base, (float)columnMaxY);
                // Only decrease above mWaterLevel
                if (tempAlt > mWaterLevel)
                    mBiomeGen->mHumIdMap[index2d] -=
                    (tempAlt - mWaterLevel) * 10.0f / mAltitudeChill;
            }

            // Optionally decrease heat with altitude
            if (mSPFlags & MGVALLEYS_ALT_CHILL) 
            {
                // Compensate to avoid reducing the average heat
                mBiomeGen->mHeatMap[index2d] += 5.0f;
                // Ground height ignoring riverbeds
                float tempAlt = std::fmax(base, (float)columnMaxY);
                // Only decrease above mWaterLevel
                if (tempAlt > mWaterLevel)
                    mBiomeGen->mHeatMap[index2d] -=
                    (tempAlt - mWaterLevel) * 20.0f / mAltitudeChill;
            }
        }
    }

	return surfaceMaxY;
}
