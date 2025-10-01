/*
Minetest
Copyright (C) 2015-2020 paramat
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

#ifndef MAPGENERATORORE_H
#define MAPGENERATORORE_H

#include "Object.h"
#include "MapNode.h"

#include "../../Graphics/Node.h"

#include "../../Utils/Noise.h"

typedef uint16_t Biometype;  // copy from MapGeneratorBiome.h to avoid an unnecessary include

class Noise;
class MapGenerator;
class MMVManip;
class Environment;

/////////////////// Ore generation flags

#define OREFLAG_ABSHEIGHT     0x01 // Non-functional but kept to not break flags
#define OREFLAG_PUFF_CLIFFS   0x02
#define OREFLAG_PUFF_ADDITIVE 0x04
#define OREFLAG_USE_NOISE     0x08
#define OREFLAG_USE_NOISE2    0x10

enum OreType 
{
	ORE_SCATTER,
	ORE_SHEET,
	ORE_PUFF,
	ORE_BLOB,
	ORE_VEIN,
	ORE_STRATUM,
};

extern FlagDescription FlagdescOre[];

class Ore : public Object, public NodeResolver 
{
public:
	const bool mNeedsNoise;

	uint16_t mContentOre;                  // the node to place
	std::vector<uint16_t> mContentWherein; // the nodes to be placed in
	unsigned int mClustScarcity; // ore cluster has a 1-in-mClustScarcity chance of appearing at a node
	int16_t mClustNumOres; // how many ore nodes are in a chunk
	int16_t mClustSize;     // how large (in nodes) a chunk of ore is
	int16_t mYMin;
	int16_t mYMax;
	unsigned char mOreParam2;		// to set node-specific attributes
	unsigned int mFlags = 0;          // attributes for this ore
	float mNoiseThresh;      // threshold for noise at which an ore is placed
	NoiseParams mNoiseParams;     // noise for distribution of clusters (NULL for uniform scattering)
	Noise* mNoise = nullptr;
	std::unordered_set<Biometype> mBiomes;

	explicit Ore(bool mNeedsNoise): mNeedsNoise(mNeedsNoise) {}
	virtual ~Ore();

	virtual void ResolveNodeNames();

	size_t PlaceOre(MapGenerator* mg, unsigned int blockseed, Vector3<short> nmin, Vector3<short> nmax);
	virtual void Generate(MMVManip* vm, int mapSeed, unsigned int blockseed,
		Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) = 0;

protected:
	void CloneTo(Ore *def) const;
};

class OreScatter : public Ore {
public:
	OreScatter() : Ore(false) {}

	Object* Clone() const override;

	void Generate(MMVManip* vm, int mapSeed, unsigned int blockseed,
			Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) override;
};

class OreSheet : public Ore 
{
public:
	OreSheet() : Ore(true) {}

	Object* Clone() const override;

	uint16_t mColumnHeightMin;
	uint16_t mColumnHeightMax;
	float mColumnMidpointFactor;

	void Generate(MMVManip* vm, int mapSeed, unsigned int blockseed,
        Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) override;
};

class OrePuff : public Ore 
{
public:
	Object* Clone() const override;

	NoiseParams mNoiseParamsPuffTop;
	NoiseParams mNoiseParamsPuffBottom;
	Noise* mNoisePuffTop = nullptr;
	Noise* mNoisePuffBottom = nullptr;

	OrePuff() : Ore(true) {}
	virtual ~OrePuff();

	void Generate(MMVManip *vm, int mapSeed, unsigned int blockseed,
        Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) override;
};

class OreBlob : public Ore 
{
public:
	Object* Clone() const override;

	OreBlob() : Ore(true) {}
	void Generate(MMVManip* vm, int mapSeed, unsigned int blockseed,
        Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) override;
};

class OreVein : public Ore 
{
public:
	Object* Clone() const override;

	float mRandomFactor;
	Noise* mNoise2 = nullptr;
	int mSizeyPrev = 0;

	OreVein() : Ore(true) {}
	virtual ~OreVein();

	void Generate(MMVManip* vm, int mapSeed, unsigned int blockseed,
        Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) override;
};

class OreStratum : public Ore 
{
public:
	Object* Clone() const override;

	NoiseParams mNoiseParamsStratumThickness;
	Noise* mNoiseStratumThickness = nullptr;
	uint16_t mStratumThickness;

	OreStratum() : Ore(false) {}
	virtual ~OreStratum();

	void Generate(MMVManip* vm, int mapSeed, unsigned int blockseed,
        Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap) override;
};

class OreManager : public ObjectManager 
{
public:
	OreManager(Environment* env);
	virtual ~OreManager() = default;

	OreManager* Clone() const;

	const char* GetObjectTitle() const
	{
		return "ore";
	}

	static Ore* Create(OreType type)
	{
		switch (type) 
        {
		    case ORE_SCATTER:
			    return new OreScatter;
		    case ORE_SHEET:
			    return new OreSheet;
		    case ORE_PUFF:
			    return new OrePuff;
		    case ORE_BLOB:
			    return new OreBlob;
		    case ORE_VEIN:
			    return new OreVein;
		    case ORE_STRATUM:
			    return new OreStratum;
		    default:
			    return nullptr;
		}
	}

	void Clear();

	size_t PlaceAllOres(MapGenerator* mg, unsigned int blockseed, Vector3<short> nmin, Vector3<short> nmax);
};

#endif