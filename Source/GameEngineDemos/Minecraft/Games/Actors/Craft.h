/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef CRAFT_H
#define CRAFT_H

#include "Inventory.h"

class Environment;

/*
	Crafting methods.

	The crafting method depends on the inventory list
	that the crafting input comes from.
*/
enum CraftMethod
{
	// Crafting grid
	CRAFT_METHOD_NORMAL,
	// Cooking something in a furnace
	CRAFT_METHOD_COOKING,
	// Using something as fuel for a furnace
	CRAFT_METHOD_FUEL,
};

/*
	The type a hash can be. The earlier a type is mentioned in this enum,
	the earlier it is tried at crafting, and the less likely is a collision.
	Changing order causes changes in behaviour, so know what you do.
 */
enum CraftHashType
{
	// Hashes the normalized names of the recipe's elements.
	// Only recipes without group usage can be found here,
	// because groups can't be guessed efficiently.
	CRAFT_HASH_TYPE_ITEM_NAMES,

	// Counts the non-empty slots.
	CRAFT_HASH_TYPE_COUNT,

	// This layer both spares an extra variable, and helps to retain (albeit rarely used) functionality. Maps to 0.
	// Before hashes are "initialized", all hashes reside here, after initialisation, none are.
	CRAFT_HASH_TYPE_UNHASHED

};
const int CraftHashTypeMax = (int) CRAFT_HASH_TYPE_UNHASHED;

/*
	Input: The contents of the crafting slots, arranged in matrix form
*/
struct CraftInput
{
	CraftMethod method = CRAFT_METHOD_NORMAL;
	unsigned int width = 0;
	std::vector<ItemStack> items;

	CraftInput() = default;

	CraftInput(CraftMethod aMethod, unsigned int aWidth,
			const std::vector<ItemStack>& aItems) :
		method(aMethod), width(aWidth), items(aItems)
	{}

	// Returns true if all items are empty.
	bool Empty() const;

	std::string Dump() const;
};

/*
	Output: Result of crafting operation
*/
struct CraftOutput
{
	// Used for normal crafting and cooking, itemstring
	std::string item = "";
	// Used for cooking (cook time) and fuel (burn time), seconds
	float time = 0.0f;

	CraftOutput() = default;

	CraftOutput(const std::string& aItem, float aTime) :
		item(aItem), time(aTime)
	{}

	std::string Dump() const;
};

/*
	A list of replacements. A replacement indicates that a specific
	input item should not be deleted (when crafting) but replaced with
	a different item. Each replacements is a pair (itemstring to remove,
	itemstring to replace with)

	Example: If ("bucket:bucket_water", "bucket:bucket_empty") is a
	replacement pair, the crafting input slot that contained a water
	bucket will contain an empty bucket after crafting.
*/
struct CraftReplacements
{
	// List of replacements
	std::vector<std::pair<std::string, std::string> > pairs;

	CraftReplacements() = default;
	CraftReplacements(const std::vector<std::pair<std::string, std::string> >& aPairs):
		pairs(aPairs)
	{}

    std::string Dump() const;
};

/*
	Crafting definition base class
*/
class Craft
{
public:
	/*
		Craft recipe priorities, from low to high

		Recipes are searched from latest to first.
		If a recipe with higher priority than a previous found one is
		encountered, it is selected instead.
	*/
	enum RecipePriority
	{
		PRIORITY_NO_RECIPE,
		PRIORITY_TOOLREPAIR,
		PRIORITY_SHAPELESS_AND_GROUPS,
		PRIORITY_SHAPELESS,
		PRIORITY_SHAPED_AND_GROUPS,
		PRIORITY_SHAPED,
	};

	Craft() = default;
	virtual ~Craft() = default;

	// Returns type of crafting definition
	virtual std::string GetName() const=0;

	// Checks whether the recipe is applicable
	virtual bool Check(const CraftInput& input, Environment* env) const=0;
	RecipePriority GetPriority() const
	{
		return priority;
	}
	// Returns the output structure, meaning depends on crafting method
	// The implementation can assume that check(input) returns true
	virtual CraftOutput GetOutput(const CraftInput& input, Environment* env) const=0;
	// the inverse of the above
	virtual CraftInput GetInput(const CraftOutput& output, Environment* env) const=0;
	// Decreases count of every input item
	virtual void DecrementInput(CraftInput &input,
		std::vector<ItemStack>& outputReplacements, Environment* env) const=0;

