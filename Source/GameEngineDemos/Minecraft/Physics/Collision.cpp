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

#include "Collision.h"

#include "../Graphics/Node.h"

#include "../Graphics/Actors/VisualPlayer.h"

#include "../Games/Actors/Item.h"

#include "../Games/Environment/Environment.h"
#include "../Games/Environment/LogicEnvironment.h"
#include "../Games/Environment/VisualEnvironment.h"

#include "../Games/Map/Map.h"
#include "../Games/Map/MapNode.h"

#include "Core/Utility/Profiler.h"

struct NearbyCollisionInfo 
{
	// node
	NearbyCollisionInfo(bool isUl, int bouncy, 
        const Vector3<short>& pos, const BoundingBox<float>& box) :
		isUnloaded(isUl), obj(nullptr), bouncy(bouncy), position(pos), box(box)
	{
    
    }

	// object
	NearbyCollisionInfo(ActiveObject* obj, int bouncy, const BoundingBox<float> &box) :
		isUnloaded(false), obj(obj), bouncy(bouncy), box(box)
	{
    
    }

	inline bool IsObject() const { return obj != nullptr; }

	bool isUnloaded;
	bool isStepUp = false;
	ActiveObject* obj;
	int bouncy;
	Vector3<short> position;
	BoundingBox<float> box;
};

// Helper functions:
// Truncate floating point numbers to specified number of decimal places
// in order to move all the floating point error to one side of the correct value
static inline float Truncate(const float val, const float factor)
{
	return truncf(val * factor) / factor;
}

