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

#include "Inventory.h"

#include "../Map/ContentMapNode.h"

#include "Core/Logger/Logger.h"

#include "Core/Utility/StringUtil.h"
#include "Core/Utility/Serialize.h"

/*
	ItemStack
*/
ItemStack::ItemStack(const std::string &aName, uint16_t aCount, uint16_t aWear, BaseItemManager* itemMgr) :
	name(itemMgr->GetAlias(aName)), count(aCount), wear(aWear)
{
	if (name.empty() || count == 0)
		Clear();
	else if (itemMgr->Get(name).type == ITEM_TOOL)
		count = 1;
}

void ItemStack::Serialize(std::ostream& os, bool serializeMeta) const
{
	if (IsEmpty())
		return;

	// Check how many parts of the itemstring are needed
	int parts = 1;
	if (!metadata.Empty())
		parts = 4;
	else if (wear != 0)
		parts = 3;
	else if (count != 1)
		parts = 2;

	os << SerializeJsonStringIfNeeded(name);
	if (parts >= 2)
		os << " " << count;
	if (parts >= 3)
		os << " " << wear;
	if (parts >= 4) {
		os << " ";
		if (serializeMeta)
			metadata.Serialize(os);
		else
			os << "<metadata size=" << metadata.Size() << ">";
	}
}

void ItemStack::Deserialize(std::istream& is, BaseItemManager* itemMgr)
{
	Clear();

	// Read name
	name = DeserializeJsonStringIfNeeded(is);

	// Skip space
	std::string tmp;
	std::getline(is, tmp, ' ');
	if(!tmp.empty())
		throw SerializationError("Unexpected text after item name");

	if(name == "MaterialItem")
	{
		// Obsoleted on 2011-07-30
		uint16_t material;
		is>>material;
		uint16_t materialcount;
		is>>materialcount;
		// Convert old materials
        if (material <= 0xff)
        {
            for (const auto& tt : TranslationTable19) 
                if (tt[1] == material)
                    material = tt[0];
        }
		if(material > 0xfff)
			throw SerializationError("Too large material number");
		// Convert old id to name
		NameIdMapping legacyNimap;
		ContentMapNodeGetNameIdMapping(&legacyNimap);
		legacyNimap.GetName(material, name);
		if(name.empty())
			name = "unknown_block";
		if (itemMgr)
			name = itemMgr->GetAlias(name);
		count = materialcount;
	}
	else if(name == "MaterialItem2")
	{
		// Obsoleted on 2011-11-16
		uint16_t material;
		is>>material;
		uint16_t materialcount;
		is>>materialcount;
		if(material > 0xfff)
			throw SerializationError("Too large material number");
		// Convert old id to name
		NameIdMapping legacyNimap;
		ContentMapNodeGetNameIdMapping(&legacyNimap);
		legacyNimap.GetName(material, name);
		if(name.empty())
			name = "unknown_block";
		if (itemMgr)
			name = itemMgr->GetAlias(name);
		count = materialcount;
	}
	else if(name == "node" || name == "NodeItem" || name == "MaterialItem3"
			|| name == "craft" || name == "CraftItem")
	{
		// Obsoleted on 2012-01-07
		std::string all;
		std::getline(is, all, '\n');
		// First attempt to read inside ""
		Strfnd fnd(all);
		fnd.Next("\"");
		// If didn't skip to end, we have ""s
		if(!fnd.AtEnd())
        {
			name = fnd.Next("\"");
		} 
        else 
        { // No luck, just read a word then
			fnd.Start(all);
			name = fnd.Next(" ");
		}
		fnd.SkipOver(" ");
		if (itemMgr)
			name = itemMgr->GetAlias(name);
		count = atoi(Trim(fnd.Next("")).c_str());
		if(count == 0)
			count = 1;
	}
	else if(name == "MBOItem")
	{
		// Obsoleted on 2011-10-14
		throw SerializationError("MBOItem not supported anymore");
	}
	else if(name == "tool" || name == "ToolItem")
	{
		// Obsoleted on 2012-01-07

		std::string all;
		std::getline(is, all, '\n');
		// First attempt to read inside ""
		Strfnd fnd(all);
		fnd.Next("\"");
		// If didn't skip to end, we have ""s
		if(!fnd.AtEnd())
        {
			name = fnd.Next("\"");
		} 
        else 
        { // No luck, just read a word then
			fnd.Start(all);
			name = fnd.Next(" ");
		}
		count = 1;
		// Then read wear
		fnd.SkipOver(" ");
		if (itemMgr)
			name = itemMgr->GetAlias(name);
		wear = atoi(Trim(fnd.Next("")).c_str());
	}
	else
	{
		do  // This loop is just to allow "break;"
		{
			// The real thing

			// Apply item aliases
			if (itemMgr)
				name = itemMgr->GetAlias(name);

			// Read the count
			std::string countStr;
			std::getline(is, countStr, ' ');
			if (countStr.empty()) 
            {
				count = 1;
				break;
			}

			count = atoi(countStr.c_str());

			// Read the wear
			std::string wearStr;
			std::getline(is, wearStr, ' ');
			if(wearStr.empty())
				break;

			wear = atoi(wearStr.c_str());

			// Read metadata
			metadata.Deserialize(is);

			// In case fields are added after metadata, skip space here:
			//std::getline(is, tmp, ' ');
			//if(!tmp.empty())
			//	throw SerializationError("Unexpected text after metadata");

		} while(false);
	}

	if (name.empty() || count == 0)
		Clear();
	else if (itemMgr && itemMgr->Get(name).type == ITEM_TOOL)
		count = 1;
}

