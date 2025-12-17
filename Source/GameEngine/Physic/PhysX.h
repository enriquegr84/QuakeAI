//========================================================================
// PhysX.h : Implements the BulletPhysic interface
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

#ifndef PHYSX_H
#define PHYSX_H

#include "GameEngineStd.h"

#include "Physic/Physic.h"
#include "Physic/PhysicEventListener.h"

#if defined(PHYSX) && defined(_WIN64)

#include "PxPhysicsAPI.h"

#include "PhysXDebugDrawer.h"

using namespace physx;

// forward declaration
class PhysX;
class BspToPhysXConverter;

class ContactReportCallback : public PxSimulationEventCallback
{
	friend class PhysX;

public:

	ContactReportCallback(PhysX* physX) : mPhysX(physX) { }

	void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) { PX_UNUSED(constraints); PX_UNUSED(count); }
	void onWake(PxActor** actors, PxU32 count) { PX_UNUSED(actors); PX_UNUSED(count); }
	void onSleep(PxActor** actors, PxU32 count) { PX_UNUSED(actors); PX_UNUSED(count); }
	void onTrigger(PxTriggerPair* pairs, PxU32 nPairs);
	void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) {}
	void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nPairs);


private:

	PhysX* mPhysX;
};

/////////////////////////////////////////////////////////////////////////////
// PhysX
//
//   The implementation of BaseGamePhysic interface using the PhysX SDK.
//
/////////////////////////////////////////////////////////////////////////////

class PhysX : public BaseGamePhysic
{
	friend class BspToPhysXConverter;
	friend class ContactReportCallback;

public:

	PhysX();				// [mrmike] This was changed post-press to add event registration!
	virtual ~PhysX();

	// Initialiazation and Maintenance of the Physics World
	virtual bool Initialize() override;
	virtual void SyncVisibleScene() override;
	virtual void OnUpdate(float deltaSeconds) override;

