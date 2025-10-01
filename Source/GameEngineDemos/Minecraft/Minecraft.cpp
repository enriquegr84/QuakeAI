//========================================================================
//
// MinecraftLogic Implementation       - Chapter 21, page 725
//
//========================================================================

#include "Minecraft.h"
#include "MinecraftApp.h"
#include "MinecraftView.h"
#include "MinecraftLevelManager.h"
#include "MinecraftActorFactory.h"
#include "MinecraftAIManager.h"
#include "MinecraftNetwork.h"
#include "MinecraftEvents.h"

#include "Core/Utility/SHA1.h"

#include "Physic/Physic.h"
#include "Physic/PhysicEventListener.h"
#include "Physic/Importer/PhysicResource.h"

#include "Games/Mods.h"
#include "Games/Subgames.h"
#include "Games/GameParams.h"

#include "Games/Actors/Craft.h"
#include "Games/Actors/PlayerLAO.h"
#include "Games/Actors/EntityLAO.h"
#include "Games/Actors/InventoryManager.h"
#include "Games/Actors/CraftingComponent.h"
#include "Games/Actors/ItemComponent.h"

#include "Games/Environment/LogicEnvironment.h"

#define TEXTURENAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-"

//
// MinecraftLogicThread::Run
//
void* MinecraftLogicThread::Run()
{

    /*
     * The real business of the server happens on the MinecraftLogicThread.
     * How this works:
     * LoadGameAsync() runs the game logical loading
     * AsyncRunStep() runs an actual server step as soon as enough time has
     * passed (dedicated_server_loop keeps track of that).
     */

    mGameLogic->LoadGameAsync(mLevelData);

    while (!StopRequested()) 
    {
        try {
            mGameLogic->AsyncStep();
        }
        catch (std::exception& )
        {
            throw;
        }
    }

    return nullptr;
}

//
// MinecraftLogic::MinecraftLogic
//
MinecraftLogic::MinecraftLogic() : GameLogic(), mPrintInfoTimer(0.f)
{
    mThread = new MinecraftLogicThread(this);
    mChatBackend = new ChatBackend();

	mPhysics.reset(CreateNullPhysics());
	RegisterAllDelegates();
}

//
// MinecraftLogic::~MinecraftLogic
//
MinecraftLogic::~MinecraftLogic()
{
	RemoveAllDelegates();
	DestroyAllNetworkEventForwarders();

    mChatBackend->AddMessage(L"", L"# Disconnected.");
    mChatBackend->AddMessage(L"", L"");
    mChatLogBuf = std::queue<std::string>();

    if (mEnvironment) 
    {
        MutexAutoLock(mEnvironment->mEnvMutex);

        LogInformation("Server: Saving players");
        mEnvironment->SaveLoadedPlayers();
    }

    // Do this before stopping the server in case mapgen callbacks need to access
    // server-controlled resources (like ModStorages). Also do them before
    // shutdown callbacks since they may modify state that is finalized in a
    // callback.
    if (mEmerge)
        mEmerge->StopThreads();

    delete mChatBackend;

    // Stop threads
    if (mThread) 
    {
        Stop();
        delete mThread;
    }

    if (mEnvironment)
    {
        MutexAutoLock(mEnvironment->mEnvMutex);

        if (mModMgr->ModsLoaded())
            BaseGame::Get()->Shutdown();

        mEnvironment->SaveMeta();
    }
}

void MinecraftLogic::Start(tinyxml2::XMLElement* pLevelData)
{
    LogInformation("Loading game world thread ");

    // Stop thread if already running
    mThread->Stop();

    mThread->mLevelData = pLevelData;

    // Start thread
    mThread->Start();
}

void MinecraftLogic::Stop()
{
    LogInformation("Stopping and waiting threads");

    // Stop threads (set run=false first so both start stopping)
    mThread->Stop();
    //m_emergethread.setRun(false);
    mThread->Wait();
    //m_emergethread.stop();

    LogInformation("Threads stopped");
}

void MinecraftLogic::Step(float dTime)
{

}

std::vector<ActorId> MinecraftLogic::GetPlayerIDs()
{
    return mPlayerIDs;
}

void MinecraftLogic::UpdatePlayerList()
{
    if (mEnvironment)
    {
        std::vector<ActorId> playerIds = GetPlayerIDs();
        //mActorsNames.clear();

        if (!playerIds.empty())
            LogInformation("Players:");

        for (ActorId playerId : playerIds)
        {
            std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(playerId);

            if (player == NULL)
                continue;

            LogInformation("* " + std::string(player->GetName()) + "\t");
            /*
            {
                RecursiveMutexAutoLock envlock(mEnvRecMutex);
                RemoteClient* client = lockedGetClientNoEx(i);
                if (client)
                    client->PrintInfo(infostream);
            }

            mClientsNames.emplace_back(player->GetName());
            */
        }
    }
}


// Logic Update
void MinecraftLogic::OnUpdate(float time, float deltaMs)
{
    GameLogic::OnUpdate(time, deltaMs);

    {
        MutexAutoLock lock1(mStepDeltaMsMutex);
        mStepDeltaMs += deltaMs;
    }

    //Get chat messages from visual
    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
    UpdateChat(deltaMs, screenSize);
}


// Step
void MinecraftLogic::AsyncStep()
{
    float dTime;
    {
        MutexAutoLock lock1(mStepDeltaMsMutex);
        dTime = mStepDeltaMs / 1000.f;
    }

    {
        // Send blocks to visuals
        SendBlocks(dTime);
    }

    if (dTime < 0.001)
        return;

    ScopeProfiler sp(Profiling, "MinecraftLogic::AsyncRunStep()", SPT_AVG);

    {
        MutexAutoLock lock1(mStepDeltaMsMutex);
        mStepDeltaMs -= mStepDeltaMs;
    }

    // Update uptime
    mEnvironment->mUptimeCounter->Increment(dTime);

    // Update time of day and overall game time
    mEnvironment->SetTimeOfDaySpeed(Settings::Get()->GetFloat("time_speed"));

    /*
        Send to visuals at constant intervals
    */

    mEnvironment->mTimeOfDaySendTimer -= dTime;
    if (mEnvironment->mTimeOfDaySendTimer < 0.f)
    {
        mEnvironment->mTimeOfDaySendTimer = Settings::Get()->GetFloat("time_send_interval");
        unsigned int time = mEnvironment->GetTimeOfDay();
        float timeSpeed = Settings::Get()->GetFloat("time_speed");
        SendTimeOfDay(INVALID_ACTOR_ID, time, timeSpeed);

        mEnvironment->mTimeofDayGauge->Set(time);
    }

    {
        MutexAutoLock lock(mEnvironment->mEnvMutex);
        // Figure out and report maximum lag to environment
        float maxLag = mEnvironment->GetMaxLagEstimate();
        maxLag *= 0.9998f; // Decrease slowly (about half per 5 minutes)
        if (dTime > maxLag)
        {
            if (dTime > 0.1f && dTime > maxLag * 2.f)
                LogInformation("Maximum lag peaked to " + std::to_string(dTime) + " s");
            maxLag = dTime;
        }
        mEnvironment->ReportMaxLagEstimate(maxLag);
        // Step environment
        mEnvironment->Step(dTime);
    }

    static const float mapTimerAndUnloadDeltaTime = 2.92f;
    if (mMapTimerAndUnloadInterval.Step(dTime, mapTimerAndUnloadDeltaTime))
    {
        MutexAutoLock lock(mEnvironment->mEnvMutex);
        // Run Map's timers and unload unused data
        ScopeProfiler sp(Profiling, "Map timer and unload");
        mEnvironment->GetMap()->TimerUpdate(mapTimerAndUnloadDeltaTime,
            Settings::Get()->GetFloat("server_unload_unused_data_timeout"), 0xFFFFFFFF);
    }

    /*
        Listen to the admin chat, if available
    */
    if (mAdminChat)
    {
        if (!mAdminChat->commandQueue.Empty())
        {
            MutexAutoLock lock(mEnvironment->mEnvMutex);
            while (!mAdminChat->commandQueue.Empty())
            {
                ChatEvent* evt = mAdminChat->commandQueue.PopFrontNoEx();
                HandleChatInterfaceEvent(evt);
                delete evt;
            }
        }
        mAdminChat->outgoingQueue.PushBack(
            new ChatEventTimeInfo(mEnvironment->GetGameTime(), mEnvironment->GetTimeOfDay()));
    }

    /*
        Send pending messages on out chat queue
    */
    if (!mOutChatQueue.empty() && CanSendChatMessage())
    {
        SendChatMessage(mOutChatQueue.front());
        mOutChatQueue.pop();
    }

    /*
        Do background stuff
    */

    /* Transform liquids */
    mLiquidTransformTimer += dTime;
    if (mLiquidTransformTimer >= mLiquidTransformEvery)
    {
        mLiquidTransformTimer -= mLiquidTransformEvery;

        MutexAutoLock lock(mEnvironment->mEnvMutex);

        ScopeProfiler sp(Profiling, "Liquid transform");

        std::map<Vector3<short>, MapBlock*> modifiedBlocks;
        mEnvironment->GetMap()->TransformLiquids(modifiedBlocks, mEnvironment.get());

        /*
            Set the modified blocks unsent for all the visuals
        */
        if (!modifiedBlocks.empty())
            mEnvironment->SetBlocksNotSent(modifiedBlocks);
    }

    mPrintInfoTimer += dTime;
    if (mPrintInfoTimer >= 30.0f)
    {
        mPrintInfoTimer = 0.0f;
        UpdatePlayerList();
    }

    /*
        Check added and deleted active objects
    */
    {
        //infostream<<"Checking added and deleted active objects"<<std::endl;
        mEnvironment->mEnvRecMutex.lock();

        ScopeProfiler sp(Profiling, "Update objects within range");

        std::vector<ActorId> playerIds = GetPlayerIDs();
        mEnvironment->mPlayerGauge->Set((double)playerIds.size());

        for (ActorId playerId : playerIds)
        {
            // This can happen if the visual times out somehow
            if (!mEnvironment->GetPlayer(playerId))
                continue;

            PlayerLAO* playerLAO = GetPlayerLAO(playerId);
            if (!playerLAO)
                continue;

            SendActiveObjectRemoveAdd(playerLAO);
        }
        mEnvironment->mEnvRecMutex.unlock();

        // Save mod storages if modified
        mModStorageSaveTimer -= dTime;
        if (mModStorageSaveTimer <= 0.0f)
        {
            mModStorageSaveTimer = Settings::Get()->GetFloat("server_map_save_interval");
            int n = 0;
            for (std::unordered_map<std::string, ModMetadata*>::const_iterator
                it = mModStorages.begin(); it != mModStorages.end(); ++it)
            {
                if (it->second->IsModified())
                {
                    it->second->Save(mWorldSpec.mPath + "/mod_storage");
                    n++;
                }
            }
            if (n > 0)
                LogInformation("Saved " + std::to_string(n) + " modified mod storages.");
        }
    }

    /*
        Send object messages
    */
    {
        MutexAutoLock envlock(mEnvironment->mEnvMutex);
        ScopeProfiler sp(Profiling, "Send LAO messages");

        // Key = object id
        // Value = data sent by object
        std::unordered_map<uint16_t, std::vector<ActiveObjectMessage>*> bufferedMessages;

        // Get active object messages from environment
        ActiveObjectMessage aom(0);
        uint32_t aomCount = 0;
        for (;;)
        {
            if (!mEnvironment->GetActiveObjectMessage(&aom))
                break;

            std::vector<ActiveObjectMessage>* messageList = nullptr;
            auto n = bufferedMessages.find(aom.id);
            if (n == bufferedMessages.end())
            {
                messageList = new std::vector<ActiveObjectMessage>;
                bufferedMessages[aom.id] = messageList;
            }
            else
            {
                messageList = n->second;
            }
            messageList->push_back(std::move(aom));
            aomCount++;
        }

        mEnvironment->mAOMBufferCounter->Increment(aomCount);

        // Route data to every visual
        mEnvironment->mEnvRecMutex.lock();
        std::vector<ActorId> playerIds = GetPlayerIDs();

        std::string reliableData, unreliableData;
        for (ActorId playerId : playerIds)
        {
            reliableData.clear();
            unreliableData.clear();

            PlayerLAO* player = GetPlayerLAO(playerId);
            if (player == NULL)
                continue;

            // Go through all objects in message buffer
            for (const auto& bufferedMessage : bufferedMessages)
            {
                // If object does not exist or is not known by visual, skip it
                uint16_t id = bufferedMessage.first;
                LogicActiveObject* lao = mEnvironment->GetActiveObject(id);
                if (!lao || mEnvironment->mKnownObjects.find(id) == mEnvironment->mKnownObjects.end())
                    continue;

                // Get message list of object
                std::vector<ActiveObjectMessage>* list = bufferedMessage.second;
                // Go through every message
                for (const ActiveObjectMessage& aom : *list)
                {
                    // Send position updates to players who do not see the attachment
                    if (aom.data[0] == AO_CMD_UPDATE_POSITION)
                    {
                        if (lao->GetId() == player->GetId())
                            continue;

                        // Do not send position updates for attached players
                        // as long the parent is known to the visual
                        LogicActiveObject* parent = lao->GetParent();
                        if (parent && mEnvironment->mKnownObjects.find(parent->GetId()) != mEnvironment->mKnownObjects.end())
                            continue;
                    }

                    // Add full new data to appropriate buffer
                    std::string& buffer = aom.reliable ? reliableData : unreliableData;
                    char idBuffer[2];
                    WriteUInt16((uint8_t*)idBuffer, aom.id);

                    buffer.append(idBuffer, sizeof(idBuffer));
                    buffer.append(SerializeString16(aom.data));
                }
            }
            /*
                reliable_data and unreliable_data are now ready.
                Send them.
            */
            if (!reliableData.empty())
                SendActiveObjectMessages(reliableData); //mVisualEnv->ProcessActiveObjectMessage(clientId, reliableData);

            if (!unreliableData.empty())
                SendActiveObjectMessages(unreliableData); //mVisualEnv->ProcessActiveObjectMessage(clientId, unreliableData);
        }
        mEnvironment->mEnvRecMutex.unlock();

        // Clear buffered_messages
        for (auto& bufferedMessage : bufferedMessages)
            delete bufferedMessage.second;
    }

    /*
        Send queued-for-sending map edit events.
    */
    {
        // We will be accessing the environment
        MutexAutoLock lock(mEnvironment->mEnvMutex);

        // Don't send too many at a time
        //unsigned int count = 0;

        // Single change sending is disabled if queue size is not small
        bool disableSingleChangeSending = false;
        if (mEnvironment->mUnsentMapEditQueue.size() >= 4)
            disableSingleChangeSending = true;

        int eventCount = (int)mEnvironment->mUnsentMapEditQueue.size();

        // We'll log the amount of each
        Profiler prof;

        std::list<Vector3<short>> nodeMetaUpdates;
        while (!mEnvironment->mUnsentMapEditQueue.empty())
        {
            MapEditEvent* evt = mEnvironment->mUnsentMapEditQueue.front();
            mEnvironment->mUnsentMapEditQueue.pop();

            // Players far away from the change are stored here.
            // Instead of sending the changes, MapBlocks are set not sent
            // for them.
            std::unordered_set<ActorId> farPlayers;

            switch (evt->type)
            {
                case MEET_ADDNODE:
                case MEET_SWAPNODE:
                    prof.Add("MEET_ADDNODE", 1);
                    SendAddNode(evt->position, evt->node, &farPlayers,
                        disableSingleChangeSending ? 5.f : 30.f, evt->type == MEET_ADDNODE);
                    break;
                case MEET_REMOVENODE:
                    prof.Add("MEET_REMOVENODE", 1);
                    SendRemoveNode(evt->position, &farPlayers,
                        disableSingleChangeSending ? 5.f : 30.f);
                    break;
                case MEET_BLOCK_NODE_METADATA_CHANGED:
                {
                    prof.Add("MEET_BLOCK_NODE_METADATA_CHANGED", 1);
                    if (!evt->IsPrivateChange)
                    {
                        // Don't send the change yet. Collect them to eliminate dupes.
                        nodeMetaUpdates.remove(evt->position);
                        nodeMetaUpdates.push_back(evt->position);
                    }

                    if (MapBlock* block = mEnvironment->GetMap()->GetBlockNoCreateNoEx(
                        GetNodeBlockPosition(evt->position)))
                    {
                        block->RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_REPORT_META_CHANGE);
                    }
                    break;
                }
                case MEET_OTHER:
                    prof.Add("MEET_OTHER", 1);
                    for (const Vector3<short>& modifiedBlock : evt->modifiedBlocks)
                        mEnvironment->SetBlockNotSent(modifiedBlock);

                    break;
                default:
                    prof.Add("unknown", 1);
                    LogWarning("Unknown MapEditEvent " + std::to_string(evt->type));
                    break;
            }

            /*
                Set blocks not sent to far players
            */
            if (!farPlayers.empty())
            {
                // Convert list format to that wanted by SetBlocksNotSent
                std::map<Vector3<short>, MapBlock*> modifiedBlocks2;
                for (const Vector3<short>& modifiedBlock : evt->modifiedBlocks)
                {
                    modifiedBlocks2[modifiedBlock] =
                        mEnvironment->GetMap()->GetBlockNoCreateNoEx(modifiedBlock);
                }

                // Set blocks not sent
                for (const uint16_t farPlayer : farPlayers)
                {
                    //uint16_t clientId = GetClientId(farPlayer);
                    mEnvironment->SetBlocksNotSent(modifiedBlocks2);
                }
            }

            delete evt;
        }

        if (eventCount >= 5)
        {
            std::stringstream infostream;
            infostream << "MapEditEvents:" << std::endl;
            prof.Print(infostream);
            LogInformation(infostream.str());
        }
        else if (eventCount != 0)
        {
            std::stringstream infostream;
            infostream << "MapEditEvents:" << std::endl;
            prof.Print(infostream);
            LogInformation(infostream.str());
        }

        // Send all metadata updates
        if (nodeMetaUpdates.size())
            SendMetadataChanged(nodeMetaUpdates);
    }

    /*
        Trigger emergethread (it somehow gets to a non-triggered but
        busy state sometimes)
    */
    {
        float& counter = mEmergethreadTriggerTimer;
        counter += dTime;
        if (counter >= 2.f)
        {
            counter = 0.0;

            mEmerge->StartThreads();
        }
    }

    // Save map, players and auth stuff
    {
        float& counter = mSavemapTimer;
        counter += dTime;
        static thread_local const float saveInterval =
            Settings::Get()->GetFloat("server_map_save_interval");
        if (counter >= saveInterval)
        {
            counter = 0.f;
            MutexAutoLock lock(mEnvironment->mEnvMutex);

            ScopeProfiler sp(Profiling, "Map saving (sum)");

            // Save changed parts of map
            mEnvironment->GetMap()->Save(MOD_STATE_WRITE_NEEDED);

            // Save players
            mEnvironment->SaveLoadedPlayers();

            // Save environment metadata
            mEnvironment->SaveMeta();
        }
    }
    //mShutdownState.Tick(dTime, this);
}

