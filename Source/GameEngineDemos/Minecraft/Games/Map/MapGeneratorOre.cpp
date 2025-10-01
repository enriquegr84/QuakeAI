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

#include "MapGeneratorOre.h"
#include "MapGenerator.h"
#include "Map.h"

#include "../../Utils/Noise.h"

#include "Core/Logger/Logger.h"


FlagDescription FlagdescOre[] = 
{
	{"absheight",                 OREFLAG_ABSHEIGHT}, // Non-functional
	{"puff_cliffs",               OREFLAG_PUFF_CLIFFS},
	{"puff_additive_composition", OREFLAG_PUFF_ADDITIVE},
	{NULL,                        0}
};


///////////////////////////////////////////////////////////////////////////////


OreManager::OreManager(Environment* env) : ObjectManager(env, OBJ_ORE)
{
}


size_t OreManager::PlaceAllOres(MapGenerator* mg, unsigned int blockSeed, Vector3<short> nmin, Vector3<short> nmax)
{
	size_t nplaced = 0;

	for (size_t i = 0; i != mObjects.size(); i++) 
    {
		Ore* ore = (Ore*)mObjects[i];
		if (!ore)
			continue;

		nplaced += ore->PlaceOre(mg, blockSeed, nmin, nmax);
		blockSeed++;
	}

	return nplaced;
}


void OreManager::Clear()
{
	for (Object* object : mObjects) 
    {
		Ore* ore = (Ore*) object;
		delete ore;
	}
	mObjects.clear();
}


OreManager* OreManager::Clone() const
{
	auto mgr = new OreManager(mEnvironment);
	ObjectManager::CloneTo(mgr);
	return mgr;
}


///////////////////////////////////////////////////////////////////////////////


Ore::~Ore()
{
	delete mNoise;
}


void Ore::ResolveNodeNames()
{
	GetIdFromNrBacklog(&mContentOre, "", CONTENT_AIR);
	GetIdsFromNrBacklog(&mContentWherein);
}


size_t Ore::PlaceOre(MapGenerator* mg, unsigned int blockSeed, Vector3<short> nmin, Vector3<short> nmax)
{
	if (nmin[1] > mYMax || nmax[1] < mYMin)
		return 0;

	int actualYmin = std::max(nmin[1], mYMin);
	int actualYmax = std::min(nmax[1], mYMax);
	if (mClustSize >= actualYmax - actualYmin + 1)
		return 0;

	nmin[1] = actualYmin;
	nmax[1] = actualYmax;
	Generate(mg->mMMVManip, mg->mSeed, blockSeed, nmin, nmax, mg->mBiomeMap);

	return 1;
}


void Ore::CloneTo(Ore* ore) const
{
	Object::CloneTo(ore);
	NodeResolver::CloneTo(ore);
    ore->mContentOre = mContentOre;
    ore->mContentWherein = mContentWherein;
    ore->mClustScarcity = mClustScarcity;
    ore->mClustNumOres = mClustNumOres;
    ore->mClustSize = mClustSize;
    ore->mYMin = mYMin;
    ore->mYMax = mYMax;
    ore->mOreParam2 = mOreParam2;
    ore->mFlags = mFlags;
    ore->mNoiseThresh = mNoiseThresh;
    ore->mNoiseParams = mNoiseParams;
    ore->mNoise = nullptr; // cannot be shared! so created on demand
    ore->mBiomes = mBiomes;
}


///////////////////////////////////////////////////////////////////////////////


Object* OreScatter::Clone() const
{
	auto oreScatter = new OreScatter();
	Ore::CloneTo(oreScatter);
	return oreScatter;
}


