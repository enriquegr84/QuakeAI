/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef MAP_H
#define MAP_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector3.h"

#include "MapNode.h"
#include "MapSettingsManager.h"
#include "Voxel.h"

#include "../../Graphics/Node.h"

#include "../../Data/MetricsBackend.h"

#include "../../Games/Games.h"

class MapDatabase;
class VisualMap;
class MapSector;
class LogicMapSector;
class MapBlock;
class MapNodeMetadata;
class LogicEnvironment;

struct BlockMakeData;

/*
    Map format serialization version
    --------------------------------

    For map data (blocks, nodes, sectors).

    NOTE: The goal is to increment this so that saved maps will be
          loadable by any version. Other compatibility is not
          Maintained.

    0: original networked test with 1-byte nodes
    1: update with 2-byte nodes
    2: lighting is transmitted in param
    3: optional fetching of far blocks
    4: block compression
    5: sector objects NOTE: block compression was left accidentally out
    6: failed attempt at switching block compression on again
    7: block compression switched on again
    8: logic-initiated block transfers and all kinds of stuff
    9: block objects
    10: water pressure
    11: zlib'd blocks, block flags
    12: UnlimitedHeightmap now uses interpolated areas
    13: Mapgen v2
    14: NodeMetadata
    15: StaticObjects
    16: larger maximum size of node metadata, and compression
    17: MapBlocks contain timestamp
    18: new generator (not really necessary, but it's there)
    19: new content type handling
    20: many existing content types translated to extended ones
    21: dynamic content type allocation
    22: minerals removed, facedir & wallmounted changed
    23: new node metadata format
    24: 16-bit node ids and node timers (never released as stable)
    25: Improved node timer format
    26: Never written; read the same as 25
    27: Added light spreading flags to blocks
    28: Added "private" flag to NodeMetadata
*/


enum ModifiedState
{
    // Has not been modified.
    MOD_STATE_CLEAN = 0,
    MOD_RESERVED1 = 1,
    // Has been modified, and will be saved when being unloaded.
    MOD_STATE_WRITE_AT_UNLOAD = 2,
    MOD_RESERVED3 = 3,
    // Has been modified, and will be saved as soon as possible.
    MOD_STATE_WRITE_NEEDED = 4,
    MOD_RESERVED5 = 5,
};


/*
	MapEditEvent
*/

#define MAPTYPE_BASE 0
#define MAPTYPE_LOGIC 1
#define MAPTYPE_VISUAL 2

enum MapEditEventType
{
	// Node added (changed from air or something else to something)
	MEET_ADDNODE,
	// Node removed (changed to air)
	MEET_REMOVENODE,
	// Node swapped (changed without metadata change)
	MEET_SWAPNODE,
	// Node metadata changed
	MEET_BLOCK_NODE_METADATA_CHANGED,
	// Anything else (modifiedBlocks are set unsent)
	MEET_OTHER
};

struct MapEditEvent
{
	MapEditEventType type = MEET_OTHER;
	Vector3<short> position;
	MapNode node = CONTENT_AIR;
    short blockSize = MAP_BLOCKSIZE;
	std::set<Vector3<short>> modifiedBlocks;
	bool IsPrivateChange = false;

	MapEditEvent() = default;

	VoxelArea GetArea() const
	{
		switch(type)
        {
		    case MEET_ADDNODE:
			    return VoxelArea(position);
		    case MEET_REMOVENODE:
			    return VoxelArea(position);
		    case MEET_SWAPNODE:
			    return VoxelArea(position);
		    case MEET_BLOCK_NODE_METADATA_CHANGED:
		    {
			    Vector3<short> np1 = position * blockSize;
                Vector3<short> np2 = np1 + Vector3<short>{1, 1, 1} * blockSize - Vector3<short>{1, 1, 1};
			    return VoxelArea(np1, np2);
		    }
		    case MEET_OTHER:
		    {
			    VoxelArea a;
			    for (Vector3<short> mb : modifiedBlocks) 
                {
				    Vector3<short> np1 = mb * blockSize;
                    Vector3<short> np2 = np1 + Vector3<short>{1, 1, 1} * blockSize - Vector3<short>{1, 1, 1};
				    a.AddPoint(np1);
				    a.AddPoint(np2);
			    }
			    return a;
		    }
		}
		return VoxelArea();
	}
};