void MinecraftLogic::DeletePlayer(ActorId actorId)
{
    std::wstring message;
    {
        /*
            Clear references to playing sounds

        for (std::unordered_map<s32, ServerPlayingSound>::iterator
            i = m_playing_sounds.begin(); i != m_playing_sounds.end();)
        {
            ServerPlayingSound &psound = i->second;
            psound.clients.erase(actorId);
            if (psound.clients.empty())
                m_playing_sounds.erase(i++);
            else
                ++i;
        }
        */
        // clear form info so the next visual can't abuse the current state
        mEnvironment->mFormStateData.erase(actorId);

        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);

        /* Run scripts and remove from environment */
        if (player)
        {
            PlayerLAO* playerLAO = player->GetPlayerLAO();
            LogAssert(playerLAO, "invalid player");

            playerLAO->ClearChildAttachments();
            playerLAO->ClearParentAttachment();

            // run scripts
            BaseGame::Get()->OnLeavePlayer(playerLAO);

            playerLAO->Disconnected();
        }
    }

    // Send leave chat message to all remaining visuals
    if (!message.empty())
        SendChatMessage(message);
}


void MinecraftLogic::UpdateViewType(const std::shared_ptr<BaseGameView>& pView, bool add)
{
    GameLogic::UpdateViewType(pView, add);

    //  This is commented out because while the view is created and waiting, the player is NOT attached yet. 
    /*
    if (pView->GetType() == GV_REMOTE)
    {
        mHumanPlayersAttached += add ? 1 : -1;
    }
    */
    if (pView->GetType() == GV_HUMAN)
    {
        mHumanPlayersAttached += add ? 1 : -1;
    }
    else if (pView->GetType() == GV_AI)
    {
        mAIPlayersAttached += add ? 1 : -1;
    }
}

void MinecraftLogic::ResetViewType()
{
    GameLogic::ResetViewType();

    mHumanPlayersAttached = 0;
    mAIPlayersAttached = 0;
}

void MinecraftLogic::SetProxy()
{
    GameLogic::SetProxy();
}


void MinecraftLogic::SendPlayerPrivileges(ActorId actorId)
{

}

void MinecraftLogic::SendPlayerInventoryForm(ActorId actorId)
{
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
    LogAssert(player, "invalid player");
    if (player->GetId() == INVALID_ACTOR_ID)
        return;

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerInventoryForm>(player->GetId(), player->mInventoryForm));
}

void MinecraftLogic::SendTimeOfDay(ActorId actorId, unsigned int time, float timeSpeed)
{
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataTimeOfDay>(time, timeSpeed));
}

bool MinecraftLogic::HudSetFlags(LogicPlayer *player, unsigned int flags, unsigned int mask)
{
    if (!player)
        return false;

    unsigned int setFlags = flags;
    setFlags &= ~(HUD_FLAG_HEALTHBAR_VISIBLE | HUD_FLAG_BREATHBAR_VISIBLE);

    EventManager::Get()->QueueEvent(std::make_shared<EventDataHudSetFlags>(mask, setFlags));

    player->mHudFlags &= ~mask;
    player->mHudFlags |= flags;

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (!playerLAO)
        return false;

    BaseGame::Get()->OnEventPlayer(playerLAO, "hud_changed");
    return true;
}

bool MinecraftLogic::HudSetHotbarItemCount(LogicPlayer* player, int hotbarItemCount)
{
    if (!player)
        return false;

    if (hotbarItemCount <= 0 || hotbarItemCount > HUD_HOTBAR_ITEMCOUNT_MAX)
        return false;

    player->SetHotbarItemCount(hotbarItemCount);
    std::ostringstream os(std::ios::binary);
    WriteInt32(os, hotbarItemCount);
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHudSetParam>(HUD_PARAM_HOTBAR_ITEMCOUNT, os.str()));
    return true;
}

void MinecraftLogic::SetSky(LogicPlayer* player, const SkyboxParams& params)
{
    LogAssert(player, "invalid player");
    player->SetSky(params);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHudSetSky>(params.bgcolor, params.type, params.clouds,
            params.fogSunTint, params.fogMoonTint, params.fogTintType, params.skyColor, params.textures));
}

void MinecraftLogic::SetSun(LogicPlayer* player, const SunParams& params)
{
    LogAssert(player, "invalid player");
    player->SetSun(params);

    EventManager::Get()->QueueEvent(std::make_shared<EventDataHudSetSun>(
            params.texture, params.toneMap, params.sunrise, params.sunriseVisible, params.visible, params.scale));
}

void MinecraftLogic::SetMoon(LogicPlayer* player, const MoonParams& params)
{
    LogAssert(player, "invalid player");
    player->SetMoon(params);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHudSetMoon>(params.texture, params.toneMap, params.visible, params.scale));
}

void MinecraftLogic::SetStars(LogicPlayer* player, const StarParams& params)
{
    LogAssert(player, "invalid player");
    player->SetStars(params);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHudSetStars>(params.starcolor, params.count, params.visible, params.scale));
}

void MinecraftLogic::SetClouds(LogicPlayer* player, const CloudParams& params)
{
    LogAssert(player, "invalid player");
    player->SetCloudParams(params);

    EventManager::Get()->QueueEvent(std::make_shared<EventDataSetClouds>(
        params.colorBright, params.colorAmbient, params.speed, params.thickness, params.density, params.height));
}

void MinecraftLogic::OverrideDayNightRatio(LogicPlayer* player, bool doOverride, float ratio)
{
    LogAssert(player, "invalid player");
    player->OverrideDayNightRatio(doOverride, ratio);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataOverrideDayNightRatio>(doOverride, ratio));
}

void MinecraftLogic::SetPlayerEyeOffset(LogicPlayer* player, const Vector3<float>& first, const Vector3<float>& third)
{
    LogAssert(player, "invalid player");
    player->mEyeOffsetFirst = first;
    player->mEyeOffsetThird = third;

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerEyeOffset>(player->GetId(), first, third));
}

void MinecraftLogic::SendActiveObjectRemoveAdd(PlayerLAO* playerLAO)
{
    // Radius inside which objects are active
    static thread_local const int16_t radius =
        Settings::Get()->GetInt16("active_object_send_range_blocks") * MAP_BLOCKSIZE;

    // Radius inside which players are active
    static thread_local const bool isTransferLimited =
        Settings::Get()->Exists("unlimited_player_transfer_distance") &&
        !Settings::Get()->GetBool("unlimited_player_transfer_distance");

    static thread_local const int16_t playerTransferDist =
        Settings::Get()->GetInt16("player_transfer_distance") * MAP_BLOCKSIZE;

    int16_t playerRadius = playerTransferDist == 0 && isTransferLimited ? radius : playerTransferDist;
    int16_t myRadius = std::min(radius, (int16_t)(playerLAO->GetWantedRange() * MAP_BLOCKSIZE));
    if (myRadius <= 0) myRadius = radius;

    std::queue<uint16_t> removedObjects, addedObjects;
    mEnvironment->GetRemovedActiveObjects(
        playerLAO, myRadius, playerRadius, mEnvironment->mKnownObjects, removedObjects);
    mEnvironment->GetAddedActiveObjects(
        playerLAO, myRadius, playerRadius, mEnvironment->mKnownObjects, addedObjects);

    int removedCount = (int)removedObjects.size();
    int addedCount = (int)addedObjects.size();

    if (removedObjects.empty() && addedObjects.empty())
        return;

    char buf[4];
    std::string data;

    // Handle removed objects
    WriteUInt16((uint8_t*)buf, (uint16_t)removedObjects.size());
    data.append(buf, 2);
    while (!removedObjects.empty())
    {
        // Get object
        uint16_t id = removedObjects.front();
        LogicActiveObject* obj = mEnvironment->GetActiveObject(id);

        // Add to data buffer for sending
        WriteUInt16((uint8_t*)buf, id);
        data.append(buf, 2);

        // Remove from known objects
        mEnvironment->mKnownObjects.erase(id);

        if (obj && obj->mKnownByCount > 0)
            obj->mKnownByCount--;

        removedObjects.pop();
    }

    // Handle added objects
    WriteUInt16((uint8_t*)buf, (uint16_t)addedObjects.size());
    data.append(buf, 2);
    while (!addedObjects.empty())
    {
        // Get object
        uint16_t id = addedObjects.front();
        LogicActiveObject* obj = mEnvironment->GetActiveObject(id);
        addedObjects.pop();

        if (!obj)
        {
            LogWarning("NULL object id=" + std::to_string(id));
            continue;
        }

        // Get object type
        uint8_t type = obj->GetSendType();

        // Add to data buffer for sending
        WriteUInt16((uint8_t*)buf, id);
        data.append(buf, 2);
        WriteUInt8((uint8_t*)buf, type);
        data.append(buf, 1);

        data.append(
            SerializeString32(obj->GetVisualInitializationData()));

        // Add to known objects
        mEnvironment->mKnownObjects.insert(id);

        obj->mKnownByCount++;
    }

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataActiveObjectRemoveAdd>(data, data.size()));
}

void MinecraftLogic::SendActiveObjectMessages(const std::string& data, bool reliable)
{
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataActiveObjectMessages>(data, data.size()));
}

void MinecraftLogic::NotifyPlayer(const char* name, const std::wstring& msg)
{
    // m_env will be NULL if the server is initializing
    if (!mEnvironment)
        return;

    if (mAdminNick == name && !mAdminNick.empty())
        mAdminChat->outgoingQueue.PushBack(new ChatEventChat("", msg));

    SendChatMessage(msg);
}

void MinecraftLogic::NotifyPlayers(const std::wstring& msg)
{
    SendChatMessage(msg);
}