void ItemStack::Deserialize(const std::string& str, BaseItemManager* itemMgr)
{
	std::istringstream is(std::string(str), std::ios::binary);
	Deserialize(is, itemMgr);
}

std::string ItemStack::GetItemString(bool includeMeta) const
{
	std::ostringstream os(std::ios::binary);
	Serialize(os, includeMeta);
	return os.str();
}

std::string ItemStack::GetDescription(BaseItemManager* itemMgr) const
{
	std::string desc = metadata.GetString("description");
	if (desc.empty())
		desc = GetDefinition(itemMgr).description;
	return desc.empty() ? name : desc;
}

std::string ItemStack::GetInt16Description(BaseItemManager* itemMgr) const
{
	std::string desc = metadata.GetString("shortDescription");
	if (desc.empty())
		desc = GetDefinition(itemMgr).shortDescription;
	if (!desc.empty())
		return desc;
	// no shortDescription because of old logic version or modified builtin
	// return first line of description
	std::stringstream sstr(GetDescription(itemMgr));
	std::getline(sstr, desc, '\n');
	return desc;
}


ItemStack ItemStack::AddItem(ItemStack newItem, BaseItemManager* itemMgr)
{
	// If the item is empty or the position invalid, bail out
	if(newItem.IsEmpty())
	{
		// nothing can be added trivially
	}
	// If this is an empty item, it's an easy job.
	else if(IsEmpty())
	{
		*this = newItem;
        newItem.Clear();
	}
	// If item name or metadata differs, bail out
	else if (name != newItem.name || metadata != newItem.metadata)
	{
		// cannot be added
	}
	// If the item fits fully, add counter and delete it
	else if(newItem.count <= FreeSpace(itemMgr))
	{
		Add(newItem.count);
        newItem.Clear();
	}
	// Else the item does not fit fully. Add all that fits and return
	// the rest.
	else
	{
		uint16_t freespace = FreeSpace(itemMgr);
		Add(freespace);
        newItem.Remove(freespace);
	}

	return newItem;
}

bool ItemStack::ItemFits(ItemStack newItem, ItemStack* restItem, BaseItemManager* itemMgr) const
{
	// If the item is empty or the position invalid, bail out
	if(newItem.IsEmpty())
	{
		// nothing can be added trivially
	}
	// If this is an empty item, it's an easy job.
	else if(IsEmpty())
	{
        newItem.Clear();
	}
	// If item name or metadata differs, bail out
	else if (name != newItem.name || metadata != newItem.metadata)
	{
		// cannot be added
	}
	// If the item fits fully, delete it
	else if(newItem.count <= FreeSpace(itemMgr))
	{
        newItem.Clear();
	}
	// Else the item does not fit fully. Return the rest.
	else
	{
		uint16_t freespace = FreeSpace(itemMgr);
        newItem.Remove(freespace);
	}

	if(restItem)
		*restItem = newItem;

	return newItem.IsEmpty();
}

