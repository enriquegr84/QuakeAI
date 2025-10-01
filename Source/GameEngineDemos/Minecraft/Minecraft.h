//========================================================================
// Minecraft.h : source file for the sample game
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/GameEngine/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#ifndef MINECRAFT_H
#define MINECRAFT_H

#include "Data/Huddata.h"
#include "Data/CloudParams.h"
#include "Data/ParticleParams.h"
#include "Data/TileParams.h"
#include "Data/SkyParams.h"

#include "Game/Game.h"
#include "Games/Mods.h"
#include "Games/Subgames.h"

#include "Games/Actors/Player.h"
#include "Games/Actors/LogicPlayer.h"

#include "Games/Map/MapBlock.h"

#include "Games/Map/Emerge.h"

#define CHAT_MESSAGE_LIMIT_PER_10S 10.0f

class BaseWritableCraftManager;
class BaseWritableItemManager;

class BaseEventManager;
class NetworkEventForwarder;

class MinecraftLogicThread;

struct MediaInfo
{
    std::wstring path;
    std::string sha1Digest;

    MediaInfo(const std::wstring& aPath = L"",
        const std::string& aSha1Digest = "") :
        path(aPath), sha1Digest(aSha1Digest)
    {

    }
};

//---------------------------------------------------------------------------------------------------------------------
// MinecraftLogic class                        - Chapter 21, page 723
//---------------------------------------------------------------------------------------------------------------------
class MinecraftLogic : public GameLogic
{
	friend class MinecraftAIManager;
    friend class MinecraftLogicThread;

public:
	MinecraftLogic();
	virtual ~MinecraftLogic();

	// Update
	virtual void SetProxy();

	virtual void SyncActor(const ActorId id, Transform const &transform);

	virtual void ResetViewType();
	virtual void UpdateViewType(const std::shared_ptr<BaseGameView>& pView, bool add);

    virtual void Start(tinyxml2::XMLElement* pLevelData);
    virtual void Stop();

    void Step(float dTime);

    // This is run by logic thread and does the actual processing
    void AsyncStep();

    // Logic Update
    virtual void OnUpdate(float time, float deltaMs);

	// Overloads
	virtual void ChangeState(BaseGameState newState);
	virtual std::shared_ptr<BaseGamePhysic> GetGamePhysics(void) { return mPhysics; }

	// event delegates
    void GameInitDelegate(BaseEventDataPtr pEventData);
    void GameReadyDelegate(BaseEventDataPtr pEventData);
	void RequestStartGameDelegate(BaseEventDataPtr pEventData);
	void RemoteClientDelegate(BaseEventDataPtr pEventData);
	void NetworkPlayerActorAssignmentDelegate(BaseEventDataPtr pEventData);
	void PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData);
	void PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData);
	void PhysicsCollisionDelegate(BaseEventDataPtr pEventData);
	void PhysicsSeparationDelegate(BaseEventDataPtr pEventData);
	void EnvironmentLoadedDelegate(BaseEventDataPtr pEventData);

    void DeletedBlocksDelegate(BaseEventDataPtr pEventData);
    void GotBlocksDelegate(BaseEventDataPtr pEventData);

    void PlayerDamageDelegate(BaseEventDataPtr pEventData);
    void PlayerRespawnDelegate(BaseEventDataPtr pEventData);
    void PlayerPositionDelegate(BaseEventDataPtr pEventData);
    void PlayerItemDelegate(BaseEventDataPtr pEventData);

    void HandleRemoveSoundDelegate(BaseEventDataPtr pEventData);

    void HandleNodeMetaFieldsDelegate(BaseEventDataPtr pEventData);

    void HandleInventoryFieldsDelegate(BaseEventDataPtr pEventData);
    void HandleInventoryActionDelegate(BaseEventDataPtr pEventData);

    void HandleInteractDelegate(BaseEventDataPtr pEventData);

    void HandleChatMessageDelegate(BaseEventDataPtr pEventData);
    void HandleNotifyPlayerDelegate(BaseEventDataPtr pEventData);

    void SaveBlockDataDelegate(BaseEventDataPtr pEventData);

