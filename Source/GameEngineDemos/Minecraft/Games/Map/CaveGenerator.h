/*
Minetest
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

#ifndef CAVEGENERATOR_H
#define CAVEGENERATOR_H

#include "GameEngineStd.h"

#define VMANIP_FLAG_CAVE VOXELFLAG_CHECKED1

typedef uint16_t Biometype;  // copy from MapGeneratorBiome.h to avoid an unnecessary include

class GenerateNotifier;

/*
	CavesNoiseIntersection is a cave digging algorithm that carves smooth,
	web-like, continuous tunnels at points where the density of the intersection
	between two separate 3d noises is above a certain value.  This value,
	mCaveWidth, can be modified to set the effective width of these tunnels.

	This algorithm is relatively heavyweight, taking ~80ms to generate an
	80x80x80 chunk of map on a modern processor.  Use sparingly!

	TODO(hmmmm): Remove dependency on biomes
	TODO(hmmmm): Find alternative to overgeneration as solution for sunlight issue
*/
class CavesNoiseIntersection
{
public:
	CavesNoiseIntersection(const NodeManager* nodeMgr,
		BiomeManager* biomeMgr, Vector3<short> chunkSize, NoiseParams* npCave1,
		NoiseParams* npCave2, int seed, float caveWidth);
	~CavesNoiseIntersection();

	void GenerateCaves(MMVManip *vm, Vector3<short> nmin, Vector3<short> nmax, Biometype* mBiomeMap);

private:
	const NodeManager* mNodeMgr;
	BiomeManager* mBiomeMgr;

	// configurable parameters
	Vector3<short> mChunkSize;
	float mCaveWidth;

	// intermediate state variables
	uint16_t mYStride;
	uint16_t mZStride1d;

	Noise* mNoiseCave1;
	Noise* mNoiseCave2;
};

/*
	CavernsNoise is a cave digging algorithm
*/
class CavernsNoise
{
public:
	CavernsNoise(const NodeManager* nodeMgr, Vector3<short> chunkSize,
		NoiseParams* mNoiseParamsCavern, int seed, float mCavernLimit,
		float mCavernTaper, float mCavernThreshold);
	~CavernsNoise();

	bool GenerateCaverns(MMVManip* vm, Vector3<short> nmin, Vector3<short> nmax);

private:
	const NodeManager* mNodeMgr;

	// configurable parameters
	Vector3<short> mChunkSize;
	float mCavernLimit;
	float mCavernTaper;
	float mCavernThreshold;

	// intermediate state variables
	uint16_t mYStride;
	uint16_t mZStride1d;

	Noise* mNoiseCavern;

	uint16_t mContentWaterSource;
	uint16_t mContentLavaSource;
};

/*
	CavesRandomWalk is an implementation of a cave-digging algorithm that
	operates on the principle of a "random walk" to approximate the stochiastic
	activity of cavern development.

	In summary, this algorithm works by carving a randomly sized tunnel in a
	random direction a random amount of times, randomly varying in width.
	All randomness here is uniformly distributed; alternative distributions have
	not yet been implemented.

	This algorithm is very fast, executing in less than 1ms on average for an
	80x80x80 chunk of map on a modern processor.
*/
class CavesRandomWalk
{
public:
	MMVManip* mMMVManip;
	const NodeManager* mNodeMgr;
	GenerateNotifier* mGenNotify;
	int16_t* mHeightmap;
	BiomeGenerator* mBiomeGenerator;

	int mSeed;
	int mWaterLevel;
	float mLargeCaveFlooded;
	// TODO 'mNPCaveliquids' is deprecated and should eventually be removed.
	// Cave liquids are now defined and located using biome definitions.
	NoiseParams* mNPCaveliquids;

	uint16_t mYStride;

	int16_t mMinTunnelDiameter;
	int16_t mMaxTunnelDiameter;
	uint16_t mTunnelRoutepoints;
	int mPartMaxLengthRS;

	bool mLargeCave;
	bool mLargeCaveIsFlat;
	bool mFlooded;
	bool mUseBiomeLiquid;

	Vector3<short> mNodeMin;
	Vector3<short> mNodeMax;

