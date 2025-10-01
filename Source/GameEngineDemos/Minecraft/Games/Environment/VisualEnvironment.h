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

#ifndef VISUALENVIRONMENT_H
#define VISUALENVIRONMENT_H

#include "Environment.h"

#include "../Actors/ActiveObjectManager.h"
#include "../Actors/InventoryManager.h"

#include "../../Data/Database.h"

#include "../../Graphics/Actors/VisualSimpleObject.h"
#include "../../Graphics/MeshGeneratorThread.h"

#include "Core/OS/OS.h"

class VisualPlayer;
class PlayerCamera;

class Sky;
class Minimap;
class VisualMap;
class GenericVisualActiveObject;

class NodeManager;
class BaseTextureSource;
class BaseWritableShaderSource;
class BaseWritableCraftManager;

/*
	The visual environment.

	This is not thread-safe.
	Must be called from main game engine thread (uses the SceneManager)
*/

enum VisualEnvironmentEventType
{
	VEE_NONE,
    VEE_PLAYER_DAMAGE
};


enum InteractAction
{
    INTERACT_START_DIGGING,     // 0: start digging (from undersurface) or use
    INTERACT_STOP_DIGGING,      // 1: stop digging (all parameters ignored)
    INTERACT_DIGGING_COMPLETED, // 2: digging completed
    INTERACT_PLACE,             // 3: place block or item (to abovesurface)
    INTERACT_USE,               // 4: use item
    INTERACT_ACTIVATE           // 5: rightclick air ("activate")
};

struct VisualEnvironmentEvent
{
	VisualEnvironmentEventType type;
	union 
    {
		struct
        {
			uint16_t amount;
			bool sendToLogic;
		} playerDamage;
	};
};

typedef std::unordered_map<uint16_t, VisualActiveObject*> VisualActiveObjectMap;
class VisualEnvironment : public Environment, public InventoryManager
{
public:
    VisualEnvironment(VisualMap* map, BaseTextureSource* textureSrc, BaseWritableShaderSource* shaderSrc);
	~VisualEnvironment();

	void Step(float dTime);
    void Stop();

    bool IsShutdown();

	virtual void SetVisualPlayer(VisualPlayer* player);
	VisualPlayer* GetPlayer() const { return mVisualPlayer; }

    std::shared_ptr<Map> GetMap();
    std::shared_ptr<VisualMap> GetVisualMap();

    // Node manager
    virtual NodeManager* GetNodeManager() { return mNodeMgr.get(); }

    // Item manager
    virtual BaseItemManager* GetItemManager() { return mItemMgr.get(); }

    // Craft manager
    virtual BaseCraftManager* GetCraftManager() { return nullptr; }

    BaseSoundManager* GetSoundManager() { return mSoundMgr; }

    BaseTextureSource* GetTextureSource() { return mTextureSrc; }
    BaseWritableShaderSource* GetShaderSource() { return mShaderSrc; }

    PlayerCamera* GetPlayerCamera() { return mPlayerCamera; }
    void SetPlayerCamera(PlayerCamera* camera) { mPlayerCamera = camera; }

    Sky* GetSky() { return mSky; }
    void SetSky(Sky* sky) { mSky = sky; }

    Minimap* GetMinimap() { return mMinimap; }
    void SetMinimap(Minimap* minimap) { mMinimap = minimap; }

    void AddNode(Vector3<short> position, MapNode node, bool removeMetadata = true);
    void RemoveNode(Vector3<short> position);

	/*
		VisualSimpleObjects
	*/
	void AddSimpleObject(VisualSimpleObject* simple);

	/*
		ActiveObjects
	*/
    GenericVisualActiveObject* GetGenericVAO(uint16_t id);
	VisualActiveObject* GetActiveObject(uint16_t id) { return mAOManager.GetActiveObject(id); }