bool MinecraftLogic::CanSendChatMessage() const
{
    unsigned int now = (unsigned int)time(NULL);
    float timePassed = (float)(now - mLastChatMessageSent);

    float virtChatMessageAllowance =
        mChatMessageAllowance + timePassed * (CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
    if (virtChatMessageAllowance < 1.0f)
        return false;

    return true;
}

void MinecraftLogic::SendChatMessage(const std::wstring& message)
{
    const short maxQueueSize = Settings::Get()->GetInt16("max_out_chat_queue_size");
    if (CanSendChatMessage())
    {
        unsigned int now = (unsigned int)time(NULL);
        float timePassed = (float)(now - mLastChatMessageSent);
        mLastChatMessageSent = now;

        mChatMessageAllowance += timePassed * (CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
        if (mChatMessageAllowance > CHAT_MESSAGE_LIMIT_PER_10S)
            mChatMessageAllowance = CHAT_MESSAGE_LIMIT_PER_10S;

        mChatMessageAllowance -= 1.0f;

        std::vector<ActorId> playerIds = GetPlayerIDs();
        for (ActorId playerId : playerIds)
        {
            std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(playerId);
            if (player == NULL)
            {
                LogError("Canceling: No player for actorId=" +
                    std::to_string(playerId) + " disconnecting peer!");
                return;
            }

            std::string name = player->GetName();

            std::wstring answerToSender = HandleChat(name, message, true, player.get());
            if (!answerToSender.empty())
            {
                // Send the answer to sender
                HandleChatMessage(playerId, ChatMessage(CHATMESSAGE_TYPE_SYSTEM, answerToSender));
            }
        }
    }
    else if (mOutChatQueue.size() < maxQueueSize || maxQueueSize == -1)
    {
        mOutChatQueue.push(message);
    }
    else
    {
        LogInformation("Could not queue chat message because maximum out chat queue size ("
            + std::to_string(maxQueueSize) + ") is reached.");
    }
}

void MinecraftLogic::HandleChatMessage(ActorId actorId, const ChatMessage& chat)
{
    /*
        unsigned char version
        unsigned char message_type
        unsigned short sendername length
        wstring sendername
        unsigned short length
        wstring message
     */
    ChatMessage* chatMessage = new ChatMessage();
    unsigned char version = 1;

    if (version != 1 || chat.type >= CHATMESSAGE_TYPE_MAX)
    {
        delete chatMessage;
        return;
    }

    chatMessage->message = chat.message;
    chatMessage->timestamp = static_cast<std::time_t>(chat.timestamp);
    chatMessage->type = (ChatMessageType)chat.type;

    // @TODO send this to CSM using ChatMessage object
    if (mModMgr->ModsLoaded() && mGame->OnReceivingChatMessage(ToString(chatMessage->message)))
    {
        // Message was consumed by CSM and should not be handled by visual
        delete chatMessage;
    }
    else PushToChatQueue(chatMessage);
}


// Chat message formatter

// Implemented to allow redefinition
std::wstring MinecraftLogic::FormatChatMessage(const std::string& name, const std::string& message)
{
    std::string errorStr = "Invalid chat message format - missing %s";
    std::string str = Settings::Get()->Get("chat_message_format");

    char timeBuf[30];
    Timer::RealTimeDate time = Timer::GetRealTimeAndDate();
    snprintf(timeBuf, sizeof(timeBuf), "%uH:%uM:%uS", time.Hour, time.Minute, time.Second);;

    StringReplace(str, "@name", "<" + name + ">");
    StringReplace(str, "@timestamp", timeBuf);
    StringReplace(str, "@message", message);

    return ToWideString(str);
}

// Chat command handler
bool MinecraftLogic::OnChatMessage(const std::string& name, const std::string& message)
{
    //core.chatcommands = core.registered_chatcommands // BACKWARDS COMPATIBILITY
    float msgTimeThreshold = 0.1f;
    if (Settings::Get()->Exists("chatcommand_msg_time_threshold"))
    {
        msgTimeThreshold =
            Settings::Get()->GetFloat("chatcommand_msg_time_threshold");
    }

    if (message.front() != '/')
        return false;

    return true;  // Handled chat message
}

void MinecraftLogic::HandleAdminChat(const ChatEventChat* evt)
{
    std::string name = evt->nick;
    std::wstring wmessage = evt->evtMsg;

    std::wstring answer = HandleChat(name, wmessage);

    // If asked to send answer to sender
    if (!answer.empty())
        mAdminChat->outgoingQueue.PushBack(new ChatEventChat("", answer));
}

void MinecraftLogic::HandleChatInterfaceEvent(ChatEvent* evt)
{
    if (evt->type == CET_NICK_ADD)
    {
        // The terminal informed us of its nick choice
        mAdminNick = ((ChatEventNick *)evt)->nick;
        /*
        if (!m_script->getAuth(mAdminNick, NULL, NULL)) {
            errorstream << "You haven't set up an account." << std::endl
                << "Please log in using the visual as '"
                << m_admin_nick << "' with a secure password." << std::endl
                << "Until then, you can't execute admin tasks via the console," << std::endl
                << "and everybody can claim the user account instead of you," << std::endl
                << "giving them full control over this logic." << std::endl;
        }
        */
    }
    else
    {
        LogAssert(evt->type == CET_CHAT, "invalid type");
        HandleAdminChat((ChatEventChat *)evt);
    }
}

bool MinecraftLogic::GetChatMessage(std::wstring& res)
{
    if (mChatQueue.empty())
        return false;

    ChatMessage* chatMessage = mChatQueue.front();
    mChatQueue.pop();

    res = L"";

    switch (chatMessage->type)
    {
        case CHATMESSAGE_TYPE_RAW:
        case CHATMESSAGE_TYPE_ANNOUNCE:
        case CHATMESSAGE_TYPE_SYSTEM:
            res = chatMessage->message;
            break;
        case CHATMESSAGE_TYPE_NORMAL:
        {
            if (!chatMessage->sender.empty())
                res = L"<" + chatMessage->sender + L"> " + chatMessage->message;
            else
                res = chatMessage->message;
            break;
        }
        default:
            break;
    }

    delete chatMessage;
    return true;
}

void MinecraftLogic::UpdateChat(float dTime, const Vector2<unsigned int>& screensize)
{
    // Get new messages from error log buffer
    while (!mChatLogBuf.empty())
    {
        mChatBackend->AddMessage(L"", ToWideString(mChatLogBuf.front()));
        mChatLogBuf.pop();
    }

    // Get new messages from visual
    std::wstring message;
    while (GetChatMessage(message))
        mChatBackend->AddUnparsedMessage(message);

    // Remove old messages
    mChatBackend->Step(dTime);

    EventManager::Get()->QueueEvent(std::make_shared<EventDataUpdateChat>(
        mChatBackend->GetRecentBuffer().GetLineCount(), mChatBackend->GetRecentChat()));
}

std::wstring MinecraftLogic::HandleChat(const std::string& name,
    std::wstring messageInput, bool checkShoutPriv, LogicPlayer* player)
{
    // If something goes wrong, this player is to blame
    //RollbackScopeActor rollback_scope(m_rollback, std::string("player:") + name);

    if (Settings::Get()->GetBool("strip_color_codes"))
        messageInput = UnescapeEnriched(messageInput);

    if (player)
    {
        switch (player->CanSendChatMessage())
        {
            case LPLAYER_CHATRESULT_FLOODING:
            {
                std::wstringstream ws;
                ws << L"You cannot send more messages. You are limited to "
                    << Settings::Get()->GetFloat("chat_message_limit_per_10sec")
                    << L" messages per 10 seconds.";
                return ws.str();
            }
            case LPLAYER_CHATRESULT_KICK:
                //DenyAccessLegacy(player->GetId(), L"You have been kicked due to message flooding.");
                return L"";
            case LPLAYER_CHATRESULT_OK:
                break;
            default:
                LogError("Unhandled chat filtering result found.");
        }
    }

    if (mMaxChatMessageLength > 0 && messageInput.length() > mMaxChatMessageLength)
    {
        return L"Your message exceed the maximum chat message limit set on the logic. "
            L"It was refused. Send a shorter message";
    }

    auto message = Trim(ToString(messageInput));
    if (message.find_first_of("\n\r") != std::wstring::npos)
        return L"Newlines are not permitted in chat messages";

    // Run script hook, exit if script ate the chat message
    if (OnChatMessage(name, message))
        return L"";

    // Line to send
    std::wstring line;
    // Whether to send line to the player that sent the message, or to all players
    bool broadcastLine = true;

    if (!checkShoutPriv)
    {
        line += L"-!- You don't have permission to shout.";
        broadcastLine = false;
    }
    else
    {
        line += FormatChatMessage(name, ToString(messageInput));
    }

    /*
        Tell calling method to send the message to sender
    */
    if (!broadcastLine)
        return line;

    /*
        Send the message to others
    */
    LogInformation("CHAT: " + ToString(UnescapeEnriched(line)));

    ChatMessage chatmsg(line);

    std::vector<ActorId> playerIds = GetPlayerIDs();
    for (ActorId playerId : playerIds)
        HandleChatMessage(playerId, chatmsg);

    return L"";
}

PlayerLAO* MinecraftLogic::GetPlayerLAO(ActorId actorId)
{
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
    if (!player)
        return NULL;
    return player->GetPlayerLAO();
}


void MinecraftLogic::RespawnPlayer(ActorId actorId)
{
    PlayerLAO* playerLAO = GetPlayerLAO(actorId);
    LogAssert(playerLAO, "invalid player");

    LogInformation("RespawnPlayer(): Player " +
        std::string(playerLAO->GetPlayer()->GetName()) + " respawns");

    playerLAO->SetHP(playerLAO->AccessObjectProperties()->hpMax,
        PlayerHPChangeReason(PlayerHPChangeReason::RESPAWN));
    playerLAO->SetBreath(playerLAO->AccessObjectProperties()->breathMax);

    try
    {
        Vector3<float> staticSpawnPoint = Settings::Get()->GetVector3("static_spawnpoint");
        LogInformation("Moving " + std::string(playerLAO->GetPlayer()->GetName()) +
            " to static spawnpoint at (" + std::to_string(staticSpawnPoint[0]) + "," +
            std::to_string(staticSpawnPoint[1]) + "," + std::to_string(staticSpawnPoint[2]) + ")");
        playerLAO->SetPosition(staticSpawnPoint * BS);
        // Dont place node when player would be inside new node
    }
    catch (const SettingNotFoundException&)
    {
        playerLAO->SetPosition(mEnvironment->FindSpawnPosition());
    }

    mEnvironment->SendPlayerHP(actorId);
}

bool MinecraftLogic::CheckInteractDistance(LogicPlayer* player, const float distance, const std::string& what)
{
    ItemStack selectedItem, handItem;
    player->GetWieldedItem(&selectedItem, &handItem);
    float maxDistance = BS * GetToolRange(
        selectedItem.GetDefinition(mEnvironment->GetItemManager()), 
        handItem.GetDefinition(mEnvironment->GetItemManager()));

    // Cube diagonal * 1.5 for maximal supported node extents:
    // sqrt(3) * 1.5 = 2.6
    if (distance > maxDistance + 2.6f * BS)
    {
        LogInformation("Player " + std::string(player->GetName()) +
            " tried to access " + what + " from too far: distance=" +
            std::to_string(distance) + ", maxDistance=" +
            std::to_string(maxDistance) + "; ignoring.");
        // Call callbacks
        //m_script->on_cheat(player->getPlayerLAO(), "interacted_too_far");
        return false;
    }
    return true;
}

void MinecraftLogic::SendMovement(ActorId playerId)
{
    float g, mad, maa, maf, msw, mscr, msf, mscl, msj, lf, lfs, ls;

    g = Settings::Get()->GetFloat("movement_gravity");
    mad = Settings::Get()->GetFloat("movement_acceleration_default");
    maa = Settings::Get()->GetFloat("movement_acceleration_air");
    maf = Settings::Get()->GetFloat("movement_acceleration_fast");
    msw =  Settings::Get()->GetFloat("movement_speed_walk");
    mscr = Settings::Get()->GetFloat("movement_speed_crouch");
    msf = Settings::Get()->GetFloat("movement_speed_fast");
    mscl = Settings::Get()->GetFloat("movement_speed_climb");
    msj = Settings::Get()->GetFloat("movement_speed_jump");
    lf = Settings::Get()->GetFloat("movement_liquid_fluidity");
    lfs = Settings::Get()->GetFloat("movement_liquid_fluidity_smooth");
    ls = Settings::Get()->GetFloat("movement_liquid_sink");

    EventManager::Get()->QueueEvent(std::make_shared<EventDataMovement>(
        playerId, g, mad, maa, maf, lf, lfs, ls, msw, mscr, msf, mscl, msj));
}

PlayerLAO* MinecraftLogic::EmergePlayer(const char* name, ActorId actorId)
{
    //Try to get an existing player
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(name);

    // If player is already connected, cancel
    if (player && player->GetId() != INVALID_ACTOR_ID)
    {
        LogInformation("EmergePlayer(): Player already connected");
        return NULL;
    }

    /*
        If player with the wanted actorId already exists, cancel.
    */
    if (mEnvironment->GetPlayer(actorId))
    {
        LogInformation("EmergePlayer(): Player with wrong name but same actorId already exists");
        return NULL;
    }

    if (!player)
        player = CreatePlayer(actorId, name, "actors\\minecraft\\players\\tutorial\\player.xml", NULL);

    bool newPlayer = false;

    // Load player
    PlayerLAO* playerLAO = mEnvironment->LoadPlayer(player, &newPlayer, mEnvironment->IsSingleplayer());

    // Complete init with logic parts
    //playerLAO->Finalize(player, GetPlayerEffectivePrivileges(player->GetName()));
    //player->protocol_version = proto_version;

    /* Run scripts */
    if (newPlayer)
        RespawnPlayer(playerLAO->GetId());

    return playerLAO;
}

// Spawns a particle on actor with actorId
void MinecraftLogic::SendSpawnParticle(ActorId actorId, const ParticleParameters& parameters)
{
    static thread_local const float radius =
        Settings::Get()->GetInt16("max_block_send_distance") * MAP_BLOCKSIZE * BS;

    if (actorId == INVALID_ACTOR_ID)
    {
        std::vector<ActorId> playerIds = GetPlayerIDs();
        const Vector3<float> pos = parameters.pos * BS;
        const float radiusSq = radius * radius;

        for (ActorId playerId : playerIds)
        {
            std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
            if (!player)
                continue;

            PlayerLAO* playerLAO = player->GetPlayerLAO();
            if (!playerLAO)
                continue;

            // Do not send to distant visuals
            if (LengthSq(playerLAO->GetBasePosition() - pos) > radiusSq)
                continue;

            SendSpawnParticle(actorId, parameters);
        }
        return;
    }

    uint16_t protoVersion = 39;
    LogAssert(protoVersion != 0, "invalid version");


    // NetworkPacket and iostreams are incompatible...
    std::ostringstream oss(std::ios_base::binary);
    parameters.Serialize(oss, protoVersion);

    EventManager::Get()->QueueEvent(std::make_shared<EventDataSpawnParticle>(oss.str()));
}

// Adds a ParticleSpawner on actor with peer_id
void MinecraftLogic::SendAddParticleSpawner(ActorId actorId,
    const ParticleSpawnerParameters& parameters, ActorId attachedId, unsigned int id)
{
    static thread_local const float radius =
        Settings::Get()->GetInt16("max_block_send_distance") * MAP_BLOCKSIZE * BS;

    if (actorId == INVALID_ACTOR_ID)
    {
        std::vector<ActorId> playerIds = GetPlayerIDs();
        const Vector3<float> pos = (parameters.minPos + parameters.maxPos) / 2.0f * BS;
        const float radiusSq = radius * radius;
        /* Don't send short-lived spawners to distant players.
         * This could be replaced with proper tracking at some point. */
        const bool distanceCheck = !attachedId && parameters.time <= 1.0f;

        for (ActorId playerId : playerIds)
        {
            std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
            if (!player)
                continue;

            if (distanceCheck)
            {
                PlayerLAO* playerLAO = player->GetPlayerLAO();
                if (!playerLAO)
                    continue;
                if (LengthSq(playerLAO->GetBasePosition() - pos) > radiusSq)
                    continue;
            }

            SendAddParticleSpawner(actorId, parameters, attachedId, id);
        }
        return;
    }

    std::ostringstream os(std::ios_base::binary);
    parameters.animation.Serialize(os);

    std::shared_ptr<EventDataAddParticleSpawner> pNewEvent(
        new EventDataAddParticleSpawner(id, attachedId, parameters.texture, os.str(), 
            parameters.collisionDetection, parameters.collisionRemoval, parameters.objectCollision, 
            parameters.vertical, parameters.glow, parameters.nodeTile, parameters.node.param2, 
            parameters.node.param0, parameters.time, parameters.amount, parameters.minPos, parameters.maxPos, 
            parameters.minVel, parameters.maxVel, parameters.minAcc, parameters.maxAcc, parameters.minExpTime, 
            parameters.maxExpTime, parameters.minSize, parameters.maxSize));
    BaseEventManager::Get()->QueueEvent(pNewEvent);
}

void MinecraftLogic::SendDeleteParticleSpawner(ActorId actorId, unsigned int id)
{
    EventManager::Get()->QueueEvent(std::make_shared<EventDataDeleteParticleSpawner>(id));
}

void MinecraftLogic::SendRemoveNode(Vector3<short> position,
    std::unordered_set<ActorId>* farPlayers, float farDistNodes)
{
    float maxDist = farDistNodes * BS;

    Vector3<float> blockPos = Vector3<float>{
        position[0] * BS, position[1] * BS, position[2] * BS };
    Vector3<short> blockPosition = GetNodeBlockPosition(position);

    mEnvironment->mEnvRecMutex.lock();

    std::vector<ActorId> playerIds = GetPlayerIDs();
    for (ActorId playerId : playerIds)
    {
        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(playerId);
        PlayerLAO* playerLAO = player ? player->GetPlayerLAO() : nullptr;

        // If player is far away, only set modified blocks not sent
        if (!mEnvironment->IsBlockSent(blockPosition) ||
            (playerLAO && Length(playerLAO->GetBasePosition() - blockPos) > maxDist))
        {
            if (farPlayers)
                farPlayers->emplace(playerId);
            else
                mEnvironment->SetBlockNotSent(blockPosition);
            continue;
        }

        // Send as reliable
        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataMapNodeRemove>(position));
    }
    mEnvironment->mEnvRecMutex.unlock();
}

void MinecraftLogic::SendAddNode(Vector3<short> position, MapNode node,
    std::unordered_set<ActorId>* farPlayers, float farDistNodes, bool removeMetadata)
{
    float maxd = farDistNodes * BS;

    Vector3<float> blockPos = Vector3<float>{
        position[0] * BS, position[1] * BS, position[2] * BS };
    Vector3<short> blockPosition = GetNodeBlockPosition(position);

    mEnvironment->mEnvRecMutex.lock();

    std::vector<ActorId> playerIds = GetPlayerIDs();
    for (ActorId playerId : playerIds)
    {
        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(playerId);
        PlayerLAO* playerLAO = player ? player->GetPlayerLAO() : nullptr;

        // If player is far away, only set modified blocks not sent
        if (!mEnvironment->IsBlockSent(blockPosition) ||
            (playerLAO && Length(playerLAO->GetBasePosition() - blockPos) > maxd))
        {
            if (farPlayers)
                farPlayers->emplace(playerId);
            else
                mEnvironment->SetBlockNotSent(blockPosition);
            continue;
        }

        // Send as reliable
        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataMapNodeAdd>(position, node));
    }
    mEnvironment->mEnvRecMutex.unlock();
}

