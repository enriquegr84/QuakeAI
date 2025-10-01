/*
Minetest
Copyright (C) 2015-2019 paramat
Copyright (C) 2015-2016 kwolekr, Ryan Kwolek

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
#include "MapGeneratorFractal.h"
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


FlagDescription FlagdescMapGeneratorFractal[] = 
{
	{"terrain", MGFRACTAL_TERRAIN},
	{NULL,      0}
};

///////////////////////////////////////////////////////////////////////////////////////


MapGeneratorFractal::MapGeneratorFractal(MapGeneratorFractalParams* params, EmergeParams* emerge)
	: MapGeneratorBasic(MAPGEN_FRACTAL, params, emerge)
{
	mSPFlags = params->spFlags;
	mCaveWidth = params->caveWidth;
	mLargeCaveDepth = params->largeCaveDepth;
	mSmallCaveNumMin = params->smallCaveNumMin;
	mSmallCaveNumMax = params->smallCaveNumMax;
	mLargeCaveNumMin = params->largeCaveNumMin;
	mLargeCaveNumMax = params->largeCaveNumMax;
	mLargeCaveFlooded = params->largeCaveFlooded;
	mDungeonYmin = params->dungeonYmin;
	mDungeonYmax = params->dungeonYmax;
	mFractal = params->fractal;
	mIterations = params->iterations;
	mScale = params->scale;
	mOffset = params->offset;
	mSliceW = params->sliceW;
	mJuliaX = params->juliaX;
	mJuliaY = params->juliaY;
	mJuliaZ = params->juliaZ;
	mJuliaW = params->juliaW;

	//// 2D noise
	if (mSPFlags & MGFRACTAL_TERRAIN)
		mNoiseSeabed = new Noise(&params->noiseParamsSeabed, mSeed, mChunkSize[0], mChunkSize[2]);

	mNoiseFillerDepth = new Noise(&params->noiseParamsFillerDepth, mSeed, mChunkSize[0], mChunkSize[2]);

	//// 3D noise
	MapGeneratorBasic::mNoiseParamsDungeons = params->noiseParamsDungeons;
	// Overgeneration to mNodeMin[1] - 1
	MapGeneratorBasic::mNoiseParamsCave1 = params->noiseParamsCave1;
	MapGeneratorBasic::mNoiseParamsCave2 = params->noiseParamsCave2;

	mFormula = mFractal / 2 + mFractal % 2;
	mJulia = mFractal % 2 == 0;
}


MapGeneratorFractal::~MapGeneratorFractal()
{
	delete mNoiseSeabed;
	delete mNoiseFillerDepth;
}


MapGeneratorFractalParams::MapGeneratorFractalParams():
    noiseParamsSeabed(-14, 9, Vector3<float>{600, 600, 600}, 41900, 5, 0.6f, 2.f),
    noiseParamsFillerDepth(0, 1.2f, Vector3<float>{150, 150, 150}, 261, 3, 0.7f, 2.f),
    noiseParamsCave1(0, 12, Vector3<float>{61, 61, 61}, 52534, 3, 0.5f, 2.f),
    noiseParamsCave2(0, 12, Vector3<float>{67, 67, 67}, 10325, 3, 0.5f, 2.f),
    noiseParamsDungeons(0.9f, 0.5f, Vector3<float>{500, 500, 500}, 0, 2, 0.8f, 2.f)
{
}


void MapGeneratorFractalParams::ReadParams(const Settings* settings)
{
    try {
	    settings->GetFlagString("mgfractal_spflags", FlagdescMapGeneratorFractal, nullptr);
        caveWidth = settings->GetFloat("mgfractal_cave_width");
        largeCaveDepth = settings->GetInt16("mgfractal_large_cave_depth");
        smallCaveNumMin = settings->GetUInt16("mgfractal_small_cave_num_min");
        smallCaveNumMax = settings->GetUInt16("mgfractal_small_cave_num_max");
        largeCaveNumMin = settings->GetUInt16("mgfractal_large_cave_num_min");
        largeCaveNumMax = settings->GetUInt16("mgfractal_large_cave_num_max");
        largeCaveFlooded = settings->GetFloat("mgfractal_large_cave_flooded");
        dungeonYmin = settings->GetInt16("mgfractal_dungeon_ymin");
        dungeonYmax = settings->GetInt16("mgfractal_dungeon_ymax");
        fractal = settings->GetUInt16("mgfractal_fractal");
        iterations = settings->GetUInt16("mgfractal_iterations");
        scale = settings->GetVector3("mgfractal_scale");
        offset = settings->GetVector3("mgfractal_offset");
        sliceW = settings->GetFloat("mgfractal_slice_w");
        juliaX = settings->GetFloat("mgfractal_julia_x");
        juliaY = settings->GetFloat("mgfractal_julia_y");
        juliaZ = settings->GetFloat("mgfractal_julia_z");
        juliaW = settings->GetFloat("mgfractal_julia_w");
    }
    catch (SettingNotFoundException&) 
    {

    }

    GetNoiseParams(settings, "mgfractal_np_seabed", noiseParamsSeabed);
	GetNoiseParams(settings, "mgfractal_np_filler_depth", noiseParamsFillerDepth);
	GetNoiseParams(settings, "mgfractal_np_cave1", noiseParamsCave1);
	GetNoiseParams(settings, "mgfractal_np_cave2", noiseParamsCave2);
	GetNoiseParams(settings, "mgfractal_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorFractalParams::WriteParams(Settings* settings)
{
	settings->SetFlagString("mgfractal_spflags", spFlags, FlagdescMapGeneratorFractal, spFlags);
	settings->SetFloat("mgfractal_cave_width", caveWidth);
	settings->SetInt16("mgfractal_large_cave_depth", largeCaveDepth);
	settings->SetUInt16("mgfractal_small_cave_num_min", smallCaveNumMin);
	settings->SetUInt16("mgfractal_small_cave_num_max", smallCaveNumMax);
	settings->SetUInt16("mgfractal_large_cave_num_min", largeCaveNumMin);
	settings->SetUInt16("mgfractal_large_cave_num_max", largeCaveNumMax);
	settings->SetFloat("mgfractal_large_cave_flooded", largeCaveFlooded);
	settings->SetInt16("mgfractal_dungeon_ymin", dungeonYmin);
	settings->SetInt16("mgfractal_dungeon_ymax", dungeonYmax);
	settings->SetUInt16("mgfractal_fractal", fractal);
	settings->SetUInt16("mgfractal_iterations", iterations);
	settings->SetVector3("mgfractal_scale", scale);
	settings->SetVector3("mgfractal_offset", offset);
	settings->SetFloat("mgfractal_slice_w", sliceW);
	settings->SetFloat("mgfractal_julia_x", juliaX);
	settings->SetFloat("mgfractal_julia_y", juliaY);
	settings->SetFloat("mgfractal_julia_z", juliaZ);
	settings->SetFloat("mgfractal_julia_w", juliaW);

    SetNoiseParams(settings, "mgfractal_np_seabed", noiseParamsSeabed);
    SetNoiseParams(settings, "mgfractal_np_filler_depth", noiseParamsFillerDepth);
    SetNoiseParams(settings, "mgfractal_np_cave1", noiseParamsCave1);
    SetNoiseParams(settings, "mgfractal_np_cave2", noiseParamsCave2);
    SetNoiseParams(settings, "mgfractal_np_dungeons", noiseParamsDungeons);
}


void MapGeneratorFractalParams::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mgmFractal_mSPFlags", FlagdescMapGeneratorFractal, MGFRACTAL_TERRAIN);
}


/////////////////////////////////////////////////////////////////


int MapGeneratorFractal::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	bool solidBelow = false; // mFractal node is present below to spawn on
	unsigned char airCount = 0; // Consecutive air nodes above a mFractal node
	int16_t searchStart = 0; // No terrain search start

	// If terrain present, don't start search below terrain or water level
	if (mNoiseSeabed) 
    {
		int16_t seabedLevel = (int16_t)NoisePerlin2D(&mNoiseSeabed->mNoiseParams, (float)pos[0], (float)pos[1], mSeed);
		searchStart = std::max(searchStart, std::max(seabedLevel, (short)mWaterLevel));
	}

	for (int16_t y = searchStart; y <= searchStart + 4096; y++) 
    {
		if (GetFractalAtPoint(pos[0], y, pos[1]))
        {
			// mFractal node
			solidBelow = true;
			airCount = 0;
		} 
        else if (solidBelow) 
        {
			// Air above mFractal node
			airCount++;
			// 3 and -2 to account for biome dust nodes
			if (airCount == 3)
				return y - 2;
		}
	}

	return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point
}


void MapGeneratorFractal::MakeChunk(BlockMakeData *data)
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

	// Generate mFractal and optional terrain
	int16_t stoneSurfaceMaxY = GenerateTerrain();

	// Create heightmap
	UpdateHeightmap(mNodeMin, mNodeMax);

	// Init biome generator, place biome-specific nodes, and build mBiomeMap
	if (mFlags & MG_BIOMES) 
    {
		mBiomeGenerator->CalculateBiomeNoise(mNodeMin);
		GenerateBiomes();
	}

	// Generate tunnels and randomwalk caves
	if (mFlags & MG_CAVES) 
    {
		GenerateCavesNoiseIntersection(stoneSurfaceMaxY);
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
	if (mSPFlags & MGFRACTAL_TERRAIN)
		UpdateLiquid(&data->transformingLiquid, mFullNodeMin, mFullNodeMax);

	// Calculate lighting
	if (mFlags & MG_LIGHT)
        CalculateLighting(mNodeMin - Vector3<short>{0, 1, 0}, 
            mNodeMax + Vector3<short>{0, 1, 0}, mFullNodeMin, mFullNodeMax);

	this->mGenerating = false;

	//printf("MakeChunk: %lums\n", t.stop());
}


bool MapGeneratorFractal::GetFractalAtPoint(int16_t x, int16_t y, int16_t z)
{
	float cx, cy, cz, cw, ox, oy, oz, ow;

	if (mJulia) 
    {  // Julia set
		cx = mJuliaX;
		cy = mJuliaY;
		cz = mJuliaZ;
		cw = mJuliaW;
		ox = (float)x / mScale[0] - mOffset[0];
		oy = (float)y / mScale[1] - mOffset[1];
		oz = (float)z / mScale[2] - mOffset[2];
		ow = mSliceW;
	} 
    else 
    {  // Mandelbrot set
		cx = (float)x / mScale[0] - mOffset[0];
		cy = (float)y / mScale[1] - mOffset[1];
		cz = (float)z / mScale[2] - mOffset[2];
		cw = mSliceW;
		ox = 0.0f;
		oy = 0.0f;
		oz = 0.0f;
		ow = 0.0f;
	}

	float nx = 0.0f;
	float ny = 0.0f;
	float nz = 0.0f;
	float nw = 0.0f;

	for (uint16_t iter = 0; iter < mIterations; iter++) 
    {
		switch (mFormula) 
        {
		    default:
		    case 1: // 4D "Roundy"
			    nx = ox * ox - oy * oy - oz * oz - ow * ow + cx;
			    ny = 2.0f * (ox * oy + oz * ow) + cy;
			    nz = 2.0f * (ox * oz + oy * ow) + cz;
			    nw = 2.0f * (ox * ow + oy * oz) + cw;
			    break;
		    case 2: // 4D "Squarry"
			    nx = ox * ox - oy * oy - oz * oz - ow * ow + cx;
			    ny = 2.0f * (ox * oy + oz * ow) + cy;
			    nz = 2.0f * (ox * oz + oy * ow) + cz;
			    nw = 2.0f * (ox * ow - oy * oz) + cw;
			    break;
		    case 3: // 4D "Mandy Cousin"
			    nx = ox * ox - oy * oy - oz * oz + ow * ow + cx;
			    ny = 2.0f * (ox * oy + oz * ow) + cy;
			    nz = 2.0f * (ox * oz + oy * ow) + cz;
			    nw = 2.0f * (ox * ow + oy * oz) + cw;
			    break;
		    case 4: // 4D "Variation"
			    nx = ox * ox - oy * oy - oz * oz - ow * ow + cx;
			    ny = 2.0f * (ox * oy + oz * ow) + cy;
			    nz = 2.0f * (ox * oz - oy * ow) + cz;
			    nw = 2.0f * (ox * ow + oy * oz) + cw;
			    break;
		    case 5: // 3D "Mandelbrot/Mandelbar"
			    nx = ox * ox - oy * oy - oz * oz + cx;
			    ny = 2.0f * ox * oy + cy;
			    nz = -2.0f * ox * oz + cz;
			    break;
		    case 6: // 3D "Christmas Tree"
			    // Altering the formula here is necessary to avoid division by zero
			    if (std::fabs(oz) < 0.000000001f) 
                {
				    nx = ox * ox - oy * oy - oz * oz + cx;
				    ny = 2.0f * oy * ox + cy;
				    nz = 4.0f * oz * ox + cz;
			    } 
                else
                {
				    float a = (2.0f * ox) / (std::sqrt(oy * oy + oz * oz));
				    nx = ox * ox - oy * oy - oz * oz + cx;
				    ny = a * (oy * oy - oz * oz) + cy;
				    nz = a * 2.0f * oy * oz + cz;
			    }
			    break;
		    case 7: // 3D "Mandelbulb"
			    if (std::fabs(oy) < 0.000000001f)
                {
				    nx = ox * ox - oz * oz + cx;
				    ny = cy;
				    nz = -2.0f * oz * std::sqrt(ox * ox) + cz;
			    } 
                else 
                {
				    float a = 1.0f - (oz * oz) / (ox * ox + oy * oy);
				    nx = (ox * ox - oy * oy) * a + cx;
				    ny = 2.0f * ox * oy * a + cy;
				    nz = -2.0f * oz * std::sqrt(ox * ox + oy * oy) + cz;
			    }
			    break;
		    case 8: // 3D "Cosine Mandelbulb"
			    if (std::fabs(oy) < 0.000000001f) 
                {
				    nx = 2.0f * ox * oz + cx;
				    ny = 4.0f * oy * oz + cy;
				    nz = oz * oz - ox * ox - oy * oy + cz;
			    } 
                else 
                {
				    float a = (2.0f * oz) / std::sqrt(ox * ox + oy * oy);
				    nx = (ox * ox - oy * oy) * a + cx;
				    ny = 2.0f * ox * oy * a + cy;
				    nz = oz * oz - ox * ox - oy * oy + cz;
			    }
			    break;
		    case 9: // 4D "Mandelbulb"
			    float rxy = std::sqrt(ox * ox + oy * oy);
			    float rxyz = std::sqrt(ox * ox + oy * oy + oz * oz);
			    if (std::fabs(ow) < 0.000000001f && std::fabs(oz) < 0.000000001f) 
                {
				    nx = (ox * ox - oy * oy) + cx;
				    ny = 2.0f * ox * oy + cy;
				    nz = -2.0f * rxy * oz + cz;
				    nw = 2.0f * rxyz * ow + cw;
			    } 
                else 
                {
				    float a = 1.0f - (ow * ow) / (rxyz * rxyz);
				    float b = a * (1.0f - (oz * oz) / (rxy * rxy));
				    nx = (ox * ox - oy * oy) * b + cx;
				    ny = 2.0f * ox * oy * b + cy;
				    nz = -2.0f * rxy * oz * a + cz;
				    nw = 2.0f * rxyz * ow + cw;
			    }
			    break;
		}

		if (nx * nx + ny * ny + nz * nz + nw * nw > 4.0f)
			return false;

		ox = nx;
		oy = ny;
		oz = nz;
		ow = nw;
	}

	return true;
}


int16_t MapGeneratorFractal::GenerateTerrain()
{
	MapNode nodeAir(CONTENT_AIR);
	MapNode nodeStone(mContentStone);
	MapNode nodeWater(mContentWaterSource);

	int16_t stoneSurfaceMaxY = -MAX_MAP_GENERATION_LIMIT;
	unsigned int index2d = 0;

	if (mNoiseSeabed)
		mNoiseSeabed->PerlinMap2D(mNodeMin[0], mNodeMin[2]);

	for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
		for (int16_t y = mNodeMin[1] - 1; y <= mNodeMax[1] + 1; y++) 
        {
			unsigned int vi = mMMVManip->mArea.Index(mNodeMin[0], y, z);
			for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++, vi++, index2d++) 
            {
				if (mMMVManip->mData[vi].GetContent() != CONTENT_IGNORE)
					continue;

				int16_t seabedheight = -MAX_MAP_GENERATION_LIMIT;
				if (mNoiseSeabed)
					seabedheight = (int16_t)mNoiseSeabed->mResult[index2d];

				if (((mSPFlags & MGFRACTAL_TERRAIN) && y <= seabedheight) ||
						GetFractalAtPoint(x, y, z)) 
                {
                    mMMVManip->mData[vi] = nodeStone;
					if (y > stoneSurfaceMaxY)
						stoneSurfaceMaxY = y;
				} 
                else if ((mSPFlags & MGFRACTAL_TERRAIN) && y <= mWaterLevel) 
                {
                    mMMVManip->mData[vi] = nodeWater;
				} 
                else
                {
                    mMMVManip->mData[vi] = nodeAir;
				}
			}
			index2d -= mYStride;
		}
		index2d += mYStride;
	}

	return stoneSurfaceMaxY;
}
