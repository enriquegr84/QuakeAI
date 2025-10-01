/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Kahrl <kahrl@gmx.net>

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

#include "Item.h"

#include "Tool.h"
#include "Inventory.h"

#include "../Environment//VisualEnvironment.h"

#include "../Map/MapNode.h"

#include "../../Graphics/Shader.h"
#include "../../Graphics/WieldMesh.h"
#include "../../Graphics/Node.h"

#include "Core/Threading/Thread.h"
#include "Core/Utility/StringUtil.h"
#include "Core/Utility/Serialize.h"

std::map<std::string, unsigned int> ItemTypes =
{
    {"none", ITEM_NONE},
    {"node", ITEM_NODE},
    {"craft", ITEM_CRAFT},
    {"tool", ITEM_TOOL}
};

/*
	Item
*/
Item::Item()
{
	ResetInitial();
}

Item::Item(const Item& item)
{
	ResetInitial();
	*this = item;
}

Item& Item::operator=(const Item& item)
{
	if(this == &item)
		return *this;

	Reset();

	type = item.type;
	name = item.name;
	description = item.description;
	shortDescription = item.shortDescription;
	inventoryImage = item.inventoryImage;
	inventoryOverlay = item.inventoryOverlay;
	wieldImage = item.wieldImage;
	wieldOverlay = item.wieldOverlay;
	wieldScale = item.wieldScale;
	stackMax = item.stackMax;
	usable = item.usable;
	liquidsPointable = item.liquidsPointable;
	if (item.toolCapabilities)
		toolCapabilities = new ToolCapabilities(*item.toolCapabilities);
	groups = item.groups;
	nodePlacementPrediction = item.nodePlacementPrediction;
	placeParam2 = item.placeParam2;
	soundPlace = item.soundPlace;
	soundPlaceFailed = item.soundPlaceFailed;
	range = item.range;
	paletteImage = item.paletteImage;
	color = item.color;
	return *this;
}

Item::~Item()
{
	Reset();
}

void Item::ResetInitial()
{
	// Initialize pointers to NULL so reset() does not delete unodeMgrined pointers
	toolCapabilities = NULL;
	Reset();
}

void Item::Reset()
{
	type = ITEM_NONE;
	name = "";
	description = "";
	shortDescription = "";
	inventoryImage = "";
	inventoryOverlay = "";
	wieldImage = "";
	wieldOverlay = "";
	paletteImage = "";
	color = SColor(0xFFFFFFFF);
    wieldScale = Vector3<float>{ 1.0, 1.0, 1.0 };
	stackMax = 99;
	usable = false;
	liquidsPointable = false;
	delete toolCapabilities;
	toolCapabilities = NULL;
	groups.clear();
	soundPlace = SimpleSound();
	soundPlaceFailed = SimpleSound();
	range = -1;
	nodePlacementPrediction = "";
	placeParam2 = 0;
}

void Item::Serialize(std::ostream& os) const
{
	char version = 6;
	WriteUInt8(os, version);
	WriteUInt8(os, type);
	os << SerializeString16(name);
	os << SerializeString16(description);
	os << SerializeString16(inventoryImage);
	os << SerializeString16(wieldImage);
	WriteV3Float(os, wieldScale);
	WriteInt16(os, stackMax);
	WriteUInt8(os, usable);
	WriteUInt8(os, liquidsPointable);

	std::string toolCapabilitiesStr;
	if (toolCapabilities) 
    {
		std::ostringstream tmpOs(std::ios::binary);
		toolCapabilities->Serialize(tmpOs);
		toolCapabilitiesStr = tmpOs.str();
	}
	os << SerializeString16(toolCapabilitiesStr);

	WriteUInt16(os, (uint16_t)groups.size());
	for (const auto& group : groups) 
    {
		os << SerializeString16(group.first);
		WriteInt16(os, group.second);
	}

	os << SerializeString16(nodePlacementPrediction);

	// Version from ContentFeatures::Serialize to keep in sync
	soundPlace.Serialize(os);
	soundPlaceFailed.Serialize(os);

	WriteFloat(os, range);
	os << SerializeString16(paletteImage);
	WriteARGB8(os, color);
	os << SerializeString16(inventoryOverlay);
	os << SerializeString16(wieldOverlay);
	os << SerializeString16(shortDescription);

	os << placeParam2;
}

