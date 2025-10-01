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

#include "MapGeneratorDecoration.h"
#include "MapGeneratorSchematic.h"
#include "MapGenerator.h"
#include "Map.h"

#include "../../Utils/Noise.h"

#include "Core/Logger/Logger.h"


FlagDescription FlagdescDeco[] = {
	{"place_center_x",  DECO_PLACE_CENTER_X},
	{"place_center_y",  DECO_PLACE_CENTER_Y},
	{"place_center_z",  DECO_PLACE_CENTER_Z},
	{"forcePlacement", DECO_FORCE_PLACEMENT},
	{"liquid_surface",  DECO_LIQUID_SURFACE},
	{"all_floors",      DECO_ALL_FLOORS},
	{"all_ceilings",    DECO_ALL_CEILINGS},
	{NULL,              0}
};


///////////////////////////////////////////////////////////////////////////////


DecorationManager::DecorationManager(Environment* env) : ObjectManager(env, OBJ_DECORATION)
{
}


size_t DecorationManager::PlaceAllDecos(
    MapGenerator* mg, unsigned int blockSeed, Vector3<short> nmin, Vector3<short> nmax)
{
	size_t nplaced = 0;

	for (size_t i = 0; i != mObjects.size(); i++) 
    {
		Decoration* deco = (Decoration*)mObjects[i];
		if (!deco)
			continue;

		nplaced += deco->PlaceDeco(mg, blockSeed, nmin, nmax);
		blockSeed++;
	}

	return nplaced;
}

DecorationManager *DecorationManager::Clone() const
{
	auto mgr = new DecorationManager(mEnvironment);
	ObjectManager::CloneTo(mgr);
	return mgr;
}


///////////////////////////////////////////////////////////////////////////////


void Decoration::ResolveNodeNames()
{
	GetIdsFromNrBacklog(&mContentPlaceOn);
	GetIdsFromNrBacklog(&mContentSpawnBy);
}


bool Decoration::CanPlaceDecoration(MMVManip* vm, Vector3<short> pos)
{
	// Check if the decoration can be placed on this node
	unsigned int vi = vm->mArea.Index(pos);
    unsigned short content = vm->mData[vi].GetContent();
	if (std::find(mContentPlaceOn.begin(), mContentPlaceOn.end(), content) == mContentPlaceOn.end())
		return false;

	// Don't continue if there are no spawnby constraints
	if (mNodeSpawnBy == -1)
		return true;

	int nneighs = 0;
	static const Vector3<short> dirs[16] = {
        Vector3<short>{ 0, 0,  1},
        Vector3<short>{ 0, 0, -1},
        Vector3<short>{ 1, 0,  0},
        Vector3<short>{-1, 0,  0},
        Vector3<short>{ 1, 0,  1},
        Vector3<short>{-1, 0,  1},
        Vector3<short>{-1, 0, -1},
        Vector3<short>{ 1, 0, -1},

        Vector3<short>{ 0, 1,  1},
        Vector3<short>{ 0, 1, -1},
        Vector3<short>{ 1, 1,  0},
        Vector3<short>{-1, 1,  0},
        Vector3<short>{ 1, 1,  1},
        Vector3<short>{-1, 1,  1},
        Vector3<short>{-1, 1, -1},
        Vector3<short>{ 1, 1, -1}
	};

	// Check these 16 neighbouring nodes for enough spawnby nodes
    size_t arrLen = sizeof(dirs) / sizeof((dirs)[0]);
	for (size_t i = 0; i != arrLen; i++) 
    {
		unsigned int index = vm->mArea.Index(pos + dirs[i]);
		if (!vm->mArea.Contains(index))
			continue;

        content = vm->mData[index].GetContent();
        if (std::find(mContentSpawnBy.begin(), mContentSpawnBy.end(), content) != mContentSpawnBy.end())
			nneighs++;
	}

	if (nneighs < mNodeSpawnBy)
		return false;

	return true;
}


