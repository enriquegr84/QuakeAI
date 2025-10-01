/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef MAPBLOCK_H
#define MAPBLOCK_H

#include "GameEngineStd.h"

#include "Map.h"
#include "MapNode.h"
#include "MapNodeMetadata.h"

#include "../../Utils/Util.h"

#include "../../Graphics/Map/MapBlockMesh.h"

#include "../../Games/Actors/StaticObject.h"

#include "Graphic/Scene/Hierarchy/BoundingBox.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

#include "Core/Logger/Logger.h"

class Map;
class MapNodeMetadataList;
class VoxelManipulator;

#define BLOCK_TIMESTAMP_UNDEFINED 0xffffffff

////
//// MapBlock modified reason flags
////

#define MOD_REASON_INITIAL                   (1 << 0)
#define MOD_REASON_REALLOCATE                (1 << 1)
#define MOD_REASON_SET_IS_UNDERGROUND        (1 << 2)
#define MOD_REASON_SET_LIGHTING_COMPLETE     (1 << 3)
#define MOD_REASON_SET_GENERATED             (1 << 4)
#define MOD_REASON_SET_NODE                  (1 << 5)
#define MOD_REASON_SET_NODE_NO_CHECK         (1 << 6)
#define MOD_REASON_SET_TIMESTAMP             (1 << 7)
#define MOD_REASON_REPORT_META_CHANGE        (1 << 8)
#define MOD_REASON_CLEAR_ALL_OBJECTS         (1 << 9)
#define MOD_REASON_BLOCK_EXPIRED             (1 << 10)
#define MOD_REASON_ADD_ACTIVE_OBJECT_RAW     (1 << 11)
#define MOD_REASON_REMOVE_OBJECTS_REMOVE     (1 << 12)
#define MOD_REASON_REMOVE_OBJECTS_DEACTIVATE (1 << 13)
#define MOD_REASON_TOO_MANY_OBJECTS          (1 << 14)
#define MOD_REASON_STATIC_DATA_ADDED         (1 << 15)
#define MOD_REASON_STATIC_DATA_REMOVED       (1 << 16)
#define MOD_REASON_STATIC_DATA_CHANGED       (1 << 17)
#define MOD_REASON_EXPIRE_DAYNIGHTDIFF       (1 << 18)
#define MOD_REASON_VMANIP                    (1 << 19)
#define MOD_REASON_UNKNOWN                   (1 << 20)

////
//// MapBlock itself
////

class MapBlock
{
public:
	MapBlock(Map* parent, Environment* env, Vector3<short> pos, bool dummy=false);
	~MapBlock();

	/*virtual unsigned short NodeContainerId() const
	{
		return NODECONTAINER_ID_MAPBLOCK;
	}*/

	Map* GetParent()
	{
		return mParent;
	}

