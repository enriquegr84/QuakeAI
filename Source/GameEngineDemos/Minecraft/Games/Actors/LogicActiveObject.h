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


#ifndef LOGICACTIVEOBJECT_H
#define LOGICACTIVEOBJECT_H

#include "ActiveObject.h"
#include "InventoryManager.h"
#include "Item.h"

/*

Some planning
-------------
    Logic environment adds an active object, which gets the id 1
    The active object list is scanned for each visual once in a while,
    and it finds out what objects have been added that are not known by the visual yet.
    A network packet is created with the info and sent to the visual.
    Environment converts objects to static data and static data to
    objects, based on how close players are to them.
*/

class LogicEnvironment;
struct ItemStack;
struct ToolCapabilities;
struct ObjectProperties;
struct PlayerHPChangeReason;

class LogicActiveObject : public ActiveObject
{
public:
	/*
		NOTE: mEnvironment can be NULL, but step() isn't called if it is.
		Prototypes are used that way.
	*/
	LogicActiveObject(LogicEnvironment* env, Vector3<float> pos);
	virtual ~LogicActiveObject() = default;

	virtual ActiveObjectType GetSendType() const { return GetType(); }

	// Called after id has been set and has been inserted in environment
	virtual void AddedToEnvironment(unsigned int dTime){};
	// Called before removing from environment
	virtual void RemovingFromEnvironment(){};
	// Returns true if object's deletion is the job of the
	// environment
	virtual bool EnvironmentDeletes() const { return true; }

	// Safely mark the object for removal or deactivation
	void MarkForRemoval();
	void MarkForDeactivation();

	// Create a certain type of LogicActiveObject
	static LogicActiveObject* Create(
        ActiveObjectType type, LogicEnvironment* env, 
        uint16_t id, Vector3<float> pos, const std::string &data);

	/*
		Some simple getters/setters
	*/
	Vector3<float> GetBasePosition() const { return mBasePosition; }
	void SetBasePosition(Vector3<float> pos){ mBasePosition = pos; }
	LogicEnvironment* GetEnvironment(){ return mEnvironment; }

	/*
		Some more dynamic interface
	*/

	virtual void SetPosition(const Vector3<float>& pos) { SetBasePosition(pos); }
	// continuous: if true, object does not stop immediately at pos
	virtual void MoveTo(Vector3<float> pos, bool continuous) { SetBasePosition(pos); }
	// If object has moved less than this and data has not changed,
	// saving to disk may be omitted
	virtual float GetMinimumSavedMovement();

	virtual std::string GetDescription(){return "LAO";}

	/*
		Step object in time.
		Messages added to messages are sent to visual over network.

		sendRecommended:
			True at around 5-10 times a second, same for all objects.
			This is used to let objects send most of the data at the
			same time so that the data can be combined in a single
			packet.
	*/
	virtual void Step(float dTime, bool sendRecommended){}

	/*
	The return value of this is passed to the visual-side object
	when it is created
*/
	virtual std::string GetVisualInitializationData() { return ""; }

	/*
		The return value of this is passed to the logic object
		when it is created (converted from static to active - actually
		the data is the static form)
	*/
	virtual void GetStaticData(std::string* result) const
	{
		LogAssert(IsStaticAllowed(), "invalid static data");
		*result = "";
	}

	/*
		Return false in here to never save and instead remove object
		on unload. GetStaticData() will not be called in that case.
	*/
	virtual bool IsStaticAllowed() const {return true;}

	/*
		Return false here to never unload the object.
		IsStaticAllowed && ShouldUnload -> unload when out of active block range
		!IsStaticAllowed && ShouldUnload -> unload when block is unloaded
	*/
	virtual bool ShouldUnload() const { return true; }

	// Returns tool wear
	virtual uint16_t Punch(Vector3<float> dir, const ToolCapabilities* toolcap = nullptr,
        LogicActiveObject* puncher = nullptr, float timeFromLastPunch = 1000000.0f) 
    { return 0; }