	/*
		Adds an active object to the environment.
		Environment handles deletion of object.
		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	uint16_t AddActiveObject(VisualActiveObject* object);

	void AddActiveObject(uint16_t id, uint8_t type, const std::string& initData);
	void RemoveActiveObject(uint16_t id);

	void ProcessActiveObjectMessage(uint16_t id, const std::string& data);

	/*
		Callbacks for activeobjects
	*/

	void DamageVisualPlayer(uint16_t damage, bool handleHP=true);

	/*
		Visual likes to call these
	*/

	// Get all nearby objects
	void GetActiveObjects(const Vector3<float>& origin, float maxDistance,
		std::vector<DistanceSortedActiveObject>& dest)
	{
		return mAOManager.GetActiveObjects(origin, maxDistance, dest);
	}

	bool HasEnvironmentEvents() const { return !mVisualEventQueue.empty(); }

	// Get event from queue. If queue is empty, it triggers an assertion failure.
	VisualEnvironmentEvent GetEnvironmentEvent();

    const std::unordered_set<std::string>& GetPrivilegeList() const { return mPrivileges; }
    bool CheckPrivilege(const std::string &priv) const { return (mPrivileges.count(priv) != 0); }
    bool CheckLocalPrivilege(const std::string& priv) { return CheckPrivilege(priv); }

	virtual void GetSelectedActiveObjects(
		const Line3<float>& shootlineOnMap, std::vector<PointedThing>& objects);

    // InventoryManager
    virtual void DoInventoryAction(InventoryAction* action);
    virtual Inventory* GetInventory(const InventoryLocation& loc);

    void HandleDetachedInventory(const std::string& name, const std::string& content, bool keepInv);

    float GetAnimationTime();

    int GetCrackLevel();
    Vector3<short> GetCrackPosition();
    void SetCrack(int level, Vector3<short> pos);

    uint16_t GetHP();

	void UpdateCameraOffset(const Vector3<short>& cameraOffset) 
    {
        mCameraOffset = cameraOffset; 
        mMeshUpdateThread.mCameraOffset = cameraOffset;
    }
	Vector3<short> GetCameraOffset() const { return mCameraOffset; }

    void AddUpdateMeshTask(Vector3<short> position, bool ackToLogic = false, bool urgent = false);
    void AddUpdateMeshTaskWithEdge(Vector3<short> blockPos, bool ackToLogic = false, bool urgent = false);
    void AddUpdateMeshTaskForNode(Vector3<short> nodePos, bool ackToLogic = false, bool urgent = false);

    MeshUpdateThread mMeshUpdateThread;

private:

    bool mShutdown = false;

	std::shared_ptr<VisualMap> mMap;
	VisualPlayer* mVisualPlayer = nullptr;

    BaseSoundManager* mSoundMgr = nullptr;

    BaseTextureSource* mTextureSrc = nullptr;
    BaseWritableShaderSource* mShaderSrc = nullptr;

    // When created, these will be filled with data received from the logic
    std::shared_ptr<BaseWritableItemManager> mItemMgr = nullptr;
    std::shared_ptr<NodeManager> mNodeMgr = nullptr;

	VisualActiveObjectManager mAOManager;
	std::vector<VisualSimpleObject*> mSimpleObjects;
    std::queue<VisualEnvironmentEvent> mVisualEventQueue;
	IntervalLimiter mActiveObjectLightUpdateInterval;

    Vector3<short> mCameraOffset = Vector3<short>::Zero();
    PlayerCamera* mPlayerCamera = nullptr;

    Minimap* mMinimap = nullptr;
    Sky* mSky = nullptr;

    // Privileges
    std::unordered_set<std::string> mPrivileges;

    // Block mesh animation parameters
    float mAnimationTime = 0.0f;
    Vector3<short> mCrackPosition = Vector3<short>::Zero();
    int mCrackLevel = -1;

    // Detached inventories
    // key = name
    std::unordered_map<std::string, Inventory*> mDetachedInventories;
};

#endif