void Item::Deserialize(std::istream& is)
{
	// Reset everything
	Reset();

	// Deserialize
	int version = ReadUInt8(is);
	if (version < 6)
		throw SerializationError("unsupported Item version");

	type = (enum ItemType)ReadUInt8(is);
	name = DeserializeString16(is);
	description = DeserializeString16(is);
	inventoryImage = DeserializeString16(is);
	wieldImage = DeserializeString16(is);
	wieldScale = ReadV3Float(is);
	stackMax = ReadInt16(is);
	usable = ReadUInt8(is);
	liquidsPointable = ReadUInt8(is);

	std::string toolCapabilitiesStr = DeserializeString16(is);
	if (!toolCapabilitiesStr.empty()) 
    {
		std::istringstream tmpIs(toolCapabilitiesStr, std::ios::binary);
		toolCapabilities = new ToolCapabilities;
		toolCapabilities->Deserialize(tmpIs);
	}

	groups.clear();
	unsigned int groupsSize = ReadUInt16(is);
	for(unsigned int i=0; i<groupsSize; i++)
    {
		std::string name = DeserializeString16(is);
		int value = ReadInt16(is);
		groups[name] = value;
	}
	nodePlacementPrediction = DeserializeString16(is);

	// Version from ContentFeatures::Serialize to keep in sync
	soundPlace.Deserialize(is);
	soundPlaceFailed.Deserialize(is);

	range = ReadFloat(is);
	paletteImage = DeserializeString16(is);
	color = ReadARGB8(is);
	inventoryOverlay = DeserializeString16(is);
	wieldOverlay = DeserializeString16(is);

	// If you add anything here, insert it primarily inside the try-catch
	// block to not need to increase the version.
	try 
    {
		shortDescription = DeserializeString16(is);

		placeParam2 = ReadUInt8(is); // 0 if missing
	} 
    catch(SerializationError&) 
    {
    };
}


/*
	ItemManager
*/

// SUGG: Support chains of aliases?

class ItemManager: public BaseWritableItemManager
{

	struct InventoryCached
	{
		std::shared_ptr<Texture2> inventoryTexture;
		ItemMesh wieldMesh;
		Palette* palette;

		InventoryCached():
			inventoryTexture(NULL), palette(NULL)
		{}
	};

public:
	ItemManager()
	{
		mMainThread = std::this_thread::get_id();
		Clear();
	}

	virtual ~ItemManager()
	{
		for (InventoryCached* ic : mInventoryCached.GetValues())
			delete ic;

		for (auto &itemDefinition : mItems)
			delete itemDefinition.second;

		mItems.clear();
	}

	virtual const Item& Get(const std::string& itemName) const
	{
		// Convert name according to possible alias
		std::string name = GetAlias(itemName);
		// Get the definition
		auto i = mItems.find(name);
		if (i == mItems.cend())
			i = mItems.find("unknown");
		LogAssert(i != mItems.cend(), "invalid item definition");
		return *(i->second);
	}

	virtual const std::string& GetAlias(const std::string& name) const
	{
		auto it = mAliases.find(name);
		if (it != mAliases.cend())
			return it->second;
		return name;
	}

	virtual void GetAll(std::set<std::string>& result) const
	{
		result.clear();
		for (const auto& itemDefinition : mItems)
			result.insert(itemDefinition.first);

		for (const auto &alias : mAliases)
			result.insert(alias.first);
	}

	virtual bool IsKnown(const std::string& name) const
	{
		// Convert name according to possible alias
		std::string alias = GetAlias(name);
		// Get the definition
		return mItems.find(alias) != mItems.cend();
	}

public:
    
    InventoryCached* CreateInventoryCachedDirect(const std::string& name, VisualEnvironment* env) const
	{
		LogInformation("Lazily creating item texture and mesh for \"" + name + "\"");

		// This is not thread-safe
		LogAssert(std::this_thread::get_id() == mMainThread, "invalid thread");

		// Skip if already in cache
		InventoryCached* ic = NULL;
		mInventoryCached.Get(name, &ic);
		if(ic)
			return ic;

		BaseTextureSource* textureSrc = env->GetTextureSource();
		const Item& item = Get(name);

		// Create new InventoryCached
        ic = new InventoryCached();

		// Create an inventory texture
        ic->inventoryTexture = NULL;
		if (!item.inventoryImage.empty())
            ic->inventoryTexture = textureSrc->GetTexture(item.inventoryImage);

		ItemStack itemStack = ItemStack();
		itemStack.name = item.name;

		GetItemMesh(itemStack, &(ic->wieldMesh), env);

        ic->palette = textureSrc->GetPalette(item.paletteImage);

		// Put in cache
		mInventoryCached.Set(name, ic);
		return ic;
	}

