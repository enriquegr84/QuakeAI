/*
Minetest
Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef LOGICENVIRONMENT_H
#define LOGICENVIRONMENT_H

#include "Environment.h"

#include "../../Data/MetricsBackend.h"

#include "../Actors/ActiveObjectManager.h"
#include "../Actors/ActiveObject.h"

#include "../Map/MapNode.h"
#include "../Map/Map.h"

#include "../../Utils/Util.h"

#include "Core/OS/OS.h"

#include "Application/Settings.h"

class BaseGame;
class LogicMap;
class MapBlock;
class LogicPlayer;
class PlayerLAO;
class PlayerDatabase;
class AuthDatabase;
class LogicEnvironment;
class LogicActiveObject;

struct GameParams;
struct StaticObject;
struct MapEditEvent;

/*
	{Active, Loading} block modifier interface.

	These are fed into LogicEnvironment at initialization time;
	LogicEnvironment handles deleting them.
*/

struct SoundParams
{
	enum Type
	{
		SP_LOCAL,
		SP_POSITIONAL,
		SP_OBJECT
	} type = SP_LOCAL;
	float gain = 1.0f;
	float fade = 0.0f;
	float pitch = 1.0f;
	bool loop = false;
	float maxHearDistance = 32 * BS;
	Vector3<float> position;
	uint16_t object = 0;
	std::string toPlayer = "";
	std::string excludePlayer = "";

	Vector3<float> GetPosition(LogicEnvironment* env, bool* positionExists) const;
};

struct SoundPlaying
{
	SoundParams params;
	SimpleSound sound;
	std::unordered_set<ActorId> actors; // ActorIds
};

class ActiveBlockModifier
{
public:
	ActiveBlockModifier() = default;
	virtual ~ActiveBlockModifier() = default;

	// Set of contents to trigger on
	virtual const std::vector<std::string>& GetTriggerContents() const = 0;
	// Set of required neighbors (trigger doesn't happen if none are found)
	// Empty = do not check neighbors
	virtual const std::vector<std::string>& GetRequiredNeighbors() const = 0;
	// Trigger interval in seconds
	virtual float GetTriggerInterval() = 0;
	// Random chance of (1 / return value), 0 is disallowed
	virtual unsigned int GetTriggerChance() = 0;
	// Whether to modify chance to simulate time lost by an unnattended block
	virtual bool GetSimpleCatchUp() = 0;
	// This is called usually at interval for 1/chance of the nodes
	virtual void Trigger(LogicEnvironment *env, Vector3<short> pos, MapNode n){};
	virtual void Trigger(LogicEnvironment *env, Vector3<short> pos, MapNode n,
		unsigned int activeObjectCount, unsigned int activeObjectCountWider){};
};


class ABM : public ActiveBlockModifier 
{
public:
	ABM(int id,
		const std::vector<std::string>& triggerContents,
		const std::vector<std::string>& requiredNeighbors, 
		float triggerInterval, unsigned int triggerChance, bool simpleCatchUp) : 
		mId(id), mTriggerContents(triggerContents), mRequiredNeighbors(requiredNeighbors),
		mTriggerInterval(triggerInterval), mTriggerChance(triggerChance), mSimpleCatchUp(simpleCatchUp)
	{

	}

	virtual const std::vector<std::string>& GetTriggerContents() const
	{
		return mTriggerContents;
	}

	virtual const std::vector<std::string>& GetRequiredNeighbors() const
	{
		return mRequiredNeighbors;
	}

	virtual float GetTriggerInterval()
	{
		return mTriggerInterval;
	}

	virtual unsigned int GetTriggerChance()
	{
		return mTriggerChance;
	}

	virtual bool GetSimpleCatchUp()
	{
		return mSimpleCatchUp;
	}

	virtual void Trigger(LogicEnvironment* env, Vector3<short> p, MapNode n,
		unsigned int activeObjectCount, unsigned int activeObjectCountWider);

private:
	int mId;

	std::vector<std::string> mTriggerContents;
	std::vector<std::string> mRequiredNeighbors;
	float mTriggerInterval;
	unsigned int mTriggerChance;
	bool mSimpleCatchUp;
};


struct ABMWithState
{
	ActiveBlockModifier* activeBlockModifier;
	float timer = 0.0f;

