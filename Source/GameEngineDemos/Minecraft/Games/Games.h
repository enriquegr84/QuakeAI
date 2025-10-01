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

#ifndef GAMES_H
#define GAMES_H

#include "GameEngineStd.h"

#include "../Data/Huddata.h"

#include "../Utils/AreaStore.h"

#include "Actors/Craft.h"
#include "Actors/PlayerLAO.h"
#include "Actors/LogicPlayer.h"

#include "../Graphics/Actors/VisualPlayer.h"
#include "../Graphics/PlayerCamera.h"
#include "../Graphics/Map/Minimap.h"

class EntityLAO;
class LogicActiveObject;
class LogicEnvironment;
class ABM;

struct Mod;
struct MapNode;
struct PointedThing;
struct ObjectProperties;
struct InventoryLocation;
struct CollisionMoveResult;
struct PlayerHPChangeReason;

class StatBars
{
public:

    StatBars();
    ~StatBars();

    void Cleanup(PlayerLAO* player);
    void Update(PlayerLAO* player);
    bool ReplaceHud(HudElement* hud, std::string hudName);
    bool EventHandler(PlayerLAO* player, std::string eventName);

protected:

    unsigned int ScaleToDefault(PlayerLAO* player, std::string field);

    //cache setting
    bool mEnableDamage;

    HudElement* mHealthBar;
    HudElement* mBreathBar;

    std::vector<std::string> mHudIds;
    std::map<std::string, uint16_t> mHealthBarIds;
    std::map<std::string, uint16_t> mBreathBarIds;
};

/*
    An interface for fetching game-global definitions like tool and
    mapnode properties
*/

class BaseGame
{
public:

    BaseGame(LogicEnvironment* env);
    ~BaseGame();

    // These are thread-safe IF they are not edited while running threads.
    // Thus, first they are set up and then they are only read.


    //virtual const std::vector<Mod>& GetMods() const;
    //virtual const Mod* GetMod(const std::string& modname) const;

    virtual bool ModsLoaded() { return mModsLoaded; }
    virtual void ModsLoaded(bool loaded) { mModsLoaded = loaded; }

    virtual void Privilege() { }

    virtual bool OnReceivingChatMessage(std::string message) { return false; }
    virtual bool OnSendingChatMessage(std::string message) { return false; }
    virtual bool OnChatCommand(std::string message) { return false; }
    virtual void OnDeath();
    virtual void Shutdown() { }

    virtual void OnCameraReady(PlayerCamera* playerCamera) { }
    virtual void OnPlayerReady(VisualPlayer* visualPlayer) { }
    virtual void OnMinimapReady(Minimap* minimap) { }

    virtual void OnHpModification() { }
    virtual void OnDamageTaken() { }
    virtual void OnFormInput() { }

    virtual void MapGenerator() { }

