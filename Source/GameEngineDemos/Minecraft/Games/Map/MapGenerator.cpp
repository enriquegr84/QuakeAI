/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#include "Map.h"
#include "MapNode.h"
#include "MapBlock.h"
#include "MapGenerator.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorCarpathian.h"
#include "MapGeneratorFlat.h"
#include "MapGeneratorFractal.h"
#include "MapGeneratorV5.h"
#include "MapGeneratorV6.h"
#include "MapGeneratorV7.h"
#include "MapGeneratorValleys.h"
#include "MapGeneratorSingleNode.h"

#include "TreeGenerator.h"
#include "CaveGenerator.h"
#include "DungeonGenerator.h"

#include "Emerge.h"
#include "VoxelAlgorithms.h"
#include "Voxel.h"

#include "../../Graphics/Node.h"

#include "../../Utils/Noise.h"

#include "../../Games/Games.h"

#include "Core/Logger/Logger.h"
#include "Core/Utility/Serialize.h"
#include "Core/Utility/Profiler.h"

#include "Mathematic/Arithmetic/BitHacks.h"

#include "Application/Settings.h"

FlagDescription FlagdescMapGenerator[] = 
{
	{"caves",       MG_CAVES},
	{"dungeons",    MG_DUNGEONS},
	{"light",       MG_LIGHT},
	{"decorations", MG_DECORATIONS},
	{"biomes",      MG_BIOMES},
	{"ores",        MG_ORES},
	{NULL,          0}
};

FlagDescription FlagdescGenNotify[] = 
{
	{"dungeon",          1 << GENNOTIFY_DUNGEON},
	{"temple",           1 << GENNOTIFY_TEMPLE},
	{"cave_begin",       1 << GENNOTIFY_CAVE_BEGIN},
	{"cave_end",         1 << GENNOTIFY_CAVE_END},
	{"mLargeCave_begin", 1 << GENNOTIFY_LARGECAVE_BEGIN},
	{"mLargeCave_end",   1 << GENNOTIFY_LARGECAVE_END},
	{"decoration",       1 << GENNOTIFY_DECORATION},
	{NULL,               0}
};

struct MapGeneratorDesc 
{
	const char* name;
	bool isUserVisible;
};

////
//// Built-in mapgens
////

// Order used here defines the order of appearence in mainmenu.
// v6 always last to discourage selection.
// Special mapgens flat, fractal, singlenode, next to last. Of these, singlenode
// last to discourage selection.
// Of the remaining, v5 last due to age, v7 first due to being the default.
// The order of 'enum MapGeneratorType' in mapgen.h must match this order.
static MapGeneratorDesc RegisteredMapGenerators[] = 
{
	{"v7",         true},
	{"valleys",    true},
	{"carpathian", true},
	{"v5",         true},
	{"flat",       true},
	{"fractal",    true},
	{"singlenode", true},
	{"v6",         true},
};

////
//// MapGenerator
////

MapGenerator::MapGenerator(int mapgenid, MapGeneratorParams* params, EmergeParams* emerge) :
	mGenNotify(emerge->mGenNotifyOn, emerge->mGenNotifyOnDecoIds)
{
	mId = mapgenid;
	mWaterLevel = params->waterLevel;
	mMapgenLimit = params->mapgenLimit;
	mFlags = params->flags;
    mChunkSize = Vector3<short>{ 1, 1, 1 } *(short)(params->chunkSize * MAP_BLOCKSIZE);

	/*
		We are losing half our entropy by doing this, but it is necessary to
		preserve reverse compatibility.  If the top half of our current 64 bit
		seeds ever starts getting used, existing worlds will break due to a
		different hash outcome and no way to differentiate between versions.

		A solution could be to add a new bit to designate that the top half of
		the seed value should be used, essentially a 1-bit version code, but
		this would require increasing the total size of a seed to 9 bytes (yuck)

		It's probably okay if this never gets fixed.  4.2 billion possibilities
		ought to be enough for anyone.
	*/
	mSeed = (int)params->seed;
    mNodeMgr = emerge->mNodeMgr;
}


MapGeneratorType MapGenerator::GetMapGeneratorType(const std::string& mgname)
{
    size_t arrayLen = sizeof(RegisteredMapGenerators) / sizeof(RegisteredMapGenerators[0]);
	for (size_t i = 0; i != arrayLen; i++)
		if (mgname == RegisteredMapGenerators[i].name)
			return (MapGeneratorType)i;

	return MAPGEN_INVALID;
}


const char* MapGenerator::GetMapGeneratorName(MapGeneratorType mgtype)
{
	size_t index = (size_t)mgtype;
    size_t arrayLen = sizeof(RegisteredMapGenerators) / sizeof(RegisteredMapGenerators[0]);
	if (index == MAPGEN_INVALID || index >= arrayLen)
		return "invalid";

	return RegisteredMapGenerators[index].name;
}


MapGenerator* MapGenerator::CreateMapGenerator(MapGeneratorType mgtype, MapGeneratorParams* params,
	EmergeParams* emerge)
{
	switch (mgtype) 
    {
	    case MAPGEN_CARPATHIAN:
		    return new MapGeneratorCarpathian((MapGeneratorCarpathianParams *)params, emerge);
	    case MAPGEN_FLAT:
		    return new MapGeneratorFlat((MapGeneratorFlatParams *)params, emerge);
	    case MAPGEN_FRACTAL:
		    return new MapGeneratorFractal((MapGeneratorFractalParams *)params, emerge);
	    case MAPGEN_SINGLENODE:
		    return new MapGeneratorSinglenode((MapGeneratorSinglenodeParams *)params, emerge);
	    case MAPGEN_V5:
		    return new MapGeneratorV5((MapGeneratorV5Params *)params, emerge);
	    case MAPGEN_V6:
		    return new MapGeneratorV6((MapGeneratorV6Params *)params, emerge);
	    case MAPGEN_V7:
		    return new MapGeneratorV7((MapGeneratorV7Params *)params, emerge);
	    case MAPGEN_VALLEYS:
		    return new MapGeneratorValleys((MapGeneratorValleysParams *)params, emerge);
	    default:
		    return nullptr;
	}
}


