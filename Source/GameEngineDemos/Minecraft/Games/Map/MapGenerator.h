/*
Minetest
Copyright (C) 2010-2020 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2015-2020 paramat
Copyright (C) 2013-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#ifndef MAPGENERATOR_H
#define MAPGENERATOR_H

#include "../../Graphics/Node.h"

#include "../../Utils/Noise.h"

#include "Core/Utility/StringUtil.h"

#define MAPGEN_DEFAULT MAPGEN_V7
#define MAPGEN_DEFAULT_NAME "v7"

/////////////////// MapGenerator flags
#define MG_CAVES       0x02
#define MG_DUNGEONS    0x04
#define MG_LIGHT       0x10
#define MG_DECORATIONS 0x20
#define MG_BIOMES      0x40
#define MG_ORES        0x80

typedef uint16_t Biometype;  // copy from MapGeneratorBiome.h to avoid an unnecessary include

class MMVManip;
class NodeManager;
class Settings;

extern FlagDescription FlagdescMapGenerator[];
extern FlagDescription FlagdescGenNotify[];

class Biome;
class BiomeGenerator;
struct BiomeParams;
class BiomeManager;
class EmergeParams;
class MapBlock;
class VoxelManipulator;
struct BlockMakeData;
class VoxelArea;
class Map;

enum MapGeneratorObject 
{
	MGOBJ_VMANIP,
	MGOBJ_HEIGHTMAP,
	MGOBJ_BIOMEMAP,
	MGOBJ_HEATMAP,
	MGOBJ_HUMIDMAP,
	MGOBJ_GENNOTIFY
};

enum GenNotifyType 
{
	GENNOTIFY_DUNGEON,
	GENNOTIFY_TEMPLE,
	GENNOTIFY_CAVE_BEGIN,
	GENNOTIFY_CAVE_END,
	GENNOTIFY_LARGECAVE_BEGIN,
	GENNOTIFY_LARGECAVE_END,
	GENNOTIFY_DECORATION,
	NUM_GENNOTIFY_TYPES
};

struct GenNotifyEvent 
{
	GenNotifyType type;
	Vector3<short> pos;
	unsigned int id;
};

class GenerateNotifier 
{
public:
	// Use only for temporary MapGenerator objects with no map generation!
	GenerateNotifier() = default;
	GenerateNotifier(unsigned int notifyOn, const std::set<unsigned int>* notifyOnDecoIds);

	bool AddEvent(GenNotifyType type, Vector3<short> pos, unsigned int id=0);
	void GetEvents(std::map<std::string, std::vector<Vector3<short>>>& eventMap);
	void ClearEvents();

private:
	unsigned int mNotifyOn = 0;
	const std::set<unsigned int>* mNotifyOnDecoIds = nullptr;
	std::list<GenNotifyEvent> mNotifyEvents;
};

// Order must match the order of 'static MapGeneratorDesc RegisteredMapGenerators[]' in mapgen.cpp
enum MapGeneratorType 
{
	MAPGEN_V7,
	MAPGEN_VALLEYS,
	MAPGEN_CARPATHIAN,
	MAPGEN_V5,
	MAPGEN_FLAT,
	MAPGEN_FRACTAL,
	MAPGEN_SINGLENODE,
	MAPGEN_V6,
	MAPGEN_INVALID,
};

struct MapGeneratorParams 
{
	MapGeneratorParams() = default;
	virtual ~MapGeneratorParams();

	MapGeneratorType mgtype = MAPGEN_DEFAULT;
	short chunkSize = 5;
	uint64_t seed = 0;
	short waterLevel = 1;
	short mapgenLimit = MAX_MAP_GENERATION_LIMIT;
	// Flags set in ReadParams
	unsigned int flags = 0;
	unsigned int spFlags = 0;

	BiomeParams* bparams = nullptr;

	short mapgenEdgeMin = -MAX_MAP_GENERATION_LIMIT;
	short mapgenEdgeMax = MAX_MAP_GENERATION_LIMIT;

	virtual void ReadParams(const Settings* settings);
	virtual void WriteParams(Settings* settings);
	// Default settings for g_settings such as flags
	virtual void SetDefaultSettings(Settings* settings) {};

	int GetSpawnRangeMax();

    bool GetNoiseParams(const Settings* settings, const std::string& name, NoiseParams& np) const;
    bool SetNoiseParams(Settings* settings, const std::string &name, const NoiseParams &np);

protected:

    bool GetNoiseParamsFromValue(const Settings* settings, const std::string& name, NoiseParams& np) const;
    bool GetNoiseParamsFromGroup(const Settings* settings, const std::string& name, NoiseParams &np) const;

private:

	void CalcMapGeneratorEdges();

	bool mapgenEdgesCalculated = false;
};


/*
	Generic interface for map generators.  All mapgens must inherit this class.
	If a feature exposed by a public member pointer is not supported by a
	certain mapgen, it must be set to NULL.

	Apart from MakeChunk, GetGroundLevelAtPoint, and GetSpawnLevelAtPoint, all
	methods can be used by constructing a MapGenerator base class and setting the
	appropriate public members (e.g. vm, nodeMgr, and so on).
*/
class MapGenerator
{
public:
	int mSeed = 0;
	int mWaterLevel = 0;
	int mMapgenLimit = 0;
	unsigned int mFlags = 0;
	bool mGenerating = false;
	int mId = -1;

	MMVManip* mMMVManip = nullptr;
	const NodeManager* mNodeMgr = nullptr;

	unsigned int mBlockSeed;
	short* mHeightmap = nullptr;
	Biometype* mBiomeMap = nullptr;
	Vector3<short> mChunkSize;