size_t Decoration::PlaceDeco(MapGenerator* mg, unsigned int blockSeed, Vector3<short> nmin, Vector3<short> nmax)
{
	PcgRandom ps(blockSeed + 53);
	int careaSize = nmax[0] - nmin[0] + 1;

	// Divide area into parts
	// If chunkSize is changed it may no longer be divisable by mSideLen
	if (careaSize % mSideLen)
		mSideLen = careaSize;

	int16_t divlen = careaSize / mSideLen;
	int area = mSideLen * mSideLen;

    for (int16_t z0 = 0; z0 < divlen; z0++)
    {
        for (int16_t x0 = 0; x0 < divlen; x0++) 
        {
            Vector2<short> p2dCenter{ // Center position of part of division
                (short)(nmin[0] + mSideLen / 2 + mSideLen * x0),
                (short)(nmin[2] + mSideLen / 2 + mSideLen * z0)
            };
            Vector2<short> p2dMin{ // Minimum edge of part of division
                (short)(nmin[0] + mSideLen * x0),
                (short)(nmin[2] + mSideLen * z0)
            };
            Vector2<short> p2dMax{ // Maximum edge of part of division
                (short)(nmin[0] + mSideLen + mSideLen * x0 - 1),
                (short)(nmin[2] + mSideLen + mSideLen * z0 - 1)
            };

            bool cover = false;
            // Amount of decorations
            float nval = (mFlags & DECO_USE_NOISE) ?
                NoisePerlin2D(&mNoiseParams, p2dCenter[0], p2dCenter[1], mMapSeed) : mFillRatio;
            unsigned int decoCount = 0;

            if (nval >= 10.0f) 
            {
                // Complete coverage. Disable random placement to avoid
                // redundant multiple placements at one position.
                cover = true;
                decoCount = area;
            }
            else 
            {
                float decoCountFloat = (float)area * nval;
                if (decoCountFloat >= 1.0f) 
                {
                    decoCount = (unsigned int)decoCountFloat;
                }
                else if (decoCountFloat > 0.0f) 
                {
                    // For very low density calculate a chance for 1 decoration
                    if (ps.Range(1000) <= decoCountFloat * 1000.0f)
                        decoCount = 1;
                }
            }

            int16_t x = p2dMin[0] - 1;
            int16_t z = p2dMin[1];

            for (unsigned int i = 0; i < decoCount; i++) 
            {
                if (!cover) 
                {
                    x = ps.Range(p2dMin[0], p2dMax[0]);
                    z = ps.Range(p2dMin[1], p2dMax[1]);
                }
                else 
                {
                    x++;
                    if (x == p2dMax[0] + 1)
                    {
                        z++;
                        x = p2dMin[0];
                    }
                }
                int mapindex = careaSize * (z - nmin[2]) + (x - nmin[0]);

                if ((mFlags & DECO_ALL_FLOORS) ||
                    (mFlags & DECO_ALL_CEILINGS)) 
                {
                    // All-surfaces decorations
                    // Check biome of column
                    if (mg->mBiomeMap && !mBiomes.empty()) 
                    {
                        auto iter = mBiomes.find(mg->mBiomeMap[mapindex]);
                        if (iter == mBiomes.end())
                            continue;
                    }

                    // Get all floors and ceilings in node column
                    uint16_t size = (nmax[1] - nmin[1] + 1) / 2;
                    std::vector<int16_t> floors;
                    std::vector<int16_t> ceilings;
                    floors.reserve(size);
                    ceilings.reserve(size);

                    mg->GetSurfaces(Vector2<short>{x, z}, nmin[1], nmax[1], floors, ceilings);

                    if (mFlags & DECO_ALL_FLOORS) 
                    {
                        // Floor decorations
                        for (const int16_t y : floors)
                        {
                            if (y < mYMin || y > mYMax)
                                continue;

                            Vector3<short> pos{ x, y, z };
                            if (Generate(mg->mMMVManip, &ps, pos, false))
                                mg->mGenNotify.AddEvent(GENNOTIFY_DECORATION, pos, mIndex);
                        }
                    }

                    if (mFlags & DECO_ALL_CEILINGS) 
                    {
                        // Ceiling decorations
                        for (const int16_t y : ceilings)
                        {
                            if (y < mYMin || y > mYMax)
                                continue;

                            Vector3<short> pos{ x, y, z };
                            if (Generate(mg->mMMVManip, &ps, pos, true))
                                mg->mGenNotify.AddEvent(GENNOTIFY_DECORATION, pos, mIndex);
                        }
                    }
                }
                else 
                { // Heightmap decorations
                    int16_t y = -MAX_MAP_GENERATION_LIMIT;
                    if (mFlags & DECO_LIQUID_SURFACE)
                        y = mg->FindLiquidSurface(Vector2<short>{x, z}, nmin[1], nmax[1]);
                    else if (mg->mHeightmap)
                        y = mg->mHeightmap[mapindex];
                    else
                        y = mg->FindGroundLevel(Vector2<short>{x, z}, nmin[1], nmax[1]);

                    if (y < mYMin || y > mYMax || y < nmin[1] || y > nmax[1])
                        continue;

                    if (mg->mBiomeMap && !mBiomes.empty())
                    {
                        auto iter = mBiomes.find(mg->mBiomeMap[mapindex]);
                        if (iter == mBiomes.end())
                            continue;
                    }

                    Vector3<short> pos{ x, y, z };
                    if (Generate(mg->mMMVManip, &ps, pos, false))
                        mg->mGenNotify.AddEvent(GENNOTIFY_DECORATION, pos, mIndex);
                }
            }
        }
    }

	return 0;
}


