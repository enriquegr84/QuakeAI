/*
Minetest
Copyright (C) 2010-2020 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2015-2020 paramat
Copyright (C) 2010-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#include "Map.h"
#include "MapGenerator.h"
#include "MapGeneratorV5.h"
#include "MapGeneratorV6.h"
#include "MapGeneratorV7.h"
#include "MapGeneratorBiome.h"
#include "CaveGenerator.h"

// TODO Remove this. Cave liquids are now defined and located using biome definitions
static NoiseParams NoiseParamsCaveLiquids(0, 1, Vector3<float>{150.f, 150.f, 150.f}, 776, 3, 0.6f, 2.f);


////
//// CavesNoiseIntersection
////

CavesNoiseIntersection::CavesNoiseIntersection(
	const NodeManager* nodeMgr, BiomeManager* biomemgr, Vector3<short> chunkSize,
	NoiseParams* noiseParamsCave1, NoiseParams* noiseParamsCave2, int seed, float caveWidth)
{
	LogAssert(nodeMgr, "invalid node manager");
    LogAssert(biomemgr, "invalid biome manager");

	mNodeMgr = nodeMgr;
	mBiomeMgr = biomemgr;

	mChunkSize = chunkSize;
	mCaveWidth = mCaveWidth;

	mYStride    = mChunkSize[0];
	mZStride1d = mChunkSize[0] * (mChunkSize[1] + 1);

	// Noises are created using 1-down overgeneration
	// A Nx-by-1-by-Nz-sized plane is at the bottom of the desired for
	// re-carving the solid overtop placed for blocking sunlight
	mNoiseCave1 = new Noise(noiseParamsCave1, seed, mChunkSize[0], mChunkSize[1] + 1, mChunkSize[2]);
	mNoiseCave2 = new Noise(noiseParamsCave2, seed, mChunkSize[0], mChunkSize[1] + 1, mChunkSize[2]);
}


CavesNoiseIntersection::~CavesNoiseIntersection()
{
	delete mNoiseCave1;
	delete mNoiseCave2;
}


void CavesNoiseIntersection::GenerateCaves(MMVManip* vm,
	Vector3<short> nmin, Vector3<short> nmax, Biometype* biomeMap)
{
	LogAssert(vm, "invalid vmanip");
	LogAssert(biomeMap, "invalid biome type");

	mNoiseCave1->PerlinMap3D((float)nmin[0], (float)nmin[1] - 1.f, (float)nmin[2]);
	mNoiseCave2->PerlinMap3D((float)nmin[0], (float)nmin[1] - 1.f, (float)nmin[2]);

	const Vector3<short>& em = vm->mArea.GetExtent();
	unsigned int index2d = 0;  // Biomemap index

    for (int16_t z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int16_t x = nmin[0]; x <= nmax[0]; x++, index2d++) 
        {
            bool columnIsOpen = false;  // Is column open to overground
            bool isUnderRiver = false;  // Is column under river water
            bool isUnderTunnel = false;  // Is tunnel or is under tunnel
            bool isTopFillerAbove = false;  // Is top or filler above node
            // Indexes at column top
            unsigned int vi = vm->mArea.Index(x, nmax[1], z);
            unsigned int index3d = (z - nmin[2]) * mZStride1d + mChunkSize[1] * mYStride + (x - nmin[0]);  // 3D noise index
            // Biome of column
            Biome* biome = (Biome*)mBiomeMgr->GetRaw(biomeMap[index2d]);
            uint16_t depthTop = biome->mDepthTop;
            uint16_t baseFiller = depthTop + biome->mDepthFiller;
            uint16_t depthRiverbed = biome->mDepthRiverbed;
            uint16_t nplaced = 0;
            // Don't excavate the overgenerated stone at nmax[1] + 1,
            // this creates a 'roof' over the tunnel, preventing light in
            // tunnels at mapchunk borders when generating mapchunks upwards.
            // This 'roof' is removed when the mapchunk above is generated.
            for (int16_t y = nmax[1]; y >= nmin[1] - 1; y--, index3d -= mYStride, VoxelArea::AddY(em, vi, -1)) 
            {
                uint16_t c = vm->mData[vi].GetContent();

                if (c == CONTENT_AIR || c == biome->mContentWaterTop || c == biome->mContentWater) 
                {
                    columnIsOpen = true;
                    isTopFillerAbove = false;
                    continue;
                }

                if (c == biome->mContentRiverWater) 
                {
                    columnIsOpen = true;
                    isUnderRiver = true;
                    isTopFillerAbove = false;
                    continue;
                }

                // Ground
                float d1 = Contour(mNoiseCave1->mResult[index3d]);
                float d2 = Contour(mNoiseCave2->mResult[index3d]);

                if (d1 * d2 > mCaveWidth && mNodeMgr->Get(c).isGroundContent) 
                {
                    // In tunnel and ground content, excavate
                    vm->mData[vi] = MapNode(CONTENT_AIR);
                    isUnderTunnel = true;
                    // If tunnel roof is top or filler, replace with stone
                    if (isTopFillerAbove)
                        vm->mData[vi + em[0]] = MapNode(biome->mContentStone);
                    isTopFillerAbove = false;
                }
                else if (columnIsOpen && isUnderTunnel &&
                    (c == biome->mContentStone || c == biome->mContentFiller)) 
                {
                    // Tunnel entrance floor, place biome surface nodes
                    if (isUnderRiver) 
                    {
                        if (nplaced < depthRiverbed) 
                        {
                            vm->mData[vi] = MapNode(biome->mContentRiverbed);
                            isTopFillerAbove = true;
                            nplaced++;
                        }
                        else 
                        {
                            // Disable top/filler placement
                            columnIsOpen = false;
                            isUnderRiver = false;
                            isUnderTunnel = false;
                        }
                    }
                    else if (nplaced < depthTop) 
                    {
                        vm->mData[vi] = MapNode(biome->mContentTop);
                        isTopFillerAbove = true;
                        nplaced++;
                    }
                    else if (nplaced < baseFiller) 
                    {
                        vm->mData[vi] = MapNode(biome->mContentFiller);
                        isTopFillerAbove = true;
                        nplaced++;
                    }
                    else 
                    {
                        // Disable top/filler placement
                        columnIsOpen = false;
                        isUnderTunnel = false;
                    }
                }
                else {
                    // Not tunnel or tunnel entrance floor
                    // Check node for possible replacing with stone for tunnel roof
                    if (c == biome->mContentTop || c == biome->mContentFiller)
                        isTopFillerAbove = true;

                    columnIsOpen = false;
                }
            }
        }
    }
}


////
//// CavernsNoise
////

CavernsNoise::CavernsNoise(
	const NodeManager* nodeMgr, Vector3<short> chunkSize, NoiseParams* noiseParamsCavern,
	int seed, float cavernLimit, float cavernTaper, float cavernThreshold)
{
	LogAssert(nodeMgr, "invalid node manager");

	mNodeMgr = nodeMgr;

	mChunkSize = chunkSize;
	mCavernLimit = cavernLimit;
	mCavernTaper = cavernTaper;
	mCavernThreshold = cavernThreshold;

	mYStride = chunkSize[0];
	mZStride1d = chunkSize[0] * (chunkSize[1] + 1);

	// Noise is created using 1-down overgeneration
	// A Nx-by-1-by-Nz-sized plane is at the bottom of the desired for
	// re-carving the solid overtop placed for blocking sunlight
	mNoiseCavern = new Noise(noiseParamsCavern, seed, chunkSize[0], chunkSize[1] + 1, chunkSize[2]);

	mContentWaterSource = mNodeMgr->GetId("mapgen_water_source");
	if (mContentWaterSource == CONTENT_IGNORE)
		mContentWaterSource = CONTENT_AIR;

	mContentLavaSource = mNodeMgr->GetId("mapgen_lava_source");
	if (mContentLavaSource == CONTENT_IGNORE)
		mContentLavaSource = CONTENT_AIR;
}


CavernsNoise::~CavernsNoise()
{
	delete mNoiseCavern;
}


bool CavernsNoise::GenerateCaverns(MMVManip *vm, Vector3<short> nmin, Vector3<short> nmax)
{
	LogAssert(vm, "invalid vmanip");

	// Calculate noise
	mNoiseCavern->PerlinMap3D((float)nmin[0], (float)nmin[1] - 1.f, (float)nmin[2]);

	// Cache cavernAmp values
	float* cavernAmp = new float[mChunkSize[1] + 1];
	unsigned char cavernAmpIndex = 0;  // Index zero at column top
	for (int16_t y = nmax[1]; y >= nmin[1] - 1; y--, cavernAmpIndex++) 
    {
		cavernAmp[cavernAmpIndex] =
			std::min((mCavernLimit - y) / (float)mCavernTaper, 1.0f);
	}

	//// Place nodes
	bool nearCavern = false;
	const Vector3<short> &em = vm->mArea.GetExtent();
	unsigned int index2d = 0;

    for (int16_t z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int16_t x = nmin[0]; x <= nmax[0]; x++, index2d++) 
        {
            // Reset cave_amp index to column top
            cavernAmpIndex = 0;
            // Initial voxelmanip index at column top
            unsigned int vi = vm->mArea.Index(x, nmax[1], z);
            // Initial 3D noise index at column top
            unsigned int index3d = (z - nmin[2]) * mZStride1d + mChunkSize[1] * mYStride + (x - nmin[0]);
            // Don't excavate the overgenerated stone at mNodeMax[1] + 1,
            // this creates a 'roof' over the cavern, preventing light in
            // caverns at mapchunk borders when generating mapchunks upwards.
            // This 'roof' is excavated when the mapchunk above is generated.
            for (int16_t y = nmax[1]; y >= nmin[1] - 1; y--, index3d -= mYStride, VoxelArea::AddY(em, vi, -1),cavernAmpIndex++) 
            {
                uint16_t content = vm->mData[vi].GetContent();
                float noiseAbsampCavern = std::fabs(mNoiseCavern->mResult[index3d]) * cavernAmp[cavernAmpIndex];
                // Disable CavesRandomWalk at a safe distance from caverns
                // to avoid excessively spreading liquids in caverns.
                if (noiseAbsampCavern > mCavernThreshold - 0.1f) 
                {
                    nearCavern = true;
                    if (noiseAbsampCavern > mCavernThreshold && mNodeMgr->Get(content).isGroundContent)
                        vm->mData[vi] = MapNode(CONTENT_AIR);
                }
            }
        }
    }

	delete[] cavernAmp;

	return nearCavern;
}


////
//// CavesRandomWalk
////

CavesRandomWalk::CavesRandomWalk(
    const NodeManager* nodeMgr, GenerateNotifier* genNotify, int seed, 
    int waterLevel, uint16_t waterSource, uint16_t lavaSource, 
    float largeCaveFlooded, BiomeGenerator* biomegen)
{
	LogAssert(nodeMgr, "invalid node manager");

	this->mNodeMgr = nodeMgr;
	this->mGenNotify = genNotify;
	this->mSeed = seed;
	this->mWaterLevel = waterLevel;
	this->mNPCaveliquids = &NoiseParamsCaveLiquids;
	this->mLargeCaveFlooded = largeCaveFlooded;
	this->mBiomeGenerator = biomegen;

	mContentWaterSource = waterSource;
	if (mContentWaterSource == CONTENT_IGNORE)
		mContentWaterSource = nodeMgr->GetId("mapgen_water_source");
	if (mContentWaterSource == CONTENT_IGNORE)
		mContentWaterSource = CONTENT_AIR;

	mContentLavaSource = lavaSource;
	if (mContentLavaSource == CONTENT_IGNORE)
		mContentLavaSource = nodeMgr->GetId("mapgen_lava_source");
	if (mContentLavaSource == CONTENT_IGNORE)
		mContentLavaSource = CONTENT_AIR;
}


void CavesRandomWalk::MakeCave(MMVManip* vm, Vector3<short> nmin, Vector3<short> nmax,
    PseudoRandom* ps, bool isLargeCave, int maxStoneHeight, int16_t* heightmap)
{
    LogAssert(vm, "invalid vmanip");
    LogAssert(ps, "invalid randomizer");

    this->mMMVManip = vm;
    this->mRandom = ps;
    this->mNodeMin = nmin;
    this->mNodeMax = nmax;
    this->mHeightmap = heightmap;
    this->mLargeCave = isLargeCave;

    this->mYStride = nmax[0] - nmin[0] + 1;

    mFlooded = ps->Range(1, 1000) <= mLargeCaveFlooded * 1000.0f;

    // If flooded:
    // Get biome at mapchunk midpoint. If cave liquid defined for biome, use it.
    // If defined liquid is "air", disable 'flooded' to avoid placing "air".
    mUseBiomeLiquid = false;
    if (mFlooded && mBiomeGenerator)
    {
        Vector3<short> midp = mNodeMin + (mNodeMax - mNodeMin) / Vector3<short>{2, 2, 2};
        Biome* biome = (Biome*)mBiomeGenerator->GetBiomeAtPoint(midp);
        if (biome->mContentCaveLiquid[0] != CONTENT_IGNORE)
        {
            mUseBiomeLiquid = true;
            mContentBiomeLiquid =
                biome->mContentCaveLiquid[ps->Range(0, (int)biome->mContentCaveLiquid.size() - 1)];
            if (mContentBiomeLiquid == CONTENT_AIR)
                mFlooded = false;
        }
    }

    // Set initial parameters from randomness
    int dswitchint = ps->Range(1, 14);

    if (mLargeCave)
    {
        mPartMaxLengthRS = ps->Range(2, 4);
        mTunnelRoutepoints = ps->Range(5, ps->Range(15, 30));
        mMinTunnelDiameter = 5;
        mMaxTunnelDiameter = ps->Range(7, ps->Range(8, 24));
    }
    else
    {
        mPartMaxLengthRS = ps->Range(2, 9);
        mTunnelRoutepoints = ps->Range(10, ps->Range(15, 30));
        mMinTunnelDiameter = 2;
        mMaxTunnelDiameter = ps->Range(2, 6);
    }

    mLargeCaveIsFlat = (ps->Range(0, 1) == 0);

    mMainDirection = Vector3<float>::Zero();

    // Allowed route area size in nodes
    mAr = mNodeMax - mNodeMin + Vector3<short>{ 1, 1, 1 };
    // Area starting point in nodes
    mOf = mNodeMin;

    // Allow caves to extend up to 16 nodes beyond the mapchunk edge, to allow
    // connecting with caves of neighbor mapchunks.
    // 'insure' is needed to avoid many 'out of voxelmanip' cave nodes.
    const short insure = 2;
    short more = std::max(MAP_BLOCKSIZE - mMaxTunnelDiameter / 2 - insure, 1);
    mAr += Vector3<short>{1, 1, 1} * more * (short)2;
    mOf -= Vector3<short>{1, 1, 1} * more;

	mRouteYMin = 0;
	// Allow half a diameter + 7 over stone surface
	mRouteYMax = -mOf[1] + maxStoneHeight + mMaxTunnelDiameter / 2 + 7;

	// Limit maximum to area
	mRouteYMax = std::clamp(mRouteYMax, (short)0, (short)(mAr[1] - 1));

	if (mLargeCave) 
    {
		int16_t minpos = 0;
		if (mNodeMin[1] < mWaterLevel && mNodeMax[1] > mWaterLevel) 
        {
			minpos = mWaterLevel - mMaxTunnelDiameter / 3 - mOf[1];
			mRouteYMax = mWaterLevel + mMaxTunnelDiameter / 3 - mOf[1];
		}
		mRouteYMin = ps->Range(minpos, minpos + mMaxTunnelDiameter);
		mRouteYMin = std::clamp(mRouteYMin, (short)0, mRouteYMax);
	}

	int16_t routeStartYMin = mRouteYMin;
	int16_t routeStartYMax = mRouteYMax;

	routeStartYMin = std::clamp(routeStartYMin, (short)0, (short)(mAr[1] - 1));
	routeStartYMax = std::clamp(routeStartYMax, routeStartYMin, (short)(mAr[1] - 1));

	// Randomize starting position
	mOrp[2] = (float)(ps->Next() % mAr[2]) + 0.5f;
    mOrp[1] = (float)(ps->Range(routeStartYMin, routeStartYMax)) + 0.5f;
    mOrp[0] = (float)(ps->Next() % mAr[0]) + 0.5f;

	// Add generation notify begin event
	if (mGenNotify) 
    {
        Vector3<short> absPosition{ (
            short)(mOf[0] + mOrp[0]), (short)(mOf[1] + mOrp[1]), (short)(mOf[2] + mOrp[2]) };
		GenNotifyType notifyType = mLargeCave ?
			GENNOTIFY_LARGECAVE_BEGIN : GENNOTIFY_CAVE_BEGIN;
		mGenNotify->AddEvent(notifyType, absPosition);
	}

	// Generate some tunnel starting from orp
	for (uint16_t j = 0; j < mTunnelRoutepoints; j++)
		MakeTunnel(j % dswitchint == 0);

	// Add generation notify end event
	if (mGenNotify) 
    {
        Vector3<short> absPosition{ (
            short)(mOf[0] + mOrp[0]), (short)(mOf[1] + mOrp[1]), (short)(mOf[2] + mOrp[2]) };
		GenNotifyType notifyType = mLargeCave ?
			GENNOTIFY_LARGECAVE_END : GENNOTIFY_CAVE_END;
		mGenNotify->AddEvent(notifyType, absPosition);
	}
}


void CavesRandomWalk::MakeTunnel(bool dirSwitch)
{
	if (dirSwitch && !mLargeCave) 
    {
		mMainDirection[2] = ((float)(mRandom->Next() % 20) - (float)10) / 10;
		mMainDirection[1] = ((float)(mRandom->Next() % 20) - (float)10) / 30;
		mMainDirection[0] = ((float)(mRandom->Next() % 20) - (float)10) / 10;

		mMainDirection *= (float)mRandom->Range(0, 10) / 10;
	}

	// Randomize size
	int16_t minDiameter = mMinTunnelDiameter;
	int16_t maxDiameter = mMaxTunnelDiameter;
	mRs = mRandom->Range(minDiameter, maxDiameter);
	int16_t partMaxLengthRS = mRs * mPartMaxLengthRS;

	Vector3<short> maxlen;
    if (mLargeCave) 
    {
        maxlen = Vector3<short>{
            partMaxLengthRS, (short)(partMaxLengthRS / 2), partMaxLengthRS };
	}
    else 
    {
        maxlen = Vector3<short>{
            partMaxLengthRS, (short)mRandom->Range(1, partMaxLengthRS), partMaxLengthRS };
	}

	Vector3<float> vec;
	// Jump downward sometimes
	if (!mLargeCave && mRandom->Range(0, 12) == 0) 
    {
		vec[2] = (float)(mRandom->Next() % (maxlen[2] * 1)) - (float)maxlen[2] / 2;
		vec[1] = (float)(mRandom->Next() % (maxlen[1] * 2)) - (float)maxlen[1];
		vec[0] = (float)(mRandom->Next() % (maxlen[0] * 1)) - (float)maxlen[0] / 2;
	} 
    else 
    {
		vec[2] = (float)(mRandom->Next() % (maxlen[2] * 1)) - (float)maxlen[2] / 2;
		vec[1] = (float)(mRandom->Next() % (maxlen[1] * 1)) - (float)maxlen[1] / 2;
		vec[0] = (float)(mRandom->Next() % (maxlen[0] * 1)) - (float)maxlen[0] / 2;
	}

	// Do not make caves that are above ground.
	// It is only necessary to check the startpoint and endpoint.
    Vector3<short> p1 = Vector3<short>{ (short)mOrp[0], (short)mOrp[1], (short)mOrp[2] } + mOf + Vector3<short>{(short)(mRs / 2)};
    Vector3<short> p2 = Vector3<short>{ (short)vec[0], (short)vec[1], (short)vec[2] } + p1;
	if (IsPositionAboveSurface(p1) || IsPositionAboveSurface(p2))
		return;

	vec += mMainDirection;

	Vector3<float> rp = mOrp + vec;
	if (rp[0] < 0)
		rp[0] = 0;
	else if (rp[0] >= mAr[0])
		rp[0] = (float)mAr[0] - 1.f;

	if (rp[1] < mRouteYMin)
		rp[1] = mRouteYMin;
	else if (rp[1] >= mRouteYMax)
		rp[1] = (float)mRouteYMax - 1.f;

	if (rp[2] < 0)
		rp[2] = 0;
	else if (rp[2] >= mAr[2])
		rp[2] = (float)mAr[2] - 1.f;

	vec = rp - mOrp;

	float veclen = Length(vec);
	if (veclen < 0.05f)
		veclen = 1.0f;

	// Every second section is rough
	bool randomizeXZ = (mRandom->Range(1, 2) == 1);

	// Carve routes
	for (float f = 0.f; f < 1.0f; f += 1.0f / veclen)
		CarveRoute(vec, f, randomizeXZ);

	mOrp = rp;
}


void CavesRandomWalk::CarveRoute(Vector3<float> vec, float f, bool randomizeXZ)
{
	MapNode airnode(CONTENT_AIR);
	MapNode waternode(mContentWaterSource);
	MapNode lavanode(mContentLavaSource);

    Vector3<short> startp{ (short)mOrp[0], (short)mOrp[1], (short)mOrp[2] };
	startp += mOf;

	Vector3<float> fp = mOrp + vec * f;
	fp[0] += 0.1f * mRandom->Range(-10, 10);
	fp[2] += 0.1f * mRandom->Range(-10, 10);
    Vector3<short> cp{ (short)fp[0], (short)fp[1], (short)fp[2] };

	// Choose cave liquid
	MapNode liquidnode = CONTENT_IGNORE;
	if (mFlooded) 
    {
		if (mUseBiomeLiquid) 
        {
			liquidnode = mContentBiomeLiquid;
		} 
        else 
        {
			// If cave liquid not defined by biome, fallback to old hardcoded behaviour.
			// TODO 'mNPCaveliquids' is deprecated and should eventually be removed.
			// Cave liquids are now defined and located using biome definitions.
			float nval = NoisePerlin3D(mNPCaveliquids, startp[0], startp[1], startp[2], mSeed);
			liquidnode = (nval < 0.40f && mNodeMax[1] < mWaterLevel - 256) ? lavanode : waternode;
		}
	}

	int16_t d0 = -mRs / 2;
	int16_t d1 = d0 + mRs;
	if (randomizeXZ) 
    {
		d0 += mRandom->Range(-1, 1);
		d1 += mRandom->Range(-1, 1);
	}

	bool flatCaveFloor = !mLargeCave && mRandom->Range(0, 2) == 2;
	for (int16_t z0 = d0; z0 <= d1; z0++) 
    {
		int16_t si = mRs / 2 - std::max(0, abs(z0) - mRs / 7 - 1);
		for (int16_t x0 = -si - mRandom->Range(0,1); x0 <= si - 1 + mRandom->Range(0,1); x0++) 
        {
			int16_t maxabsxz = std::max(abs(x0), abs(z0));

			int16_t si2 = mRs / 2 - std::max(0, maxabsxz - mRs / 7 - 1);
			for (int16_t y0 = -si2; y0 <= si2; y0++) 
            {
				// Make better floors in small caves
				if (flatCaveFloor && y0 <= -mRs / 2 && mRs <= 7)
					continue;

				if (mLargeCaveIsFlat)
                {
					// Make large caves not so tall
					if (mRs > 7 && abs(y0) >= mRs / 3)
						continue;
				}

                Vector3<short> p{ (short)(cp[0] + x0), (short)(cp[1] + y0), (short)(cp[2] + z0) };
				p += mOf;
                
				if (!mMMVManip->mArea.Contains(p))
					continue;

				unsigned int i = mMMVManip->mArea.Index(p);
				uint16_t content = mMMVManip->mData[i].GetContent();
				if (!mNodeMgr->Get(content).isGroundContent)
					continue;

				if (mLargeCave) 
                {
					int fullYmin = mNodeMin[1] - MAP_BLOCKSIZE;
					int fullYmax = mNodeMax[1] + MAP_BLOCKSIZE;

					if (mFlooded && fullYmin < mWaterLevel && fullYmax > mWaterLevel)
                        mMMVManip->mData[i] = (p[1] <= mWaterLevel) ? waternode : airnode;
					else if (mFlooded && fullYmax < mWaterLevel)
                        mMMVManip->mData[i] = (p[1] < startp[1] - 4) ? liquidnode : airnode;
					else
                        mMMVManip->mData[i] = airnode;
				} 
                else 
                {
                    mMMVManip->mData[i] = airnode;
                    mMMVManip->mFlags[i] |= VMANIP_FLAG_CAVE;
				}
			}
		}
	}
}


inline bool CavesRandomWalk::IsPositionAboveSurface(Vector3<short> pos)
{
	if (mHeightmap != NULL &&
        pos[2] >= mNodeMin[2] && pos[2] <= mNodeMax[2] &&
        pos[0] >= mNodeMin[0] && pos[0] <= mNodeMax[0]) 
    {
		unsigned int index = (pos[2] - mNodeMin[2]) * mYStride + (pos[0] - mNodeMin[0]);
		if (mHeightmap[index] < pos[1])
			return true;
	}
    else if (pos[1] > mWaterLevel)
    {
		return true;
	}

	return false;
}


////
//// CavesV6
////

CavesV6::CavesV6(const NodeManager* nodeMgr, GenerateNotifier* genNotify,
	int waterLevel, uint16_t waterSource, uint16_t lavaSource)
{
	LogAssert(nodeMgr, "invalid node manager");

	this->mNodeMgr = nodeMgr;
	this->mGenNotify = genNotify;
	this->mWaterLevel = waterLevel;

	mContentWaterSource = waterSource;
	if (mContentWaterSource == CONTENT_IGNORE)
		mContentWaterSource = nodeMgr->GetId("mapgen_water_source");
	if (mContentWaterSource == CONTENT_IGNORE)
		mContentWaterSource = CONTENT_AIR;

	mContentLavaSource = lavaSource;
	if (mContentLavaSource == CONTENT_IGNORE)
		mContentLavaSource = nodeMgr->GetId("mapgen_lava_source");
	if (mContentLavaSource == CONTENT_IGNORE)
		mContentLavaSource = CONTENT_AIR;
}


void CavesV6::MakeCave(MMVManip* vm, Vector3<short> nmin, Vector3<short> nmax,
	PseudoRandom* rand, PseudoRandom* rand2, bool isLargeCave, int maxStoneHeight, int16_t* heightmap)
{
	LogAssert(vm, "invalid vmanip");
    LogAssert(rand, "invalid randomizer");
    LogAssert(rand2, "invalid randomizer");

	this->mMMVManip = vm;
	this->mRandom = rand;
	this->mRandom2 = rand2;
	this->mNodeMin = nmin;
	this->mNodeMax = nmax;
	this->mHeightmap = heightmap;
	this->mLargeCave = isLargeCave;

	this->ystride = nmax[0] - nmin[0] + 1;

	// Set initial parameters from randomness
	mMinTunnelDiameter = 2;
	mMaxTunnelDiameter = mRandom->Range(2, 6);
	int dswitchint = mRandom->Range(1, 14);
	if (mLargeCave) 
    {
		mPartMaxLengthRS  = mRandom->Range(2, 4);
		mTunnelRoutepoints  = mRandom->Range(5, mRandom->Range(15, 30));
		mMinTunnelDiameter = 5;
		mMaxTunnelDiameter = mRandom->Range(7, mRandom->Range(8, 24));
	} 
    else 
    {
		mPartMaxLengthRS = mRandom->Range(2, 9);
		mTunnelRoutepoints = mRandom->Range(10, mRandom->Range(15, 30));
	}
	mLargeCaveIsFlat = (mRandom->Range(0, 1) == 0);

    mMainDirection = Vector3<float>::Zero();

	// Allowed route area size in nodes
    mAr = mNodeMax - mNodeMin + Vector3<short>{1, 1, 1};
	// Area starting point in nodes
	mOf = mNodeMin;

	// Allow a bit more
	//(this should be more than the maximum radius of the tunnel)
	const int16_t maxSpreadAmount = MAP_BLOCKSIZE;
	const int16_t insure = 10;
	int16_t more = std::max(maxSpreadAmount - mMaxTunnelDiameter / 2 - insure, 1);
    mAr += Vector3<short>{1, 0, 1} * more * (short)2;
    mOf -= Vector3<short>{1, 0, 1} * more;

	mRouteYMin = 0;
	// Allow half a diameter + 7 over stone surface
	mRouteYMax = -mOf[1] + maxStoneHeight + mMaxTunnelDiameter / 2 + 7;

	// Limit maximum to area
	mRouteYMax = std::clamp(mRouteYMax, (short)0, (short)(mAr[1] - 1));

	if (mLargeCave) 
    {
		int16_t minpos = 0;
		if (mNodeMin[1] < mWaterLevel && mNodeMax[1] > mWaterLevel) 
        {
			minpos = mWaterLevel - mMaxTunnelDiameter / 3 - mOf[1];
			mRouteYMax = mWaterLevel + mMaxTunnelDiameter / 3 - mOf[1];
		}
		mRouteYMin = mRandom->Range(minpos, minpos + mMaxTunnelDiameter);
		mRouteYMin = std::clamp(mRouteYMin, (short)0, mRouteYMax);
	}

	int16_t routeStartYMin = mRouteYMin;
	int16_t routeStartYMax = mRouteYMax;

	routeStartYMin = std::clamp(routeStartYMin, (short)0, (short)(mAr[1] - 1));
	routeStartYMax = std::clamp(routeStartYMax, routeStartYMin, (short)(mAr[1] - 1));

	// Randomize starting position
	mOrp[2] = (float)(mRandom->Next() % mAr[2]) + 0.5f;
    mOrp[1] = (float)(mRandom->Range(routeStartYMin, routeStartYMax)) + 0.5f;
    mOrp[0] = (float)(mRandom->Next() % mAr[0]) + 0.5f;

	// Add generation notify begin event
	if (mGenNotify != NULL) 
    {
        Vector3<short> absPosition{ (short)(mOf[0] + mOrp[0]), 
            (short)(mOf[1] + mOrp[1]), (short)(mOf[2] + mOrp[2]) };
		GenNotifyType notifyType = mLargeCave ?
			GENNOTIFY_LARGECAVE_BEGIN : GENNOTIFY_CAVE_BEGIN;
		mGenNotify->AddEvent(notifyType, absPosition);
	}

	// Generate some tunnel starting from orp
	for (uint16_t j = 0; j < mTunnelRoutepoints; j++)
		MakeTunnel(j % dswitchint == 0);

	// Add generation notify end event
	if (mGenNotify != NULL) 
    {
        Vector3<short> absPosition{ (short)(mOf[0] + mOrp[0]),
            (short)(mOf[1] + mOrp[1]), (short)(mOf[2] + mOrp[2]) };
		GenNotifyType notifyType = mLargeCave ?
			GENNOTIFY_LARGECAVE_END : GENNOTIFY_CAVE_END;
		mGenNotify->AddEvent(notifyType, absPosition);
	}
}


void CavesV6::MakeTunnel(bool dirSwitch)
{
	if (dirSwitch && !mLargeCave) 
    {
		mMainDirection[2] = ((float)(mRandom->Next() % 20) - (float)10) / 10;
		mMainDirection[1] = ((float)(mRandom->Next() % 20) - (float)10) / 30;
		mMainDirection[0] = ((float)(mRandom->Next() % 20) - (float)10) / 10;

		mMainDirection *= (float)mRandom->Range(0, 10) / 10;
	}

	// Randomize size
	int16_t minDiameter = mMinTunnelDiameter;
	int16_t maxDiameter = mMaxTunnelDiameter;
	mRS = mRandom->Range(minDiameter, maxDiameter);
	int16_t partMaxLengthRS = mRS * mPartMaxLengthRS;

	Vector3<short> maxlen;
	if (mLargeCave) 
    {
        maxlen = Vector3<short>{
            partMaxLengthRS, (short)(partMaxLengthRS / 2), partMaxLengthRS };
	} 
    else 
    {
        maxlen = Vector3<short>{
            partMaxLengthRS, (short)(mRandom->Range(1, partMaxLengthRS)), partMaxLengthRS };
	}

	Vector3<float> vec;
	vec[2] = (float)(mRandom->Next() % maxlen[2]) - (float)maxlen[2] / 2;
	vec[1] = (float)(mRandom->Next() % maxlen[1]) - (float)maxlen[1] / 2;
	vec[0] = (float)(mRandom->Next() % maxlen[0]) - (float)maxlen[0] / 2;

	// Jump downward sometimes
	if (!mLargeCave && mRandom->Range(0, 12) == 0) 
    {
		vec[2] = (float)(mRandom->Next() % maxlen[2]) - (float)maxlen[2] / 2;
		vec[1] = (float)(mRandom->Next() % (maxlen[1] * 2)) - (float)maxlen[1];
		vec[0] = (float)(mRandom->Next() % maxlen[0]) - (float)maxlen[0] / 2;
	}

	// Do not make caves that are entirely above ground, to fix shadow bugs
	// caused by overgenerated large caves.
	// It is only necessary to check the startpoint and endpoint.
    Vector3<short> p1 = Vector3<short>{ (short)mOrp[0], (short)mOrp[1], (short)mOrp[2] } + 
        mOf + Vector3<short>{ (short)(mRS / 2)};
    Vector3<short> p2 = Vector3<short>{ (short)vec[0], (short)vec[1], (short)vec[2] } + p1;

	// If startpoint and endpoint are above ground, disable placement of nodes
	// in CarveRoute while still running all PseudoRandom calls to ensure caves
	// are consistent with existing worlds.
	bool tunnelAboveGround =
		p1[1] > GetSurfaceFromHeightmap(p1) &&
		p2[1] > GetSurfaceFromHeightmap(p2);

	vec += mMainDirection;

	Vector3<float> rp = mOrp + vec;
	if (rp[0] < 0)
		rp[0] = 0;
	else if (rp[0] >= mAr[0])
		rp[0] = (float)mAr[0] - 1.f;

	if (rp[1] < mRouteYMin)
		rp[1] = (float)mRouteYMin;
	else if (rp[1] >= mRouteYMax)
		rp[1] = (float)mRouteYMax - 1.f;

	if (rp[2] < 0)
		rp[2] = 0;
	else if (rp[2] >= mAr[2])
		rp[2] = (float)mAr[2] - 1.f;

	vec = rp - mOrp;

	float veclen = Length(vec);
	// As odd as it sounds, veclen is *exactly* 0.0 sometimes, causing a FPE
	if (veclen < 0.05f)
		veclen = 1.0f;

	// Every second section is rough
	bool randomizeXZ = (mRandom2->Range(1, 2) == 1);

	// Carve routes
	for (float f = 0.f; f < 1.0f; f += 1.0f / veclen)
		CarveRoute(vec, f, randomizeXZ, tunnelAboveGround);

	mOrp = rp;
}


void CavesV6::CarveRoute(Vector3<float> vec, float f, bool randomizeXZ, bool tunnelAboveGround)
{
	MapNode airnode(CONTENT_AIR);
	MapNode waternode(mContentWaterSource);
	MapNode lavanode(mContentLavaSource);

    Vector3<short> startp{ (short)mOrp[0], (short)mOrp[1], (short)mOrp[2] };
	startp += mOf;

	Vector3<float> fp = mOrp + vec * f;
	fp[0] += 0.1f * mRandom->Range(-10, 10);
	fp[2] += 0.1f * mRandom->Range(-10, 10);
    Vector3<short> cp{ (short)fp[0], (short)fp[1], (short)fp[2] };

	int16_t d0 = -mRS / 2;
	int16_t d1 = d0 + mRS;
	if (randomizeXZ) 
    {
		d0 += mRandom->Range(-1, 1);
		d1 += mRandom->Range(-1, 1);
	}

	for (int16_t z0 = d0; z0 <= d1; z0++) 
    {
		int16_t si = mRS / 2 - std::max(0, abs(z0) - mRS / 7 - 1);
		for (int16_t x0 = -si - mRandom->Range(0,1); x0 <= si - 1 + mRandom->Range(0,1); x0++) 
        {
			if (tunnelAboveGround)
				continue;

			int16_t maxabsxz = std::max(abs(x0), abs(z0));
			int16_t si2 = mRS / 2 - std::max(0, maxabsxz - mRS / 7 - 1);
			for (int16_t y0 = -si2; y0 <= si2; y0++) 
            {
				if (mLargeCaveIsFlat) 
                {
					// Make large caves not so tall
					if (mRS > 7 && abs(y0) >= mRS / 3)
						continue;
				}

                Vector3<short> p{ (short)(cp[0] + x0), (short)(cp[1] + y0), (short)(cp[2] + z0) };
				p += mOf;
                
				if (!mMMVManip->mArea.Contains(p))
					continue;

				unsigned int i = mMMVManip->mArea.Index(p);
				uint16_t content = mMMVManip->mData[i].GetContent();
				if (!mNodeMgr->Get(content).isGroundContent)
					continue;

				if (mLargeCave) 
                {
					int fullYmin = mNodeMin[1] - MAP_BLOCKSIZE;
					int fullYmax = mNodeMax[1] + MAP_BLOCKSIZE;

					if (fullYmin < mWaterLevel && fullYmax > mWaterLevel) 
                    {
                        mMMVManip->mData[i] = (p[1] <= mWaterLevel) ? waternode : airnode;
					} 
                    else if (fullYmax < mWaterLevel) 
                    {
                        mMMVManip->mData[i] = (p[1] < startp[1] - 2) ? lavanode : airnode;
					} 
                    else 
                    {
                        mMMVManip->mData[i] = airnode;
					}
				} else 
                {
					if (content == CONTENT_AIR)
						continue;

                    mMMVManip->mData[i] = airnode;
                    mMMVManip->mFlags[i] |= VMANIP_FLAG_CAVE;
				}
			}
		}
	}
}


inline int16_t CavesV6::GetSurfaceFromHeightmap(Vector3<short> pos)
{
	if (mHeightmap != NULL &&
        pos[2] >= mNodeMin[2] && pos[2] <= mNodeMax[2] &&
        pos[0] >= mNodeMin[0] && pos[0] <= mNodeMax[0])
    {
		unsigned int index = (pos[2] - mNodeMin[2]) * ystride + (pos[0] - mNodeMin[0]);
		return mHeightmap[index];
	}

	return mWaterLevel;

}
