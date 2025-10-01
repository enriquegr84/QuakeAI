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

#include "Craft.h"

#include "../Environment/Environment.h"

#include "Core/Utility/StringUtil.h"

#include "Mathematic/Arithmetic/BitHacks.h"


inline bool IsGroupRecipeStr(const std::string& recName)
{
	return StringStartsWith(recName, "group:");
}

static bool HasGroupItem(const std::vector<std::string>& recipe)
{
	for (const auto& item : recipe) 
    {
		if (IsGroupRecipeStr(item))
			return true;
	}
	return false;
}

inline uint64_t GetHashForString(const std::string& recipeStr)
{
	/*errorstream << "Hashing craft string  \"" << recipe_str << '"';*/
	return MurmurHash64ua(recipeStr.data(), (int)recipeStr.length(), 0xdeadbeef);
}

static uint64_t GetHashForGrid(CraftHashType type, const std::vector<std::string>& gridNames)
{
	switch (type) 
    {
		case CRAFT_HASH_TYPE_ITEM_NAMES: 
        {
			std::ostringstream os;
			bool isFirst = true;
			for (const std::string &gridName : gridNames) 
            {
				if (!gridName.empty()) 
                {
					os << (isFirst ? "" : "\n") << gridName;
                    isFirst = false;
				}
			}
			return GetHashForString(os.str());
		} 
        case CRAFT_HASH_TYPE_COUNT: 
        {
			uint64_t cnt = 0;
			for (const std::string& gridName : gridNames)
				if (!gridName.empty())
					cnt++;
			return cnt;
		} 
        case CRAFT_HASH_TYPE_UNHASHED:
			return 0;
	}
	// invalid CraftHashType
	LogAssert(false, "invalid hash");
	return 0;
}

// Check if input matches recipe
// Takes recipe groups into account
static bool InputItemMatchesRecipe(const std::string& inpName,
		const std::string& recName, BaseItemManager* itemMgr)
{
	// Exact name
	if (inpName == recName)
		return true;

	// Group
	if (IsGroupRecipeStr(recName) && itemMgr->IsKnown(inpName)) 
    {
		const struct Item& item = itemMgr->Get(inpName);
		Strfnd f(recName.substr(6));
		bool allGroupsMatch = true;
		do 
        {
			std::string checkGroup = f.Next(",");
			if (ItemGroupGet(item.groups, checkGroup) == 0)
            {
				allGroupsMatch = false;
				break;
			}
		} while (!f.AtEnd());
		if (allGroupsMatch)
			return true;
	}

	// Didn't match
	return false;
}

// Deserialize an itemstring then return the name of the item
static std::string CraftGetItemName(const std::string& itemString, Environment* env)
{
	ItemStack item;
	item.Deserialize(itemString, env->GetItemManager());
	return item.name;
}

// (mapcar CraftGetItemName itemstrings)
static std::vector<std::string> CraftGetItemNames(
		const std::vector<std::string>& itemStrings, Environment* env)
{
	std::vector<std::string> result;
	result.reserve(itemStrings.size());
	for (const auto& itemString : itemStrings) 
		result.push_back(CraftGetItemName(itemString, env));

	return result;
}

// Get name of each item, and return them as a new list.
static std::vector<std::string> CraftGetItemNames(
		const std::vector<ItemStack>& items, Environment* env)
{
	std::vector<std::string> result;
	result.reserve(items.size());
	for (const auto& item : items)
		result.push_back(item.name);

	return result;
}

// convert a list of item names, to ItemStacks.
static std::vector<ItemStack> CraftGetItems(
		const std::vector<std::string>& items, Environment* env)
{
	std::vector<ItemStack> result;
	result.reserve(items.size());
	for (const auto& item : items) 
    {
		result.emplace_back(std::string(item), (uint16_t)1,
			(uint16_t)0, env->GetItemManager());
	}
	return result;
}

