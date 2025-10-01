/*
Minetest
Copyright (C) 2010-2015 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#include "Object.h"

#include "Core/Logger/Logger.h"

#include "Mathematic/Arithmetic/BitHacks.h"

#include "../Environment/Environment.h"


ObjectManager::ObjectManager(Environment* env, ObjectType type) :
	mEnvironment(env), mObjectType(type)
{

}


ObjectManager::~ObjectManager()
{
	for (size_t i = 0; i != mObjects.size(); i++)
		delete mObjects[i];
}


ObjectHandle ObjectManager::Add(Object* obj)
{
	LogAssert(obj, "invalid object");

	if (obj->mName.length() && GetByName(obj->mName))
		return OBJ_INVALID_HANDLE;

	unsigned int index = AddRaw(obj);
	if (index == OBJ_INVALID_INDEX)
		return OBJ_INVALID_HANDLE;

	obj->mHandle = CreateHandle(index, mObjectType, obj->mUId);
	return obj->mHandle;
}


Object* ObjectManager::Get(ObjectHandle handle) const
{
	unsigned int index = ValidateHandle(handle);
	return (index != OBJ_INVALID_INDEX) ? GetRaw(index) : NULL;
}


Object* ObjectManager::Set(ObjectHandle handle, Object* obj)
{
	unsigned int index = ValidateHandle(handle);
	if (index == OBJ_INVALID_INDEX)
		return NULL;

	Object* oldobj = SetRaw(index, obj);

	obj->mUId = oldobj->mUId;
	obj->mIndex = oldobj->mIndex;
	obj->mHandle = oldobj->mHandle;

	return oldobj;
}


unsigned int ObjectManager::AddRaw(Object* obj)
{
	unsigned int nObjects = (unsigned int)mObjects.size();
	if (nObjects >= OBJ_MAX_ITEMS)
		return -1;

	obj->mIndex = nObjects;

	// Ensure UID is nonzero so that a valid handle == OBJ_INVALID_HANDLE
	// is not possible.  The slight randomness bias isn't very significant.
	obj->mUId = mPcgRand.Next() & OBJ_UID_MASK;
	if (obj->mUId == 0)
		obj->mUId = 1;

	mObjects.push_back(obj);

	LogInformation("ObjectManager: added " + std::string(GetObjectTitle()) + 
        ": name=\"" + obj->mName + "\" index=" + std::to_string(obj->mIndex) + 
        " uid=" + std::to_string(obj->mUId));

	return nObjects;
}


Object* ObjectManager::GetRaw(unsigned int index) const
{
	return mObjects[index];
}


Object* ObjectManager::SetRaw(unsigned int index, Object* obj)
{
	Object* oldObj = mObjects[index];
	mObjects[index] = obj;
	return oldObj;
}


Object* ObjectManager::GetByName(const std::string& name) const
{
	for (size_t i = 0; i != mObjects.size(); i++) 
    {
		Object* obj = mObjects[i];
		if (obj && !stricmp(name.c_str(), obj->mName.c_str()))
			return obj;
	}

	return NULL;
}


void ObjectManager::Clear()
{
	for (size_t i = 0; i != mObjects.size(); i++)
		delete mObjects[i];

	mObjects.clear();
}


unsigned int ObjectManager::ValidateHandle(ObjectHandle handle) const
{
	ObjectType type;
	unsigned int index;
	unsigned int uid;

	bool isValid =
		(handle != OBJ_INVALID_HANDLE) &&
		DecodeHandle(handle, &index, &type, &uid) &&
		(type == mObjectType) &&
		(index < mObjects.size()) &&
		(mObjects[index]->mUId == uid);

	return isValid ? index : -1;
}


ObjectHandle ObjectManager::CreateHandle(unsigned int index, ObjectType type, unsigned int uid)
{
	ObjectHandle handle = 0;
	SetBits(&handle, 0, 18, index);
    SetBits(&handle, 18, 6, type);
    SetBits(&handle, 24, 7, uid);

	unsigned int parity = CalculateParity(handle);
    SetBits(&handle, 31, 1, parity);

	return handle ^ OBJ_HANDLE_SALT;
}


bool ObjectManager::DecodeHandle(ObjectHandle handle, unsigned int* index, ObjectType* type, unsigned int* uid)
{
	handle ^= OBJ_HANDLE_SALT;

	unsigned int parity = GetBits(handle, 31, 1);
    SetBits(&handle, 31, 1, 0);
	if (parity != CalculateParity(handle))
		return false;

	*index = GetBits(handle, 0, 18);
	*type = (ObjectType)GetBits(handle, 18, 6);
	*uid = GetBits(handle, 24, 7);
	return true;
}

// Cloning

void Object::CloneTo(Object* obj) const
{
    obj->mIndex = mIndex;
    obj->mUId = mUId;
    obj->mHandle = mHandle;
    obj->mName = mName;
}

void ObjectManager::CloneTo(ObjectManager* mgr) const
{
	mgr->mObjects.reserve(mObjects.size());
	for (const auto& obj : mObjects)
		mgr->mObjects.push_back(obj->Clone());
	mgr->mObjectType = mObjectType;
}
