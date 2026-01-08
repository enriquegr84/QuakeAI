//========================================================================
// Physic.h : Implements the BaseGamePhysic interface
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/gamecode4/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#ifndef PHYSIC_H
#define PHYSIC_H

#include "GameEngineStd.h"

#include "Graphic/Scene/Hierarchy/PVWUpdater.h"

#include "Mathematic/Algebra/AxisAngle.h"
#include "Mathematic/Algebra/Transform.h"
#include "Mathematic/Algebra/Vector3.h"
#include "Mathematic/Geometric/Hyperplane.h"

// forward declaration
class BspLoader;

/////////////////////////////////////////////////////////////////////////////
//   Materials Description						- Chapter 17, page 579
//
//   Predefines some useful physics materials. Define new ones here, and 
//   have similar objects use it, so if you ever need to change it you'll 
//   only have to change it here.
//
/////////////////////////////////////////////////////////////////////////////
struct MaterialData
{
	float mRestitution;
	float mFriction;

	MaterialData(float restitution, float friction)
	{
		mRestitution = restitution;
		mFriction = friction;
	}

	MaterialData(const MaterialData& other)
	{
		mRestitution = other.mRestitution;
		mFriction = other.mFriction;
	}
};

/////////////////////////////////////////////////////////////////////////////
// class BaseGamePhysic							- Chapter 17, page 589
//
//   The interface defintion for a generic physics API.
/////////////////////////////////////////////////////////////////////////////
class BaseGamePhysic
{
public:

	// Initialiazation and Maintenance of the Physics World
	virtual bool Initialize() = 0;
	virtual void SyncVisibleScene() = 0;
	virtual void OnUpdate(float deltaSeconds) = 0;