    virtual bool AfterPlaceNode(const Vector3<short>& pos, std::shared_ptr<LogicPlayer> player, const Item& item);
    virtual void OnPlaceNode(const PointedThing& pointed, const Item& item);
    virtual void OnUseNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnPunchNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnPunch(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnFallNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool CanDigNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnDigNode(const Vector3<short>& pos, const MapNode& node, PlayerLAO* digger);
    virtual bool OnFloodNode(const Vector3<short>& pos, const MapNode& node, const MapNode& newNode) { return false; }
    virtual bool OnTimerNode(const Vector3<short>& pos, const MapNode& node, float dTime);
    virtual void OnDestructNode(const Vector3<short>& pos, const MapNode& node);
    virtual void AfterDestructNode(const Vector3<short>& pos, const MapNode& node);
    virtual void OnConstructNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnRightClickNode(const Vector3<short>& pos, const MapNode& node);
    virtual void OnRecieveFieldsNode(const Vector3<short>& pos,
        const std::string& formName, const StringMap& fields, UnitLAO* player);

    virtual bool OnSecondaryUseItem(ItemStack& stack, LogicActiveObject* user, const PointedThing& pointed);
    virtual bool OnUseItem(const ItemStack& stack, const PointedThing& pointed);
    virtual bool OnUseItem(ItemStack& stack, LogicActiveObject* user, const PointedThing& pointed);
    virtual bool OnPlaceItem(ItemStack& stack, LogicActiveObject* placer, const PointedThing& pointed);
    virtual bool OnCraftItem(ItemStack& stack, LogicActiveObject* user,
        const InventoryList* oldCraftGrid, const InventoryLocation& craftInv);
    virtual bool OnCraftPredictItem(ItemStack& stack, LogicActiveObject* user,
        const InventoryList* oldCraftGrid, const InventoryLocation& craftInv);
    virtual bool OnDropItem(ItemStack& stack, LogicActiveObject* dropper, Vector3<float> pos);

    virtual bool OnOpenInventory(Inventory* inv) { return false; }
    //virtual void OnActionInventory();

    void AddObjectReference(LogicActiveObject* cobj) { }
    void RemoveObjectReference(LogicActiveObject* cobj) { }

    bool TryPlaceEntity(unsigned int id, MapNode node, Vector3<short> nodePos);
    bool TryMergeWithEntity(ItemStack ownStack, EntityLAO* objectLao, EntityLAO* entityLao);
    void EnablePhysicsEntity(EntityLAO* lao);
    void DisablePhysicsEntity(EntityLAO* lao);

    virtual void RemoveEntity(EntityLAO* lao);
    virtual bool AddEntity(EntityLAO* lao, const char* name);
    virtual void OnDeactivateEntity(EntityLAO* lao) { }
    virtual void OnActivateEntity(EntityLAO* lao,
        const std::string& staticData, unsigned int dTime);
    virtual std::string GetStaticDataEntity(const EntityLAO* lao) const;
    virtual void GetPropertiesEntity(EntityLAO* lao, ObjectProperties* prop);
    virtual void OnStepEntity(EntityLAO* lao, float dTime, const CollisionMoveResult* moveResult);
    virtual bool OnPunchEntity(EntityLAO* lao, LogicActiveObject* puncher, float timeFromLastPunch,
        const ToolCapabilities* toolcap, Vector3<float> dir, short damage);
    virtual bool OnDeathEntity(EntityLAO* lao, LogicActiveObject* killer);
    virtual void OnRightClickEntity(EntityLAO* lao, LogicActiveObject* clicker);
    virtual void OnAttachChildEntity(LogicActiveObject* lao, LogicActiveObject* child);
    virtual void OnDetachChildEntity(LogicActiveObject* lao, LogicActiveObject* child);
    virtual void OnDetachEntity(LogicActiveObject* lao, LogicActiveObject* parent);

    virtual void OnMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual void OnPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual void OnTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual int AllowPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);

    virtual void OnMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual void OnPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual void OnTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual int AllowPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);

    virtual void OnMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual void OnPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual void OnTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual int AllowPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);

    virtual Inventory* CreateDetachedInventory(const std::string& name, const std::string& player);
    virtual void RemoveDetachedInventory(const std::string& name);

    virtual void OnRecieveFieldsPlayer(PlayerLAO* player, const std::string& formName, const StringMap& fields);
    virtual void OnLeavePlayer(PlayerLAO* playerLAO);
    virtual void OnJoinPlayer(PlayerLAO* playerLAO);
    virtual void OnPrejoinPlayer(PlayerLAO* playerLAO) { }
    virtual void OnDiePlayer(PlayerLAO* player, const PlayerHPChangeReason& reason);
    virtual void OnCheatPlayer(PlayerLAO* player, const std::string& cheatType) { }
    virtual void OnRightClickPlayer(PlayerLAO* player, LogicActiveObject* clicker);
    virtual bool OnEventPlayer(PlayerLAO* player, const std::string& type);
    virtual bool OnPunchPlayer(PlayerLAO* player, LogicActiveObject* hitter,
        float timeFromLastPunch, const ToolCapabilities* toolcap, Vector3<float> dir, short damage);
    virtual int OnHPChangePlayer(PlayerLAO* player, int hpChange, const PlayerHPChangeReason& reason);
    virtual void OnRespawnPlayer() { }
    virtual void OnNewPlayer() { }

    virtual void OnGenerateEnvironment(Vector3<short> minp, Vector3<short> maxp, unsigned int blockseed) { }
    //virtual void OnEmergeAreaCompletion(Vector3<short> pos, int action);

    virtual void OnActionABM(ABM* abm, Vector3<short> p, MapNode n,
        unsigned int activeObjectCount, unsigned int activeObjectCountWider) { }

    virtual void RegisterABM() { }
    virtual void RegisterCraft() { }
    virtual void RegisterItem(tinyxml2::XMLElement* pData);
    virtual void RegisterLBM() { }
    virtual void RegisterEntity() { }

    virtual void Step(float dTime);

    // Getter for the minecraft visual and logic game events. 
    static BaseGame* Get(void);