	ABMWithState(ActiveBlockModifier* abm);
};

struct LoadingBlockModifier
{
	// Set of contents to trigger on
	std::set<std::string> mTriggerContents;
	std::string mName;
	bool mRunAtEveryLoad = false;

	virtual ~LoadingBlockModifier() = default;

	virtual void Trigger(LogicEnvironment* env, Vector3<short> pos, MapNode n){};
};

class LBM : public LoadingBlockModifier
{
public:
	LBM(int id,
		const std::set<std::string>& triggerContents,
		const std::string& name, bool runAtEveryLoad) : mId(id)
	{
		mRunAtEveryLoad = runAtEveryLoad; 
		mTriggerContents = triggerContents; 
		mName = name;
	}

	virtual void Trigger(LogicEnvironment* env, Vector3<short> pos, MapNode n);

private:
	int mId;
};


struct LBMContentMapping
{
	typedef std::unordered_map<uint16_t, std::vector<LoadingBlockModifier *>> LBMMap;
	LBMMap map;

	std::vector<LoadingBlockModifier*> lbmList;

	// Needs to be separate method (not inside destructor),
	// because the LBMContentMapping may be copied and destructed
	// many times during operation in the LBMLookupMap.
	void DeleteContents();
	void AddLBM(LoadingBlockModifier* lbm, LogicEnvironment* env);
	const std::vector<LoadingBlockModifier*>* Lookup(uint16_t c) const;
};

class LBMManager
{
public:
	LBMManager() = default;
	~LBMManager();

	// Don't call this after LoadIntroductionTimes() ran.
	void AddLBM(LoadingBlockModifier* lbm);

	void LoadIntroductionTimes(const std::string& times, LogicEnvironment* env, unsigned int now);

	// Don't call this before LoadIntroductionTimes() ran.
	std::string CreateIntroductionTimesString();

	// Don't call this before LoadIntroductionTimes() ran.
	void ApplyLBMs(LogicEnvironment *env, MapBlock *block, unsigned int stamp);

	// Warning: do not make this std::unordered_map, order is relevant here
	typedef std::map<unsigned int, LBMContentMapping> LBMLookupMap;

private:
	// Once we set this to true, we can only query,
	// not modify
	bool mQueryMode = false;

	// For mQueryMode == false:
	// The key of the map is the LBM def's name.
	// TODO make this std::unordered_map
    std::map<std::string, LoadingBlockModifier *> mLBMs;

	// For mQueryMode == true:
	// The key of the map is the LBM def's first introduction time.
	LBMLookupMap mLBMLookup;

	// Returns an iterator to the LBMs that were introduced
	// after the given time. This is guaranteed to return
	// valid values for everything
	LBMLookupMap::const_iterator GetLBMsIntroducedAfter(unsigned int time) 
    { return mLBMLookup.lower_bound(time); }
};

/*
	List of active blocks, used by LogicEnvironment
*/

class ActiveBlockList
{
public:
	void Update(std::vector<PlayerLAO*>& activePlayers,
		short activeBlockRange, short activeObjectRange,
		std::set<Vector3<short>>& blocksRemoved,
        std::set<Vector3<short>>& blocksAdded);

	bool Contains(Vector3<short> pos) { return (mList.find(pos) != mList.end()); }
	void Clear() { mList.clear(); }

    std::set<Vector3<short>> mList;
    std::set<Vector3<short>> mABMList;
    std::set<Vector3<short>> mForceloadedList;

};

/*
	Operation mode for LogicEnvironment::ClearObjects()
*/
enum ClearObjectsMode 
{
	// Load and go through every mapblock, clearing objects
		CLEAR_OBJECTS_MODE_FULL,

	// Clear objects immediately in loaded mapblocks;
	// clear objects in unloaded mapblocks only when the mapblocks are next activated.
		CLEAR_OBJECTS_MODE_QUICK,
};

/*
    Used for queueing and sorting block transfers in containers

    Lower priority number means higher priority.
*/
struct PrioritySortedBlockTransfer
{
    PrioritySortedBlockTransfer(float pr, const Vector3<short>& pos, ActorId aId)
    {
        priority = pr;
        position = pos;
        actorId = aId;
    }
    bool operator < (const PrioritySortedBlockTransfer &other) const
    {
        return priority < other.priority;
    }
    float priority;
    Vector3<short> position;
    ActorId actorId;
};