ItemStack ItemStack::TakeItem(unsigned int takecount)
{
	if(takecount == 0 || count == 0)
		return ItemStack();

	ItemStack result = *this;
	if(takecount >= count)
	{
		// Take all
		Clear();
	}
	else
	{
		// Take part
		Remove(takecount);
		result.count = takecount;
	}
	return result;
}

ItemStack ItemStack::PeekItem(unsigned int peekcount) const
{
	if(peekcount == 0 || count == 0)
		return ItemStack();

	ItemStack result = *this;
	if(peekcount < count)
		result.count = peekcount;
	return result;
}

/*
	Inventory
*/
InventoryList::InventoryList(const std::string& name, unsigned int size, BaseItemManager* itemMgr) :
	mName(name), mSize(size), mItemMgr(itemMgr)
{
	ClearItems();
}

void InventoryList::ClearItems()
{
	mItems.clear();

	for (unsigned int i=0; i < mSize; i++) {
		mItems.emplace_back();
	}

	SetModified();
}

void InventoryList::SetSize(unsigned int newsize)
{
	if (newsize == mItems.size())
		return;

	mItems.resize(newsize);
	mSize = newsize;
	SetModified();
}

void InventoryList::SetWidth(unsigned int newwidth)
{
	mWidth = newwidth;
	SetModified();
}

void InventoryList::SetName(const std::string& name)
{
	mName = name;
	SetModified();
}

void InventoryList::Serialize(std::ostream& os, bool incremental) const
{
	//os.imbue(std::locale("C"));

	os<<"Width "<<mWidth<<"\n";

	for (const auto& item : mItems) 
    {
		if (item.IsEmpty()) 
        {
			os<<"Empty";
		} 
        else 
        {
			os<<"Item ";
			item.Serialize(os);
		}
		// TODO: Implement this:
		// if (!incremental || item.CheckModified())
		// os << "Keep";
		os<<"\n";
	}

	os<<"EndInventoryList\n";
}

void InventoryList::Deserialize(std::istream& is)
{
	//is.imbue(std::locale("C"));
	SetModified();

	unsigned int itemIdx = 0;
	mWidth = 0;
	while (is.good()) 
    {
		std::string line;
		std::getline(is, line, '\n');

		std::istringstream iss(line);
		//iss.imbue(std::locale("C"));

		std::string name;
		std::getline(iss, name, ' ');

		if (name == "EndInventoryList" || name == "end") 
        {
			// If partial incremental: Clear leftover items (should not happen!)
			for (size_t i = itemIdx; i < mItems.size(); ++i)
				mItems[i].Clear();
			return;
		}

		if (name == "screen_width") 
        {
			iss >> mWidth;
			if (iss.fail())
				throw SerializationError("incorrect width property");
		}
		else if(name == "Item")
		{
			if(itemIdx > GetSize() - 1)
				throw SerializationError("too many items");
			ItemStack item;
			item.Deserialize(iss, mItemMgr);
			mItems[itemIdx++] = item;
		}
		else if(name == "Empty")
		{
			if(itemIdx > GetSize() - 1)
				throw SerializationError("too many items");
			mItems[itemIdx++].Clear();
		} 
        else if (name == "Keep") 
        {
			++itemIdx; // Unmodified item
		}
	}

	// Contents given to Deserialize() were not terminated properly: throw error.

	std::ostringstream ss;
	ss << "Malformatted inventory list. list="
		<< mName << ", read " << itemIdx << " of " << GetSize()
		<< " ItemStacks." << std::endl;
	throw SerializationError(ss.str());
}

InventoryList::InventoryList(const InventoryList &other)
{
	*this = other;
}

