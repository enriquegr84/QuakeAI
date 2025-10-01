/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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


#ifndef STATICOBJECT_H
#define STATICOBJECT_H

#include "../../MinecraftStd.h"

#include "Core/Logger/Logger.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

class LogicActiveObject;

struct StaticObject
{
	char type = 0;
	Vector3<float> position;
	std::string data;

	StaticObject() = default;
	StaticObject(const LogicActiveObject* sObj, const Vector3<float>& pos);

	void Serialize(std::ostream& os);
	void Deserialize(std::istream& is, char version);
};

class StaticObjectList
{
public:
	/*
		Inserts an object to the container.
		Id must be unique (active) or 0 (stored).
	*/
	void Insert(uint16_t id, const StaticObject& obj)
	{
		if(id == 0)
		{
			mStored.push_back(obj);
		}
		else
		{
			if(mActive.find(id) != mActive.end())
				LogError("StaticObjectList::insert(): id already exists");

			mActive[id] = obj;
		}
	}

	void Remove(uint16_t id)
	{
		LogAssert(id != 0, "invalid object"); // Pre-condition
		if(mActive.find(id) == mActive.end())
		{
			LogWarning("StaticObjectList::remove(): id=" + 
                std::to_string(id) + " not found");
			return;
		}
		mActive.erase(id);
	}

	void Serialize(std::ostream& os);
	void Deserialize(std::istream& is);

	/*
		NOTE: When an object is transformed to active, it is removed
		from mStored and inserted to mActive.
		The caller directly manipulates these containers.
	*/
	std::vector<StaticObject> mStored;
	std::map<uint16_t, StaticObject> mActive;
};

#endif