	Vector3<float> mOrp;  // starting point, relative to caved space
	Vector3<short> mOf; // absolute coordinates of caved space
	Vector3<short> mAr; // allowed route area
	int16_t mRs;   // tunnel radius size
	Vector3<float> mMainDirection;

	int16_t mRouteYMin;
	int16_t mRouteYMax;

	PseudoRandom* mRandom;

	uint16_t mContentWaterSource;
	uint16_t mContentLavaSource;
	uint16_t mContentBiomeLiquid;

	// nodeMgr is a mandatory parameter.
	// If mGenNotify is NULL, generation events are not logged.
	// If biomegen is NULL, cave liquids have classic behaviour.
	CavesRandomWalk(const NodeManager* nodeMgr, GenerateNotifier* genNotify = NULL, 
        int seed = 0, int waterLevel = 1, uint16_t waterSource = CONTENT_IGNORE, 
        uint16_t lavaSource = CONTENT_IGNORE, float mLargeCaveFlooded = 0.5f, BiomeGenerator* biomegen = NULL);

	// vm and ps are mandatory parameters.
	// If heightmap is NULL, the surface level at all points is assumed to
	// be mWaterLevel.
	void MakeCave(MMVManip *vm, Vector3<short> nmin, Vector3<short> nmax, PseudoRandom* ps,
			bool isLargeCave, int maxStoneHeight, int16_t* heightmap);

private:
	void MakeTunnel(bool dirSwitch);
	void CarveRoute(Vector3<float> vec, float f, bool randomizeXZ);

	inline bool IsPositionAboveSurface(Vector3<short> pos);
};

/*
	CavesV6 is the original version of caves used with MapGenerator V6.

	Though it uses the same fundamental algorithm as CavesRandomWalk, it is made
	separate to preserve the exact sequence of PseudoRandom calls - any change
	to this ordering results in the output being radically different.
	Because caves in MapGenerator V6 are responsible for a large portion of the basic
	terrain shape, modifying this will break our contract of reverse
	compatibility for a 'stable' mapgen such as V6.

	tl;dr,
	*** DO NOT TOUCH THIS CLASS UNLESS YOU KNOW WHAT YOU ARE DOING ***
*/
class CavesV6
{
public:
	MMVManip* mMMVManip;
	const NodeManager* mNodeMgr;
	GenerateNotifier* mGenNotify;
	PseudoRandom* mRandom;
	PseudoRandom* mRandom2;

	// configurable parameters
	int16_t* mHeightmap;
	uint16_t mContentWaterSource;
	uint16_t mContentLavaSource;
	int mWaterLevel;

	// intermediate state variables
	uint16_t ystride;

	int16_t mMinTunnelDiameter;
	int16_t mMaxTunnelDiameter;
	uint16_t mTunnelRoutepoints;
	int mPartMaxLengthRS;

	bool mLargeCave;
	bool mLargeCaveIsFlat;

	Vector3<short> mNodeMin;
	Vector3<short> mNodeMax;

	Vector3<float> mOrp;  // starting point, relative to caved space
	Vector3<short> mOf; // absolute coordinates of caved space
	Vector3<short> mAr; // allowed route area
	int16_t mRS;   // tunnel radius size
	Vector3<float> mMainDirection;

	int16_t mRouteYMin;
	int16_t mRouteYMax;

	// nodeMgr is a mandatory parameter.
	// If mGenNotify is NULL, generation events are not logged.
	CavesV6(const NodeManager* nodeMgr, GenerateNotifier* genNotify = NULL,
			int mWaterLevel = 1, uint16_t waterSource = CONTENT_IGNORE,
			uint16_t lavaSource = CONTENT_IGNORE);

	// vm, ps, and ps2 are mandatory parameters.
	// If heightmap is NULL, the surface level at all points is assumed to
	// be mWaterLevel.
	void MakeCave(MMVManip* vm, Vector3<short> nmin, Vector3<short> nmax, PseudoRandom* ps,
			PseudoRandom* ps2, bool isLargeCave, int maxStoneHeight, int16_t* heightmap = NULL);

private:
	void MakeTunnel(bool dirSwitch);
	void CarveRoute(Vector3<float> vec, float f, bool randomizeXZ, bool tunnelAboveGround);

	inline int16_t GetSurfaceFromHeightmap(Vector3<short> pos);
};

#endif