class LogicEnvironment : public Environment, public MapEventReceiver
{
public:
	LogicEnvironment(const std::string& pathWorld);
	~LogicEnvironment();

    inline bool IsSingleplayer() { return mSimpleSingleplayerMode; }

	std::shared_ptr<Map> GetMap();

	std::shared_ptr<LogicMap> GetLogicMap();

	std::string GetWorldPath() { return mWorldPath; }

    Vector3<float> FindSpawnPosition();
	float GetSendRecommendedInterval() { return mRecommendedSendInterval; }

	// Both setter and getter need no envlock,
	// can be called freely from threads
	void SetTimeOfDay(unsigned int time);

    /*
        Shall be called with the environment locked.
        This is accessed by the map, which is inside the environment,
        so it shouldn't be a problem.
    */
    virtual void OnMapEditEvent(const MapEditEvent& evt);

    void GotBlock(Vector3<short> pos);
    void SentBlock(Vector3<short> pos);
    void SetBlockNotSent(Vector3<short> pos);
    void SetBlocksNotSent(std::map<Vector3<short>, MapBlock*>& blocks);
    void ResendBlockIfOnWire(Vector3<short> pos);

    unsigned int GetSendingCount() const { return (unsigned int)mBlocksSending.size(); }


    bool IsBlockSent(Vector3<short> pos) const
    {
        return mBlocksSent.find(pos) != mBlocksSent.end();
    }

    /*
        Finds block that should be sent next to the visual.
        Environment should be locked when this is called.
        dtime is used for resetting send radius at slow interval
    */
    void GetNextBlocks(ActorId actorId, float dTime, std::vector<PrioritySortedBlockTransfer>& dest);

    void SendBlockNoLock(ActorId actorId, MapBlock* block, uint8_t ver);
    bool SendBlock(ActorId actorId, const Vector3<short>& blockpos);


    void DiePlayer(ActorId actorId, const PlayerHPChangeReason& reason);

    void SendDeathScreen(ActorId actorId, bool setCameraPointTarget, Vector3<float> cameraPointTarget);

    void SendPlayerMove(PlayerLAO* playerLAO);
    void SendPlayerHPOrDie(PlayerLAO* playerLAO, const PlayerHPChangeReason& reason);
    void SendPlayerHP(ActorId actorId);

    void SendPlayerBreath(PlayerLAO* playerLAO);

    void SendInventory(PlayerLAO* playerLAO, bool incremental);

    void SendDetachedInventory(Inventory* inventory, const std::string& name, ActorId actorId);
    void SendDetachedInventories(ActorId actorId, bool incremental);

	void SendShowFormMessage(ActorId actorId, const std::string& form, const std::string& formName);

    void HandleDetachedInventory(const std::string& name, bool keepInv);

    void UpdateCrafting(LogicPlayer* player);

	//void KickAllPlayers(AccessDeniedCode reason, const std::string& strReason, bool reconnect);
	// Save players
	void SaveLoadedPlayers(bool force = false);
	void SavePlayer(LogicPlayer* player);
	PlayerLAO* LoadPlayer(std::shared_ptr<LogicPlayer> player, bool* newPlayer, bool isSingleplayer);
	void AddPlayer(std::shared_ptr<LogicPlayer> player);
	void RemovePlayer(ActorId playerId);
	bool RemovePlayerFromDatabase(LogicPlayer* player);

	int PlaySound(const SimpleSound& sound, const SoundParams& params, bool ephemeral = false);
	void FadeSound(int handle, float step, float gain);
	void StopSound(int handle);
	void RemoveSounds(const std::vector<int>& soundList);

	/*
		Save and load time of day and game timer
	*/
	void SaveMeta();
	void LoadMeta();

	unsigned int AddParticleSpawner(float exptime);
	unsigned int AddParticleSpawner(float exptime, uint16_t attachedId);
	void DeleteParticleSpawner(unsigned int id, bool removeFromObject = true);

	// Node manager
	virtual NodeManager* GetNodeManager() { return mNodeMgr.get(); }

	// Item manager
	virtual BaseItemManager* GetItemManager() { return mItemMgr.get(); }
    