	virtual void RightClick(LogicActiveObject* clicker) {}
	virtual void SetHP(int hp, const PlayerHPChangeReason& reason) {}
	virtual uint16_t GetHP() const { return 0; }

	virtual void SetArmorGroups(const ItemGroupList& armorGroups) {}
	virtual const ItemGroupList& GetArmorGroups() const { static ItemGroupList rv; return rv; }
	virtual void SetAnimation(Vector2<float> frames, float frameSpeed, float frameBlend, bool frameLoop) {}
	virtual void GetAnimation(Vector2<float>* frames, float* frameSpeed, float* frameBlend, bool* frameLoop) {}
	virtual void SetAnimationSpeed(float frameSpeed) {}
	virtual void SetBonePosition(const std::string& bone, Vector3<float> position, Vector3<float> rotation) {}
	virtual void GetBonePosition(const std::string& bone, Vector3<float>* position, Vector3<float>* lotation) {}
	virtual const std::unordered_set<int>& GetAttachmentChildIds() const
	{ static std::unordered_set<int> rv; return rv; }
	virtual LogicActiveObject* GetParent() const { return nullptr; }
	virtual ObjectProperties* AccessObjectProperties() { return nullptr; }
	virtual void NotifyObjectPropertiesModified() {}

	// Inventory and wielded item
	virtual Inventory* GetInventory() const { return NULL; }
	virtual InventoryLocation GetInventoryLocation() const { return InventoryLocation(); }
	virtual void SetInventoryModified() {}
	virtual std::string GetWieldList() const { return ""; }
	virtual uint16_t GetWieldIndex() const { return 0; }
	virtual ItemStack GetWieldedItem(ItemStack* selected, ItemStack* hand = nullptr) const;
	virtual bool SetWieldedItem(const ItemStack& item);
	inline void AttachParticleSpawner(unsigned int id)
	{ mAttachedParticleSpawners.insert(id); }
	inline void DetachParticleSpawner(unsigned int id)
	{
		mAttachedParticleSpawners.erase(id);
	}

	std::string GenerateUpdateInfantCommand(uint16_t infantId);

	void DumpAOMessagesToQueue(std::queue<ActiveObjectMessage>&queue);

	/*
		Number of players which know about this object. Object won't be
		deleted until this is 0 to keep the id preserved for the right
		object.
	*/
    uint16_t mKnownByCount = 0;

	/*
		A getter that unifies the above to answer the question:
		"Can the environment still interact with this object?"
	*/
	inline bool IsGone() const { return mPendingRemoval || mPendingDeactivation; }

	inline bool IsPendingRemoval() const { return mPendingRemoval; }

	/*
		Whether the object's static data has been stored to a block
	*/
	bool mStaticExists = false;
	/*
		The block from which the object was loaded from, and in which
		a copy of the static data resides.
	*/
    Vector3<short> mStaticBlock = Vector3<short>{ 1337, 1337, 1337 };

protected:
	virtual void OnMarkedForDeactivation() {}
	virtual void OnMarkedForRemoval() {}

	virtual void OnAttach(int parentId) {}
	virtual void OnDetach(int parentId) {}

	LogicEnvironment* mEnvironment;
	Vector3<float> mBasePosition;
	std::unordered_set<unsigned int> mAttachedParticleSpawners;

	/*
		Same purpose as mPendingRemoval but for deactivation.
		deactvation = save static data in block, remove active object

		If this is set alongside with mPendingRemoval, removal takes
		priority.
		Note: Do not assign this directly, use MarkForDeactivation() instead.
	*/
	bool mPendingDeactivation = false;

	/*
		- Whether this object is to be removed when nobody knows about
		  it anymore.
		- Removal is delayed to preserve the id for the time during which
		  it could be confused to some other object by visual.
		- This is usually set to true by the step() method when the object wants
		  to be deleted but can be set by anything else too.
		Note: Do not assign this directly, use MarkForRemoval() instead.
	*/
	bool mPendingRemoval = false;

	/*
		Queue of messages to be sent to the visual
	*/
	std::queue<ActiveObjectMessage> mMessagesOut;
};

#endif