MapGeneratorParams* MapGenerator::CreateMapGeneratorParams(MapGeneratorType mgtype)
{
	switch (mgtype) 
    {
	    case MAPGEN_CARPATHIAN:
		    return new MapGeneratorCarpathianParams;
	    case MAPGEN_FLAT:
		    return new MapGeneratorFlatParams;
	    case MAPGEN_FRACTAL:
		    return new MapGeneratorFractalParams;
	    case MAPGEN_SINGLENODE:
		    return new MapGeneratorSinglenodeParams;
	    case MAPGEN_V5:
		    return new MapGeneratorV5Params;
	    case MAPGEN_V6:
		    return new MapGeneratorV6Params;
	    case MAPGEN_V7:
		    return new MapGeneratorV7Params;
	    case MAPGEN_VALLEYS:
		    return new MapGeneratorValleysParams;
	    default:
		    return nullptr;
	}
}


void MapGenerator::GetMapGeneratorNames(std::vector<const char *>* mgnames, bool includeHidden)
{
    size_t arrayLen = sizeof(RegisteredMapGenerators) / sizeof(RegisteredMapGenerators[0]);
	for (unsigned int i = 0; i != arrayLen; i++)
		if (includeHidden || RegisteredMapGenerators[i].isUserVisible)
			mgnames->push_back(RegisteredMapGenerators[i].name);
}

void MapGenerator::SetDefaultSettings(Settings* settings)
{
	settings->SetDefault("mg_flags", FlagdescMapGenerator,
		 MG_CAVES | MG_DUNGEONS | MG_LIGHT | MG_DECORATIONS | MG_BIOMES | MG_ORES);

	for (int i = 0; i < (int)MAPGEN_INVALID; ++i) 
    {
		MapGeneratorParams *params = CreateMapGeneratorParams((MapGeneratorType)i);
		params->SetDefaultSettings(settings);
		delete params;
	}
}

unsigned int MapGenerator::GetBlockSeed(Vector3<short> pos, int seed)
{
	return (unsigned int)seed +
        pos[2] * 38134234 +
        pos[1] * 42123 +
        pos[0] * 23;
}


unsigned int MapGenerator::GetBlockSeed2(Vector3<short> pos, int seed)
{
	unsigned int n = 1619 * pos[0] + 31337 * pos[1] + 52591 * pos[2] + 1013 * seed;
	n = (n >> 13) ^ n;
	return (n * (n * n * 60493 + 19990303) + 1376312589);
}


// Returns -MAX_MAP_GENERATION_LIMIT if not found
short MapGenerator::FindGroundLevel(Vector2<short> p2d, short ymin, short ymax)
{
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	unsigned int i = mMMVManip->mArea.Index(p2d[0], ymax, p2d[1]);
	short y;

	for (y = ymax; y >= ymin; y--) 
    {
		MapNode& n = mMMVManip->mData[i];
		if (mNodeMgr->Get(n).walkable)
			break;

		VoxelArea::AddY(em, i, -1);
	}
	return (y >= ymin) ? y : -MAX_MAP_GENERATION_LIMIT;
}


// Returns -MAX_MAP_GENERATION_LIMIT if not found or if ground is found first
short MapGenerator::FindLiquidSurface(Vector2<short> p2d, short ymin, short ymax)
{
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	unsigned int i = mMMVManip->mArea.Index(p2d[0], ymax, p2d[1]);
	short y;

	for (y = ymax; y >= ymin; y--) 
    {
		MapNode& node = mMMVManip->mData[i];
		if (mNodeMgr->Get(node).walkable)
			return -MAX_MAP_GENERATION_LIMIT;

		if (mNodeMgr->Get(node).IsLiquid())
			break;

		VoxelArea::AddY(em, i, -1);
	}
	return (y >= ymin) ? y : -MAX_MAP_GENERATION_LIMIT;
}


void MapGenerator::UpdateHeightmap(Vector3<short> nmin, Vector3<short> nmax)
{
	if (!mHeightmap)
		return;

	//TimeTaker t("MapGenerator::UpdateHeightmap", NULL, PRECISION_MICRO);
	int index = 0;
	for (short z = nmin[2]; z <= nmax[2]; z++) 
    {
		for (short x = nmin[0]; x <= nmax[0]; x++, index++) 
        {
            short y = FindGroundLevel(Vector2<short>{x, z}, nmin[1], nmax[1]);
			mHeightmap[index] = y;
		}
	}
}


void MapGenerator::GetSurfaces(Vector2<short> p2d, short ymin, short ymax,
	std::vector<short>& floors, std::vector<short>& ceilings)
{
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();

	bool isWalkable = false;
	unsigned int vi = mMMVManip->mArea.Index(p2d[0], ymax, p2d[1]);
	MapNode mnMax = mMMVManip->mData[vi];
	bool walkableAbove = mNodeMgr->Get(mnMax).walkable;
	VoxelArea::AddY(em, vi, -1);

	for (short y = ymax - 1; y >= ymin; y--) 
    {
		MapNode mn = mMMVManip->mData[vi];
		isWalkable = mNodeMgr->Get(mn).walkable;

		if (isWalkable && !walkableAbove) 
        {
			floors.push_back(y);
		}
        else if (!isWalkable && walkableAbove) 
        {
			ceilings.push_back(y + 1);
		}

		VoxelArea::AddY(em, vi, -1);
		walkableAbove = isWalkable;
	}
}


