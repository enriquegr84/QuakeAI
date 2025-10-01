//========================================================================
// QuakePhysic.cpp : Implements the BaseGamePhysic interface with Bullet
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

#include "QuakeStd.h"

#include "QuakePhysic.h"

#include "Quake.h"
#include "QuakeApp.h"
#include "QuakeAIView.h"
#include "QuakeAIManager.h"

#include "QuakeEvents.h"
#include "QuakeNetwork.h"
#include "QuakeLevelManager.h"
#include "QuakePlayerController.h"
#include "QuakeCameraController.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/TransformComponent.h"


/////////////////////////////////////////////////////////////////////////////
// helpers for conversion to and from Bullet's data types
static btVector3 Vector3TobtVector3(Vector3<float> const& vector3)
{
	return btVector3(vector3[0], vector3[1], vector3[2]);
}

static Vector3<float> btVector3ToVector3(btVector3 const& btvec)
{
	return Vector3<float>{ (float)btvec.x(), (float)btvec.y(), (float)btvec.z() };
}

static btTransform TransformTobtTransform(Transform const& transform)
{
	// convert from matrix4 (GameEngine) to btTransform (Bullet)
	btMatrix3x3 bulletRotation;
	btVector3 bulletPosition;

	// copy transform matrix
	Matrix4x4<float> transformMatrix = transform.GetRotation();
	for (int row = 0; row < 3; ++row)
		for (int column = 0; column < 3; ++column)
			bulletRotation[row][column] = transformMatrix(row, column);
	// note the reversed indexing (row/column vs. column/row)
	//  this is because matrix4s are row-major matrices and
	//  btMatrix3x3 are column-major.  This reversed indexing
	//  implicitly transposes (flips along the diagonal) 
	//  the matrix when it is copied.

// copy position
	Vector3<float> translation = transform.GetTranslation();
	for (int column = 0; column < 3; ++column)
		bulletPosition[column] = translation[column];

	return btTransform(bulletRotation, bulletPosition);
}

static Transform btTransformToTransform(btTransform const& trans)
{
	Transform returnTransform;

	// convert from btTransform (Bullet) to matrix4 (GameEngine)
	btMatrix3x3 const& bulletRotation = trans.getBasis();
	btVector3 const& bulletPosition = trans.getOrigin();

	// copy transform matrix
	Matrix4x4<float> transformMatrix = Matrix4x4<float>::Identity();
	for (int row = 0; row < 3; ++row)
		for (int column = 0; column < 3; ++column)
			transformMatrix(row, column) = (float)bulletRotation[row][column];
	// note the reversed indexing (row/column vs. column/row)
	//  this is because matrix4s are row-major matrices and
	//  btMatrix3x3 are column-major.  This reversed indexing
	//  implicitly transposes (flips along the diagonal) 
	//  the matrix when it is copied.

// copy position
	Vector3<float> translationVector;
	for (int column = 0; column < 3; ++column)
		translationVector[column] = (float)bulletPosition[column];

	returnTransform.SetRotation(transformMatrix);
	returnTransform.SetTranslation(translationVector);
	return returnTransform;
}

void BulletCharacterController::updateState()
{
	bool isGround = onGround();
	Transform transform = btTransformToTransform(m_ghostObject->getWorldTransform());
	mInterpolations.push_back({transform, isGround });

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	aiManager->SetPlayerGround(mPlayerId, isGround);
	if (!isGround)
		return;

	std::shared_ptr<QuakeAIView> aiView;
	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (std::shared_ptr<BaseGameView> gameView : gameViews)
		if (gameView->GetType() == GV_AI && gameView->GetActorId() == mPlayerId)
			aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);

	if (aiView && aiView->GetPathingGraph())
	{
		bool updatedActionPlan = aiView->UpdateActionPlan(false);

		Vector3<float> currentPosition = transform.GetTranslation();
		if (!aiView->UpdateActionPlan(currentPosition, 0.5f))
		{
			if (updatedActionPlan)
				aiManager->UpdatePlayerView(mPlayerId, aiView->GetActionPlayer(), false);

			return;
		}

		if (updatedActionPlan)
			aiManager->UpdatePlayerView(mPlayerId, aiView->GetActionPlayer(), false);
		else
			aiManager->UpdatePlayerView(mPlayerId, aiView->GetActionPlayer());

		Vector3<float> fall = Vector3<float>::Zero();
		Vector3<float> velocity = Vector3<float>::Zero();
		if (aiView->GetActionPlanArc())
			velocity = aiView->GetActionPlanArc()->GetNode()->GetPosition() - currentPosition;
		else
			velocity = aiView->GetActionPlanNode()->GetPosition() - currentPosition;

		Normalize(velocity);
		float yaw = atan2(velocity[1], velocity[0]) * (float)GE_C_RAD_TO_DEG;
		aiView->SetYaw(yaw, false);

		// Calculate the new rotation matrix from the camera
		// yaw and pitch (zrotate and xrotate).
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), yaw * (float)GE_C_DEG_TO_RAD));

		// This will give us the "look at" vector 
		// in world space - we'll use that to move.
		Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
		atWorld = rotation * atWorld;
#else
		atWorld = atWorld * rotation;