void MinecraftLogic::SendMetadataChanged(const std::list<Vector3<short>>& metaUpdates, float farDistNodes)
{
    float maxd = farDistNodes * BS;
    MapNodeMetadataList metaUpdatesList(false);

    mEnvironment->mEnvRecMutex.lock();

    std::vector<ActorId> playerIds = GetPlayerIDs();
    for (ActorId playerId : playerIds)
    {
        LogicActiveObject* player = mEnvironment->GetActiveObject(playerId);
        Vector3<float> playerPos = player ? player->GetBasePosition() : Vector3<float>();

        for (const Vector3<short>& pos : metaUpdates)
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);

            if (!meta)
                continue;

            Vector3<short> blockPosition = GetNodeBlockPosition(pos);
            Vector3<float> blockPos = Vector3<float>{
                pos[0] * BS, pos[1] * BS, pos[2] * BS };

            if (!mEnvironment->IsBlockSent(blockPosition) || (player && Length(playerPos - blockPos) > maxd))
            {
                mEnvironment->SetBlockNotSent(blockPosition);
                continue;
            }

            // Add the change to send list
            metaUpdatesList.Set(pos, meta);
        }
        if (metaUpdatesList.Size() == 0)
            continue;

        // Send the meta changes
        std::ostringstream os(std::ios::binary);
        metaUpdatesList.Serialize(os, false, true);
        std::ostringstream oss(std::ios::binary);
        CompressZlib(os.str(), oss);

        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataHandleNodeMetaChanged>(oss.str()));

        metaUpdatesList.Clear();
    }
    mEnvironment->mEnvRecMutex.unlock();
}

void MinecraftLogic::SendBlocks(float dTime)
{
    MutexAutoLock envlock(mEnvironment->mEnvMutex);
    //TODO check if one big lock could be faster then multiple small ones

    std::vector<PrioritySortedBlockTransfer> queue;

    unsigned int totalSending = 0;
    {
        ScopeProfiler sp2(Profiling, "MinecraftLogic::SendBlocks(): Collect list");

        mEnvironment->mEnvRecMutex.lock();

        std::vector<ActorId> playerIds = GetPlayerIDs();
        for (ActorId playerId : playerIds)
        {
            totalSending += mEnvironment->GetSendingCount();
            mEnvironment->GetNextBlocks(playerId, dTime, queue);
        }

        mEnvironment->mEnvRecMutex.unlock();
    }

    // Sort.
    // Lowest priority number comes first.
    // Lowest is most important.
    std::sort(queue.begin(), queue.end());

    mEnvironment->mEnvRecMutex.lock();

    // Maximal total count calculation
    // The per-visual block sends is halved with the maximal online users
    unsigned int maxBlocksToSend =
        (mEnvironment->GetPlayerCount() + Settings::Get()->GetUInt("max_users")) *
        Settings::Get()->GetUInt("max_simultaneous_block_sends_per_client") / 4 + 1;

    ScopeProfiler sp(Profiling, "MinecraftLogic::SendBlocks(): Send to visuals");
    std::shared_ptr<Map> map = mEnvironment->GetMap();

    uint8_t version = SER_FMT_VER_HIGHEST_READ;
    for (const PrioritySortedBlockTransfer& blockToSend : queue)
    {
        if (totalSending >= maxBlocksToSend)
            break;

        MapBlock* block = map->GetBlockNoCreateNoEx(blockToSend.position);
        if (!block)
            continue;

        mEnvironment->SendBlockNoLock(blockToSend.actorId, block, version);

        mEnvironment->SentBlock(blockToSend.position);
        totalSending++;
    }

    mEnvironment->mEnvRecMutex.unlock();
}

bool MinecraftLogic::AddMediaFile(const std::wstring& fileName, const std::wstring& filePath, 
    const std::wstring& fileRelativePath, std::string* fileData, std::string* digestTo)
{
    // If name contains illegal characters, ignore the file
    if (!StringAllowed(ToString(fileName), TEXTURENAME_ALLOWED_CHARS))
    {
        LogWarning(L"Ignoring illegal file name: \"" + fileName + L"\"");
        return false;
    }
    // If name is not in a supported format, ignore it
    const char* supportedExt[] = {
        ".png", ".jpg", ".bmp", ".tga", ".pcx", 
        ".ppm", ".psd", ".wal", ".rgb", ".ogg", 
        ".x", ".b3d", ".md2", ".dae", ".obj", NULL };
    if (StringRemoveEnd(ToString(fileName), supportedExt).empty())
    {
        LogWarning(L"Ignoring unsupported file extension: \"" + fileName + L"\"");
        return false;
    }
    // Ok, attempt to load the file and add to cache

    // Read data
    BaseReadFile* file = FileSystem::Get()->CreateReadFile(filePath);
    if (!file)
    {
        LogWarning(L"MinecraftHumanView::AddMediaFile(): Failed to open \"" + fileName + L"\" for reading");
        return false;
    }

    if (!file->GetSize())
    {
        delete file;

        LogWarning(L"MinecraftHumanView::AddMediaFile(): Empty file \"" + filePath + L"\"");
        return false;
    }

    char* filedata = new char[file->GetSize() + 1];
    memset(filedata, 0, file->GetSize() + 1);
    file->Read(filedata, file->GetSize());

    SHA1 sha1;
    sha1.AddBytes(filedata, file->GetSize());

    unsigned char* digest = sha1.GetDigest();
    std::string sha1Base64 = Base64Encode(digest, 20);
    std::string sha1Hex = HexEncode((char*)digest, 20);
    if (digestTo)
        *digestTo = std::string((char*)digest, 20);
    free(digest);

    delete file;

    // Put in list
    mMedia[fileName] = MediaInfo(fileRelativePath, sha1Base64);
    //LogInformation(sha1Hex + " is " + ToString(fileName));

    if (fileData)
        *fileData = std::move(filedata);

    return true;
}

void MinecraftLogic::FillMediaCache()
{
    LogInformation("Calculating media file checksums");

    // Collect all media file paths
    std::vector<std::wstring> files;

    // Collect media file information from paths into cache
    const std::set<wchar_t> ignore = { L'.' };
    FileSystem::Get()->GetFileList(files, ToWideString(mGameSpec.mPath) + L"/textures", true);
    for (const std::wstring& file : files)
    {
        std::wstring fileName = FileSystem::Get()->GetFileName(file);
        if (mMedia.find(file) != mMedia.end()) // Do not override
            continue;

        if (ignore.count(fileName[0]))
            continue;

        AddMediaFile(fileName, file, ToWideString(mGameSpec.mRelativePath) + L"/textures/" + fileName);
    }

    FileSystem::Get()->GetFileList(files, ToWideString(mGameSpec.mPath) + L"/sounds", true);
    for (const std::wstring& file : files)
    {
        std::wstring fileName = FileSystem::Get()->GetFileName(file);
        if (mMedia.find(file) != mMedia.end()) // Do not override
            continue;

        if (ignore.count(fileName[0]))
            continue;

        AddMediaFile(fileName, file, ToWideString(mGameSpec.mRelativePath) + L"/sounds/" + fileName);
    }

    FileSystem::Get()->GetFileList(files, ToWideString(mGameSpec.mPath) + L"/models", true);
    for (const std::wstring& file : files)
    {
        std::wstring fileName = FileSystem::Get()->GetFileName(file);
        if (mMedia.find(file) != mMedia.end()) // Do not override
            continue;

        if (ignore.count(fileName[0]))
            continue;

        AddMediaFile(fileName, file, ToWideString(mGameSpec.mRelativePath) + L"/models/" + fileName);
    }

    LogInformation(std::to_string(mMedia.size()) + " media files collected");
}

void MinecraftLogic::SendNodeData()
{
    std::ostringstream os(std::ios::binary);
    mEnvironment->GetNodeManager()->Serialize(os);

    // Send nodes
    EventManager::Get()->TriggerEvent(std::make_shared<EventDataHandleNodes>(os.str()));
}

void MinecraftLogic::SendItemData()
{
    std::ostringstream os(std::ios::binary);
    mEnvironment->GetItemManager()->Serialize(os);

    // Send items
    EventManager::Get()->TriggerEvent(std::make_shared<EventDataHandleItems>(os.str()));
}

void MinecraftLogic::SendMediaData()
{
    std::wstring langSuffix;
    langSuffix.append(L".").append(L".tr");

    std::unordered_map<std::wstring, std::wstring> mediaSent;
    for (const auto& m : mMedia) 
    {
        if (StringEndsWith(m.first, L".tr") && !StringEndsWith(m.first, langSuffix))
            continue;
        mediaSent[m.first] = m.second.path;
    }

    EventManager::Get()->TriggerEvent(std::make_shared<EventDataHandleMedia>(mediaSent));
}

