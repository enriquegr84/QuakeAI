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

#include "LogicEnvironment.h"

#include "../Actors/LogicPlayer.h"
#include "../Actors/EntityLAO.h"
#include "../Actors/PlayerLAO.h"
#include "../Actors/Craft.h"

#include "../Map/Emerge.h"
#include "../Map/MapGenerator.h"
#include "../Map/MapSector.h"
#include "../Map/MapBlock.h"
#include "../Map/Map.h"

#include "../../Physics/Raycast.h"

#include "../../Utils/FacePositionCache.h"

#include "../../Data/Database.h"

#include "../../Graphics/Node.h"

#include "../../Games/Games.h"

#include "Core/IO/FileSystem.h"
#include "Core/Utility/Profiler.h"

#include "../../MinecraftEvents.h"

#include <random>

#define LBM_NAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz0123456789_:"

// A number that is much smaller than the timeout for particle spawners should/could ever be
#define PARTICLE_SPAWNER_NO_EXPIRY -1024.f

Vector3<float> SoundParams::GetPosition(LogicEnvironment* env, bool* posExists) const
{
    if (posExists) *posExists = false;
    switch (type) 
    {
        case SP_LOCAL:
        {
            return Vector3<float>::Zero();
        }
        case SP_POSITIONAL:
        {
            if (posExists) *posExists = true;
            return position;
        }
        case SP_OBJECT: 
        {
            if (object == 0)
                return  Vector3<float>::Zero();
            LogicActiveObject* playerLAO = env->GetActiveObject(object);
            if (!playerLAO)
                return  Vector3<float>::Zero();
            if (posExists) *posExists = true;
            return playerLAO->GetBasePosition(); 
        }
    }
    return Vector3<float>::Zero();
}


/* ABMWithState */
ABMWithState::ABMWithState(ActiveBlockModifier* abm) : activeBlockModifier(abm)
{
	// Initialize timer to random value to spread processing
	float itv = abm->GetTriggerInterval();
	itv = std::max(0.001f, itv); // No less than 1ms
	int minval = (int)std::max(-0.51f*itv, -60.f); // Clamp to
	int maxval = (int)std::min(0.51f*itv, 60.f);   // +-60 seconds

    PcgRandom pcgRand;
	timer = (float)pcgRand.Range(minval, maxval);
}

/*
	LBMManager
*/
void LBMContentMapping::DeleteContents()
{
	for (auto& it : lbmList)
		delete it;
}

void LBMContentMapping::AddLBM(LoadingBlockModifier* lbm, LogicEnvironment* env)
{
	// Add the lbm to the LBMContentMapping.
	// Unknown names get added to the global NameIdMapping.
	lbmList.push_back(lbm);

	for (const std::string& nodeTrigger: lbm->mTriggerContents) 
    {
		std::vector<uint16_t> contentIds;
		bool found = env->GetNodeManager()->GetIds(nodeTrigger, contentIds);
		if (!found) 
        {
			uint16_t contentId = env->GetNodeManager()->AllocateDummy(nodeTrigger);
			if (contentId == CONTENT_IGNORE) 
            {
				// Seems it can't be allocated.
				LogWarning("Could not internalize node name \"" + nodeTrigger +
                    "\" while loading LBM \"" + lbm->mName + "\".");
				continue;
			}
			contentIds.push_back(contentId);
		}

		for (uint16_t contentId : contentIds)
			map[contentId].push_back(lbm);
	}
}

const std::vector<LoadingBlockModifier*>* LBMContentMapping::Lookup(uint16_t c) const
{
	LBMMap::const_iterator it = map.find(c);
	if (it == map.end())
		return NULL;
	// This first dereferences the iterator, returning
	// a std::vector<LoadingBlockModifier *>
	// reference, then we convert it to a pointer.
	return &(it->second);
}

LBMManager::~LBMManager()
{
	for (auto& lbm : mLBMs)
		delete lbm.second;

	for (auto &it : mLBMLookup)
		(it.second).DeleteContents();
}

void LBMManager::AddLBM(LoadingBlockModifier* lbm)
{
	// Precondition, in query mode the map isn't used anymore
	LogAssert(mQueryMode, "attempted to modify LBMManager in query mode");

	if (!StringAllowed(lbm->mName, LBM_NAME_ALLOWED_CHARS)) 
    {
		throw ModError("Error adding LBM \"" + lbm->mName +
			"\": Does not follow naming conventions: "
				"Only characters [a-z0-9_:] are allowed.");
	}

	mLBMs[lbm->mName] = lbm;
}

void LBMManager::LoadIntroductionTimes(const std::string& times, LogicEnvironment* env, unsigned int now)
{
	mQueryMode = true;

	// name -> time map.
	// Storing it in a map first instead of
	// handling the stuff directly in the loop
	// removes all duplicate entries.
	// TODO make this std::unordered_map
	std::map<std::string, unsigned int> introductionTimes;

	/*
	The introduction times string consists of name~time entries,
	with each entry terminated by a semicolon. The time is decimal.
	 */

	size_t idx = 0, idxNew;
	while ((idxNew = times.find(';', idx)) != std::string::npos)
    {
        std::string entry = times.substr(idx, idxNew - idx);
        std::vector<std::string> components = StringSplit(entry, '~');
		if (components.size() != 2)
			throw SerializationError("Introduction times entry \""
				+ entry + "\" requires exactly one '~'!");
		const std::string& name = components[0];
		unsigned int time = atoi(components[1].c_str());
		introductionTimes[name] = time;
		idx = idxNew + 1;
	}

	// Put stuff from introductionTimes into mLBMLookup
    std::map<std::string, unsigned int>::const_iterator it;
	for (it = introductionTimes.begin(); it != introductionTimes.end(); ++it) 
    {
		const std::string& name = it->first;
		unsigned int time = it->second;

		std::map<std::string, LoadingBlockModifier *>::iterator defIt = mLBMs.find(name);
		if (defIt == mLBMs.end()) 
        {
			// This seems to be an LBM entry for
			// an LBM we haven't loaded. Discard it.
			continue;
		}
		LoadingBlockModifier* lbm = defIt->second;
		if (lbm->mRunAtEveryLoad) 
        {
			// This seems to be an LBM entry for
			// an LBM that runs at every load.
			// Don't add it just yet.
			continue;
		}
		mLBMLookup[time].AddLBM(lbm, env);

		// Erase the entry so that we know later
		// what elements didn't get put into mLBMLookup
		mLBMs.erase(name);
	}

	// Now also add the elements from mLBMs to mLBMLookup
	// that weren't added in the previous step.
	// They are introduced first time to this world,
	// or are run at every load (introducement time hardcoded to U32_MAX).

	LBMContentMapping& lbmsAdded = mLBMLookup[now];
	LBMContentMapping& lbmsRunning = mLBMLookup[0xFFFFFFFF];

	for (auto &m_lbm : mLBMs) 
    {
		if (m_lbm.second->mRunAtEveryLoad) 
			lbmsRunning.AddLBM(m_lbm.second, env);
        else 
			lbmsAdded.AddLBM(m_lbm.second, env);
	}

	// Clear the list, so that we don't delete remaining elements
	// twice in the destructor
	mLBMs.clear();
}

std::string LBMManager::CreateIntroductionTimesString()
{
	// Precondition, we must be in query mode
	LogAssert(mQueryMode, "attempted to query on non fully set up LBMManager");

	std::ostringstream oss;
	for (const auto& it : mLBMLookup) 
    {
		unsigned int time = it.first;
		const std::vector<LoadingBlockModifier *>& lbmList = it.second.lbmList;
		for (const auto &lbm : lbmList) 
        {
			// Don't add if the LBM runs at every load,
			// then introducement time is hardcoded
			// and doesn't need to be stored
			if (lbm->mRunAtEveryLoad)
				continue;
			oss << lbm->mName << "~" << time << ";";
		}
	}
	return oss.str();
}

void LBMManager::ApplyLBMs(LogicEnvironment* env, MapBlock* block, unsigned int stamp)
{
	// Precondition, we need mLBMLookup to be initialized
	LogAssert(mQueryMode, "attempted to query on non fully set up LBMManager");
	Vector3<short> posBlock = block->GetRelativePosition();
	Vector3<short> pos;
	MapNode node;
	uint16_t content;
	LBMLookupMap::const_iterator it = GetLBMsIntroducedAfter(stamp);
	for (; it != mLBMLookup.end(); ++it) 
    {
		// Cache previous version to speedup lookup which has a very high performance
		// penalty on each call
		uint16_t previousContent{};
        std::vector<LoadingBlockModifier *> *lbmList = nullptr;

        for (pos[0] = 0; pos[0] < MAP_BLOCKSIZE; pos[0]++)
        {
            for (pos[1] = 0; pos[1] < MAP_BLOCKSIZE; pos[1]++)
            {
                for (pos[2] = 0; pos[2] < MAP_BLOCKSIZE; pos[2]++)
                {
                    node = block->GetNodeNoEx(pos);
                    content = node.GetContent();

                    // If uint16_t are not matching perform an LBM lookup
                    if (previousContent != content) 
                    {
                        lbmList = (std::vector<LoadingBlockModifier *> *)
                            it->second.Lookup(content);
                        previousContent = content;
                    }

                    if (!lbmList)
                        continue;
                    for (auto lbmdef : *lbmList)
                        lbmdef->Trigger(env, pos + posBlock, node);
                }
            }
        }
	}
}

void LBM::Trigger(LogicEnvironment* env, Vector3<short> pos, MapNode n)
{

}

/*
	ActiveBlockList
*/

void FillRadiusBlock(Vector3<short> p0, short r, std::set<Vector3<short>> &list)
{
	Vector3<short> p;
    for (p[0] = p0[0] - r; p[0] <= p0[0] + r; p[0]++)
    {
        for (p[1] = p0[1] - r; p[1] <= p0[1] + r; p[1]++)
        {
            for (p[2] = p0[2] - r; p[2] <= p0[2] + r; p[2]++)
            {
                // limit to a sphere
                if (Length(p - p0) <= r)
                {
                    // Set in list
                    list.insert(p);
                }
            }
        }
    }
}

void FillViewConeBlock(Vector3<short> p0, const short r,
	const Vector3<float> cameraPos, const Vector3<float> cameraDir,
	const float cameraFov, std::set<Vector3<short>>& list)
{
	Vector3<short> p;
	const short rNodes = (short)(r * BS * MAP_BLOCKSIZE);
    for (p[0] = p0[0] - r; p[0] <= p0[0] + r; p[0]++)
    {
        for (p[1] = p0[1] - r; p[1] <= p0[1] + r; p[1]++)
        {
            for (p[2] = p0[2] - r; p[2] <= p0[2] + r; p[2]++) 
            {
                if (IsBlockInsight(p, cameraPos, cameraDir, cameraFov, rNodes))
                    list.insert(p);
            }
        }
    }
}

void ActiveBlockList::Update(
    std::vector<PlayerLAO*>& activePlayers, short activeBlockRange, short activeObjectRange,
    std::set<Vector3<short>>& blocksRemoved, std::set<Vector3<short>>& blocksAdded)
{
	/*
		Create the new list
	*/
	std::set<Vector3<short>> newlist = mForceloadedList;
	mABMList = mForceloadedList;
	for (const PlayerLAO* playerLAO : activePlayers) 
    {
        Vector3<float> p = playerLAO->GetBasePosition();
        Vector3<short> pos;
        pos[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos = GetNodeBlockPosition(pos);

		FillRadiusBlock(pos, activeBlockRange, mABMList);
		FillRadiusBlock(pos, activeBlockRange, newlist);

		short playerAORange = std::min(activeObjectRange, playerLAO->GetWantedRange());
		// only do this if this would add blocks
		if (playerAORange > activeBlockRange) 
        {
            Vector3<float> cameraDir = Vector3<float>{ 0,0,1 };

			Quaternion<float> tgt = Rotation<3, float>(
				AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -playerLAO->GetLookPitch() * (float)GE_C_DEG_TO_RAD));
			cameraDir = HProject(Rotate(tgt, HLift(cameraDir, 0.f)));
			//cameraDir.rotateYZBy(playerLAO->GetLookPitch());

			tgt = Rotation<3, float>(
				AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), playerLAO->GetRotation()[1] * (float)GE_C_DEG_TO_RAD));
			cameraDir = HProject(Rotate(tgt, HLift(cameraDir, 0.f)));
			//cameraDir.rotateXZBy(playerLAO->GetRotation()[1]);

			FillViewConeBlock(pos, playerAORange, 
                playerLAO->GetEyePosition(), cameraDir, playerLAO->GetFov(), newlist);
		}
	}

	/*
		Find out which blocks on the old list are not on the new list
	*/
	// Go through old list
	for (Vector3<short> p : mList) 
    {
		// If not on new list, it's been removed
		if (newlist.find(p) == newlist.end())
			blocksRemoved.insert(p);
	}

	/*
		Find out which blocks on the new list are not on the old list
	*/
	// Go through new list
	for (Vector3<short> p : newlist) 
    {
		// If not on old list, it's been added
		if(mList.find(p) == mList.end())
			blocksAdded.insert(p);
	}

	/*
		Update mList
	*/
	mList.clear();
	for (Vector3<short> p : newlist)
		mList.insert(p);
}

void ABM::Trigger(LogicEnvironment* env, Vector3<short> p, MapNode n,
	unsigned int activeObjectCount, unsigned int activeObjectCountWider)
{
	BaseGame::Get()->OnActionABM(this, p, n, activeObjectCount, activeObjectCountWider);
}