	CraftHashType GetHashType() const
	{
		return hashType;
	}
	virtual uint64_t GetHash(CraftHashType type) const = 0;

	// to be called after all mods are loaded, so that we catch all aliases
	virtual void InitHash(Environment* env) = 0;

	virtual std::string Dump() const=0;

protected:
	CraftHashType hashType;
	RecipePriority priority;
};

/*
	A plain-jane (shaped) crafting definition

	Supported crafting method: CRAFT_METHOD_NORMAL.
	Requires the input items to be arranged exactly like in the recipe.
*/
class CraftShaped: public Craft
{
public:
	CraftShaped() = delete;
	CraftShaped(
		const std::string& output, unsigned int width,
		const std::vector<std::string>& recipe,
		const CraftReplacements& replacements);

	virtual ~CraftShaped() = default;

	virtual std::string GetName() const;
	virtual bool Check(const CraftInput& input, Environment* env) const;
	virtual CraftOutput GetOutput(const CraftInput& input, Environment* env) const;
	virtual CraftInput GetInput(const CraftOutput& output, Environment* env) const;
	virtual void DecrementInput(CraftInput& input,
        std::vector<ItemStack>& outputReplacements, Environment* env) const;

	virtual uint64_t GetHash(CraftHashType type) const;

	virtual void InitHash(Environment* env);

	virtual std::string Dump() const;

private:
	// Output itemstring
    std::string output = "";
	// Width of recipe
	unsigned int width = 1;
	// Recipe matrix (itemstrings)
	std::vector<std::string> recipe;
	// Recipe matrix (item names)
	std::vector<std::string> recipeNames;
	// bool indicating if InitHash has been called already
	bool hashInited = false;
	// Replacement items for DecrementInput()
	CraftReplacements replacements;
};

/*
	A shapeless crafting definition
	Supported crafting method: CRAFT_METHOD_NORMAL.
	Input items can arranged in any way.
*/
class CraftShapeless: public Craft
{
public:
	CraftShapeless() = delete;
	CraftShapeless(
		const std::string& output,
		const std::vector<std::string>& recipe,
		const CraftReplacements& replacements);

	virtual ~CraftShapeless() = default;

	virtual std::string GetName() const;
	virtual bool Check(const CraftInput& input, Environment* env) const;
	virtual CraftOutput GetOutput(const CraftInput& input, Environment* env) const;
	virtual CraftInput GetInput(const CraftOutput& output, Environment* env) const;
	virtual void DecrementInput(CraftInput& input,
		std::vector<ItemStack>& outputReplacements, Environment* env) const;

	virtual uint64_t GetHash(CraftHashType type) const;

	virtual void InitHash(Environment* env);

	virtual std::string Dump() const;

private:
	// Output itemstring
    std::string output;
	// Recipe list (itemstrings)
	std::vector<std::string> recipe;
	// Recipe list (item names)
	std::vector<std::string> recipeNames;
	// bool indicating if InitHash has been called already
	bool hashInited = false;
	// Replacement items for DecrementInput()
	CraftReplacements replacements;
};

/*
	Tool repair crafting definition
	Supported crafting method: CRAFT_METHOD_NORMAL.
	Put two damaged tools into the crafting grid, get one tool back.
	There should only be one crafting definition of this type.
*/
class CraftToolRepair: public Craft
{
public:
	CraftToolRepair() = delete;
	CraftToolRepair(float additionalWear);

	virtual ~CraftToolRepair() = default;

	virtual std::string GetName() const;
	virtual bool Check(const CraftInput& input, Environment* env) const;
	virtual CraftOutput GetOutput(const CraftInput& input, Environment* env) const;
	virtual CraftInput GetInput(const CraftOutput& output, Environment* env) const;
	virtual void DecrementInput(CraftInput& input,
		std::vector<ItemStack>& outputReplacements, Environment* env) const;

	virtual uint64_t GetHash(CraftHashType type) const { return 2; }

	virtual void InitHash(Environment* env)
	{
		hashType = CRAFT_HASH_TYPE_COUNT;
	}

	virtual std::string Dump() const;

private:
	// This is a constant that is added to the wear of the result.
	// May be positive or negative, allowed range [-1,1].
	// 1 = new tool is completely broken
	// 0 = simply add remaining uses of both input tools
	// -1 = new tool is completely pristine
	float additionalWear = 0.0f;
};

/*
	A cooking (in furnace) definition
	Supported crafting method: CRAFT_METHOD_COOKING.
*/
class CraftCooking: public Craft
{
public:
	CraftCooking() = delete;
	CraftCooking(
		const std::string& output,
		const std::string& recipe,
		float cooktime, const CraftReplacements& replacements);