	void Reallocate()
	{
		delete[] mData;
        mData = new MapNode[NodeCount];
		for (unsigned int i = 0; i < NodeCount; i++)
            mData[i] = MapNode(CONTENT_IGNORE);

		RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_REALLOCATE);
	}

	MapNode* GetData()
	{
		return mData;
	}

	////
	//// Modification tracking methods
	////
	void RaiseModified(unsigned int mod, unsigned int reason=MOD_REASON_UNKNOWN)
	{
		if (mod > mModified) 
        {
            mModified = mod;
			mModifiedReason = reason;
			if (mModified >= MOD_STATE_WRITE_AT_UNLOAD)
				mDiskTimestamp = mTimestamp;
		} 
        else if (mod == mModified) 
        {
			mModifiedReason |= reason;
		}
		if (mod == MOD_STATE_WRITE_NEEDED)
		{
			mContentsCached = false;
		}

	}

	inline unsigned int GetModified()
	{
		return mModified;
	}

	inline unsigned int GetModifiedReason()
	{
		return mModifiedReason;
	}

	std::string GetModifiedReasonString();

	inline void ResetModified()
	{
		mModified = MOD_STATE_CLEAN;
		mModifiedReason = 0;
	}

	////
	//// Flags
	////
	inline bool IsDummy()
	{
		return !mData;
	}

	inline void UnDummify()
	{
		LogAssert(IsDummy(), "Not dummy"); // Pre-condition
		Reallocate();
	}

	// IsUnderground getter/setter
	inline bool IsUnderground()
	{
		return mIsUnderground;
	}

	inline void SetIsUnderground(bool aIsUnderground)
	{
        mIsUnderground = aIsUnderground;
		RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_IS_UNDERGROUND);
	}

	inline void SetLightingComplete(unsigned short newflags)
	{
		if (newflags != mLightingComplete) 
        {
			mLightingComplete = newflags;
			RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_LIGHTING_COMPLETE);
		}
	}

	inline unsigned short GetLightingComplete()
	{
		return mLightingComplete;
	}

	inline void SetLightingComplete(LightBank bank, uint8_t direction, bool isComplete)
	{
		LogAssert(direction >= 0 && direction <= 5, "Incorrect direction");
		if (bank == LIGHTBANK_NIGHT) 
			direction += 6;

		unsigned short newflags = mLightingComplete;
		if (isComplete) 
			newflags |= 1 << direction;
        else 
			newflags &= ~(1 << direction);

		SetLightingComplete(newflags);
	}

	inline bool IsLightingComplete(LightBank bank, uint8_t direction)
	{
		LogAssert(direction >= 0 && direction <= 5, "Incorrect direction");
		if (bank == LIGHTBANK_NIGHT) 
			direction += 6;

		return (mLightingComplete & (1 << direction)) != 0;
	}

	inline bool IsGenerated()
	{
		return mGenerated;
	}

	inline void SetGenerated(bool b)
	{
		if (b != mGenerated) 
        {
			RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_GENERATED);
            mGenerated = b;
		}
	}

	////
	//// Position stuff
	////

	inline Vector3<short> GetPosition()
	{
		return mPosition;
	}

	inline Vector3<short> GetRelativePosition()
	{
		return mRelativePosition;
	}

	inline BoundingBox<short> GetBoundingBox()
	{
		return BoundingBox<short>(
            GetRelativePosition(), GetRelativePosition()
            + Vector3<short>{MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE}
            - Vector3<short>{1, 1, 1});
	}

	////
	//// Regular MapNode get-setters
	////

	inline bool IsValidPosition(short x, short y, short z)
	{
		return mData
			&& x >= 0 && x < MAP_BLOCKSIZE
			&& y >= 0 && y < MAP_BLOCKSIZE
			&& z >= 0 && z < MAP_BLOCKSIZE;
	}

	inline bool IsValidPosition(Vector3<short> pos)
	{
		return IsValidPosition(pos[0], pos[1], pos[2]);
	}

	inline MapNode GetNode(short x, short y, short z, bool* validPosition)
	{
		*validPosition = IsValidPosition(x, y, z);

		if (!*validPosition)
			return {CONTENT_IGNORE};

		return mData[z * ZStride + y * YStride + x];
	}

	inline MapNode GetNode(Vector3<short> pos, bool *validPosition)
	{
		return GetNode(pos[0], pos[1], pos[2], validPosition);
	}

	inline MapNode GetNodeNoEx(Vector3<short> pos)
	{
		bool isValid;
		return GetNode(pos[0], pos[1], pos[2], &isValid);
	}

	inline void SetNode(short x, short y, short z, MapNode& node)
	{
		if (!IsValidPosition(x, y, z))
			throw InvalidPositionException();

		mData[z * ZStride + y * YStride + x] = node;
		RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE);
	}

	inline void SetNode(Vector3<short> pos, MapNode& node)
	{
		SetNode(pos[0], pos[1], pos[2], node);
	}

	////
	//// Non-checking variants of the above
	////
	inline MapNode GetNodeNoCheck(short x, short y, short z, bool* validPosition)
	{
		*validPosition = mData != nullptr;
		if (!*validPosition)
			return {CONTENT_IGNORE};

		return mData[z * ZStride + y * YStride + x];
	}

	inline MapNode GetNodeNoCheck(Vector3<short> pos, bool* validPosition)
	{
		return GetNodeNoCheck(pos[0], pos[1], pos[2], validPosition);
	}

	////
	//// Non-checking, unsafe variants of the above
	//// MapBlock must be loaded by another function in the same scope/function
	//// Caller must ensure that this is not a dummy block (by calling IsDummy())
	////

	inline const MapNode& GetNodeUnsafe(short x, short y, short z)
	{
		return mData[z * ZStride + y * YStride + x];
	}

	inline const MapNode& GetNodeUnsafe(Vector3<short>& pos)
	{
		return GetNodeUnsafe(pos[0], pos[1], pos[2]);
	}

	inline void SetNodeNoCheck(short x, short y, short z, MapNode& node)
	{
		if (!mData)
			throw InvalidPositionException();

        mData[z * ZStride + y * YStride + x] = node;
		RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE_NO_CHECK);
	}

	inline void SetNodeNoCheck(Vector3<short> pos, MapNode& node)
	{
		SetNodeNoCheck(pos[0], pos[1], pos[2], node);
	}

	// These functions consult the parent container if the position
	// is not valid on this MapBlock.
	bool IsValidPositionParent(Vector3<short> pos);
	MapNode GetNodeParent(Vector3<short> pos, bool* isValidPosition = NULL);

	// Copies data to VoxelManipulator to GetRelativePosition()
	void CopyTo(VoxelManipulator& dst);

	// Copies data from VoxelManipulator GetRelativePosition()
	void CopyFrom(VoxelManipulator& dst);

	// Update day-night lighting difference flag.
	// Sets mDayNightDiffers to appropriate value.
	// These methods don't care about neighboring blocks.
	void ActuallyUpdateDayNightDiff();

	// Call this to schedule what the previous function does to be done
	// when the value is actually needed.
	void ExpireDayNightDiff();

	inline bool GetDayNightDiff()
	{
		if (mDayNightDiffersExpired)
			ActuallyUpdateDayNightDiff();
		return mDayNightDiffers;
	}

	////
	//// Miscellaneous stuff
	////

	/*
		Tries to measure ground level.
		Return value:
			-1 = only air
			-2 = only ground
			-3 = random fail
			0...MAP_BLOCKSIZE-1 = ground level
	*/
	short GetGroundLevel(Vector2<short> p2d);

	////
	//// Timestamp (see mTimestamp)
	////

	// NOTE: BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.

	inline void SetTimestamp(unsigned int time)
	{
		mTimestamp = time;
		RaiseModified(MOD_STATE_WRITE_AT_UNLOAD, MOD_REASON_SET_TIMESTAMP);
	}

	inline void SetTimestampNoChangedFlag(unsigned int time)
	{
		mTimestamp = time;
	}

	inline unsigned int GetTimestamp()
	{
		return mTimestamp;
	}

	inline unsigned int GetDiskTimestamp()
	{
		return mDiskTimestamp;
	}

	////
	//// Usage timer (see mUsageTimer)
	////

	inline void ResetUsageTimer()
	{
        mUsageTimer = 0;
	}

	inline void IncrementUsageTimer(float dTime)
	{
        mUsageTimer += dTime;
	}

	inline float GetUsageTimer()
	{
		return mUsageTimer;
	}


    ////
    //// Reference counting (see mRefCount)
    ////

    inline void RefGrab()
    {
        mRefCount++;
    }

    inline void RefDrop()
    {
        mRefCount--;
    }

    inline int RefGet()
    {
        return mRefCount;
    }

	////
	//// Node Timers
	////

	inline NodeTimer GetNodeTimer(const Vector3<short>& p)
	{
		return mNodeTimers.Get(p);
	}

	inline void RemoveNodeTimer(const Vector3<short>& p)
	{
		mNodeTimers.Remove(p);
	}

	inline void SetNodeTimer(const NodeTimer& t)
	{
        mNodeTimers.Set(t);
	}

	inline void ClearNodeTimers()
	{
        mNodeTimers.Clear();
	}

	////
	//// Serialization
	///

	// These don't write or read version by itself
	// Set disk to true for on-disk format, false for over-the-network format
	// Precondition: version >= SER_FMT_VER_LOWEST_WRITE
	void Serialize(std::ostream& os, uint8_t version, bool disk, int compressionLevel);
	// If disk == true: In addition to doing other things, will add
	// unknown blocks from id-name mapping to wnodeMgr
	void Deserialize(std::istream& is, uint8_t version, bool disk);

	void SerializeNetworkSpecific(std::ostream &os);
	void DeserializeNetworkSpecific(std::istream &is);