	// Initialization of Physics Objects
	virtual void AddTrigger(const Vector3<float>& dimension,
		std::weak_ptr<Actor> pGameActor, const std::string& physicMaterial) override;
	virtual void AddBSP(BspLoader& bspLoader, const std::unordered_set<int>& convexSurfaces,
		const std::unordered_set<int>& ignoreSurfaces, const std::unordered_set<int>& ignoreConvexSurfaces,
		std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void AddCharacterController(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void AddSphere(float radius, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void AddBox(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void AddConvexVertices(Plane3<float>* planes, int numPlanes, const Vector3<float>& scale,
		std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void AddPointCloud(Vector3<float>* verts, int numPoints, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void AddPointCloud(Plane3<float>* planes, int numPlanes, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;
	virtual void RemoveActor(ActorId id) override;

	// Debugging
	virtual void RenderDiagnostics() override;

	// Physics world modifiers
	virtual void ApplyForce(ActorId aid, const Vector3<float>& velocity) override;
	virtual void ApplyTorque(ActorId aid, const Vector3<float>& velocity) override;

	virtual bool OnGround(ActorId actorId);
	virtual bool CheckPenetration(ActorId actorId);
	virtual void Jump(ActorId actorId, const Vector3<float>& dir);
	virtual void Move(ActorId actorId, const Vector3<float>& dir);
	virtual void Fall(ActorId actorId, const Vector3<float>& dir);

	// Collisions
	virtual bool FindIntersection(ActorId actorId, const Vector3<float>& point);
	virtual ActorId ConvexSweep(ActorId aId,
		const Transform& origin, const Transform& end,
		std::optional<Vector3<float>>& collisionPoint,
		std::optional<Vector3<float>>& collisionNormal);
	virtual void ConvexSweep(ActorId aId,
		const Transform& origin, const Transform& end,
		std::vector<ActorId>& collisionActors,
		std::vector<Vector3<float>>& collisionPoints,
		std::vector<Vector3<float>>& collisionNormals);

	virtual ActorId CastRay(
		const Vector3<float>& origin, const Vector3<float>& end,
		Vector3<float>& collisionPoint, Vector3<float>& collisionNormal);
	virtual void CastRay(
		const Vector3<float>& origin, const Vector3<float>& end,
		std::vector<ActorId>& collisionActors,
		std::vector<Vector3<float>>& collisionPoints,
		std::vector<Vector3<float>>& collisionNormals);

	virtual void SetCollisionFlags(ActorId actorId, int collisionFlags);
	virtual void SetIgnoreCollision(ActorId actorId, ActorId ignoreActorId, bool ignoreCollision);

	virtual void StopActor(ActorId actorId);
	virtual Vector3<float> GetCenter(ActorId actorId);
	virtual Vector3<float> GetScale(ActorId actorId);
	virtual Vector3<float> GetVelocity(ActorId actorId);
	virtual float GetJumpSpeed(ActorId actorId);
	virtual void SetGravity(ActorId actorId, const Vector3<float>& g);
	virtual void SetVelocity(ActorId actorId, const Vector3<float>& vel);
	virtual void SetPosition(ActorId actorId, const Vector3<float>& pos);
	virtual void SetRotation(ActorId aid, const Transform& trans);
	virtual Vector3<float> GetAngularVelocity(ActorId actorId);
	virtual void SetAngularVelocity(ActorId actorId, const Vector3<float>& vel);
	virtual void Translate(ActorId actorId, const Vector3<float>& vec);

	virtual void SetTransform(const ActorId id, const Transform& trans);
	virtual Transform GetTransform(const ActorId id);
	virtual void GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations);

protected:

	// use auto pointers to automatically call delete on these objects
	// during ~PhysX

	// these are all of the objects that PhysX uses to do its work.
	// see PhysX::Initialize() for some more info.
	PxDefaultAllocator mAllocator;
	PxDefaultErrorCallback mErrorCallback;
	PxFoundation* mFoundation = NULL;
	PxPhysics* mPhysicsSystem = NULL;
	PxDefaultCpuDispatcher* mDispatcher = NULL;
	PxScene* mScene = NULL;
	PxControllerManager* mControllerManager = NULL;
	PxMaterial* mMaterial = NULL;
	PxPvd* mPvd = NULL;
	PhysXDebugDrawer* mDebugDrawer = NULL;

	// tables read from the XML
	typedef std::map<std::string, float> DensityTable;
	typedef std::map<std::string, MaterialData> MaterialTable;
	DensityTable mDensityTable;
	MaterialTable mMaterialTable;

	void LoadXml();
	float LookupSpecificGravity(const std::string& densityStr);
	MaterialData LookupMaterialData(const std::string& materialStr);

	// keep track of the existing controllers: To check them for updates
	// to the actors' positions, and to remove them when their lives are over.
	std::map<PxController*, bool> mCCTGround;
	std::map<PxController*, PxVec3> mCCTMove, mCCTJump, mCCTJumpAccel, mCCTFall, mCCTFallAccel;
	typedef std::map<ActorId, PxController*> ActorIDToPhysXControllerMap;
	ActorIDToPhysXControllerMap mActorIdToController;
	PxController* FindPhysXController(ActorId id) const;

	// keep track of the existing collision objects:  To check them for updates
	//   to the actors' positions, and to remove them when their lives are over.
	typedef std::map<ActorId, PxRigidActor*> ActorIDToPhysXCollisionObjectMap;
	ActorIDToPhysXCollisionObjectMap mActorIdToCollisionObject;
	PxRigidActor* FindPhysXCollisionObject(ActorId id) const;

	// also keep a map to get the actor id from the PxRigidActor*
	typedef std::map<PxRigidActor const*, ActorId> PhysXCollisionObjectToActorIDMap;
	PhysXCollisionObjectToActorIDMap mCollisionObjectToActorId;
	ActorId FindActorID(PxRigidActor const*) const;

	// helpers for sending events relating to trigger pairs
	void SendTriggerPairAddEvent(const PxTriggerPair& pair);
	void SendTriggerPairRemoveEvent(PxRigidActor const* body0, PxRigidActor const* body1);
	// helpers for sending events relating to collision pairs
	void SendCollisionPairAddEvent(const PxContactPair& pair);
	void SendCollisionPairRemoveEvent(PxRigidActor const* body0, PxRigidActor const* body1);

	// common functionality used by AddSphere, AddBox, etc
	void AddShape(std::shared_ptr<Actor> pGameActor, PxShape* shape,
		float mass, const std::string& physicMaterial);

	// helper for cleaning up objects
	void RemoveTriggerObject(PxRigidActor* removeMe);
	void RemoveCollisionObject(PxRigidActor* removeMe);
};

#endif

#endif