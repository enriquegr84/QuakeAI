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

#ifndef NAMEIDMAPPING_H
#define NAMEIDMAPPING_H

#include "../MinecraftStd.h"

typedef std::unordered_map<uint16_t, std::string> IdToNameMap;
typedef std::unordered_map<std::string, uint16_t> NameToIdMap;

class NameIdMapping
{
public:
	void Serialize(std::ostream& os) const;
	void Deserialize(std::istream& is);

	void Clear()
	{
		mIdToName.clear();
		mNameToId.clear();
	}

	void Set(uint16_t id, const std::string& name)
	{
		mIdToName[id] = name;
		mNameToId[name] = id;
	}

	void RemoveId(uint16_t id)
	{
        std::string name;
		bool found = GetName(id, name);
		if (!found)
			return;
		mIdToName.erase(id);
		mNameToId.erase(name);
	}

	void EraseName(const std::string& name)
	{
		uint16_t id;
		bool found = GetId(name, id);
		if (!found)
			return;
		mIdToName.erase(id);
		mNameToId.erase(name);
	}
	bool GetName(uint16_t id, std::string& result) const
	{
		IdToNameMap::const_iterator i;
		i = mIdToName.find(id);
		if (i == mIdToName.end())
			return false;
		result = i->second;
		return true;
	}
	bool GetId(const std::string& name, uint16_t& result) const
	{
		NameToIdMap::const_iterator i;
		i = mNameToId.find(name);
		if (i == mNameToId.end())
			return false;
		result = i->second;
		return true;
	}

	uint16_t Size() const { return (uint16_t)mIdToName.size(); }

private:
	IdToNameMap mIdToName;
	NameToIdMap mNameToId;
};

#endif