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

#ifndef MAPNODEMETADATA_H
#define MAPNODEMETADATA_H

#include "../../Data/Metadata.h"

#include "Mathematic/Algebra/Vector3.h"

/*
	MapNodeMetadata stores arbitary amounts of data for special blocks.
	Used for furnaces, chests and signs.

	There are two interaction methods: inventory menu and text input.
	Only one can be used for a single metadata, thus only inventory OR
	text input should exist in a metadata.
*/

class Inventory;
class BaseItemManager;

class MapNodeMetadata : public Metadata
{
public:
    MapNodeMetadata(BaseItemManager* itemMgr);
	~MapNodeMetadata();

	void Serialize(std::ostream& os, uint8_t version, bool disk=true) const;
	void Deserialize(std::istream& is, uint8_t version);

	void Clear();
	bool Empty() const;

	// The inventory
	Inventory* GetInventory() { return mInventory; }

	inline bool IsPrivate(const std::string& name) const { return mPrivateVars.count(name) != 0; }
	void MarkPrivate(const std::string& name, bool set);

private:
	int CountNonPrivate() const;

	Inventory* mInventory;
	std::unordered_set<std::string> mPrivateVars;
};


/*
	List of metadata of all the nodes of a block
*/

typedef std::map<Vector3<short>, MapNodeMetadata*> MapNodeMetadataMap;

class MapNodeMetadataList
{
public:
	MapNodeMetadataList(bool isMetadataOwner = true) :
		mIsMetadataOwner(isMetadataOwner)
	{}

	~MapNodeMetadataList();

	void Serialize(std::ostream& os, bool disk = true, bool absolutePos = false) const;
	void Deserialize(std::istream& is, BaseItemManager* itemMgr, bool absolutePos = false);

	// Add all keys in this list to the vector keys
	std::vector<Vector3<short>> GetAllKeys();
	// Get pointer to data
	MapNodeMetadata* Get(Vector3<short> pos);
	// Deletes data
	void Remove(Vector3<short> pos);
	// Deletes old data and sets a new one
	void Set(Vector3<short> pos, MapNodeMetadata* d);
	// Deletes all
	void Clear();

	size_t Size() const { return mData.size(); }

	MapNodeMetadataMap::const_iterator Begin() { return mData.begin(); }
	MapNodeMetadataMap::const_iterator End() { return mData.end(); }

private:
	int CountNonEmpty() const;

	bool mIsMetadataOwner;
	MapNodeMetadataMap mData;
};

#endif