private:
	/*
		Private methods
	*/

	void DeserializePre22(std::istream& is, uint8_t version, bool disk);

	/*
		Used only internally, because changes can't be tracked
	*/

	inline MapNode& GetNodeRef(short x, short y, short z)
	{
		if (!IsValidPosition(x, y, z))
			throw InvalidPositionException();

		return mData[z * ZStride + y * YStride + x];
	}

	inline MapNode &GetNodeRef(Vector3<short> &p)
	{
		return GetNodeRef(p[0], p[1], p[2]);
	}

public:
	/*
		Public member variables
	*/
    std::shared_ptr<MapBlockMesh> mMesh = nullptr;

	MapNodeMetadataList mMapNodeMetadata;
	NodeTimerList mNodeTimers;
	StaticObjectList mStaticObjects;

	static const unsigned int YStride = MAP_BLOCKSIZE;
	static const unsigned int ZStride = MAP_BLOCKSIZE * MAP_BLOCKSIZE;

	static const unsigned int NodeCount = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;

	//// ABM optimizations ////
	// Cache of content types
	std::unordered_set<unsigned short> mContents;
	// True if content types are cached
	bool mContentsCached = false;
	// True if we never want to cache content types for this block
	bool mNoCachedContents = false;

private:
	/*
		Private member variables
	*/

	// NOTE: Lots of things rely on this being the Map
	Map* mParent;

	Environment* mEnvironment;

	// Position in blocks on parent
	Vector3<short> mPosition;

	/* This is the precalculated mRelativePosition value
	* This caches the value, improving performance by removing 3 short multiplications
	* at runtime on each GetRelativePosition call
	* For a 5 minutes runtime with valgrind this removes 3 * 19M short multiplications
	* The gain can be estimated in Release Build to 3 * 100M multiply operations for 5 mins
	*/
    Vector3<short> mRelativePosition;

	/*
		If NULL, block is a dummy block.
		Dummy blocks are used for caching not-found-on-disk blocks.
	*/
	MapNode* mData = nullptr;

	/*
		- On the logic, this is used for telling whether the
		  block has been modified from the one on disk.
		- On the visual, this is used for nothing.
	*/
	unsigned int mModified = MOD_STATE_WRITE_NEEDED;
	unsigned int mModifiedReason = MOD_REASON_INITIAL;

	/*
		When propagating sunlight and the above block doesn't exist,
		sunlight is assumed if this is false.

		In practice this is set to true if the block is completely
		undeground with nothing visible above the ground except
		caves.
	*/
	bool mIsUnderground = false;

	/*!
	 * Each bit indicates if light spreading was finished
	 * in a direction. (Because the neighbor could also be unloaded.)
	 * Bits (most significant first):
	 * nothing,  nothing,  nothing,  nothing,
	 * night X-, night Y-, night Z-, night Z+, night Y+, night X+,
	 * day X-,   day Y-,   day Z-,   day Z+,   day Y+,   day X+.
	*/
	unsigned short mLightingComplete = 0xFFFF;

	// Whether day and night lighting differs
	bool mDayNightDiffers = false;
	bool mDayNightDiffersExpired = true;

	bool mGenerated = false;

	/*
		When block is removed from active blocks, this is set to gametime.
		Value BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	unsigned int mTimestamp = BLOCK_TIMESTAMP_UNDEFINED;
	// The on-disk (or to-be on-disk) timestamp value
	unsigned int mDiskTimestamp = BLOCK_TIMESTAMP_UNDEFINED;

	/*
		When the block is accessed, this is set to 0.
		Map will unload the block when this reaches a timeout.
	*/
	float mUsageTimer = 0;

	/*
		Reference count; currently used for determining if this block is in
		the list of blocks to be drawn.
	*/
	int mRefCount = 0;
};

