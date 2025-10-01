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

#ifndef INVENTORY_H
#define INVENTORY_H

#include "Item.h"
#include "ItemstackMetadata.h"

struct ToolCapabilities;

struct ItemStack
{
	ItemStack() = default;
	ItemStack(const std::string &aName, uint16_t aCount, uint16_t aWear, BaseItemManager* itemMgr);

	~ItemStack() = default;

	// Serialization
	void Serialize(std::ostream& os, bool serializeMeta = true) const;
	// Deserialization. Pass itemMgr unless you don't want aliases resolved.
	void Deserialize(std::istream& is,BaseItemManager* itemMgr = NULL);
	void Deserialize(const std::string& str, BaseItemManager* itemMgr = NULL);

	// Returns the string used for inventory
	std::string GetItemString(bool includeMeta = true) const;
	// Returns the tooltip
	std::string GetDescription(BaseItemManager* itemMgr) const;
	std::string GetInt16Description(BaseItemManager* itemMgr) const;

	/*
		Quantity methods
	*/

	bool IsEmpty() const
	{
		return count == 0;
	}

	void Clear()
	{
		name = "";
		count = 0;
		wear = 0;
		metadata.Clear();
	}

	void Add(uint16_t n)
	{
		count += n;
	}

	void Remove(uint16_t n)
	{
		LogAssert(count >= n, "invalid item"); // Pre-condition
		count -= n;
		if(count == 0)
			Clear(); // reset name, wear and metadata too
	}

	// Maximum size of a stack
	uint16_t GetStackMax(BaseItemManager* itemMgr) const
	{
		return itemMgr->Get(name).stackMax;
	}

	// Number of items that can be added to this stack
	uint16_t FreeSpace(BaseItemManager* itemMgr) const
	{
		uint16_t max = GetStackMax(itemMgr);
		if (count >= max)
			return 0;
		return max - count;
	}

	// Returns false if item is not known and cannot be used
	bool IsKnown(BaseItemManager* itemMgr) const
	{
		return itemMgr->IsKnown(name);
	}

	// Returns a pointer to the item definition struct,
	// or a fallback one (name="unknown") if the item is unknown.
	const Item& GetDefinition(BaseItemManager* itemMgr) const
	{
		return itemMgr->Get(name);
	}

	// Get tool digging properties, or those of the hand if not a tool
	const ToolCapabilities& GetToolCapabilities(BaseItemManager* itemMgr) const
	{
		const ToolCapabilities* itemCap = itemMgr->Get(name).toolCapabilities;

        if (itemCap == NULL)
        {
            // Fall back to the hand's tool capabilities
            itemCap = itemMgr->Get("").toolCapabilities;
        }

		LogAssert(itemCap != NULL, "invalid item capability");
		return metadata.GetToolCapabilities(*itemCap); // Check for override
	}

	// Wear out (only tools)
	// Returns true if the item is (was) a tool
	bool AddWear(int amount, BaseItemManager* itemMgr)
	{
		if(GetDefinition(itemMgr).type == ITEM_TOOL)
		{
			if(amount > 65535 - wear)
				Clear();
			else if(amount < -wear)
				wear = 0;
			else
				wear += amount;
			return true;
		}

		return false;
	}

	// If possible, adds newitem to this item.
	// If cannot be added at all, returns the item back.
	// If can be added partly, decremented item is returned back.
	// If can be added fully, empty item is returned.
	ItemStack AddItem(ItemStack newitem, BaseItemManager* itemMgr);

	// Checks whether newitem could be added.
	// If restitem is non-NULL, it receives the part of newitem that
	// would be left over after adding.
	bool ItemFits(ItemStack newitem, ItemStack* restitem, BaseItemManager* itemMgr) const;

	// Takes some items.
	// If there are not enough, takes as many as it can.
	// Returns empty item if couldn't take any.
	ItemStack TakeItem(unsigned int takecount);

	// Similar to TakeItem, but keeps this ItemStack intact.
	ItemStack PeekItem(unsigned int peekcount) const;

	bool operator ==(const ItemStack& s) const
	{
		return (this->name     == s.name &&
				this->count    == s.count &&
				this->wear     == s.wear &&
				this->metadata == s.metadata);
	}

	bool operator !=(const ItemStack &s) const
	{
		return !(*this == s);
	}

	/*
		Properties
	*/
	std::string name = "";
	unsigned short count = 0;
	unsigned short wear = 0;
	ItemStackMetadata metadata;
};

class InventoryList
{
public:
	InventoryList(const std::string& name, unsigned int size, BaseItemManager* itemMgr);
	~InventoryList() = default;
	void ClearItems();
	void SetSize(unsigned int newsize);
	void SetWidth(unsigned int newWidth);
	void SetName(const std::string& name);
	void Serialize(std::ostream& os, bool incremental) const;
	void Deserialize(std::istream& is);

