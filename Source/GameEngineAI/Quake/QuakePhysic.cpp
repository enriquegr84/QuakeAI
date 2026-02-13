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

#include "Game/Actor/Actor.h"
#include "Game/Actor/TransformComponent.h"

#if defined(PHYSX) && defined(_WIN64)

static Vector3<float> PxVector3ToVector3(PxVec3 const& pxVec)
{
	return Vector3<float>{ pxVec.x, pxVec.y, pxVec.z };
}

static Transform PxTransformToTransform(PxTransform const& trans)
{
	// convert from PxMat44 (Physx) to matrix4 (GameEngine)
	PxMat44 pxMatrix = PxMat44(trans);

	// copy rotation matrix
	const PxVec4& col0 = pxMatrix.column0;
	const PxVec4& col1 = pxMatrix.column1;
	const PxVec4& col2 = pxMatrix.column2;
	Matrix4x4<float> rotationMatrix;
	rotationMatrix.SetCol(0, Vector4<float>{col0[0], col0[1], col0[2], col0[3]});
	rotationMatrix.SetCol(1, Vector4<float>{col1[0], col1[1], col1[2], col1[3]});
	rotationMatrix.SetCol(2, Vector4<float>{col2[0], col2[1], col2[2], col2[3]});

	// copy position
	const PxVec4& col3 = pxMatrix.column3;
	Vector4<float> translationVector = Vector4<float>{ col3[0], col3[1], col3[2], col3[3] };

	Transform returnTransform;
	returnTransform.SetRotation(rotationMatrix);
	returnTransform.SetTranslation(translationVector);
	return returnTransform;
}

QuakePhysX::QuakePhysX() : PhysX()
{
	mGravity = Settings::Get()->GetVector3("default_gravity");
}


/////////////////////////////////////////////////////////////////////////////
// QuakePhysX::~QuakePhysX
//
QuakePhysX::~QuakePhysX()
{

}