inline bool MapGenerator::IsLiquidHorizontallyFlowable(unsigned int vi, Vector3<short> em)
{
	unsigned int viNegX = vi;
	VoxelArea::AddX(em, viNegX, -1);
	if (mMMVManip->mData[viNegX].GetContent() != CONTENT_IGNORE) 
    {
		const ContentFeatures& contentNx = mNodeMgr->Get(mMMVManip->mData[viNegX]);
		if (contentNx.floodable && !contentNx.IsLiquid())
			return true;
	}
	unsigned int viPosX = vi;
	VoxelArea::AddX(em, viPosX, +1);
	if (mMMVManip->mData[viPosX].GetContent() != CONTENT_IGNORE) 
    {
		const ContentFeatures& contentPx = mNodeMgr->Get(mMMVManip->mData[viPosX]);
		if (contentPx.floodable && !contentPx.IsLiquid())
			return true;
	}
	unsigned int viNegZ = vi;
	VoxelArea::AddZ(em, viNegZ, -1);
	if (mMMVManip->mData[viNegZ].GetContent() != CONTENT_IGNORE) 
    {
		const ContentFeatures& contentNz = mNodeMgr->Get(mMMVManip->mData[viNegZ]);
		if (contentNz.floodable && !contentNz.IsLiquid())
			return true;
	}
	unsigned int viPosZ = vi;
	VoxelArea::AddZ(em, viPosZ, +1);
	if (mMMVManip->mData[viPosZ].GetContent() != CONTENT_IGNORE) 
    {
		const ContentFeatures& contentPz = mNodeMgr->Get(mMMVManip->mData[viPosZ]);
		if (contentPz.floodable && !contentPz.IsLiquid())
			return true;
	}
	return false;
}

void MapGenerator::UpdateLiquid(
    std::queue<Vector3<short>>* transLiquid, Vector3<short> nmin, Vector3<short> nmax)
{
	bool isignored, isliquid, wasignored, wasliquid, waschecked, waspushed;
	const Vector3<short>& em  = mMMVManip->mArea.GetExtent();

    for (short z = nmin[2] + 1; z <= nmax[2] - 1; z++)
    {
        for (short x = nmin[0] + 1; x <= nmax[0] - 1; x++) 
        {
            wasignored = true;
            wasliquid = false;
            waschecked = false;
            waspushed = false;

            unsigned int vi = mMMVManip->mArea.Index(x, nmax[1], z);
            for (short y = nmax[1]; y >= nmin[1]; y--) 
            {
                isignored = mMMVManip->mData[vi].GetContent() == CONTENT_IGNORE;
                isliquid = mNodeMgr->Get(mMMVManip->mData[vi]).IsLiquid();

                if (isignored || wasignored || isliquid == wasliquid) 
                {
                    // Neither topmost node of liquid column nor topmost node below column
                    waschecked = false;
                    waspushed = false;
                }
                else if (isliquid) 
                {
                    // This is the topmost node in the column
                    bool ispushed = false;
                    if (IsLiquidHorizontallyFlowable(vi, em)) 
                    {
                        transLiquid->push(Vector3<short>{x, y, z});
                        ispushed = true;
                    }
                    // Remember waschecked and waspushed to avoid repeated
                    // checks/pushes in case the column consists of only this node
                    waschecked = true;
                    waspushed = ispushed;
                }
                else 
                {
                    // This is the topmost node below a liquid column
                    unsigned int viAbove = vi;
                    VoxelArea::AddY(em, viAbove, 1);
                    if (!waspushed && (mNodeMgr->Get(mMMVManip->mData[vi]).floodable ||
                        (!waschecked && IsLiquidHorizontallyFlowable(viAbove, em)))) 
                    {
                        // Push back the lowest node in the column which is one
                        // node above this one
                        transLiquid->push(Vector3<short>{x, (short)(y + 1), z});
                    }
                }

                wasliquid = isliquid;
                wasignored = isignored;
                VoxelArea::AddY(em, vi, -1);
            }
        }
    }
}


void MapGenerator::SetLighting(uint8_t light, Vector3<short> nmin, Vector3<short> nmax)
{
	ScopeProfiler sp(Profiling, "EmergeThread: update lighting", SPT_AVG);
	VoxelArea a(nmin, nmax);

	for (int z = a.mMinEdge[2]; z <= a.mMaxEdge[2]; z++) 
    {
		for (int y = a.mMinEdge[1]; y <= a.mMaxEdge[1]; y++) 
        {
			unsigned int i = mMMVManip->mArea.Index(a.mMinEdge[0], y, z);
			for (int x = a.mMinEdge[0]; x <= a.mMaxEdge[0]; x++, i++)
                mMMVManip->mData[i].param1 = light;
		}
	}
}


void MapGenerator::LightSpread(VoxelArea& a, 
    std::queue<std::pair<Vector3<short>, uint8_t>>& queue,
	const Vector3<short>& p, uint8_t light)
{
	if (light <= 1 || !a.Contains(p))
		return;

	unsigned int vi = mMMVManip->mArea.Index(p);
	MapNode& n = mMMVManip->mData[vi];

	// Decay light in each of the banks separately
	uint8_t lightDay = light & 0x0F;
	if (lightDay > 0)
		lightDay -= 0x01;

	uint8_t lightNight = light & 0xF0;
	if (lightNight > 0)
		lightNight -= 0x10;

	// Bail out only if we have no more light from either bank to propogate, or
	// we hit a solid block that light cannot pass through.
	if ((lightDay  <= (n.param1 & 0x0F) &&
        lightNight <= (n.param1 & 0xF0)) ||
		!mNodeMgr->Get(n).lightPropagates)
		return;

	// Since this recursive function only terminates when there is no light from
	// either bank left, we need to take the max of both banks into account for
	// the case where spreading has stopped for one light bank but not the other.
	light = std::max(lightDay, (uint8_t)(n.param1 & 0x0F)) |
			std::max(lightNight, (uint8_t)(n.param1 & 0xF0));

	n.param1 = light;

	// add to queue
	queue.emplace(p, light);
}