void Decoration::CloneTo(Decoration* dec) const
{
	Object::CloneTo(dec);
    dec->mFlags = mFlags;
    dec->mMapSeed = mMapSeed;
    dec->mContentPlaceOn = mContentPlaceOn;
    dec->mSideLen = mSideLen;
    dec->mYMin = mYMin;
    dec->mYMax = mYMax;
    dec->mFillRatio = mFillRatio;
    dec->mNoiseParams = mNoiseParams;
    dec->mContentSpawnBy = mContentSpawnBy;
    dec->mNodeSpawnBy = mNodeSpawnBy;
    dec->mPlaceOffsetY = mPlaceOffsetY;
    dec->mBiomes = mBiomes;
}


///////////////////////////////////////////////////////////////////////////////


Object* DecoSimple::Clone() const
{
	auto dec = new DecoSimple();
	Decoration::CloneTo(dec);

    dec->mContentDecos = mContentDecos;
    dec->mDecoHeight = mDecoHeight;
    dec->mDecoHeightMax = mDecoHeightMax;
    dec->mDecoParam2 = mDecoParam2;
    dec->mDecoParam2Max = mDecoParam2Max;

	return dec;
}


void DecoSimple::ResolveNodeNames()
{
	Decoration::ResolveNodeNames();
	GetIdsFromNrBacklog(&mContentDecos);
}


size_t DecoSimple::Generate(MMVManip* vm, PcgRandom* pr, Vector3<short> pos, bool ceiling)
{
	// Don't bother if there aren't any decorations to place
	if (mContentDecos.empty())
		return 0;

	if (!CanPlaceDecoration(vm, pos))
		return 0;

	// Check for placement outside the voxelmanip volume
	if (ceiling) 
    {
		// Ceiling decorations
		// 'place offset y' is inverted
		if (pos[1] - mPlaceOffsetY - std::max(mDecoHeight, mDecoHeightMax) < vm->mArea.mMinEdge[1])
			return 0;

		if (pos[1] - 1 - mPlaceOffsetY > vm->mArea.mMaxEdge[1])
			return 0;

	} 
    else 
    { // Heightmap and floor decorations
		if (pos[1] + mPlaceOffsetY + std::max(mDecoHeight, mDecoHeightMax) > vm->mArea.mMaxEdge[1])
			return 0;

		if (pos[1] + 1 + mPlaceOffsetY < vm->mArea.mMinEdge[1])
			return 0;
	}

	uint16_t contentPlace = mContentDecos[pr->Range(0, (int)mContentDecos.size() - 1)];
	int16_t height = (mDecoHeightMax > 0) ?
		pr->Range(mDecoHeight, mDecoHeightMax) : mDecoHeight;
	unsigned char param2 = (mDecoParam2Max > 0) ?
		pr->Range(mDecoParam2, mDecoParam2Max) : mDecoParam2;
	bool forcePlacement = (mFlags & DECO_FORCE_PLACEMENT);

	const Vector3<short>& em = vm->mArea.GetExtent();
	unsigned int vi = vm->mArea.Index(pos);

	if (ceiling) 
    {
		// Ceiling decorations
		// 'place offset y' is inverted
		VoxelArea::AddY(em, vi, -mPlaceOffsetY);

		for (int i = 0; i < height; i++) 
        {
			VoxelArea::AddY(em, vi, -1);
			uint16_t c = vm->mData[vi].GetContent();
			if (c != CONTENT_AIR && c != CONTENT_IGNORE && !forcePlacement)
				break;

			vm->mData[vi] = MapNode(contentPlace, 0, param2);
		}
	} 
    else 
    { // Heightmap and floor decorations
		VoxelArea::AddY(em, vi, mPlaceOffsetY);

		for (int i = 0; i < height; i++) 
        {
			VoxelArea::AddY(em, vi, 1);
			uint16_t c = vm->mData[vi].GetContent();
			if (c != CONTENT_AIR && c != CONTENT_IGNORE && !forcePlacement)
				break;

			vm->mData[vi] = MapNode(contentPlace, 0, param2);
		}
	}

	return 1;
}