#endif

		if (aiView->GetActionPlanType() == AT_JUMP)
		{
			aiView->SetActionPlanType(AT_MOVE);

			Vector4<float> direction = atWorld;
			direction[AXIS_Y] = 0;
			Normalize(direction);

			velocity[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
			velocity[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
			velocity[AXIS_Y] = mJumpSpeed[AXIS_Y];

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataJumpActor>(mPlayerId, velocity, fall));
		}
		else if (aiView->GetActionPlanType() == AT_MOVE)
		{
			Vector4<float> direction = atWorld;
			direction[AXIS_Y] = 0;
			Normalize(direction);

			velocity = HProject(direction);
			velocity *= mMoveSpeed;

			fall = mGravity;

			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataMoveActor>(mPlayerId, velocity, fall));
		}
	}	
}

///BulletCharacterController
void BulletCharacterController::updateAction(btCollisionWorld* collisionWorld, btScalar deltaTime)
{
	preStep(collisionWorld);
	playerStep(collisionWorld, deltaTime / 4.f);
	updateState();

	preStep(collisionWorld);
	playerStep(collisionWorld, deltaTime / 4.f);
	updateState();

	preStep(collisionWorld);
	playerStep(collisionWorld, deltaTime / 4.f);
	updateState();

	preStep(collisionWorld);
	playerStep(collisionWorld, deltaTime / 4.f);
	updateState();
}

void BulletCharacterController::getInterpolations(std::vector<std::pair<Transform, bool>>& interpolations)
{
	for (auto const& interpolation : mInterpolations)
		interpolations.push_back(interpolation);
}


QuakePhysics::QuakePhysics() : BulletPhysics()
{

}


/////////////////////////////////////////////////////////////////////////////
// QuakePhysics::~QuakePhysics
//
QuakePhysics::~QuakePhysics()
{

}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::OnUpdate
//
void QuakePhysics::OnUpdate(float const deltaSeconds)
{
	ActorIDToBulletActionMap::const_iterator itAction = mActorIdToAction.begin();
	for (;itAction != mActorIdToAction.end(); itAction++)
	{
		if (BulletCharacterController* const controller =
			dynamic_cast<BulletCharacterController*>(itAction->second))
		{
			controller->resetInterpolations();
		}
	}

	// Bullet uses an internal fixed timestep (default 1/60th of a second)
	// Bullet will run the simulation in increments of the fixed timestep 
	// until "deltaSeconds" amount of time has passed (maximum of 10 steps).
	mDynamicsWorld->stepSimulation(deltaSeconds, 10);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddCharacterController
//
void QuakePhysics::AddCharacterController(
	const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	// create the collision body, which specifies the shape of the object
	float radius = std::max(dimensions[0], dimensions[1]) / 2.f;
	float height = dimensions[2] > 2 * radius ? dimensions[2] - 2 * radius : 0;
	btConvexShape* collisionShape = new btCapsuleShapeZ(radius, height);

	// calculate absolute mass from specificGravity
	float specificGravity = LookupSpecificGravity(densityStr);
	float const volume = dimensions[0] * dimensions[1] * dimensions[2];
	btScalar const mass = volume * specificGravity;

	ActorId actorId = pStrongActor->GetId();
	LogAssert(mActorIdToCollisionObject.find(actorId) == mActorIdToCollisionObject.end(),
		"Actor with more than one physics body?");

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));

	// localInertia defines how the object's mass is distributed
	btVector3 localInertia(0.f, 0.f, 0.f);
	if (mass > 0.f)
		collisionShape->calculateLocalInertia(mass, localInertia);

	Transform transform;
	std::shared_ptr<TransformComponent> pTransformComponent =
		pStrongActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
	LogAssert(pTransformComponent, "no transform");
	if (pTransformComponent)
	{
		transform = pTransformComponent->GetTransform();
	}
	else
	{
		// Physics can't work on an actor that doesn't have a TransformComponent!
		return;
	}

	btPairCachingGhostObject* ghostObject = new btPairCachingGhostObject();
	ghostObject->setWorldTransform(TransformTobtTransform(transform));
	mBroadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
	ghostObject->setCollisionShape(collisionShape);
	ghostObject->setCollisionFlags(
		btCollisionObject::CF_KINEMATIC_OBJECT | btCollisionObject::CF_CHARACTER_OBJECT);
	BulletCharacterController* controller = 
		new BulletCharacterController(actorId, ghostObject, collisionShape, 16.f);
	controller->setGravity(mDynamicsWorld->getGravity());

	mDynamicsWorld->addCollisionObject(ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::AllFilter);
	mDynamicsWorld->addAction(controller);

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToAction[actorId] = controller;
	mActorIdToCollisionObject[actorId] = ghostObject;
	mCollisionObjectToActorId[ghostObject] = actorId;
}

void QuakePhysics::GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations)
{
	if (btCollisionObject* const pCollisionObject = FindBulletCollisionObject(id))
	{
		LogAssert(pCollisionObject, "no collision object");

		if (pCollisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (BulletCharacterController* const controller =
				dynamic_cast<BulletCharacterController*>(FindBulletAction(id)))
			{
				controller->getInterpolations(interpolations);
			}
		}
		else
		{
			const btTransform& actorTransform = pCollisionObject->getInterpolationWorldTransform();
			interpolations.push_back({ btTransformToTransform(actorTransform), true });
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// CreateQuakePhysics 
//   The free function that creates an object that implements the BaseGamePhysic interface.
//
BaseGamePhysic* CreateQuakePhysics()
{
	std::unique_ptr<BaseGamePhysic> gamePhysics;
	gamePhysics.reset( new QuakePhysics );

	if (gamePhysics.get() && !gamePhysics->Initialize())
	{
		// physics failed to initialize.  delete it.
		gamePhysics.reset();
	}

	return gamePhysics.release();
}