	virtual ~CraftCooking() = default;

	virtual std::string GetName() const;
	virtual bool Check(const CraftInput& input, Environment* env) const;
	virtual CraftOutput GetOutput(const CraftInput& input, Environment* env) const;
	virtual CraftInput GetInput(const CraftOutput& output, Environment* env) const;
	virtual void DecrementInput(CraftInput& input,
		std::vector<ItemStack>& outputReplacements, Environment* env) const;

	virtual uint64_t GetHash(CraftHashType type) const;

	virtual void InitHash(Environment* env);

	virtual std::string Dump() const;

private:
	// Output itemstring
    std::string output;
	// Recipe itemstring
    std::string recipe;
	// Recipe item name
    std::string recipeName;
	// bool indicating if InitHash has been called already
	bool hashInited = false;
	// Time in seconds
	float cooktime;
	// Replacement items for DecrementInput()
	CraftReplacements replacements;
};

/*
	A fuel (for furnace) definition
	Supported crafting method: CRAFT_METHOD_FUEL.
*/
class CraftFuel: public Craft
{
public:
	CraftFuel() = delete;
	CraftFuel(
		const std::string& recipe, float burntime,
		const CraftReplacements& replacements);

	virtual ~CraftFuel() = default;

	virtual std::string GetName() const;
	virtual bool Check(const CraftInput& input, Environment* env) const;
	virtual CraftOutput GetOutput(const CraftInput& input, Environment* env) const;
	virtual CraftInput GetInput(const CraftOutput& output, Environment* env) const;
	virtual void DecrementInput(CraftInput& input,
		std::vector<ItemStack>& outputReplacements, Environment* env) const;

	virtual uint64_t GetHash(CraftHashType type) const;

	virtual void InitHash(Environment* env);

	virtual std::string Dump() const;

private:
	// Recipe itemstring
    std::string recipe;
	// Recipe item name
    std::string recipeName;
	// bool indicating if InitHash has been called already
	bool hashInited = false;
	// Time in seconds
	float burntime;
	// Replacement items for DecrementInput()
	CraftReplacements replacements;
};

/*
	Crafting manager
*/
class BaseCraftManager
{
public:
	BaseCraftManager() = default;
	virtual ~BaseCraftManager() = default;

	/**
	 * The main crafting function.
	 *
	 * @param input The input grid.
	 * @param output CraftOutput where the result is placed.
	 * @param outputReplacements A vector of ItemStacks where replacements are
	 * placed if they cannot be placed in the input. Replacements can be placed
	 * in the input if the stack of the replaced item has a count of 1.
	 * @param DecrementInput If true, consume or replace input items.
	 * @param game
	 * @return true if a result was found, otherwise false.
	 */
	virtual bool GetCraftResult(CraftInput& input, CraftOutput& output,
			std::vector<ItemStack>& outputReplacements,
			bool decrementInput, Environment* env) const=0;

	virtual std::vector<Craft*> GetCraftRecipes(CraftOutput& output,
			Environment* env, unsigned limit=0) const=0;

	// Print crafting recipes for debugging
	virtual std::string Dump() const=0;
};

class BaseWritableCraftManager : public BaseCraftManager
{
public:
	BaseWritableCraftManager() = default;
	virtual ~BaseWritableCraftManager() = default;

	// The main crafting function
	virtual bool GetCraftResult(CraftInput& input, CraftOutput& output,
			std::vector<ItemStack>& outputReplacements,
			bool decrementInput, Environment* env) const=0;
	virtual std::vector<Craft*> GetCraftRecipes(CraftOutput& output,
			Environment* env, unsigned limit=0) const=0;

	virtual bool ClearCraftsByOutput(const CraftOutput& output, Environment* env) = 0;
	virtual bool ClearCraftsByInput(const CraftInput& input, Environment* env) = 0;

	// Print crafting recipes for debugging
	virtual std::string Dump() const=0;

	// Add a crafting definition.
	// After calling this, the pointer belongs to the manager.
	virtual void RegisterCraft(Craft* def, Environment* env) = 0;

	// Delete all crafting definitions
	virtual void Clear()=0;

	// To be called after all mods are loaded, so that we catch all aliases
	virtual void InitHashes(Environment* env) = 0;
};

std::shared_ptr<BaseWritableCraftManager> CreateCraftManager();

#endif