	// Craft manager
	virtual BaseCraftManager* GetCraftManager() { return mCraftMgr.get(); }

    // Inventory manager
    LogicInventoryManager* GetInventoryManager() { return mInventoryMgr.get(); }

    // Global logic metrics backend
    std::unique_ptr<MetricsBackend> mMetricsBackend;

    // Logic metrics
    MetricCounterPtr mUptimeCounter;
    MetricGaugePtr mPlayerGauge;
    MetricGaugePtr mTimeofDayGauge;
    MetricCounterPtr mAOMBufferCounter;

	/*
		External ActiveObject interface
		-------------------------------------------
	*/

	LogicActiveObject* GetActiveObject(uint16_t id)
	{
		return mActiveObjectMgr.GetActiveObject(id);
	}

	/*
		Add an active object to the environment.
		Environment handles deletion of object.
		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	uint16_t AddActiveObject(LogicActiveObject* object);

	/*
		Add an active object as a static object to the corresponding
		MapBlock.
		Caller allocates memory, LogicEnvironment frees memory.
		Return value: true if succeeded, false if failed.
		(note:  not used, pending removal from engine)
	*/
	//bool AddActiveObjectAsStatic(LogicActiveObject* object);

	/*
		Find out what new objects have been added to
		inside a radius around a position
	*/
	void GetAddedActiveObjects(PlayerLAO* playerLAO, short radius, short playerRadius, 
        std::set<uint16_t>& currentObjects, std::queue<uint16_t>& addedObjects);

	/*
		Find out what new objects have been removed from
		inside a radius around a position
	*/
	void GetRemovedActiveObjects(PlayerLAO* playerLAO, short radius, short playerRadius,
        std::set<uint16_t>& currentObjects, std::queue<uint16_t>& removedObjects);

	/*
		Get the next message emitted by some active object.
		Returns false if no messages are available, true otherwise.
	*/
	bool GetActiveObjectMessage(ActiveObjectMessage* dest);

	virtual void GetSelectedActiveObjects(
		const Line3<float>& shootlineOnMap, std::vector<PointedThing> &objects);

	/*
		Activate objects and dynamically modify for the dtime determined
		from timestamp and additionalDTime
	*/
	void ActivateBlock(MapBlock* block, unsigned int additionalDTime=0);

	/*
		{Active,Loading}BlockModifiers
		-------------------------------------------
	*/

	void AddActiveBlockModifier(ActiveBlockModifier* abm);
	void AddLoadingBlockModifier(LoadingBlockModifier* lbm);

	/*
		Other stuff
		-------------------------------------------
	*/

	// Script-aware node setters
	bool SetNode(Vector3<short> pos, const MapNode& node);
	bool RemoveNode(Vector3<short> pos);
	bool SwapNode(Vector3<short> pos, const MapNode& node);

	// Find the daylight value at pos with a Depth First Search
	uint8_t FindSunlight(Vector3<short> pos);

	// Find all active objects inside a radius around a point
	void GetObjectsInsideRadius(std::vector<LogicActiveObject*>& objects, 
        const Vector3<float>& pos, float radius, std::function<bool(LogicActiveObject *obj)> includeObjCB)
	{
		return mActiveObjectMgr.GetObjectsInsideRadius(pos, radius, objects, includeObjCB);
	}

	// Find all active objects inside a box
	void GetObjectsInArea(std::vector<LogicActiveObject *>& objects,
        const BoundingBox<float>& box, std::function<bool(LogicActiveObject *obj)> includeObjCB)
	{
		return mActiveObjectMgr.GetObjectsInArea(box, objects, includeObjCB);
	}

	// Clear objects, loading and going through every MapBlock
	void ClearObjects(ClearObjectsMode mode);

	// This makes stuff happen
	void Step(float dTime);

	unsigned int GetGameTime() const { return mGameTime; }

	void ReportMaxLagEstimate(float f) { mMaxLagEstimate = f; }
	float GetMaxLagEstimate() { return mMaxLagEstimate; }

	std::set<Vector3<short>>* GetForceloadedBlocks() { return &mActiveBlocks.mForceloadedList; };

    int AddVelocity(UnitLAO* playerLAO, Vector3<float> vel);

