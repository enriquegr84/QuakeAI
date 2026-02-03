// QuakePlayerController.cpp - Controller class for the player 
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

#include "QuakePlayerController.h"
#include "QuakeEvents.h"
#include "QuakeApp.h"

#include "Games/Actors/PlayerActor.h"
#include "Games/Actors/PushTrigger.h"
#include "Games/Actors/TeleporterTrigger.h"

#include "Core/Logger/Logger.h"

#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

////////////////////////////////////////////////////
// QuakePlayerController Implementation
////////////////////////////////////////////////////

QuakePlayerController::QuakePlayerController(
	const std::shared_ptr<Node>& target, float initialYaw, float initialPitch)
	: mTarget(target), mEnabled(true)
{
	mYaw = (float)GE_C_RAD_TO_DEG * initialYaw;
	mPitchTarget = (float)GE_C_RAD_TO_DEG * initialPitch;

	mMaxRotateSpeed = 0.5f;
	mMouseSensitivity = Settings::Get()->GetFloat("mouse_sensitivity");
	mGravity = Settings::Get()->GetVector3("default_gravity");
	mRespawnTimeMs = 0;


#if defined(PHYSX) && defined(_WIN64)

	mMaxPushSpeed = Vector3<float>{ 4.f, 4.f, 20.f };
	mMaxJumpSpeed = Vector3<float>{ 10.f, 10.f, 12.f };
	mMaxFallSpeed = Vector3<float>{ 15.f, 15.f, 40.f };
	mMaxMoveSpeed = 300.f;

#else

	mMaxPushSpeed = Vector3<float>{
		PUSHTRIGGER_JUMP_SPEED_XZ, PUSHTRIGGER_JUMP_SPEED_XZ, PUSHTRIGGER_JUMP_SPEED_Y };
	mMaxJumpSpeed = Vector3<float>{
		DEFAULT_JUMP_SPEED_XZ, DEFAULT_JUMP_SPEED_XZ, DEFAULT_JUMP_SPEED_Y };
	mMaxFallSpeed = Vector3<float>{
		DEFAULT_FALL_SPEED_XZ, DEFAULT_FALL_SPEED_XZ, DEFAULT_FALL_SPEED_Y };
	mMaxMoveSpeed = DEFAULT_MOVE_SPEED;

#endif

	mPushSpeed = mMaxPushSpeed;
	mJumpSpeed = mMaxJumpSpeed;
	mFallSpeed = mMaxFallSpeed;
	mMoveSpeed = mMaxMoveSpeed;

	//Point cursor;
	System* system = System::Get();
	Vector2<unsigned int> cursorPosition = system->GetCursorControl()->GetPosition();
	mLastMousePos = Vector2<int>{ (int)cursorPosition[0], (int)cursorPosition[1] };

	memset(mKey, 0x00, sizeof(mKey));

	mMouseUpdate = true;
	mMouseRButtonDown = false;
	mMouseLButtonDown = false;
	mWheelRollDown = false;
	mWheelRollUp = false;

	Transform initTransform = mAbsoluteTransform;
	mProjectileActor = GameLogic::Get()->CreateActor(
		"actors/quake/effects/rocketghostlauncherfire.xml", nullptr, &initTransform);

	const std::shared_ptr<ScreenElementScene>& pScene = GameApplication::Get()->GetHumanView()->mScene;
	std::shared_ptr<Node> pProjectileNode = pScene->GetSceneNode(mProjectileActor->GetId());
	if (pProjectileNode)
		pProjectileNode->SetVisible(false);
}

ActorId QuakePlayerController::GetProjectileId() const 
{ 
	return mProjectileActor->GetId(); 
}