/*
	LogicEnvironment
*/
LogicEnvironment::LogicEnvironment(const std::string& pathWorld) 
    : Environment(), mSimpleSingleplayerMode(true), mWorldPath(pathWorld),
    mMaxSimulSends(Settings::Get()->GetUInt16("max_simultaneous_block_sends_per_client")),
    mMinTimeFromBuilding(Settings::Get()->GetFloat("full_block_send_enable_min_time_from_building")),
    mMaxSendDist(Settings::Get()->GetInt16("max_block_send_distance")),
    mBlockOptimizeDist(Settings::Get()->GetInt16("block_send_optimize_distance")),
    mMaxGenDist(Settings::Get()->GetInt16("max_block_generate_distance")),
    mOcclusionCull(Settings::Get()->GetBool("server_side_occlusion_culling"))
{
	mItemMgr = CreateItemManager();
	mNodeMgr = CreateNodeManager();

    // Must be created before mod loading because we have some inventory creation
    mInventoryMgr = std::unique_ptr<LogicInventoryManager>(new LogicInventoryManager());

    mCraftMgr = CreateCraftManager();

	// Determine which database backend to use
	std::string confPath = pathWorld + "/world.mt";
    std::string playerBackendName = "bin";
    std::string authBackendName = "bin";

	bool succeeded = Settings::Get()->ReadConfigFile(confPath.c_str());

	// If we open world.mt read the backend configurations.
	if (succeeded) 
    {
		// Read those values before setting defaults
		bool playerBackendExists = Settings::Get()->Exists("player_backend");
		bool authBackendExists = Settings::Get()->Exists("auth_backend");

		// player backend is not set, assume it's legacy file backend.
		if (!playerBackendExists) 
        {
			// fall back to files
            Settings::Get()->Set("player_backend", "files");
			playerBackendName = "files";

			if (!Settings::Get()->UpdateConfigFile(confPath.c_str())) 
            {
				LogWarning("LogicEnvironment::LogicEnvironment(): Failed to update world.mt!");
			}
		} 
        else 
        {
            playerBackendName = Settings::Get()->Get("player_backend");
		}

		// auth backend is not set, assume it's legacy file backend.
		if (!authBackendExists) 
        {
            Settings::Get()->Set("auth_backend", "files");
			authBackendName = "files";

			if (!Settings::Get()->UpdateConfigFile(confPath.c_str())) 
            {
				LogWarning("LogicEnvironment::LogicEnvironment(): Failed to update world.mt!");
			}
		} 
        else 
        {
            authBackendName = Settings::Get()->Get("auth_backend");
		}
	}

	if (playerBackendName == "files") 
    {
		LogWarning("/!\\ You are using old player file backend. ");
	}

	if (authBackendName == "files") 
    {
		LogWarning("/!\\ You are using old auth file backend. ");
	}

	mPlayerDatabase = OpenPlayerDatabase(playerBackendName, pathWorld);
	mAuthDatabase = OpenAuthDatabase(authBackendName, pathWorld);

    mMetricsBackend = std::make_unique<MetricsBackend>();
    mUptimeCounter = mMetricsBackend->AddCounter("minetest_core_server_uptime", "Logic uptime (in seconds)");
    mPlayerGauge = mMetricsBackend->AddGauge("minetest_core_player_number", "Number of connected players");
    mTimeofDayGauge = mMetricsBackend->AddGauge("minetest_core_timeofday", "Time of day value");
    mAOMBufferCounter = mMetricsBackend->AddCounter("minetest_core_aom_generated_count",
        "Number of active object messages generated");

    // Create the Map (loads map_meta.txt, overriding configured mapgen params)
    mMap = std::make_shared<LogicMap>(mWorldPath, mMetricsBackend.get(), this);

	// Register us to receive map edit events
	mMap->AddEventReceiver(this);
}

LogicEnvironment::~LogicEnvironment()
{
	// Clear active block list.
	// This makes the next one delete all active objects.
	mActiveBlocks.Clear();

	// Convert all objects to static and delete the active objects
	DeactivateFarObjects(true);

	// Drop/delete map
    mMap.reset();

	// Delete ActiveBlockModifiers
	for (ABMWithState& abm : mABMs)
		delete abm.activeBlockModifier;

	// Deallocate players
	mPlayers.clear();

	delete mPlayerDatabase;
	delete mAuthDatabase;

    // Save local logic map
    if (mLocalDB)
    {
        LogInformation("Local map saving ended.");
        mLocalDB->EndSave();
    }
}

EntityLAO* GetAttachedObject(PlayerLAO* playerLAO, LogicEnvironment* env)
{
    if (!playerLAO->IsAttached())
        return nullptr;

    int id;
    bool forceVisible;
    std::string bone;
    Vector3<float> dummy;
    playerLAO->GetAttachment(&id, &bone, &dummy, &dummy, &forceVisible);
    LogicActiveObject* ao = env->GetActiveObject(id);
    while (id && ao)
    {
        ao->GetAttachment(&id, &bone, &dummy, &dummy, &forceVisible);
        if (id)
            ao = env->GetActiveObject(id);
    }
    return dynamic_cast<EntityLAO *>(ao);
}

void LogicEnvironment::SendBlockNoLock(ActorId actorId, MapBlock* block, uint8_t ver)
{
    /*
        Create a packet with the block in the right format
    */
    thread_local const int netCompressionLevel =
        std::clamp(Settings::Get()->GetInt("map_compression_level_net"), -1, 9);
    std::ostringstream os(std::ios_base::binary);
    block->Serialize(os, ver, false, netCompressionLevel);
    block->SerializeNetworkSpecific(os);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHandleBlockData>(os.str(), block->GetPosition()));
}

bool LogicEnvironment::SendBlock(ActorId actorId, const Vector3<short>& blockPos)
{
	mEnvRecMutex.lock();

    MapBlock* block = GetMap()->GetBlockNoCreateNoEx(blockPos);
	if (!block) {
		mEnvRecMutex.unlock();
		return false;
	}

	if (IsBlockSent(blockPos)) {
		mEnvRecMutex.unlock();
		return false;
	}

    uint8_t version = SER_FMT_VER_HIGHEST_READ;
    SendBlockNoLock(actorId, block, version);
	mEnvRecMutex.unlock();
    return true;
}

