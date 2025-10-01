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

#include "Tool.h"
#include "Inventory.h"

#include "Core/Utility/Serialize.h"

void ToolGroupCap::ToJson(json& object) const
{
	object["maxlevel"] = maxlevel;
	object["uses"] = uses;

	json timesObject;
	for (auto time : times)
		timesObject[time.first] = time.second;
	object["times"] = timesObject;
}

void ToolGroupCap::FromJson(const json& object)
{
	if (object.is_object())
    {
		if (object["maxlevel"].is_number_integer())
			maxlevel = object["maxlevel"];
		if (object["uses"].is_number_integer())
			uses = object["uses"];
		const json& timesObject = object["times"];
		if (timesObject.is_array())
        {
			unsigned int size = (unsigned int)timesObject.size();
			for (unsigned int i = 0; i < size; ++i)
				if (timesObject[i].is_number_float())
					times[i] = timesObject[i];
		}
	}
}

void ToolCapabilities::Serialize(std::ostream& os) const
{
	WriteUInt8(os, 5);
	WriteFloat(os, fullPunchInterval);
	WriteInt16(os, maxDropLevel);
	WriteUInt32(os, (unsigned int)groupCaps.size());
	for (const auto& groupcap : groupCaps) 
    {
		const std::string* name = &groupcap.first;
		const ToolGroupCap* cap = &groupcap.second;
		os << SerializeString16((*name));
		WriteInt16(os, cap->uses);
		WriteInt16(os, cap->maxlevel);
		WriteUInt32(os, (unsigned int)cap->times.size());
		for (const auto& time : cap->times) 
        {
			WriteInt16(os, time.first);
			WriteFloat(os, time.second);
		}
	}

	WriteUInt32(os, (unsigned int)damageGroups.size());

	for (const auto& damageGroup : damageGroups) 
    {
		os << SerializeString16(damageGroup.first);
		WriteInt16(os, damageGroup.second);
	}

	WriteUInt16(os, std::clamp((uint16_t)punchAttackUses, (uint16_t)0, (uint16_t)0xFFFF));
}

void ToolCapabilities::Deserialize(std::istream& is)
{
	int version = ReadUInt8(is);
	if (version < 4)
		throw SerializationError("unsupported ToolCapabilities version");

	fullPunchInterval = ReadFloat(is);
	maxDropLevel = ReadInt16(is);
	groupCaps.clear();
	unsigned int groupCaps_size = ReadUInt32(is);
	for (unsigned int i = 0; i < groupCaps_size; i++) 
    {
		std::string name = DeserializeString16(is);
		ToolGroupCap cap;
		cap.uses = ReadInt16(is);
		cap.maxlevel = ReadInt16(is);
		unsigned int timesSize = ReadUInt32(is);
		for(unsigned int i = 0; i < timesSize; i++) 
        {
			int level = ReadInt16(is);
			float time = ReadFloat(is);
			cap.times[level] = time;
		}
		groupCaps[name] = cap;
	}

	unsigned int damageGroupsSize = ReadUInt32(is);
	for (unsigned int i = 0; i < damageGroupsSize; i++) 
    {
		std::string name = DeserializeString16(is);
		short rating = ReadInt16(is);
		damageGroups[name] = rating;
	}

	if (version >= 5)
		punchAttackUses = ReadUInt16(is);
}

void ToolCapabilities::SerializeJson(std::ostream& os) const
{
	json root;
	root["fullPunchInterval"] = fullPunchInterval;
	root["maxDropLevel"] = maxDropLevel;
	root["punchAttackUses"] = punchAttackUses;

	json groupCapsObject;
	for (const auto &groupcap : groupCaps)
		groupcap.second.ToJson(groupCapsObject[groupcap.first]);
	root["groupCaps"] = groupCapsObject;

	json damageGroupsObject;
	DamageGroup::const_iterator dgiter;
	for (dgiter = damageGroups.begin(); dgiter != damageGroups.end(); ++dgiter)
		damageGroupsObject[dgiter->first] = dgiter->second;

	root["damageGroups"] = damageGroupsObject;

	os << root;
}

