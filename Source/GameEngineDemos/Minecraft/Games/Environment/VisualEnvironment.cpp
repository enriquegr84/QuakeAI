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

#include "VisualEnvironment.h"
#include "LogicEnvironment.h"

#include "../Map/MapSector.h"
#include "../Map/MapBlock.h"

#include "../Map/VoxelAlgorithms.h"
#include "../Map/Emerge.h"

#include "../Games.h"

#include "../../Graphics/Node.h"
#include "../../Graphics/Shader.h"

#include "../../Graphics/MeshGeneratorThread.h"

#include "../../Graphics/Actors/VisualPlayer.h"
#include "../../Graphics/Actors/VisualSimpleObject.h"
#include "../../Graphics/Actors/ContentVisualActiveObject.h"

#include "../../Graphics/Map/MapBlockMesh.h"
#include "../../Graphics/Map/VisualMap.h"

#include "../../Physics/Raycast.h"
#include "../../Physics/Collision.h"

#include "Core/Utility/Serialize.h"
#include "Core/Utility/Profiler.h"

#include "Application/Settings.h"

#include "../../MinecraftEvents.h"


/*
	VisualEnvironment
*/
VisualEnvironment::VisualEnvironment(VisualMap* map, BaseTextureSource* textureSrc, BaseWritableShaderSource* shaderSrc) 
    : Environment(), mMap(map), mTextureSrc(textureSrc), mShaderSrc(shaderSrc), mMeshUpdateThread(this)
{
    mItemMgr = CreateItemManager();
    mNodeMgr = CreateNodeManager();
}

VisualEnvironment::~VisualEnvironment()
{
    mShutdown = true;
	mAOManager.Clear();

    mMeshUpdateThread.Stop();
    mMeshUpdateThread.Wait();
    while (!mMeshUpdateThread.mQueueOut.Empty()) 
    {
        MeshUpdateResult r = mMeshUpdateThread.mQueueOut.PopFrontNoEx();
        r.mesh.reset();
    }

    //delete mInventoryFromLogic;

    // cleanup 3d model meshes on visual shutdown
    //RenderingEngine::get_mesh_cache()->removeMesh(mesh);

	for (auto& simpleObject : mSimpleObjects)
		delete simpleObject;

    // Delete detached inventories
    for (auto& detachedInventory : mDetachedInventories) 
        delete detachedInventory.second;

	// Drop/delete map
	//mMap->drop();

	delete mVisualPlayer;
}

std::shared_ptr<Map> VisualEnvironment::GetMap()
{
	return mMap;
}

std::shared_ptr<VisualMap> VisualEnvironment::GetVisualMap()
{
	return mMap;
}

void VisualEnvironment::SetVisualPlayer(VisualPlayer* player)
{
	/*
		It is a failure if already is a Visual player
	*/
	LogAssert(mVisualPlayer == NULL, "Visual player already allocated");
	mVisualPlayer = player;
}

void VisualEnvironment::Stop()
{
    mShutdown = true;

    //request all visual managed threads to stop
    mMeshUpdateThread.Stop();
}

bool VisualEnvironment::IsShutdown()
{
    return mShutdown || !mMeshUpdateThread.IsRunning();
}


