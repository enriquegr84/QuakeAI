/*
Minetest
Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2015-2018 paramat

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

#ifndef MAPGENERATORDECORATION_H
#define MAPGENERATORDECORATION_H

#include "Object.h"
#include "MapNode.h"

#include "../../Utils/Noise.h"

#include "../../Graphics/Node.h"

typedef uint16_t Biometype;  // copy from MapGeneratorBiome.h to avoid an unnecessary include

class MapGenerator;
class MMVManip;
class PcgRandom;
class Schematic;
class Environment;

enum DecorationType 
{
	DECO_SIMPLE,
	DECO_SCHEMATIC,
	DECO_LSYSTEM
};

#define DECO_PLACE_CENTER_X  0x01
#define DECO_PLACE_CENTER_Y  0x02
#define DECO_PLACE_CENTER_Z  0x04
#define DECO_USE_NOISE       0x08
#define DECO_FORCE_PLACEMENT 0x10
#define DECO_LIQUID_SURFACE  0x20
#define DECO_ALL_FLOORS      0x40
#define DECO_ALL_CEILINGS    0x80

extern FlagDescription FlagdescDeco[];


class Decoration : public Object, public NodeResolver 
{
public:
	Decoration() = default;
	virtual ~Decoration() = default;

	virtual void ResolveNodeNames();

	bool CanPlaceDecoration(MMVManip* vm, Vector3<short> pos);
	size_t PlaceDeco(MapGenerator* mg, unsigned int blockseed, Vector3<short> nmin, Vector3<short> nmax);

	virtual size_t Generate(MMVManip* vm, PcgRandom* pr, Vector3<short> pos, bool ceiling) = 0;

	unsigned int mFlags = 0;
	int mMapSeed = 0;
	std::vector<uint16_t> mContentPlaceOn;
	int16_t mSideLen = 1;
	int16_t mYMin;
	int16_t mYMax;
	float mFillRatio = 0.0f;
	NoiseParams mNoiseParams;
	std::vector<uint16_t> mContentSpawnBy;
	int16_t mNodeSpawnBy;
	int16_t mPlaceOffsetY = 0;

	std::unordered_set<Biometype> mBiomes;

protected:
	void CloneTo(Decoration* dec) const;
};


class DecoSimple : public Decoration 
{
public:
	Object* Clone() const;

	virtual void ResolveNodeNames();
	virtual size_t Generate(MMVManip* vm, PcgRandom* pr, Vector3<short> pos, bool ceiling);

	std::vector<uint16_t> mContentDecos;
	int16_t mDecoHeight;
	int16_t mDecoHeightMax;
	unsigned char mDecoParam2;
	unsigned char mDecoParam2Max;
};


class DecoSchematic : public Decoration 
{
public:
	Object* Clone() const;

	DecoSchematic() = default;
	virtual ~DecoSchematic();

	virtual size_t Generate(MMVManip* vm, PcgRandom* pr, Vector3<short> pos, bool ceiling);

    RotationDegrees mRotation;
	Schematic* mSchematic = nullptr;
	bool mWasCloned = false; // see FIXME inside DecoSchemtic::clone()
};


/*
class DecoLSystem : public Decoration 
{
public:
	virtual void Generate(MapGenerator* mg, unsigned int blockseed, Vector3<short> nmin, Vector3<short> nmax);
};
*/


class DecorationManager : public ObjectManager 
{
public:
	DecorationManager(Environment* env);
	virtual ~DecorationManager() = default;

	DecorationManager* Clone() const;

	const char* GetObjectTitle() const
	{
		return "decoration";
	}

	static Decoration* Create(DecorationType type)
	{
		switch (type) 
        {
		    case DECO_SIMPLE:
			    return new DecoSimple;
		    case DECO_SCHEMATIC:
			    return new DecoSchematic;
		    //case DECO_LSYSTEM:
		    //	return new DecoLSystem;
		    default:
			    return NULL;
		}
	}

	size_t PlaceAllDecos(MapGenerator* mg, unsigned int blockseed, 
        Vector3<short> nmin, Vector3<short> nmax);
};

#endif