void ToolCapabilities::DeserializeJson(std::istream& is)
{
	json root;
	is >> root;
	if (root.is_object()) 
    {
		if (root["fullPunchInterval"].is_number_float())
			fullPunchInterval = root["fullPunchInterval"];
		if (root["maxDropLevel"].is_number_integer())
			maxDropLevel = root["maxDropLevel"];
		if (root["punchAttackUses"].is_number_integer())
			punchAttackUses = root["punchAttackUses"];

		json& groupCapsObject = root["groupCaps"];
		if (groupCapsObject.is_object()) 
        {
			for (auto groupCapItem : groupCapsObject.items())
			{
				ToolGroupCap groupCap;
				groupCap.FromJson(groupCapItem);
				groupCaps[groupCapItem.key()] = groupCap;
			}
		}

		json& damageGroupsObject = root["damageGroups"];
		if (damageGroupsObject.is_object())
		{
			for (auto damageGroupItem : damageGroupsObject.items())
				damageGroups[damageGroupItem.key()] = damageGroupItem.value();
		}
	}
}

DigParams GetDigParams(const ItemGroupList& groups, const ToolCapabilities* tp)
{
	// Group dig_immediate defaults to fixed time and no wear
	if (tp->groupCaps.find("DigImmediate") == tp->groupCaps.cend()) 
    {
		switch (ItemGroupGet(groups, "DigImmediate")) 
        {
		    case 2:
			    return DigParams(true, 0.5, 0, "DigImmediate");
		    case 3:
			    return DigParams(true, 0, 0, "DigImmediate");
		    default:
			    break;
		}
	}

	// Values to be returned (with a bit of conversion)
	bool resultDiggable = false;
	float resultTime = 0.0;
	float resultWear = 0.0;
	std::string resultMainGroup;

	int level = ItemGroupGet(groups, "Level");
	for (const auto& groupCap : tp->groupCaps) 
    {
		const ToolGroupCap& cap = groupCap.second;
		int levelDiff = cap.maxlevel - level;
		if (levelDiff < 0)
			continue;

		const std::string& groupName = groupCap.first;
		float time = 0;
		int rating = ItemGroupGet(groups, groupName);
		bool timeExists = cap.GetTime(rating, &time);
		if (!timeExists)
			continue;

		if (levelDiff > 1)
			time /= levelDiff;
		if (!resultDiggable || time < resultTime) 
        {
			resultTime = time;
			resultDiggable = true;
			if (cap.uses != 0)
				resultWear = 1.f / cap.uses / (float)pow(3.f, levelDiff);
			else
				resultWear = 0;
			resultMainGroup = groupName;
		}
	}

	return DigParams(resultDiggable, resultTime, (unsigned int)(0xFFFF * resultWear), resultMainGroup);
}

HitParams GetHitParams(const ItemGroupList& armorGroups, const ToolCapabilities* tp, float timeFromLastPunch)
{
	short damage = 0;
	float resultWear = 0.0f;
	float punchIntervalMultiplier = std::clamp(timeFromLastPunch / tp->fullPunchInterval, 0.f, 1.f);

	for (const auto& damageGroup : tp->damageGroups) 
    {
		short armor = ItemGroupGet(armorGroups, damageGroup.first);
		damage += (short)(damageGroup.second * punchIntervalMultiplier * armor / 100.f);
	}

	if (tp->punchAttackUses > 0)
		resultWear = 1.f / tp->punchAttackUses * punchIntervalMultiplier;

    return {damage, (unsigned short)(0xFFFF * resultWear)};
}

HitParams GetHitParams(const ItemGroupList& armorGroups, const ToolCapabilities* tp)
{
	return GetHitParams(armorGroups, tp, 1000000);
}

PunchDamageResult GetPunchDamage(
		const ItemGroupList& armorGroups, const ToolCapabilities* toolcap,
		const ItemStack* punchitem, float timeFromLastPunch)
{
	bool doHit = true;
	{
		if (doHit && punchitem) 
        {
            if (ItemGroupGet(armorGroups, "PunchOperable") &&
                (toolcap == NULL || punchitem->name.empty()))
            {
                doHit = false;
            }
		}

		if (doHit) 
        {
			if(ItemGroupGet(armorGroups, "Immortal"))
				doHit = false;
		}
	}

	PunchDamageResult result;
	if(doHit)
	{
		HitParams hitparams = GetHitParams(armorGroups, toolcap, timeFromLastPunch);
		result.didPunch = true;
		result.wear = hitparams.wear;
		result.damage = hitparams.hp;
	}

	return result;
}

float GetToolRange(const Item& itemSelected, const Item& itemHand)
{
	float maxD = itemSelected.range;
	float maxDHand = itemHand.range;

	if (maxD < 0 && maxDHand >= 0)
		maxD = maxDHand;
	else if (maxD < 0)
		maxD = 4.0f;

	return maxD;
}