	InventoryList(const InventoryList& other);
	InventoryList& operator = (const InventoryList& other);
	bool operator == (const InventoryList& other) const;
	bool operator != (const InventoryList& other) const
	{
		return !(*this == other);
	}

	const std::string& GetName() const;
	unsigned int GetSize() const;
	unsigned int GetWidth() const;
	// Count used slots
	unsigned int GetUsedSlots() const;
	unsigned int GetFreeSlots() const;

	// Get reference to item
	const ItemStack& GetItem(unsigned int i) const;
	ItemStack& GetItem(unsigned int i);
	// Returns old item. Parameter can be an empty item.
	ItemStack ChangeItem(unsigned int i, const ItemStack& newitem);
	// Delete item
	void DeleteItem(unsigned int i);

	// Adds an item to a suitable place. Returns leftover item (possibly empty).
	ItemStack AddItem(const ItemStack& newitem);

	// If possible, adds item to given slot.
	// If cannot be added at all, returns the item back.
	// If can be added partly, decremented item is returned back.
	// If can be added fully, empty item is returned.
	ItemStack AddItem(unsigned int i, const ItemStack& newitem);

	// Checks whether the item could be added to the given slot
	// If restitem is non-NULL, it receives the part of newitem that
	// would be left over after adding.
	bool ItemFits(const unsigned int i, const ItemStack& newitem, ItemStack* restitem = NULL) const;

	// Checks whether there is room for a given item
	bool RoomForItem(const ItemStack& item) const;

	// Checks whether the given count of the given item
	// exists in this inventory list.
	// If matchMeta is false, only the items' names are compared.
	bool ContainsItem(const ItemStack& item, bool matchMeta) const;

	// Removes the given count of the given item name from
	// this inventory list. Walks the list in reverse order.
	// If not as many items exist as requested, removes as
	// many as possible.
	// Returns the items that were actually removed.
    ItemStack RemoveItem(const ItemStack& item);

	// Takes some items from a slot.
	// If there are not enough, takes as many as it can.
	// Returns empty item if couldn't take any.
	ItemStack TakeItem(unsigned int i, unsigned int takecount);

	// Move an item to a different list (or a different stack in the same list)
	// count is the maximum number of items to move (0 for everything)
	// returns number of moved items
	unsigned int MoveItem(unsigned int i, InventoryList* dest, unsigned int destItem,
		unsigned int count = 0, bool swapIfNeeded = true, bool* didSwap = NULL);

	// like MoveItem, but without a fixed destination index
	// also with optional rollback recording
	void MoveItemSomewhere(unsigned int item, InventoryList* dest, unsigned int count);

	inline bool CheckModified() const { return mDirty; }
	inline void SetModified(bool dirty = true) { mDirty = dirty; }

private:
    BaseItemManager* mItemMgr;

	std::vector<ItemStack> mItems;
	std::string mName;
	unsigned int mSize;
	unsigned int mWidth = 0;
	bool mDirty = true;
};

class Inventory
{
public:
	~Inventory();

	void Clear();

	Inventory(BaseItemManager* itemMgr);
	Inventory(const Inventory& other);
	Inventory & operator = (const Inventory& other);
	bool operator == (const Inventory& other) const;
	bool operator != (const Inventory& other) const
	{
		return !(*this == other);
	}

	// Never ever Serialize to disk using "incremental"!
	void Serialize(std::ostream& os, bool incremental = false) const;
	void Deserialize(std::istream& is);

	InventoryList* AddList(const std::string& name, unsigned int size);
	InventoryList* GetList(const std::string& name);
	const InventoryList * GetList(const std::string& name) const;
	std::vector<const InventoryList*> GetLists();
	bool DeleteList(const std::string& name);
	// A shorthand for adding items. Returns leftover item (possibly empty).
	ItemStack AddItem(const std::string& listname, const ItemStack& newitem)
	{
		InventoryList *list = GetList(listname);
		if(list == NULL)
			return newitem;
		return list->AddItem(newitem);
	}

	inline bool CheckModified() const
	{
		if (mDirty)
			return true;

		for (const auto &list : mLists)
			if (list->CheckModified())
				return true;

		return false;
	}

	inline void SetModified(bool dirty = true)
	{
		mDirty = dirty;
		// Set all as handled
		if (!dirty) {
			for (const auto &list : mLists)
				list->SetModified(dirty);
		}
	}
private:
	// -1 if not found
	const int GetListIndex(const std::string& name) const;

	std::vector<InventoryList*> mLists;
	BaseItemManager* mItemMgr;
	bool mDirty = true;
};


#endif