static inline Vector3<float> Truncate(const Vector3<float>& vec, const float factor)
{
    return Vector3<float>{
        Truncate(vec[0], factor),
        Truncate(vec[1], factor),
        Truncate(vec[2], factor)};
}

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// The time after which the collision occurs is stored in dtime.
CollisionAxis AxisAlignedCollision(
    const BoundingBox<float>& staticBox, const BoundingBox<float>& movingBox,
    const Vector3<float>& speed, float* dTime)
{
	//TimeTaker tt("AxisAlignedCollision");

	BoundingBox<float> relbox(
			(movingBox.mMaxEdge[0] - movingBox.mMinEdge[0]) + (staticBox.mMaxEdge[0] - staticBox.mMinEdge[0]),						// sum of the widths
			(movingBox.mMaxEdge[1] - movingBox.mMinEdge[1]) + (staticBox.mMaxEdge[1] - staticBox.mMinEdge[1]),
			(movingBox.mMaxEdge[2] - movingBox.mMinEdge[2]) + (staticBox.mMaxEdge[2] - staticBox.mMinEdge[2]),
			std::max(movingBox.mMaxEdge[0], staticBox.mMaxEdge[0]) - std::min(movingBox.mMinEdge[0], staticBox.mMinEdge[0]),	//outer bounding 'box' dimensions
        std::max(movingBox.mMaxEdge[1], staticBox.mMaxEdge[1]) - std::min(movingBox.mMinEdge[1], staticBox.mMinEdge[1]),
        std::max(movingBox.mMaxEdge[2], staticBox.mMaxEdge[2]) - std::min(movingBox.mMinEdge[2], staticBox.mMinEdge[2])
	);

	const float dTimeMax = *dTime;
	float innerMargin;		// the distance of clipping recovery
	float distance;
	float time;

	if (speed[1]) 
    {
		distance = relbox.mMaxEdge[1] - relbox.mMinEdge[1];
		*dTime = distance / std::abs(speed[1]);
		time = std::max(*dTime, 0.0f);

		if (*dTime <= dTimeMax)
        {
			innerMargin = std::max(-0.5f * (staticBox.mMaxEdge[1] - staticBox.mMinEdge[1]), -2.0f);

			if ((speed[1] > 0 && staticBox.mMinEdge[1] - movingBox.mMaxEdge[1] > innerMargin) ||
				(speed[1] < 0 && movingBox.mMinEdge[1] - staticBox.mMaxEdge[1] > innerMargin)) 
            {
				if ((std::max(movingBox.mMaxEdge[0] + speed[0] * time, staticBox.mMaxEdge[0])
                    - std::min(movingBox.mMinEdge[0] + speed[0] * time, staticBox.mMinEdge[0])
					- relbox.mMinEdge[0] < 0) &&
					(std::max(movingBox.mMaxEdge[2] + speed[2] * time, staticBox.mMaxEdge[2])
                    - std::min(movingBox.mMinEdge[2] + speed[2] * time, staticBox.mMinEdge[2])
					- relbox.mMinEdge[2] < 0))
					return COLLISION_AXIS_Y;
			}
		}
		else 
        {
			return COLLISION_AXIS_NONE;
		}
	}

	// NO else if here
	if (speed[0]) 
    {
		distance = relbox.mMaxEdge[0] - relbox.mMinEdge[0];
		*dTime = distance / std::abs(speed[0]);
		time = std::max(*dTime, 0.0f);

		if (*dTime <= dTimeMax)
        {
			innerMargin = std::max(-0.5f * (staticBox.mMaxEdge[0] - staticBox.mMinEdge[0]), -2.0f);

			if ((speed[0] > 0 && staticBox.mMinEdge[0] - movingBox.mMaxEdge[0] > innerMargin) ||
				(speed[0] < 0 && movingBox.mMinEdge[0] - staticBox.mMaxEdge[0] > innerMargin)) 
            {
				if ((std::max(movingBox.mMaxEdge[1] + speed[1] * time, staticBox.mMaxEdge[1])
					- std::min(movingBox.mMinEdge[1] + speed[1] * time, staticBox.mMinEdge[1])
					- relbox.mMinEdge[1] < 0) &&
					(std::max(movingBox.mMaxEdge[2] + speed[2] * time, staticBox.mMaxEdge[2])
					- std::min(movingBox.mMinEdge[2] + speed[2] * time, staticBox.mMinEdge[2])
					- relbox.mMinEdge[2] < 0)) 
					return COLLISION_AXIS_X;
			}
		} 
        else 
        {
			return COLLISION_AXIS_NONE;
		}
	}

	// NO else if here
	if (speed[2]) 
    {
		distance = relbox.mMaxEdge[2] - relbox.mMinEdge[2];
		*dTime = distance / std::abs(speed[2]);
		time = std::max(*dTime, 0.0f);

		if (*dTime <= dTimeMax)
        {
			innerMargin = std::max(-0.5f * (staticBox.mMaxEdge[2] - staticBox.mMinEdge[2]), -2.0f);

			if ((speed[2] > 0 && staticBox.mMinEdge[2] - movingBox.mMaxEdge[2] > innerMargin) ||
				(speed[2] < 0 && movingBox.mMinEdge[2] - staticBox.mMaxEdge[2] > innerMargin)) 
            {
				if ((std::max(movingBox.mMaxEdge[0] + speed[0] * time, staticBox.mMaxEdge[0])
					- std::min(movingBox.mMinEdge[0] + speed[0] * time, staticBox.mMinEdge[0])
					- relbox.mMinEdge[0] < 0) &&
                    (std::max(movingBox.mMaxEdge[1] + speed[1] * time, staticBox.mMaxEdge[1])
                    - std::min(movingBox.mMinEdge[1] + speed[1] * time, staticBox.mMinEdge[1])
                    - relbox.mMinEdge[1] < 0)) 
					return COLLISION_AXIS_Z;
			}
		}
	}

	return COLLISION_AXIS_NONE;
}

// Helper function:
// Checks if moving the movingBox up by the given distance would hit a ceiling.
bool WouldCollideWithCeiling(const std::vector<NearbyCollisionInfo>& cInfo,
		const BoundingBox<float>& movingBox, float yIncrease, float d)
{
	//TimeTaker tt("WouldCollideWithCeiling");

	LogAssert(yIncrease >= 0, "invalid");	// pre-condition

	for (const auto &it : cInfo) 
    {
		const BoundingBox<float> &staticBox = it.box;
		if ((movingBox.mMaxEdge[1] - d <= staticBox.mMinEdge[1]) &&
            (movingBox.mMaxEdge[1] + yIncrease > staticBox.mMinEdge[1]) &&
            (movingBox.mMinEdge[0] < staticBox.mMaxEdge[0]) &&
            (movingBox.mMaxEdge[0] > staticBox.mMinEdge[0]) &&
            (movingBox.mMinEdge[2] < staticBox.mMaxEdge[2]) &&
            (movingBox.mMaxEdge[2] > staticBox.mMinEdge[2]))
			return true;
	}

	return false;
}

