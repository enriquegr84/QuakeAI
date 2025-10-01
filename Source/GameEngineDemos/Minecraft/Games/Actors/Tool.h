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

#ifndef TOOL_H
#define TOOL_H

#include "Item.h"

struct ToolGroupCap
{
	std::unordered_map<int, float> times;
	int maxlevel = 1;
	int uses = 20;

	ToolGroupCap() = default;

	bool GetTime(int rating, float* time) const
	{
		std::unordered_map<int, float>::const_iterator i = times.find(rating);
		if (i == times.end()) 
        {
			*time = 0;
			return false;
		}
		*time = i->second;
		return true;
	}

	void ToJson(json& object) const;
	void FromJson(const json& object);
};


typedef std::unordered_map<std::string, struct ToolGroupCap> ToolGCMap;
typedef std::unordered_map<std::string, short> DamageGroup;

struct ToolCapabilities
{
	float fullPunchInterval;
	int maxDropLevel;
	ToolGCMap groupCaps;
	DamageGroup damageGroups;
	int punchAttackUses;

	ToolCapabilities(
        float fpi = 1.4f, int mdl = 1, 
        const ToolGCMap& gc = ToolGCMap(),
        const DamageGroup& dg = DamageGroup(), int pau = 0) :
		fullPunchInterval(fpi), maxDropLevel(mdl), groupCaps(gc), damageGroups(dg), punchAttackUses(pau)
	{}

	void Serialize(std::ostream& os) const;
	void Deserialize(std::istream& is);
	void SerializeJson(std::ostream& os) const;
	void DeserializeJson(std::istream& is);
};

struct DigParams
{
	bool diggable;
	// Digging time in seconds
	float time;
	// Caused wear
	unsigned short wear;
	std::string mainGroup;

	DigParams(bool aDiggable = false, float aTime = 0.0f, unsigned short aWear = 0,
			const std::string& aMainGroup = "") :
		diggable(aDiggable), time(aTime), wear(aWear), mainGroup(aMainGroup)
	{}
};

DigParams GetDigParams(const ItemGroupList& groups, const ToolCapabilities* tp);

struct HitParams
{
	short hp;
	unsigned short wear;

	HitParams(short aHp = 0, unsigned short aWear = 0) : hp(aHp), wear(aWear)
	{}
};

HitParams GetHitParams(const ItemGroupList& armorGroups, const ToolCapabilities* tp, float timeFromLastPunch);
HitParams GetHitParams(const ItemGroupList& armorGroups, const ToolCapabilities *tp);

struct PunchDamageResult
{
	bool didPunch = false;
	int damage = 0;
	int wear = 0;

	PunchDamageResult() = default;
};

struct ItemStack;

PunchDamageResult GetPunchDamage(
		const ItemGroupList& armorGroups, const ToolCapabilities* toolcap,
		const ItemStack* punchitem, float timeFromLastPunch);

float GetToolRange(const Item& itemSelected, const Item& itemHand);

#endif