	InventoryCached* GetInventoryCached(const std::string& name, VisualEnvironment* env) const
	{
		InventoryCached* ic = NULL;
		mInventoryCached.Get(name, &ic);
		if (ic)
			return ic;

		if (std::this_thread::get_id() == mMainThread)
			return CreateInventoryCachedDirect(name, env);

		// We're gonna ask the result to be put into here
		static ResultQueue<std::string, InventoryCached*, char, char> resultQueue;

		// Throw a request in
		mGetInventoryCachedQueue.Add(name, 0, 0, &resultQueue);
		try 
        {
			while(true) 
            {
				// Wait result for a second
				GetResult<std::string, InventoryCached*, char, char>
					result = resultQueue.PopFront(1000);

				if (result.mKey == name)
					return result.mItem;
			}
		} 
        catch(ItemNotFoundException&) 
        {
			LogError("Waiting for visual cached " + name + " timed out.");
			return &mDummyInventoryCached;
		}
	}

	// Get item inventory texture
	virtual std::shared_ptr<Texture2> GetInventoryTexture(const std::string& name, VisualEnvironment* env) const
	{
		InventoryCached* ic = GetInventoryCached(name, env);
		if(!ic)
			return NULL;
		return ic->inventoryTexture;
	}

	// Get item wield mesh
	virtual ItemMesh* GetWieldMesh(const std::string& name, VisualEnvironment* env) const
	{
		InventoryCached *ic = GetInventoryCached(name, env);
		if(!ic)
			return NULL;
		return &(ic->wieldMesh);
	}

	// Get item palette
	virtual Palette* GetPalette(const std::string &name, VisualEnvironment* env) const
	{
		InventoryCached *ic = GetInventoryCached(name, env);
		if(!ic)
			return NULL;
		return ic->palette;
	}

	virtual SColor GetItemstackColor(const ItemStack& stack, VisualEnvironment* env) const
	{
		// Look for direct color definition
		const std::string& colorStr = stack.metadata.GetString("color", 0);
		SColor directColor;
		if (!colorStr.empty() && ParseColorString(colorStr, directColor, true))
			return directColor;

		// See if there is a palette
		Palette* palette = GetPalette(stack.name, env);
		const std::string& index = stack.metadata.GetString("palette_index", 0);
		if (palette && !index.empty())
			return (*palette)[std::clamp(atoi(index.c_str()), 0, 255)];
		// Fallback color
		return Get(stack.name).color;
	}

    void ApplyTextureOverrides(const std::vector<TextureOverride>& overrides)
    {
        LogInformation("ItemManager::ApplyTextureOverrides(): Applying overrides to textures");

        for (const TextureOverride& textureOverride : overrides)
        {
            if (mItems.find(textureOverride.id) == mItems.end())
                continue; // Ignore unknown item

            Item* itemMgr = mItems[textureOverride.id];
            if (textureOverride.hasTarget(OverrideTarget::INVENTORY))
                itemMgr->inventoryImage = textureOverride.texture;

            if (textureOverride.hasTarget(OverrideTarget::WIELD))
                itemMgr->wieldImage = textureOverride.texture;
        }
    }

	void Clear()
	{
		for (auto& item : mItems)
			delete item.second;

		mItems.clear();
		mAliases.clear();

		// Add the four builtin items:
		//   "" is the hand
		//   "unknown" is returned whenever an unodeMgrined item
		//     is accessed (is also the unknown node)
		//   "air" is the air node
		//   "ignore" is the ignore node

		Item* handItem = new Item;
		handItem->name = "";
		handItem->wieldImage = "wieldhand.png";
		handItem->toolCapabilities = new ToolCapabilities;
		mItems.insert(std::make_pair("", handItem));

		Item* unknownItem = new Item;
		unknownItem->type = ITEM_NODE;
		unknownItem->name = "unknown";
		mItems.insert(std::make_pair("unknown", unknownItem));

		Item* airItem = new Item;
		airItem->type = ITEM_NODE;
		airItem->name = "air";
		mItems.insert(std::make_pair("air", airItem));

		Item* ignoreItem = new Item;
		ignoreItem->type = ITEM_NODE;
		ignoreItem->name = "ignore";
		mItems.insert(std::make_pair("ignore", ignoreItem));
	}

