//========================================================================
// Physics.h : Implements the BaseGamePhysic interface
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

#ifndef QUAKEPHYSIC_H
#define QUAKEPHYSIC_H

#include "GameEngineStd.h"

#include "Application/Settings.h"

#if defined(PHYSX) && defined(_WIN64)

#include "Physic/PhysX.h"


/////////////////////////////////////////////////////////////////////////////
// QuakePhysX
//
//   The implementation of QuakePhysX interface using the PhysX SDK.
//
/////////////////////////////////////////////////////////////////////////////

class QuakePhysX : public PhysX
{

public:

	QuakePhysX(); // [mrmike] This was changed post-press to add event registration!
	virtual ~QuakePhysX();

	virtual void OnUpdate(float deltaSeconds) override;

	virtual void AddCharacterController(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;

	virtual void GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& transforms);
};

#else

#include "Physic/BulletPhysic.h"

/////////////////////////////////////////////////////////////////////////////
// QuakeBulletPhysics
//
//   The implementation of QuakeBulletPhysics interface using the Bullet SDK.
//
/////////////////////////////////////////////////////////////////////////////

class QuakeBulletPhysics : public BulletPhysics
{

public:

	QuakeBulletPhysics();				// [mrmike] This was changed post-press to add event registration!
	virtual ~QuakeBulletPhysics();

	virtual void OnUpdate(float deltaSeconds) override;

	virtual void AddCharacterController(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
		const std::string& densityStr, const std::string& physicMaterial) override;

	virtual void GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& transforms);
};

class BulletCharacterController : public btKinematicCharacterController
{
	friend class QuakeBulletPhysics;

public:
	BulletCharacterController(ActorId playerId, btPairCachingGhostObject* ghostObject,
		btConvexShape* convexShape, btScalar stepHeight, const btVector3& up = btVector3(0.0, 0.0, 1.0))
		: btKinematicCharacterController(ghostObject, convexShape, stepHeight, up), mPlayerId(playerId)
	{
		mGravity = Settings::Get()->GetVector3("default_gravity");

		mMaxPushSpeed = Vector3<float>{
			PUSHTRIGGER_JUMP_SPEED_XZ, PUSHTRIGGER_JUMP_SPEED_XZ, PUSHTRIGGER_JUMP_SPEED_Y };
		mMaxJumpSpeed = Vector3<float>{
			DEFAULT_JUMP_SPEED_XZ, DEFAULT_JUMP_SPEED_XZ, DEFAULT_JUMP_SPEED_Y };
		mMaxFallSpeed = Vector3<float>{
			DEFAULT_FALL_SPEED_XZ, DEFAULT_FALL_SPEED_XZ, DEFAULT_FALL_SPEED_Y };
		mMaxMoveSpeed = DEFAULT_MOVE_SPEED;

		mPushSpeed = mMaxPushSpeed;
		mJumpSpeed = mMaxJumpSpeed;
		mFallSpeed = mMaxFallSpeed;
		mMoveSpeed = mMaxMoveSpeed;
	}

	~BulletCharacterController() {};

	///BulletCharacterController
	virtual void updateAction(btCollisionWorld* collisionWorld, btScalar deltaTime);

	void getInterpolations(std::vector<std::pair<Transform, bool>>& interpolations);

protected:

	void updateState();

	void resetInterpolations() { mInterpolations.clear(); }

private:

	Vector3<float> mGravity;

	// Speed Controls
	Vector3<float> mMaxPushSpeed;
	Vector3<float> mMaxJumpSpeed;
	Vector3<float> mMaxFallSpeed;
	float mMaxMoveSpeed;

	Vector3<float> mPushSpeed;
	Vector3<float> mJumpSpeed;
	Vector3<float> mFallSpeed;
	float mMoveSpeed;

	std::vector<std::pair<Transform, bool>> mInterpolations;

	ActorId mPlayerId;
};

#endif

extern BaseGamePhysic* CreateQuakePhysics();

#endif