protected:

    void SampleStep(float dTime);
    void Remove(EntityLAO* lao);
    void SetItem(EntityLAO* lao, const std::string& itemString);
    void GetNodeDrops(MapNode node, std::string toolName, std::vector<std::string>& drops);

    bool IsProtected(const Vector3<short>& placeTo, LogicPlayer* player);

    bool EatItem(uint16_t hpChange, LogicActiveObject* user, 
        ItemStack& itemStack, const PointedThing& pointed);
    ItemStack ItemPlaceNode(ItemStack& itemStack, LogicActiveObject* placer,
        const PointedThing& pointed, uint8_t param2, bool preventAfterPlace = false);

    uint8_t DirectionToFaceDirection(const Vector3<int>& dir, bool is6Dir = true);
    uint8_t DirectionToWallmounted(const Vector3<int>& dir);

    float CalculateKnockback(PlayerLAO* player, LogicActiveObject* hitter,
        float timeFromLastPunch, const ToolCapabilities* toolcap, 
        Vector3<float> dir, float distance, short damage);

    void CheckForFalling(const Vector3<short>& pos);
    bool CheckSingleForFalling(const Vector3<short>& pos);
    bool ConvertToFallingNode(const Vector3<short>& pos, const MapNode& node);
    bool CheckAttachedNode(const Vector3<short>& pos, const MapNode& node);
    void DropAttachedNode(const Vector3<short>& pos);
    bool DigNode(const Vector3<short>& pos, const MapNode& node, PlayerLAO* digger);
    void HandleNodeDrops(const Vector3<short>& pos, const std::vector<std::string>& drops, PlayerLAO* digger);

    bool RotateAndPlace(ItemStack& stack, LogicActiveObject* placer, const PointedThing& pointed,
        bool infiniteStacks, bool invertWall, bool preventAfterPlace);

    void ReportMetadataChange(MapNodeMetadata* meta, const Vector3<short>& pos, const std::string& name);

    struct DetachedInventory
    {
        std::function<int(Inventory* inv, InventoryList* fromList, unsigned int fromIndex, 
            InventoryList* toList, unsigned int toIndex, int count, const std::string& player)> allowMove = nullptr;
        std::function<int(Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)> allowPut = nullptr;
        std::function<int(Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)> allowTake = nullptr;

        std::function<bool(Inventory* inv, InventoryList* fromList, unsigned int fromIndex,
            InventoryList* toList, unsigned int toIndex, int count, const std::string& player)> onMove = nullptr;
        std::function<bool(Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)> onPut = nullptr;
        std::function<bool(Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)> onTake = nullptr;
    };

    static BaseGame* mGame;

    bool mModsLoaded = false;
    float mGravity = 9.81f;
    float mTimeToLive = 900.f;

    /*  This table is specifically ordered.
        We don't walk diagonals, only our direct neighbors, and self.
        Down first as likely case, but always before self.The same with sides.
        Up must come last, so that things above self will also fall all at once.*/
    std::vector<Vector3<short>> mCheckForFallingNeighbors;

    //Table of dirs in wallmounted order
    std::vector<Vector3<short>> mWallmountedToDirection;

    std::vector<Vector3<float>> mFacedirToEuler;

    LogicEnvironment* mEnvironment = nullptr;

    std::map<ActorId, bool> mEntitiesMoving;
    std::map<ActorId, float> mEntitiesActive;
    std::map<ActorId, std::string> mEntitiesItemString;
    std::map<ActorId, Vector3<float>> mEntitiesForceOut;
    std::map<ActorId, Vector3<float>> mEntitiesForceOutStart;

    std::map<ActorId, bool> mFallingEntitiesFloats;
    std::map<ActorId, std::string> mFallingEntitiesNode;
    std::map<ActorId, std::string> mFallingEntitiesMeta;

    std::shared_ptr<StatBars> mStatBars;
    std::unordered_map<std::string, DetachedInventory> mDetachedInventories;
};

class TutorialGame : public BaseGame
{
public:

    TutorialGame(LogicEnvironment* env);

    virtual void Step(float dTime);