	// Sets the static object status all the active objects in the specified block
	// This is only really needed for deleting blocks from the map
	void SetStaticForActiveObjectsInBlock(Vector3<short> blockpos,
		bool staticExists, Vector3<short> staticBlock=Vector3<short>::Zero());

    const std::list<std::string>& GetPlayerNames() { return mPlayerNames; }
    void AddPlayerName(const std::string& name) { mPlayerNames.push_back(name); }
    void RemovePlayerName(const std::string& name) { mPlayerNames.remove(name); }

	std::shared_ptr<LogicPlayer> GetPlayer(const ActorId actorId);
	std::shared_ptr<LogicPlayer> GetPlayer(const char* name);
	const std::vector<std::shared_ptr<LogicPlayer>> GetPlayers() const { return mPlayers; }
	unsigned int GetPlayerCount() const { return (unsigned int)mPlayers.size(); }

	static bool MigratePlayersDatabase(const GameParams& gameParams);

	AuthDatabase* GetAuthDatabase() { return mAuthDatabase; }
	static bool MigrateAuthDatabase(const GameParams& gameParams);

    /*
        Map edit event queue. Automatically receives all map edits.
        The constructor of this class registers us to receive them through
        onMapEditEvent

        NOTE: Should these be moved to actually be members of
        LogicEnvironment?
    */

    /*
        Queue of map edits from the environment for sending to the visuals
        This is behind m_env_mutex
    */
    std::queue<MapEditEvent*> mUnsentMapEditQueue;

    /*
        If a non-empty area, map edit events contained within are left
        unsent. Done at map generation time to speed up editing of the
        generated area, as it will be sent anyway.
        This is behind mEnvMutex
    */
    VoxelArea mIgnoreMapEditEventsArea;

    // Time from last placing or removing blocks
    float mTimeFromBuilding = 9999;

	// Timer for sending time of day over network
	float mTimeOfDaySendTimer = 0.0f;

    // List of active objects that the visual knows of.
    std::set<unsigned short> mKnownObjects;

    // Used for saving logic map to disk
    MapDatabase* mLocalDB = nullptr;
    IntervalLimiter mLocalDBSaveInterval;
    uint16_t mCacheSaveInterval;

	std::unordered_map<uint16_t, std::string> mFormStateData;

	// Environment mutex (envlock)
	std::mutex mEnvMutex;

	std::recursive_mutex mEnvRecMutex;

private:

    friend class EmergeThread;


	/**
	 * called if env_meta.txt doesn't exist (e.g. new world)
	 */
	void LoadDefaultMeta();

	static PlayerDatabase* OpenPlayerDatabase(const std::string& name, const std::string& savedir);
	static AuthDatabase* OpenAuthDatabase(const std::string& name, const std::string& savedir);
	/*
		Internal ActiveObject interface
		-------------------------------------------
	*/

	/*
		Add an active object to the environment.

		Called by AddActiveObject.

		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	uint16_t AddActiveObjectRaw(LogicActiveObject* object, bool setChanged, unsigned int dTime);

	/*
		Remove all objects that satisfy (isGone() && m_known_by_count==0)
	*/
	void RemoveRemovedObjects();

	/*
		Convert stored objects from block to active
	*/
	void ActivateObjects(MapBlock* block, unsigned int dTime);

	/*
		Convert objects that are not in active blocks to static.

		If m_known_by_count != 0, active object is not deleted, but static
		data is still updated.

		If forceDelete is set, active object is deleted nevertheless. It
		shall only be set so in the destructor of the environment.
	*/
	void DeactivateFarObjects(bool forceDelete);

	/*
		A few helpers used by the three above methods
	*/
	void DeleteStaticFromBlock(
			LogicActiveObject* obj, uint16_t id, unsigned int modReason, bool noEmerge);
	bool SaveStaticToBlock(Vector3<short> blockpos, uint16_t storeId,
			LogicActiveObject* obj, const StaticObject& sObj, unsigned int modReason);

	/*
		Member variables
	*/

    // If true, do not allow multiple players and hide some multiplayer
    // functionality
    bool mSimpleSingleplayerMode;