//
// MinecraftLogic::ChangeState
//
void MinecraftLogic::ChangeState(BaseGameState newState)
{
	GameLogic::ChangeState(newState);

	switch (newState)
	{
		case BGS_MAINMENU:
		{
			std::shared_ptr<BaseGameView> menuView(new MinecraftMainMenuView());
            GameApplication::Get()->AddView(menuView);

			break;
		}

		case BGS_WAITINGFORPLAYERS:
		{
			// spawn all local players (should only be one, though we might support more in the future)
			LogAssert(mExpectedPlayers == 1, "needs only one player");
			for (int i = 0; i < mExpectedPlayers; ++i)
			{
				std::shared_ptr<BaseGameView> playersView(new MinecraftHumanView());
                GameApplication::Get()->AddView(playersView);

				if (mIsProxy)
				{
					// if we are a logic player, all we have to do is spawn our view - the logic will do the rest.
					return;
				}
			}
			// spawn all remote player's views on the game
			for (int i = 0; i < mExpectedRemotePlayers; ++i)
			{
				std::shared_ptr<BaseGameView> remoteGameView(new NetworkGameView());
                GameApplication::Get()->AddView(remoteGameView);
			}
			break;
		}

		case BGS_SPAWNINGPLAYERACTORS:
		{
			if (mIsProxy)
			{
				// only the server needs to do this.
				return;
			}
           
			break;
		}
	}
}

void MinecraftLogic::SyncActor(const ActorId id, Transform const &transform)
{
	GameLogic::SyncActor(id, transform);
}

void MinecraftLogic::GameInitDelegate(BaseEventDataPtr pEventData)
{
    mGameInit = true;
}

void MinecraftLogic::GameReadyDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGameReady> pCastEventData =
        std::static_pointer_cast<EventDataGameReady>(pEventData);

    PlayerLAO* playerLAO = NULL;
    std::string playerName = "singleplayer";
    ActorId actorId = pCastEventData->GetId();

    mEnvironment->mEnvRecMutex.lock();
    try
    {
        playerLAO = EmergePlayer(playerName.c_str(), actorId);
        if (playerLAO == NULL)
            LogWarning("init failed id=" + std::to_string(actorId));
    }
    catch (std::exception&)
    {
        mEnvironment->mEnvRecMutex.unlock();
        throw;
    }
    mEnvironment->mEnvRecMutex.unlock();

    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(playerName.c_str());

    // If failed, cancel
    if (!playerLAO || !player)
    {
        if (player && player->GetId() != INVALID_ACTOR_ID)
        {
            LogInformation("Failed to emerge player \"" + playerName +
                "\" (player allocated to an another visual)");
            /*
            DenyAccessLegacy(actorId, L"Another visual is connected with this "
                L"name. If your visual closed unexpectedly, try again in a minute.");
            */
        }
        else
        {
            LogWarning(": Failed to emerge player");
            //DenyAccessLegacy(actorId, L"Could not allocate player.");
        }
    }
    else
    {
        /*
            Send complete position information
        */
        mEnvironment->SendPlayerMove(playerLAO);

        // Send privileges
        SendPlayerPrivileges(actorId);

        // Send inventory form
        SendPlayerInventoryForm(actorId);

        // Send inventory
        mEnvironment->SendInventory(playerLAO, false);

        // Send HP or death screen
        if (playerLAO->IsDead())
            mEnvironment->SendDeathScreen(actorId, false, Vector3<float>::Zero());
        else
            mEnvironment->SendPlayerHPOrDie(playerLAO, PlayerHPChangeReason(PlayerHPChangeReason::SET_HP));

        // Send Breath
        mEnvironment->SendPlayerBreath(playerLAO);

        /*
            Print out action
        */
        LogInformation(std::string(player->GetName()) + " joins game. ");
    }

    mEnvironment->AddPlayerName(playerLAO->GetPlayer()->GetName());
    //mEnvironment->RemovePlayerName(name);
    BaseGame::Get()->OnJoinPlayer(playerLAO);

    // Send shutdown timer if shutdown has been scheduled
    /*
    if (mShutdownState.IsTimerRunning())
    {
        SendChatMessage(mShutdownState.GetShutdownTimerMessage());
    }
    */
}

void MinecraftLogic::RequestStartGameDelegate(BaseEventDataPtr pEventData)
{
	ChangeState(BGS_WAITINGFORPLAYERS);
}

void MinecraftLogic::EnvironmentLoadedDelegate(BaseEventDataPtr pEventData)
{
	++mHumanGamesLoaded;
}

// FUTURE WORK - this isn't Minecraft specific so it can go into the game agnostic base class
void MinecraftLogic::RemoteClientDelegate(BaseEventDataPtr pEventData)
{
	// This event is always sent from clients to the game server.
	std::shared_ptr<EventDataRemoteClient> pCastEventData = 
		std::static_pointer_cast<EventDataRemoteClient>(pEventData);
	const int sockID = pCastEventData->GetSocketId();
	const int ipAddress = pCastEventData->GetIpAddress();

	// go find a NetworkGameView that doesn't have a socket ID, and attach this client to that view.
	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<BaseGameView> pView = *it;
		if (pView->GetType() == GV_REMOTE)
		{
			std::shared_ptr<NetworkGameView> pNetworkGameView = 
				std::static_pointer_cast<NetworkGameView, BaseGameView>(pView);
			if (!pNetworkGameView->HasRemotePlayerAttached())
			{
				pNetworkGameView->AttachRemotePlayer(sockID);
				CreateNetworkEventForwarder(sockID);
				mHumanPlayersAttached++;

				return;
			}
		}
	}
}

void MinecraftLogic::NetworkPlayerActorAssignmentDelegate(BaseEventDataPtr pEventData)
{
	if (!mIsProxy)
		return;

	// we're a remote client getting an actor assignment.
	// the server assigned us a playerId when we first attached (the server's socketId, actually)
	std::shared_ptr<EventDataNetworkPlayerActorAssignment> pCastEventData =
		std::static_pointer_cast<EventDataNetworkPlayerActorAssignment>(pEventData);
	if (pCastEventData->GetActorId() == INVALID_ACTOR_ID)
	{
		mRemotePlayerId = pCastEventData->GetSocketId();
		return;
	}

	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<BaseGameView> pView = *it;
		if (pView->GetType() == GV_HUMAN)
		{
			std::shared_ptr<MinecraftHumanView> pHumanView =
				std::static_pointer_cast<MinecraftHumanView, BaseGameView>(pView);
			if (mRemotePlayerId == pCastEventData->GetSocketId())
				pHumanView->SetControlledActor(pCastEventData->GetActorId());

			return;
		}
	}

	LogError("Could not find HumanView to attach actor to!");
}


void MinecraftLogic::HandleNodeMetaFieldsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleNodeMetaFields> pCastEventData =
        std::static_pointer_cast<EventDataHandleNodeMetaFields>(pEventData);


    BaseGame::Get()->OnRecieveFieldsNode(
        pCastEventData->GetPosition(), pCastEventData->GetForm(), pCastEventData->GetFields(), nullptr);
}

void MinecraftLogic::HandleInventoryFieldsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleInventoryFields> pCastEventData =
        std::static_pointer_cast<EventDataHandleInventoryFields>(pEventData);

    std::vector<ActorId> playerIds = GetPlayerIDs();
    for (ActorId playerId : playerIds)
    {
        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(playerId);
        if (player == NULL)
        {
            LogError("Canceling: No player for actorId=" +
                std::to_string(playerId) + " disconnecting peer!");
            return;
        }

        PlayerLAO* playerLAO = player->GetPlayerLAO();
        if (playerLAO == NULL)
        {
            LogError("Canceling: No player object for actorId=" +
                std::to_string(playerId) + " disconnecting peer!");
            return;
        }

        if (pCastEventData->GetForm().empty())
        {
            // pass through inventory submits
            mGame->OnRecieveFieldsPlayer(playerLAO, pCastEventData->GetForm(), pCastEventData->GetFields());
            return;
        }

        // verify that we displayed the form to the user
        const auto actorStateIterator = mEnvironment->mFormStateData.find(playerId);
        if (actorStateIterator != mEnvironment->mFormStateData.end())
        {
            const std::string& formName = actorStateIterator->second;
            if (pCastEventData->GetForm() == formName)
            {
                auto it = pCastEventData->GetFields().find("quit");
                if (it != pCastEventData->GetFields().end() && it->second == "true")
                    mEnvironment->mFormStateData.erase(actorStateIterator);

                mGame->OnRecieveFieldsPlayer(playerLAO, pCastEventData->GetForm(), pCastEventData->GetFields());
                return;
            }
            LogInformation("'" + std::string(player->GetName()) +
                "' submitted form ('" + pCastEventData->GetForm() +
                "') but the name of the form doesn't match the"
                " expected name ('" + formName + "')");

        }
        else
        {
            LogInformation("'" + std::string(player->GetName()) +
                "' submitted form ('" + pCastEventData->GetForm() +
                "') but logic hasn't sent form to visual");
        }
        LogInformation(", possible exploitation attempt");
    }
}


void MinecraftLogic::HandleInventoryActionDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleInventoryAction> pCastEventData =
        std::static_pointer_cast<EventDataHandleInventoryAction>(pEventData);

    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(pCastEventData->GetId());
    if (player == NULL)
        return;

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (playerLAO == NULL)
        return;

    // Strip command and create a stream
    std::istringstream is(pCastEventData->GetAction(), std::ios_base::binary);
    // Create an action
    std::unique_ptr<InventoryAction> action(InventoryAction::Deserialize(is));
    if (!action) 
    {
        LogInformation("InventoryAction::deSerialize() returned NULL");
        return;
    }

    /*
        Note: Always set inventory not sent, to repair cases
        where the visual made a bad prediction.
    */
    const bool playerHasInteract = true;// CheckPrivilege(player->GetName(), "interact");
    auto CheckInventoryAccess = [player, playerHasInteract, this](const InventoryLocation &loc) -> bool
    {
        // Players without interact may modify their own inventory
        if (!playerHasInteract && loc.type != InventoryLocation::PLAYER)
        {
            LogInformation("Cannot modify foreign inventory: No interact privilege");
            return false;
        }

        switch (loc.type)
        {
            case InventoryLocation::CURRENT_PLAYER:
                // Only used internally on the visual, never sent
                return false;
            case InventoryLocation::PLAYER:
                // Allow access to own inventory in all cases
                return loc.name == player->GetName();
            case InventoryLocation::NODEMETA:
            {
                // Check for out-of-range interaction
                Vector3<float> nodePos = Vector3<float>{
                    loc.nodePosition[0] * BS, loc.nodePosition[1] * BS, loc.nodePosition[2] * BS };
                Vector3<float> playerPos = player->GetPlayerLAO()->GetEyePosition();
                float distance = Length(playerPos - nodePos);
                return CheckInteractDistance(player.get(), distance, "inventory");
            }
            case InventoryLocation::DETACHED:
                return mEnvironment->GetInventoryManager()->CheckDetachedInventoryAccess(loc, player->GetName());
            default:
                return false;
        }
    };

    /*
        Handle restrictions and special cases of the move action
    */
    if (action->GetType() == Action::Move)
    {
        BaseMoveAction* moveAction = (BaseMoveAction*)action.get();
        moveAction->fromInventory.ApplyCurrentPlayer(player->GetName());
        moveAction->toInventory.ApplyCurrentPlayer(player->GetName());

        mEnvironment->GetInventoryManager()->SetInventoryModified(moveAction->fromInventory);
        if (moveAction->fromInventory != moveAction->toInventory)
            mEnvironment->GetInventoryManager()->SetInventoryModified(moveAction->toInventory);

        if (!CheckInventoryAccess(moveAction->fromInventory) ||
            !CheckInventoryAccess(moveAction->toInventory))
            return;

        /*
            Disable moving items out of craftpreview
        */
        if (moveAction->fromList == "craftpreview")
        {
            LogInformation("Ignoring BaseMoveAction from " +
                moveAction->fromInventory.Dump() + ":" + moveAction->fromList +
                " to " + moveAction->toInventory.Dump() + ":" + moveAction->toList +
                " because src is " + moveAction->fromList);
            return;
        }

        /*
            Disable moving items into craftresult and craftpreview
        */
        if (moveAction->toList == "craftpreview" || moveAction->toList == "craftresult")
        {
            LogInformation("Ignoring BaseMoveAction from " +
                moveAction->fromInventory.Dump() + ":" + moveAction->fromList +
                " to " + moveAction->toInventory.Dump() + ":" + moveAction->toList +
                " because dst is " + moveAction->toList);
            return;
        }
    }
    /*
        Handle restrictions and special cases of the drop action
    */
    else if (action->GetType() == Action::Drop)
    {
        BaseDropAction* dropAction = (BaseDropAction*)action.get();
        dropAction->fromInventory.ApplyCurrentPlayer(player->GetName());
        mEnvironment->GetInventoryManager()->SetInventoryModified(dropAction->fromInventory);

        /*
            Disable dropping items out of craftpreview
        */
        if (dropAction->fromList == "craftpreview")
        {
            LogInformation("Ignoring BaseDropAction from " + dropAction->fromInventory.Dump() +
                ":" + dropAction->fromList + " because src is " + dropAction->fromList);
            return;
        }

        // Disallow dropping items if not allowed to interact
        if (!playerHasInteract || !CheckInventoryAccess(dropAction->fromInventory))
            return;

        // Disallow dropping items if dead
        if (playerLAO->IsDead())
        {
            LogInformation("Ignoring BaseDropAction from " + dropAction->fromInventory.Dump() +
                ":" + dropAction->fromList + " because player is dead.");
            return;
        }
    }
    /*
        Handle restrictions and special cases of the craft action
    */
    else if (action->GetType() == Action::Craft)
    {
        BaseCraftAction* craftAction = (BaseCraftAction*)action.get();
        craftAction->craftInventory.ApplyCurrentPlayer(player->GetName());
        mEnvironment->GetInventoryManager()->SetInventoryModified(craftAction->craftInventory);

        // Disallow crafting if not allowed to interact
        if (!playerHasInteract)
        {
            LogInformation("Cannot craft: No interact privilege");
            return;
        }

        if (!CheckInventoryAccess(craftAction->craftInventory))
            return;
    }
    else
    {
        // Unknown action. Ignored.
        return;
    }

    // Do the action
    action->Apply(mEnvironment->GetInventoryManager(), playerLAO, mEnvironment.get());
}

void MinecraftLogic::HandleChatMessageDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataChatMessage> pCastEventData =
        std::static_pointer_cast<EventDataChatMessage>(pEventData);

    // Discard empty line
    if (pCastEventData->GetResource().empty())
        return;

    SendChatMessage(pCastEventData->GetResource());
}