// Compute bounding rectangle given a matrix of items
// Returns false if every item is ""
static bool CraftGetBounds(const std::vector<std::string>& items, unsigned int width,
		unsigned int& minX, unsigned int& maxX, unsigned int& minY, unsigned int& maxY)
{
	bool success = false;
	unsigned int x = 0;
	unsigned int y = 0;
	for (const std::string& item : items) 
    {
		// Is this an actual item?
		if (!item.empty()) 
        {
			if (!success) 
            {
				// This is the first nonempty item
				minX = maxX = x;
				minY = maxY = y;
				success = true;
			} 
            else 
            {
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
		}

		// Step coordinate
		x++;
		if (x == width) 
        {
			x = 0;
			y++;
		}
	}
	return success;
}

// Removes 1 from each item stack
static void CraftDecrementInput(CraftInput& input, Environment* env)
{
	for (auto& item : input.items) 
		if (item.count != 0)
			item.Remove(1);
}

// Removes 1 from each item stack with replacement support
// Example: if replacements contains the pair ("bucket:bucket_water", "bucket:bucket_empty"),
//   a water bucket will not be removed but replaced by an empty bucket.
static void CraftDecrementOrReplaceInput(
    CraftInput& input, std::vector<ItemStack>& outputReplacements,
    const CraftReplacements& replacements, Environment* env)
{
	if (replacements.pairs.empty()) 
    {
		CraftDecrementInput(input, env);
		return;
	}

	// Make a copy of the replacements pair list
	std::vector<std::pair<std::string, std::string> > pairs = replacements.pairs;
	for (auto &item : input.items) 
    {
		// Find an appropriate replacement
		bool foundReplacement = false;
		for (auto pair = pairs.begin(); pair != pairs.end(); ++pair)
        {
			if (InputItemMatchesRecipe(item.name, pair->first, env->GetItemManager()))
            {
				if (item.count == 1) 
                {
					item.Deserialize(pair->second, env->GetItemManager());
					foundReplacement = true;
					pairs.erase(pair);
					break;
				}

				ItemStack rep;
				rep.Deserialize(pair->second, env->GetItemManager());
				item.Remove(1);
				foundReplacement = true;
				outputReplacements.push_back(rep);
				break;

			}
		}
		// No replacement was found, simply decrement count by one
		if (!foundReplacement && item.count > 0)
			item.Remove(1);
	}
}

// Dump an itemstring matrix
static std::string CraftDumpMatrix(const std::vector<std::string>& items, unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os << "{ ";
	unsigned int x = 0;
	for(std::vector<std::string>::size_type i = 0; i < items.size(); i++, x++) 
    {
		if (x == width) 
        {
			os << "; ";
			x = 0;
		} 
        else if (x != 0) 
        {
			os << ",";
		}
		os << '"' << items[i] << '"';
	}
	os << " }";
	return os.str();
}

// Dump an item matrix
std::string CraftDumpMatrix(const std::vector<ItemStack>& items, unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os << "{ ";
	unsigned int x = 0;
	for (std::vector<ItemStack>::size_type i = 0; i < items.size(); i++, x++) 
    {
		if (x == width) 
        {
			os << "; ";
			x = 0;
		} 
        else if (x != 0) 
        {
			os << ",";
		}
		os << '"' << items[i].GetItemString() << '"';
	}
	os << " }";
	return os.str();
}


/*
	CraftInput
*/
bool CraftInput::Empty() const
{
	for (const auto &item : items) 
		if (!item.IsEmpty())
			return false;

	return true;
}

std::string CraftInput::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(method=" << ((int)method) << ", items="
		<< CraftDumpMatrix(items, width) << ")";
	return os.str();
}

/*
	CraftOutput
*/
std::string CraftOutput::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(item=\"" << item << "\", time=" << time << ")";
	return os.str();
}

/*
	CraftReplacements
*/
std::string CraftReplacements::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os<<"{";
	const char *sep = "";
	for (const auto& replPair : pairs) 
    {
		os << sep << '"' << replPair.first
			<< "\"=>\"" << replPair.second << '"';
		sep = ",";
	}
	os << "}";
	return os.str();
}

