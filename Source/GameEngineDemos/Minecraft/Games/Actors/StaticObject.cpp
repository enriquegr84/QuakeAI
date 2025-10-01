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

#include "StaticObject.h"

#include "Core/Utility/Serialize.h"

#include "LogicActiveObject.h"

StaticObject::StaticObject(const LogicActiveObject* sObj, const Vector3<float>& pos):
	type(sObj->GetType()), position(pos)
{
	sObj->GetStaticData(&data);
}

void StaticObject::Serialize(std::ostream& os)
{
	// type
	WriteUInt8(os, type);
	// pos
    LogAssert(position[0] >= FLOAT_MIN && position[0] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(position[0] * FIXEDPOINT_FACTOR));
    LogAssert(position[1] >= FLOAT_MIN && position[1] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(position[1] * FIXEDPOINT_FACTOR));
    LogAssert(position[2] >= FLOAT_MIN && position[2] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(position[2] * FIXEDPOINT_FACTOR));

	// data
	os<<SerializeString16(data);
}

void StaticObject::Deserialize(std::istream& is, char version)
{
	// type
	type = ReadUInt8(is);
	// pos
    position[0] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
    position[1] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
    position[2] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
	// data
	data = DeserializeString16(is);
}

void StaticObjectList::Serialize(std::ostream& os)
{
	// version
	char version = 0;
	WriteUInt8(os, version);

	// count
	uint16_t count = (uint16_t)(mStored.size() + mActive.size());
	// Make sure it fits into uint16_t, else it would get truncated and cause e.g.
	// issue #2610 (Invalid block data in database: unsupported NameIdMapping version).
	if (count > 0xFFFF)
    {
		LogError("StaticObjectList::Serialize(): too many objects (" + 
            std::to_string(count) + ") in list, not writing them to disk.");
		WriteUInt16(os, 0);  // count = 0
		return;
	}
	WriteUInt16(os, count);

	for (StaticObject& sObj : mStored)
		sObj.Serialize(os);

	for (auto& active : mActive)
    {
		StaticObject sObj = active.second;
		sObj.Serialize(os);
	}
}
void StaticObjectList::Deserialize(std::istream& is)
{
	if (mActive.size()) 
    {
		LogError("StaticObjectList::Deserialize(): deserializing objects while " +
            std::to_string(mActive.size()) + " active objects already exist (not cleared). " +
			std::to_string(mStored.size()) + " stored objects _were_ cleared");
	}
	mStored.clear();

	// version
	uint8_t version = ReadUInt8(is);
	// count
	uint16_t count = ReadUInt16(is);
	for(uint16_t i = 0; i < count; i++) 
    {
		StaticObject sObj;
		sObj.Deserialize(is, version);
		mStored.push_back(sObj);
	}
}