InventoryList & InventoryList::operator = (const InventoryList& other)
{
	mItems = other.mItems;
	mSize = other.mSize;
	mWidth = other.mWidth;
	mName = other.mName;
	mItemMgr = other.mItemMgr;
	//SetDirty(true);

	return *this;
}

bool InventoryList::operator == (const InventoryList& other) const
{
	if(mSize != other.mSize)
		return false;
	if(mWidth != other.mWidth)
		return false;
	if(mName != other.mName)
		return false;
	for (unsigned int i = 0; i < mItems.size(); i++)
		if (mItems[i] != other.mItems[i])
			return false;

	return true;
}

const std::string& InventoryList::GetName() const
{
	return mName;
}

unsigned int InventoryList::GetSize() const
{
	return (unsigned int)mItems.size();
}

unsigned int InventoryList::GetWidth() const
{
	return mWidth;
}

unsigned int InventoryList::GetUsedSlots() const
{
	unsigned int num = 0;
	for (const auto& item : mItems)
		if (!item.IsEmpty())
			num++;

	return num;
}

unsigned int InventoryList::GetFreeSlots() const
{
	return GetSize() - GetUsedSlots();
}

const ItemStack& InventoryList::GetItem(unsigned int i) const
{
	LogAssert(i < mSize, "Invalid item"); // Pre-condition
	return mItems[i];
}

ItemStack& InventoryList::GetItem(unsigned int i)
{
	LogAssert(i < mSize, "Invalid item"); // Pre-condition
	return mItems[i];
}

ItemStack InventoryList::ChangeItem(unsigned int i, const ItemStack& newItem)
{
	if(i >= mItems.size())
		return newItem;

	ItemStack oldItem = mItems[i];
	mItems[i] = newItem;
	SetModified();
	return oldItem;
}

void InventoryList::DeleteItem(unsigned int i)
{
	LogAssert(i < mItems.size(), "Invalid item"); // Pre-condition
	mItems[i].Clear();
	SetModified();
}

ItemStack InventoryList::AddItem(const ItemStack& item)
{
	ItemStack newItem = item;
	if(newItem.IsEmpty())
		return newItem;

	/*
		First try to find if it could be added to some existing items
	*/
	for(unsigned int i=0; i<mItems.size(); i++)
	{
		// Ignore empty slots
		if(mItems[i].IsEmpty())
			continue;
		// Try adding
        newItem = AddItem(i, newItem);
		if(newItem.IsEmpty())
			return newItem; // All was eaten
	}

	/*
		Then try to add it to empty slots
	*/
	for(unsigned int i=0; i<mItems.size(); i++)
	{
		// Ignore unempty slots
		if(!mItems[i].IsEmpty())
			continue;
		// Try adding
        newItem = AddItem(i, newItem);
		if(newItem.IsEmpty())
			return newItem; // All was eaten
	}

	// Return leftover
	return newItem;
}

ItemStack InventoryList::AddItem(unsigned int i, const ItemStack& newItem)
{
	if(i >= mItems.size())
		return newItem;

	ItemStack leftover = mItems[i].AddItem(newItem, mItemMgr);
	if (leftover != newItem)
		SetModified();
	return leftover;
}

bool InventoryList::ItemFits(const unsigned int i, const ItemStack& newItem, ItemStack* restItem) const
{
	if(i >= mItems.size())
	{
		if(restItem)
			*restItem = newItem;
		return false;
	}

	return mItems[i].ItemFits(newItem, restItem, mItemMgr);
}

bool InventoryList::RoomForItem(const ItemStack& itemStack) const
{
	ItemStack item = itemStack;
	ItemStack leftover;
	for(unsigned int i=0; i<mItems.size(); i++)
	{
		if(ItemFits(i, item, &leftover))
			return true;
		item = leftover;
	}
	return false;
}

bool InventoryList::ContainsItem(const ItemStack& item, bool matchMeta) const
{
	unsigned int count = item.count;
	if (count == 0)
		return true;

	for (auto i = mItems.rbegin(); i != mItems.rend(); ++i) 
    {
		if (count == 0)
			break;
		if (i->name == item.name && 
            (!matchMeta || (i->metadata == item.metadata))) 
        {
			if (i->count >= count)
				return true;

			count -= i->count;
		}
	}
	return false;
}

