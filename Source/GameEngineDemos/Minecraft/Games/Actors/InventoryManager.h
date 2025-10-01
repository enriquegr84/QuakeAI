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

#ifndef INVENTORYMANAGER_H
#define INVENTORYMANAGER_H

#include "Inventory.h"

#include "Mathematic/Algebra/Vector3.h"

class Environment;
class LogicEnvironment;
class LogicActiveObject;

struct InventoryLocation
{
	enum Type
    {
		UNDEFINED,
		CURRENT_PLAYER,
		PLAYER,
		NODEMETA,
        DETACHED,
	} type;

	std::string name; // PLAYER, DETACHED
	Vector3<short> nodePosition; // NODEMETA

	InventoryLocation()
	{
		nodePosition = Vector3<short>::Zero();
		SetUndefined();
	}
	void SetUndefined()
	{
		type = UNDEFINED;
	}
	void SetCurrentPlayer()
	{
		type = CURRENT_PLAYER;
	}
	void SetPlayer(const std::string& n)
	{
		type = PLAYER;
		name = n;
	}
	void SetNodeMeta(const Vector3<short>& position)
	{
		type = NODEMETA;
        nodePosition = position;
	}
	void SetDetached(const std::string& n)
	{
		type = DETACHED;
		name = n;
	}

	bool operator==(const InventoryLocation& other) const
	{
		if(type != other.type)
			return false;

		switch(type)
        {
		    case UNDEFINED:
			    return false;
		    case CURRENT_PLAYER:
			    return true;
		    case PLAYER:
			    return (name == other.name);
		    case NODEMETA:
			    return (nodePosition == other.nodePosition);
		    case DETACHED:
			    return (name == other.name);
		}
		return false;
	}
	bool operator!=(const InventoryLocation& other) const
	{
		return !(*this == other);
	}

	void ApplyCurrentPlayer(const std::string& name)
	{
		if(type == CURRENT_PLAYER)
			SetPlayer(name);
	}

	std::string Dump() const;
	void Serialize(std::ostream& os) const;
	void Deserialize(std::istream& is);
	void Deserialize(const std::string& str);
};

struct InventoryAction;

class InventoryManager
{
public:
	InventoryManager() = default;
	virtual ~InventoryManager() = default;

	// Get an inventory
	virtual Inventory* GetInventory(const InventoryLocation& loc){ return NULL; }
    // Set modified (will be saved and sent over network;)
	virtual void SetInventoryModified(const InventoryLocation& loc) {}
    // Send inventory action
	virtual void DoInventoryAction(InventoryAction* a) {}
};

enum class Action : uint16_t 
{
	Move,
	Drop,
	Craft
};

struct InventoryAction
{
	static InventoryAction* Deserialize(std::istream& is);

	virtual Action GetType() const = 0;
	virtual void Serialize(std::ostream& os) const = 0;
	virtual void Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env) = 0;
	virtual void Apply(InventoryManager* mgr, Environment* env) = 0;
	virtual ~InventoryAction() = default;;
};

struct MoveAction
{
	InventoryLocation fromInventory;
	std::string fromList;
	short fromItem = -1;
	InventoryLocation toInventory;
	std::string toList;
	short toItem = -1;
};

struct BaseMoveAction : public InventoryAction, public MoveAction
{
	// count=0 means "everything"
	uint16_t count = 0;
	bool moveSomewhere = false;

	// treat these as private
	// related to movement to somewhere
	bool causedByMoveSomewhere = false;
	unsigned int moveCount = 0;

    BaseMoveAction() = default;

    BaseMoveAction(std::istream& is, bool somewhere);

    Action GetType() const
	{
		return Action::Move;
	}

	void Serialize(std::ostream& os) const
	{
		if (!moveSomewhere)
			os << "Move ";
		else
			os << "MoveSomewhere ";
		os << count << " ";
		os << fromInventory.Dump() << " ";
		os << fromList << " ";
		os << fromItem << " ";
		os << toInventory.Dump() << " ";
		os << toList;
		if (!moveSomewhere)
			os << " " << toItem;
	}

	void Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env);
	void Apply(InventoryManager* mgr, Environment* env);

	void SwapDirections();

	void OnPutAndOnTake(const ItemStack& srcItem, LogicActiveObject* player) const;
	void OnMove(int count, LogicActiveObject* player) const;

	int AllowPut(const ItemStack& dstItem, LogicActiveObject* player) const;
	int AllowTake(const ItemStack& srcItem, LogicActiveObject* player) const;
	int AllowMove(int tryTakeCount, LogicActiveObject* player) const;
};

struct BaseDropAction : public InventoryAction, public MoveAction
{
	// count=0 means "everything"
	uint16_t count = 0;

    BaseDropAction() = default;

    BaseDropAction(std::istream& is);

    Action GetType() const
	{
		return Action::Drop;
	}

	void Serialize(std::ostream& os) const
	{
		os<<"Drop ";
		os<<count<<" ";
		os<<fromInventory.Dump()<<" ";
		os<<fromList<<" ";
		os<<fromItem;
	}

	void Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env);

	void Apply(InventoryManager* mgr, Environment* env);
};

struct BaseCraftAction : public InventoryAction
{
	// count=0 means "everything"
	uint16_t count = 0;
	InventoryLocation craftInventory;

    BaseCraftAction() = default;

    BaseCraftAction(std::istream& is);

    Action GetType() const
	{
		return Action::Craft;
	}

	void Serialize(std::ostream& os) const
	{
		os<<"Craft ";
		os<<count<<" ";
		os<<craftInventory.Dump()<<" ";
	}

	void Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env);

	void Apply(InventoryManager* mgr, Environment* env);
};

// Crafting helper
bool GetCraftingResult(Inventory* inv, ItemStack& result,
    std::vector<ItemStack>& outputReplacements, bool DecrementInput, Environment* env);

class LogicInventoryManager : public InventoryManager
{
public:
    LogicInventoryManager();
    virtual ~LogicInventoryManager();

    void SetEnvironment(LogicEnvironment* env)
    {
        LogAssert(!mEnvironment, "invalid environment");
        mEnvironment = env;
    }

    Inventory* GetInventory(const InventoryLocation& loc);
    void SetInventoryModified(const InventoryLocation& loc);

    // Creates or resets inventory
    Inventory* CreateDetachedInventory(
		const std::string& name, BaseItemManager* itemMgr, const std::string& player = "");
    bool RemoveDetachedInventory(const std::string& name);
    bool CheckDetachedInventoryAccess(const InventoryLocation& loc, const std::string& player) const;

    void SendDetachedInventories(const std::string& actorName, bool incremental,
        std::function<void(const std::string&, Inventory *)> ApplyCB);

private:

    struct DetachedInventory
    {
        Inventory* inventory;
        std::string owner;
    };

    LogicEnvironment* mEnvironment = nullptr;

    std::unordered_map<std::string, DetachedInventory> mDetachedInventories;
};


#endif