void MapGenerator::CalculateLighting(Vector3<short> nmin, Vector3<short> nmax, 
    Vector3<short> fullnmin, Vector3<short> fullnmax, bool propagateShadow)
{
	ScopeProfiler sp(Profiling, "EmergeThread: update lighting", SPT_AVG);
	//TimeTaker t("updateLighting");

	PropagateSunlight(nmin, nmax, propagateShadow);
	SpreadLight(fullnmin, fullnmax);

	//printf("updateLighting: %dms\n", t.stop());
}


void MapGenerator::PropagateSunlight(Vector3<short> nmin, Vector3<short> nmax, bool propagateShadow)
{
	//TimeTaker t("PropagateSunlight");
	VoxelArea a(nmin, nmax);
	bool blockIsUnderground = (mWaterLevel >= nmax[1]);
	const Vector3<short>& em = mMMVManip->mArea.GetExtent();

	// NOTE: Direct access to the low 4 bits of param1 is okay here because,
	// by definition, sunlight will never be in the night lightbank.

	for (int z = a.mMinEdge[2]; z <= a.mMaxEdge[2]; z++) 
    {
		for (int x = a.mMinEdge[0]; x <= a.mMaxEdge[0]; x++) 
        {
			// see if we can get a light value from the overtop
			unsigned int i = mMMVManip->mArea.Index(x, a.mMaxEdge[1] + 1, z);
			if (mMMVManip->mData[i].GetContent() == CONTENT_IGNORE)
            {
				if (blockIsUnderground)
					continue;
			} 
            else if ((mMMVManip->mData[i].param1 & 0x0F) != LIGHT_SUN && propagateShadow) 
            {
				continue;
			}
			VoxelArea::AddY(em, i, -1);

			for (int y = a.mMaxEdge[1]; y >= a.mMinEdge[1]; y--) 
            {
				MapNode &n = mMMVManip->mData[i];
				if (!mNodeMgr->Get(n).sunlightPropagates)
					break;
				n.param1 = LIGHT_SUN;
				VoxelArea::AddY(em, i, -1);
			}
		}
	}
	//printf("PropagateSunlight: %dms\n", t.stop());
}

const Vector3<short> Face6D[6] =
{
    // +right, +top, +back
    Vector3<short>{0, 0, 1}, // back
    Vector3<short>{0, 1, 0}, // top
    Vector3<short>{1, 0, 0}, // right
    Vector3<short>{0, 0,-1}, // front
    Vector3<short>{0,-1, 0}, // bottom
    Vector3<short>{-1, 0, 0} // left
};

void MapGenerator::SpreadLight(const Vector3<short> &nmin, const Vector3<short> &nmax)
{
	//TimeTaker t("SpreadLight");
	std::queue<std::pair<Vector3<short>, uint8_t>> queue;
	VoxelArea a(nmin, nmax);

	for (short z = a.mMinEdge[2]; z <= a.mMaxEdge[2]; z++)
    {
		for (short y = a.mMinEdge[1]; y <= a.mMaxEdge[1]; y++) 
        {
			unsigned int i = mMMVManip->mArea.Index(a.mMinEdge[0], y, z);
			for (short x = a.mMinEdge[0]; x <= a.mMaxEdge[0]; x++, i++) 
            {
				MapNode& n = mMMVManip->mData[i];
				if (n.GetContent() == CONTENT_IGNORE)
					continue;

				const ContentFeatures& contentFeatures = mNodeMgr->Get(n);
				if (!contentFeatures.lightPropagates)
					continue;

				// TODO(hmmmmm): Abstract away direct param1 accesses with a
				// wrapper, but something lighter than MapNode::get/setLight

				uint8_t lightProduced = contentFeatures.lightSource;
				if (lightProduced)
					n.param1 = lightProduced | (lightProduced << 4);

				uint8_t light = n.param1;
				if (light) 
                {
                    const Vector3<short> p{ x, y, z };
					// spread to all 6 neighbor nodes
					for (const auto& dir : Face6D)
						LightSpread(a, queue, p + dir, light);
				}
			}
		}
	}

	while (!queue.empty()) 
    {
		const auto& i = queue.front();
		// spread to all 6 neighbor nodes
		for (const auto& dir : Face6D)
			LightSpread(a, queue, i.first + dir, i.second);
		queue.pop();
	}

	//printf("SpreadLight: %lums\n", t.stop());
}


////
//// MapGeneratorBasic
////