void QuakePhysX::UpdatePlayerState(ActorId playerId, PxController* controller)
{
	PxControllerState controllerState;
	controller->getState(controllerState);

	//check if CCT is onground
	mCCTGround[controller] = (controllerState.collisionFlags & PxControllerCollisionFlag::eCOLLISION_DOWN);

	PxExtendedVec3 position = controller->getPosition();
	Transform transform = PxTransformToTransform(controller->getActor()->getGlobalPose());
	transform.SetTranslation((float)position.x, (float)position.y, (float)position.z);
	mInterpolations[playerId].push_back({transform, mCCTGround[controller]});

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	aiManager->SetPlayerGround(playerId, mCCTGround[controller]);
	if (!mCCTGround[controller])
		return;
	
	mCCTJump[controller] = PxVec3(PxZero);
	mCCTFall[controller] = PxVec3(PxZero);
	mCCTJumpAccel[controller] = PxVec3(PxZero);
	mCCTFallAccel[controller] = PxVec3(PxZero);

	PxExtendedVec3 footPosition = controller->getFootPosition();

	QuakeLogic* quake = dynamic_cast<QuakeLogic*>(GameLogic::Get());
	std::vector<std::shared_ptr<Actor>> triggers;
	quake->GetTriggerActors(triggers);
	for (auto& trigger : triggers)
	{
		std::shared_ptr<PushTrigger> pTriggerPushComponent(
			trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock());
		if (pTriggerPushComponent)
		{
			PxShape* triggerShape;
			PxRigidActor* triggerActor = FindPhysXCollisionObject(trigger->GetId());
			triggerActor->getShapes(&triggerShape, 1);

			PxReal dist = PxGeometryQuery::pointDistance(PxVec3((float)footPosition.x, (float)footPosition.y, (float)footPosition.z),
				triggerShape->getGeometry(), triggerShape->getActor()->getGlobalPose() * triggerShape->getLocalPose());
			if (dist <= 0.0f)
			{
				std::shared_ptr<EventDataPhysTriggerEnter> pEvent(
					new EventDataPhysTriggerEnter(trigger->GetId(), playerId));
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}
		}
	}

	std::shared_ptr<QuakeAIView> aiView;
	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (std::shared_ptr<BaseGameView> gameView : gameViews)
		if (gameView->GetType() == GV_AI && gameView->GetActorId() == playerId)
			aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);

	if (aiView && aiView->GetPathingGraph())
	{
		bool updatedActionPlan = aiView->UpdateActionPlan(false);

		Vector3<float> currentPosition = transform.GetTranslation();
		if (!aiView->UpdateActionPlan(currentPosition, 0.5f))
		{
			if (updatedActionPlan)
				aiManager->UpdatePlayerView(playerId, aiView->GetActionPlayer(), false);

			return;
		}

		if (updatedActionPlan)
			aiManager->UpdatePlayerView(playerId, aiView->GetActionPlayer(), false);
		else
			aiManager->UpdatePlayerView(playerId, aiView->GetActionPlayer());

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

			velocity[AXIS_X] = direction[AXIS_X] * mJumpSpeed[playerId][AXIS_X];
			velocity[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[playerId][AXIS_Z];
			velocity[AXIS_Y] = mJumpSpeed[playerId][AXIS_Y];

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[playerId][AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[playerId][AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[playerId][AXIS_Y];

			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataJumpActor>(playerId, velocity, fall));
		}
		else if (aiView->GetActionPlanType() == AT_MOVE)
		{
			Vector4<float> direction = atWorld;
			direction[AXIS_Y] = 0;
			Normalize(direction);

			velocity = HProject(direction);
			velocity *= mMoveSpeed[playerId];

			//specific to physx
			velocity[AXIS_Y] = mGravity[AXIS_Y];

			fall = mGravity;

			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataMoveActor>(playerId, velocity, fall));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// QuakePhysX::OnUpdate
//
void QuakePhysX::OnUpdate(float const deltaSeconds)
{
	ResetInterpolations();

	for (auto& actorController : mActorIdToController)
	{
		ActorId playerId = actorController.first;
		PxController* const controller = actorController.second;

		PxVec3 velocity = mCCTMove[controller] * deltaSeconds;

		if (mCCTJump[controller].z > 0.f)  //jump condition
		{
			if (mCCTGround[controller])
			{
				// Instantly teleport the capsule upward by a small amount (e.g. 0.01–0.1)
				PxExtendedVec3 upOffset = PxExtendedVec3(0.f, 0.f, 0.1f);   // small upward nudge
				controller->setPosition(controller->getPosition() + upOffset);

				// Then apply upward velocity via the displacement vector
				mCCTJumpAccel[controller] = mCCTJump[controller];
				velocity = mCCTJumpAccel[controller];
				//printf("\n physx initial jump %f %f %f elpased %f", velocity[0], velocity[1], velocity[2], deltaSeconds);
			}
			else if (mCCTJumpAccel[controller].z > 0.f)
			{
				mCCTJumpAccel[controller].z -= mCCTJump[controller].z * deltaSeconds;
				velocity = mCCTJumpAccel[controller];
				//printf("\n physx jumping %f %f %f elpased %f", velocity[0], velocity[1], velocity[2], deltaSeconds);
			}
		}

		if (!mCCTGround[controller]) //fall acceleration
		{
			mCCTFallAccel[controller] += mCCTFall[controller] * deltaSeconds;
			velocity += mCCTFallAccel[controller];
			//printf("\n physx player %u falling %f %f %f elpased %f", actorController.first, velocity[0], velocity[1], velocity[2], deltaSeconds);
		}

		// update physics player
		int substeps = ceil(Length(PxVector3ToVector3(velocity)));
		float subDT = deltaSeconds / substeps;
		PxVec3 displacement = velocity / (float)substeps;

		PxControllerFilters filters;
		for (int i = 0; i < substeps; i++)
		{
			controller->move(displacement, 0.001f, subDT, filters);
			UpdatePlayerState(playerId, controller);
		}
	}

	PhysX::OnUpdate(deltaSeconds);
}

/////////////////////////////////////////////////////////////////////////////
// QuakePhysX::AddCharacterController
//
void QuakePhysX::AddCharacterController(
	const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	ActorId playerId = pStrongActor->GetId();
	mMaxPushSpeed[playerId] = Vector3<float>{4.f, 4.f, 20.f};
	mMaxJumpSpeed[playerId] = Vector3<float>{ 10.f, 10.f, 12.f };
	mMaxFallSpeed[playerId] = Vector3<float>{ 15.f, 15.f, 40.f };
	mMaxMoveSpeed[playerId] = 300.f;

	mPushSpeed[playerId] = mMaxPushSpeed[playerId];
	mJumpSpeed[playerId] = mMaxJumpSpeed[playerId];
	mFallSpeed[playerId] = mMaxFallSpeed[playerId];
	mMoveSpeed[playerId] = mMaxMoveSpeed[playerId];

	PhysX::AddCharacterController(dimensions, pGameActor, densityStr, physicMaterial);
}

void QuakePhysX::GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(id)))
	{
		for (auto const& interpolation : mInterpolations[id])
			interpolations.push_back(interpolation);
	}
	else if (PxRigidActor* const pCollisionObject = FindPhysXCollisionObject(id))
	{
		const PxTransform& actorTransform = pCollisionObject->getGlobalPose();
		interpolations.push_back({ PxTransformToTransform(actorTransform), true });
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
	gamePhysics.reset(new QuakePhysX);

	if (gamePhysics.get() && !gamePhysics->Initialize())
	{
		// physics failed to initialize.  delete it.
		gamePhysics.reset();
	}

	return gamePhysics.release();
}

#else

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
	mInterpolations.push_back({ transform, isGround });

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
	// update physics player
	int substeps = 4;
	btScalar subDT = deltaTime / substeps;

	for (int i = 0; i < substeps; i++)
	{
		preStep(collisionWorld);
		playerStep(collisionWorld, subDT);
		updateState();
	}
}

void BulletCharacterController::getInterpolations(std::vector<std::pair<Transform, bool>>& interpolations)
{
	for (auto const& interpolation : mInterpolations)
		interpolations.push_back(interpolation);
}


QuakeBulletPhysics::QuakeBulletPhysics() : BulletPhysics()
{

}


/////////////////////////////////////////////////////////////////////////////
// QuakeBulletPhysics::~QuakeBulletPhysics
//
QuakeBulletPhysics::~QuakeBulletPhysics()
{

}

/////////////////////////////////////////////////////////////////////////////
// QuakeBulletPhysics::OnUpdate
//
void QuakeBulletPhysics::OnUpdate(float const deltaSeconds)
{
	ActorIDToBulletActionMap::const_iterator itAction = mActorIdToAction.begin();
	for (; itAction != mActorIdToAction.end(); itAction++)
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
// QuakeBulletPhysics::AddCharacterController
//
void QuakeBulletPhysics::AddCharacterController(
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

void QuakeBulletPhysics::GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations)
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
	gamePhysics.reset(new QuakeBulletPhysics);

	if (gamePhysics.get() && !gamePhysics->Initialize())
	{
		// physics failed to initialize.  delete it.
		gamePhysics.reset();
	}
	return gamePhysics.release();
}

#endif