class MapEventReceiver
{
public:
	// event shall be deleted by caller after the call.
	virtual void OnMapEditEvent(const MapEditEvent& evt) = 0;
};

class Map /*: public NodeContainer*/
{
public:

	Map(Environment* env = nullptr);
	virtual ~Map();

	virtual int MapType() const
	{
		return MAPTYPE_BASE;
	}

	/*
		Drop (visual) or delete (logic) the map.
	*/
	virtual void Drop()
	{
		delete this;
	}

	void AddEventReceiver(MapEventReceiver* eventReceiver);
	void RemoveEventReceiver(MapEventReceiver* eventReceiver);
	// event shall be deleted by caller after the call.
	void DispatchEvent(const MapEditEvent& evt);

	// On failure returns NULL
	MapSector* GetSectorNoGenerateNoLock(Vector2<short> p2d);
	// Same as the above (there exists no lock anymore)
	MapSector* GetSectorNoGenerate(Vector2<short> p2d);

	/*
		This is overloaded by VisualMap and LogicMap to allow
		their differing fetch methods.
	*/
	virtual MapSector* EmergeSector(Vector2<short> pos){ return NULL; }

	// Returns InvalidPositionException if not found
	MapBlock* GetBlockNoCreate(Vector3<short> pos);
	// Returns NULL if not found
	MapBlock* GetBlockNoCreateNoEx(Vector3<short> pos);

	/* Logic overrides */
	virtual MapBlock* EmergeBlock(Vector3<short> pos, bool createBlank=true) 
	{ return GetBlockNoCreateNoEx(pos); }

	inline const NodeManager* GetNodeManager() { return mEnvironment->GetNodeManager(); }

	// Returns InvalidPositionException if not found
	bool IsNodeUnderground(Vector3<short> pos);

	bool IsValidPosition(Vector3<short> pos);

	// throws InvalidPositionException if not found
	void SetNode(Vector3<short> pos, MapNode& n);

	// Returns a CONTENT_IGNORE node if not found
	// If isValidPosition is not NULL then this will be set to true if the
	// position is valid, otherwise false
	MapNode GetNode(Vector3<short> pos, bool* isValidPosition = NULL);

	/*
		These handle lighting but not faces.
	*/
	void AddNodeAndUpdate(Vector3<short> pos, MapNode n,
			std::map<Vector3<short>, MapBlock*>& modifiedBlocks, bool removeMetadata = true);
	void RemoveNodeAndUpdate(Vector3<short> pos,
			std::map<Vector3<short>, MapBlock*>& modifiedBlocks);

	/*
		Wrappers for the latter ones.
		These emit events.
		Return true if succeeded, false if not.
	*/
	bool AddNodeWithEvent(Vector3<short> pos, MapNode n, bool removeMetadata = true);
	bool RemoveNodeWithEvent(Vector3<short> pos);

	// Call these before and after saving of many blocks
	virtual void BeginSave() {}
	virtual void EndSave() {}

	virtual void Save(ModifiedState saveLevel) { LogError("FIXME"); }

	// Logic implements these.
	// Visual leaves them as no-op.
	virtual bool SaveBlock(MapBlock* block) { return false; }
	virtual bool DeleteBlock(Vector3<short> blockPos) { return false; }

	/*
		Updates usage timers and unloads unused blocks and sectors.
		Saves modified blocks before unloading on MAPTYPE_LOGIC.
	*/
	void TimerUpdate(float dTime, float unloadTimeout, unsigned int maxLoadedBlocks,
			std::vector<Vector3<short>>* unloadedBlocks=NULL);