void VisualEnvironment::Step(float dTime)
{
    mAnimationTime += dTime;
    if (mAnimationTime > 60.0)
        mAnimationTime -= 60.0;

	/* Step time of day */
	StepTimeOfDay(dTime);

	// Get some settings
	bool flyAllowed = CheckLocalPrivilege("fly");
	bool freeMove = flyAllowed && Settings::Get()->GetBool("free_move");

	// Get visual player
	VisualPlayer* visualPlayer = GetPlayer();
	LogAssert(visualPlayer, "invalid visual player");
	// collision info queue
	std::vector<CollisionInfo> playerCollisions;

	/*
		Get the speed the player is going
	*/
	bool isClimbing = visualPlayer->mIsClimbing;
	float playerSpeed = Length(visualPlayer->GetSpeed());

	/*
		Maximum position increment
	*/
	//float positionMaxIncrement = 0.05*BS;
	float positionMaxIncrement = 0.1*BS;

	// Maximum time increment (for collision detection etc)
	// time = distance / speed
	float dTimeMaxIncrement = 1;
	if(playerSpeed > 0.001)
		dTimeMaxIncrement = positionMaxIncrement / playerSpeed;

	// Maximum time increment is 10ms or lower
	if(dTimeMaxIncrement > 0.01)
		dTimeMaxIncrement = 0.01f;

	// Don't allow overly huge dtime
	if(dTime > 0.5)
        dTime = 0.5;

	/*
		Stuff that has a maximum time increment
	*/
	unsigned int steps = (unsigned int)ceil(dTime / dTimeMaxIncrement);
	float dtimePart = dTime / steps;
	for (; steps > 0; --steps) 
    {
		/*
			Visual player handling
		*/

		// Control Visual player
		visualPlayer->ApplyControl(dtimePart);

		// Apply physics
		if (!freeMove && !isClimbing) 
        {
			// Gravity
			Vector3<float> speed = visualPlayer->GetSpeed();
            if (!visualPlayer->mInLiquid)
            {
                speed[1] -= visualPlayer->mMovementGravity *
                    visualPlayer->mPhysicsOverrideGravity * dtimePart * 2.0f;
            }

			// Liquid floating / sinking
			if (visualPlayer->mInLiquid && !visualPlayer->mSwimmingVertical && !visualPlayer->mSwimmingPitch)
				speed[1] -= visualPlayer->mMovementLiquidSink * dtimePart * 2.0f;

			// Liquid resistance
			if (visualPlayer->mInLiquidStable || visualPlayer->mInLiquid) 
            {
				// How much the node's viscosity blocks movement, ranges
				// between 0 and 1. Should match the scale at which viscosity
				// increase affects other liquid attributes.
				static const float viscosityFactor = 0.3f;

				Vector3<float> dWanted = -speed / visualPlayer->mMovementLiquidFluidity;
				float dl = Length(dWanted);
				if (dl > visualPlayer->mMovementLiquidFluiditySmooth)
					dl = visualPlayer->mMovementLiquidFluiditySmooth;

				dl *= (visualPlayer->mLiquidViscosity * viscosityFactor) + (1 - viscosityFactor);
                Normalize(dWanted);
				Vector3<float> d = dWanted * (dl * dtimePart * 100.0f);
				speed += d;
			}

			visualPlayer->SetSpeed(speed);
		}

		/*
			Move the visualPlayer.
			This also does collision detection.
		*/
		visualPlayer->Move(dtimePart, positionMaxIncrement, &playerCollisions);
	}

	bool playerImmortal = visualPlayer->GetVAO() && visualPlayer->GetVAO()->IsImmortal();

	for (const CollisionInfo& info : playerCollisions) 
    {
		Vector3<float> speedDiff = info.newSpeed - info.oldSpeed;
		// Handle only fall damage
		// (because otherwise walking against something in fast_move kills you)
		if (speedDiff[1] < 0 || info.oldSpeed[1] >= 0)
			continue;

		// Get rid of other components
		speedDiff[0] = 0;
		speedDiff[2] = 0;
		float preFactor = 1; // 1 hp per node/s
		float tolerance = BS*14; // 5 without damage
		float postFactor = 1; // 1 hp per node/s
		if (info.type == COLLISION_NODE) 
        {
			const ContentFeatures& f = mNodeMgr->Get(mMap->GetNode(info.node));
			// Determine fall damage multiplier
			int addp = ItemGroupGet(f.groups, "FallDamageAddPercent");
			preFactor = 1.0f + (float)addp / 100.0f;
		}
		float speed = preFactor * Length(speedDiff);
		if (speed > tolerance && !playerImmortal)
        {
			float damageFloat = (speed - tolerance) / BS * postFactor;
			uint16_t damage = (uint16_t)std::min(damageFloat + 0.5f, (float)0xFFFF);
			if (damage != 0) 
            {
				DamageVisualPlayer(damage, true);
                BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataPlayerFallingDamage>());
			}
		}
	}
    
	// Update lighting on player (used for wield item)
	unsigned int dayNightRatio = GetDayNightRatio();
	{
		// Get node at head

		// On InvalidPositionException, use this as default
		// (day: LIGHT_SUN, night: 0)
		MapNode nodeAtVisualPlayer(CONTENT_AIR, 0x0f, 0);

		Vector3<short> p = visualPlayer->GetLightPosition();
		nodeAtVisualPlayer = mMap->GetNode(p);

		uint16_t light = GetInteriorLight(nodeAtVisualPlayer, 0, mNodeMgr.get());
		FinalColorBlend(&visualPlayer->mLightColor, light, dayNightRatio);
	}

	/*
		Step active objects and update lighting of them
	*/

	bool UpdateLighting = mActiveObjectLightUpdateInterval.Step(dTime, 0.21f);
	auto cbState = [this, dTime, UpdateLighting, dayNightRatio] (VisualActiveObject *vao)
    {
		// Step object
		vao->Step(dTime, this);

		if (UpdateLighting)
			vao->UpdateLight(dayNightRatio);
	};

	mAOManager.Step(dTime, cbState);

	/*
		Step and handle simple objects
	*/
	Profiling->Avg("VisualEnv: VSO count [#]", (float)mSimpleObjects.size());
	for (auto i = mSimpleObjects.begin(); i != mSimpleObjects.end();) 
    {
		VisualSimpleObject* simpleObj = *i;

        simpleObj->Step(dTime);
		if(simpleObj->mRemove)
        {
			delete simpleObj;
			i = mSimpleObjects.erase(i);
		}
		else ++i;
	}
}

