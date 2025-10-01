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

#include "ContentNodeMeta.h"

#include "../../Graphics/Node.h"

#include "../../Games/Actors/Inventory.h"

#include "MapNodeMetadata.h"
#include "MapNode.h"

#include "Core/Utility/Serialize.h"

#define NODEMETA_GENERIC 1
#define NODEMETA_SIGN 14
#define NODEMETA_CHEST 15
#define NODEMETA_FURNACE 16
#define NODEMETA_LOCKABLE_CHEST 17

// Returns true if node timer must be set
static bool ContentNodeMetaDeserializeLegacyBody(
		std::istream& is, short id, MapNodeMetadata* meta)
{
	meta->Clear();

	if(id == NODEMETA_GENERIC) // GenericMapNodeMetadata (0.4-dev)
	{
		meta->GetInventory()->Deserialize(is);
		DeserializeString32(is);  // m_text
		DeserializeString16(is);  // m_owner

		meta->SetString("infotext",DeserializeString16(is));
		meta->SetString("formspec",DeserializeString16(is));
		ReadUInt8(is);  // m_allow_text_input
		ReadUInt8(is);  // m_allow_removal
		ReadUInt8(is);  // m_enforce_owner

		int numVars = ReadUInt32(is);
		for(int i=0; i<numVars; i++)
        {
            std::string name = DeserializeString16(is);
            std::string var = DeserializeString32(is);
			meta->SetString(name, var);
		}
		return false;
	}
	else if(id == NODEMETA_SIGN) // SignMapNodeMetadata
	{
		meta->SetString("text", DeserializeString16(is));
		//meta->SetString("infotext","\"${text}\"");
		meta->SetString("infotext",
				std::string("\"") + meta->GetString("text") + "\"");
		meta->SetString("formspec","field[text;;${text}]");
		return false;
	}
	else if(id == NODEMETA_CHEST) // ChestMapNodeMetadata
	{
		meta->GetInventory()->Deserialize(is);

		// Rename inventory list "0" to "main"
		Inventory* inv = meta->GetInventory();
		if(!inv->GetList("main") && inv->GetList("0"))
			inv->GetList("0")->SetName("main");

		LogAssert(inv->GetList("main") && !inv->GetList("0"), "Invalid list");

		meta->SetString("formspec","size[8,9]"
				"list[current_name;main;0,0;8,4;]"
				"list[current_player;main;0,5;8,4;]");
		return false;
	}
	else if(id == NODEMETA_LOCKABLE_CHEST) // LockingChestMapNodeMetadata
	{
		meta->SetString("owner", DeserializeString16(is));
		meta->GetInventory()->Deserialize(is);

		// Rename inventory list "0" to "main"
		Inventory* inv = meta->GetInventory();
		if(!inv->GetList("main") && inv->GetList("0"))
			inv->GetList("0")->SetName("main");

		LogAssert(inv->GetList("main") && !inv->GetList("0"), "Invalid list");

		meta->SetString("formspec","size[8,9]"
				"list[current_name;main;0,0;8,4;]"
				"list[current_player;main;0,5;8,4;]");
		return false;
	}
	else if(id == NODEMETA_FURNACE) // FurnaceMapNodeMetadata
	{
		meta->GetInventory()->Deserialize(is);
		int temp = 0;
		is>>temp;
		meta->SetString("fuel_totaltime", std::to_string((float)temp/10));
		temp = 0;
		is>>temp;
		meta->SetString("fuel_time", std::to_string((float)temp/10));
		temp = 0;
		is>>temp;
		//meta->SetString("src_totaltime", std::to_string((float)temp/10));
		temp = 0;
		is>>temp;
		meta->SetString("src_time", std::to_string((float)temp/10));

		meta->SetString("formspec","size[8,9]"
			"list[current_name;fuel;2,3;1,1;]"
			"list[current_name;src;2,1;1,1;]"
			"list[current_name;dst;5,1;2,2;]"
			"list[current_player;main;0,5;8,4;]");
		return true;
	}
	else
	{
		throw SerializationError("Unknown legacy node metadata");
	}
}

static bool ContentNodeMetaDeserializeLegacyMeta(std::istream& is, MapNodeMetadata* meta)
{
	// Read id
	short id = ReadInt16(is);

	// Read data
	std::string data = DeserializeString16(is);
	std::istringstream tmpIs(data, std::ios::binary);
	return ContentNodeMetaDeserializeLegacyBody(tmpIs, id, meta);
}

void ContentNodeMetaDeserializeLegacy(std::istream& is,
    MapNodeMetadataList* meta, NodeTimerList* timers, BaseItemManager* itemMgr)
{
	meta->Clear();
	timers->Clear();

	uint16_t version = ReadUInt16(is);

	if(version > 1)
	{
		LogInformation("ContentNodeMeta: version " + std::to_string(version) + " not supported");
		throw SerializationError(FUNCTION_NAME);
	}

	short count = ReadUInt16(is);
	for(short i=0; i<count; i++)
	{
		short p16 = ReadUInt16(is);

        Vector3<short> p{ 0,0,0 };
		p[2] += p16 / MAP_BLOCKSIZE / MAP_BLOCKSIZE;
		p16 -= p[2] * MAP_BLOCKSIZE * MAP_BLOCKSIZE;
		p[1] += p16 / MAP_BLOCKSIZE;
		p16 -= p[1] * MAP_BLOCKSIZE;
		p[0] += p16;

		if(meta->Get(p) != NULL)
		{
			LogWarning("ContentNodeMeta: already set data at position (" + 
                std::to_string(p[0]) + "," + std::to_string(p[1]) + "," + 
                std::to_string(p[2]) + "): Ignoring.");
			continue;
		}

		MapNodeMetadata* data = new MapNodeMetadata(itemMgr);
		bool needTimer = ContentNodeMetaDeserializeLegacyMeta(is, data);
		meta->Set(p, data);

		if(needTimer)
			timers->Set(NodeTimer(1., 0., p));
	}
}