	/*
		Unloads all blocks with a zero refCount().
		Saves modified blocks before unloading on MAPTYPE_LOGIC.
	*/
	void UnloadUnreferencedBlocks(std::vector<Vector3<short>>* unloadedBlocks=NULL);

	// Deletes sectors and their blocks from memory
	// Takes cache into account
	// If deleted sector is in sector cache, clears cache
	void DeleteSectors(std::vector<Vector2<short>>& list);

	// For debug printing. Prints "Map: ", "LogicMap: " or "VisualMap: "
	virtual void PrintInfo(std::ostream& out);
	void TransformLiquids(std::map<Vector3<short>, MapBlock*>& modifiedBlocks, LogicEnvironment* env);

	/*
		Node metadata
		These are basically coordinate wrappers to MapBlock
	*/

	std::vector<Vector3<short>> FindNodesWithMetadata(Vector3<short> p1, Vector3<short> p2);
	MapNodeMetadata* GetMapNodeMetadata(Vector3<short> pos);

	/**
	 * Sets metadata for a node.
	 * This method sets the metadata for a given node.
	 * On success, it returns @c true and the object pointed to
	 * by @p meta is then managed by the system and should
	 * not be deleted by the caller.
	 *
	 * In case of failure, the method returns @c false and the
	 * caller is still responsible for deleting the object!
	 *
	 * @param p node coordinates
	 * @param meta pointer to @c MapNodeMetadata object
	 * @return @c true on success, false on failure
	 */
	bool SetMapNodeMetadata(Vector3<short> pos, MapNodeMetadata* meta);
	void RemoveMapNodeMetadata(Vector3<short> pos);

	/*
		Node Timers
		These are basically coordinate wrappers to MapBlock
	*/

	NodeTimer GetNodeTimer(Vector3<short> pos);
	void SetNodeTimer(const NodeTimer& t);
	void RemoveNodeTimer(Vector3<short> pos);

	/*
		Variables
	*/
	void TransformingLiquidAdd(Vector3<short> pos);

	bool IsBlockOccluded(MapBlock* block, Vector3<short> camPositionNodes);

protected:
	friend class VoxelManip;

	Environment* mEnvironment;

	std::set<MapEventReceiver*> mEventReceivers;

	std::map<Vector2<short>, MapSector*> mSectors;

	// Be sure to set this to NULL when the cached sector is deleted
	MapSector* mSectorCache = nullptr;
	Vector2<short> mSectorCachePos;

	// Queued transforming water nodes
	std::queue<Vector3<short>> mTransformingLiquid;

	bool DetermineAdditionalOcclusionCheck(const Vector3<short>& posCamera,
		const BoundingBox<short>& blockBounds, Vector3<short>& check);
	bool IsOccluded(const Vector3<short>& posCamera, const Vector3<short>& posTarget,
		float step, float stepfac, float startOffset, float endOffset, unsigned int neededCount);

private:

	unsigned int mUnprocessedCount = 0;
	uint64_t mIncTrendingUpStartTime = 0; // milliseconds
	bool mQueueSizeTimerStarted = false;
    float mTransformingLiquidLoopCountMultiplier = 1.0f;

};

/*
	LogicMap

	This is the only map class that is able to generate map.
*/

class LogicMap : public Map
{
public:
	/*
		savedir: directory to which map data should be saved
	*/
	LogicMap(const std::string& savedir, MetricsBackend* mb, LogicEnvironment* env);
	~LogicMap();

	int MapType() const
	{
		return MAPTYPE_LOGIC;
	}

	/*
		Get a sector from somewhere.
		- Check memory
		- Check disk (doesn't load blocks)
		- Create blank one
	*/
	MapSector* CreateSector(Vector2<short> pos);

