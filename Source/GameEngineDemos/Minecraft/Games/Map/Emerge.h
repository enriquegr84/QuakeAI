/*
Minetest
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#ifndef EMERGE_H
#define EMERGE_H

#include "MapSettingsManager.h"
#include "MapGenerator.h" // for MapgenParams
#include "Map.h"

#define BLOCK_EMERGE_ALLOW_GEN   (1 << 0)
#define BLOCK_EMERGE_FORCE_QUEUE (1 << 1)

class EmergeThread;
class NodeManager;

class BiomeManager;
class OreManager;
class DecorationManager;
class SchematicManager;
class ModApiMapgen;

// Structure containing inputs/outputs for chunk generation
struct BlockMakeData 
{
	MMVManip* vmanip = nullptr;
	uint64_t seed = 0;
	Vector3<short> blockPosMin;
	Vector3<short> blockPosMax;
	std::queue<Vector3<short>> transformingLiquid;
	const NodeManager* nodeMgr = nullptr;

	BlockMakeData() = default;

	~BlockMakeData() { delete vmanip; }
};

// Result from processing an item on the emerge queue
enum EmergeAction 
{
	EMERGE_CANCELLED,
	EMERGE_ERRORED,
	EMERGE_FROM_MEMORY,
	EMERGE_FROM_DISK,
	EMERGE_GENERATED,
};

// Callback
typedef void (*EmergeCompletionCallback)(
	Vector3<short> blockpos, EmergeAction action, void *param);

typedef std::vector<std::pair<EmergeCompletionCallback,void *>> EmergeCallbackList;

struct BlockEmergeData 
{
	uint16_t actorRequested;
	uint16_t flags;
	EmergeCallbackList callbacks;
};

class EmergeParams 
{
	friend class EmergeManager;
public:
	EmergeParams() = delete;
	~EmergeParams();

	const NodeManager* mNodeMgr; // shared
	bool mEnableMapgenDebugInfo;

	unsigned int mGenNotifyOn;
	const std::set<unsigned int>* mGenNotifyOnDecoIds; // shared

	BiomeGenerator* mBiomeGen;
	BiomeManager* mBiomeMgr;
	OreManager* mOreMgr;
	DecorationManager* mDecoMgr;
	SchematicManager* mSchemMgr;

private:
	EmergeParams(EmergeManager* parent, const BiomeGenerator* biomeGen, const BiomeManager* biomeMgr, 
        const OreManager* oreMgr, const DecorationManager* decoMgr, const SchematicManager* schemMgr);
};

class EmergeManager 
{
	/* The mod API needs unchecked access to allow:
	 * - using mDecoMgr or mOreMgr to place decos/ores
	 * - using mSchemMgr to load and place schematics
	 */
	friend class ModApiMapgen;
public:
	const NodeManager* mNodeMgr;
	bool mEnableMapgenDebugInfo;

	// Generation Notify
	unsigned int mGenNotifyOn = 0;
	std::set<unsigned int> mGenNotifyOnDecoIds;

	// Parameters passed to mapgens owned by LogicMap
	// TODO(hmmmm): Remove this after mapgen helper methods using them
	// are moved to LogicMap
	MapGeneratorParams* mMapgenParams;
     
	// Hackish workaround:
	// For now, EmergeManager must hold onto a ptr to the Map's setting manager
	// since the Map can only be accessed through the Environment, and the
	// Environment is not created until after script initialization.
	MapSettingsManager* mMapSettingsMgr;

	// Methods
	EmergeManager(LogicEnvironment* env);
	~EmergeManager();

	const BiomeGenerator* GetBiomeGenerator() const { return mBiomeGen; }

	// no usage restrictions
	const BiomeManager* GetBiomeManager() const { return mBiomeMgr; }
	const OreManager* GetOreManager() const { return mOreMgr; }
	const DecorationManager* GetDecorationManager() const { return mDecoMgr; }
	const SchematicManager* GetSchematicManager() const { return mSchemMgr; }
	// only usable before mapgen init
	BiomeManager* GetWritableBiomeManager();
	OreManager* GetWritableOreManager();
	DecorationManager* GetWritableDecorationManager();
	SchematicManager* GetWritableSchematicManager();

	void InitMapGenerators(MapGeneratorParams* mapgenParams);

	void StartThreads();
	void StopThreads();
	bool IsRunning();

	bool EnqueueBlockEmerge(ActorId actorId, 
        Vector3<short> blockpos, bool allowGenerate, bool ignoreQueueLimits=false);

	bool EnqueueBlockEmergeEx(Vector3<short> blockpos, 
        ActorId actorId, uint16_t flags, EmergeCompletionCallback callback, void* callbackParam);

	Vector3<short> GetContainingChunk(Vector3<short> blockpos);

    MapGenerator* GetCurrentMapGenerator();

	// Mapgen helpers methods
	int GetSpawnLevelAtPoint(Vector2<short> pos);
	int GetGroundLevelAtPoint(Vector2<short> pos);
	bool IsBlockUnderground(Vector3<short> blockpos);

	static Vector3<short> GetContainingChunk(Vector3<short> blockpos, short chunksize);

    // Getter for the main global emerge manager. This is the system that is used by the majority of the 
    // engine, though you are free to define your own as long as you instantiate it.
    // It is not valid to have more than one global emerge manager.
    static EmergeManager* Get(void);

private:

    static EmergeManager* mEmergeMgr;

	std::vector<MapGenerator*> mMapgens;
	std::vector<EmergeThread *> mThreads;
	bool mThreadsActive = false;

	std::mutex mQueueMutex;
	std::map<Vector3<short>, BlockEmergeData> mBlocksEnqueued;
	std::unordered_map<uint16_t, uint16_t> mActorQueueCount;

	uint16_t mQLimitTotal;
	uint16_t mQLimitDiskonly;
	uint16_t mQLimitGenerate;

	// Managers of various map generation-related components
	// Note that each Mapgen gets a copy(!) of these to work with
    BiomeGenerator* mBiomeGen;
	BiomeManager* mBiomeMgr;
	OreManager* mOreMgr;
	DecorationManager* mDecoMgr;
	SchematicManager* mSchemMgr;

	// Requires mQueueMutex held
	EmergeThread* GetOptimalThread();

	bool PushBlockEmergeData(Vector3<short> pos, ActorId actorRequested, 
        uint16_t flags, EmergeCompletionCallback callback, void* callbackParam, bool* entryAlreadyExists);

	bool PopBlockEmergeData(Vector3<short> pos, BlockEmergeData* bedata);

	friend class EmergeThread;
};

#endif