/*
	CraftShaped
*/

CraftShaped::CraftShaped(
		const std::string& aOutput, unsigned int aWidth,
		const std::vector<std::string>& aRecipe,
		const CraftReplacements& aReplacements):
	output(aOutput), width(aWidth), recipe(aRecipe), replacements(aReplacements)
{
	if (HasGroupItem(recipe))
		priority = PRIORITY_SHAPED_AND_GROUPS;
	else
		priority = PRIORITY_SHAPED;
}

std::string CraftShaped::GetName() const
{
	return "shaped";
}

bool CraftShaped::Check(const CraftInput& input, Environment* env) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Get input item matrix
	std::vector<std::string> inpNames = CraftGetItemNames(input.items, env);
	unsigned int inpWidth = input.width;
	if (inpWidth == 0)
		return false;
	while (inpNames.size() % inpWidth != 0)
		inpNames.emplace_back("");

	// Get input bounds
	unsigned int inpMinX = 0, inpMaxX = 0, inpMinY = 0, inpMaxY = 0;
	if (!CraftGetBounds(inpNames, inpWidth, inpMinX, inpMaxX, inpMinY, inpMaxY))
		return false;  // it was empty

	std::vector<std::string> recNames;
	if (hashInited)
		recNames = recipeNames;
	else
		recNames = CraftGetItemNames(recipe, env);

	// Get recipe item matrix
	unsigned int recWidth = width;
	if (recWidth == 0)
		return false;
	while (recNames.size() % recWidth != 0)
		recNames.emplace_back("");

	// Get recipe bounds
	unsigned int recMinX=0, recMaxX=0, recMinY=0, recMaxY=0;
	if (!CraftGetBounds(recNames, recWidth, recMinX, recMaxX, recMinY, recMaxY))
		return false;  // it was empty

	// Different sizes?
	if (inpMaxX - inpMinX != recMaxX - recMinX || 
        inpMaxY - inpMinY != recMaxY - recMinY)
		return false;

	// Verify that all item names in the bounding box are equal
	unsigned int w = inpMaxX - inpMinX + 1;
	unsigned int h = inpMaxY - inpMinY + 1;

	for (unsigned int y=0; y < h; y++) 
    {
		unsigned int inpY = (inpMinY + y) * inpWidth;
		unsigned int recY = (recMinY + y) * recWidth;

		for (unsigned int x=0; x < w; x++) 
        {
			unsigned int inpX = inpMinX + x;
			unsigned int recX = recMinX + x;

			if (!InputItemMatchesRecipe(
					inpNames[inpY + inpX], recNames[recY + recX], env->GetItemManager()))
            {
				return false;
			}
		}
	}

	return true;
}

CraftOutput CraftShaped::GetOutput(const CraftInput& input, Environment* env) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftShaped::GetInput(const CraftOutput& output, Environment* env) const
{
	return CraftInput(CRAFT_METHOD_NORMAL,width,CraftGetItems(recipe, env));
}

void CraftShaped::DecrementInput(CraftInput& input, 
    std::vector<ItemStack>& outputReplacements, Environment* env) const
{
	CraftDecrementOrReplaceInput(input, outputReplacements, replacements, env);
}

uint64_t CraftShaped::GetHash(CraftHashType type) const
{
	LogAssert(hashInited, "hash no inited"); // Pre-condition
    LogAssert((type == CRAFT_HASH_TYPE_ITEM_NAMES)
		|| (type == CRAFT_HASH_TYPE_COUNT), "invalid hash type"); // Pre-condition

	std::vector<std::string> recNames = recipeNames;
	std::sort(recNames.begin(), recNames.end());
	return GetHashForGrid(type, recNames);
}