void QuakePlayerController::PlayerSpawn(const Transform& spawnTransform)
{
	AxisAngle<4, float> localRotation;
	spawnTransform.GetRotation(localRotation);
	mYaw = localRotation.mAngle * localRotation.mAxis[AXIS_Y] * (float)GE_C_RAD_TO_DEG;
	//mPitchTarget = -yawPitchRoll.mAngle[AXIS_Z] * (float)GE_C_RAD_TO_DEG;

	mAbsoluteTransform.SetRotation(spawnTransform.GetRotation());
	mAbsoluteTransform.SetTranslation(spawnTransform.GetTranslation());
}

//
// QuakePlayerController::OnMouseButtonDown		- Chapter 10, page 282
//
bool QuakePlayerController::OnMouseButtonDown(
	const Vector2<int>& mousePos, const int radius, const std::string& buttonName)
{
	if (buttonName == "PointerLeft")
	{
		mMouseLButtonDown = true;

		// We want mouse movement to be relative to the position
		// the cursor was at when the user first presses down on
		// the left button
		mLastMousePos = mousePos;
		return true;
	}
	else if (buttonName == "PointerRight")
	{
		mMouseRButtonDown = true;

		// We want mouse movement to be relative to the position
		// the cursor was at when the user first presses down on
		// the right button
		mLastMousePos = mousePos;
		return true;
	}
	return false;
}

bool QuakePlayerController::OnMouseButtonUp(
	const Vector2<int>& mousePos, const int radius, const std::string& buttonName)
{
	if (buttonName == "PointerLeft")
	{
		mMouseLButtonDown = false;
		return true;
	}
	else if (buttonName == "PointerRight")
	{
		mMouseRButtonDown = false;
		return true;
	}
	return false;
}


//  class QuakePlayerController::OnMouseMove		- Chapter 10, page 282
bool QuakePlayerController::OnMouseMove(const Vector2<int>& mousePos, const int radius)
{
	if (mMouseUpdate)
	{
		mRotateSpeed = mMaxRotateSpeed;

		Vector2<unsigned int> center(Renderer::Get()->GetScreenSize() / (unsigned int)2);
		Vector2<unsigned int> cursorPos = System::Get()->GetCursorControl()->GetPosition();
		Vector2<int> dist = { (int)center[0] - (int)cursorPos[0], (int)cursorPos[1] - (int)center[1] };

		mYaw += dist[0] * mMouseSensitivity * mRotateSpeed;
		mPitchTarget += dist[1] * mMouseSensitivity * mRotateSpeed;
		mLastMousePos = Vector2<int>{ (int)cursorPos[0], (int)cursorPos[1] };

		if (dist[0] != 0 || dist[1] != 0)
			System::Get()->GetCursorControl()->SetPosition(0.5f, 0.5f);
	}

	return true;
}