static inline void GetNeighborConnectingFace(const Vector3<short>& p,
	const NodeManager* nodeMgr, std::shared_ptr<Map> map, MapNode n, int v, int* neighbors)
{
	MapNode n2 = map->GetNode(p);
	if (nodeMgr->NodeboxConnects(n, n2, v))
		*neighbors |= v;
}

CollisionMoveResult CollisionMoveSimple(Environment* env,
    float posMaxDist, const BoundingBox<float>& box, float stepHeight, float dTime,
    Vector3<float>* pos, Vector3<float>* speed, Vector3<float> accel, 
    ActiveObject* self, bool collideWithObjects)
{
	static bool timeNotificationDone = false;
	std::shared_ptr<Map> map = env->GetMap();

	ScopeProfiler sp(Profiling, "CollisionMoveSimple()", SPT_AVG);

	CollisionMoveResult result;

	/*
		Calculate new velocity
	*/
	if (dTime > 0.5f)
    {
		if (!timeNotificationDone) 
        {
			timeNotificationDone = true;
			LogInformation("CollisionMoveSimple: maximum step interval exceeded, lost movement details!");
		}
        dTime = 0.5f;
	} 
    else 
    {
		timeNotificationDone = false;
	}
	*speed += accel * dTime;

	// If there is no speed, there are no collisions
	if (Length(*speed) == 0)
		return result;

	// Limit speed for avoiding hangs
	(*speed)[1] = std::clamp((*speed)[1], -5000.f, 5000.f);
	(*speed)[0] = std::clamp((*speed)[0], -5000.f, 5000.f);
	(*speed)[2] = std::clamp((*speed)[2], -5000.f, 5000.f);

	*speed = Truncate(*speed, 10000.0f);

	/*
		Collect node boxes in movement range
	*/
	std::vector<NearbyCollisionInfo> cInfo;
	{
	    //TimeTaker tt2("collisionMoveSimple collect boxes");
	    ScopeProfiler sp2(Profiling, "CollisionMoveSimple(): collect boxes", SPT_AVG);

	    Vector3<float> newpos = *pos + *speed * dTime;
        Vector3<float> minpos{
            std::min((*pos)[0], newpos[0]),
            std::min((*pos)[1], newpos[1]) + 0.01f * BS, // bias rounding, player often at +/-n.5
            std::min((*pos)[2], newpos[2]) };
        Vector3<float> maxpos{
            std::max((*pos)[0], newpos[0]),
            std::max((*pos)[1], newpos[1]),
            std::max((*pos)[2], newpos[2]) };

        Vector3<float> pos = minpos + box.mMinEdge;
        Vector3<short> p = Vector3<short>{(short)pos[0], (short)pos[1], (short)pos[2]};
        Vector3<short> min;
        min[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        min[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        min[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        min -= Vector3<short>{1, 1, 1};
        
        pos = maxpos + box.mMaxEdge;
        p = Vector3<short>{ (short)pos[0], (short)pos[1], (short)pos[2] };
        Vector3<short> max;
        max[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        max[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        max[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
        max += Vector3<short>{1, 1, 1};

	    bool anyPositionValid = false;
        for (p[0] = min[0]; p[0] <= max[0]; p[0]++)
        {
            for (p[1] = min[1]; p[1] <= max[1]; p[1]++)
            {
                for (p[2] = min[2]; p[2] <= max[2]; p[2]++)
                {
                    bool isPositionValid;
                    MapNode node = map->GetNode(p, &isPositionValid);
                    if (isPositionValid && node.GetContent() != CONTENT_IGNORE)
                    {
                        // Object collides into walkable nodes
                        anyPositionValid = true;
                        const NodeManager* nodeMgr = env->GetNodeManager();
                        const ContentFeatures& cFeatures = nodeMgr->Get(node);

                        if (!cFeatures.walkable)
                            continue;

                        int nBouncyValue = ItemGroupGet(cFeatures.groups, "Bouncy");

                        int neighbors = 0;
                        if (cFeatures.drawType == NDT_NODEBOX && 
                            cFeatures.nodeBox.type == NODEBOX_CONNECTED)
                        {
                            Vector3<short> p2 = p;

                            p2[1]++;
                            GetNeighborConnectingFace(p2, nodeMgr, map, node, 1, &neighbors);

                            p2 = p;
                            p2[1]--;
                            GetNeighborConnectingFace(p2, nodeMgr, map, node, 2, &neighbors);

                            p2 = p;
                            p2[2]--;
                            GetNeighborConnectingFace(p2, nodeMgr, map, node, 4, &neighbors);

                            p2 = p;
                            p2[0]--;
                            GetNeighborConnectingFace(p2, nodeMgr, map, node, 8, &neighbors);

                            p2 = p;
                            p2[2]++;
                            GetNeighborConnectingFace(p2, nodeMgr, map, node, 16, &neighbors);

                            p2 = p;
                            p2[0]++;
                            GetNeighborConnectingFace(p2, nodeMgr, map, node, 32, &neighbors);
                        }
                        std::vector<BoundingBox<float>> nodeboxes;
                        node.GetCollisionBoxes(env->GetNodeManager(), &nodeboxes, neighbors);

                        // Calculate float position only once
                        pos = Vector3<float>{ (float)p[0], (float)p[1], (float)p[2] } * BS;
                        for (auto box : nodeboxes) 
                        {
                            box.mMinEdge += pos;
                            box.mMaxEdge += pos;
                            cInfo.emplace_back(false, nBouncyValue, p, box);
                        }
                    }
                    else 
                    {
                        // Collide with unloaded nodes (position invalid) and loaded
                        // CONTENT_IGNORE nodes (position valid)
                        BoundingBox<float> box = GetNodeBox(p, BS);
                        cInfo.emplace_back(true, 0, p, box);
                    }
                }
            }
        }

	    // Do not move if world has not loaded yet, since custom node boxes
	    // are not available for collision detection.
	    // This also intentionally occurs in the case of the object being positioned
	    // solely on loaded CONTENT_IGNORE nodes, no matter where they come from.
	    if (!anyPositionValid) 
        {
		    *speed = Vector3<float>::Zero();
		    return result;
	    }

	} // tt2

	if(collideWithObjects)
	{
		/* add object boxes to cinfo */
		std::vector<ActiveObject*> objects;
		VisualEnvironment* visualEnv = dynamic_cast<VisualEnvironment*>(env);
		if (visualEnv != 0)
        {
			// Calculate distance by speed, add own extent and 1.5m of tolerance
			float distance = Length(*speed) * dTime + Length(box.GetExtent()) + 1.5f * BS;
			std::vector<DistanceSortedActiveObject> visualObjects;
            visualEnv->GetActiveObjects(*pos, distance, visualObjects);

			for (auto& visualObject : visualObjects)
            {
				// Do collide with everything but itself and the parent VAO
				if (!self || (self != visualObject.mObj &&
                    self != visualObject.mObj->GetParent()))
                {
					objects.push_back((ActiveObject*)visualObject.mObj);
				}
			}
		}
		else
		{
			LogicEnvironment* logicEnv = dynamic_cast<LogicEnvironment*>(env);
			if (logicEnv != NULL)
            {
				// Calculate distance by speed, add own extent and 1.5m of tolerance
				float distance = Length(*speed) * dTime + Length(box.GetExtent()) + 1.5f * BS;

				// search for objects which are not us, or we are not its parent
				// we directly use the callback to populate the result to prevent
				// a useless result loop here
				auto includeObjCb = [self, &objects] (LogicActiveObject *obj) 
                {
					if (!obj->IsGone() && (!self || (self != obj && self != obj->GetParent()))) 
						objects.push_back((ActiveObject *)obj);

					return false;
				};

				std::vector<LogicActiveObject*> sObjects;
                logicEnv->GetObjectsInsideRadius(sObjects, *pos, distance, includeObjCb);
			}
		}

		for (std::vector<ActiveObject*>::const_iterator iter = objects.begin(); iter != objects.end(); ++iter) 
        {
			ActiveObject* object = *iter;

			if (object && object->CollideWithObjects()) 
            {
				BoundingBox<float> objectCollisionBox;
				if (object->GetCollisionBox(&objectCollisionBox))
					cInfo.emplace_back(object, 0, objectCollisionBox);
			}
		}
		if (self && visualEnv)
        {
			VisualPlayer* visualPlayer = visualEnv->GetPlayer();
			if (visualPlayer->GetParent() == nullptr) 
            {
				BoundingBox<float> visualPlayerCollisionBox = visualPlayer->GetCollisionBox();
				Vector3<float> visualPlayerPos = visualPlayer->GetPosition();
				visualPlayerCollisionBox.mMinEdge += visualPlayerPos;
				visualPlayerCollisionBox.mMaxEdge += visualPlayerPos;
				ActiveObject *obj = (ActiveObject*) visualPlayer->GetVAO();
				cInfo.emplace_back(obj, 0, visualPlayerCollisionBox);
			}
		}
	} //tt3

	/*
		Collision detection
	*/

	float d = 0.0f;
	int loopCount = 0;
	while(dTime > BS * 1e-10f)
    {
		// Avoid infinite loop
		loopCount++;
		if (loopCount >= 100) 
        {
			LogWarning("collisionMoveSimple: Loop count exceeded, aborting to avoid infiniite loop");
			break;
		}

		BoundingBox<float> movingBox = box;
		movingBox.mMinEdge += *pos;
		movingBox.mMaxEdge += *pos;

		CollisionAxis nearestCollided = COLLISION_AXIS_NONE;
		float nearestDTime = dTime;
		int nearestBoxIndex = -1;

		/*
			Go through every nodebox, find nearest collision
		*/
		for (unsigned int boxIndex = 0; boxIndex < cInfo.size(); boxIndex++) 
        {
			const NearbyCollisionInfo& boxInfo = cInfo[boxIndex];
			// Ignore if already stepped up this nodebox.
			if (boxInfo.isStepUp)
				continue;

			// Find nearest collision of the two boxes (raytracing-like)
			float dtimeTmp = nearestDTime;
			CollisionAxis collided = AxisAlignedCollision(boxInfo.box, movingBox, *speed, &dtimeTmp);

			if (collided == -1 || dtimeTmp >= nearestDTime)
				continue;

			nearestDTime = dtimeTmp;
			nearestCollided = collided;
			nearestBoxIndex = boxIndex;
		}

		if (nearestCollided == COLLISION_AXIS_NONE) 
        {
			// No collision with any collision box.
			*pos += Truncate(*speed * dTime, 100.0f);
            dTime = 0;  // Set to 0 to avoid "infinite" loop due to small FP numbers
		} 
        else 
        {
			// Otherwise, a collision occurred.
			NearbyCollisionInfo& nearestInfo = cInfo[nearestBoxIndex];
			const BoundingBox<float>& cBox = nearestInfo.box;

			//movingBox except moved to the horizontal position it would be after step up
			BoundingBox<float> stepBox = movingBox;
			stepBox.mMinEdge[0] += (*speed)[0] * dTime;
			stepBox.mMinEdge[2] += (*speed)[2] * dTime;
			stepBox.mMaxEdge[0] += (*speed)[0] * dTime;
			stepBox.mMaxEdge[2] += (*speed)[2] * dTime;

			// Check for stairs.
			bool stepUp = (nearestCollided != COLLISION_AXIS_Y) && // must not be Y direction
                (movingBox.mMinEdge[1] < cBox.mMaxEdge[1]) &&
                (movingBox.mMinEdge[1] + stepHeight > cBox.mMaxEdge[1]) &&
                (!WouldCollideWithCeiling(cInfo, stepBox, cBox.mMaxEdge[1] - movingBox.mMinEdge[1], d));

			// Get bounce multiplier
			float bounce = -(float)nearestInfo.bouncy / 100.0f;

			// Move to the point of collision and reduce dtime by nearestDTime
			if (nearestDTime < 0) 
            {
				// Handle negative nearestDTime
				if (!stepUp) 
                {
					if (nearestCollided == COLLISION_AXIS_X)
						(*pos)[0] += (*speed)[0] * nearestDTime;
					if (nearestCollided == COLLISION_AXIS_Y)
						(*pos)[1] += (*speed)[1] * nearestDTime;
					if (nearestCollided == COLLISION_AXIS_Z)
						(*pos)[2] += (*speed)[2] * nearestDTime;
				}
			} 
            else 
            {
				*pos += Truncate(*speed * nearestDTime, 100.0f);
                dTime -= nearestDTime;
			}

			bool isCollision = true;
			if (nearestInfo.isUnloaded)
				isCollision = false;

			CollisionInfo info;
			if (nearestInfo.IsObject())
				info.type = COLLISION_OBJECT;
			else
				info.type = COLLISION_NODE;

			info.node = nearestInfo.position;
			info.object = nearestInfo.obj;
			info.oldSpeed = *speed;
			info.plane = nearestCollided;

			// Set the speed component that caused the collision to zero
			if (stepUp) 
            {
				// Special case: Handle stairs
				nearestInfo.isStepUp = true;
				isCollision = false;
			} 
            else if (nearestCollided == COLLISION_AXIS_X) 
            {
				if (fabs((*speed)[0]) > BS * 3)
					(*speed)[0] *= bounce;
				else
					(*speed)[0] = 0;
				result.collides = true;
			}
            else if (nearestCollided == COLLISION_AXIS_Y) 
            {
				if(fabs((*speed)[1]) > BS * 3)
					(*speed)[1] *= bounce;
				else
                    (*speed)[1] = 0;
				result.collides = true;
			} 
            else if (nearestCollided == COLLISION_AXIS_Z) 
            {
				if (fabs((*speed)[2]) > BS * 3)
                    (*speed)[2] *= bounce;
				else
                    (*speed)[2] = 0;
				result.collides = true;
			}

			info.newSpeed = *speed;
			if (Length(info.newSpeed - info.oldSpeed) < 0.1f * BS)
				isCollision = false;

			if (isCollision) 
            {
				info.axis = nearestCollided;
				result.collisions.push_back(info);
			}
		}
	}

	/*
		Final touches: Check if standing on ground, step up stairs.
	*/
	BoundingBox<float> standingBox = box;
    standingBox.mMinEdge += *pos;
    standingBox.mMaxEdge += *pos;
	for (const auto &boxInfo : cInfo) 
    {
		const BoundingBox<float> &cBox = boxInfo.box;

		/*
			See if the object is touching ground.

			Object touches ground if object's minimum Y is near node's
			maximum Y and object's X-Z-area overlaps with the node's
			X-Z-area.
		*/

		if (cBox.mMaxEdge[0] - d > standingBox.mMinEdge[0] &&
            cBox.mMinEdge[0] + d < standingBox.mMaxEdge[0] &&
            cBox.mMaxEdge[2] - d > standingBox.mMinEdge[2] &&
            cBox.mMinEdge[2] + d < standingBox.mMaxEdge[2])
        {
			if (boxInfo.isStepUp) 
            {
				(*pos)[1] += cBox.mMaxEdge[1] - standingBox.mMinEdge[1];
                standingBox = box;
                standingBox.mMinEdge += *pos;
                standingBox.mMaxEdge += *pos;
			}
			if (std::fabs(cBox.mMaxEdge[1] - standingBox.mMinEdge[1]) < 0.05f) 
            {
				result.touchingGround = true;

				if (boxInfo.IsObject())
					result.standingOnObject = true;
			}
		}
	}

	return result;
}