	// The map
	std::shared_ptr<LogicMap> mMap;
	// Active Object Manager
    LogicActiveObjectManager mActiveObjectMgr;
	// World path
	const std::string mWorldPath;
	// Outgoing network message buffer for active objects
	std::queue<ActiveObjectMessage> mActiveObjectMessages;
	// Some timers
	float mSendRecommendedTimer = 0.0f;
	IntervalLimiter mObjectManagementInterval;

	// Sounds
	std::unordered_map<int, SoundPlaying> mPlayingSounds;
	int mNextSoundId = 0; // positive values only
	int NextSoundId();

    // managers
    std::unique_ptr<LogicInventoryManager> mInventoryMgr = nullptr;
    std::shared_ptr<BaseWritableCraftManager> mCraftMgr = nullptr;
	std::shared_ptr<BaseItemManager> mItemMgr = nullptr;
	std::shared_ptr<NodeManager> mNodeMgr = nullptr;

    /*
        Blocks that have been sent to visual.
        - These don't have to be sent again.
        - A block is cleared from here when visual says it has deleted it from it's memory

        List of block positions.
        No MapBlock* is stored here because the blocks can get deleted.
    */
    std::set<Vector3<short>> mBlocksSent;
    int16_t mNearestUnsentDist = 0;
    Vector3<short> mLastCenter = Vector3<short>::Zero();
    Vector3<float> mLastCameraDir = Vector3<float>::Zero();

    const uint16_t mMaxSimulSends;
    const float mMinTimeFromBuilding;
    const int16_t mMaxSendDist;
    const int16_t mBlockOptimizeDist;
    const int16_t mMaxGenDist;
    const bool mOcclusionCull;

    /*
        Blocks that are currently on the line.
        This is used for throttling the sending of blocks.
        - The size of this list is limited to some value
        Block is added when it is sent with BLOCKDATA.
        Block is removed when GOTBLOCKS is received.
        Value is time from sending. (not used at the moment)
    */
    std::map<Vector3<short>, float> mBlocksSending;

    /*
        Blocks that have been modified since blocks were
        sent to the visual last (getNextBlocks()).
        This is used to reset the unsent distance, so that
        modified blocks are resent to the visual.

        List of block positions.
    */
    std::set<Vector3<short>> mBlocksModified;

    /*
        Count of excess GotBlocks().
        There is an excess amount because the visual sometimes
        gets a block so late that the logic sends it again,
        and the visual then sends two GotBlocks.
        This is resetted by PrintInfo()
    */
    unsigned int mExcessGotBlocks = 0;

	// CPU usage optimization
	float mNothingToSendPauseTimer = 0.0f;

	// List of active blocks
	ActiveBlockList mActiveBlocks;
	IntervalLimiter mActiveBlocksManagementInterval;
	IntervalLimiter mActiveBlockModifierInterval;
	IntervalLimiter mActiveBlocksNodeMetadataInterval;
	// Whether the variables below have been read from file yet
	bool mMetaLoaded = false;
	// Time from the beginning of the game in seconds.
	// Incremented in step().
	unsigned int mGameTime = 0;
	// A helper variable for incrementing the latter
	float mGameTimeFractionCounter = 0.0f;
	// Time of last ClearObjects call (game time).
	// When a mapblock older than this is loaded, its objects are cleared.
	unsigned int mLastClearObjectsTime = 0;
	// Active block modifiers
    std::vector<ABMWithState> mABMs;
	LBMManager mLBMMgr;
	// An interval for generally sending object positions and stuff
	float mRecommendedSendInterval = 0.1f;
	// Estimate for general maximum lag as determined by logic.
	// Can raise to high values like 15s with eg. map generation mods.
	float mMaxLagEstimate = 0.1f;

	// actorIds in here should be unique, except that there may be many 0s
	std::vector<std::shared_ptr<LogicPlayer>> mPlayers;

    std::list<std::string> mPlayerNames;

	PlayerDatabase* mPlayerDatabase = nullptr;
	AuthDatabase* mAuthDatabase = nullptr;

	// Particles
	IntervalLimiter mParticleManagementInterval;
	std::unordered_map<unsigned int, float> mParticleSpawners;
    std::unordered_map<unsigned int, uint16_t> mParticleSpawnerAttachments;

	LogicActiveObject* CreateLAO(ActiveObjectType type, Vector3<float> pos, const std::string& data);
};

#endif