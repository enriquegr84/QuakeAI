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

#ifndef OBJECT_H
#define OBJECT_H

#include "GameEngineStd.h"

#include "Core/OS/OS.h"

#define OBJ_INVALID_INDEX ((unsigned int)(-1))
#define OBJ_INVALID_HANDLE 0
#define OBJ_HANDLE_SALT 0x00585e6fu
#define OBJ_MAX_ITEMS (1 << 18)
#define OBJ_UID_MASK ((1 << 7) - 1)

class Environment;

typedef unsigned int ObjectHandle;

enum ObjectType 
{
	OBJ_GENERIC,
	OBJ_BIOME,
	OBJ_ORE,
	OBJ_DECORATION,
	OBJ_SCHEMATIC,
};

class Object 
{
public:
	virtual ~Object() = default;

	// Only implemented by child classes (leafs in class hierarchy)
	// Should create new object of its own type, call CloneTo() of parent class
	// and copy its own instance variables over
	virtual Object* Clone() const = 0;

	unsigned int mIndex;
	unsigned int mUId;
	ObjectHandle mHandle;
	std::string mName;

protected:
	// Only implemented by classes that have children themselves
	// by copying the defintion and changing that argument type (!!!)
	// Should defer to parent class CloneTo() if applicable and then copy
	// over its own properties
	void CloneTo(Object* obj) const;
};

// WARNING: Ownership of Objects is transferred to the ObjectManager it is
// added/set to.  Note that Objects managed by ObjectManager are NOT refcounted,
// so the same Object instance must not be referenced multiple
// TODO: const correctness for getter methods
class ObjectManager 
{
public:
	ObjectManager(Environment* env, ObjectType type);
	virtual ~ObjectManager();

	// T *clone() const; // implemented in child class with correct type

	virtual const char* GetObjectTitle() const { return "Object"; }

	virtual void Clear();
	virtual Object* GetByName(const std::string& name) const;

	//// Add new/get/set object definitions by handle
	virtual ObjectHandle Add(Object* obj);
	virtual Object* Get(ObjectHandle handle) const;
	virtual Object* Set(ObjectHandle handle, Object* obj);

	//// Raw variants that work on indexes
	virtual unsigned int AddRaw(Object *obj);

	// It is generally assumed that GetRaw() will always return a valid object
	// This won't be true if people do odd things such as call SetRaw() with NULL
	virtual Object* GetRaw(unsigned int index) const;
	virtual Object* SetRaw(unsigned int index, Object *obj);

	size_t GetNumObjects() const { return mObjects.size(); }
	ObjectType GetType() const { return mObjectType; }

	unsigned int ValidateHandle(ObjectHandle handle) const;
	static ObjectHandle CreateHandle(
        unsigned int index, ObjectType type, unsigned int uid);
	static bool DecodeHandle(ObjectHandle handle, 
        unsigned int* index, ObjectType* type, unsigned int* uid);

protected:
	ObjectManager() {};
	// Helper for child classes to implement clone()
	void CloneTo(ObjectManager* mgr) const;

	Environment* mEnvironment;
	std::vector<Object*> mObjects;
	ObjectType mObjectType;

private:

    PcgRandom mPcgRand;
};

#endif