void MinecraftLogic::HandleNotifyPlayerDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataNotifyActor> pCastEventData =
        std::static_pointer_cast<EventDataNotifyActor>(pEventData);

    auto player = mEnvironment->GetPlayer(pCastEventData->GetActorId());
    NotifyPlayer(player->GetName(), pCastEventData->GetNote());
}

void MinecraftLogic::HandleInteractDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataInteract> pCastEventData =
        std::static_pointer_cast<EventDataInteract>(pEventData);

    /*
        [0] unsigned short command
        [2] unsigned char action
        [3] unsigned short item
        [5] unsigned int length of the next item (plen)
        [9] serialized PointedThing
        [9 + plen] player position information
    */
    InteractAction action = (InteractAction)pCastEventData->GetAction();
    unsigned short itemInteraction = pCastEventData->GetWieldIndex();

    std::istringstream tmpIs(pCastEventData->GetPointedThing(), std::ios::binary);
    PointedThing pointed;
    pointed.Deserialize(tmpIs);

    LogInformation("action=" + std::to_string((int)action) + ", item=" +
        std::to_string(itemInteraction) + ", pointed=" + pointed.Dump());

    ActorId actorId = pCastEventData->GetId();
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
    if (player == NULL)
    {
        LogError("Canceling: No player for actorId=" +
            std::to_string(actorId) + " disconnecting peer!");
        return;
    }

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (playerLAO == NULL)
    {
        LogError("Canceling: No player object for actorId=" +
            std::to_string(actorId) + " disconnecting peer!");
        return;
    }

    if (playerLAO->IsDead())
    {
        LogInformation(std::string(player->GetName()) + " tried to interact while dead; ignoring.");
        if (pointed.type == POINTEDTHING_NODE)
        {
            // Re-send block to revert change on visual-side
            //RemoteClient* client = GetClient(actorId);
            Vector3<short> blockPos = GetNodeBlockPosition(pointed.nodeUndersurface);
            mEnvironment->SetBlockNotSent(blockPos);
        }
        // Call callbacks
        BaseGame::Get()->OnCheatPlayer(playerLAO, "interacted_while_dead");
        return;
    }

    Vector3<int> pos = pCastEventData->GetPosition();
    Vector3<int> sp = pCastEventData->GetSpeed();
    float pitch = (float)pCastEventData->GetPitch() / 100.0f;
    float yaw = (float)pCastEventData->GetYaw() / 100.0f;
    unsigned int keyPressed = pCastEventData->GetKeyPressed();

    // default behavior (in case an old visual doesn't send these)
    float fov = (float)pCastEventData->GetFov() / 80.f;
    uint8_t wantedRange = pCastEventData->GetWantedRange();

    Vector3<float> position{ (float)pos[0] / 100.0f, (float)pos[1] / 100.0f, (float)pos[2] / 100.0f };
    Vector3<float> speed{ (float)sp[0] / 100.0f, (float)sp[1] / 100.0f, (float)sp[2] / 100.0f };

    pitch = Modulo360(pitch);
    yaw = WrapDegrees360(yaw);

    if (!playerLAO->IsAttached())
    {
        // Only update player positions when moving freely
        // to not interfere with attachment handling
        playerLAO->SetBasePosition(position);
        player->SetSpeed(speed);
    }
    playerLAO->SetLookPitch(pitch);
    playerLAO->SetPlayerYaw(yaw);
    playerLAO->SetFov(fov);
    playerLAO->SetWantedRange(wantedRange);

    player->mKeyPressed = keyPressed;
    player->mControl.up = (keyPressed & (0x1 << 0));
    player->mControl.down = (keyPressed & (0x1 << 1));
    player->mControl.left = (keyPressed & (0x1 << 2));
    player->mControl.right = (keyPressed & (0x1 << 3));
    player->mControl.jump = (keyPressed & (0x1 << 4));
    player->mControl.aux1 = (keyPressed & (0x1 << 5));
    player->mControl.sneak = (keyPressed & (0x1 << 6));
    player->mControl.dig = (keyPressed & (0x1 << 7));
    player->mControl.place = (keyPressed & (0x1 << 8));
    player->mControl.zoom = (keyPressed & (0x1 << 9));

    if (playerLAO->CheckMovementCheat())
    {
        // Call callbacks
        BaseGame::Get()->OnCheatPlayer(playerLAO, "moved_too_fast");

        mEnvironment->SendPlayerMove(playerLAO);
    }

    // Update wielded item
    Vector3<float> playerPos = playerLAO->GetLastGoodPosition();
    if (itemInteraction >= player->GetHotbarItemCount())
    {
        LogInformation("Player: " + std::string(player->GetName()) +
            " tried to access item=" + std::to_string(itemInteraction) +
            " out of hotbar_itemcount=" + std::to_string(player->GetHotbarItemCount()) +
            "; ignoring.");
        return;
    }

    playerLAO->GetPlayer()->SetWieldIndex(itemInteraction);

    // Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
    LogicActiveObject* pointedObject = NULL;
    if (pointed.type == POINTEDTHING_OBJECT)
    {
        pointedObject = mEnvironment->GetActiveObject(pointed.objectId);
        if (pointedObject == NULL)
        {
            LogError("Pointed object is NULL");
            return;
        }
    }

    /*
        Make sure the player is allowed to do it
    */
    const bool playerHasInteract = true;// CheckPrivilege(player->GetName(), "interact");
    if (!playerHasInteract)
    {
        LogInformation(std::string(player->GetName()) +
            " attempted to interact with " + pointed.Dump() +
            " without 'interact' privilege");
        if (pointed.type != POINTEDTHING_NODE)
            return;

        // Re-send block to revert change on visual-side
        //RemoteClient* client = GetClient(actorId);
        // Digging completed -> under
        if (action == INTERACT_DIGGING_COMPLETED)
        {
            Vector3<short> blockPos = GetNodeBlockPosition(pointed.nodeUndersurface);
            mEnvironment->SetBlockNotSent(blockPos);
        }
        // Placement -> above
        else if (action == INTERACT_PLACE)
        {
            Vector3<short> blockPos = GetNodeBlockPosition(pointed.nodeAbovesurface);
            mEnvironment->SetBlockNotSent(blockPos);
        }
        return;
    }

    /*
        Check that target is reasonably close
    */
    static thread_local const bool enableAnticheat = !Settings::Get()->GetBool("disable_anticheat");

    if ((action == INTERACT_START_DIGGING || action == INTERACT_DIGGING_COMPLETED ||
        action == INTERACT_PLACE || action == INTERACT_USE) && enableAnticheat &&
        !mEnvironment->IsSingleplayer())
    {
        Vector3<float> targetPos = playerPos;
        if (pointed.type == POINTEDTHING_NODE)
        {
            targetPos = Vector3<float>{
                pointed.nodeUndersurface[0] * BS,
                pointed.nodeUndersurface[1] * BS,
                pointed.nodeUndersurface[2] * BS };
        }
        else if (pointed.type == POINTEDTHING_OBJECT)
        {
            if (playerLAO->GetId() == pointedObject->GetId())
            {
                LogInformation(std::string(player->GetName()) + " attempted to interact with themselves");
                BaseGame::Get()->OnCheatPlayer(playerLAO, "interacted_with_self");
                return;
            }
            targetPos = pointedObject->GetBasePosition();
        }
        float distance = Length(targetPos - playerLAO->GetEyePosition());

        if (!CheckInteractDistance(player.get(), distance, pointed.Dump()))
        {
            if (pointed.type == POINTEDTHING_NODE)
            {
                // Re-send block to revert change on visual-side
                Vector3<short> blockPos = GetNodeBlockPosition(pointed.nodeUndersurface);
                mEnvironment->SetBlockNotSent(blockPos);
            }
            return;
        }
    }

    switch (action)
    {
        // Start digging or punch object
        case INTERACT_START_DIGGING:
        {
            if (pointed.type == POINTEDTHING_NODE)
            {
                MapNode node(CONTENT_IGNORE);
                bool posOk;

                Vector3<short> nodeUnder = pointed.nodeUndersurface;
                node = mEnvironment->GetMap()->GetNode(nodeUnder, &posOk);
                if (!posOk)
                {
                    LogInformation("Not punching: Node not found. Adding block to emerge queue.");
                    EmergeManager::Get()->EnqueueBlockEmerge(actorId,
                        GetNodeBlockPosition(pointed.nodeUndersurface), false);
                }

                if (node.GetContent() != CONTENT_IGNORE)
                    BaseGame::Get()->OnPunch(nodeUnder, node);

                // Cheat prevention
                playerLAO->NoCheatDigStart(nodeUnder);

                return;
            }

            // Skip if the object can't be interacted with anymore
            if (pointed.type != POINTEDTHING_OBJECT || pointedObject->IsGone())
                return;

            ItemStack selectedItem, handItem;
            ItemStack toolItem = playerLAO->GetWieldedItem(&selectedItem, &handItem);
            ToolCapabilities toolcap = toolItem.GetToolCapabilities(mEnvironment->GetItemManager());

            Vector3<float> dir = pointedObject->GetBasePosition() -
                (playerLAO->GetBasePosition() + playerLAO->GetEyeOffset());
            Normalize(dir);

            float timeFromLastPunch = playerLAO->ResetTimeFromLastPunch();
            unsigned short srcOriginalHP = pointedObject->GetHP();
            unsigned short dstOriginHP = playerLAO->GetHP();

            unsigned short wear = pointedObject->Punch(dir, &toolcap, playerLAO, timeFromLastPunch);

            // Callback may have changed item, so get it again
            playerLAO->GetWieldedItem(&selectedItem);
            bool changed = selectedItem.AddWear(wear, mEnvironment->GetItemManager());
            if (changed)
                playerLAO->SetWieldedItem(selectedItem);

            // If the object is a player and its HP changed
            if (srcOriginalHP != pointedObject->GetHP() &&
                pointedObject->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
            {
                mEnvironment->SendPlayerHPOrDie((PlayerLAO *)pointedObject,
                    PlayerHPChangeReason(PlayerHPChangeReason::PLAYER_PUNCH, playerLAO));
            }

            // If the puncher is a player and its HP changed
            if (dstOriginHP != playerLAO->GetHP())
            {
                mEnvironment->SendPlayerHPOrDie(playerLAO,
                    PlayerHPChangeReason(PlayerHPChangeReason::PLAYER_PUNCH, pointedObject));
            }

            return;
        } // action == INTERACT_START_DIGGING

        case INTERACT_STOP_DIGGING:
            // Nothing to do
            return;

        case INTERACT_DIGGING_COMPLETED:
        {
            // Only digging of nodes
            if (pointed.type != POINTEDTHING_NODE)
                return;
            bool posOk;
            Vector3<short> nodeUnder = pointed.nodeUndersurface;
            MapNode node = mEnvironment->GetMap()->GetNode(nodeUnder, &posOk);
            if (!posOk)
            {
                LogInformation("Not finishing digging: Node not found. Adding block to emerge queue.");
                EmergeManager::Get()->EnqueueBlockEmerge(actorId,
                    GetNodeBlockPosition(pointed.nodeAbovesurface), false);
            }

            /* Cheat prevention */
            bool isValidDig = true;
            if (enableAnticheat && !mEnvironment->IsSingleplayer())
            {
                Vector3<short> nocheatPos = playerLAO->GetNoCheatDigPosition();
                float nocheatTime = playerLAO->GetNoCheatDigTime();
                playerLAO->NoCheatDigEnd();
                // If player didn't start digging this, ignore dig
                if (nocheatPos != nodeUnder)
                {
                    LogInformation(
                        std::string(player->GetName()) + " started digging (" +
                        std::to_string(nocheatPos[0]) + "," + std::to_string(nocheatPos[1]) + "," +
                        std::to_string(nocheatPos[2]) + ") and completed digging (" + std::to_string(nodeUnder[0]) +
                        "," + std::to_string(nodeUnder[1]) + "," + std::to_string(nodeUnder[2]) + "); not digging.");
                    isValidDig = false;
                    // Call callbacks
                    BaseGame::Get()->OnCheatPlayer(playerLAO, "finished_unknown_dig");
                }

                // Get player's wielded item
                ItemStack selectedItem, handItem;
                playerLAO->GetPlayer()->GetWieldedItem(&selectedItem, &handItem);

                // Get diggability and expected digging time
                DigParams params = GetDigParams(mEnvironment->GetNodeManager()->Get(node).groups,
                    &selectedItem.GetToolCapabilities(mEnvironment->GetItemManager()));
                // If can't dig, try hand
                if (!params.diggable)
                {
                    params = GetDigParams(mEnvironment->GetNodeManager()->Get(node).groups,
                        &handItem.GetToolCapabilities(mEnvironment->GetItemManager()));
                }
                // If can't dig, ignore dig
                if (!params.diggable)
                {
                    LogInformation(
                        std::string(player->GetName()) + " completed digging (" +
                        std::to_string(nodeUnder[0]) + "," + std::to_string(nodeUnder[1]) + "," +
                        std::to_string(nodeUnder[2]) + "), which is not diggable with tool; not digging.");
                    isValidDig = false;
                    // Call callbacks
                    BaseGame::Get()->OnCheatPlayer(playerLAO, "dug_unbreakable");
                }
                // Check digging time
                // If already invalidated, we don't have to
                if (!isValidDig)
                {
                    // Well not our problem then
                }
                // Clean and long dig
                else if (params.time > 2.0 && nocheatTime * 1.2 > params.time)
                {
                    // All is good, but grab time from pool; don't care if
                    // it's actually available
                    playerLAO->GetDigPool().Grab(params.time);
                }
                // Short or laggy dig
                // Try getting the time from pool
                else if (playerLAO->GetDigPool().Grab(params.time))
                {
                    // All is good
                }
                // Dig not possible
                else
                {
                    LogInformation(
                        std::string(player->GetName()) + " completed digging (" +
                        std::to_string(nodeUnder[0]) + "," + std::to_string(nodeUnder[1]) + "," +
                        std::to_string(nodeUnder[2]) + "), too fast; not digging.");
                    isValidDig = false;

                    // Call callbacks
                    BaseGame::Get()->OnCheatPlayer(playerLAO, "dug_too_fast");
                }
            }

            /* Actually dig node */
            if (isValidDig && node.GetContent() != CONTENT_IGNORE)
                BaseGame::Get()->OnDigNode(nodeUnder, node, playerLAO);

            Vector3<short> blockPos = GetNodeBlockPosition(nodeUnder);

            // Send unusual result (that is, node not being removed)
            if (mEnvironment->GetMap()->GetNode(nodeUnder).GetContent() != CONTENT_AIR)
            {
                // Re-send block to revert change on visual-side
                mEnvironment->SetBlockNotSent(blockPos);
            }
            else mEnvironment->ResendBlockIfOnWire(blockPos);

            return;
        } // action == INTERACT_DIGGING_COMPLETED

        // Place block or right-click object
        case INTERACT_PLACE:
        {
            ItemStack selectedItem;
            playerLAO->GetWieldedItem(&selectedItem, nullptr);

            // Reset build time counter
            if (pointed.type == POINTEDTHING_NODE &&
                selectedItem.GetDefinition(mEnvironment->GetItemManager()).type == ITEM_NODE)
                mEnvironment->mTimeFromBuilding = 0.0;

            if (pointed.type == POINTEDTHING_OBJECT)
            {
                // Right click object

                // Skip if object can't be interacted with anymore
                if (pointedObject->IsGone())
                    return;

                LogInformation(std::string(player->GetName()) + " right-clicks object " +
                    std::to_string(pointed.objectId) + ": " + pointedObject->GetDescription());

                // Do stuff
                if (BaseGame::Get()->OnSecondaryUseItem(selectedItem, playerLAO, pointed))
                    if (playerLAO->SetWieldedItem(selectedItem))
                        mEnvironment->SendInventory(playerLAO, true);

                pointedObject->RightClick(playerLAO);
            }
            else if (BaseGame::Get()->OnPlaceItem(selectedItem, playerLAO, pointed))
            {
                // Apply returned ItemStack
                if (playerLAO->SetWieldedItem(selectedItem))
                    mEnvironment->SendInventory(playerLAO, true);
            }

            if (pointed.type != POINTEDTHING_NODE)
                return;

            // If item has node placement prediction, always send the
            // blocks to make sure the visual knows what exactly happened
            Vector3<short> blockPos = GetNodeBlockPosition(pointed.nodeAbovesurface);
            Vector3<short> blockPos2 = GetNodeBlockPosition(pointed.nodeUndersurface);
            if (!selectedItem.GetDefinition(mEnvironment->GetItemManager()).nodePlacementPrediction.empty())
            {
                mEnvironment->SetBlockNotSent(blockPos);
                if (blockPos2 != blockPos)
                    mEnvironment->SetBlockNotSent(blockPos2);
            }
            else
            {
                mEnvironment->ResendBlockIfOnWire(blockPos);
                if (blockPos2 != blockPos)
                    mEnvironment->ResendBlockIfOnWire(blockPos2);
            }

            return;
        } // action == INTERACT_PLACE

        case INTERACT_USE:
        {
            ItemStack selectedItem;
            playerLAO->GetWieldedItem(&selectedItem, nullptr);

            LogInformation(std::string(player->GetName()) + " uses " +
                selectedItem.name + ", pointing at " + pointed.Dump());

            if (BaseGame::Get()->OnUseItem(selectedItem, playerLAO, pointed))
            {
                // Apply returned ItemStack
                if (playerLAO->SetWieldedItem(selectedItem))
                    mEnvironment->SendInventory(playerLAO, true);
            }

            return;
        }

        // Rightclick air
        case INTERACT_ACTIVATE:
        {
            ItemStack selectedItem;
            playerLAO->GetWieldedItem(&selectedItem, nullptr);

            LogInformation(std::string(player->GetName()) + " activates " + selectedItem.name);
            pointed.type = POINTEDTHING_NOTHING; // can only ever be NOTHING

            if (BaseGame::Get()->OnSecondaryUseItem(selectedItem, playerLAO, pointed))
                if (playerLAO->SetWieldedItem(selectedItem))
                    mEnvironment->SendInventory(playerLAO, true);

            return;
        }

        default:
            LogWarning("Invalid action " + std::to_string(action));

    }
}

