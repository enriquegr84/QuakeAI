/*
Minetest
Copyright (C) 2016 juhdanad, Daniel Juhasz <juhdanad@gmail.com>

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

#ifndef RAYCAST_H
#define RAYCAST_H

#include "Mathematic/Geometric/Line3.h"
#include "Mathematic/Geometric/AlignedBox.h"

#include "../Games/Map/VoxelAlgorithms.h"

#include "../Utils/PointedThing.h"

//! Sorts PointedThings based on their distance.
struct RaycastSort
{
	bool operator() (const PointedThing& pt1, const PointedThing& pt2) const;
};

//! Describes the state of a raycast.
class RaycastState
{
public:
	/*!
	 * Creates a raycast.
	 * @param objectsPointable if false, only nodes will be found
	 * @param liquids pointable if false, liquid nodes won't be found
	 */
	RaycastState(const Line3<float>& shootline, bool objectsPointable, bool liquidsPointable);

	//! Shootline of the raycast.
	Line3<float> mShootLine;
	//! Iterator to store the progress of the raycast.
	VoxelLineIterator mIterator;
	//! Previous tested node during the raycast.
	Vector3<short> mPreviousNode;

	/*!
	 * This priority queue stores the found pointed things
	 * waiting to be returned.
	 */
	std::priority_queue<PointedThing, std::vector<PointedThing>, RaycastSort> mFoundThings;

	bool mObjectsPointable;
	bool mLiquidsPointable;

	//! The code needs to search these nodes around the center node.
    BoundingBox<short> mSearchRange { 0, 0, 0, 0, 0, 0 };

	//! If true, the Environment will initialize this state.
	bool mInitializationNeeded = true;
};

/*!
 * Checks if a line and a box intersects.
 * @param[in]  box              box to test collision
 * @param[in]  start            starting point of the line
 * @param[in]  dir              direction and length of the line
 * @param[out] collisionPoint  first point of the collision
 * @param[out] collisionNormal normal vector at the collision, points
 * outwards of the surface. If start is in the box, zero vector.
 * @returns true if a collision point was found
 */
bool BoxLineCollision(const BoundingBox<float>& box,
    const Vector3<float>& start, const Vector3<float>& dir,
	Vector3<float>* collisionPoint, Vector3<short>* collisionNormal);

#endif