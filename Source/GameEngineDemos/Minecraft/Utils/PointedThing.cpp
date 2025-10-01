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

#include "PointedThing.h"

#include "Core/Utility/Serialize.h"


PointedThing::PointedThing(
    const Vector3<short>& under, const Vector3<short>& above,
	const Vector3<short>& realUnder, const Vector3<float>& point, 
    const Vector3<short>& normal, unsigned short boxId, float distSq) :
	type(POINTEDTHING_NODE), nodeUndersurface(under),
	nodeAbovesurface(above), nodeRealUndersurface(realUnder),
	intersectionPoint(point), intersectionNormal(normal),
	boxId(boxId), distanceSq(distSq)
{

}

PointedThing::PointedThing(short id, const Vector3<float>& point, 
    const Vector3<short>& normal, float distSq) :
	type(POINTEDTHING_OBJECT), objectId(id), intersectionPoint(point),
	intersectionNormal(normal), distanceSq(distSq)
{

}

std::string PointedThing::Dump() const
{
	std::ostringstream os(std::ios::binary);
	switch (type) 
    {
	    case POINTEDTHING_NOTHING:
		    os << "[nothing]";
		    break;

	    case POINTEDTHING_NODE:
	    {
		    const Vector3<short>& u = nodeUndersurface;
		    const Vector3<short>& a = nodeAbovesurface;
		    os << "[node under=" << u[0] << "," << u[1] << "," << u[2] << " above="
			    << a[0] << "," << a[1] << "," << a[2] << "]";
	    }
        break;

	    case POINTEDTHING_OBJECT:
		    os << "[object " << objectId << "]";
		    break;

	    default:
		    os << "[unknown PointedThing]";
	}
	return os.str();
}

void PointedThing::Serialize(std::ostream& os) const
{
	WriteUInt8(os, 0); // version
	WriteUInt8(os, (uint8_t)type);
	switch (type) 
    {
	    case POINTEDTHING_NOTHING:
		    break;
	    case POINTEDTHING_NODE:
		    WriteV3Short(os, nodeUndersurface);
		    WriteV3Short(os, nodeAbovesurface);
		    break;
	    case POINTEDTHING_OBJECT:
		    WriteInt16(os, objectId);
		    break;
	}
}

void PointedThing::Deserialize(std::istream& is)
{
	int version = ReadUInt8(is);
	if (version != 0) 
        throw SerializationError("unsupported PointedThing version");
	type = (PointedThingType) ReadUInt8(is);
	switch (type) 
    {
	    case POINTEDTHING_NOTHING:
		    break;
	    case POINTEDTHING_NODE:
		    nodeUndersurface = ReadV3Short(is);
		    nodeAbovesurface = ReadV3Short(is);
		    break;
	    case POINTEDTHING_OBJECT:
		    objectId = ReadInt16(is);
		    break;
	    default:
		    throw SerializationError("unsupported PointedThingType");
	}
}

bool PointedThing::operator==(const PointedThing& pt2) const
{
	if (type != pt2.type)
		return false;

	if (type == POINTEDTHING_NODE)
	{
		if ((nodeUndersurface != pt2.nodeUndersurface) || 
            (nodeAbovesurface != pt2.nodeAbovesurface) || 
            (nodeRealUndersurface != pt2.nodeRealUndersurface))
			return false;
	}
	else if (type == POINTEDTHING_OBJECT)
	{
		if (objectId != pt2.objectId)
			return false;
	}
	return true;
}

bool PointedThing::operator!=(const PointedThing& pt2) const
{
	return !(*this == pt2);
}