void MinecraftLogic::SaveBlockDataDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataSaveBlockData> pCastEventData =
        std::static_pointer_cast<EventDataSaveBlockData>(pEventData);

    if (mEnvironment->mLocalDB)
        LogicMap::SaveBlock(pCastEventData->GetBlock(), mEnvironment->mLocalDB);
}

void MinecraftLogic::DeletedBlocksDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataDeletedBlocks> pCastEventData =
        std::static_pointer_cast<EventDataDeletedBlocks>(pEventData);

    /*
        [0] uint16_t command
        [2] uint8_t count
        [3] Vector3<short> pos_0
        [3+6] Vector3<short> pos_1
    */
    for (auto block : pCastEventData->GetBlocks())
        mEnvironment->SetBlockNotSent(block);
}

void MinecraftLogic::GotBlocksDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGotBlocks> pCastEventData =
        std::static_pointer_cast<EventDataGotBlocks>(pEventData);

    /*
        [0] unsigned short command
        [2] unsigned char count
        [3] Vector3<short> pos_0
        [3+6] Vector3<short> pos_1
        ...
    */

    mEnvironment->mEnvRecMutex.lock();

    for (auto block : pCastEventData->GetBlocks())
        mEnvironment->GotBlock(block);

    mEnvironment->mEnvRecMutex.unlock();
}

void MinecraftLogic::HandleRemoveSoundDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataRemoveSounds> pCastEventData =
        std::static_pointer_cast<EventDataRemoveSounds>(pEventData);

    mEnvironment->RemoveSounds(pCastEventData->GetIds());
}

void MinecraftLogic::PlayerItemDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerItem> pCastEventData =
        std::static_pointer_cast<EventDataPlayerItem>(pEventData);

    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(pCastEventData->GetId());
    if (player == NULL)
        return;

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (playerLAO == NULL)
        return;

    if (pCastEventData->GetItem() >= player->GetHotbarItemCount())
    {
        LogInformation("Player: " +
            std::string(player->GetName()) + " tried to access item= " +
            std::to_string(pCastEventData->GetItem()) + " out of hotbaritemcount= " +
            std::to_string(player->GetHotbarItemCount()) + "; ignoring.");
        return;
    }

    playerLAO->GetPlayer()->SetWieldIndex(pCastEventData->GetItem());
}

void MinecraftLogic::PlayerPositionDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerPosition> pCastEventData =
        std::static_pointer_cast<EventDataPlayerPosition>(pEventData);

    ActorId actorId = pCastEventData->GetId();
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
    if (player == NULL)
    {
        LogError("Canceling: No player for actorId=" +
            std::to_string(actorId) + " disconnecting peer!");
        return;
    }

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (playerLAO == NULL)
    {
        LogError("Canceling: No player object for actorId=" +
            std::to_string(actorId) + " disconnecting peer!");
        return;
    }

    // If player is dead we don't care of this packet
    if (playerLAO->IsDead())
    {
        LogWarning(std::string(player->GetName()) + " is dead. Ignoring packet");
        return;
    }

    Vector3<int> ps = pCastEventData->GetPosition();
    Vector3<int> ss = pCastEventData->GetSpeed();
    float pitch = (float)pCastEventData->GetPitch() / 100.0f;
    float yaw = (float)pCastEventData->GetYaw() / 100.0f;
    unsigned int keyPressed = pCastEventData->GetKeyPressed();

    // default behavior (in case an old visual doesn't send these)
    float fov = (float)pCastEventData->GetFov() / 80.f;
    uint8_t wantedRange = pCastEventData->GetWantedRange();

    Vector3<float> position{ (float)ps[0] / 100.0f, (float)ps[1] / 100.0f, (float)ps[2] / 100.0f };
    Vector3<float> speed{ (float)ss[0] / 100.0f, (float)ss[1] / 100.0f, (float)ss[2] / 100.0f };

    pitch = Modulo360(pitch);
    yaw = WrapDegrees360(yaw);

    if (!playerLAO->IsAttached())
    {
        // Only update player positions when moving freely
        // to not interfere with attachment handling
        playerLAO->SetBasePosition(position);
        player->SetSpeed(speed);
    }
    playerLAO->SetLookPitch(pitch);
    playerLAO->SetPlayerYaw(yaw);
    playerLAO->SetFov(fov);
    playerLAO->SetWantedRange(wantedRange);

    player->mKeyPressed = keyPressed;
    player->mControl.up = (keyPressed & (0x1 << 0));
    player->mControl.down = (keyPressed & (0x1 << 1));
    player->mControl.left = (keyPressed & (0x1 << 2));
    player->mControl.right = (keyPressed & (0x1 << 3));
    player->mControl.jump = (keyPressed & (0x1 << 4));
    player->mControl.aux1 = (keyPressed & (0x1 << 5));
    player->mControl.sneak = (keyPressed & (0x1 << 6));
    player->mControl.dig = (keyPressed & (0x1 << 7));
    player->mControl.place = (keyPressed & (0x1 << 8));
    player->mControl.zoom = (keyPressed & (0x1 << 9));

    if (playerLAO->CheckMovementCheat())
    {
        // Call callbacks
        BaseGame::Get()->OnCheatPlayer(playerLAO, "moved_too_fast");

        mEnvironment->SendPlayerMove(playerLAO);
    }
}

void MinecraftLogic::PlayerDamageDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerDamage> pCastEventData =
        std::static_pointer_cast<EventDataPlayerDamage>(pEventData);

    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(pCastEventData->GetId());
    if (player == NULL)
    {
        LogError("Canceling: No player for actorId=" +
            std::to_string(pCastEventData->GetId()) + " disconnecting peer!");
        return;
    }

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    if (playerLAO == NULL)
    {
        LogError("Canceling: No player object for actorId=" +
            std::to_string(pCastEventData->GetId()) + " disconnecting peer!");
        return;
    }

    if (!playerLAO->IsImmortal())
    {
        if (playerLAO->IsDead())
        {
            LogInformation("Ignoring damage as player " + 
                std::string(player->GetName()) + " is already dead.");
            return;
        }

        Vector3<float> pos = playerLAO->GetBasePosition() / BS;
        LogInformation(std::string(player->GetName()) + " damaged by " +
            std::to_string((int)pCastEventData->GetDamage()) + " hp at (" +
            std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]) + ")");

        PlayerHPChangeReason reason(PlayerHPChangeReason::FALL);
        playerLAO->SetHP((int)playerLAO->GetHP() - (int)pCastEventData->GetDamage(), reason);
        mEnvironment->SendPlayerHPOrDie(playerLAO, reason);
    }
}

void MinecraftLogic::PlayerRespawnDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerRespawn> pCastEventData =
        std::static_pointer_cast<EventDataPlayerRespawn>(pEventData);

    ActorId actorId = pCastEventData->GetId();
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(actorId);
    if (player == NULL)
    {
        LogWarning("Canceling: No player for id =" + std::to_string(actorId));
        return;
    }

    PlayerLAO* playerLAO = player->GetPlayerLAO();
    LogAssert(playerLAO, "invalid player");

    if (!playerLAO->IsDead())
        return;

    if (!pCastEventData->GetString().empty())
        PushToChatQueue(new ChatMessage(ToWideString(pCastEventData->GetString())));

    RespawnPlayer(actorId);

    Vector3<float> respawnPos = playerLAO->GetBasePosition() / BS;
    LogInformation(std::string(player->GetName()) +
        " respawns at (" + std::to_string(respawnPos[0]) +
        "," + std::to_string(respawnPos[1]) +
        "," + std::to_string(respawnPos[2]) + ")");


    // ActiveObject is added to environment in AsyncRunStep after
    // the previous addition has been successfully removed
}

void MinecraftLogic::PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerEnter> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerEnter>(pEventData);

	std::shared_ptr<Actor> pItemActor(
		GameLogic::Get()->GetActor(pCastEventData->GetTriggerId()).lock());
}


void MinecraftLogic::PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerLeave> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerLeave>(pEventData);
}


void MinecraftLogic::PhysicsCollisionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysCollision> pCastEventData =
		std::static_pointer_cast<EventDataPhysCollision>(pEventData);

	std::shared_ptr<Actor> pGameActorA(
		GameLogic::Get()->GetActor(pCastEventData->GetActorA()).lock());
	std::shared_ptr<Actor> pGameActorB(
		GameLogic::Get()->GetActor(pCastEventData->GetActorB()).lock());
}

void MinecraftLogic::PhysicsSeparationDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysSeparation> pCastEventData =
		std::static_pointer_cast<EventDataPhysSeparation>(pEventData);

	std::shared_ptr<Actor> pGameActorA(
		GameLogic::Get()->GetActor(pCastEventData->GetActorA()).lock());
	std::shared_ptr<Actor> pGameActorB(
		GameLogic::Get()->GetActor(pCastEventData->GetActorB()).lock());
}

void MinecraftLogic::RegisterAllDelegates(void)
{
    // FUTURE WORK: Lots of these functions are ok to go into the base game logic!
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::RemoteClientDelegate),
        EventDataRemoteClient::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::SyncActorDelegate),
        EventDataSyncActor::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::GameInitDelegate),
        EventDataGameInit::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::GameReadyDelegate),
        EventDataGameReady::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::RequestStartGameDelegate),
        EventDataRequestStartGame::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::NetworkPlayerActorAssignmentDelegate),
        EventDataNetworkPlayerActorAssignment::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::EnvironmentLoadedDelegate),
        EventDataEnvironmentLoaded::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::EnvironmentLoadedDelegate),
        EventDataRemoteEnvironmentLoaded::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsTriggerEnterDelegate),
        EventDataPhysTriggerEnter::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsTriggerLeaveDelegate),
        EventDataPhysTriggerLeave::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsCollisionDelegate),
        EventDataPhysCollision::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsSeparationDelegate),
        EventDataPhysSeparation::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleRemoveSoundDelegate),
        EventDataRemoveSounds::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PlayerItemDelegate),
        EventDataPlayerItem::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PlayerPositionDelegate),
        EventDataPlayerPosition::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PlayerRespawnDelegate),
        EventDataPlayerRespawn::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::PlayerDamageDelegate),
        EventDataPlayerDamage::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleChatMessageDelegate),
        EventDataChatMessage::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleNotifyPlayerDelegate),
        EventDataNotifyActor::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::SaveBlockDataDelegate),
        EventDataSaveBlockData::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::DeletedBlocksDelegate),
        EventDataDeletedBlocks::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::GotBlocksDelegate),
        EventDataGotBlocks::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleNodeMetaFieldsDelegate),
        EventDataHandleNodeMetaFields::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleInventoryFieldsDelegate),
        EventDataHandleInventoryFields::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleInventoryActionDelegate),
        EventDataHandleInventoryAction::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftLogic::HandleInteractDelegate),
        EventDataInteract::skEventType);
}

