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


#ifndef COLLISION_H
#define COLLISION_H

#include "GameEngineStd.h"

#include "Graphic/Scene/Hierarchy/BoundingBox.h"

#include "Mathematic/Algebra/Vector3.h"

class Map;
class Environment;
class ActiveObject;

enum CollisionType
{
	COLLISION_NODE,
	COLLISION_OBJECT,
};

enum CollisionAxis
{
	COLLISION_AXIS_NONE = -1,
	COLLISION_AXIS_X,
	COLLISION_AXIS_Y,
	COLLISION_AXIS_Z,
};

struct CollisionInfo
{
	CollisionInfo() = default;

	CollisionType type = COLLISION_NODE;
	CollisionAxis axis = COLLISION_AXIS_NONE;
    Vector3<short> node = Vector3<short>{ -32768,-32768,-32768 }; // COLLISION_NODE
	ActiveObject* object = nullptr; // COLLISION_OBJECT
	Vector3<float> oldSpeed;
	Vector3<float> newSpeed;
	int plane = -1;
};

struct CollisionMoveResult
{
	CollisionMoveResult() = default;

	bool touchingGround = false;
	bool collides = false;
	bool standingOnObject = false;
	std::vector<CollisionInfo> collisions;
};

// Moves using a single iteration; speed should not exceed posMaxDist/dtime
CollisionMoveResult CollisionMoveSimple(
	Environment* env, float posMaxDist, const BoundingBox<float>& box, float stepHeight, 
	float dTime, Vector3<float>* pos, Vector3<float>* speed, Vector3<float> accel, 
    ActiveObject* self=NULL, bool collideWithObjects=true);

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// dtime receives time until first collision, invalid if -1 is returned
CollisionAxis AxisAlignedCollision(const BoundingBox<float>& staticBox, 
    const BoundingBox<float>& movingBox, const Vector3<float>& speed, float* dTime);

// Helper function:
// Checks if moving the movingBox up by the given distance would hit a ceiling.
bool WouldCollideWithCeiling(
    const std::vector<BoundingBox<float>>& staticBoxes,
    const BoundingBox<float>& movingBox, float yIncrease, float d);

#endif