	BiomeGenerator* mBiomeGenerator = nullptr;
	GenerateNotifier mGenNotify;

	MapGenerator() = default;
	MapGenerator(int mapgenid, MapGeneratorParams* params, EmergeParams* emerge);
	virtual ~MapGenerator() = default;

	virtual MapGeneratorType GetType() const { return MAPGEN_INVALID; }

	static unsigned int GetBlockSeed(Vector3<short> pos, int seed);
	static unsigned int GetBlockSeed2(Vector3<short> pos, int seed);
	short FindGroundLevel(Vector2<short> p2d, short ymin, short ymax);
	short FindLiquidSurface(Vector2<short> p2d, short ymin, short ymax);
	void UpdateHeightmap(Vector3<short> nmin, Vector3<short> nmax);
	void GetSurfaces(Vector2<short> p2d, short ymin, short ymax,
		std::vector<short>& floors, std::vector<short>& ceilings);

	void UpdateLiquid(std::queue<Vector3<short>>* transLiquid, Vector3<short> nmin, Vector3<short> nmax);

	void SetLighting(uint8_t light, Vector3<short> nmin, Vector3<short> nmax);
	void LightSpread(VoxelArea& a, std::queue<std::pair<Vector3<short>, uint8_t>>& queue,
		const Vector3<short>& p, uint8_t light);
	void CalculateLighting(Vector3<short> nmin, Vector3<short> nmax, Vector3<short> fullnmin, 
        Vector3<short> fullnmax, bool propagateShadow = true);
	void PropagateSunlight(Vector3<short> nmin, Vector3<short> nmax, bool propagateShadow);
	void SpreadLight(const Vector3<short>& nmin, const Vector3<short>& nmax);

	virtual void MakeChunk(BlockMakeData* data) {}
	virtual int GetGroundLevelAtPoint(Vector2<short> pos) { return 0; }

	// GetSpawnLevelAtPoint() is a function within each mapgen that returns a
	// suitable y co-ordinate for player spawn ('suitable' usually meaning
	// within 16 nodes of waterLevel). If a suitable spawn level cannot be
	// found at the specified (X, Z) 'MAX_MAP_GENERATION_LIMIT' is returned to
	// signify this and to cause FindSpawnPosition() to try another (X, Z).
	virtual int GetSpawnLevelAtPoint(Vector2<short> pos) { return 0; }

	// MapGenerator management functions
	static MapGeneratorType GetMapGeneratorType(const std::string& mgname);
	static const char* GetMapGeneratorName(MapGeneratorType mgtype);
	static MapGenerator* CreateMapGenerator(MapGeneratorType mgtype, 
        MapGeneratorParams *params, EmergeParams *emerge);
	static MapGeneratorParams* CreateMapGeneratorParams(MapGeneratorType mgtype);
	static void GetMapGeneratorNames(std::vector<const char *>* mgnames, bool includeHidden);
	static void SetDefaultSettings(Settings *settings);

private:
	// IsLiquidHorizontallyFlowable() is a helper function for UpdateLiquid()
	// that checks whether there are floodable nodes without liquid beneath
	// the node at index vi.
	inline bool IsLiquidHorizontallyFlowable(unsigned int vi, Vector3<short> em);
};

/*
	MapGeneratorBasic is a MapGenerator implementation that handles basic functionality
	the majority of conventional mapgens will probably want to use, but isn't
	generic enough to be included as part of the base MapGenerator class (such as
	generating biome terrain over terrain node skeletons, generating caves,
	dungeons, etc.)

	Inherit MapGeneratorBasic instead of MapGenerator to add this basic functionality to
	your mapgen without having to reimplement it.  Feel free to override any of
	these methods if you desire different or more advanced behavior.

	Note that you must still create your own GenerateTerrain implementation when
	inheriting MapGeneratorBasic.
*/
class MapGeneratorBasic : public MapGenerator 
{
public:
	MapGeneratorBasic(int mapgenid, MapGeneratorParams* params, EmergeParams* emerge);
	virtual ~MapGeneratorBasic();

	virtual void GenerateBiomes();
	virtual void DustTopNodes();
	virtual void GenerateCavesNoiseIntersection(short maxStoneY);
	virtual void GenerateCavesRandomWalk(short maxStoneY, short mLargeCave_ymax);
	virtual bool GenerateCavernsNoise(short maxStoneY);
	virtual void GenerateDungeons(short maxStoneY);

protected:
	EmergeParams* mEmerge;
	BiomeManager* mBiomeMgr;

	Noise* mNoiseFillerDepth;

	Vector3<short> mNodeMin;
	Vector3<short> mNodeMax;
	Vector3<short> mFullNodeMin;
	Vector3<short> mFullNodeMax;

	uint16_t mContentStone;
	uint16_t mContentWaterSource;
	uint16_t mContentRiverWaterSource;
	uint16_t mContentLavaSource;
	uint16_t mContentCobble;

	int mYStride;
	int mZStride;
	int mZStride1d;
	int mZStride1u1d;

	unsigned int mSPFlags;

	NoiseParams mNoiseParamsCave1;
	NoiseParams mNoiseParamsCave2;
	NoiseParams mNoiseParamsCavern;
	NoiseParams mNoiseParamsDungeons;
	float mCaveWidth;
	float mCavernLimit;
	float mCavernTaper;
	float mCavernThreshold;
	int mSmallCaveNumMin;
	int mSmallCaveNumMax;
	int mLargeCaveNumMin;
	int mLargeCaveNumMax;
	float mLargeCaveFlooded;
	short mLargeCaveDepth;
	short mDungeonYmin;
	short mDungeonYmax;
};

#endif