protected:

    void RespawnPlayer(ActorId actorId);
    void DeletePlayer(ActorId actorId);

    bool HudSetHotbarItemCount(LogicPlayer* player, int hotbarItemCount);
    bool HudSetFlags(LogicPlayer* player, unsigned int flags, unsigned int mask);

    void SetPlayerEyeOffset(LogicPlayer* player,
        const Vector3<float>& first, const Vector3<float>& third);

    void SetSky(LogicPlayer* player, const SkyboxParams& params);
    void SetSun(LogicPlayer* player, const SunParams& params);
    void SetMoon(LogicPlayer* player, const MoonParams& params);
    void SetStars(LogicPlayer* player, const StarParams& params);
    void SetClouds(LogicPlayer* player, const CloudParams& params);

    void OverrideDayNightRatio(LogicPlayer* player, bool doOverride, float ratio);

    void SendMovement(ActorId playerId);

    /*
        Get a player from memory or creates one.
        If player is already connected, return NULL
        Does not verify/modify auth info and password.

        Call with env and con locked.
    */
    PlayerLAO* EmergePlayer(const char* name, ActorId actorId);

    void SendPlayerPrivileges(ActorId actorId);
    void SendPlayerInventoryForm(ActorId actorId);

    void SendTimeOfDay(ActorId actorId, unsigned int time, float timeSpeed);

    void SendBlocks(float dTime);

    void SendSpawnParticle(ActorId actorId, const ParticleParameters& parameters);
    void SendAddParticleSpawner(ActorId actorId,
        const ParticleSpawnerParameters& parameters, ActorId attachedId, unsigned int id);
    void SendDeleteParticleSpawner(ActorId actorId, unsigned int id);

    void NotifyPlayer(const char* name, const std::wstring& msg);
    void NotifyPlayers(const std::wstring& msg);

    bool CanSendChatMessage() const;
    void SendChatMessage(const std::wstring& message);

    bool GetChatMessage(std::wstring& res);
    void UpdateChat(float dTime, const Vector2<unsigned int>& screensize);
    inline void PushToChatQueue(ChatMessage* cec)
    {
        mChatQueue.push(cec);
    }

    void HandleChatMessage(ActorId actorId, const ChatMessage& message);
    void HandleChatInterfaceEvent(ChatEvent* evt);

    // This returns the answer to the sender of wmessage, or "" if there is none
    std::wstring HandleChat(const std::string& name, std::wstring messageInput,
        bool checkShoutPriv = false, LogicPlayer* player = nullptr);
    void HandleAdminChat(const ChatEventChat* evt);

	// event registers
	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);
	void CreateNetworkEventForwarder(const int socketId);
	void DestroyAllNetworkEventForwarders(void);

	std::list<NetworkEventForwarder*> mNetworkEventForwarders;

    std::vector<ActorId> GetPlayerIDs();

    std::shared_ptr<LogicPlayer> CreatePlayer(
        ActorId id, const char* name, const std::string& actorResource,
        tinyxml2::XMLElement* overrides, const Transform* initialTransform = NULL);

    virtual ActorFactory* CreateActorFactory(void);
    virtual LevelManager* CreateLevelManager(void);
    virtual AIManager* CreateAIManager(void);

	virtual bool LoadGameDelegate(tinyxml2::XMLElement* pLevelData);

    bool LoadGameAsync(tinyxml2::XMLElement* pLevelData);

    std::shared_ptr<ModManager> mModMgr = nullptr;

private:

    bool GetGameInit();

    void UpdatePlayerList();

    PlayerLAO* GetPlayerLAO(ActorId actorId);
    void SendActiveObjectRemoveAdd(PlayerLAO* playerLAO);
    void SendActiveObjectMessages(const std::string& datas, bool reliable = true);

    bool CheckInteractDistance(LogicPlayer* player, const float distance, const std::string& what);

    void SendRemoveNode(Vector3<short> position,
        std::unordered_set<ActorId>* farPlayers, float farDistNodes = 100);
    void SendAddNode(Vector3<short> position,
        MapNode node, std::unordered_set<ActorId>* farPlayers,
        float farDistanceNodes = 100, bool removeMetadata = true);
    void SendMetadataChanged(
        const std::list<Vector3<short>>& metaUpdates, float farDistNodes = 100);

    bool AddMediaFile(const std::wstring& fileName, 
        const std::wstring& filePath, const std::wstring& fileRelativePath, 
        std::string* fileData = nullptr, std::string* digest = nullptr);
    void FillMediaCache();

    void SendItemData();
    void SendNodeData();
    void SendMediaData();

    std::wstring FormatChatMessage(const std::string& name, const std::string& message);
    bool OnChatMessage(const std::string& name, const std::string& message);

    std::vector<ActorId> mPlayerIDs;

    // Subgame specification
    Subgame mGameSpec;

    // World Spec must be kept in sync!
    WorldSpec mWorldSpec;

    bool mGameInit = false;

    /* Threads */

    // A buffer for time steps
    // step() increments and AsyncRunStep() run by m_thread reads it.
    float mStepDeltaMs = 0.0f;
    std::mutex mStepDeltaMsMutex;

    // The minecraft logic mainly operates in this thread
    MinecraftLogicThread* mThread = nullptr;

    // media files known
    std::unordered_map<std::wstring, MediaInfo> mMedia;

    std::unordered_map<std::string, ModMetadata*> mModStorages;
    float mModStorageSaveTimer = 10.0f;

    unsigned short mMaxChatMessageLength;

    ChatBackend* mChatBackend = nullptr;
    std::queue<std::string> mChatLogBuf;

    std::queue<std::wstring> mOutChatQueue;
    unsigned int mLastChatMessageSent = (unsigned int)std::time(0);
    float mChatMessageAllowance = 5.0f;
    std::queue<ChatMessage*> mChatQueue;

    ChatInterface* mAdminChat = nullptr;
    std::string mAdminNick;

    // Logic
    std::unique_ptr<LogicEnvironment> mEnvironment;

    //Base game
    std::unique_ptr<BaseGame> mGame;

    //Emerge manager
    std::unique_ptr<EmergeManager> mEmerge;

    // Some timers
    float mEmergethreadTriggerTimer = 0.0f;
    float mLiquidTransformTimer = 0.0f;
    float mLiquidTransformEvery = 1.0f;
    float mMasterTimer = 0.0f;
    float mSavemapTimer = 0.0f;
    IntervalLimiter mMapTimerAndUnloadInterval;

    float mPrintInfoTimer;
};


class MinecraftLogicThread : public Thread
{
    friend class MinecraftLogic;

public:

    MinecraftLogicThread(MinecraftLogic* logic) : Thread("MinecraftLogic"), mGameLogic(logic)
    {

    }

    virtual void* Run();

protected:

    tinyxml2::XMLElement* mLevelData;

private:
    MinecraftLogic* mGameLogic;
};

#endif