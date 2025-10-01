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

#ifndef POINTEDTHING_H
#define POINTEDTHING_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector3.h"

enum PointedThingType
{
	POINTEDTHING_NOTHING,
	POINTEDTHING_NODE,
	POINTEDTHING_OBJECT
};

//! An active object or node which is selected by a ray on the map.
struct PointedThing
{
	//! The type of the pointed object.
	PointedThingType type = POINTEDTHING_NOTHING;
	/*!
	 * Only valid if type is POINTEDTHING_NODE.
	 * The coordinates of the node which owns the
	 * nodebox that the ray hits first.
	 * This may differ from nodeRealUndersurface if
	 * a nodebox exceeds the limits of its node.
	 */
	Vector3<short> nodeUndersurface;
	/*!
	 * Only valid if type is POINTEDTHING_NODE.
	 * The coordinates of the last node the ray intersects
	 * before nodeUndersurface. Same as nodeUndersurface
	 * if the ray starts in a nodebox.
	 */
	Vector3<short> nodeAbovesurface;
	/*!
	 * Only valid if type is POINTEDTHING_NODE.
	 * The coordinates of the node which contains the
	 * point of the collision and the nodebox of the node.
	 */
	Vector3<short> nodeRealUndersurface;
	/*!
	 * Only valid if type is POINTEDTHING_OBJECT.
	 * The ID of the object the ray hit.
	 */
	short objectId = -1;
	/*!
	 * Only valid if type isn't POINTEDTHING_NONE.
	 * First intersection point of the ray and the nodebox in game engine
	 * coordinates.
	 */
	Vector3<float> intersectionPoint;
	/*!
	 * Only valid if type isn't POINTEDTHING_NONE.
	 * Normal vector of the intersection.
	 * This is perpendicular to the face the ray hits,
	 * points outside of the box and it's length is 1.
	 */
	Vector3<short> intersectionNormal;
	/*!
	 * Only valid if type isn't POINTEDTHING_NONE.
	 * Indicates which selection box is selected, if there are more of them.
	 */
	unsigned short boxId = 0;
	/*!
	 * Square of the distance between the pointing
	 * ray's start point and the intersection point in game engine coordinates.
	 */
	float distanceSq = 0;

	//! Constructor for POINTEDTHING_NOTHING
	PointedThing() = default;
	//! Constructor for POINTEDTHING_NODE
	PointedThing(
        const Vector3<short> &under, const Vector3<short>& above, 
        const Vector3<short>& realUnder, const Vector3<float>& point, 
        const Vector3<short>& normal, unsigned short boxId, float distSq);

	//! Constructor for POINTEDTHING_OBJECT
	PointedThing(short id, const Vector3<float>& point, const Vector3<short>& normal, float distSq);
	std::string Dump() const;
	void Serialize(std::ostream &os) const;
	void Deserialize(std::istream &is);
	/*!
	 * This function ignores the intersection point and normal.
	 */
	bool operator==(const PointedThing& pt2) const;
	bool operator!=(const PointedThing& pt2) const;
};

#endif