ItemStack InventoryList::RemoveItem(const ItemStack& item)
{
	ItemStack removed;
	for (auto i = mItems.rbegin(); i != mItems.rend(); ++i) 
    {
		if (i->name == item.name) 
        {
			unsigned int stillToRemove = item.count - removed.count;
			ItemStack leftover = removed.AddItem(i->TakeItem(stillToRemove), mItemMgr);
			// Allow oversized stacks
			removed.count += leftover.count;

			if (removed.count == item.count)
				break;
		}
	}
	if (!removed.IsEmpty())
		SetModified();
	return removed;
}

ItemStack InventoryList::TakeItem(unsigned int i, unsigned int takecount)
{
	if(i >= mItems.size())
		return ItemStack();

	ItemStack taken = mItems[i].TakeItem(takecount);
	if (!taken.IsEmpty())
		SetModified();
	return taken;
}

void InventoryList::MoveItemSomewhere(unsigned int i, InventoryList *dest, unsigned int count)
{
	// Take item from source list
	ItemStack item1;
	if (count == 0)
		item1 = ChangeItem(i, ItemStack());
	else
		item1 = TakeItem(i, count);

	if (item1.IsEmpty())
		return;

	ItemStack leftover;
	leftover = dest->AddItem(item1);

	if (!leftover.IsEmpty()) 
    {
		// Add the remaining part back to the source item
		AddItem(i, leftover);
	}
}

unsigned int InventoryList::MoveItem(unsigned int i, InventoryList* dest, 
    unsigned int destIdx, unsigned int count, bool swapIfNeeded, bool* didSwap)
{
	if (this == dest && i == destIdx)
		return count;

	// Take item from source list
	ItemStack item1;
	if (count == 0)
		item1 = ChangeItem(i, ItemStack());
	else
		item1 = TakeItem(i, count);

	if (item1.IsEmpty())
		return 0;

	// Try to add the item to destination list
	unsigned int oldcount = item1.count;
	item1 = dest->AddItem(destIdx, item1);

	// If something is returned, the item was not fully added
	if (!item1.IsEmpty()) 
    {
		// If olditem is returned, nothing was added.
		bool nothingAdded = (item1.count == oldcount);

		// If something else is returned, part of the item was left unadded.
		// Add the other part back to the source item
		AddItem(i, item1);

		// If olditem is returned, nothing was added.
		// Swap the items
		if (nothingAdded && swapIfNeeded) 
        {
			// Tell that we swapped
			if (didSwap != NULL) {
				*didSwap = true;
			}
			// Take item from source list
			item1 = ChangeItem(i, ItemStack());
			// Adding was not possible, swap the items.
			ItemStack item2 = dest->ChangeItem(destIdx, item1);
			// Put item from destination list to the source list
			ChangeItem(i, item2);
		}
	}
	return (oldcount - item1.count);
}

/*
	Inventory
*/
Inventory::~Inventory()
{
    Clear();
}

void Inventory::Clear()
{
	for (auto& list : mLists)
		delete list;
	mLists.clear();
	SetModified();
}

Inventory::Inventory(BaseItemManager* itemMgr) : mItemMgr(itemMgr)
{
	SetModified();
}

Inventory::Inventory(const Inventory &other)
{
	*this = other;
}

Inventory& Inventory::operator = (const Inventory& other)
{
	// Gracefully handle self assignment
	if(this != &other)
	{
		Clear();
		mItemMgr = other.mItemMgr;
		for (InventoryList *list : other.mLists)
			mLists.push_back(new InventoryList(*list));
		SetModified();
	}
	return *this;
}

bool Inventory::operator == (const Inventory& other) const
{
	if(mLists.size() != other.mLists.size())
		return false;

	for(unsigned int i=0; i<mLists.size(); i++)
		if(*mLists[i] != *other.mLists[i])
			return false;

	return true;
}