void VisualEnvironment::AddSimpleObject(VisualSimpleObject* simple)
{
	mSimpleObjects.push_back(simple);
}

GenericVisualActiveObject* VisualEnvironment::GetGenericVAO(uint16_t id)
{
	VisualActiveObject *obj = GetActiveObject(id);
	if (obj && obj->GetType() == ACTIVEOBJECT_TYPE_GENERIC)
		return (GenericVisualActiveObject*) obj;

	return NULL;
}

uint16_t VisualEnvironment::AddActiveObject(VisualActiveObject* object)
{
	// Register object. If failed return zero id
	if (!mAOManager.RegisterObject(object))
		return 0;

	object->AddToScene();

	// Update lighting immediately
	object->UpdateLight(GetDayNightRatio());
	return object->GetId();
}

void VisualEnvironment::AddActiveObject(uint16_t id, uint8_t type, const std::string& initData)
{
	VisualActiveObject* obj = VisualActiveObject::Create((ActiveObjectType) type, this);
	if(obj == NULL)
	{
		LogInformation("VisualEnvironment::AddActiveObject(): id=" + 
            std::to_string(id) + " type=" + std::to_string(type) + ": Couldn't create object");
		return;
	}

	obj->SetId(id);

	try
	{
		obj->Initialize(initData);
	}
	catch(SerializationError& e)
	{
		LogError("VisualEnvironment::AddActiveObject(): id=" + std::to_string(id) + 
            " type=" + std::to_string(type) + ": SerializationError in initialize(): " + 
            e.what() + ": initData=" + SerializeJsonString(initData));
	}

	uint16_t newId = AddActiveObject(obj);
	// Object initialized:
	if (obj = GetActiveObject(newId))
    {
		// Final step is to update all children which are already known
		// Data provided by AO_CMD_SPAWN_INFANT
		const auto& children = obj->GetAttachmentChildIds();
		for (auto child : children)
			if (auto* aObj = GetActiveObject(child))
                aObj->UpdateAttachments();
	}
}

void VisualEnvironment::AddUpdateMeshTask(Vector3<short> position, bool ackToLogic, bool urgent)
{
    // Check if the block exists to begin with. In the case when a non-existing
    // neighbor is automatically added, it may not. In that case we don't want
    // to tell the mesh update thread about it.
    MapBlock* b = GetMap()->GetBlockNoCreateNoEx(position);
    if (b == NULL)
        return;

    mMeshUpdateThread.UpdateBlock(GetMap().get(), position, ackToLogic, urgent);
}

void VisualEnvironment::AddUpdateMeshTaskWithEdge(Vector3<short> blockPos, bool ackToLogic, bool urgent)
{
    try 
    {
        AddUpdateMeshTask(blockPos, ackToLogic, urgent);
    }
    catch (InvalidPositionException&) 
    {
    }

    // Leading edge
    const Vector3<short> dirs[6] =
    {
        // +right, +top, +back
        Vector3<short>{0,0,1}, // back
        Vector3<short>{0,1,0}, // top
        Vector3<short>{1,0,0}, // right
        Vector3<short>{0,0,-1}, // front
        Vector3<short>{0,-1,0}, // bottom
        Vector3<short>{-1,0,0} // left
    };
    for (int i = 0; i < 6; i++)
    {
        try 
        {
            Vector3<short> pos = blockPos + dirs[i];
            AddUpdateMeshTask(pos, false, urgent);
        }
        catch (InvalidPositionException&) 
        {
        }
    }
}

