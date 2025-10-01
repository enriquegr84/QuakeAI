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

#ifndef ITEM_H
#define ITEM_H

#include "../../MinecraftStd.h"

#include "../../Data/TileParams.h"

#include "../../Graphics/TextureOverride.h"

#include "Audio/Sound.h"

class VisualEnvironment;

struct ToolCapabilities;
struct ItemMesh;
struct ItemStack;

/*
	Base item definition
*/

enum ItemType
{
	ITEM_NONE = 0,
	ITEM_NODE,
	ITEM_CRAFT,
	ITEM_TOOL
};

extern std::map<std::string, unsigned int> ItemTypes;

typedef std::unordered_map<std::string, int> ItemGroupList;

static inline int ItemGroupGet(const ItemGroupList& groups, const std::string& name)
{
    ItemGroupList::const_iterator i = groups.find(name);
    if (i == groups.end())
        return 0;
    return i->second;
}

struct Item
{
	/*
		Basic item properties
	*/
	ItemType type;
	std::string name; // "" = hand
	std::string description; // Shown in tooltip.
	std::string shortDescription;

	/*
		Visual properties
	*/
	std::string inventoryImage; // Optional for nodes, mandatory for tools/craftitems
	std::string inventoryOverlay; // Overlay of inventoryImage.
	std::string wieldImage; // If empty, inventoryImage or mesh (only nodes) is used
	std::string wieldOverlay; // Overlay of wieldImage.
	std::string paletteImage; // If specified, the item will be colorized based on this
	SColor color; // The fallback color of the node.
	Vector3<float> wieldScale;

	/*
		Item stack and interaction properties
	*/
	uint16_t stackMax;
	bool usable;
	bool liquidsPointable;
	// May be NULL. If non-NULL, deleted by destructor
	ToolCapabilities* toolCapabilities;
	ItemGroupList groups;
	SimpleSound soundPlace;
	SimpleSound soundPlaceFailed;
	float range;

	// Visual shall immediately place this node when player places the item.
	// Logic will update the precise end result a moment later.
	// "" = no prediction
	std::string nodePlacementPrediction;
	char placeParam2;

	/*
		Some helpful methods
	*/
	Item();
	Item(const Item& item);
	Item& operator=(const Item& item);
	~Item();
	void Reset();
	void Serialize(std::ostream& os) const;
	void Deserialize(std::istream& is);
private:
	void ResetInitial();
};

class BaseItemManager
{
public:
	BaseItemManager() = default;

	virtual ~BaseItemManager() = default;

	// Get item definition
	virtual const Item& Get(const std::string& name) const=0;
	// Get alias definition
	virtual const std::string &GetAlias(const std::string& name) const=0;
	// Get set of all defined item names and aliases
	virtual void GetAll(std::set<std::string>& result) const=0;
	// Check if item is known
	virtual bool IsKnown(const std::string& name) const=0;
	// Get item inventory texture
	virtual std::shared_ptr<Texture2> GetInventoryTexture(const std::string& name, VisualEnvironment* env) const=0;
	// Get item wield mesh
	virtual ItemMesh* GetWieldMesh(const std::string& name, VisualEnvironment* env) const=0;
	// Get item palette
	virtual Palette* GetPalette(const std::string& name, VisualEnvironment* env) const = 0;
	// Returns the base color of an item stack: the color of all
	// tiles that do not define their own color.
	virtual SColor GetItemstackColor(const ItemStack& stack, VisualEnvironment* env) const = 0;

	virtual void Serialize(std::ostream& os)=0;
};

class BaseWritableItemManager : public BaseItemManager
{
public:
	BaseWritableItemManager() = default;

	virtual ~BaseWritableItemManager() = default;

	// Get item definition
	virtual const Item& Get(const std::string& name) const=0;
	// Get alias definition
	virtual const std::string& GetAlias(const std::string& name) const=0;
	// Get set of all defined item names and aliases
	virtual void GetAll(std::set<std::string>& result) const=0;
	// Check if item is known
	virtual bool IsKnown(const std::string& name) const=0;
	// Get item inventory texture
	virtual std::shared_ptr<Texture2> GetInventoryTexture(const std::string& name, VisualEnvironment* env) const=0;
	// Get item wield mesh
	virtual ItemMesh* GetWieldMesh(const std::string& name, VisualEnvironment* env) const=0;

	// Replace the textures of registered nodes with the ones specified in
	// the texture pack's override.txt files
	virtual void ApplyTextureOverrides(const std::vector<TextureOverride>& overrides)=0;

	// Remove all registered item and node definitions and aliases
	// Then re-add the builtin item definitions
	virtual void Clear()=0;
	// Register item definition
	virtual void RegisterItem(const Item& item)=0;
	virtual void UnregisterItem(const std::string& name)=0;
	// Set an alias so that items named <name> will load as <convertTo>.
	// Alias is not set if <name> has already been defined.
	// Alias will be removed if <name> is defined at a later point of time.
	virtual void RegisterAlias(const std::string& name, const std::string& convertTo)=0;

	virtual void Serialize(std::ostream& os)=0;
	virtual void Deserialize(std::istream& is)=0;

	// Do stuff asked by threads that can only be done in the main thread
	virtual void ProcessQueue(VisualEnvironment* env)=0;
};

std::shared_ptr<BaseWritableItemManager> CreateItemManager();

#endif