    virtual bool AfterPlaceNode(const Vector3<short>& pos, std::shared_ptr<LogicPlayer> player, const Item& item);
    virtual void OnPlaceNode(const PointedThing& pointed, const Item& item);
    virtual void OnUseNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnPunchNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnPunch(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnFallNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool CanDigNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnDigNode(const Vector3<short>& pos, const MapNode& node, PlayerLAO* digger);
    virtual bool OnFloodNode(const Vector3<short>& pos, const MapNode& node, const MapNode& newNode) { return false; }
    virtual bool OnTimerNode(const Vector3<short>& pos, const MapNode& node, float dTime);
    virtual void OnDestructNode(const Vector3<short>& pos, const MapNode& node);
    virtual void AfterDestructNode(const Vector3<short>& pos, const MapNode& node);
    virtual void OnConstructNode(const Vector3<short>& pos, const MapNode& node);
    virtual bool OnRightClickNode(const Vector3<short>& pos, const MapNode& node);
    virtual void OnRecieveFieldsNode(const Vector3<short>& pos,
        const std::string& formName, const StringMap& fields, UnitLAO* player);

    virtual bool OnUseItem(ItemStack& stack, LogicActiveObject* user, const PointedThing& pointed);
    virtual bool OnPlaceItem(ItemStack& stack, LogicActiveObject* placer, const PointedThing& pointed);

    virtual void OnMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual void OnPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual void OnTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual int AllowPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);

    virtual void OnMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual void OnPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual void OnTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual int AllowPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);

    virtual void OnMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual void OnPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual void OnTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player);
    virtual int AllowPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);
    virtual int AllowTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player);

    virtual void OnGenerateEnvironment(Vector3<short> minp, Vector3<short> maxp, unsigned int blockseed);

    virtual Inventory* CreateDetachedInventory(const std::string& name, const std::string& player);
    virtual void RemoveDetachedInventory(const std::string& name);

    void OnRecieveFieldsPlayer(PlayerLAO* player, const std::string& formName, const StringMap& fields);

    virtual void OnLeavePlayer(PlayerLAO* playerLAO);
    virtual void OnJoinPlayer(PlayerLAO* playerLAO);

    virtual void OnActionABM(ABM* abm, Vector3<short> p, MapNode n,
        unsigned int activeObjectCount, unsigned int activeObjectCountWider);

    virtual void RegisterItem(tinyxml2::XMLElement* pData);

protected:

    void AreasInit();

    void TutorialStep(float dTime);
    void WieldedItemStep(float dTime);
    void BackgroundMusicStep(float dTime);
    void PlayerStep(float dTime);
    void AreasStep(float dTime);

    void SetCraftingForm(PlayerLAO* player);
    void SetCreativeForm(PlayerLAO* player, unsigned short startIndex, unsigned short pageNum, unsigned short tabId);
    void UpdateCreativeInventory(PlayerLAO* player, const std::string& filter, unsigned short tabId);

private:

    void LoadState();
    void SaveState();

    std::vector<Area*> GetAreasAtPosition(const Vector3<float>& pos);

    std::vector<std::string> mLocationsOrder;
    std::map<std::string, Vector3<float>> mLocationsPosition;
    std::map<std::string, Vector2<float>> mLocationsLookAt;

    std::map<ActorId, unsigned int> mPlayerInventorySize;
    std::map<ActorId, std::string> mPlayerInventoryFilter;
    std::map<ActorId, unsigned int> mPlayerInventoryTabId;
    std::map<ActorId, unsigned int> mPlayerInventoryStartIndex;

    std::map<std::string, std::string> mCaptions;
    std::map<std::string, std::string> mTexts;

    std::map<std::string, std::string> mWield;
    std::map<std::string, int> mWieldIndex;

    std::map<std::string, unsigned int> mHuds;
    std::map<std::string, float> mDeltaTimes;
    float mDeltaLimit = 3;  //HUD element will be hidden after this many seconds

    float mStepTimer = 0.f;
    json mState;

    //Number of gold ingots / lumps
    uint8_t mGold = 13;

    //Number of hidden diamonds
    uint8_t mDiamonds = 12;

    bool mSongPlaying = false;
    float mSongTimeLeft = 0.f;
    float mSongTimeNext = 10.f; // seconds
    float mSongPauseBetween = 7.f;

    VectorAreaStore mAreaStore;
    std::map<std::string, unsigned int> mAreasHuds;

};

#endif