MapGeneratorBasic::MapGeneratorBasic(int mapgenid, MapGeneratorParams* params, EmergeParams* emerge)
	: MapGenerator(mapgenid, params, emerge)
{
	this->mEmerge = emerge;
	this->mBiomeMgr = emerge->mBiomeMgr;

	//// Here, 'stride' refers to the number of elements needed to skip to index
	//// an adjacent element for that coordinate in noise/height/biome maps
	//// (*not* vmanip content map!)

	// Note there is no X stride explicitly defined.  Items adjacent in the X
	// coordinate are assumed to be adjacent in memory as well (i.e. stride of 1).

	// Number of elements to skip to get to the next Y coordinate
	this->mYStride = mChunkSize[0];

	// Number of elements to skip to get to the next Z coordinate
	this->mZStride = mChunkSize[0] * mChunkSize[1];

	// Z-stride value for maps oversized for 1-down overgeneration
	this->mZStride1d = mChunkSize[0] * (mChunkSize[1] + 1);

	// Z-stride value for maps oversized for 1-up 1-down overgeneration
	this->mZStride1u1d = mChunkSize[0] * (mChunkSize[1] + 2);

	//// Allocate heightmap
	this->mHeightmap = new short[mChunkSize[0] * mChunkSize[2]];

	//// Initialize biome generator
	mBiomeGenerator = emerge->mBiomeGen;
	mBiomeGenerator->AssertChunkSize(mChunkSize);
	mBiomeMap = mBiomeGenerator->mBiomeMap;

	//// Look up some commonly used content
	mContentStone = mNodeMgr->GetId("mapgen_stone");
	mContentWaterSource = mNodeMgr->GetId("mapgen_water_source");
	mContentRiverWaterSource = mNodeMgr->GetId("mapgen_river_water_source");
	mContentLavaSource = mNodeMgr->GetId("mapgen_lava_source");
	mContentCobble = mNodeMgr->GetId("mapgen_cobble");

	// Fall back to more basic content if not defined.
	// Lava falls back to water as both are suitable as cave liquids.
	if (mContentLavaSource == CONTENT_IGNORE)
		mContentLavaSource = mContentWaterSource;

	if (mContentStone == CONTENT_IGNORE)
		LogWarning("MapGenerator: MapGenerator alias 'mapgenodeStone' is invalid!");
	if (mContentWaterSource == CONTENT_IGNORE)
		LogWarning("MapGenerator: MapGenerator alias 'mapgenodeWaterSource' is invalid!");
	if (mContentRiverWaterSource == CONTENT_IGNORE)
		LogWarning("MapGenerator: MapGenerator alias 'mapgen_river_waterSource' is invalid!");
}


MapGeneratorBasic::~MapGeneratorBasic()
{
	delete []mHeightmap;

	delete mEmerge; // destroying EmergeParams is our responsibility
}


void MapGeneratorBasic::GenerateBiomes()
{
	// can't generate biomes without a biome generator!
	LogAssert(mBiomeGenerator, "invalid biome generator");
    LogAssert(mBiomeMap, "invalid biome type");

	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	unsigned int index = 0;

	mNoiseFillerDepth->PerlinMap2D(mNodeMin[0], mNodeMin[2]);

    for (short z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (short x = mNodeMin[0]; x <= mNodeMax[0]; x++, index++) 
        {
            Biome* biome = NULL;
            Biometype waterBiomeIndex = 0;
            uint16_t depthTop = 0;
            uint16_t baseFiller = 0;
            uint16_t depthWaterTop = 0;
            uint16_t depthRiverbed = 0;
            short biomeYMin = -MAX_MAP_GENERATION_LIMIT;
            unsigned int vi = mMMVManip->mArea.Index(x, mNodeMax[1], z);

            // Check node at base of mapchunk above, either a node of a previously
            // generated mapchunk or if not, a node of overgenerated base terrain.
            uint16_t contentAbove = mMMVManip->mData[vi + em[0]].GetContent();
            bool airAbove = contentAbove == CONTENT_AIR;
            bool riverWaterAbove = contentAbove == mContentRiverWaterSource;
            bool waterAbove = contentAbove == mContentWaterSource || riverWaterAbove;

            mBiomeMap[index] = BIOME_NONE;

            // If there is air or water above enable top/filler placement, otherwise force
            // nplaced to stone level by setting a number exceeding any possible filler depth.
            uint16_t nplaced = (airAbove || waterAbove) ? 0 : 0xFFFF;

            for (short y = mNodeMax[1]; y >= mNodeMin[1]; y--)
            {
                uint16_t content = mMMVManip->mData[vi].GetContent();
                // Biome is (re)calculated:
                // 1. At the surface of stone below air or water.
                // 2. At the surface of water below air.
                // 3. When stone or water is detected but biome has not yet been calculated.
                // 4. When stone or water is detected just below a biome's lower limit.
                bool isStoneSurface = (content == mContentStone) &&
                    (airAbove || waterAbove || !biome || y < biomeYMin); // 1, 3, 4

                bool isWaterSurface =
                    (content == mContentWaterSource || content == mContentRiverWaterSource) &&
                    (airAbove || !biome || y < biomeYMin); // 2, 3, 4

                if (isStoneSurface || isWaterSurface) 
                {
                    // (Re)calculate biome
                    biome = mBiomeGenerator->GetBiomeAtIndex(index, Vector3<short>{x, y, z});

                    // Add biome to mBiomeMap at first stone surface detected
                    if (mBiomeMap[index] == BIOME_NONE && isStoneSurface)
                        mBiomeMap[index] = biome->mIndex;

                    // Store biome of first water surface detected, as a fallback
                    // entry for the mBiomeMap.
                    if (waterBiomeIndex == 0 && isWaterSurface)
                        waterBiomeIndex = biome->mIndex;

                    depthTop = biome->mDepthTop;
                    baseFiller = (uint16_t)std::max(depthTop +
                        biome->mDepthFiller + mNoiseFillerDepth->mResult[index], 0.0f);
                    depthWaterTop = biome->mDepthWaterTop;
                    depthRiverbed = biome->mDepthRiverbed;
                    biomeYMin = biome->mMinPosition[1];
                }

                if (content == mContentStone) 
                {
                    uint16_t contentBelow = mMMVManip->mData[vi - em[0]].GetContent();

                    // If the node below isn't solid, make this node stone, so that
                    // any top/filler nodes above are structurally supported.
                    // This is done by aborting the cycle of top/filler placement
                    // immediately by forcing nplaced to stone level.
                    if (contentBelow == CONTENT_AIR ||
                        contentBelow == mContentWaterSource ||
                        contentBelow == mContentRiverWaterSource)
                    {
                        nplaced = 0xFFFF;
                    }

                    if (riverWaterAbove) 
                    {
                        if (nplaced < depthRiverbed) 
                        {
                            mMMVManip->mData[vi] = MapNode(biome->mContentRiverbed);
                            nplaced++;
                        }
                        else 
                        {
                            nplaced = 0xFFFF;  // Disable top/filler placement
                            riverWaterAbove = false;
                        }
                    }
                    else if (nplaced < depthTop) 
                    {
                        mMMVManip->mData[vi] = MapNode(biome->mContentTop);
                        nplaced++;
                    }
                    else if (nplaced < baseFiller) 
                    {
                        mMMVManip->mData[vi] = MapNode(biome->mContentFiller);
                        nplaced++;
                    }
                    else 
                    {
                        mMMVManip->mData[vi] = MapNode(biome->mContentStone);
                        nplaced = 0xFFFF;  // Disable top/filler placement
                    }

                    airAbove = false;
                    waterAbove = false;
                }
                else if (content == mContentWaterSource) 
                {
                    mMMVManip->mData[vi] = MapNode((y > (int)(mWaterLevel - depthWaterTop))
                        ? biome->mContentWaterTop : biome->mContentWater);
                    nplaced = 0;  // Enable top/filler placement for next surface
                    airAbove = false;
                    waterAbove = true;
                }
                else if (content == mContentRiverWaterSource) 
                {
                    mMMVManip->mData[vi] = MapNode(biome->mContentRiverWater);
                    nplaced = 0;  // Enable riverbed placement for next surface
                    airAbove = false;
                    waterAbove = true;
                    riverWaterAbove = true;
                }
                else if (content == CONTENT_AIR) 
                {
                    nplaced = 0;  // Enable top/filler placement for next surface
                    airAbove = true;
                    waterAbove = false;
                }
                else 
                {  // Possible various nodes overgenerated from neighbouring mapchunks
                    nplaced = 0xFFFF;  // Disable top/filler placement
                    airAbove = false;
                    waterAbove = false;
                }

                VoxelArea::AddY(em, vi, -1);
            }
            // If no stone surface detected in mapchunk column and a water surface
            // biome fallback exists, add it to the mBiomeMap. This avoids water
            // surface decorations failing in deep water.
            if (mBiomeMap[index] == BIOME_NONE && waterBiomeIndex != 0)
                mBiomeMap[index] = waterBiomeIndex;
        }
    }
}