void LogicEnvironment::GetNextBlocks(
    ActorId actorId, float dTime, std::vector<PrioritySortedBlockTransfer>& dest)
{
	// Increment timers
	mNothingToSendPauseTimer -= dTime;

	if (mNothingToSendPauseTimer >= 0)
		return;

    std::shared_ptr<LogicPlayer> player = GetPlayer(actorId);
    // This can happen sometimes; visuals and logic players are not in perfect sync.
    if (!player)
        return;

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (!playerLAO)
        return;

	// Won't send anything if already sending
	if (mBlocksSending.size() >= mMaxSimulSends) {
		//LogInformation("Not sending any blocks, Queue full.");
		return;
	}

    Vector3<float> playerPos = playerLAO->GetBasePosition();
    // if the player is attached, get the velocity from the attached object
    EntityLAO* entityPlayerLAO = GetAttachedObject(playerLAO, this);
    const Vector3<float>& playerSpeed = entityPlayerLAO ? entityPlayerLAO->GetVelocity() : player->GetSpeed();
    Vector3<float> playerSpeedDir = Vector3<float>::Zero();
    if (Length(playerSpeed) > 1.0f * BS)
        playerSpeedDir = playerSpeed / Length(playerSpeed);
    // Predict to next block
    Vector3<float> playerPosPredicted = playerPos + playerSpeedDir * (MAP_BLOCKSIZE * BS);

    Vector3<short> centerNodePos;
    centerNodePos[0] = 
        (short)((playerPosPredicted[0] + (playerPosPredicted[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    centerNodePos[1] = 
        (short)((playerPosPredicted[1] + (playerPosPredicted[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    centerNodePos[2] = 
        (short)((playerPosPredicted[2] + (playerPosPredicted[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    Vector3<short> center = GetNodeBlockPosition(centerNodePos);

    // Camera position and direction
    Vector3<float> cameraPos = playerLAO->GetEyePosition();
    Vector3<float> cameraDir = Vector3<float>{ 0, 0, 1 };

	Quaternion<float> tgt = Rotation<3, float>(
		AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -playerLAO->GetLookPitch() * (float)GE_C_DEG_TO_RAD));
	cameraDir = HProject(Rotate(tgt, HLift(cameraDir, 0.f)));
	//cameraDir.rotateYZBy(playerLAO->GetLookPitch());

	tgt = Rotation<3, float>(
		AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), playerLAO->GetRotation()[1] * (float)GE_C_DEG_TO_RAD));
	cameraDir = HProject(Rotate(tgt, HLift(cameraDir, 0.f)));
	//cameraDir.rotateXZBy(playerLAO->GetRotation()[1]);
	//printf("\n\n pitch %f yaw rot %f ", playerLAO->GetLookPitch(), playerLAO->GetRotation()[1]);
	//printf("\n cameraDir %f %f %f ", cameraDir[0], cameraDir[1], cameraDir[2]);
    uint16_t maxSimulSendsUsually = mMaxSimulSends;

    /*
        Check the time from last addNode/removeNode.
        Decrease send rate if player is building stuff.
    */
    mTimeFromBuilding += dTime;
    if (mTimeFromBuilding < mMinTimeFromBuilding)
        maxSimulSendsUsually = LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS;

    /*
        Number of blocks sending + number of blocks selected for sending
    */
    unsigned int numBlocksSelected = (unsigned int)mBlocksSending.size();

    /*
        next time d will be continued from the d from which the nearest
        unsent block was found this time.

        This is because not necessarily any of the blocks found this
        time are actually sent.
    */
    int newNearestUnsentDistance = -1;

    // Get view range and camera fov (radians) from the visual
    int16_t wantedRange = playerLAO->GetWantedRange() + 1;
    float cameraFOV = playerLAO->GetFov();

    /// Get the starting value of the block finder radius.
    if (mLastCenter != center)
    {
        mNearestUnsentDist = 0;
        mLastCenter = center;
    }
    // reset the unsent distance if the view angle has changed more that 10% of the fov
    // (this matches IsBlockInsight which allows for an extra 10%)
    if (Dot(cameraDir, mLastCameraDir) < std::cos(cameraFOV * 0.1f))
    {
        mNearestUnsentDist = 0;
        mLastCameraDir = cameraDir;
    }
    if (mNearestUnsentDist > 0)
    {
        // make sure any blocks modified since the last time we sent blocks are resent
		for (const Vector3<short>& blockPos : mBlocksModified)
		{
			//printf("blockPos %i %i %i dist %i ", blockPos[0], blockPos[1], blockPos[2], Length(center - blockPos));
			mNearestUnsentDist = std::min(mNearestUnsentDist, Length(center - blockPos));
		}
    }
	//printf("\n\nnearest dist %i ", mNearestUnsentDist);
    mBlocksModified.clear();

    int16_t distStart = mNearestUnsentDist;

    // Distrust visual-sent FOV and get logic-set player object property
    // zoom FOV (degrees) as a check to avoid hacked visuals using FOV to load
    // distant world.
    // (zoom is disabled by value 0)
    float propZoomFov = playerLAO->GetZoomFOV() < 0.001f ?
        0.0f : std::max(cameraFOV, playerLAO->GetZoomFOV() * (float)GE_C_DEG_TO_RAD);

    const int16_t fullDistMax = std::min(AdjustDistance(mMaxSendDist, propZoomFov), wantedRange);
    const int16_t distOpt = std::min(AdjustDistance(mBlockOptimizeDist, propZoomFov), wantedRange);
    const int16_t distBlocksInSight = (int16_t)(fullDistMax * BS * MAP_BLOCKSIZE);

    int16_t distMaxGen = std::min(AdjustDistance(mMaxGenDist, propZoomFov), wantedRange);

    int16_t distMax = fullDistMax;

    // Don't loop very much at a time
    int16_t maxDistIncrementAtTime = 2;
    if (distMax > distStart + maxDistIncrementAtTime)
        distMax = distStart + maxDistIncrementAtTime;

    // cos(angle between velocity and camera) * |velocity|
    // Limit to 0.0f in case player moves backwards.
    float dot = std::clamp(Dot(cameraDir, playerSpeed), 0.0f, 300.0f);

    // Reduce the field of view when a player moves and looks forward.
    // limit max fov effect to 50%, 60% at 20n/s fly speed
    cameraFOV = cameraFOV / (1 + dot / 300.0f);

    int nearestEmergedDist = -1;
    int nearestEmergeFullDist = -1;
    int nearestSentDist = -1;
    //bool queue_is_full = false;

    Vector3<short> camPosNodes;
    camPosNodes[0] = (short)((cameraPos[0] + (cameraPos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    camPosNodes[1] = (short)((cameraPos[1] + (cameraPos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    camPosNodes[2] = (short)((cameraPos[2] + (cameraPos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    int16_t distCurrent;
    for (distCurrent = distStart; distCurrent <= distMax; distCurrent++)
    {
        /*
            Get the border/face dot coordinates of a "d-radiused"
            box
        */
        std::vector<Vector3<short>> list = FacePositionCache::GetFacePositions(distCurrent);
		//printf("\n\nlist size %u ", list.size());
        std::vector<Vector3<short>>::iterator li;
        for (li = list.begin(); li != list.end(); ++li)
        {
            Vector3<short> p = *li + center;
			//printf("pos %i %i %i ", p[0], p[1], p[2]);
            /*
                Send throttling
                - Don't allow too many simultaneous transfers
                - EXCEPT when the blocks are very close

                Also, don't send blocks that are already flying.
            */

			// Start with the usual maximum
			uint16_t maxSimulDynamic = maxSimulSendsUsually;
			// If block is very close, allow full maximum
			if (distCurrent <= BLOCK_SEND_DISABLE_LIMITS_MAX_D)
				maxSimulDynamic = mMaxSimulSends;

			// Don't select too many blocks for sending
			if (numBlocksSelected >= maxSimulDynamic) {
				//queue_is_full = true;
				//printf("max block sent reached ");
				goto QueueFullBreak;
			}

            // Don't send blocks that are currently being transferred
			if (mBlocksSending.find(p) != mBlocksSending.end())
			{
				//printf("block is being sent ");
				continue;
			}


            /*
                Do not go over max mapgen limit
            */
			if (BlockPositionOverMaxLimit(p))
			{
				//printf("block over max limit ");
				continue;
			}


            // If this is true, inexistent block will be made from scratch
            bool generate = distCurrent <= distMaxGen;

            /*
                Don't generate or send if not in sight
                FIXME This only works if the visual uses a small enough
                FOV setting. The default of 72 degrees is fine.
                Also retrieve a smaller view cone in the direction of the player's
                movement.
                (0.1 is about 4 degrees)
            */
            float dist;
            if (!(IsBlockInsight(p, cameraPos, cameraDir, cameraFOV, distBlocksInSight, &dist) ||
                (Length(playerSpeed) > 1.0f * BS &&
                    IsBlockInsight(p, cameraPos, playerSpeedDir, 0.1f, distBlocksInSight))))
            {
				//printf("not in sight %i %f ", distBlocksInSight, dist);
                continue;
            }

            /*
                Don't send already sent blocks
            */
            if (mBlocksSent.find(p) != mBlocksSent.end())
			{
				//printf("block already sent ");
				continue;
			}

            /*
                Check if map has this block
            */
            MapBlock* block = GetMap()->GetBlockNoCreateNoEx(p);

            bool blockNotFound = false;
            if (block)
            {
                // Reset usage timer, this block will be of use in the future.
                block->ResetUsageTimer();

                // Check whether the block exists (with data)
                if (block->IsDummy() || !block->IsGenerated())
                    blockNotFound = true;

                /*
                    If block is not close, don't send it unless it is near
                    ground level.

                    Block is near ground level if night-time mesh
                    differs from day-time mesh.
                */
                if (distCurrent >= distOpt)
					if (!block->IsUnderground() && !block->GetDayNightDiff())
					{
						//printf("block is not closed ");
						continue;
					}

                if (mOcclusionCull && !blockNotFound &&
                    GetMap()->IsBlockOccluded(block, camPosNodes))
                {
					//printf("block is occluded ");
                    continue;
                }
            }

            /*
                If block has been marked to not exist on disk (dummy) or is
                not generated and generating new ones is not wanted, skip block.
            */
            if (!generate && blockNotFound)
            {
                // get next one.
				//printf("block not existing get next one1 ");
                continue;
            }

            /*
                Add inexistent block to emerge queue.
            */
            if (block == NULL || blockNotFound)
            {
                if (EmergeManager::Get()->EnqueueBlockEmerge(actorId, p, generate))
                {
					//printf("succeded ");
                    if (nearestEmergedDist == -1)
                        nearestEmergedDist = distCurrent;
                }
                else
                {
					//printf("failed ");
                    if (nearestEmergeFullDist == -1)
                        nearestEmergeFullDist = distCurrent;
                    goto QueueFullBreak;
                }

                // get next one.
				//printf("block not existing get next one2 ");
                continue;
            }

            if (nearestSentDist == -1)
                nearestSentDist = distCurrent;

            /*
                Add block to send queue
            */
            PrioritySortedBlockTransfer q((float)dist, p, actorId);

            dest.push_back(q);

            numBlocksSelected += 1;
        }
    }
QueueFullBreak:

    // If nothing was found for sending and nothing was queued for
    // emerging, continue next time browsing from here
    if (nearestEmergedDist != -1)
    {
		//printf("nearestEmergedDist %i ", nearestEmergedDist);
        newNearestUnsentDistance = nearestEmergedDist;
    }
    else if (nearestEmergeFullDist != -1)
    {
		//printf("nearestEmergeFullDist %i ", nearestEmergeFullDist);
        newNearestUnsentDistance = nearestEmergeFullDist;
    }
    else
    {
        if (distCurrent > fullDistMax)
        {
			//printf("newNearestUnsentDistance 0 ");
            newNearestUnsentDistance = 0;
			mNothingToSendPauseTimer = 2.0f;
        }
        else
        {
			if (nearestSentDist != -1)
			{
				//printf("nearestSentDist %i ", nearestSentDist);
				newNearestUnsentDistance = nearestSentDist;
			}
			else
			{
				//printf("distCurrent %i ", distCurrent);
				newNearestUnsentDistance = distCurrent;
			}
        }
    }

    if (newNearestUnsentDistance != -1)
        mNearestUnsentDist = newNearestUnsentDistance;
}

void LogicEnvironment::GotBlock(Vector3<short> pos)
{
    if (mBlocksSending.find(pos) != mBlocksSending.end())
    {
        mBlocksSending.erase(pos);
        // only add to sent blocks if it actually was sending
        // (it might have been modified since)
        mBlocksSent.insert(pos);
    }
    else mExcessGotBlocks++;
}

void LogicEnvironment::SentBlock(Vector3<short> pos)
{
    if (mBlocksSending.find(pos) == mBlocksSending.end())
        mBlocksSending[pos] = 0.0f;
    else
        LogInformation("SentBlock(): Sent block already in mBlocksSending");
}

void LogicEnvironment::ResendBlockIfOnWire(Vector3<short> pos)
{
    // if this block is on wire, mark it for sending again as soon as possible
    if (mBlocksSending.find(pos) != mBlocksSending.end())
        SetBlockNotSent(pos);
}

void LogicEnvironment::SetBlockNotSent(Vector3<short> pos)
{
	mNothingToSendPauseTimer = 0;

    // remove the block from sending and sent sets,
    // and mark as modified if found
    if (mBlocksSending.erase(pos) + mBlocksSent.erase(pos) > 0)
        mBlocksModified.insert(pos);
}

void LogicEnvironment::SetBlocksNotSent(std::map<Vector3<short>, MapBlock*>& blocks)
{
	mEnvRecMutex.lock();

	mNothingToSendPauseTimer = 0;
    for (auto& block : blocks)
    {
        Vector3<short> pos = block.first;
        // remove the block from sending and sent sets,
        // and mark as modified if found
        if (mBlocksSending.erase(pos) + mBlocksSent.erase(pos) > 0)
            mBlocksModified.insert(pos);
    }
	mEnvRecMutex.unlock();
}

void LogicEnvironment::UpdateCrafting(LogicPlayer* player)
{
    InventoryList* clist = player->mInventory.GetList("craft");
    if (!clist || clist->GetSize() == 0)
        return;

    if (!clist->CheckModified())
        return;

    // Get a preview for crafting
    ItemStack preview;
    InventoryLocation loc;
    loc.SetPlayer(player->GetName());
    std::vector<ItemStack> outputReplacements;
    GetCraftingResult(&player->mInventory, preview, outputReplacements, false, this);
    BaseGame::Get()->OnCraftPredictItem(preview, player->GetPlayerLAO(), clist, loc);

    InventoryList* plist = player->mInventory.GetList("craftpreview");
    if (plist && plist->GetSize() >= 1)
    {
        // Put the new preview in
        plist->ChangeItem(0, preview);
    }
}

void LogicEnvironment::SendPlayerMove(PlayerLAO* playerLAO)
{
    // Send attachment updates instantly to the visual prior updating position
    playerLAO->SendOutdatedData();

	float yaw = playerLAO->GetRotation()[1] * (float)GE_C_DEG_TO_RAD;
	float pitch = playerLAO->GetLookPitch() * (float)GE_C_DEG_TO_RAD;
	float roll = playerLAO->GetRotation()[0] * (float)GE_C_DEG_TO_RAD;

	EulerAngles<float> yawPitchRoll;
	yawPitchRoll.mAxis[1] = 1;
	yawPitchRoll.mAxis[2] = 2;
	yawPitchRoll.mAngle[0] = roll;
	yawPitchRoll.mAngle[1] = yaw;
	yawPitchRoll.mAngle[2] = pitch;
    EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlayerMove>(
        playerLAO->GetId(), playerLAO->GetBasePosition(), yawPitchRoll));
}

void LogicEnvironment::SendPlayerBreath(PlayerLAO* playerLAO)
{
    LogAssert(playerLAO, "invalid player");

    BaseGame::Get()->OnEventPlayer(playerLAO, "breath_changed");

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerBreath>(playerLAO->GetId(), playerLAO->GetBreath()));
}

void LogicEnvironment::SendPlayerHPOrDie(PlayerLAO* playerLAO, const PlayerHPChangeReason& reason)
{
    if (playerLAO->IsImmortal())
        return;

    ActorId actorId = playerLAO->GetId();
    bool isAlive = !playerLAO->IsDead();

    if (isAlive)
        SendPlayerHP(actorId);
    else
        DiePlayer(actorId, reason);
}

void LogicEnvironment::SendPlayerHP(ActorId actorId)
{
    std::shared_ptr<LogicPlayer> player = GetPlayer(actorId);
    if (!player)
        return;

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    LogAssert(playerLAO, "invalid player");

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerHP>(playerLAO->GetId(), playerLAO->GetHP()));
    BaseGame::Get()->OnEventPlayer(playerLAO, "health_changed");

    // Send to other visuals
    playerLAO->SendPunchCommand();
}

void LogicEnvironment::DiePlayer(ActorId actorId, const PlayerHPChangeReason& reason)
{
    std::shared_ptr<LogicPlayer> player = GetPlayer(actorId);
    if (!player)
        return;

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    LogAssert(playerLAO, "invalid player");

    LogInformation("Player " + std::string(playerLAO->GetPlayer()->GetName()) + " dies");

    playerLAO->SetHP(0, reason);
    playerLAO->ClearParentAttachment();

    // Trigger scripted stuff
    BaseGame::Get()->OnDiePlayer(playerLAO, reason);

    SendPlayerHP(actorId);
    SendDeathScreen(actorId, false, Vector3<float>::Zero());
}

void LogicEnvironment::SendDeathScreen(ActorId actorId, bool setCameraPointTarget, Vector3<float> cameraPointTarget)
{
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataDeathScreen>(setCameraPointTarget, cameraPointTarget));
}

void LogicEnvironment::SendInventory(PlayerLAO* playerLAO, bool incremental)
{
    UpdateCrafting(playerLAO->GetPlayer());

    std::ostringstream os(std::ios::binary);
    playerLAO->GetInventory()->Serialize(os, incremental);
    playerLAO->GetInventory()->SetModified(false);
    playerLAO->GetPlayer()->SetModified(true);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHandleInventory>(playerLAO->GetId(), os.str()));
}

void LogicEnvironment::SendDetachedInventory(Inventory* inventory, const std::string& name, ActorId actorId)
{
    // Serialization 
    std::ostringstream os(std::ios_base::binary);
	bool keepInventory;

    if (!inventory)
    {
        // Remove inventory
		keepInventory = false;
    }
    else
    {
		keepInventory = true;

        // Update inventory
        inventory->Serialize(os);
        inventory->SetModified(false);
    }

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHandleDetachedInventory>(keepInventory, name, os.str()));
}

void LogicEnvironment::SendDetachedInventories(ActorId actorId, bool incremental)
{
    // Lookup player name, to filter detached inventories just after
    std::string actorName;

    auto SendCB = [this, actorId](const std::string& name, Inventory* inv)
    {
        SendDetachedInventory(inv, name, actorId);
    };

    mInventoryMgr->SendDetachedInventories(actorName, incremental, SendCB);
}

void LogicEnvironment::HandleDetachedInventory(const std::string& name, bool keepInv)
{

}

void LogicEnvironment::SendShowFormMessage(ActorId actorId, const std::string& form, const std::string& formName)
{
	if (form.empty())
	{
		//the visual should close the form
		//but make sure there wasn't another one open in meantime
		const auto it = mFormStateData.find(actorId);
		if (it != mFormStateData.end() && it->second == formName)
			mFormStateData.erase(actorId);
	}
	else mFormStateData[actorId] = formName;

	EventManager::Get()->QueueEvent(std::make_shared<EventDataShowForm>(form, formName));
}

std::shared_ptr<Map> LogicEnvironment::GetMap()
{
	return std::static_pointer_cast<Map>(mMap);
}

std::shared_ptr<LogicMap> LogicEnvironment::GetLogicMap()
{
	return mMap;
}

void LogicEnvironment::SetTimeOfDay(unsigned int time)
{
	Environment::SetTimeOfDay(time);
	mTimeOfDaySendTimer = 0.f;
}

void LogicEnvironment::OnMapEditEvent(const MapEditEvent& evt)
{
    if (mIgnoreMapEditEventsArea.Contains(evt.GetArea()))
        return;

    mUnsentMapEditQueue.push(new MapEditEvent(evt));
}

std::shared_ptr<LogicPlayer> LogicEnvironment::GetPlayer(const ActorId actorId)
{
	for (std::shared_ptr<LogicPlayer> player : mPlayers)
		if (player->GetId() == actorId)
			return player;

	return NULL;
}

std::shared_ptr<LogicPlayer> LogicEnvironment::GetPlayer(const char* name)
{
	for (std::shared_ptr<LogicPlayer> player : mPlayers)
		if (strcmp(player->GetName(), name) == 0)
			return player;

	return NULL;
}

void LogicEnvironment::AddPlayer(std::shared_ptr<LogicPlayer> player)
{
	/*
		Check that actorIds are unique.
		Also check that names are unique.
		Exception: there can be multiple players with actorId=0
	*/
	// If actor id is non-zero, it has to be unique.
	if (player->GetId() != INVALID_ACTOR_ID)
		LogAssert(GetPlayer(player->GetId()) == NULL, "Actor id not unique");
	// Name has to be unique.
    LogAssert(GetPlayer(player->GetName()) == NULL, "Player name not unique");
	// Add.
	mPlayers.push_back(player);
}

void LogicEnvironment::RemovePlayer(ActorId playerId)
{
    std::vector<std::shared_ptr<LogicPlayer>>::iterator it;
	for (it = mPlayers.begin();it != mPlayers.end(); ++it)
    {
		if ((*it)->GetId() == playerId)
        {
			mPlayers.erase(it);
			return;
		}
	}
}

bool LogicEnvironment::RemovePlayerFromDatabase(LogicPlayer* player)
{
	return mPlayerDatabase->RemovePlayer(player->GetPlayerLAO());
}

void LogicEnvironment::SaveLoadedPlayers(bool force)
{
	for (std::shared_ptr<LogicPlayer> player : mPlayers) 
    {
		if (force || player->CheckModified() || 
            (player->GetPlayerLAO() && player->GetPlayerLAO()->GetMeta().IsModified()))
        {
			try 
            {
				mPlayerDatabase->SavePlayer(player->GetPlayerLAO());
			} 
            catch (DatabaseException &e) 
            {
				LogWarning("Failed to save player " + std::string(player->GetName()) + 
                    " exception: " + e.what());
				throw;
			}
		}
	}
}

void LogicEnvironment::SavePlayer(LogicPlayer *player)
{
	try 
    {
		mPlayerDatabase->SavePlayer(player->GetPlayerLAO());
	} 
    catch (DatabaseException &e) 
    {
        LogWarning("Failed to save player " + std::string(player->GetName()) +
            " exception: " + e.what());
		throw;
	}
}

PlayerLAO* LogicEnvironment::LoadPlayer(std::shared_ptr<LogicPlayer> player, bool* newPlayer, bool isSingleplayer)
{
	PlayerLAO* playerLAO = new PlayerLAO(this, player.get(), isSingleplayer);
	// Create player if it doesn't exist
	if (!mPlayerDatabase->LoadPlayer(playerLAO))
    {
		*newPlayer = true;
		// Set player position
		LogInformation("Finding spawn place for player \"" + 
            std::string(player->GetName()) + "\"");
		playerLAO->SetBasePosition(FindSpawnPosition());

		// Make sure the player is saved
		player->SetModified(true);
	} 
    else 
    {
		// If the player exists, ensure that they respawn inside legal bounds
		// This fixes an assert crash when the player can't be added
		// to the environment
        
		if (ObjectPositionOverLimit(playerLAO->GetBasePosition())) 
        {
			LogInformation("Respawn position for player \"" + 
                std::string(player->GetName()) + "\" outside limits, resetting");
			playerLAO->SetBasePosition(FindSpawnPosition());
		}
	}

	// Add player to environment
	AddPlayer(player);

	/* Clean up old HUD elements from previous sessions */
	player->ClearHud();

	/* Add object to environment */
	AddActiveObject(playerLAO);

	return playerLAO;
}

void LogicEnvironment::SaveMeta()
{
	if (!mMetaLoaded)
		return;

	std::string path = mWorldPath + "/env_meta.txt";

	// Open file and serialize
	std::ostringstream ss(std::ios_base::binary);

	Settings args("EnvArgsEnd");
	args.SetUInt64("game_time", mGameTime);
	args.SetUInt64("time_of_day", GetTimeOfDay());
	args.SetUInt64("last_clear_objects_time", mLastClearObjectsTime);
	args.SetUInt64("lbm_introduction_times_version", 1);
	args.Set("lbm_introduction_times", mLBMMgr.CreateIntroductionTimesString());
	args.SetUInt64("day_count", mDayCount);
	args.WriteLines(ss);

	if(!FileSystem::Get()->SafeWriteToFile(path, ss.str()))
	{
		LogInformation("LogicEnvironment::SaveMeta(): Failed to write " + path);
		throw SerializationError("Couldn't save env meta");
	}
}

void LogicEnvironment::LoadMeta()
{
	LogAssert(!mMetaLoaded, "meta already loaded");
	mMetaLoaded = true;

	// If file doesn't exist, load default environment metadata
	if (!FileSystem::Get()->ExistFile(ToWideString(mWorldPath) + L"/env_meta.txt")) 
    {
		LogInformation("LogicEnvironment: Loading default environment metadata");
		LoadDefaultMeta();
		return;
	}

	LogInformation("LogicEnvironment: Loading environment metadata");

	std::string path = mWorldPath + "/env_meta.txt";

	// Open file and deserialize
	std::ifstream is(path, std::ios_base::binary);
	if (!is.good()) 
    {
        LogInformation("LogicEnvironment::LoadMeta(): Failed to open " + path);
		throw SerializationError("Couldn't load env meta");
	}

	Settings args("EnvArgsEnd");

	if (!args.ParseConfigLines(is)) 
    {
		throw SerializationError("LogicEnvironment::LoadMeta(): "
			"EnvArgsEnd not found!");
	}

	try 
    {
		mGameTime = args.GetUInt("game_time");
	} 
    catch (SettingNotFoundException&) 
    {
		// Getting this is crucial, otherwise timestamps are useless
		throw SerializationError("Couldn't load env meta game_time");
	}

	SetTimeOfDay(args.Exists("time_of_day") ?
		// set day to early morning by default
		args.GetUInt("time_of_day") : 5250);

	mLastClearObjectsTime = args.Exists("last_clear_objects_time") ?
		// If missing, do as if ClearObjects was never called
		args.GetUInt("last_clear_objects_time") : 0;

	std::string lbmIntroductionTimes;
	try 
    {
		uint64_t ver = args.GetUInt64("lbm_introduction_times_version");
		if (ver == 1) 
        {
			lbmIntroductionTimes = args.Get("lbm_introduction_times");
		} 
        else 
        {
			LogInformation("LogicEnvironment::LoadMeta(): Non-supported"
                " introduction time version " + std::to_string(ver));
		}
	} 
    catch (SettingNotFoundException&) 
    {
		// No problem, this is expected. Just continue with an empty string
	}
	mLBMMgr.LoadIntroductionTimes(lbmIntroductionTimes, this, mGameTime);

	mDayCount = args.Exists("day_count") ? args.GetUInt("day_count") : 0;
}

/**
 * called if env_meta.txt doesn't exist (e.g. new world)
 */
void LogicEnvironment::LoadDefaultMeta()
{
	mLBMMgr.LoadIntroductionTimes("", this, mGameTime);
}


int LogicEnvironment::NextSoundId()
{
	int ret = mNextSoundId;
	if (mNextSoundId == 0x7FFFFFFF)
		mNextSoundId = 0; // signed overflow is undefined
	else
		mNextSoundId++;
	return ret;
}

int LogicEnvironment::PlaySound(const SimpleSound& sound, const SoundParams& params, bool ephemeral)
{
	// Find out initial position of sound
	bool posExists = false;
	Vector3<float> pos = params.GetPosition(this, &posExists);
	// If position is not found while it should be, cancel sound
	if (posExists != (params.type != SoundParams::SP_LOCAL))
		return -1;

	// Filter destination clients
	std::vector<uint16_t> dstActors;
	if (!params.toPlayer.empty())
	{
		std::shared_ptr<LogicPlayer> player = GetPlayer(params.toPlayer.c_str());
		if (!player)
		{
			LogInformation("Player \"" + params.toPlayer + "\" not found");
			return -1;
		}
		if (player->GetId() == INVALID_ACTOR_ID)
		{
			LogInformation("Player \"" + params.toPlayer + "\" not connected");
			return -1;
		}
		dstActors.push_back(player->GetId());
	}
	else
	{
		std::vector<uint16_t> actors;
		for (const uint16_t actorId : actors)
		{
			std::shared_ptr<LogicPlayer> player = GetPlayer(actorId);
			if (!player)
				continue;

			if (!params.excludePlayer.empty() && params.excludePlayer == player->GetName())
				continue;

			PlayerLAO* playerLAO = player->GetPlayerLAO();
			if (!playerLAO)
				continue;

			if (posExists)
			{
				if (Length(playerLAO->GetBasePosition() - pos) > params.maxHearDistance)
					continue;
			}
			dstActors.push_back(actorId);
		}
	}

	if (dstActors.empty())
		return -1;

	// Create the sound
	int id;
	SoundPlaying* pSound = nullptr;
	if (!ephemeral)
	{
		id = NextSoundId();
		// The sound will exist as a reference in mPlayingSounds
		mPlayingSounds[id] = SoundPlaying();
		pSound = &mPlayingSounds[id];
		pSound->params = params;
		pSound->sound = sound;
	}
	else
	{
		id = -1; // old visuals will still use this, so pick a reserved ID
	}

	float gain = params.gain * sound.gain;
	bool asReliable = !ephemeral;

	for (const uint16_t dstActor : dstActors)
		if (pSound)
			pSound->actors.insert(dstActor);

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlaySoundType>(id, sound.name, 
		params.type, pos, params.object, gain, params.fade, params.pitch, ephemeral, params.loop));
	return id;
}

void LogicEnvironment::StopSound(int handle)
{
	// Get sound reference
	std::unordered_map<int, SoundPlaying>::iterator itSound = mPlayingSounds.find(handle);
	if (itSound == mPlayingSounds.end())
		return;

	//Remove sound reference
	mPlayingSounds.erase(itSound);

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataStopSound>(handle));
}

void LogicEnvironment::RemoveSounds(const std::vector<int>& soundList)
{
	for (int soundId : soundList)
		mPlayingSounds.erase(soundId);
}

void LogicEnvironment::FadeSound(int handle, float step, float gain)
{
	// Get sound reference
	std::unordered_map<int, SoundPlaying>::iterator itSound = mPlayingSounds.find(handle);
	if (itSound == mPlayingSounds.end())
		return;

	SoundPlaying& pSound = itSound->second;
	pSound.params.gain = gain;

	// Backwards compability
	bool playSound = gain > 0;
	SoundPlaying compatPlaySound = pSound;
	compatPlaySound.actors.clear();

	for (std::unordered_set<ActorId>::iterator it = pSound.actors.begin(); it != pSound.actors.end();)
	{
		uint16_t protoVersion = 39;
		if (protoVersion >= 32)
		{
			// Send as reliable
			EventManager::Get()->TriggerEvent(std::make_shared<EventDataStopSound>(*it));

			++it;
		}
		else
		{
			compatPlaySound.actors.insert(*it);

			pSound.actors.erase(it++);

			// Stop old sound
			EventManager::Get()->TriggerEvent(std::make_shared<EventDataStopSound>(*it));
		}
	}

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataFadeSound>(handle, step, gain));

	// Remove sound reference
	if (!playSound || pSound.actors.empty())
		mPlayingSounds.erase(itSound);

	if (playSound && !compatPlaySound.actors.empty())
	{
		// Play new sound volume on older clients
		PlaySound(compatPlaySound.sound, compatPlaySound.params);
	}
}

struct ActiveABM
{
	ActiveBlockModifier* abm;
	int chance;
	std::vector<uint16_t> requiredNeighbors;
	bool checkRequiredNeighbors; // false if requiredNeighbors is known to be empty
};

class ABMHandler
{
public:
	ABMHandler(std::vector<ABMWithState>& abms, 
        float dTime, LogicEnvironment* env, bool useTimers) : mEnvironment(env)
	{
		if(dTime < 0.001)
			return;

		const NodeManager* nodeMgr = mEnvironment->GetNodeManager();
		for (ABMWithState& abmws : abms) 
        {
			ActiveBlockModifier* abm = abmws.activeBlockModifier;
			float triggerInterval = abm->GetTriggerInterval();
			if(triggerInterval < 0.001)
				triggerInterval = 0.001f;
			float actualInterval = dTime;
			if(useTimers)
            {
				abmws.timer += dTime;
				if(abmws.timer < triggerInterval)
					continue;
				abmws.timer -= triggerInterval;
				actualInterval = triggerInterval;
			}
			unsigned int chance = abm->GetTriggerChance();
			if(chance == 0)
				chance = 1;
			ActiveABM aabm;
			aabm.abm = abm;
			if (abm->GetSimpleCatchUp()) 
            {
				float intervals = actualInterval / triggerInterval;
				if(intervals == 0)
					continue;
				aabm.chance = (int)(chance / intervals);
				if(aabm.chance == 0)
					aabm.chance = 1;
			} 
            else aabm.chance = chance;

			// Trigger neighbors
			const std::vector<std::string>& requiredNeighbors = abm->GetRequiredNeighbors();
			for (const std::string& requiredNeighbor : requiredNeighbors)
				nodeMgr->GetIds(requiredNeighbor, aabm.requiredNeighbors);

			aabm.checkRequiredNeighbors = !requiredNeighbors.empty();

			// Trigger contents
			const std::vector<std::string> &contents = abm->GetTriggerContents();
			for (const std::string& content : contents) 
            {
                std::vector<uint16_t> ids;
				nodeMgr->GetIds(content, ids);
				for (uint16_t c : ids) 
                {
					if (c >= mAABMs.size())
						mAABMs.resize(c + 256, NULL);
					if (!mAABMs[c])
						mAABMs[c] = new std::vector<ActiveABM>;
					mAABMs[c]->push_back(aabm);
				}
			}
		}
	}

	~ABMHandler()
	{
		for (auto& aabms : mAABMs)
			delete aabms;
	}

	// Find out how many objects the given block and its neighbours contain.
	// Returns the number of objects in the block, and also in 'wider' the
	// number of objects in the block and all its neighbours. The latter
	// may an estimate if any neighbours are unloaded.
	unsigned int CountObjects(MapBlock* block, std::shared_ptr<LogicMap> map, unsigned int& wider)
	{
		wider = 0;
		unsigned int widerUnknownCount = 0;
        for (short x = -1; x <= 1; x++)
        {
            for (short y = -1; y <= 1; y++)
            {
                for (short z = -1; z <= 1; z++)
                {
                    MapBlock* block2 = map->GetBlockNoCreateNoEx(
                        block->GetPosition() + Vector3<short>{x, y, z});
                    if (block2 == NULL) 
                    {
                        widerUnknownCount++;
                        continue;
                    }
                    wider += (unsigned int)block2->mStaticObjects.mActive.size() +
						(unsigned int)block2->mStaticObjects.mStored.size();
                }
            }
        }
		// Extrapolate
		unsigned int activeObjectCount = (unsigned int)block->mStaticObjects.mActive.size();
		unsigned int widerKnownCount = 3*3*3 - widerUnknownCount;
		wider += widerUnknownCount * wider / widerKnownCount;
		return activeObjectCount;

	}

    void Apply(MapBlock* block, int& blocksScanned, int& abmsRun, int& blocksCached)
    {
        if (mAABMs.empty() || block->IsDummy())
            return;

        // Check the content type cache first
        // to see whether there are any ABMs
        // to be run at all for this block.
        if (block->mContentsCached)
        {
            blocksCached++;
            bool runABMs = false;
            for (uint16_t content : block->mContents)
            {
                if (content < mAABMs.size() && mAABMs[content])
                {
                    runABMs = true;
                    break;
                }
            }
            if (!runABMs)
                return;
        }
        else
        {
            // Clear any caching
            block->mContents.clear();
        }
        blocksScanned++;

        std::shared_ptr<LogicMap> map = mEnvironment->GetLogicMap();

        unsigned int activeObjectCountWider;
        unsigned int activeObjectCount = this->CountObjects(block, map, activeObjectCountWider);
        mEnvironment->mAddedObjects = 0;

        Vector3<short> p0;
        PcgRandom pcgRand;
        for (p0[0] = 0; p0[0] < MAP_BLOCKSIZE; p0[0]++)
        {
            for (p0[1] = 0; p0[1] < MAP_BLOCKSIZE; p0[1]++)
            {
                for (p0[2] = 0; p0[2] < MAP_BLOCKSIZE; p0[2]++)
                {
                    const MapNode& node = block->GetNodeUnsafe(p0);
                    uint16_t c = node.GetContent();
                    // Cache content types as we go
                    if (!block->mContentsCached && !block->mNoCachedContents) 
                    {
                        block->mContents.insert(c);
                        if (block->mContents.size() > 64) 
                        {
                            // Too many different nodes... don't try to cache
                            block->mNoCachedContents = true;
                            block->mContents.clear();
                        }
                    }

                    if (c >= mAABMs.size() || !mAABMs[c])
                        continue;

                    Vector3<short> p = p0 + block->GetRelativePosition();
                    for (ActiveABM& aabm : *mAABMs[c])
                    {
                        if (pcgRand.Next() % aabm.chance != 0)
                            continue;

                        // Check neighbors
                        if (aabm.checkRequiredNeighbors) 
                        {
                            Vector3<short> p1;
                            for (p1[0] = p0[0] - 1; p1[0] <= p0[0] + 1; p1[0]++)
                            {
                                for (p1[1] = p0[1] - 1; p1[1] <= p0[1] + 1; p1[1]++)
                                {
                                    for (p1[2] = p0[2] - 1; p1[2] <= p0[2] + 1; p1[2]++)
                                    {
                                        if (p1 == p0)
                                            continue;
                                        uint16_t content;
                                        if (block->IsValidPosition(p1)) 
                                        {
                                            // if the neighbor is found on the same map block
                                            // get it straight from there
                                            const MapNode& node = block->GetNodeUnsafe(p1);
                                            content = node.GetContent();
                                        }
                                        else 
                                        {
                                            // otherwise consult the map
                                            MapNode node = map->GetNode(p1 + block->GetRelativePosition());
                                            content = node.GetContent();
                                        }
                                        if (std::find(aabm.requiredNeighbors.begin(),
                                            aabm.requiredNeighbors.end(), c) != aabm.requiredNeighbors.end())
                                        {
                                            goto neighborFound;
                                        }
                                    }
                                }
                            }
                            // No required neighbor found
                            continue;
                        }

						neighborFound:

                        abmsRun++;
                        // Call all the trigger variations
                        aabm.abm->Trigger(mEnvironment, p, node);
                        aabm.abm->Trigger(mEnvironment, p, node,
                            activeObjectCount, activeObjectCountWider);

                        // Count surrounding objects again if the abms added any
                        if (mEnvironment->mAddedObjects > 0) 
                        {
                            activeObjectCount = CountObjects(block, map, activeObjectCountWider);
                            mEnvironment->mAddedObjects = 0;
                        }
                    }
                }
            }
        }
		block->mContentsCached = !block->mNoCachedContents;
    }

private:
    LogicEnvironment* mEnvironment;
    std::vector<std::vector<ActiveABM>*> mAABMs;
};

void LogicEnvironment::ActivateBlock(MapBlock* block, unsigned int additionalDTime)
{
	// Reset usage timer immediately, otherwise a block that becomes active
	// again at around the same time as it would normally be unloaded will
	// get unloaded incorrectly. (I think this still leaves a small possibility
	// of a race condition between this and logic AsyncRunStep, which only
	// some kind of synchronisation will fix, but it at least reduces the window
	// of opportunity for it to break from seconds to nanoseconds)
	block->ResetUsageTimer();

	// Get time difference
	unsigned int dTime = 0;
	unsigned int stamp = block->GetTimestamp();
	if (mGameTime > stamp && stamp != BLOCK_TIMESTAMP_UNDEFINED)
		dTime = mGameTime - stamp;
	dTime += additionalDTime;

	/*infostream<<"LogicEnvironment::ActivateBlock(): block timestamp: "
			<<stamp<<", game time: "<<mGameTime<<std::endl;*/

	// Remove stored static objects if ClearObjects was called since block's timestamp
	if (stamp == BLOCK_TIMESTAMP_UNDEFINED || stamp < mLastClearObjectsTime) 
    {
		block->mStaticObjects.mStored.clear();
		// do not set changed flag to avoid unnecessary mapblock writes
	}

	// Set current time as timestamp
	block->SetTimestampNoChangedFlag(mGameTime);

	/*infostream<<"LogicEnvironment::ActivateBlock(): block is "
			<<dTime<<" seconds old."<<std::endl;*/

	// Activate stored objects
	ActivateObjects(block, dTime);

	/* Handle LoadingBlockModifiers */
	mLBMMgr.ApplyLBMs(this, block, stamp);

	// Run node timers
	std::vector<NodeTimer> elapsedTimers = block->mNodeTimers.Step((float)dTime);
	if (!elapsedTimers.empty()) 
    {
		MapNode node;
		for (const NodeTimer& elapsedTimer : elapsedTimers) 
        {
			node = block->GetNodeNoEx(elapsedTimer.mPosition);
			Vector3<short> pos = elapsedTimer.mPosition + block->GetRelativePosition();
			if (BaseGame::Get()->OnTimerNode(pos, node, elapsedTimer.mElapsed))
				block->SetNodeTimer(NodeTimer(elapsedTimer.mTimeout, 0, elapsedTimer.mPosition));
		}
	}
}

void LogicEnvironment::AddActiveBlockModifier(ActiveBlockModifier *abm)
{
	mABMs.emplace_back(abm);
}

void LogicEnvironment::AddLoadingBlockModifier(LoadingBlockModifier *lbm)
{
	mLBMMgr.AddLBM(lbm);
}

bool LogicEnvironment::SetNode(Vector3<short> pos, const MapNode& node)
{
	const NodeManager* nodeMgr = GetNodeManager();
	MapNode nodeOld = mMap->GetNode(pos);

	const ContentFeatures& contentFeaturesOld = nodeMgr->Get(nodeOld);

	// Call destructor
	if (contentFeaturesOld.hasOnDestruct)
        BaseGame::Get()->OnDestructNode(pos, nodeOld);

	// Replace node
	if (!mMap->AddNodeWithEvent(pos, node))
		return false;

	// Update active VoxelManipulator if a mapgen thread
	mMap->UpdateVManip(pos);

	// Call post-destructor
	if (contentFeaturesOld.hasAfterDestruct)
        BaseGame::Get()->AfterDestructNode(pos, nodeOld);

	// Retrieve node content features
	// if new node is same as old, reuse old definition to prevent a lookup
	const ContentFeatures& contentFeaturesNew = 
        nodeOld == node ? contentFeaturesOld : nodeMgr->Get(node);

	// Call constructor
	if (contentFeaturesNew.hasOnConstruct)
        BaseGame::Get()->OnConstructNode(pos, node);

	return true;
}

bool LogicEnvironment::RemoveNode(Vector3<short> pos)
{
	const NodeManager* nodeMgr = GetNodeManager();
	MapNode nodeOld = mMap->GetNode(pos);

	// Call destructor
	if (nodeMgr->Get(nodeOld).hasOnDestruct)
        BaseGame::Get()->OnDestructNode(pos, nodeOld);

	// Replace with air
	// This is slightly optimized compared to addNodeWithEvent(air)
	if (!mMap->RemoveNodeWithEvent(pos))
		return false;

	// Update active VoxelManipulator if a mapgen thread
	mMap->UpdateVManip(pos);

	// Call post-destructor
	if (nodeMgr->Get(nodeOld).hasAfterDestruct)
        BaseGame::Get()->AfterDestructNode(pos, nodeOld);

	// Air doesn't require constructor
	return true;
}

bool LogicEnvironment::SwapNode(Vector3<short> pos, const MapNode& node)
{
	if (!mMap->AddNodeWithEvent(pos, node, false))
		return false;

	// Update active VoxelManipulator if a mapgen thread
	mMap->UpdateVManip(pos);

	return true;
}

uint8_t LogicEnvironment::FindSunlight(Vector3<short> pos)
{
	// Directions for neighbouring nodes with specified order
	static const Vector3<short> dirs[] = {
        Vector3<short>{-1, 0, 0}, Vector3<short>{1, 0, 0}, Vector3<short>{0, 0, -1},
        Vector3<short>{0, 0, 1}, Vector3<short>{0, -1, 0}, Vector3<short>{0, 1, 0}};

    const NodeManager* nodeMgr = GetNodeManager();

	// foundLight remembers the highest known sunlight value at pos
	uint8_t foundLight = 0;

	struct StackEntry 
    {
		Vector3<short> pos;
		short dist;
	};
	std::stack<StackEntry> stack;
	stack.push({pos, 0});

	std::unordered_map<int64_t, int8_t> dists;
	dists[MapDatabase::GetBlockAsInteger(pos)] = 0;

	while (!stack.empty()) 
    {
		struct StackEntry e = stack.top();
		stack.pop();

		Vector3<short> currentPos = e.pos;
		int8_t dist = e.dist + 1;

		for (const Vector3<short>& off : dirs) 
        {
			Vector3<short> neighborPos = currentPos + off;
			int64_t neighborHash = MapDatabase::GetBlockAsInteger(neighborPos);

			// Do not walk neighborPos multiple times unless the distance to the start
			// position is shorter
			auto it = dists.find(neighborHash);
			if (it != dists.end() && dist >= it->second)
				continue;

			// Position to walk
			bool isPositionOk;
			MapNode node = mMap->GetNode(neighborPos, &isPositionOk);
			if (!isPositionOk) 
            {
				// This happens very rarely because the map at currentPos is loaded
				mMap->EmergeBlock(neighborPos, false);
				node = mMap->GetNode(neighborPos, &isPositionOk);
				if (!isPositionOk)
					continue;  // not generated
			}

			const ContentFeatures& cFeatures = nodeMgr->Get(node);
			if (!cFeatures.sunlightPropagates) 
            {
				// Do not test propagation here again
				dists[neighborHash] = -1;
				continue;
			}

			// Sunlight could have come from here
			dists[neighborHash] = dist;
			uint8_t daylight = node.param1 & 0x0f;

			// In the special case where sunlight shines from above and thus
			// does not decrease with upwards distance, daylight is always
			// bigger than nightlight, which never reaches 15
			int possibleFindlight = daylight - dist;
			if (possibleFindlight <= foundLight) 
            {
				// Light from here cannot make a brighter light at currentPos than
				// foundLight
				continue;
			}

			uint8_t nightlight = node.param1 >> 4;
			if (daylight > nightlight) 
            {
				// Found a valid daylight
				foundLight = possibleFindlight;
			} 
            else 
            {
				// Sunlight may be darker, so walk the neighbours
				stack.push({neighborPos, dist});
			}
		}
	}
	return foundLight;
}

void LogicEnvironment::ClearObjects(ClearObjectsMode mode)
{
	LogInformation("LogicEnvironment::ClearObjects(): Removing all active objects");
	auto CBRemoval = [this] (LogicActiveObject* obj, uint16_t id) 
    {
		if (obj->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
			return false;

		// Delete static object if block is loaded
		DeleteStaticFromBlock(obj, id, MOD_REASON_CLEAR_ALL_OBJECTS, true);

		// If known by some visual, don't delete immediately
		if (obj->mKnownByCount > 0) 
        {
			obj->MarkForRemoval();
			return false;
		}

		// Tell the object about removal
		obj->RemovingFromEnvironment();
		// Deregister in scripting api
        BaseGame::Get()->RemoveObjectReference(obj);

		// Delete active object
		if (obj->EnvironmentDeletes())
			delete obj;

		return true;
	};

    mActiveObjectMgr.Clear(CBRemoval);

	// Get list of loaded blocks
	std::vector<Vector3<short>> loadedBlocks;
	LogInformation("LogicEnvironment::ClearObjects(): Listing all loaded blocks");
	mMap->ListAllLoadedBlocks(loadedBlocks);
	LogInformation("LogicEnvironment::ClearObjects(): Done listing all loaded blocks: " +
		std::to_string(loadedBlocks.size()));

	// Get list of loadable blocks
	std::vector<Vector3<short>> loadableBlocks;
	if (mode == CLEAR_OBJECTS_MODE_FULL) 
    {
		LogInformation("LogicEnvironment::ClearObjects(): Listing all loadable blocks");
		mMap->ListAllLoadableBlocks(loadableBlocks);
		LogInformation("LogicEnvironment::ClearObjects(): Done listing all loadable blocks: " +
            std::to_string(loadableBlocks.size()));
	} 
    else 
    {
		loadableBlocks = loadedBlocks;
	}

	LogInformation("LogicEnvironment::ClearObjects(): Now clearing objects in " +
        std::to_string(loadableBlocks.size()) + " blocks");

	// Grab a reference on each loaded block to avoid unloading it
	for (Vector3<short> p : loadedBlocks) 
    {
		MapBlock* block = mMap->GetBlockNoCreateNoEx(p);
		LogAssert(block != NULL, "invalid block");
		block->RefGrab();
	}

	// Remove objects in all loadable blocks
	unsigned int unloadInterval = 0xFFFFFFFF;
	if (mode == CLEAR_OBJECTS_MODE_FULL) 
    {
		unloadInterval = Settings::Get()->GetInt("max_clearobjects_extra_loadedBlocks");
		unloadInterval = std::max(unloadInterval, (unsigned int)1);
	}
	unsigned int reportInterval = (unsigned int)loadableBlocks.size() / 10;
	unsigned int numBlocksChecked = 0;
	unsigned int numBlocksCleared = 0;
	unsigned int numObjsCleared = 0;
	for (auto i = loadableBlocks.begin(); i != loadableBlocks.end(); ++i) 
    {
		Vector3<short> p = *i;
		MapBlock* block = mMap->EmergeBlock(p, false);
		if (!block) 
        {
            LogWarning("LogicEnvironment::ClearObjects(): "
                "Failed to emerge block (" + std::to_string(p[0]) + "," +
                std::to_string(p[1]) + "," + std::to_string(p[2]) + ")");
			continue;
		}
		unsigned int numStored = (unsigned int)block->mStaticObjects.mStored.size();
		unsigned int numActive = (unsigned int)block->mStaticObjects.mActive.size();
		if (numStored != 0 || numActive != 0) 
        {
			block->mStaticObjects.mStored.clear();
			block->mStaticObjects.mActive.clear();
			block->RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_CLEAR_ALL_OBJECTS);
			numObjsCleared += numStored + numActive;
			numBlocksCleared++;
		}
		numBlocksChecked++;

		if (reportInterval != 0 && numBlocksChecked % reportInterval == 0) 
        {
			float percent = 100.f * (float)numBlocksChecked / loadableBlocks.size();
			LogInformation("LogicEnvironment::ClearObjects(): Cleared " + 
                std::to_string(numObjsCleared) + " objects in " + 
                std::to_string(numBlocksCleared) + " blocks (" + 
                std::to_string(percent) + "%)");
		}
		if (numBlocksChecked % unloadInterval == 0)
			mMap->UnloadUnreferencedBlocks();
	}
	mMap->UnloadUnreferencedBlocks();

	// Drop references that were added above
	for (Vector3<short> p : loadedBlocks) 
    {
		MapBlock* block = mMap->GetBlockNoCreateNoEx(p);
		LogAssert(block, "invalid block");
		block->RefDrop();
	}

	mLastClearObjectsTime = mGameTime;

	LogInformation("LogicEnvironment::ClearObjects(): Finished: Cleared " +
        std::to_string(numObjsCleared) + " objects in " +
        std::to_string(numBlocksCleared) + " blocks");
}

void LogicEnvironment::Step(float dTime)
{
	ScopeProfiler sp2(Profiling, "LogicEnv::step()", SPT_AVG);
	/* Step time of day */
	StepTimeOfDay(dTime);

	// Update this one
	// NOTE: This is kind of funny on a singleplayer game, but doesn't
	// really matter that much.
	static thread_local const float step =
        Settings::Get()->GetFloat("dedicated_server_step");
	mRecommendedSendInterval = step;

	/*
		Increment game time
	*/
	{
		mGameTimeFractionCounter += dTime;
		unsigned int incTime = (unsigned int)mGameTimeFractionCounter;
		mGameTime += incTime;
		mGameTimeFractionCounter -= (float)incTime;
	}

	/*
		Handle players
	*/
	{
		ScopeProfiler sp(Profiling, "LogicEnv: move players", SPT_AVG);
		for (std::shared_ptr<LogicPlayer> player : mPlayers) 
        {
			// Ignore disconnected players
			if (player->GetId() == INVALID_ACTOR_ID)
				continue;

			// Move
			player->Move(dTime, this, 100 * BS);
		}
	}

	/*
		Manage active block list
	*/
    
	if (mActiveBlocksManagementInterval.Step(dTime, mCacheActiveBlockMgmtInterval))
    {
		ScopeProfiler sp(Profiling, "LogicEnv: update active blocks", SPT_AVG);
		/*
			Get player block positions
		*/
		std::vector<PlayerLAO*> players;
		for (std::shared_ptr<LogicPlayer> player: mPlayers) 
        {
			// Ignore disconnected players
			if (player->GetId() == INVALID_ACTOR_ID)
				continue;

			PlayerLAO* playerLAO = player->GetPlayerLAO();
			LogAssert(playerLAO, "invalid player");

			players.push_back(playerLAO);
		}

		/*
			Update list of active blocks, collecting changes
		*/
		// use active_object_send_range_blocks since that is max distance
		// for active objects sent the visual anyway
		static thread_local const short activeObjectRange =
            Settings::Get()->GetInt16("active_object_send_range_blocks");
		static thread_local const short activeBlockRange =
            Settings::Get()->GetInt16("active_block_range");
		std::set<Vector3<short>> blocksRemoved;
        std::set<Vector3<short>> blocksAdded;
		mActiveBlocks.Update(players, activeBlockRange, activeObjectRange, blocksRemoved, blocksAdded);

		/*
			Handle removed blocks
		*/

		// Convert active objects that are no more in active blocks to static
		DeactivateFarObjects(false);

		for (const Vector3<short>& p: blocksRemoved) 
        {
			MapBlock* block = mMap->GetBlockNoCreateNoEx(p);
			if (!block)
				continue;

			// Set current time as timestamp (and let it set ChangedFlag)
			block->SetTimestamp(mGameTime);
		}

		/*
			Handle added blocks
		*/

		for (const Vector3<short>& p: blocksAdded) 
        {
			MapBlock* block = mMap->GetBlockOrEmerge(p);
			if (!block) 
            {
				mActiveBlocks.mList.erase(p);
				mActiveBlocks.mABMList.erase(p);
				continue;
			}

			ActivateBlock(block);
		}
	}

	/*
		Mess around in active blocks
	*/
	if (mActiveBlocksNodeMetadataInterval.Step(dTime, mCacheNodetimerInterval))
    {
		ScopeProfiler sp(Profiling, "LogicEnv: Run node timers", SPT_AVG);

		float dTime = mCacheNodetimerInterval;
		for (const Vector3<short> &p: mActiveBlocks.mList) 
        {
			MapBlock* block = mMap->GetBlockNoCreateNoEx(p);
			if (!block)
				continue;

			// Reset block usage timer
			block->ResetUsageTimer();

			// Set current time as timestamp
			block->SetTimestampNoChangedFlag(mGameTime);
			// If time has changed much from the one on disk,
			// set block to be saved when it is unloaded
			if(block->GetTimestamp() > block->GetDiskTimestamp() + 60)
				block->RaiseModified(MOD_STATE_WRITE_AT_UNLOAD, MOD_REASON_BLOCK_EXPIRED);

			// Run node timers
			std::vector<NodeTimer> elapsedTimers = block->mNodeTimers.Step(dTime);
			if (!elapsedTimers.empty())
            {
				MapNode node;
				Vector3<short> pos2;
				for (const NodeTimer& elapsedTimer: elapsedTimers) 
                {
					node = block->GetNodeNoEx(elapsedTimer.mPosition);
					pos2 = elapsedTimer.mPosition + block->GetRelativePosition();
					if (BaseGame::Get()->OnTimerNode(pos2, node, elapsedTimer.mElapsed))
                    {
						block->SetNodeTimer(NodeTimer(
							elapsedTimer.mTimeout, 0, elapsedTimer.mPosition));
					}
				}
			}
		}
	}

	if (mActiveBlockModifierInterval.Step(dTime, mCacheAbmInterval))
    {
		ScopeProfiler sp(Profiling, "SEnv: modify in blocks avg per interval", SPT_AVG);
		TimeTaker timer("modify in active blocks per interval");

		// Initialize handling of ActiveBlockModifiers
		ABMHandler abmhandler(mABMs, mCacheAbmInterval, this, true);

        int abmsRun = 0;
		int blocksScanned = 0;
		int blocksCached = 0;

		std::random_device rd;
		std::mt19937 gen(rd());

		std::vector<Vector3<short>> output(mActiveBlocks.mABMList.size());

		// Shuffle the active blocks so that each block gets an equal chance
		// of having its ABMs run.
		std::copy(mActiveBlocks.mABMList.begin(), mActiveBlocks.mABMList.end(), output.begin());
		std::shuffle(output.begin(), output.end(), gen);

		int i = 0;
		// determine the time budget for ABMs
		unsigned int maxTimeMs = (unsigned int)(mCacheAbmInterval * 1000 * mCacheAbmTimeBudget);
		for (const Vector3<short>& bl : output) 
        {
			MapBlock* block = mMap->GetBlockNoCreateNoEx(bl);
			if (!block)
				continue;

			i++;

			// Set current time as timestamp
			block->SetTimestampNoChangedFlag(mGameTime);

			/* Handle ActiveBlockModifiers */
			abmhandler.Apply(block, blocksScanned, abmsRun, blocksCached);

			uint64_t timeMs = timer.GetTimeElapsed();

			if (timeMs > maxTimeMs) 
            {
				LogWarning("active block modifiers took " + std::to_string(timeMs) + 
                    "ms (processed " + std::to_string(i) + " of " + 
                    std::to_string(output.size()) + " active blocks)");
				break;
			}
		}
		Profiling->Avg("LogicEnv: active blocks", (float)mActiveBlocks.mABMList.size());
        Profiling->Avg("LogicEnv: active blocks cached", (float)blocksCached);
        Profiling->Avg("LogicEnv: active blocks scanned for ABMs", (float)blocksScanned);
        Profiling->Avg("LogicEnv: ABMs run", (float)abmsRun);

		timer.Stop(true);
	}

	/*
		Step environment (run global on_step())
	*/
    BaseGame::Get()->Step(dTime);

	/*
		Step active objects
	*/
	{
		ScopeProfiler sp(Profiling, "LogicEnv: Run LAO::step()", SPT_AVG);

		// This helps the objects to send data at the same time
		bool sendRecommended = false;
		mSendRecommendedTimer += dTime;
		if (mSendRecommendedTimer > GetSendRecommendedInterval()) 
        {
			mSendRecommendedTimer -= GetSendRecommendedInterval();
			sendRecommended = true;
		}

		auto cbState = [this, dTime, sendRecommended] (LogicActiveObject *obj)
        {
			if (obj->IsGone())
				return;

			// Step object
			obj->Step(dTime, sendRecommended);
			// Read messages from object
			obj->DumpAOMessagesToQueue(mActiveObjectMessages);
		};
		mActiveObjectMgr.Step(dTime, cbState);
	}

	/*
		Manage active objects
	*/
	if (mObjectManagementInterval.Step(dTime, 0.5))
		RemoveRemovedObjects();

	/*
		Manage particle spawner expiration
	*/
	if (mParticleManagementInterval.Step(dTime, 1.0))
    {
        std::unordered_map<unsigned int, float>::iterator i;
		for (i = mParticleSpawners.begin(); i != mParticleSpawners.end(); ) 
        {
			//non expiring spawners
			if (i->second == PARTICLE_SPAWNER_NO_EXPIRY) {
				++i;
				continue;
			}

			i->second -= 1.0f;
			if (i->second <= 0.f)
				mParticleSpawners.erase(i++);
			else
				++i;
		}
	}

	// Send outdated player inventories
	for (std::shared_ptr<LogicPlayer> player : mPlayers) 
    {
		if (player->GetId() == INVALID_ACTOR_ID)
			continue;

		PlayerLAO* playerLAO = player->GetPlayerLAO();
		if (playerLAO && player->mInventory.CheckModified())
			SendInventory(playerLAO, true);
	}

	// Send outdated detached inventories
	SendDetachedInventories(INVALID_ACTOR_ID, true);

    // Write logic map
    if (mLocalDB && mLocalDBSaveInterval.Step(dTime, mCacheSaveInterval))
    {
        mLocalDB->EndSave();
        mLocalDB->BeginSave();
    }
}

unsigned int LogicEnvironment::AddParticleSpawner(float exptime)
{
	// Timers with lifetime 0 do not expire
	float time = exptime > 0.f ? exptime : PARTICLE_SPAWNER_NO_EXPIRY;

	unsigned int id = 0;
	for (;;) 
    { 
        // look for unused particlespawner id
		id++;
		std::unordered_map<unsigned int, float>::iterator f = mParticleSpawners.find(id);
		if (f == mParticleSpawners.end()) 
        {
			mParticleSpawners[id] = time;
			break;
		}
	}
	return id;
}

unsigned int LogicEnvironment::AddParticleSpawner(float exptime, uint16_t attachedId)
{
	unsigned int id = AddParticleSpawner(exptime);
	mParticleSpawnerAttachments[id] = attachedId;
	if (LogicActiveObject *obj = GetActiveObject(attachedId))
		obj->AttachParticleSpawner(id);

	return id;
}

void LogicEnvironment::DeleteParticleSpawner(unsigned int id, bool removeFromObject)
{
	mParticleSpawners.erase(id);
	const auto &it = mParticleSpawnerAttachments.find(id);
	if (it != mParticleSpawnerAttachments.end()) 
    {
		uint16_t objId = it->second;
		LogicActiveObject *playerLAO = GetActiveObject(objId);
		if (playerLAO != NULL && removeFromObject)
			playerLAO->DetachParticleSpawner(id);

		mParticleSpawnerAttachments.erase(id);
	}
}

uint16_t LogicEnvironment::AddActiveObject(LogicActiveObject* object)
{
	LogAssert(object, "invalid object");	// Pre-condition
	mAddedObjects++;
	uint16_t id = AddActiveObjectRaw(object, true, 0);
	return id;
}

/*
	Finds out what new objects have been added to
	inside a radius around a position
*/
void LogicEnvironment::GetAddedActiveObjects(PlayerLAO* playerLAO, short radius,
	short playerRadius, std::set<uint16_t>& currentObjects, std::queue<uint16_t>& addedObjects)
{
    float rad = radius * BS;
    float playerRad = playerRadius * BS < 0 ? 0 : playerRadius * BS;

	mActiveObjectMgr.GetAddedActiveObjectsAroundPosition(
        playerLAO->GetBasePosition(), rad, playerRad, currentObjects, addedObjects);
}

/*
	Finds out what objects have been removed from
	inside a radius around a position
*/
void LogicEnvironment::GetRemovedActiveObjects(PlayerLAO* playerLAO, short radius,
	short playerRadius, std::set<uint16_t>& currentObjects, std::queue<uint16_t>& removedObjects)
{
    float rad = radius * BS;
    float playerRad = playerRadius * BS < 0 ? 0 : playerRadius * BS;

	/*
		Go through currentObjects; object is removed if:
		- object is not found in mActiveObjects (this is actually an
		  error condition; objects should be removed only after all visuals
		  have been informed about removal), or
		- object is to be removed or deactivated, or
		- object is too far away
	*/
	for (uint16_t id : currentObjects) 
    {
		LogicActiveObject *object = GetActiveObject(id);

		if (object == NULL) 
        {
			LogInformation("LogicEnvironment::GetRemovedActiveObjects():"
                " object in currentObjects is NULL");
			removedObjects.push(id);
			continue;
		}

		if (object->IsGone()) 
        {
			removedObjects.push(id);
			continue;
		}

		float distance = Length(object->GetBasePosition() - playerLAO->GetBasePosition());
		if (object->GetType() == ACTIVEOBJECT_TYPE_PLAYER) 
        {
			if (distance <= playerRad || playerRad == 0)
				continue;
		} 
        else if (distance <= rad)
			continue;

		// Object is no longer visible
		removedObjects.push(id);
	}
}

// AddVelocity(self, velocity)
int LogicEnvironment::AddVelocity(UnitLAO* unitLAO, Vector3<float> vel)
{
    if (unitLAO->GetType() == ACTIVEOBJECT_TYPE_ENTITY)
    {
        EntityLAO* entityLAO = dynamic_cast<EntityLAO*>(unitLAO);
        entityLAO->AddVelocity(vel);
    }
    else if (unitLAO->GetType() == ACTIVEOBJECT_TYPE_PLAYER) 
    {
        PlayerLAO* playerLAO = dynamic_cast<PlayerLAO*>(unitLAO);
        playerLAO->SetMaxSpeedOverride(vel);

        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataPlayerSpeed>(playerLAO->GetId(), vel));
    }

    return 0;
}

void LogicEnvironment::SetStaticForActiveObjectsInBlock(
	Vector3<short> blockpos, bool staticExists, Vector3<short> staticBlock)
{
	MapBlock* block = mMap->GetBlockNoCreateNoEx(blockpos);
	if (!block)
		return;

	for (auto& soIt : block->mStaticObjects.mActive) 
    {
		// Get the LogicActiveObject counterpart to this StaticObject
		LogicActiveObject* playerLAO = mActiveObjectMgr.GetActiveObject(soIt.first);
		if (!playerLAO) 
        {
			// If this ever happens, there must be some kind of nasty bug.
			LogInformation("LogicEnvironment::SetStaticForObjectsInBlock(): "
				"Object from MapBlock::mStaticObjects::mActive not found "
				"in mActiveObjects");
			continue;
		}

		playerLAO->mStaticExists = staticExists;
		playerLAO->mStaticBlock = staticBlock;
	}
}

bool LogicEnvironment::GetActiveObjectMessage(ActiveObjectMessage* dest)
{
	if(mActiveObjectMessages.empty())
		return false;

	*dest = std::move(mActiveObjectMessages.front());
	mActiveObjectMessages.pop();
	return true;
}

void LogicEnvironment::GetSelectedActiveObjects(
	const Line3<float>& shootlineOnMap, std::vector<PointedThing>& objects)
{
	std::vector<LogicActiveObject*> objs;
	GetObjectsInsideRadius(objs, shootlineOnMap.mStart,
		shootlineOnMap.GetLength() + 10.0f, nullptr);
	const Vector3<float> lineVector = shootlineOnMap.GetVector();

	for (auto obj : objs) 
    {
		if (obj->IsGone())
			continue;
		BoundingBox<float> selectionBox;
		if (!obj->GetSelectionBox(&selectionBox))
			continue;

		Vector3<float> pos = obj->GetBasePosition();
		BoundingBox<float> offsettedBox(selectionBox.mMinEdge + pos, selectionBox.mMaxEdge + pos);

		Vector3<float> currentIntersection;
		Vector3<short> currentNormal;
		if (BoxLineCollision(offsettedBox, shootlineOnMap.mStart, lineVector, 
            &currentIntersection, &currentNormal)) 
        {
			objects.emplace_back(
				(short)obj->GetId(), currentIntersection, currentNormal,
				LengthSq(currentIntersection - shootlineOnMap.mStart));
		}
	}
}


Vector3<float> LogicEnvironment::FindSpawnPosition()
{
    std::shared_ptr<LogicMap> map = GetLogicMap();

    // Limit spawn range to mapgen edges (determined by 'mapgen_limit')
    int rangeMax = map->GetMapGeneratorParams()->GetSpawnRangeMax();

    // Try to find a good place a few times
    bool isGood = false;
    Vector3<float> nodePos = Vector3<float>::Zero();
    for (int i = 0; i < 4000 && !isGood; i++)
    {
        int range = std::min(1 + i, rangeMax);
        // We're going to try to throw the player to this position
        Vector2<short> nodePosition2d = Vector2<short>{
            (short)(-range + (rand() % (range * 2))),
            (short)(-range + (rand() % (range * 2))) };
        // Get spawn level at point
        short spawnLevel = EmergeManager::Get()->GetSpawnLevelAtPoint(nodePosition2d);
        // Continue if MAX_MAP_GENERATION_LIMIT was returned by the mapgen to
        // signify an unsuitable spawn position, or if outside limits.
        if (spawnLevel >= MAX_MAP_GENERATION_LIMIT ||
            spawnLevel <= -MAX_MAP_GENERATION_LIMIT)
            continue;

        Vector3<short> nodePosition{ nodePosition2d[0], spawnLevel, nodePosition2d[1] };
        // Consecutive empty nodes
        int airCount = 0;

        // Search upwards from 'spawn level' for 2 consecutive empty nodes, to
        // avoid obstructions in already-generated mapblocks.
        // In ungenerated mapblocks consisting of 'ignore' nodes, there will be
        // no obstructions, but mapgen decorations are generated after spawn so
        // the player may end up inside one.
        for (int i = 0; i < 8; i++)
        {
            Vector3<short> blockPosition = GetNodeBlockPosition(nodePosition);
            map->EmergeBlock(blockPosition, true);
            uint16_t content = map->GetNode(nodePosition).GetContent();

            // In generated mapblocks allow spawn in all 'airlike' drawtype nodes.
            // In ungenerated mapblocks allow spawn in 'ignore' nodes.
            if (GetNodeManager()->Get(content).drawType == NDT_AIRLIKE || content == CONTENT_IGNORE)
            {
                airCount++;
                if (airCount >= 2)
                {
                    // Spawn in lower empty node
                    nodePosition[1]--;
                    nodePos = Vector3<float>{
                        nodePosition[0] * BS, nodePosition[1] * BS, nodePosition[2] * BS };
                    // Don't spawn the player outside map boundaries
                    if (ObjectPositionOverLimit(nodePos))
                        // Exit this loop, positions above are probably over limit
                        break;

                    // Good position found, cause an exit from main loop
                    isGood = true;
                    break;
                }
            }
            else airCount = 0;
            nodePosition[1]++;
        }
    }

    if (isGood)
        return nodePos;

    // No suitable spawn point found, return fallback 0,0,0
    return Vector3<float>::Zero();
}



/*
	************ Private methods *************
*/

uint16_t LogicEnvironment::AddActiveObjectRaw(LogicActiveObject* object, bool setChanged, unsigned int dTime)
{
	if (!mActiveObjectMgr.RegisterObject(object))
		return 0;

	// Register reference in scripting api (must be done before post-init)
    BaseGame::Get()->AddObjectReference(object);
	// Post-initialize object
	object->AddedToEnvironment(dTime);

	// Add static data to block
	if (object->IsStaticAllowed()) 
    {
		// Add static object to active static list of the block
		Vector3<float> objectPos = object->GetBasePosition();
        Vector3<short> objPos;
        objPos[0] = (short)((objectPos[0] + (objectPos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        objPos[1] = (short)((objectPos[1] + (objectPos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        objPos[2] = (short)((objectPos[2] + (objectPos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		StaticObject sObj(object, objectPos);
		// Add to the block where the object is located in
		Vector3<short> blockpos = GetNodeBlockPosition(objPos);
		MapBlock* block = mMap->EmergeBlock(blockpos);
		if(block)
        {
			block->mStaticObjects.mActive[object->GetId()] = sObj;
			object->mStaticExists = true;
			object->mStaticBlock = blockpos;

			if(setChanged)
				block->RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_ADD_ACTIVE_OBJECT_RAW);
		} 
        else 
        {
            Vector3<short> objPos;
            objPos[0] = (short)((objectPos[0] + (objectPos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
            objPos[1] = (short)((objectPos[1] + (objectPos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
            objPos[2] = (short)((objectPos[2] + (objectPos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

            LogWarning("LogicEnvironment::AddActiveObjectRaw(): "
                "could not emerge block for storing id=" + std::to_string(object->GetId()) +
                "statically (pos=(" + std::to_string(objPos[0]) + "," + std::to_string(objPos[1]) +
                "," + std::to_string(objPos[2]) + ")");
		}
	}

	return object->GetId();
}

/*
	Remove objects that satisfy (isGone() && m_known_by_count==0)
*/
void LogicEnvironment::RemoveRemovedObjects()
{
	ScopeProfiler sp(Profiling, "LogicEnvironment::RemoveRemovedObjects()", SPT_AVG);

	auto ClearCB = [this] (LogicActiveObject *obj, uint16_t id) 
    {
		// This shouldn't happen but check it
		if (!obj) 
        {
			LogWarning("LogicEnvironment::RemoveRemovedObjects(): "
                "NULL object found. id=" + std::to_string(id));
			return true;
		}

		/*
			We will handle objects marked for removal or deactivation
		*/
		if (!obj->IsGone())
			return false;

		/*
			Delete static data from block if removed
		*/
		if (obj->IsPendingRemoval())
			DeleteStaticFromBlock(obj, id, MOD_REASON_REMOVE_OBJECTS_REMOVE, false);

		// If still known by visuals, don't actually remove. On some future
		// invocation this will be 0, which is when removal will continue.
		if(obj->mKnownByCount > 0)
			return false;

		/*
			Move static data from active to stored if deactivated
		*/
		if (!obj->IsPendingRemoval() && obj->mStaticExists) 
        {
			MapBlock* block = mMap->EmergeBlock(obj->mStaticBlock, false);
			if (block) 
            {
				const auto i = block->mStaticObjects.mActive.find(id);
				if (i != block->mStaticObjects.mActive.end()) 
                {
					block->mStaticObjects.mStored.push_back(i->second);
					block->mStaticObjects.mActive.erase(id);
					block->RaiseModified(MOD_STATE_WRITE_NEEDED,
						MOD_REASON_REMOVE_OBJECTS_DEACTIVATE);
				} 
                else 
                {
					LogWarning("LogicEnvironment::RemoveRemovedObjects(): " 
                        "id=" + std::to_string(id) + " mStaticExists=true but "
                        "static data doesn't actually exist in (" + 
                        std::to_string(obj->mStaticBlock[0]) + 
                        "," + std::to_string(obj->mStaticBlock[1]) +
                        "," + std::to_string(obj->mStaticBlock[2]) + ")");
				}
			} 
            else 
            {
				LogInformation("Failed to emerge block from which an object to "
                    "be deactivated was loaded from. id=" + std::to_string(id));
			}
		}

		// Tell the object about removal
		obj->RemovingFromEnvironment();
		// Deregister in scripting api
        BaseGame::Get()->RemoveObjectReference(obj);

		// Delete
		if (obj->EnvironmentDeletes())
			delete obj;

		return true;
	};

	mActiveObjectMgr.Clear(ClearCB);
}

static void PrintHexDump(std::ostream& o, const std::string& data)
{
	const int linelength = 16;
	for(int l=0; ; l++)
    {
		int i0 = linelength * l;
		bool atEnd = false;
		int thislinelength = linelength;
		if(i0 + thislinelength > (int)data.size())
        {
			thislinelength = (int)data.size() - i0;
			atEnd = true;
		}
		for(int di=0; di<linelength; di++)
        {
			int i = i0 + di;
			char buf[4];
			if(di<thislinelength)
                snprintf(buf, sizeof(buf), "%.2x ", data[i]);
			else
                snprintf(buf, sizeof(buf), "   ");
			o<<buf;
		}
		o<<" ";
		for(int di=0; di<thislinelength; di++)
        {
			int i = i0 + di;
			if(data[i] >= 32)
				o<<data[i];
			else
				o<<".";
		}
		o<<std::endl;
		if(atEnd)
			break;
	}
}

LogicActiveObject* LogicEnvironment::CreateLAO(ActiveObjectType type, 
    Vector3<float> pos, const std::string& data)
{
	switch (type) 
    {
		case ACTIVEOBJECT_TYPE_ENTITY:
			return new EntityLAO(this, pos, data);
		default:
            LogWarning("LogicActiveObject: No factory for type=" + std::to_string(type));
	}
	return nullptr;
}

/*
	Convert stored objects from blocks near the players to active.
*/
void LogicEnvironment::ActivateObjects(MapBlock* block, unsigned int dTime)
{
	if(block == NULL)
		return;

	// Ignore if no stored objects (to not set changed flag)
	if(block->mStaticObjects.mStored.empty())
		return;

	LogInformation("LogicEnvironment::ActivateObjects(): "
		"activating objects of block (" + 
        std::to_string(block->GetPosition()[0]) + "," + 
        std::to_string(block->GetPosition()[1]) + "," +
        std::to_string(block->GetPosition()[2]) + ") (" +
        std::to_string(block->mStaticObjects.mStored.size()) + " objects)");

    std::stringstream errorstream;
	bool largeAmount = 
        (block->mStaticObjects.mStored.size() > Settings::Get()->GetUInt16("max_objects_per_block"));
	if (largeAmount) 
    {
        errorstream << "suspiciously large amount of objects detected: "
            << std::to_string(block->mStaticObjects.mStored.size()) << " in ("
            << std::to_string(block->GetPosition()[0]) << ","
            << std::to_string(block->GetPosition()[1]) << ","
            << std::to_string(block->GetPosition()[2]) << "); removing all of them.";
		// Clear stored list
		block->mStaticObjects.mStored.clear();
		block->RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_TOO_MANY_OBJECTS);
		return;
	}

	// Activate stored objects
	std::vector<StaticObject> newStored;
	for (const StaticObject& sObj : block->mStaticObjects.mStored) 
    {
		// Create an active object from the data
		LogicActiveObject* obj = CreateLAO((ActiveObjectType) sObj.type, sObj.position, sObj.data);
        Vector3<float> pos = sObj.position / BS;

		// If couldn't create object, store static data back.
		if (!obj) 
        {
            errorstream << "LogicEnvironment::ActivateObjects(): "
                << "failed to create active object from static object "
                << "in block (" << std::to_string(pos[0]) << ","
                << std::to_string(pos[1]) << "," << std::to_string(pos[2])
                << ") type=" << std::to_string((int)sObj.type) << " data:";
			PrintHexDump(errorstream, sObj.data);

			newStored.push_back(sObj);
			continue;
		}
		LogInformation("LogicEnvironment::ActivateObjects(): "
			"activated static object pos= ( " + std::to_string(pos[0]) + 
            "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]) +
            ") type=" + std::to_string((int)sObj.type));
		// This will also add the object to the active static list
		AddActiveObjectRaw(obj, false, dTime);
	}

	// Clear stored list
	block->mStaticObjects.mStored.clear();
	// Add leftover failed stuff to stored list
	for (const StaticObject& sObj : newStored)
		block->mStaticObjects.mStored.push_back(sObj);

	/*
		Note: Block hasn't really been modified here.
		The objects have just been activated and moved from the stored
		static list to the active static list.
		As such, the block is essentially the same.
		Thus, do not call block->raiseModified(MOD_STATE_WRITE_NEEDED).
		Otherwise there would be a huge amount of unnecessary I/O.
	*/
}

/*
	Convert objects that are not standing inside active blocks to static.

	If m_known_by_count != 0, active object is not deleted, but static
	data is still updated.

	If forceDelete is set, active object is deleted nevertheless. It
	shall only be set so in the destructor of the environment.

	If block wasn't generated (not in memory or on disk),
*/
void LogicEnvironment::DeactivateFarObjects(bool forceDel)
{
	auto CBDeactivate = [this, forceDel] (LogicActiveObject* obj, uint16_t id) 
    {
		// forceDelete might be overriden per object
		bool forceDelete = forceDel;

		// Do not deactivate if disallowed
		if (!forceDelete && !obj->ShouldUnload())
			return false;

		// RemoveRemovedObjects() is responsible for these
		if (!forceDelete && obj->IsGone())
			return false;

		const Vector3<float>& objectPos = obj->GetBasePosition();

        Vector3<short> pos;
        pos[0] = (short)((objectPos[0] + (objectPos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos[1] = (short)((objectPos[1] + (objectPos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos[2] = (short)((objectPos[2] + (objectPos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		// The block in which the object resides in
		Vector3<short> blockPosOrigin = GetNodeBlockPosition(pos);

		// If object's static data is stored in a deactivated block and object
		// is actually located in an active block, re-save to the block in
		// which the object is actually located in.
		if (!forceDelete && obj->mStaticExists &&
		   !mActiveBlocks.Contains(obj->mStaticBlock) &&
		   mActiveBlocks.Contains(blockPosOrigin))
        {
			// Delete from block where object was located
			DeleteStaticFromBlock(obj, id, MOD_REASON_STATIC_DATA_REMOVED, false);

			StaticObject sObj(obj, objectPos);
			// Save to block where object is located
			SaveStaticToBlock(blockPosOrigin, id, obj, sObj, MOD_REASON_STATIC_DATA_ADDED);
			return false;
		}

		// If block is still active, don't remove
		bool stillActive = obj->IsStaticAllowed() ? mActiveBlocks.Contains(blockPosOrigin) :
			GetMap()->GetBlockNoCreateNoEx(blockPosOrigin) != nullptr;
		if (!forceDelete && stillActive)
			return false;

		LogInformation("LogicEnvironment::DeactivateFarObjects(): deactivating object id=" + 
            std::to_string(id) + " on inactive block (" + std::to_string(blockPosOrigin[0]) +
            "," + std::to_string(blockPosOrigin[1]) + "," + std::to_string(blockPosOrigin[2]) + ")");

		// If known by some visual, don't immediately delete.
		bool pendingDelete = (obj->mKnownByCount > 0 && !forceDelete);

		/*
			Update the static data
		*/
		if (obj->IsStaticAllowed()) 
        {
			// Create new static object
			StaticObject sObj(obj, objectPos);

			bool staysInSameBlock = false;
			bool dataChanged = true;

			// Check if static data has changed considerably
			if (obj->mStaticExists) 
            {
				if (obj->mStaticBlock == blockPosOrigin)
					staysInSameBlock = true;

				MapBlock* block = mMap->EmergeBlock(obj->mStaticBlock, false);
				if (block) 
                {
					const auto node = block->mStaticObjects.mActive.find(id);
					if (node != block->mStaticObjects.mActive.end())
                    {
						StaticObject staticOld = node->second;

						float saveMovem = obj->GetMinimumSavedMovement();
                        if (staticOld.data == sObj.data &&
							Length(staticOld.position - objectPos) < saveMovem)
                        {
                            dataChanged = false;
                        }
					} 
                    else 
                    {
						LogWarning("LogicEnvironment::DeactivateFarObjects(): id=" + 
                            std::to_string(id) + " mStaticExists=true but static " 
                            "data doesn't actually exist in (" +
                            std::to_string(obj->mStaticBlock[0]) + "," + 
                            std::to_string(obj->mStaticBlock[1]) + "," + 
                            std::to_string(obj->mStaticBlock[2]) + ")");
					}
				}
			}

			/*
				While changes are always saved, blocks are only marked as modified
				if the object has moved or different staticdata. (see above)
			*/
			bool shallBeWritten = (!staysInSameBlock || dataChanged);
			unsigned int reason = shallBeWritten ? MOD_REASON_STATIC_DATA_CHANGED : MOD_REASON_UNKNOWN;

			// Delete old static object
			DeleteStaticFromBlock(obj, id, reason, false);

			// Add to the block where the object is located in
            pos[0] = (short)((objectPos[0] + (objectPos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
            pos[1] = (short)((objectPos[1] + (objectPos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
            pos[2] = (short)((objectPos[2] + (objectPos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

			Vector3<short> blockpos = GetNodeBlockPosition(pos);
			uint16_t storeId = pendingDelete ? id : 0;
			if (!SaveStaticToBlock(blockpos, storeId, obj, sObj, reason))
				forceDelete = true;
		}

		// Regardless of what happens to the object at this point, deactivate it first.
		// This ensures that Entity OnDeactivate is always called.
		obj->MarkForDeactivation();

		/*
			If known by some visual, set pending deactivation.
			Otherwise delete it immediately.
		*/
		if (pendingDelete && !forceDelete) 
        {
			LogInformation("object id=" + std::to_string(id) + " is known by visuals; not deleting yet");

			return false;
		}

		LogInformation("object id=" +std::to_string(id) + " is not known by visuals; deleting");

		// Tell the object about removal
		obj->RemovingFromEnvironment();
		// Deregister in scripting api
        BaseGame::Get()->RemoveObjectReference(obj);

		// Delete active object
		if (obj->EnvironmentDeletes())
			delete obj;

		return true;
	};

	mActiveObjectMgr.Clear(CBDeactivate);
}

void LogicEnvironment::DeleteStaticFromBlock(
    LogicActiveObject* obj, uint16_t id, unsigned int modReason, bool noEmerge)
{
	if (!obj->mStaticExists)
		return;

	MapBlock* block;
	if (noEmerge)
		block = mMap->GetBlockNoCreateNoEx(obj->mStaticBlock);
	else
		block = mMap->EmergeBlock(obj->mStaticBlock, false);
	if (!block) 
    {
        if (!noEmerge)
        {
            LogWarning("LogicEnv: Failed to emerge block (" +
                std::to_string(obj->mStaticBlock[0]) + "," +
                std::to_string(obj->mStaticBlock[1]) + "," +
                std::to_string(obj->mStaticBlock[2]) + ") "
                "when deleting static data of object from it. id=" + std::to_string(id));
        }
		return;
	}

	block->mStaticObjects.Remove(id);
	if (modReason != MOD_REASON_UNKNOWN) // Do not mark as modified if requested
		block->RaiseModified(MOD_STATE_WRITE_NEEDED, modReason);

	obj->mStaticExists = false;
}

bool LogicEnvironment::SaveStaticToBlock(Vector3<short> blockpos, uint16_t storeId,
    LogicActiveObject* obj, const StaticObject& sObj, unsigned int modReason)
{
	MapBlock* block = nullptr;
	try
    {
		block = mMap->EmergeBlock(blockpos);
	}
    catch (InvalidPositionException& ) 
    {
		// Handled via NULL pointer
		// NOTE: emergeBlock's failure is usually determined by it
		//       actually returning NULL
	}

	if (!block) 
    {
		LogWarning("LogicEnv: Failed to emerge block (" +
            std::to_string(obj->mStaticBlock[0]) + "," +
            std::to_string(obj->mStaticBlock[1]) + "," +
            std::to_string(obj->mStaticBlock[2]) + ")"
            " when saving static data of object to it. id=" + std::to_string(storeId));
		return false;
	}
	if (block->mStaticObjects.mStored.size() >= Settings::Get()->GetUInt16("max_objects_per_block")) 
    {
		LogWarning("LogicEnv: Trying to store id = " + 
            std::to_string(storeId) + " statically but block (" + 
            std::to_string(blockpos[0]) + "," + std::to_string(blockpos[1]) + "," + 
            std::to_string(blockpos[2]) + ")" + " already contains " + 
            std::to_string(block->mStaticObjects.mStored.size()) + " objects.");
		return false;
	}

	block->mStaticObjects.Insert(storeId, sObj);
	if (modReason != MOD_REASON_UNKNOWN) // Do not mark as modified if requested
		block->RaiseModified(MOD_STATE_WRITE_NEEDED, modReason);

	obj->mStaticExists = true;
	obj->mStaticBlock = blockpos;

	return true;
}

PlayerDatabase* LogicEnvironment::OpenPlayerDatabase(const std::string& name, const std::string& savedir)
{
    PlayerDatabase* players = new PlayerDatabase();
    players->LoadPlayers(savedir + "/players.bin");
    return players;
}

bool LogicEnvironment::MigratePlayersDatabase(const GameParams& gameParams)
{
	return true;
}

AuthDatabase* LogicEnvironment::OpenAuthDatabase(const std::string& name, const std::string& savedir)
{
    return new AuthDatabase();
}

bool LogicEnvironment::MigrateAuthDatabase(const GameParams& gameParams)
{
	return true;
}
