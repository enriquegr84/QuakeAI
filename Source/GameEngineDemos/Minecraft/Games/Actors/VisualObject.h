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

#ifndef VISUALOBJECT_H
#define VISUALOBJECT_H

#include "ActiveObject.h"

class Node;
class AnimatedObjectMeshNode;
class VisualEnvironment;
class BaseTextureSource;
struct ItemStack;

class VisualActiveObject : public ActiveObject
{
public:
	VisualActiveObject(ActorId id, VisualEnvironment* vEnv);
	virtual ~VisualActiveObject();

	virtual void AddToScene() {}
	virtual void RemoveFromScene(bool permanent) {}

	virtual void UpdateLight(unsigned int dayNightRatio) {}

	virtual bool GetCollisionBox(BoundingBox<float>* toset) const { return false; }
	virtual bool GetSelectionBox(BoundingBox<float>* toset) const { return false; }
	virtual bool CollideWithObjects() const { return false; }
	virtual const Vector3<float> GetPosition() const { return Vector3<float>::Zero(); }
	virtual std::shared_ptr<Node> GetSceneNode() const { return NULL; }
	virtual std::shared_ptr<AnimatedObjectMeshNode> GetAnimatedMeshSceneNode() const { return NULL; }
	virtual bool IsVisualPlayer() const { return false; }

	virtual VisualActiveObject* GetParent() const { return nullptr; };
	virtual const std::unordered_set<int>& GetAttachmentChildIds() const
	{ static std::unordered_set<int> rv; return rv; }
	virtual void UpdateAttachments() {};

	virtual bool DoShowSelectionBox() { return true; }

	// Step object in time
	virtual void Step(float dTime, VisualEnvironment* env) {}

	// Process a message sent by the logic side object
	virtual void ProcessMessage(const std::string& data) {}

	virtual std::string InfoText() { return ""; }
	virtual std::string DebugInfoText() { return ""; }

	/*
		This takes the return value of
		LogicActiveObject::GetVisualInitializationData
	*/
	virtual void Initialize(const std::string& data) {}

	// Create a certain type of VisualActiveObject
	static VisualActiveObject* Create(ActiveObjectType type, VisualEnvironment* env);

	virtual bool DirectReportPunch(Vector3<float> dir, const ItemStack* punchItem = nullptr,
		float timeFromLastPunch = 1000000) { return false; }

protected:
	// Used for creating objects based on type
	typedef VisualActiveObject *(*Factory)(VisualEnvironment *env);
	static void RegisterType(unsigned short type, Factory f);

	VisualEnvironment* mEnvironment;
private:
	// Used for creating objects based on type
	static std::unordered_map<unsigned short, Factory> mTypes;
};

class DistanceSortedActiveObject
{
public:
	VisualActiveObject* mObj;

	DistanceSortedActiveObject(VisualActiveObject* obj, float distance)
	{
		mObj = obj;
		mDist = distance;
	}

	bool operator < (const DistanceSortedActiveObject& other) const
	{
		return mDist < other.mDist;
	}

private:
	float mDist;
};

#endif