void MapGeneratorBasic::DustTopNodes()
{
	if (mNodeMax[1] < mWaterLevel)
		return;

	const Vector3<short>& em = mMMVManip->mArea.GetExtent();
	unsigned int index = 0;

    for (short z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (short x = mNodeMin[0]; x <= mNodeMax[0]; x++, index++) 
        {
            Biome* biome = (Biome*)mBiomeMgr->GetRaw(mBiomeMap[index]);
            if (biome->mContentDust == CONTENT_IGNORE)
                continue;

            // Check if mapchunk above has generated, if so, drop dust from 16 nodes
            // above current mapchunk top, above decorations that will extend above
            // the current mapchunk. If the mapchunk above has not generated, it
            // will provide this required dust when it does.
            unsigned int vi = mMMVManip->mArea.Index(x, mFullNodeMax[1], z);
            uint16_t contentFullMax = mMMVManip->mData[vi].GetContent();
            short yStart;

            if (contentFullMax == CONTENT_AIR) 
            {
                yStart = mFullNodeMax[1] - 1;
            }
            else if (contentFullMax == CONTENT_IGNORE) 
            {
                vi = mMMVManip->mArea.Index(x, mNodeMax[1] + 1, z);
                uint16_t contentMax = mMMVManip->mData[vi].GetContent();

                if (contentMax == CONTENT_AIR)
                    yStart = mNodeMax[1];
                else
                    continue;
            }
            else continue;

            vi = mMMVManip->mArea.Index(x, yStart, z);
            for (short y = yStart; y >= mNodeMin[1] - 1; y--) 
            {
                if (mMMVManip->mData[vi].GetContent() != CONTENT_AIR)
                    break;

                VoxelArea::AddY(em, vi, -1);
            }

            uint16_t content = mMMVManip->mData[vi].GetContent();
            NodeDrawType dtype = mNodeMgr->Get(content).drawType;
            // Only place on cubic, walkable, non-dust nodes.
            // Dust check needed due to avoid double layer of dust caused by
            // dropping dust from 16 nodes above mapchunk top.
            if ((dtype == NDT_NORMAL ||
                dtype == NDT_ALLFACES ||
                dtype == NDT_ALLFACES_OPTIONAL ||
                dtype == NDT_GLASSLIKE ||
                dtype == NDT_GLASSLIKE_FRAMED ||
                dtype == NDT_GLASSLIKE_FRAMED_OPTIONAL) &&
                mNodeMgr->Get(content).walkable && content != biome->mContentDust) 
            {
                VoxelArea::AddY(em, vi, 1);
                mMMVManip->mData[vi] = MapNode(biome->mContentDust);
            }
        }
    }
}


void MapGeneratorBasic::GenerateCavesNoiseIntersection(short maxStoneY)
{
	// mCaveWidth >= 10 is used to disable generation and avoid the intensive
	// 3D noise calculations. Tunnels already have zero width when mCaveWidth > 1.
	if (mNodeMin[1] > maxStoneY || mCaveWidth >= 10.0f)
		return;

	CavesNoiseIntersection cavesNoise(mNodeMgr, mBiomeMgr, mChunkSize,
		&mNoiseParamsCave1, &mNoiseParamsCave2, mSeed, mCaveWidth);

    cavesNoise.GenerateCaves(mMMVManip, mNodeMin, mNodeMax, mBiomeMap);
}


