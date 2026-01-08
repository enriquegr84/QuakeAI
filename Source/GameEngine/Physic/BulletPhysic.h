//========================================================================
// BulletPhysic.h : Implements the BulletPhysic interface
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

#ifndef BULLETPHYSIC_H
#define BULLETPHYSIC_H

#include "GameEngineStd.h"

#include "Physic/Physic.h"
#include "Physic/PhysicEventListener.h"

#include "Importer/Bsp/BspLoader.h"
#include "Importer/Bsp/BspConverter.h"

#include "BulletDebugDrawer.h"

#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "BulletCollision/Gimpact/btBoxCollision.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

// forward declaration
class BspToBulletConverter;

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics								- Chapter 17, page 590
//
//   The implementation of BaseGamePhysic interface using the Bullet SDK.
//
/////////////////////////////////////////////////////////////////////////////

class BulletPhysics : public BaseGamePhysic
{
	friend class BspToBulletConverter;

public:

	struct AllHitsConvexResultCallback : public btCollisionWorld::ConvexResultCallback
	{
		AllHitsConvexResultCallback(const btVector3& convexFromWorld, const btVector3& convexToWorld)
			: m_convexFromWorld(convexFromWorld), m_convexToWorld(convexToWorld)
		{
		}

		btAlignedObjectArray<const btCollisionObject*> m_collisionObjects;

		btVector3 m_convexFromWorld;  //used to calculate hitPointWorld from hitFraction
		btVector3 m_convexToWorld;

		btAlignedObjectArray<btVector3> m_hitNormalWorld;
		btAlignedObjectArray<btVector3> m_hitPointWorld;
		btAlignedObjectArray<btScalar> m_hitFractions;

		virtual btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace)
		{
			m_collisionObjects.push_back(convexResult.m_hitCollisionObject);
			btVector3 hitNormalWorld;
			if (normalInWorldSpace)
			{
				hitNormalWorld = convexResult.m_hitNormalLocal;
			}
			else
			{
				///need to transform normal into worldspace
				hitNormalWorld = convexResult.m_hitCollisionObject->getWorldTransform().getBasis() * convexResult.m_hitNormalLocal;
			}
			m_hitNormalWorld.push_back(hitNormalWorld);
			btVector3 hitPointWorld;
			hitPointWorld.setInterpolate3(m_convexFromWorld, m_convexToWorld, convexResult.m_hitFraction);
			m_hitPointWorld.push_back(hitPointWorld);
			m_hitFractions.push_back(convexResult.m_hitFraction);
			m_closestHitFraction = convexResult.m_hitFraction;
			return m_closestHitFraction;
		}
	};

	BulletPhysics();				// [mrmike] This was changed post-press to add event registration!
	virtual ~BulletPhysics();

	// Initialiazation and Maintenance of the Physics World
	virtual bool Initialize() override;
	virtual void SyncVisibleScene() override;
	virtual void OnUpdate(float deltaSeconds) override;

	// Initialization of Physics Objects
	virtual void AddTrigger(const Vector3<float>& dimension,
		std::weak_ptr<Actor> pGameActor, const std::string& physicMaterial) override;
	virtual void AddBSP(BspLoader& bspLoader, const std::unordered_set<int>& convexSurfaces,
		const std::unordered_set<int>& ignoreBSPSurfaces, const std::unordered_set<int>& ignorePhysSurfaces,
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
	//   during ~BulletPhysics

	// these are all of the objects that Bullet uses to do its work.
	//   see BulletPhysics::Initialize() for some more info.
	btDiscreteDynamicsWorld* mDynamicsWorld;
	btBroadphaseInterface* mBroadphase;
	btCollisionDispatcher* mDispatcher;
	btConstraintSolver* mSolver;
	btDefaultCollisionConfiguration* mCollisionConfiguration;
	BulletDebugDrawer* mDebugDrawer;

	// tables read from the XML
	typedef std::map<std::string, float> DensityTable;
	typedef std::map<std::string, MaterialData> MaterialTable;
	DensityTable mDensityTable;
	MaterialTable mMaterialTable;

	void LoadXml();
	float LookupSpecificGravity(const std::string& densityStr);
	MaterialData LookupMaterialData(const std::string& materialStr);

	// keep track of the existing actions:  To check them for updates
	//   to the actors' positions, and to remove them when their lives are over.
	typedef std::map<ActorId, btActionInterface*> ActorIDToBulletActionMap;
	ActorIDToBulletActionMap mActorIdToAction;
	btActionInterface* FindBulletAction(ActorId id) const;

	// keep track of the existing collision objects:  To check them for updates
	//   to the actors' positions, and to remove them when their lives are over.
	typedef std::map<ActorId, btCollisionObject*> ActorIDToBulletCollisionObjectMap;
	ActorIDToBulletCollisionObjectMap mActorIdToCollisionObject;
	btCollisionObject* FindBulletCollisionObject(ActorId id) const;

	// also keep a map to get the actor id from the btCollisionObject*
	typedef std::map<btCollisionObject const*, ActorId> BulletCollisionObjectToActorIDMap;
	BulletCollisionObjectToActorIDMap mCollisionObjectToActorId;
	ActorId FindActorID(btCollisionObject const*) const;

	// data used to store which collision pair (bodies that are touching) need
	//   Collision events sent.  When a new pair of touching bodies are detected,
	//   they are added to m_previousTickCollisionPairs and an event is sent.
	//   When the pair is no longer detected, they are removed and another event
	//   is sent.
	typedef std::pair<btRigidBody const*, btRigidBody const*> CollisionPair;
	typedef std::set<CollisionPair> CollisionPairs;
	CollisionPairs mPreviousTickCollisionPairs;

	// helpers for sending events relating to collision pairs
	void SendCollisionPairAddEvent(btPersistentManifold const* manifold,
		btRigidBody const* body0, btRigidBody const* body1);
	void SendCollisionPairRemoveEvent(btRigidBody const* body0, btRigidBody const* body1);

	// common functionality used by AddSphere, AddBox, etc
	void AddShape(std::shared_ptr<Actor> pGameActor, btCollisionShape* shape,
		btScalar mass, const std::string& physicMaterial);

	// helper for cleaning up objects
	void RemoveCollisionObject(btCollisionObject* removeMe);

	// callback from bullet for each physics time step. set in Initialize
	static void BulletInternalTickCallback(btDynamicsWorld* const world, btScalar const timeStep);

};

#endif