void VisualEnvironment::AddUpdateMeshTaskForNode(Vector3<short> nodePos, bool ackToLogic, bool urgent)
{
    {
        Vector3<short> pos = nodePos;
        LogInformation("AddUpdateMeshTaskForNode(): (" + 
            std::to_string(pos[0]) + "," + 
            std::to_string(pos[1]) + "," + 
            std::to_string(pos[2]) + ")");
    }

    Vector3<short> blockPos = GetNodeBlockPosition(nodePos);
    Vector3<short> blockPosRelative = blockPos * (short)MAP_BLOCKSIZE;

    try 
    {
        AddUpdateMeshTask(blockPos, ackToLogic, urgent);
    }
    catch (InvalidPositionException&) 
    {
    
    }

    // Leading edge
    if (nodePos[0] == blockPosRelative[0]) 
    {
        try 
        {
            Vector3<short> pos = blockPos + Vector3<short>{-1, 0, 0};
            AddUpdateMeshTask(pos, false, urgent);
        }
        catch (InvalidPositionException&) 
        {
        
        }
    }

    if (nodePos[1] == blockPosRelative[1]) 
    {
        try 
        {
            Vector3<short> pos = blockPos + Vector3<short>{0, -1, 0};
            AddUpdateMeshTask(pos, false, urgent);
        }
        catch (InvalidPositionException&) 
        {
        
        }
    }

    if (nodePos[2] == blockPosRelative[2]) 
    {
        try 
        {
            Vector3<short> pos = blockPos + Vector3<short>{0, 0, -1};
            AddUpdateMeshTask(pos, false, urgent);
        }
        catch (InvalidPositionException&) 
        {
        
        }
    }
}

void VisualEnvironment::AddNode(Vector3<short> position, MapNode node, bool removeMetadata)
{
    //TimeTaker timer1("AddNode()");

    std::map<Vector3<short>, MapBlock*> modifiedBlocks;

    try 
    {
        //TimeTaker timer3("addNode(): addNodeAndUpdate");
        GetMap()->AddNodeAndUpdate(position, node, modifiedBlocks, removeMetadata);
    }
    catch (InvalidPositionException&) 
    {

    }

    for (const auto& modifiedBlock : modifiedBlocks)
        AddUpdateMeshTaskWithEdge(modifiedBlock.first, false, true);
}

void VisualEnvironment::RemoveNode(Vector3<short> position)
{
    std::map<Vector3<short>, MapBlock*> modifiedBlocks;

    try 
    {
        GetMap()->RemoveNodeAndUpdate(position, modifiedBlocks);
    }
    catch (InvalidPositionException&) 
    {

    }

    for (const auto& modifiedBlock : modifiedBlocks)
        AddUpdateMeshTaskWithEdge(modifiedBlock.first, false, true);
}

void VisualEnvironment::RemoveActiveObject(uint16_t id)
{
	// Get current attachment childs to detach them visually
	std::unordered_set<int> attachmentChilds;
	if (auto* obj = GetActiveObject(id))
        attachmentChilds = obj->GetAttachmentChildIds();

	mAOManager.RemoveObject(id);

	// Perform a proper detach in game engine
	for (auto childId : attachmentChilds)
		if (VisualActiveObject* child = GetActiveObject(childId))
			child->UpdateAttachments();
}

void VisualEnvironment::ProcessActiveObjectMessage(uint16_t id, const std::string& data)
{
	VisualActiveObject* obj = GetActiveObject(id);
	if (obj == NULL) 
    {
		LogInformation(
            "VisualEnvironment::ProcessActiveObjectMessage(): got message for id=" 
            + std::to_string(id) + ", which doesn't exist.");
		return;
	}

	try 
    {
		obj->ProcessMessage(data);
	} 
    catch (SerializationError& e) 
    {
		LogError(
            "VisualEnvironment::ProcessActiveObjectMessage(): id=" + 
            std::to_string(id) + " type=" + std::to_string(obj->GetType()) + 
            " SerializationError in processMessage(): " + std::string(e.what()));
	}
}

/*
	Callbacks for activeobjects
*/

void VisualEnvironment::DamageVisualPlayer(uint16_t damage, bool handleHP)
{
	VisualPlayer* visualPlayer = GetPlayer();
	LogAssert(visualPlayer, "invalid visual player");

	if (handleHP) 
    {
		if (visualPlayer->mHp > damage)
			visualPlayer->mHp -= damage;
		else
			visualPlayer->mHp = 0;
	}

	VisualEnvironmentEvent evt;
    evt.type = VEE_PLAYER_DAMAGE;
    evt.playerDamage.amount = damage;
    evt.playerDamage.sendToLogic = handleHP;
	mVisualEventQueue.push(evt);
}

/*
	Visual likes to call these
*/

VisualEnvironmentEvent VisualEnvironment::GetEnvironmentEvent()
{
	LogAssert(mVisualEventQueue.empty(),
			"VisualEnvironment::GetEnvironmentEvent(): queue is empty");

	VisualEnvironmentEvent evt = mVisualEventQueue.front();
	mVisualEventQueue.pop();
	return evt;
}