//  class QuakePlayerController::OnUpdate			- Chapter 10, page 283
void QuakePlayerController::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	// Special case, mouse is whipped outside of window before it can update.
	if (mEnabled)
	{
		System* system = System::Get();
		Vector2<unsigned int> cursorPosition = system->GetCursorControl()->GetPosition();
		Vector2<int> mousePosition{ (int)cursorPosition[0], (int)cursorPosition[1] };

		Renderer* renderer = Renderer::Get();
		Vector2<unsigned int> screenSize(renderer->GetScreenSize());
		RectangleShape<2, int> screenRectangle;
		screenRectangle.mCenter[0] = screenSize[0] / 2;
		screenRectangle.mCenter[1] = screenSize[1] / 2;
		screenRectangle.mExtent[0] = (int)screenSize[0];
		screenRectangle.mExtent[1] = (int)screenSize[1];

		// Only if we are moving outside quickly.
		bool reset = !screenRectangle.IsPointInside(mousePosition);

		if (reset)
		{
			// Force a reset.
			mMouseUpdate = false;
			cursorPosition = system->GetCursorControl()->GetPosition();
			mLastMousePos = Vector2<int>{ (int)cursorPosition[0], (int)cursorPosition[1] };
		}
		else mMouseUpdate = true;
	}
	//Handling rotation as a result of mouse position
	Matrix4x4<float> rotation;

	const ActorId actorId = mTarget->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	if (!pPlayerActor) return;

	if (pPlayerActor->GetState().moveType != PM_DEAD)
	{
		std::shared_ptr<TransformComponent> pTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
		{
			mPitchTarget = std::max(-85.f, std::min(85.f, mPitchTarget));
			mPitch = 90 * ((mPitchTarget + 85.f) / 170.f) - 45.f;

			// Calculate the new rotation matrix from the player
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
			rotation = -yawRotation;
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitch * (float)GE_C_DEG_TO_RAD));
			mAbsoluteTransform.SetRotation(yawRotation * pitchRotation);

			// update node rotation matrix
			pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitchTarget * (float)GE_C_DEG_TO_RAD));
			Matrix4x4<float> rollRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
			pTransformComponent->SetRotation(yawRotation * pitchRotation * rollRotation);
		}

		Vector4<float> atWorld = Vector4<float>::Zero();
		Vector4<float> rightWorld = Vector4<float>::Zero();
		Vector4<float> upWorld = Vector4<float>::Zero();

		if (mKey[KEY_KEY_W] || mKey[KEY_KEY_S])
		{
			// This will give us the "look at" vector 
			// in world space - we'll use that to move
			// the player.
			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			if (mKey[KEY_KEY_W])
				atWorld *= -1.f;
		}

		if (mKey[KEY_KEY_A] || mKey[KEY_KEY_D])
		{
			// This will give us the "look right" vector 
			// in world space - we'll use that to move
			// the player.
			rightWorld = Vector4<float>::Unit(AXIS_Z); // right vector
#if defined(GE_USE_MAT_VEC)
			rightWorld = rotation * rightWorld;
#else
			rightWorld = rightWorld * rotation;
#endif

			if (mKey[KEY_KEY_A])
				rightWorld *= -1.f;
		}

		if (pPlayerActor->GetAction().triggerTeleporter != INVALID_ACTOR_ID)
		{
			std::shared_ptr<Actor> pItemActor(std::dynamic_pointer_cast<Actor>(
				GameLogic::Get()->GetActor(pPlayerActor->GetAction().triggerTeleporter).lock()));
			std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
				pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();

			AxisAngle<4, float> localRotation;
			pTeleporterTrigger->GetTarget().GetRotation(localRotation);
			mYaw = localRotation.mAngle * localRotation.mAxis[AXIS_Y] * (float)GE_C_RAD_TO_DEG;
			//mPitchTarget = -yawPitchRoll.mAngle[AXIS_Z] * (float)GE_C_RAD_TO_DEG;

			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataTeleportActor>(actorId));
		}
		else
		{
			std::shared_ptr<PhysicComponent> pPhysicComponent(
				pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
			if (pPhysicComponent)
			{
				pPlayerActor->GetAction().actionType = ACTION_STAND;
				if (mMouseLButtonDown)
					pPlayerActor->GetAction().actionType |= ACTION_ATTACK;
				if (mWheelRollDown)
					pPlayerActor->PreviousWeapon();
				if (mWheelRollUp)
					pPlayerActor->NextWeapon();

				if (mKey[KEY_KEY_S])
					pPlayerActor->GetAction().actionType |= ACTION_MOVEBACK;
				else if (mKey[KEY_KEY_W])
					pPlayerActor->GetAction().actionType |= ACTION_MOVEFORWARD;
				else if (mKey[KEY_KEY_A])
					pPlayerActor->GetAction().actionType |= ACTION_MOVELEFT;
				else if (mKey[KEY_KEY_D])
					pPlayerActor->GetAction().actionType |= ACTION_MOVERIGHT;

				Vector3<float> fall = Vector3<float>::Zero();
				Vector3<float> velocity = Vector3<float>::Zero();
				if (pPhysicComponent->OnGround())
				{
					mFallSpeed = mMaxFallSpeed;

					if (pPlayerActor->GetAction().triggerPush != INVALID_ACTOR_ID)
					{

#if defined(PHYSX) && defined(_WIN64)

						mFallSpeed = mMaxFallSpeed;

#else

						mFallSpeed = Vector3<float>{
							PUSHTRIGGER_FALL_SPEED_XZ, PUSHTRIGGER_FALL_SPEED_XZ, PUSHTRIGGER_FALL_SPEED_Y };

#endif

						std::shared_ptr<Actor> pItemActor(std::dynamic_pointer_cast<Actor>(
							GameLogic::Get()->GetActor(pPlayerActor->GetAction().triggerPush).lock()));
						std::shared_ptr<PushTrigger> pPushTrigger =
							pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock();

						Vector3<float> targetPosition = pPushTrigger->GetTarget().GetTranslation();
						Vector3<float> playerPosition = pTransformComponent->GetPosition();
						Vector3<float> direction = targetPosition - playerPosition;
						float push = mPushSpeed[AXIS_Y];
#if defined(PHYSX) && defined(_WIN64)

						push += direction[AXIS_Y] * 0.06f;

#else

						push += direction[AXIS_Y] * 0.01f;

#endif
						direction[AXIS_Y] = 0;
						Normalize(direction);

						velocity[AXIS_X] = direction[AXIS_X] * mPushSpeed[AXIS_X];
						velocity[AXIS_Z] = direction[AXIS_Z] * mPushSpeed[AXIS_Z];
						velocity[AXIS_Y] = push;

						fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
						fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
						fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

						pPlayerActor->GetAction().actionType |= ACTION_JUMP;
					}
					else if (mEnabled)
					{
						if (mMouseRButtonDown)
						{
							upWorld = Vector4<float>::Zero();
							Vector4<float> direction = atWorld + rightWorld + upWorld;
							direction[AXIS_Y] = 0;
							Normalize(direction);

							velocity[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
							velocity[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
							velocity[AXIS_Y] = mJumpSpeed[AXIS_Y];

							fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
							fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
							fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

							pPlayerActor->GetAction().actionType |= ACTION_JUMP;
						}
						else
						{
							Vector4<float> direction = atWorld + rightWorld + upWorld;
							direction[AXIS_Y] = 0;
							Normalize(direction);

							velocity = HProject(direction);
							velocity *= mMoveSpeed;
#if defined(PHYSX) && defined(_WIN64)

							velocity[AXIS_Y] = mGravity[AXIS_Y];

#endif

							fall = mGravity;
						}
					}
					pPlayerActor->GetAction().actionType |= ACTION_RUN;
				}
				else
				{
					upWorld = Vector4<float>::Zero();
					Vector4<float> direction = atWorld + rightWorld + upWorld;
					direction[AXIS_Y] = 0;
					Normalize(direction);

					direction[AXIS_X] *= mFallSpeed[AXIS_X];
					direction[AXIS_Z] *= mFallSpeed[AXIS_Z];
					direction[AXIS_Y] = -mFallSpeed[AXIS_Y];
					velocity = HProject(direction);
					fall = HProject(direction);

					pPlayerActor->GetAction().actionType |= ACTION_FALL;
				}

				EventManager::Get()->TriggerEvent(
					std::make_shared<EventDataRotateActor>(actorId, mYaw, mPitch));

				pPlayerActor->UpdateTimers(deltaMs);
				pPlayerActor->UpdateWeapon(deltaMs);
				pPlayerActor->UpdateMovement(velocity, fall);
			}
		}

		mRespawnTimeMs = 0;
	}
	else
	{
		mRespawnTimeMs += deltaMs;
		if (mRespawnTimeMs >= 2000)
		{
			pPlayerActor->PlayerSpawn();
			pPlayerActor->GetAction().actionType = ACTION_STAND;
		}
		else
		{
			pPlayerActor->GetAction().actionType = ACTION_RUN;
			if (pPlayerActor->GetState().stats[STAT_HEALTH] > 0)
				pPlayerActor->GetState().legsAnim = LEGS_IDLE;

			pPlayerActor->UpdateTimers(deltaMs);
			pPlayerActor->UpdateWeapon(deltaMs);

#if defined(PHYSX) && defined(_WIN64)

			pPlayerActor->UpdateMovement(mGravity, mGravity);

#else

			pPlayerActor->UpdateMovement(Vector3<float>::Zero(), mGravity);

#endif
		}
	}

	if (mProjectileActor)
	{
		Matrix4x4<float> rotation;
		EulerAngles<float> viewAngles;
		std::shared_ptr<TransformComponent> pPlayerTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pPlayerTransformComponent)
		{
			viewAngles.mAxis[1] = 1;
			viewAngles.mAxis[2] = 2;
			pPlayerTransformComponent->GetTransform().GetRotation(viewAngles);
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), viewAngles.mAngle[2]));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), viewAngles.mAngle[1]));
			rotation = yawRotation * pitchRotation;
		}
		Vector3<float> forward = HProject(rotation * Vector4<float>::Unit(AXIS_X));
		Vector3<float> right = HProject(rotation * Vector4<float>::Unit(AXIS_Z));
		Vector3<float> up = HProject(rotation * Vector4<float>::Unit(AXIS_Y));

		//set muzzle location relative to pivoting eye
		std::shared_ptr<PhysicComponent> pPlayerPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		Vector3<float> muzzle = pPlayerPhysicComponent->GetTransform().GetTranslation();
		muzzle += up * (float)pPlayerActor->GetState().viewHeight;
		muzzle += forward * 5.f;
		muzzle -= right * 5.f;

		Transform actorTransform;
		actorTransform.SetRotation(rotation);
		actorTransform.SetTranslation(muzzle);
		std::shared_ptr<PhysicComponent> pActorPhysicComponent =
			mProjectileActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		pActorPhysicComponent->SetTransform(actorTransform);

		// update projectile node rotation matrix
		const std::shared_ptr<ScreenElementScene>& pScene = GameApplication::Get()->GetHumanView()->mScene;
		std::shared_ptr<Node> pProjectileNode = pScene->GetSceneNode(mProjectileActor->GetId());
		if (pProjectileNode)
		{
			pProjectileNode->GetRelativeTransform().SetRotation(mAbsoluteTransform);
			pProjectileNode->UpdateAbsoluteTransform();
		}
		/*
		Vector3<float> scale =
			GameLogic::Get()->GetGamePhysics()->GetScale(pPlayerActor->GetId()) / 2.f;
		Transform start = pPlayerPhysicComponent->GetTransform();
		start.SetTranslation(start.GetTranslation() + scale[2] * Vector3<float>::Unit(AXIS_Y));
		Transform end = pPlayerPhysicComponent->GetTransform();
		end.SetTranslation(end.GetTranslation() + forward * 2000.f + scale[2] * Vector3<float>::Unit(AXIS_Y));

		std::vector<ActorId> collisionActors;
		std::vector<Vector3<float>> collisions, collisionNormals;
		std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();
		gamePhysics->ConvexSweep(pPlayerActor->GetId(), start, end, collisionActors, collisions, collisionNormals);

		ActorId closestCollisionId = INVALID_ACTOR_ID;
		std::optional<Vector3<float>> closestCollision = std::nullopt;
		for (unsigned int i = 0; i < collisionActors.size(); i++)
		{
			if (collisionActors[i] == INVALID_ACTOR_ID)
			{
				if (closestCollision.has_value())
				{
					if (Length(closestCollision.value() - start.GetTranslation()) > Length(collisions[i] - start.GetTranslation()))
					{
						closestCollisionId = collisionActors[i];
						closestCollision = collisions[i];
					}
				}
				else
				{
					closestCollisionId = collisionActors[i];
					closestCollision = collisions[i];
				}
			}
		}
		if (closestCollision.has_value())
			printf("\nclosest collision %f", Length(closestCollision.value() - start.GetTranslation()));
		*/
	}

	mWheelRollDown = false;
	mWheelRollUp = false;
}