	// Initialization of Physics Objects
	virtual void AddTrigger(const Vector3<float>& dimensions, 
		std::weak_ptr<Actor> pGameActor, const std::string& physicMaterial) = 0;
	virtual void AddBSP(BspLoader& bspLoader, const std::unordered_set<int>& convexSurfaces,
		const std::unordered_set<int>& ignoreBSPSurfaces, const std::unordered_set<int>& ignorePhysSurfaces,
		std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void AddCharacterController(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void AddSphere(float radius, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void AddBox(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void AddConvexVertices(Plane3<float>* planes, int numPlanes, const Vector3<float>& scale,
		std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void AddPointCloud(Vector3<float> *verts, int numPoints, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void AddPointCloud(Plane3<float> *planes, int numPlanes, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) = 0;
	virtual void RemoveActor(ActorId id) = 0;

	// Debugging
	virtual void RenderDiagnostics() = 0;

	// Physics world modifiers
	virtual void ApplyForce(ActorId aid, const Vector3<float>& velocity) = 0;
	virtual void ApplyTorque(ActorId aid, const Vector3<float>& velocity) = 0;

	// Physics actor states
	virtual bool OnGround(ActorId actorId) = 0;
	virtual bool CheckPenetration(ActorId actorId) = 0;
	virtual void Jump(ActorId actorId, const Vector3<float>& dir) = 0;
	virtual void Move(ActorId actorId, const Vector3<float>& dir) = 0;
	virtual void Fall(ActorId actorId, const Vector3<float>& dir) = 0;

	// Collisions
	virtual bool FindIntersection(ActorId actorId, const Vector3<float>& point) = 0;
	virtual ActorId ConvexSweep(ActorId aId,
		const Transform& origin, const Transform& end,
		std::optional<Vector3<float>>& collisionPoint,
		std::optional<Vector3<float>>& collisionNormal) = 0;
	virtual void ConvexSweep(ActorId aId,
		const Transform& origin, const Transform& end,
		std::vector<ActorId>& collisionActors,
		std::vector<Vector3<float>>& collisionPoints,
		std::vector<Vector3<float>>& collisionNormals) = 0;

	virtual ActorId CastRay(
		const Vector3<float>& origin, const Vector3<float>& end,
		Vector3<float>& collisionPoint, Vector3<float>& collision) = 0;
	virtual void CastRay(
		const Vector3<float>& origin, const Vector3<float>& end,
		std::vector<ActorId>& collisionActors,
		std::vector<Vector3<float>>& collisionPoints,
		std::vector<Vector3<float>>& collisionNormals) = 0;

	virtual void SetCollisionFlags(ActorId actorId, int collisionFlags) = 0;
	virtual void SetIgnoreCollision(ActorId actorId, ActorId ignoreActorId, bool ignoreCollision) = 0;

	virtual void StopActor(ActorId actorId) = 0;
	virtual Vector3<float> GetCenter(ActorId actorId) = 0;
	virtual Vector3<float> GetScale(ActorId actorId) = 0;
	virtual Vector3<float> GetVelocity(ActorId actorId) = 0;
	virtual float GetJumpSpeed(ActorId actorId) = 0;
	virtual void SetGravity(ActorId actorId, const Vector3<float>& g) = 0;
	virtual void SetVelocity(ActorId actorId, const Vector3<float>& vel) = 0;
	virtual void SetPosition(ActorId actorId, const Vector3<float>& pos) = 0;
	virtual void SetRotation(ActorId actorId, const Transform& mat) = 0;
	virtual Vector3<float> GetAngularVelocity(ActorId actorId) = 0;
	virtual void SetAngularVelocity(ActorId actorId, const Vector3<float>& vel) = 0;
	virtual void Translate(ActorId actorId, const Vector3<float>& vec) = 0;
	virtual void SetTransform(const ActorId id, const Transform& mat) = 0;
	virtual Transform GetTransform(const ActorId id) = 0;
	virtual void GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations) = 0;

	virtual ~BaseGamePhysic() { };
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// a physics implementation which does nothing.  used if physics is disabled.
//
class NullPhysics : public BaseGamePhysic
{
public:
	NullPhysics() { }
	virtual ~NullPhysics() { }

	// Initialization and Maintenance of the Physics World
	virtual bool Initialize() { return true; }
	virtual void SyncVisibleScene() { };
	virtual void OnUpdate(float) { }

	// Initialization of Physics Objects
	virtual void AddTrigger(const Vector3<float>& dimensions,
		std::weak_ptr<Actor> pGameActor, const std::string& physicMaterial) { }
	virtual void AddBSP(BspLoader& bspLoader, const std::unordered_set<int>& convexSurfaces,
		const std::unordered_set<int>& ignoreBSPSurfaces, const std::unordered_set<int>& ignorePhysSurfaces,
		std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void AddCharacterController(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void AddSphere(float radius, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void AddBox(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void AddConvexVertices(Plane3<float>* planes, int numPlanes, const Vector3<float>& scale,
		std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void AddPointCloud(Vector3<float>* verts, int numPoints, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void AddPointCloud(Plane3<float>* planes, int numPlanes, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) { }
	virtual void RemoveActor(ActorId id) { }

	// Debugging
	virtual void RenderDiagnostics() { }

	// Physics world modifiers
	virtual void ApplyForce(ActorId aid, const Vector3<float>& velocity) { }
	virtual void ApplyTorque(ActorId aid, const Vector3<float>& velocity) { }

	// Physics actor states
	virtual bool OnGround(ActorId actorId) { return false; }
	virtual bool CheckPenetration(ActorId actorId) { return false; }
	virtual void Jump(ActorId actorId, const Vector3<float>& dir) { }
	virtual void Move(ActorId actorId, const Vector3<float>& dir) { }
	virtual void Fall(ActorId actorId, const Vector3<float>& dir) { }

	// Collisions
	virtual bool FindIntersection(ActorId actorId, const Vector3<float>& point) { return false; }
	virtual ActorId ConvexSweep(ActorId aId,
		const Transform& origin, const Transform& end,
		std::optional<Vector3<float>>& collisionPoint,
		std::optional<Vector3<float>>& collisionNormal) {
		return INVALID_ACTOR_ID;
	}
	virtual void ConvexSweep(ActorId aId,
		const Transform& origin, const Transform& end,
		std::vector<ActorId>& collisionActors,
		std::vector<Vector3<float>>& collisionPoints,
		std::vector<Vector3<float>>& collisionNormals) { }

	virtual ActorId CastRay(
		const Vector3<float>& origin, const Vector3<float>& end,
		Vector3<float>& collisionPoint, Vector3<float>& collisionNormal) {
		return INVALID_ACTOR_ID;
	}
	virtual void CastRay(
		const Vector3<float>& origin, const Vector3<float>& end,
		std::vector<ActorId>& collisionActors,
		std::vector<Vector3<float>>& collisionPoints,
		std::vector<Vector3<float>>& collisionNormals) { }

	virtual void SetCollisionFlags(ActorId actorId, int collisionFlags) { }
	virtual void SetIgnoreCollision(ActorId actorId, ActorId ignoreActorId, bool ignoreCollision) { }

	virtual void StopActor(ActorId actorId) { }
	virtual Vector3<float> GetCenter(ActorId actorId) { return Vector3<float>::Zero(); }
	virtual Vector3<float> GetScale(ActorId actorId) { return Vector3<float>::Zero(); }
	virtual Vector3<float> GetVelocity(ActorId actorId) { return Vector3<float>::Zero(); }
	virtual float GetJumpSpeed(ActorId actorId) { return 0; }
	virtual void SetGravity(ActorId actorId, const Vector3<float>& g) { }
	virtual void SetVelocity(ActorId actorId, const Vector3<float>& vel) { }
	virtual void SetPosition(ActorId actorId, const Vector3<float>& pos) { }
	virtual void SetRotation(ActorId aid, const Transform& mat) { }
	virtual Vector3<float> GetAngularVelocity(ActorId actorId) { return Vector3<float>::Zero(); }
	virtual void SetAngularVelocity(ActorId actorId, const Vector3<float>& vel) { }
	virtual void Translate(ActorId actorId, const Vector3<float>& vec) { }
	virtual void SetTransform(const ActorId id, const Transform& mat) { }
	virtual Transform GetTransform(const ActorId id) { return Transform::Identity; }
	virtual void GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations) { }
};

extern BaseGamePhysic* CreateGamePhysics();
extern BaseGamePhysic* CreateNullPhysics();

#endif