	virtual void RegisterItem(const Item& item)
	{
		LogInformation("ItemManager: registering " + item.name);
		// Ensure that the "" item (the hand) always has ToolCapabilities
		if (item.name.empty())
			LogAssert(item.toolCapabilities, "Hand does not have ToolCapabilities");

		if(mItems.count(item.name) == 0)
			mItems[item.name] = new Item(item);
		else
			*(mItems[item.name]) = item;

		// Remove conflicting alias if it exists
		bool aliasRemoved = (mAliases.erase(item.name) != 0);
        if (aliasRemoved)
        {
            LogInformation("ItemManager: erased alias " + item.name +
                " because item was defined");
        }
	}

	virtual void UnregisterItem(const std::string& name)
	{
		LogInformation("ItemManager: unregistering \"" + name + "\"");

		delete mItems[name];
		mItems.erase(name);
	}

	virtual void RegisterAlias(const std::string& name, const std::string& convertTo)
	{
		if (mItems.find(name) == mItems.end()) 
        {
			LogInformation("ItemManager: setting alias " + name + " -> " + convertTo);
			mAliases[name] = convertTo;
		}
	}

	void Serialize(std::ostream& os)
	{
		WriteUInt8(os, 0); // version
		WriteUInt16(os, (uint16_t)mItems.size());

		for (const auto& it : mItems) 
        {
			Item* item = it.second;
			// Serialize Item and write wrapped in a string
			std::ostringstream tmpOs(std::ios::binary);
			item->Serialize(tmpOs);
			os << SerializeString16(tmpOs.str());
		}
		WriteUInt16(os, (uint16_t)mAliases.size());

		for (const auto& it : mAliases) 
        {
			os << SerializeString16(it.first);
			os << SerializeString16(it.second);
		}
	}

	void Deserialize(std::istream& is)
	{
		// Clear everything
		Clear();
		// Deserialize
		int version = ReadUInt8(is);
		if(version != 0)
			throw SerializationError("unsupported ItemManager version");
		uint16_t count = ReadUInt16(is);
		for(uint16_t i=0; i<count; i++)
		{
			// Deserialize a string and grab an Item from it
			std::istringstream tmpIs(DeserializeString16(is), std::ios::binary);
			Item item;
			item.Deserialize(tmpIs);
			// Register
			RegisterItem(item);
		}
		uint16_t numAliases = ReadUInt16(is);
		for(uint16_t i=0; i<numAliases; i++)
		{
			std::string name = DeserializeString16(is);
			std::string convertTo = DeserializeString16(is);
			RegisterAlias(name, convertTo);
		}
	}

	void ProcessQueue(VisualEnvironment* env)
	{
		//NOTE this is only thread safe for ONE consumer thread!
		while(!mGetInventoryCachedQueue.Empty())
		{
			GetRequest<std::string, InventoryCached*, char, char>
                request = mGetInventoryCachedQueue.Pop();

			mGetInventoryCachedQueue.PushResult(
                request, CreateInventoryCachedDirect(request.mKey, env));
		}
	}

private:
	// Key is name
	std::map<std::string, Item*> mItems;
	// Aliases
	StringMap mAliases;
	// The id of the thread that is allowed to use irrlicht directly
	std::thread::id mMainThread;
	// A reference to this can be returned when nothing is found, to avoid NULLs
	mutable InventoryCached mDummyInventoryCached;
	// Cached textures and meshes
	mutable MutexedMap<std::string, InventoryCached*> mInventoryCached;
	// Queued visual cached fetches (to be processed by the main thread)
	mutable RequestQueue<std::string, InventoryCached*, char, char> mGetInventoryCachedQueue;
};

std::shared_ptr<BaseWritableItemManager> CreateItemManager()
{
	return std::shared_ptr<BaseWritableItemManager>(new ItemManager());
}
