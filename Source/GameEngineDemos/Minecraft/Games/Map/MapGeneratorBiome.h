/*
Minetest
Copyright (C) 2014-2020 paramat
Copyright (C) 2014-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#ifndef MAPGENERATORBIOME_H
#define MAPGENERATORBIOME_H

#include "Object.h"

#include "../../Graphics/Node.h"

#include "../../Utils/Noise.h"

class BiomeManager;
class Environment;

////
//// Biome
////

typedef uint16_t Biometype;

#define BIOME_NONE ((Biometype)0)

enum BiomeType 
{
	BIOMETYPE_NORMAL,
};

class Biome : public Object, public NodeResolver 
{
public:
	Object* Clone() const;

	unsigned int mFlags;

	uint16_t mContentTop;
	uint16_t mContentFiller;
	uint16_t mContentStone;
	uint16_t mContentWaterTop;
	uint16_t mContentWater;
	uint16_t mContentRiverWater;
	uint16_t mContentRiverbed;
	uint16_t mContentDust;
	std::vector<uint16_t> mContentCaveLiquid;
	uint16_t mContentDungeon;
	uint16_t mContentDungeonAlt;
	uint16_t mContentDungeonStair;

	int16_t mDepthTop;
	int16_t mDepthFiller;
	int16_t mDepthWaterTop;
	int16_t mDepthRiverbed;

	Vector3<short> mMinPosition;
	Vector3<short> mMaxPosition;
	float mHeatPoint;
	float mHumidityPoint;
	int16_t mVerticalBlend;

	virtual void ResolveNodeNames();
};


////
//// BiomeGenerator
////

enum BiomeGeneratorType 
{
	BIOMEGEN_ORIGINAL
};

struct BiomeParams 
{
    virtual ~BiomeParams() = default;

	virtual void ReadParams(const Settings* settings) = 0;
	virtual void WriteParams(Settings* settings) = 0;

	int seed;
};

// WARNING: this class is not thread-safe
class BiomeGenerator 
{
public:
	virtual ~BiomeGenerator() = default;

	virtual BiomeGeneratorType GetType() const = 0;

	// Clone this BiomeGenerator and set a the new BiomeManager to be used by the copy
	virtual BiomeGenerator* Clone(BiomeManager* biomemgr) const = 0;

	// Check that the internal chunk size is what the mapgen expects, just to be sure.
	inline void AssertChunkSize(Vector3<short> expect) const
	{
		LogAssert(mChunkSize != expect, "Chunk size mismatches");
	}

	// Calculates the biome at the exact position provided.  This function can
	// be called at any time, but may be less efficient than the latter methods,
	// depending on implementation.
	virtual Biome* CalculateBiomeAtPoint(Vector3<short> pos) const = 0;

	// Computes any intermediate results needed for biome generation.  Must be
	// called before using any of: GetBiomes, GetBiomeAtPoint, or GetBiomeAtIndex.
	// Calling this invalidates the previous results stored in mBiomeMap.
	virtual void CalculateBiomeNoise(Vector3<short> pmin) = 0;

	// Gets all biomes in current chunk using each corresponding element of
	// heightmap as the y position, then stores the results by biome index in
	// mBiomeMap (also returned)
	virtual Biometype* GetBiomes(int16_t* heightmap, Vector3<short> pmin) = 0;

	// Gets a single biome at the specified position, which must be contained
	// in the region formed by mPosMin and (mPosMin + mChunkSize - 1).
	virtual Biome* GetBiomeAtPoint(Vector3<short> pos) const = 0;

	// Same as above, but uses a raw numeric index correlating to the (x,z) position.
	virtual Biome* GetBiomeAtIndex(size_t index, Vector3<short> pos) const = 0;

	// Result of calcBiomes bulk computation.
	Biometype* mBiomeMap = nullptr;

protected:
	BiomeManager* mBiomeMgr = nullptr;
	Vector3<short> mPosMin;
	Vector3<short> mChunkSize;
};


////
//// BiomeGenerator implementations
////

//
// Original biome algorithm (Whittaker's classification + surface height)
//

struct BiomeParamsOriginal : public BiomeParams 
{
	BiomeParamsOriginal() :
        noiseParamsHeat(50, 50, Vector3<float>{1000.0, 1000.0, 1000.0}, 5349, 3, 0.5, 2.0),
        noiseParamsHumidity(50, 50, Vector3<float>{1000.0, 1000.0, 1000.0}, 842, 3, 0.5, 2.0),
        noiseParamsHeatBlend(0, 1.5, Vector3<float>{8.0, 8.0, 8.0}, 13, 2, 1.0, 2.0),
        noiseParamsHumidityBlend(0, 1.5, Vector3<float>{8.0, 8.0, 8.0}, 90003, 2, 1.0, 2.0)
	{
	}

	virtual void ReadParams(const Settings* settings);
	virtual void WriteParams(Settings* settings);

	NoiseParams noiseParamsHeat;
	NoiseParams noiseParamsHumidity;
	NoiseParams noiseParamsHeatBlend;
	NoiseParams noiseParamsHumidityBlend;

protected:

    bool GetNoiseParamsFromValue(const Settings* settings, const std::string& name, NoiseParams& np) const;
    bool GetNoiseParamsFromGroup(const Settings* settings, const std::string& name, NoiseParams& np) const;

    bool GetNoiseParams(const Settings* settings, const std::string& name, NoiseParams& np) const;

    bool SetNoiseParams(Settings* settings, const std::string& name, const NoiseParams& np);
};

class BiomeGeneratorOriginal : public BiomeGenerator 
{
public:
	BiomeGeneratorOriginal(BiomeManager* biomemgr,
		const BiomeParamsOriginal* params, Vector3<short> chunkSize);
	virtual ~BiomeGeneratorOriginal();

	BiomeGeneratorType GetType() const { return BIOMEGEN_ORIGINAL; }

	BiomeGenerator* Clone(BiomeManager* biomemgr) const;

	// Slower, meant for Script API use
	float CalculateHeatAtPoint(Vector3<short> pos) const;
	float CalculateHumidityAtPoint(Vector3<short> pos) const;
	Biome* CalculateBiomeAtPoint(Vector3<short> pos) const;

	void CalculateBiomeNoise(Vector3<short> pmin);

	Biometype* GetBiomes(int16_t *heightmap, Vector3<short> pmin);
	Biome* GetBiomeAtPoint(Vector3<short> pos) const;
	Biome* GetBiomeAtIndex(size_t index, Vector3<short> pos) const;

	Biome* CalculateBiomeFromNoise(float heat, float humidity, Vector3<short> pos) const;

	float* mHeatMap;
	float* mHumIdMap;

private:
	const BiomeParamsOriginal* mParams;

	Noise* mNoiseHeat;
	Noise* mNoiseHumidity;
	Noise* mNoiseHeatBlend;
	Noise* mNoiseHumidityBlend;
};


////
//// BiomeManager
////

class BiomeManager : public ObjectManager 
{
public:

	BiomeManager(Environment* env);
	virtual ~BiomeManager() = default;

	BiomeManager* Clone() const;

	const char* GetObjectTitle() const
	{
		return "biome";
	}

	static Biome* Create(BiomeType type)
	{
		return new Biome;
	}

	BiomeGenerator* CreateBiomeGenerator(BiomeGeneratorType type, 
        BiomeParams *params, Vector3<short> chunkSize)
	{
		switch (type) 
        {
		    case BIOMEGEN_ORIGINAL:
			    return new BiomeGeneratorOriginal(this,
				    (BiomeParamsOriginal *)params, chunkSize);
		    default:
			    return NULL;
		}
	}

	static BiomeParams* CreateBiomeParams(BiomeGeneratorType type)
	{
		switch (type) 
        {
		    case BIOMEGEN_ORIGINAL:
			    return new BiomeParamsOriginal();
		    default:
			    return NULL;
		}
	}

	virtual void Clear();
};

#endif