void CraftShaped::InitHash(Environment* env)
{
	if (hashInited)
		return;
	hashInited = true;
	recipeNames = CraftGetItemNames(recipe, env);

	if (HasGroupItem(recipeNames))
		hashType = CRAFT_HASH_TYPE_COUNT;
	else
		hashType = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftShaped::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(shaped, output=\"" << output
		<< "\", recipe=" << CraftDumpMatrix(recipe, width)
		<< ", replacements=" << replacements.Dump() << ")";
	return os.str();
}

/*
	CraftShapeless
*/

CraftShapeless::CraftShapeless(
		const std::string& aOutput,
		const std::vector<std::string>& aRecipe,
		const CraftReplacements& aReplacements):
	output(aOutput), recipe(aRecipe), replacements(aReplacements)
{
	if (HasGroupItem(recipe))
		priority = PRIORITY_SHAPELESS_AND_GROUPS;
	else
		priority = PRIORITY_SHAPELESS;
}

std::string CraftShapeless::GetName() const
{
	return "shapeless";
}

bool CraftShapeless::Check(const CraftInput& input, Environment* env) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> inputFiltered;
	for (const auto& item : input.items) 
		if (!item.name.empty())
			inputFiltered.push_back(item.name);

	// If there is a wrong number of items in input, no match
	if (inputFiltered.size() != recipe.size()) 
    {
		/*dstream<<"Number of input items ("<<inputFiltered.size()
				<<") does not match recipe size ("<<recipe.size()<<") "
				<<"of recipe with output="<<output<<std::endl;*/
		return false;
	}

	std::vector<std::string> recipeCopy;
	if (hashInited)
		recipeCopy = recipeNames;
	else 
    {
		recipeCopy = CraftGetItemNames(recipe, env);
        std::sort(recipeCopy.begin(), recipeCopy.end());
	}

	// Try with all permutations of the recipe,
	// start from the lexicographically first permutation (=sorted),
	// recipeNames is pre-sorted
	do 
    {
		// If all items match, the recipe matches
		bool allMatch = true;
		//dstream<<"Testing recipe (output="<<output<<"):";
		for (size_t i=0; i<recipe.size(); i++) 
        {
			//dstream<<" ("<<inputFiltered[i]<<" == "<<recipeCopy[i]<<")";
			if (!InputItemMatchesRecipe(inputFiltered[i], recipeCopy[i], env->GetItemManager()))
            {
				allMatch = false;
				break;
			}
		}
		//dstream<<" -> match="<<allMatch<<std::endl;
		if (allMatch)
			return true;
	} while (std::next_permutation(recipeCopy.begin(), recipeCopy.end()));

	return false;
}

CraftOutput CraftShapeless::GetOutput(const CraftInput& input, Environment* env) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftShapeless::GetInput(const CraftOutput& output, Environment* env) const
{
	return CraftInput(CRAFT_METHOD_NORMAL, 0, CraftGetItems(recipe, env));
}

void CraftShapeless::DecrementInput(CraftInput& input, 
    std::vector<ItemStack>& outputReplacements, Environment* env) const
{
	CraftDecrementOrReplaceInput(input, outputReplacements, replacements, env);
}

uint64_t CraftShapeless::GetHash(CraftHashType type) const
{
    LogAssert(hashInited, "hash no inited"); // Pre-condition
    LogAssert(type == CRAFT_HASH_TYPE_ITEM_NAMES
           || type == CRAFT_HASH_TYPE_COUNT, "invalid hash type"); // Pre-condition
	return GetHashForGrid(type, recipeNames);
}