void VisualEnvironment::GetSelectedActiveObjects(
	const Line3<float>& shootlineOnMap, std::vector<PointedThing>& objects)
{
	std::vector<DistanceSortedActiveObject> allObjects;
	GetActiveObjects(shootlineOnMap.mStart,
		Length(shootlineOnMap.mStart - shootlineOnMap.mEnd) + 10.0f, allObjects);
	
    const Vector3<float> lineVector = shootlineOnMap.GetVector();
	for (const auto& allObject : allObjects) 
    {
		VisualActiveObject* obj = allObject.mObj;
		BoundingBox<float> selectionBox;
		if (!obj->GetSelectionBox(&selectionBox))
			continue;

		const Vector3<float>& pos = obj->GetPosition();
        BoundingBox<float> offsettedBox(selectionBox.mMinEdge + pos, selectionBox.mMaxEdge + pos);

		Vector3<float> currentIntersection;
		Vector3<short> currentNormal;
		if (BoxLineCollision(offsettedBox, shootlineOnMap.mStart, lineVector, &currentIntersection, &currentNormal)) 
        {
			objects.emplace_back((short)obj->GetId(), currentIntersection, currentNormal,
				LengthSq(currentIntersection - shootlineOnMap.mStart));
		}
	}
}

Inventory* VisualEnvironment::GetInventory(const InventoryLocation& loc)
{
    switch (loc.type) 
    {
        case InventoryLocation::UNDEFINED:
        {

        }
        break;
        case InventoryLocation::CURRENT_PLAYER:
        {
            VisualPlayer* player = GetPlayer();
            LogAssert(player, "invalid player");
            return &player->mInventory;
        }
        break;
        case InventoryLocation::PLAYER:
        {
            // Check if we are working with player inventory
            VisualPlayer* player = GetPlayer();
            if (!player || strcmp(player->GetName(), loc.name.c_str()) != 0)
                return NULL;
            return &player->mInventory;
        }
        break;
        case InventoryLocation::NODEMETA:
        {
            MapNodeMetadata* meta = GetMap()->GetMapNodeMetadata(loc.nodePosition);
            if (!meta)
                return NULL;
            return meta->GetInventory();
        }
        break;
        case InventoryLocation::DETACHED:
        {
            if (mDetachedInventories.count(loc.name) == 0)
                return NULL;
            return mDetachedInventories[loc.name];
        }
        break;
        default:
            LogError("Invalid inventory location type.");
            break;
    }
    return NULL;
}

void VisualEnvironment::DoInventoryAction(InventoryAction* action)
{
    std::ostringstream os(std::ios_base::binary);
    action->Serialize(os);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHandleInventoryAction>(GetPlayer()->GetId(), os.str()));

    // Predict some inventory changes
    action->Apply(this, this);

    // Remove it
    delete action;
}

void VisualEnvironment::HandleDetachedInventory(
    const std::string& name, const std::string& content, bool keepInv)
{
    LogInformation("Detached inventory update: \"" + name + 
        "\", mode= " + (keepInv ? "update" : "remove"));

    const auto& invIt = mDetachedInventories.find(name);
    if (!keepInv) 
    {
        if (invIt != mDetachedInventories.end()) 
        {
            delete invIt->second;
            mDetachedInventories.erase(invIt);
        }
        return;
    }

    Inventory *inv = nullptr;
    if (invIt == mDetachedInventories.end()) 
    {
        inv = new Inventory(mItemMgr.get());
        mDetachedInventories[name] = inv;
    }
    else inv = invIt->second;

    std::istringstream is(content, std::ios::binary);
    inv->Deserialize(is);
}

float VisualEnvironment::GetAnimationTime()
{
    return mAnimationTime;
}

int VisualEnvironment::GetCrackLevel()
{
    return mCrackLevel;
}

Vector3<short> VisualEnvironment::GetCrackPosition()
{
    return mCrackPosition;
}

void VisualEnvironment::SetCrack(int level, Vector3<short> pos)
{
    int oldCrackLevel = mCrackLevel;
    Vector3<short> oldCrackPosition = mCrackPosition;

    mCrackLevel = level;
    mCrackPosition = pos;

    if (oldCrackLevel >= 0 && (level < 0 || pos != oldCrackPosition))
    {
        // remove old crack
        AddUpdateMeshTaskForNode(oldCrackPosition, false, true);
    }
    if (level >= 0 && (oldCrackLevel < 0 || pos != oldCrackPosition))
    {
        // add new crack
        AddUpdateMeshTaskForNode(pos, false, true);
    }
}

uint16_t VisualEnvironment::GetHP()
{
    VisualPlayer* player = GetPlayer();
    LogAssert(player, "invalid player");
    return player->mHp;
}