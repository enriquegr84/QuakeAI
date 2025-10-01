/*
Minetest
Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#include "Map.h" //for MMVManip
#include "MapGeneratorBiome.h"
#include "MapGeneratorDecoration.h"

#include "Emerge.h"

#include "../../Graphics/Node.h"

#include "../../Games/Environment/LogicEnvironment.h"

#include "Application/Settings.h"


///////////////////////////////////////////////////////////////////////////////


BiomeManager::BiomeManager(Environment* env) : ObjectManager(env, OBJ_BIOME)
{
	// Create default biome to be used in case none exist
	Biome* biome = new Biome;

    biome->mName = "default";
    biome->mFlags = 0;
    biome->mDepthTop = 0;
    biome->mDepthFiller = -MAX_MAP_GENERATION_LIMIT;
    biome->mDepthWaterTop = 0;
    biome->mDepthRiverbed = 0;
    biome->mMinPosition = Vector3<short>{ -MAX_MAP_GENERATION_LIMIT,
            -MAX_MAP_GENERATION_LIMIT, -MAX_MAP_GENERATION_LIMIT };
    biome->mMaxPosition = Vector3<short>{ MAX_MAP_GENERATION_LIMIT,
            MAX_MAP_GENERATION_LIMIT, MAX_MAP_GENERATION_LIMIT };
    biome->mHeatPoint = 0.0;
    biome->mHumidityPoint = 0.0;
    biome->mVerticalBlend = 0;

    biome->mNodeNames.emplace_back("mapgen_stone");
    biome->mNodeNames.emplace_back("mapgen_stone");
    biome->mNodeNames.emplace_back("mapgen_stone");
    biome->mNodeNames.emplace_back("mapgen_water_source");
    biome->mNodeNames.emplace_back("mapgen_water_source");
    biome->mNodeNames.emplace_back("mapgen_river_water_source");
    biome->mNodeNames.emplace_back("mapgen_stone");
    biome->mNodeNames.emplace_back("ignore");
    biome->mNodeNames.emplace_back("ignore");
    biome->mNodeListSizes.push_back(1);
    biome->mNodeNames.emplace_back("ignore");
    biome->mNodeNames.emplace_back("ignore");
    biome->mNodeNames.emplace_back("ignore");
	mEnvironment->GetNodeManager()->PendNodeResolve(biome);

	Add(biome);
}


void BiomeManager::Clear()
{
	// Remove all dangling references in Decorations
	DecorationManager* decomgr = EmergeManager::Get()->GetWritableDecorationManager();
	for (unsigned int i = 0; i != (unsigned int)decomgr->GetNumObjects(); i++)
    {
		Decoration* deco = (Decoration*)decomgr->GetRaw(i);
		deco->mBiomes.clear();
	}

	// Don't delete the first biome
	for (unsigned int i = 1; i < (unsigned int)mObjects.size(); i++)
		delete (Biome*)mObjects[i];

	mObjects.resize(1);
}


BiomeManager* BiomeManager::Clone() const
{
	auto mgr = new BiomeManager(mEnvironment);
	LogAssert(mgr, "invalid biome manager");
	ObjectManager::CloneTo(mgr);
	return mgr;
}

////////////////////////////////////////////////////////////////////////////////



bool BiomeParamsOriginal::GetNoiseParamsFromValue(
    const Settings* settings, const std::string& name, NoiseParams& np) const
{
    std::string value;

    try 
    {
        value = settings->Get(name);
    }
    catch (SettingNotFoundException&) 
    {
        return false;
    }

    // Format: f32,f32,(f32,f32,f32),s32,s32,f32[,f32]
    Strfnd f(value);

    np.offset = (float)atof(f.Next(",").c_str());
    np.scale = (float)atof(f.Next(",").c_str());
    f.Next("(");
    np.spread[0] = (float)atof(f.Next(",").c_str());
    np.spread[1] = (float)atof(f.Next(",").c_str());
    np.spread[2] = (float)atof(f.Next(")").c_str());
    f.Next(",");
    np.seed = atoi(f.Next(",").c_str());
    np.octaves = atoi(f.Next(",").c_str());
    np.persist = (float)atof(f.Next(",").c_str());

    std::string optionalParams = f.Next("");
    if (!optionalParams.empty())
        np.lacunarity = (float)atof(optionalParams.c_str());

    return true;
}


bool BiomeParamsOriginal::GetNoiseParamsFromGroup(
    const Settings* settings, const std::string& name, NoiseParams &np) const
{
    Settings* group = NULL;

    try 
    {
        group = settings->GetGroup(name);
    }
    catch (SettingNotFoundException&) 
    {
        return false;
    }

    try
    {
        np.offset = group->GetFloat("offset");
        np.scale = group->GetFloat("scale");
        np.spread = group->GetVector3("spread");
        np.seed = group->GetInt("seed");
        np.octaves = group->GetUInt16("octaves");
        np.persist = group->GetFloat("persistence");
        np.lacunarity = group->GetFloat("lacunarity");
    }
    catch (SettingNotFoundException&)
    {

    }

    np.flags = 0;
    if (!group->GetFlagString("flags", FlagdescNoiseparams, &np.flags))
        np.flags = NOISE_FLAG_DEFAULTS;

    return true;
}

bool BiomeParamsOriginal::GetNoiseParams(
    const Settings* settings, const std::string& name, NoiseParams& np) const
{
    if (GetNoiseParamsFromGroup(settings, name, np) ||
        GetNoiseParamsFromValue(settings, name, np))
        return true;
    if (auto parent = settings->GetParent())
        return GetNoiseParams(parent, name, np);

    return false;
}

bool BiomeParamsOriginal::SetNoiseParams(
    Settings* settings, const std::string &name, const NoiseParams &np)
{
    Settings* group = new Settings;

    group->SetFloat("offset", np.offset);
    group->SetFloat("scale", np.scale);
    group->SetVector3("spread", np.spread);
    group->SetInt("seed", np.seed);
    group->SetInt("octaves", np.octaves);
    group->SetFloat("persistence", np.persist);
    group->SetFloat("lacunarity", np.lacunarity);
    group->SetFlagString("flags", np.flags, FlagdescNoiseparams, np.flags);

    return settings->SetEntry(name, &group, true);
}

void BiomeParamsOriginal::ReadParams(const Settings* settings)
{
	GetNoiseParams(settings, "mg_biome_np_heat", noiseParamsHeat);
	GetNoiseParams(settings, "mg_biome_np_heat_blend", noiseParamsHeatBlend);
	GetNoiseParams(settings, "mg_biome_np_humidity", noiseParamsHumidity);
	GetNoiseParams(settings, "mg_biome_np_humidity_blend", noiseParamsHumidityBlend);
}

void BiomeParamsOriginal::WriteParams(Settings* settings)
{
	SetNoiseParams(settings, "mg_biome_np_heat", noiseParamsHeat);
	SetNoiseParams(settings, "mg_biome_np_heat_blend", noiseParamsHeatBlend);
	SetNoiseParams(settings, "mg_biome_np_humidity", noiseParamsHumidity);
	SetNoiseParams(settings, "mg_biome_np_humidity_blend", noiseParamsHumidityBlend);
}


////////////////////////////////////////////////////////////////////////////////

BiomeGeneratorOriginal::BiomeGeneratorOriginal(BiomeManager *biomemgr,
	const BiomeParamsOriginal *params, Vector3<short> chunkSize)
{
	mBiomeMgr = biomemgr;
	mParams = params;
	mChunkSize = chunkSize;

	mNoiseHeat = new Noise(&params->noiseParamsHeat,
        params->seed, mChunkSize[0], mChunkSize[2]);
	mNoiseHumidity = new Noise(&params->noiseParamsHumidity,
        params->seed, mChunkSize[0], mChunkSize[2]);
	mNoiseHeatBlend = new Noise(&params->noiseParamsHeatBlend,
        params->seed, mChunkSize[0], mChunkSize[2]);
	mNoiseHumidityBlend = new Noise(&params->noiseParamsHumidityBlend,
        params->seed, mChunkSize[0], mChunkSize[2]);

	mHeatMap = mNoiseHeat->mResult;
	mHumIdMap = mNoiseHumidity->mResult;

	mBiomeMap = new Biometype[mChunkSize[0] * mChunkSize[2]];
	// Initialise with the ID of 'BIOME_NONE' so that cavegen can get the
	// fallback biome when biome generation (which calculates the mBiomeMap IDs)
	// is disabled.
	memset(mBiomeMap, 0, sizeof(Biometype) * mChunkSize[0] * mChunkSize[2]);
}

BiomeGeneratorOriginal::~BiomeGeneratorOriginal()
{
	delete []mBiomeMap;

	delete mNoiseHeat;
	delete mNoiseHumidity;
	delete mNoiseHeatBlend;
	delete mNoiseHumidityBlend;
}

BiomeGenerator* BiomeGeneratorOriginal::Clone(BiomeManager* biomemgr) const
{
	return new BiomeGeneratorOriginal(biomemgr, mParams, mChunkSize);
}

float BiomeGeneratorOriginal::CalculateHeatAtPoint(Vector3<short> pos) const
{
	return NoisePerlin2D(&mParams->noiseParamsHeat, pos[0], pos[2], mParams->seed) +
		NoisePerlin2D(&mParams->noiseParamsHeatBlend, pos[0], pos[2], mParams->seed);
}

float BiomeGeneratorOriginal::CalculateHumidityAtPoint(Vector3<short> pos) const
{
	return NoisePerlin2D(&mParams->noiseParamsHumidity, pos[0], pos[2], mParams->seed) +
		NoisePerlin2D(&mParams->noiseParamsHumidityBlend, pos[0], pos[2], mParams->seed);
}

Biome* BiomeGeneratorOriginal::CalculateBiomeAtPoint(Vector3<short> pos) const
{
	return CalculateBiomeFromNoise(CalculateHeatAtPoint(pos), CalculateHumidityAtPoint(pos), pos);
}

void BiomeGeneratorOriginal::CalculateBiomeNoise(Vector3<short> pmin)
{
	mPosMin = pmin;

	mNoiseHeat->PerlinMap2D(pmin[0], pmin[2]);
	mNoiseHumidity->PerlinMap2D(pmin[0], pmin[2]);
	mNoiseHeatBlend->PerlinMap2D(pmin[0], pmin[2]);
	mNoiseHumidityBlend->PerlinMap2D(pmin[0], pmin[2]);

	for (int i = 0; i < mChunkSize[0] * mChunkSize[2]; i++) 
    {
		mNoiseHeat->mResult[i] += mNoiseHeatBlend->mResult[i];
		mNoiseHumidity->mResult[i] += mNoiseHumidityBlend->mResult[i];
	}
}


Biometype* BiomeGeneratorOriginal::GetBiomes(int16_t* heightmap, Vector3<short> pmin)
{
    for (int16_t zr = 0; zr < mChunkSize[2]; zr++)
    {
        for (int16_t xr = 0; xr < mChunkSize[0]; xr++)
        {
            int i = zr * mChunkSize[0] + xr;
            Biome* biome = CalculateBiomeFromNoise(
                mNoiseHeat->mResult[i], mNoiseHumidity->mResult[i],
                Vector3<short>{(short)(pmin[0] + xr), heightmap[i], (short)(pmin[2] + zr)});

            mBiomeMap[i] = biome->mIndex;
        }
    }

	return mBiomeMap;
}


Biome* BiomeGeneratorOriginal::GetBiomeAtPoint(Vector3<short> pos) const
{
	return GetBiomeAtIndex(
		(pos[2] - mPosMin[2]) * mChunkSize[0] + (pos[0] - mPosMin[0]), pos);
}


Biome* BiomeGeneratorOriginal::GetBiomeAtIndex(size_t index, Vector3<short> pos) const
{
	return CalculateBiomeFromNoise(
		mNoiseHeat->mResult[index], mNoiseHumidity->mResult[index], pos);
}


Biome* BiomeGeneratorOriginal::CalculateBiomeFromNoise(float heat, float humidity, Vector3<short> pos) const
{
	Biome* biomeClosest = nullptr;
	Biome* biomeClosestBlend = nullptr;
	float distMin = FLT_MAX;
	float distMinBlend = FLT_MAX;

	for (unsigned int i = 1; i < (unsigned int)mBiomeMgr->GetNumObjects(); i++)
    {
		Biome* biome = (Biome*)mBiomeMgr->GetRaw(i);
		if (!biome ||
            pos[1] < biome->mMinPosition[1] || pos[1] > biome->mMaxPosition[1] + biome->mVerticalBlend ||
            pos[0] < biome->mMinPosition[0] || pos[0] > biome->mMaxPosition[0] ||
            pos[2] < biome->mMinPosition[2] || pos[2] > biome->mMaxPosition[2])
			continue;

		float distHeat = heat - biome->mHeatPoint;
		float distHumidity = humidity - biome->mHumidityPoint;
		float dist = (distHeat * distHeat) + (distHumidity * distHumidity);

		if (pos[1] <= biome->mMaxPosition[1]) 
        { 
            // Within y limits of biome b
			if (dist < distMin) 
            {
				distMin = dist;
				biomeClosest = biome;
			}
		} 
        else if (dist < distMinBlend) 
        {
            // Blend area above biome b
			distMinBlend = dist;
			biomeClosestBlend = biome;
		}
	}

	// Carefully tune pseudorandom seed variation to avoid single node dither
	// and create larger scale blending patterns similar to horizontal biome
	// blend.
	const uint64_t seed = (uint64_t)(pos[1] + (heat + humidity) * 0.9f);
	PcgRandom rng(seed);

	if (biomeClosestBlend && distMinBlend <= distMin &&
        rng.Range(0, biomeClosestBlend->mVerticalBlend) >=
        pos[1] - biomeClosestBlend->mMaxPosition[1])
		return biomeClosestBlend;

	return (biomeClosest) ? biomeClosest : (Biome *)mBiomeMgr->GetRaw(BIOME_NONE);	
}


////////////////////////////////////////////////////////////////////////////////

Object* Biome::Clone() const
{
	auto obj = new Biome();
	Object::CloneTo(obj);
	NodeResolver::CloneTo(obj);

	obj->mFlags = mFlags;

	obj->mContentTop = mContentTop;
	obj->mContentFiller = mContentFiller;
	obj->mContentStone = mContentStone;
	obj->mContentWaterTop = mContentWaterTop;
	obj->mContentWater = mContentWater;
	obj->mContentRiverWater = mContentRiverWater;
	obj->mContentRiverbed = mContentRiverbed;
	obj->mContentDust = mContentDust;
	obj->mContentCaveLiquid = mContentCaveLiquid;
	obj->mContentDungeon = mContentDungeon;
	obj->mContentDungeonAlt = mContentDungeonAlt;
	obj->mContentDungeonStair = mContentDungeonStair;

	obj->mDepthTop = mDepthTop;
	obj->mDepthFiller = mDepthFiller;
	obj->mDepthWaterTop = mDepthWaterTop;
	obj->mDepthRiverbed = mDepthRiverbed;

	obj->mMinPosition = mMinPosition;
	obj->mMaxPosition = mMaxPosition;
	obj->mHeatPoint = mHeatPoint;
	obj->mHumidityPoint = mHumidityPoint;
	obj->mVerticalBlend = mVerticalBlend;

	return obj;
}

void Biome::ResolveNodeNames()
{
	GetIdFromNrBacklog(&mContentTop, "mapgenodeStone", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentFiller, "mapgenodeStone", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentStone, "mapgenodeStone", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentWaterTop, "mapgenodeWater_source", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentWater, "mapgenodeWater_source", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentRiverWater, "mapgen_river_water_source", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentRiverbed, "mapgenodeStone", CONTENT_AIR, false);
	GetIdFromNrBacklog(&mContentDust, "ignore", CONTENT_IGNORE, false);
	GetIdsFromNrBacklog(&mContentCaveLiquid);
	GetIdFromNrBacklog(&mContentDungeon, "ignore", CONTENT_IGNORE, false);
	GetIdFromNrBacklog(&mContentDungeonAlt, "ignore", CONTENT_IGNORE, false);
	GetIdFromNrBacklog(&mContentDungeonStair, "ignore", CONTENT_IGNORE, false);
}