void CraftShapeless::InitHash(Environment* env)
{
	if (hashInited)
		return;
	hashInited = true;
	recipeNames = CraftGetItemNames(recipe, env);
	std::sort(recipeNames.begin(), recipeNames.end());

	if (HasGroupItem(recipeNames))
		hashType = CRAFT_HASH_TYPE_COUNT;
	else
		hashType = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftShapeless::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(shapeless, output=\"" << output
		<< "\", recipe=" << CraftDumpMatrix(recipe, (unsigned int)recipe.size())
		<< ", replacements=" << replacements.Dump() << ")";
	return os.str();
}

/*
	CraftToolRepair
*/

CraftToolRepair::CraftToolRepair(float addWear):
	additionalWear(addWear)
{
	priority = PRIORITY_TOOLREPAIR;
}

static ItemStack RepairCraftTool(const ItemStack& item1, 
	const ItemStack& item2, float additionalWear, Environment* env)
{
	if (item1.count != 1 || item2.count != 1 || item1.name != item2.name
			|| env->GetItemManager()->Get(item1.name).type != ITEM_TOOL
			|| ItemGroupGet(env->GetItemManager()->Get(item1.name).groups, "DisableRepair") == 1)
    {
		// Failure
		return ItemStack();
	}

	int item1Uses = 65536 - (unsigned int) item1.wear;
	int item2Uses = 65536 - (unsigned int) item2.wear;
	int newUses = item1Uses + item2Uses;
	int newWear = 65536 - newUses + (int)floor(additionalWear * 65536 + 0.5);
	if (newWear >= 65536)
		return ItemStack();
	if (newWear < 0)
		newWear = 0;

	ItemStack repaired = item1;
	repaired.wear = newWear;
	return repaired;
}

std::string CraftToolRepair::GetName() const
{
	return "toolrepair";
}

bool CraftToolRepair::Check(const CraftInput& input, Environment* env) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	ItemStack item1;
	ItemStack item2;
	for (const auto& item : input.items) 
    {
		if (!item.IsEmpty()) 
        {
			if (item1.IsEmpty())
				item1 = item;
			else if (item2.IsEmpty())
				item2 = item;
			else
				return false;
		}
	}
	ItemStack repaired = RepairCraftTool(item1, item2, additionalWear, env);
	return !repaired.IsEmpty();
}

CraftOutput CraftToolRepair::GetOutput(const CraftInput& input, Environment* env) const
{
	ItemStack item1;
	ItemStack item2;
	for (const auto &item : input.items) 
    {
		if (!item.IsEmpty()) 
        {
			if (item1.IsEmpty())
				item1 = item;
			else if (item2.IsEmpty())
				item2 = item;
		}
	}
	ItemStack repaired = RepairCraftTool(item1, item2, additionalWear, env);
	return CraftOutput(repaired.GetItemString(), 0);
}

CraftInput CraftToolRepair::GetInput(const CraftOutput& output, Environment* env) const
{
	std::vector<ItemStack> stack;
	stack.emplace_back();
	return CraftInput(CRAFT_METHOD_COOKING, (unsigned int)additionalWear, stack);
}

void CraftToolRepair::DecrementInput(CraftInput& input, 
    std::vector<ItemStack>& outputReplacements, Environment* env) const
{
	CraftDecrementInput(input, env);
}

std::string CraftToolRepair::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(toolrepair, additionalWear=" << additionalWear << ")";
	return os.str();
}

/*
	CraftCooking
*/

CraftCooking::CraftCooking(
		const std::string& aOutput, const std::string& aRecipe,
		float aCooktime, const CraftReplacements& aReplacements):
	output(aOutput), recipe(aRecipe), cooktime(aCooktime), replacements(aReplacements)
{
	if (IsGroupRecipeStr(recipe))
		priority = PRIORITY_SHAPELESS_AND_GROUPS;
	else
		priority = PRIORITY_SHAPELESS;
}

std::string CraftCooking::GetName() const
{
	return "cooking";
}

bool CraftCooking::Check(const CraftInput& input, Environment* env) const
{
	if (input.method != CRAFT_METHOD_COOKING)
		return false;

	// Filter empty items out of input
	std::vector<std::string> inputFiltered;
	for (const auto &item : input.items) 
    {
		const std::string &name = item.name;
		if (!name.empty())
			inputFiltered.push_back(name);
	}

	// If there is a wrong number of items in input, no match
	if (inputFiltered.size() != 1) 
    {
		/*dstream<<"Number of input items ("<<inputFiltered.size()
				<<") does not match recipe size (1) "
				<<"of cooking recipe with output="<<output<<std::endl;*/
		return false;
	}

	// Check the single input item
	return InputItemMatchesRecipe(inputFiltered[0], recipe, env->GetItemManager());
}