void MinecraftLogic::RemoveAllDelegates(void)
{
    // FUTURE WORK: See the note in RegisterDelegates above....
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::RemoteClientDelegate),
        EventDataRemoteClient::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::SyncActorDelegate),
        EventDataSyncActor::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::GameInitDelegate),
        EventDataGameInit::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::GameReadyDelegate),
        EventDataGameReady::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::RequestStartGameDelegate),
        EventDataRequestStartGame::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::NetworkPlayerActorAssignmentDelegate),
        EventDataNetworkPlayerActorAssignment::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::EnvironmentLoadedDelegate),
        EventDataEnvironmentLoaded::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::EnvironmentLoadedDelegate),
        EventDataRemoteEnvironmentLoaded::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsTriggerEnterDelegate),
        EventDataPhysTriggerEnter::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsTriggerLeaveDelegate),
        EventDataPhysTriggerLeave::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsCollisionDelegate),
        EventDataPhysCollision::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PhysicsSeparationDelegate),
        EventDataPhysSeparation::skEventType);
    if (mIsProxy)
    {
        pGlobalEventManager->RemoveListener(
            MakeDelegate(this, &MinecraftLogic::RequestNewActorDelegate),
            EventDataRequestNewActor::skEventType);
    }

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleRemoveSoundDelegate),
        EventDataRemoveSounds::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PlayerItemDelegate),
        EventDataPlayerItem::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PlayerPositionDelegate),
        EventDataPlayerPosition::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PlayerRespawnDelegate),
        EventDataPlayerRespawn::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::PlayerDamageDelegate),
        EventDataPlayerDamage::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleChatMessageDelegate),
        EventDataChatMessage::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleNotifyPlayerDelegate),
        EventDataNotifyActor::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::SaveBlockDataDelegate),
        EventDataSaveBlockData::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::DeletedBlocksDelegate),
        EventDataDeletedBlocks::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::GotBlocksDelegate),
        EventDataGotBlocks::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleNodeMetaFieldsDelegate),
        EventDataHandleNodeMetaFields::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleInventoryFieldsDelegate),
        EventDataHandleInventoryFields::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleInventoryActionDelegate),
        EventDataHandleInventoryAction::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftLogic::HandleInteractDelegate),
        EventDataInteract::skEventType);
}

void MinecraftLogic::CreateNetworkEventForwarder(const int socketId)
{
    NetworkEventForwarder* pNetworkEventForwarder = new NetworkEventForwarder(socketId);

    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();

    // then add those events that need to be sent along to my attached clients
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataPhysTriggerEnter::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataGameInit::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataGameReady::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataPhysTriggerLeave::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataPhysCollision::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataPhysSeparation::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataDestroyActor::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataEnvironmentLoaded::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataNewActor::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataSyncActor::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataRequestNewActor::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataNetworkPlayerActorAssignment::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataPlayerRespawn::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataPlayerPosition::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataDeletedBlocks::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataGotBlocks::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataHandleNodeMetaFields::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataHandleInventoryFields::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataHandleInventoryAction::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
        EventDataInteract::skEventType);

    mNetworkEventForwarders.push_back(pNetworkEventForwarder);
}

void MinecraftLogic::DestroyAllNetworkEventForwarders(void)
{
    for (auto it = mNetworkEventForwarders.begin(); it != mNetworkEventForwarders.end(); ++it)
    {
        NetworkEventForwarder* networkEventForwarder = (*it);

        BaseEventManager* eventManager = BaseEventManager::Get();
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataPhysTriggerEnter::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataPhysTriggerLeave::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataPhysCollision::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataPhysSeparation::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataDestroyActor::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataEnvironmentLoaded::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataNewActor::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataSyncActor::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataRequestNewActor::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataNetworkPlayerActorAssignment::skEventType);

        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataPlayerRespawn::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataPlayerPosition::skEventType);

        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataDeletedBlocks::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataGotBlocks::skEventType);

        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataHandleNodeMetaFields::skEventType);

        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataHandleInventoryFields::skEventType);
        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataHandleInventoryAction::skEventType);

        eventManager->RemoveListener(
            MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
            EventDataInteract::skEventType);

        delete networkEventForwarder;
    }

    mNetworkEventForwarders.clear();
}

ActorFactory* MinecraftLogic::CreateActorFactory(void)
{
    return new MinecraftActorFactory();
}

LevelManager* MinecraftLogic::CreateLevelManager(void)
{
    MinecraftLevelManager* levelManager = new MinecraftLevelManager();
    levelManager->AddLevelSearchDir(L"world/minecraft/");
    levelManager->LoadLevelList(L"*.xml");

    for (auto levelId : levelManager->GetAllLevelIds())
        Settings::Get()->Set("default_game", ToString(levelId.c_str()));
    return levelManager;
}

AIManager* MinecraftLogic::CreateAIManager(void)
{
    MinecraftAIManager* aiManager = new MinecraftAIManager();
    return aiManager;
}


std::shared_ptr<LogicPlayer> MinecraftLogic::CreatePlayer(ActorId id, const char* name,
    const std::string& actorResource, tinyxml2::XMLElement* overrides, const Transform* initialTransform)
{
    MinecraftActorFactory* actorFactory = dynamic_cast<MinecraftActorFactory*>(mActorFactory);
    LogAssert(actorFactory, "minecraft actor factory is not initialized");

    std::shared_ptr<LogicPlayer> pActor = actorFactory->CreatePlayer(
        mEnvironment->GetItemManager(), name, ToWideString(actorResource).c_str(), overrides, initialTransform);
    pActor->SetId(id);
    if (pActor)
    {
        mPlayerIDs.push_back(pActor->GetId());
        mActors.insert(std::make_pair(pActor->GetId(), pActor));
        if (!mIsProxy && (mGameState == BGS_SPAWNINGPLAYERACTORS || mGameState == BGS_RUNNING))
        {
            std::shared_ptr<EventDataRequestNewActor> pNewActor(
                new EventDataRequestNewActor(actorResource, initialTransform, pActor->GetId()));
            BaseEventManager::Get()->TriggerEvent(pNewActor);
        }
        return pActor;
    }
    else
    {
        // FUTURE WORK: Log error: couldn't create actor
        return std::shared_ptr<LogicPlayer>();
    }
}

bool MinecraftLogic::GetGameInit()
{
    while (System::Get()->OnRun())
    {
        // End condition
        if (mGameInit)
            break;
    }

    return true;
}


bool MinecraftLogic::LoadGameAsync(tinyxml2::XMLElement* pRoot)
{
    // Read Textures and calculate sha1 sums
    FillMediaCache();

    /*
        Send some initialization data
    */
    ActorId actorId = INVALID_ACTOR_ID;
    //LogInformation("Sending content to " + GetPlayerName(actorId));

    // Send active objects
    PlayerLAO* playerLAO = GetPlayerLAO(actorId);
    if (playerLAO)
        SendActiveObjectRemoveAdd(playerLAO);

    // Send detached inventories
    mEnvironment->SendDetachedInventories(actorId, false);

    // Send player movement settings
    SendMovement(actorId);

    // Send time of day
    unsigned int time = mEnvironment->GetTimeOfDay();
    float timeSpeed = Settings::Get()->GetFloat("time_speed");
    SendTimeOfDay(actorId, time, timeSpeed);

    if (!GetGameInit())
    {
        LogError("Game init failed for unknown reason");
        return false;
    }

    // Create emerge manager
    mEmerge = std::make_unique<EmergeManager>(mEnvironment.get());

    // Tell the EmergeManager about the MapSettingsManager of logic map
    mEmerge->mMapSettingsMgr = &mEnvironment->GetLogicMap()->mSettingsMgr;

    MinecraftActorFactory* actorFactory = dynamic_cast<MinecraftActorFactory*>(mActorFactory);
    LogAssert(actorFactory, "actor factory is not initialized");

    // load all initial actors
    tinyxml2::XMLElement* pGameMods = pRoot->FirstChildElement("Mods");
    if (pGameMods)
    {
        tinyxml2::XMLElement* pGameMod = pGameMods->FirstChildElement();
        for (; pGameMod; pGameMod = pGameMod->NextSiblingElement())
        {
            std::wstring modResource = ToWideString(pGameMod->Attribute("resource"));
            auto actors = actorFactory->CreateMods(mEnvironment.get(), modResource.c_str(), nullptr);
            for (auto actor : actors)
            {
                // fire an event letting everyone else know that we created a new actor
                std::shared_ptr<EventDataNewActor> pNewActorEvent(new EventDataNewActor(actor->GetId()));
                BaseEventManager::Get()->QueueEvent(pNewActorEvent);
            }
        }
    }

    // Apply item aliases in the node definition manager
    mEnvironment->GetNodeManager()->UpdateAliases(mEnvironment->GetItemManager());

    mEnvironment->GetNodeManager()->SetNodeRegistrationStatus(true);

    // Perform pending node name resolutions
    mEnvironment->GetNodeManager()->RunNodeResolveCallbacks();

    // unmap node names in cross-references
    mEnvironment->GetNodeManager()->ResolveCrossrefs();

    // init the recipe hashes to speed up crafting
    BaseWritableCraftManager* craftManager =
        static_cast<BaseWritableCraftManager*>(mEnvironment->GetCraftManager());
    craftManager->InitHashes(mEnvironment.get());

    // Initialize mapgens
    mEmerge->InitMapGenerators(mEnvironment->GetLogicMap()->GetMapGeneratorParams());

    // Send items
    SendItemData();

    // Send nodes
    SendNodeData();

    // Send media
    SendMediaData();

    // Remove stale "recent" chat messages from previous connections
    mChatBackend->ClearRecentChat();

    // Make sure the size of the recent messages buffer is right
    mChatBackend->ApplySettings();

    // chat backend notification
    EventManager::Get()->QueueEvent(std::make_shared<EventDataInitChat>(mChatBackend));

    return true;
}

bool MinecraftLogic::LoadGameDelegate(tinyxml2::XMLElement* pRoot)
{
    System::Get()->SetResizable(true);

    // This is the ultimate default world path
    std::string gamePath =
        ToString(FileSystem::Get()->GetWorkingDirectory()) +
        "/../../Assets/Art/Minecraft/games/" + pRoot->Attribute("id");
    if (gamePath.empty())
        LogError("Supplied empty game path");

    // If world doesn't exist
    if (!GetWorldExists(gamePath + "/map"))
    {
        // we will be using "minetest"
        mGameSpec = FindSubgame(Settings::Get()->Get("selected_game"));
        LogInformation("Using default gameid [" + mGameSpec.mId + "]");
        if (!mGameSpec.IsValid())
        {
            LogWarning("Game specified in selected_game [" +
                Settings::Get()->Get("selected_game") + "] is invalid.");
            return false;
        }
    }
    else
    {
        // If world contains an embedded game, use it;
        // Otherwise find world from local system.
        mGameSpec = FindWorldSubgame(gamePath);
        LogInformation("Using world gameid [" + mGameSpec.mId + "]");
        if (!mGameSpec.IsValid())
        {
            LogWarning("Game [" + mGameSpec.mId + "] could not be found.");
            return false;
        }
    }

    // Update world information using main menu data
    std::vector<WorldSpec> worldSpecs = GetAvailableWorlds();
    int worldIndex = Settings::Get()->GetInt("selected_world");
    if (worldIndex >= 0 && worldIndex < (int)worldSpecs.size())
    {
        auto& worldSpec = worldSpecs[worldIndex];

        LogInformation("Selected world: " + worldSpec.mName + " [" + worldSpec.mPath + "]");

        // For singleplayer and local logic
        if (worldSpec.mPath.empty())
        {
            LogWarning("No world selected and no address provided. Nothing to do.");
            return false;
        }

        if (!FileSystem::Get()->ExistDirectory(ToWideString(worldSpec.mPath)))
        {
            LogWarning("Provided world path doesn't exist: " + worldSpec.mPath);
            return false;
        }

        mWorldSpec = worldSpec;
    }

    LogInformation("Game created id " + mGameSpec.mId +
        " - world: " + mWorldSpec.mPath + " - game: " + mGameSpec.mPath);

    Settings::Get()->CreateLayer(SL_GAME);

    // Create world if it doesn't exist
    try
    {
        std::string fileName = ToString(
            FileSystem::Get()->GetFileName(ToWideString(mWorldSpec.mPath)));
        LoadGameConfAndInitWorld(mWorldSpec.mPath, fileName, mGameSpec, false);
    }
    catch (const BaseException& e)
    {
        LogError("Failed to initialize world: " + std::string(e.what()));
    }

    mModMgr = std::make_shared<ModManager>(mGameSpec.mPath);
    /*
    std::vector<Mod> unsatisfiedMods = mModMgr->GetUnsatisfiedMods();
    // complain about mods with unsatisfied dependencies
    if (!mModMgr->IsConsistent())
        mModMgr->PrintUnsatisfiedModsError();
    */

    // LoadGame pLevelData -> add all the game registers
    //LoadGame();

    // LoadMods register each mod associated to the game
    mModMgr->LoadMods();

    // Initialize Environment
    mEnvironment.reset(new LogicEnvironment(mWorldSpec.mPath));

    //lock environment
    MutexAutoLock envlock(mEnvironment->mEnvMutex);

    if (!mEnvironment->GetLogicMap()->mSettingsMgr.MakeMapGeneratorParams())
        LogError("Couldn't create any mapgen type");

    mGame.reset(new TutorialGame(mEnvironment.get()));

    mEnvironment->LoadMeta();

    Inventory* inv = mGame->CreateDetachedInventory("creative_trash", "");
    inv->AddList("main", 1);

    mEnvironment->GetInventoryManager()->SetEnvironment(mEnvironment.get());

    // Those settings can be overwritten in world.mt, they are
    // intended to be cached after environment loading.
    mLiquidTransformEvery = Settings::Get()->GetFloat("liquid_update");
    mMaxChatMessageLength = Settings::Get()->GetUInt16("chat_message_max_size");

    return true;
}