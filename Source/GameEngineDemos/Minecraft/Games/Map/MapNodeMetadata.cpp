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

#include "MapNodeMetadata.h"

#include "../../Games/Actors/Inventory.h"

#include "Core/Utility/Serialize.h"

/*
	MapNodeMetadata
*/

MapNodeMetadata::MapNodeMetadata(BaseItemManager* itemMgr) :
	mInventory(new Inventory(itemMgr))
{

}

MapNodeMetadata::~MapNodeMetadata()
{
	delete mInventory;
}

void MapNodeMetadata::Serialize(std::ostream& os, uint8_t version, bool disk) const
{
	int numVars = disk ? (int)mStringVars.size() : CountNonPrivate();
	WriteUInt32(os, numVars);
	for (const auto& sv : mStringVars) 
    {
		bool priv = IsPrivate(sv.first);
		if (!disk && priv)
			continue;

		os << SerializeString16(sv.first);
		os << SerializeString32(sv.second);
		if (version >= 2)
			WriteUInt8(os, (priv) ? 1 : 0);
	}

	mInventory->Serialize(os);
}

void MapNodeMetadata::Deserialize(std::istream& is, uint8_t version)
{
	Clear();
	int numVars = ReadUInt32(is);
	for(int i=0; i<numVars; i++)
    {
		std::string name = DeserializeString16(is);
        std::string var = DeserializeString32(is);
		mStringVars[name] = var;
		if (version >= 2) 
			if (ReadUInt8(is) == 1)
				MarkPrivate(name, true);
	}

	mInventory->Deserialize(is);
}

void MapNodeMetadata::Clear()
{
	Metadata::Clear();
	mPrivateVars.clear();
	mInventory->Clear();
}

bool MapNodeMetadata::Empty() const
{
	return Metadata::Empty() && mInventory->GetLists().empty();
}


void MapNodeMetadata::MarkPrivate(const std::string& name, bool set)
{
	if (set)
		mPrivateVars.insert(name);
	else
		mPrivateVars.erase(name);
}

int MapNodeMetadata::CountNonPrivate() const
{
	// mPrivateVars can contain names not actually present
	// DON'T: return mStringVars.size() - mPrivateVars.size();
	int n = 0;
	for (const auto& sv : mStringVars) 
		if (!IsPrivate(sv.first))
			n++;

	return n;
}

/*
	MapNodeMetadataList
*/

void MapNodeMetadataList::Serialize(std::ostream& os, bool disk, bool absolutePos) const
{
	/*
		Version 0 is a placeholder for "nothing to see here; go away."
	*/
	uint16_t count = CountNonEmpty();
	if (count == 0) 
    {
		WriteUInt8(os, 0); // version
		return;
	}

	uint8_t version = 2;
	WriteUInt8(os, version);
	WriteUInt16(os, count);

	for (MapNodeMetadataMap::const_iterator i = mData.begin(); i != mData.end(); ++i) 
    {
		Vector3<short> p = i->first;
		MapNodeMetadata* data = i->second;
		if (data->Empty())
			continue;

		if (absolutePos) 
        {
			WriteInt16(os, p[0]);
			WriteInt16(os, p[1]);
			WriteInt16(os, p[2]);
		} 
        else 
        {
			// Serialize positions within a mapblock
			uint16_t p16 = (p[2] * MAP_BLOCKSIZE + p[1]) * MAP_BLOCKSIZE + p[0];
			WriteUInt16(os, p16);
		}
		data->Serialize(os, version, disk);
	}
}

void MapNodeMetadataList::Deserialize(std::istream& is, BaseItemManager* itemMgr, bool absolutePos)
{
	Clear();

	uint8_t version = ReadUInt8(is);

	if (version == 0) 
    {
		// Nothing
		return;
	}

	if (version > 2) 
    {
		std::string errStr = std::string(FUNCTION_NAME)
			+ ": version " + std::to_string(version) + " not supported";
		LogError(errStr);
		throw SerializationError(errStr);
	}

	uint16_t count = ReadUInt16(is);

	for (uint16_t i = 0; i < count; i++) 
    {
		Vector3<short> p;
		if (absolutePos) 
        {
			p[0] = ReadInt16(is);
			p[1] = ReadInt16(is);
			p[2] = ReadInt16(is);
		} 
        else 
        {
			uint16_t p16 = ReadUInt16(is);
			p[0] = p16 & (MAP_BLOCKSIZE - 1);
			p16 /= MAP_BLOCKSIZE;
			p[1] = p16 & (MAP_BLOCKSIZE - 1);
			p16 /= MAP_BLOCKSIZE;
			p[2] = p16;
		}
		if (mData.find(p) != mData.end()) 
        {
			LogWarning( "MapNodeMetadataList::deSerialize(): "
                "already set data at position (" + std::to_string(p[0]) + "," +
                std::to_string(p[1]) + "," + std::to_string(p[2]) + "): Ignoring.");
			continue;
		}

		MapNodeMetadata* data = new MapNodeMetadata(itemMgr);
		data->Deserialize(is, version);
		mData[p] = data;
	}
}

MapNodeMetadataList::~MapNodeMetadataList()
{
	Clear();
}

std::vector<Vector3<short>> MapNodeMetadataList::GetAllKeys()
{
	std::vector<Vector3<short>> keys;
	keys.reserve(mData.size());
	for (const auto &it : mData)
		keys.push_back(it.first);

	return keys;
}

MapNodeMetadata* MapNodeMetadataList::Get(Vector3<short> pos)
{
	MapNodeMetadataMap::const_iterator n = mData.find(pos);
	if (n == mData.end())
		return nullptr;
	return n->second;
}

void MapNodeMetadataList::Remove(Vector3<short> pos)
{
	MapNodeMetadata* olddata = Get(pos);
	if (olddata)
    {
		if (mIsMetadataOwner)
			delete olddata;
		mData.erase(pos);
	}
}

void MapNodeMetadataList::Set(Vector3<short> pos, MapNodeMetadata* d)
{
	Remove(pos);
	mData.emplace(pos, d);
}

void MapNodeMetadataList::Clear()
{
	if (mIsMetadataOwner) 
    {
		MapNodeMetadataMap::const_iterator it;
		for (it = mData.begin(); it != mData.end(); ++it)
			delete it->second;
	}
	mData.clear();
}

int MapNodeMetadataList::CountNonEmpty() const
{
	int n = 0;
	for (const auto& it : mData) 
		if (!it.second->Empty())
			n++;

	return n;
}