void MapGeneratorBasic::GenerateCavesRandomWalk(short maxStoneY, short largeCaveYMax)
{
	if (mNodeMin[1] > maxStoneY)
		return;

	PseudoRandom ps(mBlockSeed + 21343);
	// Small randomwalk caves
	unsigned int numSmallCaves = ps.Range(mSmallCaveNumMin, mSmallCaveNumMax);

	for (unsigned int i = 0; i < numSmallCaves; i++) 
    {
		CavesRandomWalk cave(mNodeMgr, &mGenNotify, mSeed, mWaterLevel,
			mContentWaterSource, mContentLavaSource, mLargeCaveFlooded, mBiomeGenerator);
		cave.MakeCave(mMMVManip, mNodeMin, mNodeMax, &ps, false, maxStoneY, mHeightmap);
	}

	if (mNodeMax[1] > largeCaveYMax)
		return;

	// Large randomwalk caves below 'mLargeCave_ymax'.
	// 'mLargeCave_ymax' can differ from the 'mLargeCaveDepth' mapgen parameter,
	// it is set to world base to disable large caves in or near caverns.
	unsigned int numLargeCaves = ps.Range(mLargeCaveNumMin, mLargeCaveNumMax);

	for (unsigned int i = 0; i < numLargeCaves; i++) 
    {
		CavesRandomWalk cave(mNodeMgr, &mGenNotify, mSeed, mWaterLevel,
			mContentWaterSource, mContentLavaSource, mLargeCaveFlooded, mBiomeGenerator);
		cave.MakeCave(mMMVManip, mNodeMin, mNodeMax, &ps, true, maxStoneY, mHeightmap);
	}
}


bool MapGeneratorBasic::GenerateCavernsNoise(short maxStoneY)
{
	if (mNodeMin[1] > maxStoneY || mNodeMin[1] > mCavernLimit)
		return false;

	CavernsNoise cavernsNoise(mNodeMgr, mChunkSize, &mNoiseParamsCavern,
		mSeed, mCavernLimit, mCavernTaper, mCavernThreshold);

	return cavernsNoise.GenerateCaverns(mMMVManip, mNodeMin, mNodeMax);
}


void MapGeneratorBasic::GenerateDungeons(short maxStoneY)
{
	if (mNodeMin[1] > maxStoneY || mNodeMin[1] > mDungeonYmax || mNodeMax[1] < mDungeonYmin)
		return;

	uint16_t numDungeons = (uint16_t)std::fmax(std::floor(
		NoisePerlin3D(&mNoiseParamsDungeons, mNodeMin[0], mNodeMin[1], mNodeMin[2], mSeed)), 0.0f);
	if (numDungeons == 0)
		return;

	PseudoRandom ps(mBlockSeed + 70033);

	DungeonParams dp;
	dp.npAltWall = NoiseParams(-0.4f, 1.f, Vector3<float>{40.f, 40.f, 40.f}, 32474, 6, 1.1f, 2.f);

	dp.seed = mSeed;
	dp.onlyInGround = true;
	dp.numDungeons = numDungeons;
	dp.notifyType = GENNOTIFY_DUNGEON;
	dp.numRooms = ps.Range(2, 16);
    dp.roomSizeMin = Vector3<short>{ 5, 5, 5 };
    dp.roomSizeMax = Vector3<short>{ 12, 6, 12 };
    dp.roomSizeLargeMin = Vector3<short>{ 12, 6, 12 };
    dp.roomSizeLargeMax = Vector3<short>{ 16, 16, 16 };
	dp.largeRoomChance = (ps.Range(1, 4) == 1) ? 8 : 0;
	dp.diagonalDirections = ps.Range(1, 8) == 1;
	// Diagonal corridors must have 'hole' width >=2 to be passable
	uint8_t holewidth = (dp.diagonalDirections) ? 2 : ps.Range(1, 2);
    dp.holeSize = Vector3<short>{ holewidth, 3, holewidth };
	dp.corridorLengthMin = 1;
	dp.corridorLengthMax = 13;

	// Get biome at mapchunk midpoint
    Vector3<short> chunkMid = mNodeMin + (mNodeMax - mNodeMin) / Vector3<short>{2, 2, 2};
	Biome* biome = (Biome*)mBiomeGenerator->GetBiomeAtPoint(chunkMid);

	// Use biome-defined dungeon nodes if defined
	if (biome->mContentDungeon != CONTENT_IGNORE) 
    {
		dp.contentWall = biome->mContentDungeon;
		// If 'node_dungeon_alt' is not defined by biome, it and dp.contentAltWall
		// become CONTENT_IGNORE which skips the alt wall node placement loop in
		// dungeongen.cpp.
		dp.contentAltWall = biome->mContentDungeonAlt;
		// Stairs fall back to 'mContentDungeon' if not defined by biome
		dp.contentStair = (biome->mContentDungeonStair != CONTENT_IGNORE) ?
            biome->mContentDungeonStair : biome->mContentDungeon;
	// Fallback to using cobble mapgen alias if defined
	} 
    else if (mContentCobble != CONTENT_IGNORE) 
    {
		dp.contentWall = mContentCobble;
		dp.contentAltWall = CONTENT_IGNORE;
		dp.contentStair = mContentCobble;
	// Fallback to using biome-defined stone
	} 
    else 
    {
		dp.contentWall = biome->mContentStone;
		dp.contentAltWall = CONTENT_IGNORE;
		dp.contentStair = biome->mContentStone;
	}

	DungeonGen dgen(mNodeMgr, &mGenNotify, &dp);
	dgen.Generate(mMMVManip, mBlockSeed, mFullNodeMin, mFullNodeMax);
}


////
//// GenerateNotifier
////

GenerateNotifier::GenerateNotifier(unsigned int notifyOn, const std::set<unsigned int>* notifyOnDecoIds)
{
	mNotifyOn = notifyOn;
	mNotifyOnDecoIds = notifyOnDecoIds;
}


bool GenerateNotifier::AddEvent(GenNotifyType type, Vector3<short> pos, unsigned int id)
{
	if (!(mNotifyOn & (1 << type)))
		return false;

	if (type == GENNOTIFY_DECORATION &&
		mNotifyOnDecoIds->find(id) == mNotifyOnDecoIds->cend())
		return false;

	GenNotifyEvent gne;
	gne.type = type;
	gne.pos = pos;
	gne.id = id;
	mNotifyEvents.push_back(gne);

	return true;
}


