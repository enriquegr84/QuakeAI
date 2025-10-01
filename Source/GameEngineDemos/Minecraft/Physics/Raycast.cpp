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

#include "Raycast.h"

bool RaycastSort::operator() (const PointedThing& pt1, const PointedThing& pt2) const
{
	// "nothing" can not be sorted
	LogAssert(pt1.type != POINTEDTHING_NOTHING, "pointed nothing");
    LogAssert(pt2.type != POINTEDTHING_NOTHING, "pointed nothing");
	float pt1DistSq = pt1.distanceSq;

	// Add some bonus when one of them is an object
	if (pt1.type != pt2.type) 
    {
		if (pt1.type == POINTEDTHING_OBJECT)
			pt1DistSq -= BS * BS;
		else if (pt2.type == POINTEDTHING_OBJECT)
			pt1DistSq += BS * BS;
	}

	// returns false if pt1 is nearer than pt2
	if (pt1DistSq < pt2.distanceSq)
		return false;

	if (pt1DistSq == pt2.distanceSq) 
    {
		// Sort them to allow only one order
		if (pt1.type == POINTEDTHING_OBJECT)
			return (pt2.type == POINTEDTHING_OBJECT && pt1.objectId < pt2.objectId);

		return (pt2.type == POINTEDTHING_OBJECT || pt1.nodeUndersurface < pt2.nodeUndersurface);
	}
	return true;
}


RaycastState::RaycastState(const Line3<float> &shootline, bool objectsPointable, bool liquidsPointable) :
	mShootLine(shootline), mIterator(shootline.mStart / BS, shootline.GetVector() / BS),
	mPreviousNode(mIterator.mCurrentNodePos), mObjectsPointable(objectsPointable),
	mLiquidsPointable(liquidsPointable)
{

}

bool BoxLineCollision(const BoundingBox<float>& box, const Vector3<float>& start,
	const Vector3<float>& dir, Vector3<float>* collisionPoint, Vector3<short>* collisionNormal)
{
	if (box.IsPointInside(start)) 
    {
		*collisionPoint = start;
        *collisionNormal = Vector3<short>::Zero();
		return true;
	}
	float m = 0;

	// Test X collision
	if (dir[0] != 0) 
    {
		if (dir[0] > 0)
			m = (box.mMinEdge[0] - start[0]) / dir[0];
		else
			m = (box.mMaxEdge[0] - start[0]) / dir[0];

		if (m >= 0 && m <= 1) 
        {
			*collisionPoint = start + dir * m;
			if ((*collisionPoint)[1] >= box.mMinEdge[1] &&
                (*collisionPoint)[1] <= box.mMaxEdge[1] &&
                (*collisionPoint)[2] >= box.mMinEdge[2] &&
                (*collisionPoint)[2] <= box.mMaxEdge[2])
            {
                if (dir[0] > 0)
                    (*collisionNormal) = Vector3<short>{ -1, 0, 0 };
                else
                    (*collisionNormal) = Vector3<short>{ 1, 0, 0 };
				return true;
			}
		}
	}

	// Test Y collision
	if (dir[1] != 0) 
    {
		if (dir[1] > 0)
			m = (box.mMinEdge[1] - start[1]) / dir[1];
		else
			m = (box.mMaxEdge[1] - start[1]) / dir[1];

		if (m >= 0 && m <= 1) 
        {
			*collisionPoint = start + dir * m;
			if ((*collisionPoint)[0] >= box.mMinEdge[0] &&
                (*collisionPoint)[0] <= box.mMaxEdge[0] &&
                (*collisionPoint)[2] >= box.mMinEdge[2] &&
                (*collisionPoint)[2] <= box.mMaxEdge[2])
            {
                if (dir[1] > 0)
                    (*collisionNormal) = Vector3<short>{ 0, -1, 0 };
                else
                    (*collisionNormal) = Vector3<short>{ 0, 1, 0 };
				return true;
			}
		}
	}

	// Test Z collision
	if (dir[2] != 0) 
    {
		if (dir[2] > 0)
			m = (box.mMinEdge[2] - start[2]) / dir[2];
		else
			m = (box.mMaxEdge[2] - start[2]) / dir[2];

		if (m >= 0 && m <= 1) 
        {
			*collisionPoint = start + dir * m;
			if ((*collisionPoint)[0] >= box.mMinEdge[0] &&
                (*collisionPoint)[0] <= box.mMaxEdge[0] &&
                (*collisionPoint)[1] >= box.mMinEdge[1] &&
                (*collisionPoint)[1] <= box.mMaxEdge[1])
            {
                if (dir[2] > 0)
                    (*collisionNormal) = Vector3<short>{ 0, 0, -1 };
                else
                    (*collisionNormal) = Vector3<short>{ 0, 0, 1 };
				return true;
			}
		}
	}
	return false;
}