void OreScatter::Generate(MMVManip* vm, int mapSeed, unsigned int blockSeed,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	PcgRandom pr(blockSeed);
	MapNode nodeOre(mContentOre, 0, mOreParam2);

	unsigned int sizex  = (nmax[0] - nmin[0] + 1);
	unsigned int volume = (nmax[0] - nmin[0] + 1) *
        (nmax[1] - nmin[1] + 1) * (nmax[2] - nmin[2] + 1);
	unsigned int csize = mClustSize;
	unsigned int cvolume = csize * csize * csize;
	unsigned int nclusters = volume / mClustScarcity;

	for (unsigned int i = 0; i != nclusters; i++) 
    {
		int x0 = pr.Range(nmin[0], nmax[0] - csize + 1);
		int y0 = pr.Range(nmin[1], nmax[1] - csize + 1);
		int z0 = pr.Range(nmin[2], nmax[2] - csize + 1);

		if ((mFlags & OREFLAG_USE_NOISE) &&
			(NoisePerlin3D(&mNoiseParams, (float)x0, (float)y0, (float)z0, mapSeed) < mNoiseThresh))
			continue;

		if (biomeMap && !mBiomes.empty()) 
        {
			unsigned int index = sizex * (z0 - nmin[2]) + (x0 - nmin[0]);
			auto it = mBiomes.find(biomeMap[index]);
			if (it == mBiomes.end())
				continue;
		}

        for (unsigned int z1 = 0; z1 != csize; z1++)
        {
            for (unsigned int y1 = 0; y1 != csize; y1++)
            {
                for (unsigned int x1 = 0; x1 != csize; x1++) 
                {
                    if (pr.Range(1, cvolume) > mClustNumOres)
                        continue;

                    unsigned int i = vm->mArea.Index(x0 + x1, y0 + y1, z0 + z1);

                    unsigned short content = vm->mData[i].GetContent();
                    if (std::find(mContentWherein.begin(), mContentWherein.end(), content) == mContentWherein.end())
                        continue;

                    vm->mData[i] = nodeOre;
                }
            }
        }
	}
}


///////////////////////////////////////////////////////////////////////////////


Object* OreSheet::Clone() const
{
	auto ore = new OreSheet();
	Ore::CloneTo(ore);

    ore->mColumnHeightMax = mColumnHeightMax;
    ore->mColumnHeightMin = mColumnHeightMin;
    ore->mColumnMidpointFactor = mColumnMidpointFactor;

	return ore;
}