	/*
		Blocks are generated by using these and makeBlock().
	*/
	bool BlockPositionOverMapgenLimit(Vector3<short> pos);
	bool InitBlockMake(Vector3<short> blockPos, BlockMakeData* data);
	void FinishBlockMake(BlockMakeData* data,
		std::map<Vector3<short>, MapBlock*>* changedBlocks);

	/*
		Get a block from somewhere.
		- Memory
		- Create blank
	*/
	MapBlock* CreateBlock(Vector3<short> pos);

	/*
		Forcefully get a block from somewhere.
		- Memory
		- Load from disk
		- Create blank filled with CONTENT_IGNORE

	*/
	MapBlock* EmergeBlock(Vector3<short> pos, bool createBlank=true);

	/*
		Try to get a block.
		If it does not exist in memory, add it to the emerge queue.
		- Memory
		- Emerge Queue (deferred disk or generate)
	*/
	MapBlock* GetBlockOrEmerge(Vector3<short> p3d);

	/*
		Database functions
	*/
	static MapDatabase* CreateDatabase(
        const std::string& name, const std::string& savedir);

	// Call these before and after saving of blocks
	void BeginSave();
	void EndSave();

	void Save(ModifiedState saveLevel);
	void ListAllLoadableBlocks(std::vector<Vector3<short>>& dst);
	void ListAllLoadedBlocks(std::vector<Vector3<short>>& dst);

	MapGeneratorParams* GetMapGeneratorParams();

	bool SaveBlock(MapBlock* block);
	static bool SaveBlock(MapBlock* block, MapDatabase* db, int compressionLevel = -1);
	MapBlock* LoadBlock(Vector3<short> pos);
	// Database version
	void LoadBlock(std::string* blob, Vector3<short> p3d, MapSector* sector, bool saveAfterLoad=false);

	bool DeleteBlock(Vector3<short> blockPos);

	void UpdateVManip(Vector3<short> pos);

	// For debug printing
	virtual void PrintInfo(std::ostream& out);

	bool IsSavingEnabled(){ return mMapSavingEnabled; }

	uint64_t GetSeed();

	/*!
	 * Fixes lighting in one map block.
	 * May modify other blocks as well, as light can spread
	 * out of the specified block.
	 * Returns false if the block is not generated (so nothing
	 * changed), true otherwise.
	 */
	bool RepairBlockLight(Vector3<short> blockPos,
		std::map<Vector3<short>, MapBlock*>* modifiedBlocks);

	MapSettingsManager mSettingsMgr;

private:

	std::wstring mSavedir;
	bool mMapSavingEnabled;

	int mMapCompressionLevel;

	std::set<Vector3<short>> mChunksInProgress;

	/*
		Metadata is re-written on disk only if this is true.
		This is reset to false when written on disk.
	*/
	bool mMapMetadataChanged = true;
	MapDatabase* mDatabase = nullptr;
	MapDatabase* mDatabaseRo = nullptr;

	MetricCounterPtr mSaveTimeCounter;
};


#define VMANIP_BLOCK_DATA_INEXIST     1
#define VMANIP_BLOCK_CONTAINS_CIGNORE 2

class MMVManip : public VoxelManipulator
{
public:
	MMVManip(Map *map);
	virtual ~MMVManip() = default;

	virtual void Clear()
	{
		VoxelManipulator::Clear();
		mLoadedBlocks.clear();
	}

	void InitialEmerge(Vector3<short> blockPosMin, Vector3<short> blockPosMax,
		bool loadIfInexistent = true);

	// This is much faster with big chunks of generated data
	void BlitBackAll(std::map<Vector3<short>, MapBlock*>* modifiedBlocks,
		bool overwriteGenerated = true);

	bool mIsDirty = false;

protected:
	Map* mMap;
	/*
		key = blockPos
		value = flags describing the block
	*/
	std::map<Vector3<short>, uint8_t> mLoadedBlocks;
};


#endif