void GenerateNotifier::GetEvents(
	std::map<std::string, std::vector<Vector3<short>>>& eventMap)
{
	std::list<GenNotifyEvent>::iterator it;

	for (it = mNotifyEvents.begin(); it != mNotifyEvents.end(); ++it) 
    {
		GenNotifyEvent& gn = *it;
		std::string name = (gn.type == GENNOTIFY_DECORATION) ?
			"decoration#"+ std::to_string(gn.id) : FlagdescGenNotify[gn.type].name;

		eventMap[name].push_back(gn.pos);
	}
}


void GenerateNotifier::ClearEvents()
{
	mNotifyEvents.clear();
}


////
//// MapGeneratorParams
////


MapGeneratorParams::~MapGeneratorParams()
{
	delete bparams;
}


uint64_t ReadSeed(const char *str)
{
    char* endptr;
    uint64_t num;

    if (str[0] == '0' && str[1] == 'x')
        num = strtoull(str, &endptr, 16);
    else
        num = strtoull(str, &endptr, 10);

    if (*endptr)
        num = MurmurHash64ua(str, (int)strlen(str), 0x1337);

    return num;
}


void MapGeneratorParams::ReadParams(const Settings* settings)
{
	std::string seedStr;
	const char* seedName = (settings == Settings::Get()) ? "fixed_map_seed" : "seed";

    PcgRandom rand;

    try 
    {
        seedStr = settings->Get(seedName);
        if (!seedStr.empty())
            seed = ReadSeed(seedStr.c_str());
        else
            rand.Bytes(&seed, sizeof(seed));
    }
    catch (SettingNotFoundException&)
    {

    }

	std::string mgName;
    try
    {
        mgName = settings->Get("mg_name");
        mgtype = MapGenerator::GetMapGeneratorType(mgName);
        if (mgtype == MAPGEN_INVALID)
            mgtype = MAPGEN_DEFAULT;
    }
    catch (SettingNotFoundException&)
    {

    }

    try
    {
        waterLevel = settings->GetInt16("water_level");
        mapgenLimit = settings->GetInt16("mapgen_limit");
        chunkSize = settings->GetInt16("chunksize");
        settings->GetFlagString("mg_flags", FlagdescMapGenerator, &flags);
    }
    catch (SettingNotFoundException&)
    {

    }

	delete bparams;
	bparams = BiomeManager::CreateBiomeParams(BIOMEGEN_ORIGINAL);
	if (bparams) 
    {
		bparams->ReadParams(settings);
		bparams->seed = (int)seed;
	}
}


void MapGeneratorParams::WriteParams(Settings* settings)
{
	settings->Set("mg_name", MapGenerator::GetMapGeneratorName(mgtype));
	settings->SetUInt64("seed", seed);
	settings->SetInt("water_level", waterLevel);
	settings->SetInt("mapgen_limit", mapgenLimit);
	settings->SetInt("chunksize", chunkSize);
	//settings->SetFlagStr("mg_flags", flags, FlagdescMapGenerator);

	if (bparams)
		bparams->WriteParams(settings);
}


// Calculate exact edges of the outermost mapchunks that are within the
// set 'mMapgenLimit'.
void MapGeneratorParams::CalcMapGeneratorEdges()
{
	// Central chunk offset, in blocks
	short ccoffBlock = -chunkSize / 2;
	// Chunksize, in nodes
	int csizeNode = chunkSize * MAP_BLOCKSIZE;
	// Minp/maxp of central chunk, in nodes
	short ccmin = ccoffBlock * MAP_BLOCKSIZE;
	short ccmax = ccmin + csizeNode - 1;
	// Fullminp/fullmaxp of central chunk, in nodes
	short ccfmin = ccmin - MAP_BLOCKSIZE;
	short ccfmax = ccmax + MAP_BLOCKSIZE;
	// Effective mapgen limit, in blocks
	// Uses same calculation as LogicMap::blockpos_over_mMapgenLimit(Vector3<short> pos)
	short mMapgenLimitBlock = std::clamp(mapgenLimit,
		(short)0, (short)MAX_MAP_GENERATION_LIMIT) / MAP_BLOCKSIZE;
	// Effective mapgen limits, in nodes
	short mapgenLimitMin = -mMapgenLimitBlock * MAP_BLOCKSIZE;
	short mapgenLimitMax = (mMapgenLimitBlock + 1) * MAP_BLOCKSIZE - 1;
	// Number of complete chunks from central chunk fullminp/fullmaxp
	// to effective mapgen limits.
	short numcmin = std::max((ccfmin - mapgenLimitMin) / csizeNode, 0);
	short numcmax = std::max((mapgenLimitMax - ccfmax) / csizeNode, 0);
	// MapGenerator edges, in nodes
	mapgenEdgeMin = ccmin - numcmin * csizeNode;
	mapgenEdgeMax = ccmax + numcmax * csizeNode;

	mapgenEdgesCalculated = true;
}


int MapGeneratorParams::GetSpawnRangeMax()
{
	if (!mapgenEdgesCalculated)
		CalcMapGeneratorEdges();

	return std::min((short)-mapgenEdgeMin, mapgenEdgeMax);
}

bool MapGeneratorParams::GetNoiseParamsFromValue(
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


bool MapGeneratorParams::GetNoiseParamsFromGroup(
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

bool MapGeneratorParams::GetNoiseParams(
    const Settings* settings, const std::string& name, NoiseParams& np) const
{
    if (GetNoiseParamsFromGroup(settings, name, np) ||
        GetNoiseParamsFromValue(settings, name, np))
        return true;
    if (auto parent = settings->GetParent())
        return GetNoiseParams(parent, name, np);

    return false;
}

bool MapGeneratorParams::SetNoiseParams(
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