void OreSheet::Generate(MMVManip* vm, int mapSeed, unsigned int blockSeed,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	PcgRandom pr(blockSeed + 4234);
	MapNode nodeOre(mContentOre, 0, mOreParam2);

	uint16_t maxHeight = mColumnHeightMax;
	int yStartMin = nmin[1] + maxHeight;
	int yStartMax = nmax[1] - maxHeight;

	int yStart = yStartMin < yStartMax ?
        pr.Range(yStartMin, yStartMax) : (yStartMin + yStartMax) / 2;

	if (!mNoise) 
    {
		int sx = nmax[0] - nmin[0] + 1;
		int sz = nmax[2] - nmin[2] + 1;
        mNoise = new Noise(&mNoiseParams, 0, sx, sz);
	}
    mNoise->mSeed = mapSeed + yStart;
    mNoise->PerlinMap2D(nmin[0], nmin[2]);

	size_t index = 0;
    for (int z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int x = nmin[0]; x <= nmax[0]; x++, index++)
        {
            float noiseval = mNoise->mResult[index];
            if (noiseval < mNoiseThresh)
                continue;

            if (biomeMap && ! mBiomes.empty()) 
            {
                auto it = mBiomes.find(biomeMap[index]);
                if (it == mBiomes.end())
                    continue;
            }

            uint16_t height = pr.Range(mColumnHeightMin, mColumnHeightMax);
            int ymidpoint = yStart + (int)noiseval;
            int y0 = std::max(nmin[1], (short)(ymidpoint - height * (1 - mColumnMidpointFactor)));
            int y1 = std::min(nmax[1], (short)(y0 + height - 1));

            for (int y = y0; y <= y1; y++) 
            {
                unsigned int i = vm->mArea.Index(x, y, z);
                if (!vm->mArea.Contains(i))
                    continue;

                unsigned short content = vm->mData[i].GetContent();
                if (std::find(mContentWherein.begin(), mContentWherein.end(), content) == mContentWherein.end())
                    continue;

                vm->mData[i] = nodeOre;
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////


OrePuff::~OrePuff()
{
	delete mNoisePuffTop;
	delete mNoisePuffBottom;
}


Object* OrePuff::Clone() const
{
	auto orePuff = new OrePuff();
	Ore::CloneTo(orePuff);

    orePuff->mNoiseParamsPuffTop = mNoiseParamsPuffTop;
    orePuff->mNoiseParamsPuffBottom = mNoiseParamsPuffBottom;
    orePuff->mNoisePuffTop = nullptr; // cannot be shared, on-demand
    orePuff->mNoisePuffBottom = nullptr;

	return orePuff;
}


void OrePuff::Generate(MMVManip* vm, int mapSeed, unsigned int blockSeed,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	PcgRandom pr(blockSeed + 4234);
	MapNode nodeOre(mContentOre, 0, mOreParam2);

	int yStart = pr.Range(nmin[1], nmax[1]);

	if (!mNoise) 
    {
		int sx = nmax[0] - nmin[0] + 1;
		int sz = nmax[2] - nmin[2] + 1;
        mNoise = new Noise(&mNoiseParams, 0, sx, sz);
		mNoisePuffTop = new Noise(&mNoiseParamsPuffTop, 0, sx, sz);
		mNoisePuffBottom = new Noise(&mNoiseParamsPuffBottom, 0, sx, sz);
	}

	mNoise->mSeed = mapSeed + yStart;
    mNoise->PerlinMap2D(nmin[0], nmin[2]);
	bool noiseGenerated = false;

	size_t index = 0;
    for (int z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int x = nmin[0]; x <= nmax[0]; x++, index++) 
        {
            float noiseval = mNoise->mResult[index];
            if (noiseval < mNoiseThresh)
                continue;

            if (biomeMap && ! mBiomes.empty()) 
            {
                auto it = mBiomes.find(biomeMap[index]);
                if (it == mBiomes.end())
                    continue;
            }

            if (!noiseGenerated) 
            {
                noiseGenerated = true;
                mNoisePuffTop->PerlinMap2D(nmin[0], nmin[2]);
                mNoisePuffBottom->PerlinMap2D(nmin[0], nmin[2]);
            }

            float ntop = mNoisePuffTop->mResult[index];
            float nbottom = mNoisePuffBottom->mResult[index];

            if (!(mFlags & OREFLAG_PUFF_CLIFFS)) 
            {
                float ndiff = noiseval - mNoiseThresh;
                if (ndiff < 1.0f) 
                {
                    ntop *= ndiff;
                    nbottom *= ndiff;
                }
            }

            int ymid = yStart;
            int y0 = ymid - (int)nbottom;
            int y1 = ymid + (int)ntop;

            if ((mFlags & OREFLAG_PUFF_ADDITIVE) && (y0 > y1))
                std::swap(y0, y1);

            for (int y = y0; y <= y1; y++) 
            {
                unsigned int i = vm->mArea.Index(x, y, z);
                if (!vm->mArea.Contains(i))
                    continue;

                unsigned short content = vm->mData[i].GetContent();
                if (std::find(mContentWherein.begin(), mContentWherein.end(), content) == mContentWherein.end())
                    continue;

                vm->mData[i] = nodeOre;
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////


Object* OreBlob::Clone() const
{
	auto oreBlob = new OreBlob();
	Ore::CloneTo(oreBlob);
	return oreBlob;
}


void OreBlob::Generate(MMVManip* vm, int mapSeed, unsigned int blockSeed,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	PcgRandom pr(blockSeed + 2404);
	MapNode nodeOre(mContentOre, 0, mOreParam2);

	unsigned int sizex  = (nmax[0] - nmin[0] + 1);
	unsigned int volume = (nmax[0] - nmin[0] + 1) *
        (nmax[1] - nmin[1] + 1) * (nmax[2] - nmin[2] + 1);
	unsigned int csize  = mClustSize;
	unsigned int nblobs = volume / mClustScarcity;

	if (!mNoise)
        mNoise = new Noise(&mNoiseParams, mapSeed, csize, csize, csize);

    for (unsigned int i = 0; i != nblobs; i++)
    {
        int x0 = pr.Range(nmin[0], nmax[0] - csize + 1);
        int y0 = pr.Range(nmin[1], nmax[1] - csize + 1);
        int z0 = pr.Range(nmin[2], nmax[2] - csize + 1);

        if (biomeMap && !mBiomes.empty())
        {
            unsigned int bmapidx = sizex * (z0 - nmin[2]) + (x0 - nmin[0]);
            auto it = mBiomes.find(biomeMap[bmapidx]);
            if (it == mBiomes.end())
                continue;
        }

        bool noiseGenerated = false;
        mNoise->mSeed = blockSeed + i;

        size_t index = 0;
        for (unsigned int z1 = 0; z1 != csize; z1++)
        {
            for (unsigned int y1 = 0; y1 != csize; y1++)
            {
                for (unsigned int x1 = 0; x1 != csize; x1++, index++) 
                {
                    unsigned int i = vm->mArea.Index(x0 + x1, y0 + y1, z0 + z1);

                    unsigned short content = vm->mData[i].GetContent();
                    if (std::find(mContentWherein.begin(), mContentWherein.end(), content) == mContentWherein.end())
                        continue;

                    // Lazily generate noise only if there's a chance of ore being placed
                    // This simple optimization makes calls 6x faster on average
                    if (!noiseGenerated) 
                    {
                        noiseGenerated = true;
                        mNoise->PerlinMap3D((float)x0, (float)y0, (float)z0);
                    }

                    float noiseval = mNoise->mResult[index];

                    float xdist = (float)(x1 - csize / 2);
                    float ydist = (float)(y1 - csize / 2);
                    float zdist = (float)(z1 - csize / 2);

                    noiseval -= std::sqrt(xdist * xdist + ydist * ydist + zdist * zdist) / csize;

                    if (noiseval < mNoiseThresh)
                        continue;

                    vm->mData[i] = nodeOre;
                }
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////


OreVein::~OreVein()
{
	delete mNoise2;
}


Object* OreVein::Clone() const
{
	auto def = new OreVein();
	Ore::CloneTo(def);

	def->mRandomFactor = mRandomFactor;
	def->mNoise2 = nullptr; // cannot be shared, on-demand
	def->mSizeyPrev = mSizeyPrev;

	return def;
}


void OreVein::Generate(MMVManip* vm, int mapSeed, unsigned int blockSeed,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	PcgRandom pr(blockSeed + 520);
	MapNode nodeOre(mContentOre, 0, mOreParam2);

	int sizex = nmax[0] - nmin[0] + 1;
	int sizey = nmax[1] - nmin[1] + 1;
	// Because this ore uses 3D noise the perlinmap Y size can be different in
	// different mapchunks due to ore Y limits. So recreate the noise objects
	// if Y size has changed.
	// Because these noise objects are created multiple times for this ore type
	// it is necessary to 'delete' them here.
	if (!mNoise || sizey != mSizeyPrev) 
    {
		delete mNoise;
		delete mNoise2;
		int sizez = nmax[2] - nmin[2] + 1;
		mNoise  = new Noise(&mNoiseParams, mapSeed, sizex, sizey, sizez);
		mNoise2 = new Noise(&mNoiseParams, mapSeed + 436, sizex, sizey, sizez);
		mSizeyPrev = sizey;
	}

	bool noiseGenerated = false;
	size_t index = 0;
    for (int z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int y = nmin[1]; y <= nmax[1]; y++)
        {
            for (int x = nmin[0]; x <= nmax[0]; x++, index++) 
            {
                unsigned int i = vm->mArea.Index(x, y, z);
                if (!vm->mArea.Contains(i))
                    continue;

                unsigned short content = vm->mData[i].GetContent();
                if (std::find(mContentWherein.begin(), mContentWherein.end(), content) == mContentWherein.end())
                    continue;

                if (biomeMap && !mBiomes.empty()) 
                {
                    unsigned int bmapidx = sizex * (z - nmin[2]) + (x - nmin[0]);
                    auto it = mBiomes.find(biomeMap[bmapidx]);
                    if (it == mBiomes.end())
                        continue;
                }

                // Same lazy generation optimization as in OreBlob
                if (!noiseGenerated) 
                {
                    noiseGenerated = true;
                    mNoise->PerlinMap3D(nmin[0], nmin[1], nmin[2]);
                    mNoise2->PerlinMap3D(nmin[0], nmin[1], nmin[2]);
                }

                // randval ranges from -1..1
                /*
                    Note: can generate values slightly larger than 1
                    but this can't be changed as mapgen must be deterministic accross versions.
                */
                float randval = (float)pr.Next() / float(pr.RANDOM_RANGE / 2) - 1.f;
                float noiseval = Contour(mNoise->mResult[index]);
                float noiseval2 = Contour(mNoise2->mResult[index]);
                if (noiseval * noiseval2 + randval * mRandomFactor < mNoiseThresh)
                    continue;

                vm->mData[i] = nodeOre;
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////


OreStratum::~OreStratum()
{
	delete mNoiseStratumThickness;
}


Object* OreStratum::Clone() const
{
	auto oreStratum = new OreStratum();
	Ore::CloneTo(oreStratum);

    oreStratum->mNoiseParamsStratumThickness = mNoiseParamsStratumThickness;
    oreStratum->mNoiseStratumThickness = nullptr; // cannot be shared, on-demand
    oreStratum->mStratumThickness = mStratumThickness;

	return oreStratum;
}


void OreStratum::Generate(MMVManip* vm, int mapSeed, unsigned int blockSeed,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	PcgRandom pr(blockSeed + 4234);
	MapNode nodeOre(mContentOre, 0, mOreParam2);

	if (mFlags & OREFLAG_USE_NOISE) 
    {
		if (!mNoise) 
        {
			int sx = nmax[0] - nmin[0] + 1;
			int sz = nmax[2] - nmin[2] + 1;
			mNoise = new Noise(&mNoiseParams, 0, sx, sz);
		}
		mNoise->PerlinMap2D(nmin[0], nmin[2]);
	}

	if (mFlags & OREFLAG_USE_NOISE2) 
    {
		if (!mNoiseStratumThickness) 
        {
			int sx = nmax[0] - nmin[0] + 1;
			int sz = nmax[2] - nmin[2] + 1;
			mNoiseStratumThickness = new Noise(&mNoiseParamsStratumThickness, 0, sx, sz);
		}
		mNoiseStratumThickness->PerlinMap2D(nmin[0], nmin[2]);
	}

	size_t index = 0;
    for (int z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int x = nmin[0]; x <= nmax[0]; x++, index++)
        {
            if (biomeMap && ! mBiomes.empty()) 
            {
                auto it = mBiomes.find(biomeMap[index]);
                if (it == mBiomes.end())
                    continue;
            }

            int y0;
            int y1;

            if (mFlags & OREFLAG_USE_NOISE) 
            {
                float nhalfthick = ((mFlags & OREFLAG_USE_NOISE2) ?
                    mNoiseStratumThickness->mResult[index] : (float)mStratumThickness) / 2.0f;
                float nmid = mNoise->mResult[index];
                y0 = std::max(nmin[1], (short)std::ceil(nmid - nhalfthick));
                y1 = std::min(nmax[1], (short)(nmid + nhalfthick));
            }
            else 
            { // Simple horizontal stratum
                y0 = nmin[1];
                y1 = nmax[1];
            }

            for (int y = y0; y <= y1; y++) 
            {
                if (pr.Range(1, mClustScarcity) != 1)
                    continue;

                unsigned int i = vm->mArea.Index(x, y, z);
                if (!vm->mArea.Contains(i))
                    continue;

                unsigned short content = vm->mData[i].GetContent();
                if (std::find(mContentWherein.begin(), mContentWherein.end(), content) == mContentWherein.end())
                    continue;

                vm->mData[i] = nodeOre;
            }
        }
    }
}