CraftOutput CraftCooking::GetOutput(const CraftInput& input, Environment* env) const
{
	return CraftOutput(output, cooktime);
}

CraftInput CraftCooking::GetInput(const CraftOutput& output, Environment* env) const
{
	std::vector<std::string> rec;
	rec.push_back(recipe);
	return CraftInput(CRAFT_METHOD_COOKING, (unsigned int)cooktime, CraftGetItems(rec, env));
}

void CraftCooking::DecrementInput(CraftInput& input, 
    std::vector<ItemStack>& outputReplacements, Environment* env) const
{
	CraftDecrementOrReplaceInput(input, outputReplacements, replacements, env);
}

uint64_t CraftCooking::GetHash(CraftHashType type) const
{
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES) 
    {
		return GetHashForString(recipeName);
	}

	if (type == CRAFT_HASH_TYPE_COUNT) 
    {
		return 1;
	}

	// illegal hash type for this Craft (pre-condition)
	LogAssert(false, "invalid hash");
	return 0;
}

void CraftCooking::InitHash(Environment* env)
{
	if (hashInited)
		return;
	hashInited = true;
	recipeName = CraftGetItemName(recipe, env);

	if (IsGroupRecipeStr(recipeName))
		hashType = CRAFT_HASH_TYPE_COUNT;
	else
		hashType = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftCooking::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(cooking, output=\"" << output
		<< "\", recipe=\"" << recipe
		<< "\", cooktime=" << cooktime << ")"
		<< ", replacements=" << replacements.Dump() << ")";
	return os.str();
}

/*
	CraftFuel
*/

CraftFuel::CraftFuel(const std::string& aRecipe,
		float aBurntime, const CraftReplacements& aReplacements):
	recipe(aRecipe), burntime(aBurntime), replacements(aReplacements)
{
	if (IsGroupRecipeStr(recipeName))
		priority = PRIORITY_SHAPELESS_AND_GROUPS;
	else
		priority = PRIORITY_SHAPELESS;
}

std::string CraftFuel::GetName() const
{
	return "fuel";
}

bool CraftFuel::Check(const CraftInput& input, Environment* env) const
{
	if (input.method != CRAFT_METHOD_FUEL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> inputFiltered;
	for (const auto& item : input.items) 
    {
		const std::string& name = item.name;
		if (!name.empty())
			inputFiltered.push_back(name);
	}

	// If there is a wrong number of items in input, no match
	if (inputFiltered.size() != 1) 
    {
		/*dstream<<"Number of input items ("<<inputFiltered.size()
				<<") does not match recipe size (1) "
				<<"of fuel recipe with burntime="<<burntime<<std::endl;*/
		return false;
	}

	// Check the single input item
	return InputItemMatchesRecipe(inputFiltered[0], recipe, env->GetItemManager());
}

CraftOutput CraftFuel::GetOutput(const CraftInput& input, Environment* env) const
{
	return CraftOutput("", burntime);
}

CraftInput CraftFuel::GetInput(const CraftOutput& output, Environment* env) const
{
	std::vector<std::string> rec;
	rec.push_back(recipe);
	return CraftInput(CRAFT_METHOD_COOKING, (int)burntime, CraftGetItems(rec, env));
}

void CraftFuel::DecrementInput(CraftInput& input, 
    std::vector<ItemStack>& outputReplacements, Environment* env) const
{
	CraftDecrementOrReplaceInput(input, outputReplacements, replacements, env);
}

uint64_t CraftFuel::GetHash(CraftHashType type) const
{
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES)
		return GetHashForString(recipeName);

	if (type == CRAFT_HASH_TYPE_COUNT)
		return 1;

	// illegal hash type for this Craft (pre-condition)
	LogAssert(false, "invalid hash");
	return 0;
}

void CraftFuel::InitHash(Environment* env)
{
	if (hashInited)
		return;

	hashInited = true;
	recipeName = CraftGetItemName(recipe, env);

	if (IsGroupRecipeStr(recipeName))
		hashType = CRAFT_HASH_TYPE_COUNT;
	else
		hashType = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftFuel::Dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(fuel, recipe=\"" << recipe
		<< "\", burntime=" << burntime << ")"
		<< ", replacements=" << replacements.Dump() << ")";
	return os.str();
}

/*
	Craft definition manager
*/

class CraftManager: public BaseWritableCraftManager
{
public:
	CraftManager()
	{
		mCrafts.resize(CraftHashTypeMax + 1);
	}

	virtual ~CraftManager()
	{
		Clear();
	}

	virtual bool GetCraftResult(CraftInput& input, CraftOutput& output,
			std::vector<ItemStack>& outputReplacement, bool decrementInput, Environment* env) const
	{
		if (input.Empty())
			return false;

		std::vector<std::string> inputNames;
		inputNames = CraftGetItemNames(input.items, env);
		std::sort(inputNames.begin(), inputNames.end());

		// Try hash types with increasing collision rate
		// while remembering the latest, highest priority recipe.
		Craft::RecipePriority priorityBest = Craft::PRIORITY_NO_RECIPE;
		Craft* craftBest = nullptr;
		for (int type = 0; type <= CraftHashTypeMax; type++) 
        {
			uint64_t hash = GetHashForGrid((CraftHashType) type, inputNames);

			/*errorstream << "Checking type " << type << " with hash " << hash << std::endl;*/

			// We'd like to do "const [...] hashCollisions = mCrafts[type][hash];"
			// but that doesn't compile for some reason. This does.
			auto colIter = (mCrafts[type]).find(hash);

			if (colIter == (mCrafts[type]).end())
				continue;

			const std::vector<Craft*>& hashCollisions = colIter->second;
			// Walk crafting definitions from back to front, so that later
			// definitions can override earlier ones.
			for (std::vector<Craft*>::size_type i = hashCollisions.size(); i > 0; i--) 
            {
				Craft* craft = hashCollisions[i - 1];

				/*errorstream << "Checking " << input.dump() << std::endl
					<< " against " << craft->dump() << std::endl;*/

				Craft::RecipePriority priority = craft->GetPriority();
				if (priority > priorityBest && craft->Check(input, env))
                {
					// Check if the crafted node/item exists
					CraftOutput out = craft->GetOutput(input, env);
					ItemStack is;
					is.Deserialize(out.item, env->GetItemManager());
					if (!is.IsKnown(env->GetItemManager()))
                    {
						LogInformation("trying to craft non-existent " + out.item + ", ignoring recipe");
						continue;
					}

					output = out;
					priorityBest = priority;
					craftBest = craft;
				}
			}
		}
		if (priorityBest == Craft::PRIORITY_NO_RECIPE)
			return false;
		if (decrementInput)
			craftBest->DecrementInput(input, outputReplacement, env);
		return true;
	}

	virtual std::vector<Craft*> GetCraftRecipes(
        CraftOutput& output, Environment* env, unsigned limit=0) const
	{
		std::vector<Craft*> recipes;

		auto vecIter = mOutputCrafts.find(output.item);
		if (vecIter == mOutputCrafts.end())
			return recipes;

		const std::vector<Craft*>& vec = vecIter->second;
		recipes.reserve(limit ? std::min(limit, (unsigned int)vec.size()) : vec.size());
		for (std::vector<Craft*>::size_type i = vec.size(); i > 0; i--) 
        {
			Craft* craft = vec[i - 1];
			if (limit && recipes.size() >= limit)
				break;
			recipes.push_back(craft);
		}
		return recipes;
	}

	virtual bool ClearCraftsByOutput(const CraftOutput& output, Environment* env)
	{
		auto toClear = mOutputCrafts.find(output.item);
		if (toClear == mOutputCrafts.end())
			return false;

		for (auto craft : toClear->second) 
        {
			// Recipes are not yet hashed at this point
			std::vector<Craft *>& crafts = mCrafts[(int)CRAFT_HASH_TYPE_UNHASHED][0];
            crafts.erase(std::remove(crafts.begin(), crafts.end(), craft), crafts.end());
			delete craft;
		}
		mOutputCrafts.erase(toClear);
		return true;
	}

	virtual bool ClearCraftsByInput(const CraftInput& input, Environment* env)
	{
		if (input.Empty())
			return false;

		// Recipes are not yet hashed at this point
		std::vector<Craft*>& crafts = mCrafts[(int)CRAFT_HASH_TYPE_UNHASHED][0];
		std::vector<Craft*> newCrafts;
		bool gotHit = false;
		for (auto craft : crafts) 
        {
			if (!craft->Check(input, env))
            {
				newCrafts.push_back(craft);
				continue;
			}
			gotHit = true;
			std::string output = craft->GetOutput(input, env).item;
			delete craft;
			auto it = mOutputCrafts.find(CraftGetItemName(output, env));
			if (it == mOutputCrafts.end())
				continue;
			std::vector<Craft*>& outCrafts = it->second;
			outCrafts.erase(std::remove(outCrafts.begin(), outCrafts.end(), craft), outCrafts.end());
		}
		if (gotHit)
			crafts.swap(newCrafts);

		return gotHit;
	}

	virtual std::string Dump() const
	{
		std::ostringstream os(std::ios::binary);
		os << "Crafting definitions:\n";
		for (int type = 0; type <= CraftHashTypeMax; ++type) 
        {
			for (auto it = mCrafts[type].begin(); it != mCrafts[type].end(); ++it) 
            {
				for (std::vector<Craft*>::size_type i = 0; i < it->second.size(); i++) 
                {
					os << "type " << type << " hash " << it->first
						<< " craft " << it->second[i]->Dump() << "\n";
				}
			}
		}
		return os.str();
	}

	virtual void RegisterCraft(Craft* craft, Environment* env)
	{
		LogInformation("RegisterCraft: registering craft definition: " + craft->Dump());
		mCrafts[(int) CRAFT_HASH_TYPE_UNHASHED][0].push_back(craft);

		CraftInput input;
		std::string outputName = CraftGetItemName(craft->GetOutput(input, env).item, env);
		mOutputCrafts[outputName].push_back(craft);
	}

	virtual void Clear()
	{
		for (int type = 0; type <= CraftHashTypeMax; ++type) 
        {
			for (auto& it : mCrafts[type]) 
            {
				for (auto& iit : it.second)
					delete iit;
				it.second.clear();
			}
			mCrafts[type].clear();
		}
		mOutputCrafts.clear();
	}

	virtual void InitHashes(Environment* env)
	{
		// Move the Crafts from the unhashed layer into layers higher up.
		std::vector<Craft*>& unhashedCrafts = mCrafts[(int) CRAFT_HASH_TYPE_UNHASHED][0];
		for (auto craft : unhashedCrafts) 
        {
			// Initialize and get the definition's hash
			craft->InitHash(env);
			CraftHashType type = craft->GetHashType();
			uint64_t hash = craft->GetHash(type);

			// Enter the definition
			mCrafts[type][hash].push_back(craft);
		}
		unhashedCrafts.clear();
	}
private:
    std::vector<std::unordered_map<uint64_t, std::vector<Craft*>>> mCrafts;
    std::unordered_map<std::string, std::vector<Craft*>> mOutputCrafts;
};

std::shared_ptr<BaseWritableCraftManager> CreateCraftManager()
{
	return std::shared_ptr<BaseWritableCraftManager>(new CraftManager());
}