typedef std::vector<MapBlock*> MapBlockVec;

inline bool ObjectPositionOverLimit(Vector3<float> pos)
{
	const float maxLimitBS = MAX_MAP_GENERATION_LIMIT * BS;
	return pos[0] < -maxLimitBS || pos[0] >  maxLimitBS ||
        pos[1] < -maxLimitBS || pos[1] >  maxLimitBS ||
        pos[2] < -maxLimitBS || pos[2] >  maxLimitBS;
}

inline bool BlockPositionOverMaxLimit(Vector3<short> pos)
{
	const short maxLimitBP = MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE;
	return pos[0] < -maxLimitBP || pos[0] >  maxLimitBP ||
        pos[1] < -maxLimitBP || pos[1] >  maxLimitBP ||
        pos[2] < -maxLimitBP || pos[2] >  maxLimitBP;
}

/*
	Returns the position of the block where the node is located
*/
inline Vector3<short> GetNodeBlockPosition(const Vector3<short>& p)
{
	return GetContainerPosition(p, MAP_BLOCKSIZE);
}

inline void GetNodeBlockPositionWithOffset(const Vector3<short>& p, Vector3<short>& block, Vector3<short>& offset)
{
	GetContainerPositionWithOffset(p, MAP_BLOCKSIZE, block, offset);
}

/*
	Get a quick string to describe what a block actually contains
*/
std::string AnalyzeBlock(MapBlock *block);

#endif