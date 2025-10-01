/*
Minetest
Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

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

#include "ActiveObjectManager.h"

#include "../Map/MapBlock.h"

#include "Core/Logger/Logger.h"
#include "Core/Utility/Profiler.h"

void VisualActiveObjectManager::Clear()
{
	// delete active objects
	for (auto& activeObject : mActiveObjects) 
    {
		delete activeObject.second;
		// Object must be marked as gone when children try to detach
		activeObject.second = nullptr;
	}
	mActiveObjects.clear();
}

void VisualActiveObjectManager::Step(float dTime, const std::function<void(VisualActiveObject *)>& func)
{
    Profiling->Avg("ActiveObjectManager: VAO count [#]", (float)mActiveObjects.size());
	for (auto& aoIt : mActiveObjects) 
        func(aoIt.second);
}

// clang-format off
bool VisualActiveObjectManager::RegisterObject(VisualActiveObject* obj)
{
	LogAssert(obj, "invalid object"); // Pre-condition
	if (obj->GetId() == 0) 
    {
		uint16_t newId = GetFreeId();
		if (newId == 0) 
        {
			LogInformation("ActiveObjectManager::registerObject(): no free id available");

			delete obj;
			return false;
		}
		obj->SetId(newId);
	}

	if (!IsFreeId(obj->GetId())) 
    {
		LogInformation("ActiveObjectManager::registerObject(): "
            "id is not free (" + std::to_string(obj->GetId()) + ")");
		delete obj;
		return false;
	}
    LogInformation("ActiveObjectManager::registerObject(): "
        "added (id=" + std::to_string(obj->GetId()) + ")");

	mActiveObjects[obj->GetId()] = obj;
	return true;
}

void VisualActiveObjectManager::RemoveObject(uint16_t id)
{
	VisualActiveObject* obj = GetActiveObject(id);
	if (!obj) 
    {
		LogError("ActiveObjectManager::RemoveObject(): id=" + std::to_string(id) + " not found");
		return;
	}

    LogInformation("ActiveObjectManager::RemoveObject(): id=" + std::to_string(id));
	mActiveObjects.erase(id);
	obj->RemoveFromScene(true);
	delete obj;
}

// clang-format on
void VisualActiveObjectManager::GetActiveObjects(const Vector3<float>& origin, float maxDist,
		std::vector<DistanceSortedActiveObject>& dest)
{
	float maxDist2 = maxDist * maxDist;
	for (auto& aoIt : mActiveObjects) 
    {
		VisualActiveObject* obj = aoIt.second;
		float dist2 = LengthSq(obj->GetPosition() - origin);
		if (dist2 > maxDist2)
			continue;

		dest.emplace_back(obj, dist2);
	}
}

void LogicActiveObjectManager::Clear(const std::function<bool(LogicActiveObject*, uint16_t)>& cb)
{
    std::vector<uint16_t> objectsToRemove;
    for (auto& it : mActiveObjects) 
    {
        if (cb(it.second, it.first)) 
        {
            // Id to be removed from mActiveObjects
            objectsToRemove.push_back(it.first);
        }
    }

    // Remove references from mActiveObjects
    for (uint16_t objId : objectsToRemove)
        mActiveObjects.erase(objId);
}

void LogicActiveObjectManager::Step(float dTime, const std::function<void(LogicActiveObject *)>& f)
{
    Profiling->Avg("ActiveObjectManager: LAO count [#]", (float)mActiveObjects.size());
    for (auto &ao_it : mActiveObjects)
        f(ao_it.second);
}

// clang-format off
bool LogicActiveObjectManager::RegisterObject(LogicActiveObject* obj)
{
    LogAssert(obj, "invalid playerLAO"); // Pre-condition
    if (obj->GetId() == 0) 
    {
        uint16_t newId = GetFreeId();
        if (newId == 0) 
        {
            LogWarning("ActiveObjectManager::AddActiveObjectRaw(): no free id available");
            if (obj->EnvironmentDeletes())
                delete obj;
            return false;
        }
        obj->SetId(newId);
    }
    else 
        LogInformation("ActiveObjectManager::AddActiveObjectRaw(): supplied with id " + 
            std::to_string(obj->GetId()));

    if (!IsFreeId(obj->GetId())) 
    {
        LogWarning("ActiveObjectManager::AddActiveObjectRaw(): id is not free (" + 
            std::to_string(obj->GetId()) + ")");
        if (obj->EnvironmentDeletes())
            delete obj;
        return false;
    }
    
    if (ObjectPositionOverLimit(obj->GetBasePosition())) 
    {
        Vector3<float> p = obj->GetBasePosition();
        LogInformation("ActiveObjectManager::AddActiveObjectRaw(): object position (" +
            std::to_string(p[0]) + "," + std::to_string(p[1]) + "," + std::to_string(p[2]) + 
            ") outside maximum range");
        if (obj->EnvironmentDeletes())
            delete obj;
        return false;
    }

    mActiveObjects[obj->GetId()] = obj;

    LogInformation(
        "ActiveObjectManager::AddActiveObjectRaw(): Added id=" + 
        std::to_string(obj->GetId()) + "; there are now " + 
        std::to_string(mActiveObjects.size()) + " active objects.");
    return true;
}

void LogicActiveObjectManager::RemoveObject(uint16_t id)
{
    LogicActiveObject* obj = GetActiveObject(id);
    if (!obj) 
    {
        LogError("ActiveObjectManager::RemoveObject(): id=" + std::to_string(id) + " not found");
        return;
    }

    LogInformation(
        "ActiveObjectManager::RemoveObject(): id=" + std::to_string(id));
    mActiveObjects.erase(id);
    delete obj;
}

// clang-format on
void LogicActiveObjectManager::GetObjectsInsideRadius(const Vector3<float>& pos, float radius,
    std::vector<LogicActiveObject *>& result, std::function<bool(LogicActiveObject *obj)> includeObjCB)
{
    float r2 = radius * radius;
    for (auto& activeObject : mActiveObjects) 
    {
        LogicActiveObject* obj = activeObject.second;
        const Vector3<float>& objectPos = obj->GetBasePosition();
        if (LengthSq(objectPos - pos) > r2)
            continue;

        if (!includeObjCB || includeObjCB(obj))
            result.push_back(obj);
    }
}

void LogicActiveObjectManager::GetObjectsInArea(const BoundingBox<float>& box,
    std::vector<LogicActiveObject *>& result, std::function<bool(LogicActiveObject *obj)> includeObjCB)
{
    for (auto& activeObject : mActiveObjects) 
    {
        LogicActiveObject *obj = activeObject.second;
        const Vector3<float>& objectPos = obj->GetBasePosition();
        if (!box.IsPointInside(objectPos))
            continue;

        if (!includeObjCB || includeObjCB(obj))
            result.push_back(obj);
    }
}

void LogicActiveObjectManager::GetAddedActiveObjectsAroundPosition(
    const Vector3<float>& playerPos, float radius, float playerRadius, 
    std::set<uint16_t>& currentObjects, std::queue<uint16_t>& addedObjects)
{
    /*
        Go through the object list,
        - discard removed/deactivated objects,
        - discard objects that are too far away,
        - discard objects that are found in current_objects.
        - add remaining objects to added_objects
    */
    for (auto& aoIt : mActiveObjects) 
    {
        uint16_t id = aoIt.first;

        // Get object
        LogicActiveObject* object = aoIt.second;
        if (!object)
            continue;

        if (object->IsGone())
            continue;

        float distance = Length(object->GetBasePosition() - playerPos);
        if (object->GetType() == ACTIVEOBJECT_TYPE_PLAYER) 
        {
            // Discard if too far
            if (distance > playerRadius && playerRadius != 0)
                continue;
        }
        else if (distance > radius)
            continue;

        // Discard if already on current_objects
        auto obj = currentObjects.find(id);
        if (obj != currentObjects.end())
            continue;

        // Add to added_objects
        addedObjects.push(id);
    }
}