///////////////////////////////////////////////////////////////////////////////


DecoSchematic::~DecoSchematic()
{
	if (mWasCloned)
		delete mSchematic;
}


Object* DecoSchematic::Clone() const
{
	auto dec = new DecoSchematic();
	Decoration::CloneTo(dec);
	NodeResolver::CloneTo(dec);

    dec->mRotation = mRotation;
	/* FIXME: We do not own this schematic, yet we only have a pointer to it
	 * and not a handle. We are left with no option but to clone it ourselves.
	 * This is a waste of memory and should be replaced with an alternative
	 * approach sometime. */
    dec->mSchematic = dynamic_cast<Schematic*>(mSchematic->Clone());
    dec->mWasCloned = true;

	return dec;
}


size_t DecoSchematic::Generate(MMVManip* vm, PcgRandom* pr, Vector3<short> pos, bool ceiling)
{
	// Schematic could have been unloaded but not the decoration
	// In this case generate() does nothing (but doesn't *fail*)
	if (mSchematic == NULL)
		return 0;

	if (!CanPlaceDecoration(vm, pos))
		return 0;

	if (mFlags & DECO_PLACE_CENTER_Y) 
    {
        pos[1] -= (mSchematic->mSize[1] - 1) / 2;
	} 
    else 
    {
		// Only apply 'place offset y' if not 'deco place center y'
        if (ceiling)
        {
            // Shift down so that schematic top layer is level with ceiling
            // 'place offset y' is inverted
            pos[1] -= (mPlaceOffsetY + mSchematic->mSize[1] - 1);
        }
		else pos[1] += mPlaceOffsetY;
	}

	// Check schematic top and base are in voxelmanip
	if (pos[1] + mSchematic->mSize[1] - 1 > vm->mArea.mMaxEdge[1])
		return 0;

	if (pos[1] < vm->mArea.mMinEdge[1])
		return 0;

	RotationDegrees rot = (mRotation == ROTATE_RAND) ?
		(RotationDegrees)pr->Range(ROTATE_0, ROTATE_270) : mRotation;

	if (mFlags & DECO_PLACE_CENTER_X) 
    {
		if (rot == ROTATE_0 || rot == ROTATE_180)
            pos[0] -= (mSchematic->mSize[0] - 1) / 2;
		else
            pos[2] -= (mSchematic->mSize[0] - 1) / 2;
	}
	if (mFlags & DECO_PLACE_CENTER_Z) 
    {
		if (rot == ROTATE_0 || rot == ROTATE_180)
            pos[2] -= (mSchematic->mSize[2] - 1) / 2;
		else
            pos[0] -= (mSchematic->mSize[2] - 1) / 2;
	}

	bool forcePlacement = (mFlags & DECO_FORCE_PLACEMENT);

	mSchematic->BlitToVManip(vm, pos, rot, forcePlacement);

	return 1;
}