void Inventory::Serialize(std::ostream& os, bool incremental) const
{
	//std::cout << "Serialize " << (int)incremental << ", n=" << mLists.size() << std::endl;
	for (const InventoryList* list : mLists) 
    {
		if (!incremental || list->CheckModified()) 
        {
			os << "List " << list->GetName() << " " << list->GetSize() << "\n";
			list->Serialize(os, incremental);
		} 
        else 
        {
			os << "KeepList " << list->GetName() << "\n";
		}
	}

	os<<"EndInventory\n";
}

void Inventory::Deserialize(std::istream &is)
{
	std::vector<InventoryList *> newLists;
    newLists.reserve(mLists.size());

	while (is.good()) 
    {
		std::string line;
		std::getline(is, line, '\n');

		std::istringstream iss(line);

		std::string name;
		std::getline(iss, name, ' ');
		if (name == "EndInventory" || name == "end") 
        {
			// Remove all lists that were not sent
			for (auto& list : mLists) 
            {
				if (std::find(newLists.begin(), newLists.end(), list) != newLists.end())
					continue;

				delete list;
				list = nullptr;
				SetModified();
			}
			mLists.erase(std::remove(mLists.begin(), mLists.end(), nullptr), mLists.end());
			return;
		}

		if (name == "List") 
        {
			std::string listname;
			unsigned int listsize;

			std::getline(iss, listname, ' ');
			iss>>listsize;

			InventoryList* list = GetList(listname);
			bool createNew = !list;
			if (createNew)
				list = new InventoryList(listname, listsize, mItemMgr);
			else
				list->SetSize(listsize);
			list->Deserialize(is);

			newLists.push_back(list);
			if (createNew)
				mLists.push_back(list);

		} 
        else if (name == "KeepList") 
        {
			// Incrementally sent list
			std::string listname;
			std::getline(iss, listname, ' ');

			InventoryList* list = GetList(listname);
			if (list)
            {
				newLists.push_back(list);
			} 
            else 
            {
				LogError("Tried to keep list " + std::string(listname) + " which is non-existent.");
			}
		}
		// Any additional fields will throw errors when received by visual
		// older than PROTOCOL_VERSION 38
	}

	// Contents given to Deserialize() were not terminated properly: throw error.

	std::ostringstream ss;
	ss << "Malformatted inventory (damaged?). "
		<< mLists.size() << " lists read." << std::endl;
	throw SerializationError(ss.str());
}

InventoryList* Inventory::AddList(const std::string& name, unsigned int size)
{
	SetModified();
	int i = GetListIndex(name);
	if(i != -1)
	{
		if(mLists[i]->GetSize() != size)
		{
			delete mLists[i];
            mLists[i] = new InventoryList(name, size, mItemMgr);
            mLists[i]->SetModified();
		}
		return mLists[i];
	}


	//don't create list with invalid name
	if (name.find(' ') != std::string::npos)
		return nullptr;

	InventoryList *list = new InventoryList(name, size, mItemMgr);
	list->SetModified();
    mLists.push_back(list);
	return list;
}

InventoryList* Inventory::GetList(const std::string& name)
{
	int i = GetListIndex(name);
	if(i == -1)
		return nullptr;
	return mLists[i];
}

std::vector<const InventoryList*> Inventory::GetLists()
{
	std::vector<const InventoryList*> lists;
	lists.reserve(mLists.size());
	for (auto list : mLists)
		lists.push_back(list);
	return lists;
}

bool Inventory::DeleteList(const std::string& name)
{
	int i = GetListIndex(name);
	if(i == -1)
		return false;

	SetModified();
	delete mLists[i];
    mLists.erase(mLists.begin() + i);
	return true;
}

const InventoryList *Inventory::GetList(const std::string& name) const
{
	int i = GetListIndex(name);
	if(i == -1)
		return nullptr;
	return mLists[i];
}

const int Inventory::GetListIndex(const std::string& name) const
{
	for(unsigned int i=0; i<mLists.size(); i++)
	{
		if(mLists[i]->GetName() == name)
			return i;
	}
	return -1;
}

//END
