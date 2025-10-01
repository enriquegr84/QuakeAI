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


#ifndef ACTIVEOBJECTMANAGER_H
#define ACTIVEOBJECTMANAGER_H

#include "../../MinecraftStd.h"

#include "VisualObject.h"
#include "LogicActiveObject.h"

template <typename T>
class ActiveObjectManager
{
public:

	virtual void Step(float dTime, const std::function<void(T *)> &f) = 0;
	virtual bool RegisterObject(T *obj) = 0;
	virtual void RemoveObject(uint16_t id) = 0;

	T* GetActiveObject(uint16_t id)
	{
		typename std::unordered_map<uint16_t, T*>::const_iterator n = mActiveObjects.find(id);
		return (n != mActiveObjects.end() ? n->second : nullptr);
	}

protected:

	uint16_t GetFreeId() const
	{
		// try to reuse id's as late as possible
		static thread_local uint16_t lastUsedId = 0;
		uint16_t startId = lastUsedId;
		while (!IsFreeId(++lastUsedId)) 
			if (lastUsedId == startId)
				return 0;

		return lastUsedId;
	}

	bool IsFreeId(uint16_t id) const
	{
		return id != 0 && mActiveObjects.find(id) == mActiveObjects.end();
	}

	std::unordered_map<uint16_t, T*> mActiveObjects;
};

class VisualActiveObjectManager : public ::ActiveObjectManager<VisualActiveObject>
{
public:
    void Clear();
    void Step(float dTime, const std::function<void(VisualActiveObject *)> &f) override;
    bool RegisterObject(VisualActiveObject *obj) override;
    void RemoveObject(uint16_t id) override;

    void GetActiveObjects(const Vector3<float>& origin, float maxDistance,
        std::vector<DistanceSortedActiveObject>& dest);
};

class LogicActiveObjectManager : public ::ActiveObjectManager<LogicActiveObject>
{
public:
    void Clear(const std::function<bool(LogicActiveObject *, uint16_t)> &cb);
    void Step(float dTime, const std::function<void(LogicActiveObject *)> &f) override;
    bool RegisterObject(LogicActiveObject *obj) override;
    void RemoveObject(uint16_t id) override;

    void GetObjectsInsideRadius(const Vector3<float>& pos, float radius,
        std::vector<LogicActiveObject *>& result,
        std::function<bool(LogicActiveObject *obj)> includeObjCB);
    void GetObjectsInArea(const BoundingBox<float>& box,
        std::vector<LogicActiveObject *>& result,
        std::function<bool(LogicActiveObject *obj)> include_obj_cb);

    void GetAddedActiveObjectsAroundPosition(const Vector3<float>& playerPos, float radius,
        float playerRadius, std::set<uint16_t>& currentObjects, std::queue<uint16_t>& addedObjects);
};

#endif