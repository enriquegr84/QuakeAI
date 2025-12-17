/*******************************************************
 * QuakeAIView.cpp : AI Controller class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#include "QuakeStd.h"

#include "QuakeAIManager.h"
#include "QuakeAIView.h"
#include "QuakeEvents.h"
#include "QuakeApp.h"
#include "Quake.h"

#include "Games/Actors/PushTrigger.h"
#include "Games/Actors/TeleporterTrigger.h"

#include "Core/OS/OS.h"
#include "Core/Logger/Logger.h"
#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

#include "AI/AIManager.h"

///////////////////////////////////////////////////////////////////////////////
//
// QuakeAIView::QuakeAIView
//

QuakeAIView::QuakeAIView() : BaseGameView(), mBehavior(BT_STAND), mEnabled(true)
{
	mYaw = 0.f;
	mYawSmooth = 0.f;
	mYawSmoothTime = 0.f;
	mPitchTarget = 0.f;
	mPitch = 0.f;

	mOrientation = 1;
	mStationaryTime = 0;
	mStationaryPosition = Vector3<float>::Zero();

	mGravity = Settings::Get()->GetVector3("default_gravity");
	mRespawnTimeMs = 0;

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

	mCurrentPlayerData = PlayerData();
	mCurrentPlanAction = -1;
	mCurrentPlanArc = NULL;

	mGoalNode = NULL;

	mViewId = INVALID_GAME_VIEW_ID;
	mPlayerId = INVALID_ACTOR_ID;
}

//
// QuakeAIView::~QuakeAIView
//
QuakeAIView::~QuakeAIView(void)
{
	//LogInformation("AI Destroying QuakeAIView");
}

//  class QuakeAIView::OnAttach
void QuakeAIView::OnAttach(GameViewId vid, ActorId actorId)
{
	mViewId = vid;
	mPlayerId = actorId;

	mCurrentPlayerData = PlayerData();
	mCurrentPlayerData.player = mPlayerId;
	mCurrentPlayerData.valid = true;
}

void QuakeAIView::PlayerSpawn(const Transform& spawnTransform)
{
	AxisAngle<4, float> localRotation;
	spawnTransform.GetRotation(localRotation);
	mYaw = mYawSmooth = localRotation.mAngle * localRotation.mAxis[AXIS_Y] * (float)GE_C_RAD_TO_DEG;
	//mPitchTarget = -yawPitchRoll.mAngle[AXIS_Z] * (float)GE_C_RAD_TO_DEG;
	mStationaryPosition = spawnTransform.GetTranslation();

	mAbsoluteTransform.SetRotation(spawnTransform.GetRotation());
	mAbsoluteTransform.SetTranslation(spawnTransform.GetTranslation());

	if (!mProjectileActor)
	{
		Transform initTransform = mAbsoluteTransform;
		mProjectileActor = GameLogic::Get()->CreateActor(
			"actors/quake/effects/rocketghostlauncherfire.xml", nullptr, &initTransform);

		const std::shared_ptr<ScreenElementScene>& pScene = GameApplication::Get()->GetHumanView()->mScene;
		std::shared_ptr<Node> pProjectileNode = pScene->GetSceneNode(mProjectileActor->GetId());
		if (pProjectileNode)
			pProjectileNode->SetVisible(false);
	}
}

bool QuakeAIView::IsAvailableWeapon(std::shared_ptr<PlayerActor> pPlayerActor, WeaponType weapon)
{
	int maxAmmo = 0;
	switch (weapon)
	{
		case WP_LIGHTNING:
			maxAmmo = 200;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				return true;
			}
			break;

		case WP_RAILGUN:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				return true;
			}
			break;

		case WP_SHOTGUN:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				return true;
			}
			break;

		case WP_ROCKET_LAUNCHER:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				return true;
			}
			break;

		case WP_PLASMAGUN:
			maxAmmo = 120;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				return true;
			}
			break;

		case WP_MACHINEGUN:
			maxAmmo = 200;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				return true;
			}
			break;

		case WP_GRENADE_LAUNCHER:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				//low tier weapon
			}
			break;

		case WP_GAUNTLET:
			{
				return true;
			}
			break;
	}

	return false;
}

bool QuakeAIView::IsOptimalWeapon(std::shared_ptr<PlayerActor> pPlayerActor, WeaponType weapon, float shootingDistance)
{
	int maxAmmo = 0;
	switch (weapon)
	{
		case WP_LIGHTNING:
			maxAmmo = 200;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				if (shootingDistance <= LIGHTNING_RANGE)
					return true;
			}
			break;

		case WP_RAILGUN:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				if (shootingDistance >= 400.f)
					return true;
			}
			break;

		case WP_SHOTGUN:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				if (shootingDistance <= 250.f)
					return true;
			}
			break;

		case WP_ROCKET_LAUNCHER:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				if (shootingDistance <= 250.f)
					return true;
			}
			break;

		case WP_PLASMAGUN:
			maxAmmo = 120;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				if (shootingDistance <= 200.f)
					return true;
			}
			break;

		case WP_MACHINEGUN:
			maxAmmo = 200;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				if (shootingDistance <= 300.f)
					return true;
			}
			break;

		case WP_GRENADE_LAUNCHER:
			maxAmmo = 20;
			if (pPlayerActor->GetState().ammo[weapon] >= maxAmmo * 0.1f && 
				(pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << weapon)))
			{
				//low tier weapon
			}
			break;

		case WP_GAUNTLET:
			{
				if (shootingDistance <= 30.f)
					return true;
			}
			break;
	}

	return false;
}

//Stationary movement
void QuakeAIView::Stationary(unsigned long deltaMs)
{
	Vector3<float> position = mAbsoluteTransform.GetTranslation();
	Matrix4x4<float> rotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));

	// This will give us the "look at" vector 
	// in world space - we'll use that to move.
	Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
	atWorld = rotation * atWorld;
#else
	atWorld = atWorld * rotation;
#endif

	Vector3<float> scale =
		GameLogic::Get()->GetGamePhysics()->GetScale(mPlayerId) / 2.f;

	Transform start;
	start.SetRotation(rotation);
	start.SetTranslation(mAbsoluteTransform.GetTranslationW1() + 
		scale[2] * Vector4<float>::Unit(AXIS_Y));

	Transform end;
	end.SetRotation(rotation);
	end.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
		atWorld * 500.f + scale[2] * Vector4<float>::Unit(AXIS_Y));

	std::optional<Vector3<float>> collision, collisionNormal;
	collision = end.GetTranslation();
	ActorId actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
		mPlayerId, start, end, collision, collisionNormal);
	if (collision.has_value() && Length(collision.value() - position) < 50.f)
	{
		mStationaryTime += deltaMs;
		if (mStationaryTime > 100)
		{
			//Choose randomly which way too look for obstacles
			int sign = Randomizer::Rand() % 2 ? 1 : -1;
			mYaw += 130.f * sign;
		}
	}
	else mStationaryTime = 0;
}

// Cliff control
void QuakeAIView::Cliff()
{
	Matrix4x4<float> rotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));

	// This will give us the "look at" vector 
	// in world space - we'll use that to move.
	Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
	atWorld = rotation * atWorld;
#else
	atWorld = atWorld * rotation;
#endif

	Vector3<float> position = HProject(
		mAbsoluteTransform.GetTranslationW1() + atWorld * 10.f);

	Transform start;
	start.SetRotation(rotation);
	start.SetTranslation(position);

	Transform end;
	end.SetRotation(rotation);
	end.SetTranslation(mAbsoluteTransform.GetTranslationW1() + 
		atWorld * 10.f - Vector4<float>::Unit(AXIS_Y) * 300.f);

	Vector3<float> collision, collisionNormal;
	collision = end.GetTranslation();
	ActorId actorId = GameLogic::Get()->GetGamePhysics()->CastRay(
		start.GetTranslation(), end.GetTranslation(), collision, collisionNormal);

	//Check whether we are close to a cliff
	if (abs(collision[AXIS_Y] - position[AXIS_Y]) > 60.f)
	{
		//Choose randomly which way too look for getting out the cliff
		int sign = Randomizer::Rand() % 2 ? 1 : -1;

		// Smoothly turn 110º and check raycasting until we meet a minimum distance
		for (int angle = 1; angle <= 110; angle++)
		{
			rotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y),
				(mYaw + angle * sign) * (float)GE_C_DEG_TO_RAD));

			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			start.SetRotation(rotation);
			end.SetRotation(rotation);
			end.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
				atWorld * 100.f - Vector4<float>::Unit(AXIS_Y) * 300.f);

			collision = end.GetTranslation();
			ActorId actorId = GameLogic::Get()->GetGamePhysics()->CastRay(
				start.GetTranslation(), end.GetTranslation(), collision, collisionNormal);
			if (abs(collision[AXIS_Y] - position[AXIS_Y]) <= 60.f)
			{
				mOrientation = Randomizer::Rand() % 2 ? 1 : -1;
				mYaw += angle * sign;
				return;
			}
		}

		//If we haven't find a way out we proceed exactly the same but in the opposite direction
		sign *= -1;
		for (int angle = 1; angle <= 110; angle++)
		{
			rotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y),
				(mYaw + angle * sign) * (float)GE_C_DEG_TO_RAD));

			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			start.SetRotation(rotation);
			end.SetRotation(rotation);
			end.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
				atWorld * 100.f - Vector4<float>::Unit(AXIS_Y) * 300.f);

			collision = end.GetTranslation();
			ActorId actorId = GameLogic::Get()->GetGamePhysics()->CastRay(
				start.GetTranslation(), end.GetTranslation(), collision, collisionNormal);
			if (abs(collision[AXIS_Y] - position[AXIS_Y]) <= 60.f)
			{
				mOrientation = Randomizer::Rand() % 2 ? 1 : -1;
				mYaw += angle * sign;
				return;
			}
		}

		//if we couldnt find any way out then we make a hard turn
		mOrientation = Randomizer::Rand() % 2 ? 1 : -1;
		mYaw += 130.f * sign;
	}
}

//Avoidance
void QuakeAIView::Avoidance(unsigned long deltaMs)
{
	Vector3<float> position = mAbsoluteTransform.GetTranslation();
	Matrix4x4<float> rotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));

	// This will give us the "look at" vector 
	// in world space - we'll use that to move.
	Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
	atWorld = rotation * atWorld;
#else
	atWorld = atWorld * rotation;
#endif

	Vector3<float> scale =
		GameLogic::Get()->GetGamePhysics()->GetScale(mPlayerId) / 2.f;

	Transform start;
	start.SetRotation(rotation);
	start.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
		scale[AXIS_Y] * Vector4<float>::Unit(AXIS_Y));

	Transform end;
	end.SetRotation(rotation);
	end.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
		atWorld * 500.f + scale[AXIS_Y] * Vector4<float>::Unit(AXIS_Y));

	std::optional<Vector3<float>> collision, collisionNormal;
	collision = end.GetTranslation();
	ActorId actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
		mPlayerId, start, end, collision, collisionNormal);
	if (collision.has_value() && Length(collision.value() - position) < 50.f)
	{
		//Choose randomly which way too look for obstacles
		int sign = mOrientation;

		// Smoothly turn 90º and check raycasting until we meet a minimum distance
		for (int angle = 1; angle <= 90; angle++)
		{
			rotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y),
					(mYaw + angle * sign) * (float)GE_C_DEG_TO_RAD));

			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			start.SetRotation(rotation);
			end.SetRotation(rotation);
			end.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
				atWorld * 500.f + scale[AXIS_Y] * Vector4<float>::Unit(AXIS_Y));

			actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
				mPlayerId, start, end, collision, collisionNormal);
			if (collision.has_value() && Length(collision.value() - position) > 50.f)
			{
				mYaw += angle * sign;
				return;
			}
		}

		//If we haven't find a way out we proceed exactly the same but in the opposite direction
		sign *= -1;
		for (int angle = 1; angle <= 90; angle++)
		{
			rotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y),
					(mYaw + angle * sign) * (float)GE_C_DEG_TO_RAD));

			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			start.SetRotation(rotation);
			end.SetRotation(rotation);
			end.SetTranslation(mAbsoluteTransform.GetTranslationW1() +
				atWorld * 500.f + scale[AXIS_Y] * Vector4<float>::Unit(AXIS_Y));

			actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
				mPlayerId, start, end, collision, collisionNormal);
			if (collision.has_value() && Length(collision.value() - position) > 50.f)
			{
				mYaw += angle * sign;
				return;
			}
		}
	}
}

//Smooth movement
void QuakeAIView::Smooth(unsigned long deltaMs)
{
	Vector3<float> position = mAbsoluteTransform.GetTranslation();
	Matrix4x4<float> rotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));

	// This will give us the "look at" vector 
	// in world space - we'll use that to move.
	Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
	atWorld = rotation * atWorld;
#else
	atWorld = atWorld * rotation;
#endif

	Vector3<float> scale = 
		GameLogic::Get()->GetGamePhysics()->GetScale(mPlayerId) / 2.f;

	Transform start;
	start.SetRotation(rotation);
	start.SetTranslation(mAbsoluteTransform.GetTranslationW1() 
		+ scale[2] * Vector4<float>::Unit(AXIS_Y));

	Transform end;
	end.SetRotation(rotation);
	end.SetTranslation(mAbsoluteTransform.GetTranslationW1() + 
		atWorld * 500.f + scale[2] * Vector4<float>::Unit(AXIS_Y));

	std::optional<Vector3<float>> collision, collisionNormal;
	collision = end.GetTranslation();
	ActorId actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
		mPlayerId, start, end, collision, collisionNormal);
	if (collision.has_value() && Length(collision.value() - position) < 80.f)
	{
		//Choose randomly which way too look for obstacles
		int sign = Randomizer::Rand() % 2 ? 1 : -1;

		// Smoothly turn 90º and check raycasting until we meet a minimum distance
		for (int angle = 1; angle <= 90; angle++)
		{
			rotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y),
				(mYaw + angle * sign) * (float)GE_C_DEG_TO_RAD));

			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			start.SetRotation(rotation);
			end.SetRotation(rotation);
			end.SetTranslation(mAbsoluteTransform.GetTranslationW1() + 
				atWorld * 500.f + scale[2] * Vector4<float>::Unit(AXIS_Y));

			actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
				mPlayerId, start, end, collision, collisionNormal);
			if (collision.has_value() && Length(collision.value() - position) > 80.f)
			{
				mOrientation = Randomizer::Rand() % 2 ? 1 : -1;
				mYaw += angle * sign;
				return;
			}
		}

		//If we haven't find a way out we proceed exactly the same but in the opposite direction
		sign *= -1;
		for (int angle = 1; angle <= 90; angle++)
		{
			rotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y),
				(mYaw + angle * sign) * (float)GE_C_DEG_TO_RAD));

			atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
			atWorld = rotation * atWorld;
#else
			atWorld = atWorld * rotation;
#endif

			start.SetRotation(rotation);
			end.SetRotation(rotation);
			end.SetTranslation(mAbsoluteTransform.GetTranslationW1() + 
				atWorld * 500.f + scale[2] * Vector4<float>::Unit(AXIS_Y));

			actorId = GameLogic::Get()->GetGamePhysics()->ConvexSweep(
				mPlayerId, start, end, collision, collisionNormal);
			if (collision.has_value() && Length(collision.value() - position) > 80.f)
			{
				mOrientation = Randomizer::Rand() % 2 ? 1 : -1;
				mYaw += angle * sign;
				return;
			}
		}

		//if we couldnt find any way out the stationary function will take care of it.
		mOrientation = Randomizer::Rand() % 2 ? 1 : -1;
	}
	else
	{
		mYaw += 0.03f * deltaMs * mOrientation;
	}
}

void QuakeAIView::ResetActionPlan()
{
	mGoalNode = NULL;
	mCurrentPlanArc = NULL;
	mCurrentPlayerData = PlayerData();
	mCurrentPlayerData.player = mPlayerId;
	mCurrentPlayerData.valid = true;

	mCurrentPlanAction = -1;
}

void QuakeAIView::UpdatePlayerWeapon(const PlayerView& playerView)
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());

	mCurrentPlayerData.target = INVALID_ACTOR_ID;
	if (playerView.simulation.weapon != WP_NONE)
	{
		std::shared_ptr<PlayerActor> pPlayerActor(
			std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mPlayerId).lock()));
		if (playerView.simulation.weapon != pPlayerActor->GetAction().weaponSelect)
		{
			std::shared_ptr<PhysicComponent> pPlayerPhysicComponent(
				pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
			Vector3<float> playerPosition = pPlayerPhysicComponent->GetTransform().GetTranslation();

			const PlayerGuessView& playerGuessView = playerView.guessViews.at(playerView.simulation.target);
			Vector3<float> playerGuessPosition = aiManager->CalculatePathPosition(playerGuessView.data);
			float playerDistance = Length(playerPosition - playerGuessPosition);

			//check if the selected weapon in the simulation is optimal
			if (!IsOptimalWeapon(pPlayerActor, playerView.simulation.weapon, playerDistance))
			{
				//check if current selected weapon is optimal
				bool isOptimal = IsOptimalWeapon(pPlayerActor, (WeaponType)pPlayerActor->GetAction().weaponSelect, playerDistance);
				if (isOptimal)
				{
					//if it is a low tier weapon we look for another optimal weapon
					if (pPlayerActor->GetAction().weaponSelect == WP_GRENADE_LAUNCHER ||
						pPlayerActor->GetAction().weaponSelect == WP_MACHINEGUN ||
						pPlayerActor->GetAction().weaponSelect == WP_PLASMAGUN ||
						pPlayerActor->GetAction().weaponSelect == WP_GAUNTLET)
					{
						isOptimal = false;
					}
				}
				if (!isOptimal)
				{
					//make sure that the current weapon at least is not a bad choice
					for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
					{
						if (IsOptimalWeapon(pPlayerActor, (WeaponType)weapon, playerDistance))
						{
							switch (weapon)
							{
								case WP_LIGHTNING:
									if (pPlayerActor->GetAction().weaponSelect != WP_RAILGUN)
									{
										//change to lightning gun if railgun is not available
										pPlayerActor->ChangeWeapon((WeaponType)weapon);
									}
									break;

								case WP_RAILGUN:
									//change to a railgun if we can keep the distances
									pPlayerActor->ChangeWeapon((WeaponType)weapon);
									break;

								case WP_SHOTGUN:
										//change to a shotgun in short range 
										pPlayerActor->ChangeWeapon((WeaponType)weapon);
									break;

								case WP_ROCKET_LAUNCHER:
									if (pPlayerActor->GetAction().weaponSelect == WP_GRENADE_LAUNCHER ||
										pPlayerActor->GetAction().weaponSelect == WP_MACHINEGUN ||
										pPlayerActor->GetAction().weaponSelect == WP_PLASMAGUN ||
										pPlayerActor->GetAction().weaponSelect == WP_GAUNTLET)
									{
										//if player weapons is lower tier we will change to rl
										pPlayerActor->ChangeWeapon((WeaponType)weapon);
									}
									break;

								case WP_PLASMAGUN:
									break;

								case WP_MACHINEGUN:
									break;

								case WP_GRENADE_LAUNCHER:
									break;

								case WP_GAUNTLET:
									break;
							}
						
							if (pPlayerActor->GetAction().weaponSelect == WP_ROCKET_LAUNCHER ||
								pPlayerActor->GetAction().weaponSelect == WP_SHOTGUN || 
								pPlayerActor->GetAction().weaponSelect == WP_RAILGUN)
								break;
						}
					}
				}

				//check if selected weapon is still not optimal
				isOptimal = IsOptimalWeapon(pPlayerActor, (WeaponType)pPlayerActor->GetAction().weaponSelect, playerDistance);
				if (isOptimal)
				{
					//if it is a low tier weapon we look for best available weapon
					if (pPlayerActor->GetAction().weaponSelect == WP_GRENADE_LAUNCHER ||
						pPlayerActor->GetAction().weaponSelect == WP_MACHINEGUN ||
						pPlayerActor->GetAction().weaponSelect == WP_PLASMAGUN ||
						pPlayerActor->GetAction().weaponSelect == WP_GAUNTLET)
					{
						isOptimal = false;
					}
				}
				if (!isOptimal)
				{
					for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
					{
						if (IsAvailableWeapon(pPlayerActor, (WeaponType)weapon))
						{
							switch (weapon)
							{
								case WP_LIGHTNING:
									if (pPlayerActor->GetAction().weaponSelect != WP_RAILGUN)
									{
										//change to lightning gun if railgun is not available
										pPlayerActor->ChangeWeapon((WeaponType)weapon);
									}
									break;

								case WP_RAILGUN:
									//if player weapon is lower tier change to a railgun if we can keep the distances
									pPlayerActor->ChangeWeapon((WeaponType)weapon);
									break;

								case WP_SHOTGUN:
									if (pPlayerActor->GetAction().weaponSelect == WP_GRENADE_LAUNCHER ||
										pPlayerActor->GetAction().weaponSelect == WP_MACHINEGUN ||
										pPlayerActor->GetAction().weaponSelect == WP_PLASMAGUN ||
										pPlayerActor->GetAction().weaponSelect == WP_GAUNTLET)
									{
										//if player weapons is lower tier change to a shotgun in short range 
										pPlayerActor->ChangeWeapon((WeaponType)weapon);
									}
									break;

								case WP_ROCKET_LAUNCHER:
									if (pPlayerActor->GetAction().weaponSelect == WP_GRENADE_LAUNCHER ||
										pPlayerActor->GetAction().weaponSelect == WP_MACHINEGUN ||
										pPlayerActor->GetAction().weaponSelect == WP_PLASMAGUN ||
										pPlayerActor->GetAction().weaponSelect == WP_GAUNTLET)
									{
										//if player weapons is lower tier we will change to rl
										pPlayerActor->ChangeWeapon((WeaponType)weapon);
									}
									break;

								case WP_PLASMAGUN:
									break;

								case WP_MACHINEGUN:
									break;

								case WP_GRENADE_LAUNCHER:
									break;

								case WP_GAUNTLET:
									break;
							}
						}
					}
				}
			}
			else
			{
				if (IsOptimalWeapon(pPlayerActor, (WeaponType)pPlayerActor->GetAction().weaponSelect, playerDistance))
				{
					//if it is a low tier weapon we will change the weapon
					if (pPlayerActor->GetAction().weaponSelect == WP_GRENADE_LAUNCHER ||
						pPlayerActor->GetAction().weaponSelect == WP_MACHINEGUN ||
						pPlayerActor->GetAction().weaponSelect == WP_PLASMAGUN ||
						pPlayerActor->GetAction().weaponSelect == WP_GAUNTLET)
					{
						pPlayerActor->ChangeWeapon(playerView.simulation.weapon);
					}
				}
				else
				{
					pPlayerActor->ChangeWeapon(playerView.simulation.weapon);
				}
			}

			if (pPlayerActor->IsChangingWeapon())
			{
				mCurrentPlayerData.target = playerView.simulation.target;

				std::stringstream weaponInfo;
				weaponInfo << "\n CHANGE WEAPON frame " << aiManager->GetFrame() <<
					" player " << mPlayerId << " current wp " << pPlayerActor->GetState().weapon <<
					" change to wp " << pPlayerActor->GetAction().weaponSelect;
				aiManager->PrintInfo(weaponInfo.str());
			}
		}
		else mCurrentPlayerData.target = playerView.simulation.target;
	}
}

void QuakeAIView::UpdatePlayerItems(const PlayerData& player)
{
	float pathWeight = 0.f;
	std::unordered_map<ActorId, float> playerItems;
	std::unordered_map<ActorId, short> playerItemAmounts;
	std::unordered_map<ActorId, float> playerItemWeights;
	PathingArcVec::iterator itArc = mCurrentPlayerData.plan.path.begin();
	for (; itArc != mCurrentPlayerData.plan.path.end(); itArc++)
	{
		pathWeight += (*itArc)->GetWeight();

		ActorId itemId = (*itArc)->GetNode()->GetActorId();
		if (playerItems.find(itemId) == playerItems.end())
		{
			if (mCurrentPlayerData.items.find(itemId) != mCurrentPlayerData.items.end())
			{
				playerItemWeights[itemId] = pathWeight;
				playerItems[itemId] = mCurrentPlayerData.items[itemId];
				playerItemAmounts[itemId] = mCurrentPlayerData.itemAmount[itemId];
			}
			else if (player.items.find(itemId) != player.items.end())
			{
				playerItemWeights[itemId] = pathWeight;
				playerItems[itemId] = player.items.at(itemId);
				playerItemAmounts[itemId] = player.itemAmount.at(itemId);
			}
		}
	}

	mCurrentPlayerData.items = playerItems;
	mCurrentPlayerData.itemAmount = playerItemAmounts;
	mCurrentPlayerData.itemWeight = playerItemWeights;
}

bool QuakeAIView::UpdateActionPlan(int actionType)
{
	if (actionType == -1)
		return false;

	if (mCurrentPlanArc)
	{
		//printf("\n");

		bool foundActionPlan = false;
		do
		{
			if (mCurrentPlanAction == actionType)
				foundActionPlan = true;
			/*
			printf("player id %u arc id %u type %u weight %f node %u \n ", mPlayerId,
				mCurrentPlanArc->GetId(), mCurrentPlanArc->GetType(), mCurrentPlanArc->GetWeight(),
				mCurrentPlanArc->GetNode()->GetId());
			*/
			if (!mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlayerData.plan.node = mCurrentPlanArc->GetNode();
				mCurrentPlayerData.plan.path.erase(mCurrentPlayerData.plan.path.begin());
			}
			if (mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlanAction = -1;
				mCurrentPlanArc = NULL;
				mCurrentPlayerData.plan.id = -1;
				mCurrentPlayerData.plan.node = NULL;
				break;
			}
			else
			{
				mCurrentPlanArc = *mCurrentPlayerData.plan.path.begin();
				mCurrentPlanAction = mCurrentPlanArc->GetType();
			}
		} while (!foundActionPlan);
	}
	else
	{
		bool foundActionPlan = false;
		do
		{
			if (mCurrentPlanAction == actionType)
				foundActionPlan = true;

			if (!mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlanArc = *mCurrentPlayerData.plan.path.begin();
				mCurrentPlayerData.plan.node = mCurrentPlanArc->GetNode();
				mCurrentPlayerData.plan.path.erase(mCurrentPlayerData.plan.path.begin());
			}
			if (mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlanAction = -1;
				mCurrentPlanArc = NULL;
				mCurrentPlayerData.plan.id = -1;
				mCurrentPlayerData.plan.node = NULL;
				break;
			}
			else
			{
				mCurrentPlanArc = *mCurrentPlayerData.plan.path.begin();
				mCurrentPlanAction = mCurrentPlanArc->GetType();
			}
		} while (!foundActionPlan);
	}

	return true;
}

bool QuakeAIView::UpdateActionPlan(const Vector3<float>& currentPosition, float nodeMargin)
{
	bool updatedActionPlan = false;

	if (mCurrentPlanAction == -1 || mCurrentPlanAction == AT_PUSH || mCurrentPlanAction == AT_TELEPORT)
		return updatedActionPlan;

	if (mCurrentPlanArc)
	{
		Vector3<float> fromNode = mCurrentPlanArc->GetNode()->GetPosition() - mCurrentPlayerData.plan.node->GetPosition();
		Vector3<float> toNode = mCurrentPlanArc->GetNode()->GetPosition() - currentPosition;
		if (mCurrentPlanArc->GetNode()->GetActorId() != INVALID_ACTOR_ID)
		{
			if (Dot(fromNode, toNode) < 0)
			{
				updatedActionPlan = true;
				/*
				printf("\nactor id %u from node %f %f %f to node %f %f %f dot %f length to node %f",
					mCurrentPlanArc->GetNode()->GetActorId(), fromNode[0], fromNode[1], fromNode[2],
					toNode[0], toNode[1], toNode[2], Dot(fromNode, toNode), Length(toNode));
				*/
			}
		}
		else if (Length(toNode) <= nodeMargin || Dot(fromNode, toNode) < 0)
		{
			updatedActionPlan = true;
			/*
			printf("\nactor id %u from node %f %f %f to node %f %f %f dot %f length to node %f",
				mCurrentPlanArc->GetNode()->GetActorId(), fromNode[0], fromNode[1], fromNode[2],
				toNode[0], toNode[1], toNode[2], Dot(fromNode, toNode), Length(toNode));
			*/
		}

		if (updatedActionPlan)
		{
			/*
			printf("\nplayer id %u arc id %u type %u weight %f node %u pos %f %f %f diff %f", 
				mPlayerId, mCurrentPlanArc->GetId(), mCurrentPlanArc->GetType(), mCurrentPlanArc->GetWeight(),
				mCurrentPlanArc->GetNode()->GetId(), currentPosition[0], currentPosition[1], currentPosition[2], Length(toNode));
			*/
			if (!mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlayerData.plan.node = mCurrentPlanArc->GetNode();
				mCurrentPlayerData.plan.path.erase(mCurrentPlayerData.plan.path.begin());
			}

			if (mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlanAction = -1;
				mCurrentPlanArc = NULL;
				mCurrentPlayerData.plan.id = -1;
				mCurrentPlayerData.plan.node = NULL;
				updatedActionPlan = false;
			}
			else
			{
				mCurrentPlanArc = *mCurrentPlayerData.plan.path.begin();
				mCurrentPlanAction = mCurrentPlanArc->GetType();
				if (mCurrentPlanAction == AT_PUSH || mCurrentPlanAction == AT_TELEPORT)
					updatedActionPlan = false;
			}
		}
	}
	else
	{
		Vector3<float> toNode = mCurrentPlayerData.plan.node->GetPosition() - currentPosition;
		if (Length(toNode) <= nodeMargin * 4.f)
		{
			if (!mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlanArc = *mCurrentPlayerData.plan.path.begin();
				mCurrentPlayerData.plan.node = mCurrentPlanArc->GetNode();
				mCurrentPlayerData.plan.path.erase(mCurrentPlayerData.plan.path.begin());
			}
			if (mCurrentPlayerData.plan.path.empty())
			{
				mCurrentPlanAction = -1;
				mCurrentPlanArc = NULL;
				mCurrentPlayerData.plan.id = -1;
				mCurrentPlayerData.plan.node = NULL;
				updatedActionPlan = false;
			}
			else
			{
				mCurrentPlanArc = *mCurrentPlayerData.plan.path.begin();
				mCurrentPlanAction = mCurrentPlanArc->GetType();
				updatedActionPlan = true;
			}
		}
	}

	return updatedActionPlan;
}

bool QuakeAIView::CanUpdateActionPlan(const PlayerData& player)
{
	QuakeLogic* gameLogic = dynamic_cast<QuakeLogic*>(GameLogic::Get());
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());

	if (player.plan.path.size() && player.plan.id != mCurrentPlayerData.plan.id)
	{
		std::shared_ptr<PlayerActor> pPlayerActor(
			std::dynamic_pointer_cast<PlayerActor>(gameLogic->GetActor(player.player).lock()));

		float currentPathWeight = -aiManager->CalculatePathWeight(mCurrentPlayerData);
		if (mCurrentPlanArc && mCurrentPlanArc->GetType() != AT_MOVE)
			currentPathWeight = -mCurrentPlanArc->GetWeight();
		for (auto& pathingArc : mCurrentPlayerData.plan.path)
		{
			if (currentPathWeight + pathingArc->GetWeight() > 0.3f)
				break;
		
			currentPathWeight += pathingArc->GetWeight();
			if (pathingArc->GetNode()->GetActorId() != INVALID_ACTOR_ID)
			{
				ActorId itemId = pathingArc->GetNode()->GetActorId();
				const AIAnalysis::ActorPickup* itemPickup = aiManager->GetGameActorPickup(itemId);
				if (itemPickup)
				{
					std::shared_ptr<Actor> pItemActor(std::dynamic_pointer_cast<Actor>(gameLogic->GetActor(itemId).lock()));
					if (gameLogic->CanItemBeGrabbed(pItemActor, pPlayerActor))
					{
						if (aiManager->CalculateHeuristicItem(player, itemId, currentPathWeight) >= 0.1f)
						{
							//if it is an item in the planning we will update to the new plan
							if (player.items.find(itemId) != player.items.end())
								return true;

							//it the item has any value we keep with the current plan and ignore the new one
							std::stringstream pathInfo;
							pathInfo << "\n IGNORE NEW plan " << player.plan.id << " frame " << aiManager->GetFrame() << 
								" player " << player.player << " found close item " << itemId;
							aiManager->PrintInfo(pathInfo.str());
							return false;
						}
					}
				}
			}
		}
		return true;
	}
	else return false;

}

bool QuakeAIView::UpdateActionPlan(bool findPath)
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());

	bool updatedActionPlan = false;

	PlayerView playerView;
	aiManager->GetPlayerView(mPlayerId, playerView);
	if (playerView.isUpdated)
	{
		if (CanUpdateActionPlan(playerView.simulation))
		{
			//lets search for closest nodes (around 1 sec of the new path plan)
			PathingNodeVec searchNodes;

			//find the current node and calculate its weight
			PathingArcVec path;
			float currentWeight = 0.f;
			PathingNode* currentNode = mCurrentPlayerData.plan.node;
			if (mCurrentPlanArc && mCurrentPlanArc->GetNode() != currentNode)
			{
				//currentWeight = mCurrentPlanArc->GetWeight();
				currentNode = mCurrentPlanArc->GetNode();
				path.push_back(mCurrentPlanArc);
			}

			if (currentNode != playerView.simulation.plan.node)
			{
				path.clear();
				currentWeight = -aiManager->CalculatePathWeight(mCurrentPlayerData);
				PathingArcVec::iterator itArc = mCurrentPlayerData.plan.path.begin();
				for (; itArc != mCurrentPlayerData.plan.path.end(); itArc++)
				{
					path.push_back((*itArc));
					if ((*itArc)->GetNode() == playerView.simulation.plan.node)
					{
						updatedActionPlan = true;
						break;
					}

					if (currentWeight + (*itArc)->GetWeight() > 1.f)
						break;

					currentWeight += (*itArc)->GetWeight();
				}
			}
			else updatedActionPlan = true;

			if (updatedActionPlan)
			{
				PathingArcVec::iterator itArc = playerView.simulation.plan.path.begin();
				for (; itArc != playerView.simulation.plan.path.end(); itArc++)
					path.push_back((*itArc));

				std::stringstream pathInfo;
				pathInfo << "\n MERGE PREV path frame " << aiManager->GetFrame() << " player " <<
					mPlayerId << " node " << mCurrentPlayerData.plan.node->GetId() << " path";
				for (PathingArc* pathArc : mCurrentPlayerData.plan.path)
					pathInfo << " " << pathArc->GetId();
				pathInfo << "\n MERGE NEW path frame " << aiManager->GetFrame() << " player " <<
					mPlayerId << " node " << mCurrentPlayerData.plan.node->GetId() << " path";
				for (PathingArc* pathArc : path)
					pathInfo << " " << pathArc->GetId();
				aiManager->PrintInfo(pathInfo.str());

				mCurrentPlayerData.plan.id = playerView.simulation.plan.id;
				mCurrentPlayerData.plan.path = path;
				mGoalNode = NULL;

				if (!mCurrentPlanArc)
				{
					mCurrentPlanArc = mCurrentPlayerData.plan.path.size() ? *mCurrentPlayerData.plan.path.begin() : NULL;
					mCurrentPlanAction = mCurrentPlanArc ? mCurrentPlanArc->GetType() : -1;
				}

				UpdatePlayerItems(playerView.simulation);
				UpdatePlayerWeapon(playerView);

				return updatedActionPlan;
			}
			else
			{
				path.clear();
				currentWeight = 0.f;
				currentNode = mCurrentPlayerData.plan.node;
				if (mCurrentPlanArc && mCurrentPlanArc->GetNode() != currentNode)
				{
					//currentWeight = mCurrentPlanArc->GetWeight();
					currentNode = mCurrentPlanArc->GetNode();
					path.push_back(mCurrentPlanArc);
				}

				PathingArcVec::iterator itArc = playerView.simulation.plan.path.begin();
				PathingArcVec::iterator itPathArc = playerView.simulation.plan.path.begin();
				if (currentNode != playerView.simulation.plan.node)
				{
					for (; itArc != playerView.simulation.plan.path.end(); itArc++)
					{
						if ((*itArc)->GetNode() == currentNode)
						{
							updatedActionPlan = true;
							itPathArc = std::next(itArc);
							break;
						}

						if (currentWeight + (*itArc)->GetWeight() > 1.f)
							break;

						currentWeight += (*itArc)->GetWeight();
						searchNodes.push_back((*itArc)->GetNode());

						//if it is an item in the planning we need to include it
						ActorId itemId = (*itArc)->GetNode()->GetActorId();
						if (playerView.simulation.itemAmount.find(itemId) != playerView.simulation.itemAmount.end())
							break;
					}
				}
				else updatedActionPlan = true;

				if (updatedActionPlan)
				{
					for (; itPathArc != playerView.simulation.plan.path.end(); itPathArc++)
						path.push_back((*itPathArc));

					std::stringstream pathInfo;
					pathInfo << "\n MERGE PREV path frame " << aiManager->GetFrame() << " player " <<
						mPlayerId << " node " << mCurrentPlayerData.plan.node->GetId() << " path";
					for (PathingArc* pathArc : mCurrentPlayerData.plan.path)
						pathInfo << " " << pathArc->GetId();
					pathInfo << "\n MERGE NEW path frame " << aiManager->GetFrame() << " player " <<
						mPlayerId << " node " << mCurrentPlayerData.plan.node->GetId() << " path";
					for (PathingArc* pathArc : path)
						pathInfo << " " << pathArc->GetId();
					aiManager->PrintInfo(pathInfo.str());

					mCurrentPlayerData.plan.id = playerView.simulation.plan.id;
					mCurrentPlayerData.plan.path = path;
					mGoalNode = NULL;

					if (!mCurrentPlanArc)
					{
						mCurrentPlanArc = mCurrentPlayerData.plan.path.size() ? *mCurrentPlayerData.plan.path.begin() : NULL;
						mCurrentPlanAction = mCurrentPlanArc ? mCurrentPlanArc->GetType() : -1;
					}

					UpdatePlayerItems(playerView.simulation);
					UpdatePlayerWeapon(playerView);

					return updatedActionPlan;
				}
				else if (findPath)
				{
					path.clear();
					currentNode = mCurrentPlayerData.plan.node;
					if (mCurrentPlanArc && mCurrentPlanArc->GetNode() != currentNode)
					{
						currentNode = mCurrentPlanArc->GetNode();
						path = { mCurrentPlanArc };
					}
				}
				else
				{
					std::stringstream pathInfo;
					pathInfo << "\n NOT FOUND path frame " << aiManager->GetFrame() << " player " << mPlayerId;
					aiManager->PrintInfo(pathInfo.str());
					
					return updatedActionPlan;
				}
			}

			float searchThreshold = 1.0f;
			PathPlan* plan = aiManager->GetPathingGraph()->FindPath(currentNode, searchNodes, AT_JUMP, searchThreshold);
			if (plan)
			{
				plan->ResetPath();
				if (!plan->CheckForEnd())
				{
					PathingNode* node = plan->GetArcs().back()->GetNode();
					for (PathingArc* planArc : plan->GetArcs())
						path.push_back(planArc);

					PathingArcVec::iterator itArc = playerView.simulation.plan.path.begin();
					PathingArcVec::iterator itPathArc = playerView.simulation.plan.path.begin();
					for (; itArc != playerView.simulation.plan.path.end(); itArc++)
					{
						if ((*itArc)->GetNode() == node)
						{
							itPathArc = std::next(itArc);
							break;
						}
					}

					for (; itPathArc != playerView.simulation.plan.path.end(); itPathArc++)
						path.push_back((*itPathArc));

					std::stringstream pathInfo;
					pathInfo << "\n FOUND PREV PLAN path frame " << aiManager->GetFrame() << " player " <<
						mPlayerId << " node " << mCurrentPlayerData.plan.node->GetId() << " path";
					for (PathingArc* pathArc : mCurrentPlayerData.plan.path)
						pathInfo << " " << pathArc->GetId();
					pathInfo << "\n FOUND NEW PLAN path frame " << aiManager->GetFrame() << " player " <<
						mPlayerId << " node " << mCurrentPlayerData.plan.node->GetId() << " path";
					for (PathingArc* pathArc : path)
						pathInfo << " " << pathArc->GetId();
					aiManager->PrintInfo(pathInfo.str());

					updatedActionPlan = true;
					mCurrentPlayerData.plan.id = playerView.simulation.plan.id;
					mCurrentPlayerData.plan.path = path;

					UpdatePlayerItems(playerView.simulation);
				}
				else
				{
					std::stringstream pathInfo;
					pathInfo << "\n NOT FOUND PLAN path frame " << aiManager->GetFrame() << " player " << 
						mPlayerId << " node " << currentNode->GetId() << " keep same old path; search nodes";
					for (PathingNode* searchNode : searchNodes)
						pathInfo << " " << searchNode->GetId();
					aiManager->PrintInfo(pathInfo.str());
				}

				mGoalNode = NULL;

				if (!mCurrentPlanArc)
				{
					mCurrentPlanArc = mCurrentPlayerData.plan.path.size() ? *mCurrentPlayerData.plan.path.begin() : NULL;
					mCurrentPlanAction = mCurrentPlanArc ? mCurrentPlanArc->GetType() : -1;
				}

				delete plan;
			}
			else
			{
				std::stringstream pathInfo;
				pathInfo << "\n NOT FOUND PLAN path frame " << aiManager->GetFrame() << " player " <<
					mPlayerId << " node " << currentNode->GetId() << "; search nodes";
				for (PathingNode* searchNode : searchNodes)
					pathInfo << " " << searchNode->GetId();
				aiManager->PrintInfo(pathInfo.str());
			}
		}
		else updatedActionPlan = true;
	}

	if (playerView.simulation.plan.id == mCurrentPlayerData.plan.id)
		UpdatePlayerWeapon(playerView);

	return updatedActionPlan;
}

//  class QuakeAIView::OnUpdate			- Chapter 10, page 283
void QuakeAIView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	return;
	if (!mEnabled)
		return;

	mYawSmoothTime += deltaMs / 1000.f;

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mPlayerId).lock()));
	if (!pPlayerActor) 
		return;

	QuakeAIManager* aiManager =
		dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	if (pPlayerActor->GetState().moveType != PM_DEAD)
	{
		mCurrentPlayerData.Update(pPlayerActor);

		if (pPlayerActor->GetAction().triggerTeleporter != INVALID_ACTOR_ID)
		{
			UpdateActionPlan(AT_TELEPORT);

			std::shared_ptr<Actor> pItemActor(std::dynamic_pointer_cast<Actor>(
				GameLogic::Get()->GetActor(pPlayerActor->GetAction().triggerTeleporter).lock()));
			std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
				pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();

			AxisAngle<4, float> localRotation;
			pTeleporterTrigger->GetTarget().GetRotation(localRotation);
			mYaw = mYawSmooth = localRotation.mAngle * localRotation.mAxis[AXIS_Y] * (float)GE_C_RAD_TO_DEG;
			//mPitchTarget = -yawPitchRoll.mAngle[AXIS_Z] * (float)GE_C_RAD_TO_DEG;

			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataTeleportActor>(mPlayerId));
			return;
		}

		std::shared_ptr<TransformComponent> pTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		std::shared_ptr<PhysicComponent> pPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (pTransformComponent && pPhysicComponent)
		{
			pPlayerActor->GetAction().actionType = ACTION_STAND;

			bool updatedActionPlan = false;

			Vector3<float> fall = mGravity;
			Vector3<float> velocity = Vector3<float>::Zero();
			if (pPhysicComponent->OnGround())
			{
				mFallSpeed = mMaxFallSpeed;

				if (pPlayerActor->GetAction().triggerPush != INVALID_ACTOR_ID)
				{
					UpdateActionPlan(AT_PUSH);

					mFallSpeed = Vector3<float>{
						PUSHTRIGGER_FALL_SPEED_XZ, PUSHTRIGGER_FALL_SPEED_XZ, PUSHTRIGGER_FALL_SPEED_Y };

					std::shared_ptr<Actor> pItemActor(std::dynamic_pointer_cast<Actor>(
						GameLogic::Get()->GetActor(pPlayerActor->GetAction().triggerPush).lock()));
					std::shared_ptr<PushTrigger> pPushTrigger =
						pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock();

					Vector3<float> targetPosition = pPushTrigger->GetTarget().GetTranslation();
					Vector3<float> playerPosition = pPhysicComponent->GetTransform().GetTranslation();
					Vector3<float> direction = targetPosition - playerPosition;
					float push = mPushSpeed[AXIS_Y] + direction[AXIS_Y] * 0.01f;
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
				else
				{
					mPitchTarget = std::max(-85.f, std::min(85.f, mPitchTarget));
					mPitch = 90 * ((mPitchTarget + 85.f) / 170.f) - 45.f;

					if (mPathingGraph)
					{
						Vector3<float> currentPosition = pPhysicComponent->GetTransform().GetTranslation();
						if (mCurrentPlayerData.plan.node == NULL)
							mCurrentPlayerData.plan.node = mPathingGraph->FindClosestNode(currentPosition);

						updatedActionPlan = UpdateActionPlan(true);

						if (mCurrentPlayerData.plan.path.empty())
						{
							if (mBehavior == BT_PATROL)
							{
								PathingNode* currentNode = mPathingGraph->FindClosestNode(currentPosition);
								if (mGoalNode == NULL || mGoalNode == currentNode)
								{
									//printf("\n random node %u : ", mPlayerId);
									PathingClusterVec clusterNodes;
									currentNode->GetClusters(AT_MOVE, clusterNodes);

									// choose a random cluster
									do
									{
										if (!clusterNodes.empty())
										{
											unsigned int cluster = Randomizer::Rand() % clusterNodes.size();
											mGoalNode = clusterNodes[cluster]->GetTarget();
										}
										else
										{
											mGoalNode = NULL;
											break;
										}
									} while (currentNode == mGoalNode || mGoalNode->GetArcs().empty());
								}

								if (mGoalNode != NULL)
								{
									float minPosDiff = FLT_MAX;
									PathingCluster* currentCluster = currentNode->FindCluster(AT_MOVE, mGoalNode);
									if (currentCluster != NULL)
									{
										PathingArc* clusterArc = currentNode->FindArc(currentCluster->GetNode());
										PathingNode* clusterNode = clusterArc->GetNode();
										unsigned int clusterArcType = clusterArc->GetType();

										mCurrentPlanArc = clusterArc;
										mCurrentPlanAction = clusterArcType;
										if (mCurrentPlanAction != AT_PUSH && mCurrentPlanAction != AT_TELEPORT)
											mCurrentPlayerData.plan.node = clusterArc->GetNode();

										Vector3<float> direction = clusterNode->GetPosition() - currentPosition;
										Normalize(direction);
										mYaw = atan2(direction[1], direction[0]) * (float)GE_C_RAD_TO_DEG;

										//printf("\n new plan %u : ", mPlayerId);
										do
										{
											currentCluster = currentNode->FindCluster(AT_MOVE, mGoalNode);
											clusterArc = currentNode->FindArc(currentCluster->GetNode());
											currentNode = clusterArc->GetNode();
											//printf("%u ", currentNode->GetId());
										} while (currentNode != mGoalNode);
									}
									else ResetActionPlan();
								}
								else
								{
									Stationary(deltaMs);
									Smooth(deltaMs);
									Cliff();
								}

								pPlayerActor->GetAction().actionType |= ACTION_RUN;
								pPlayerActor->GetAction().actionType |= ACTION_MOVEFORWARD;
							}
							else
							{
								mYawSmooth = mYaw;
							}
						}
						else
						{
							Vector3<float> direction;
							if (mCurrentPlanAction == AT_JUMP || mCurrentPlanAction == AT_PUSH || mCurrentPlanAction == AT_TELEPORT)
							{
								if (mCurrentPlanAction == AT_JUMP)
								{
									pPlayerActor->GetAction().actionType |= ACTION_JUMP;
									mCurrentPlanAction = AT_MOVE;

									if (mCurrentPlanArc)
										direction = mCurrentPlanArc->GetNode()->GetPosition() - currentPosition;
									else
										direction = mCurrentPlayerData.plan.node->GetPosition() - currentPosition;
								}
								else
								{
									pPlayerActor->GetAction().actionType |= ACTION_RUN;
									pPlayerActor->GetAction().actionType |= ACTION_MOVEFORWARD;

									direction = mCurrentPlayerData.plan.node->GetPosition() - currentPosition;
								}
								/*
								printf("\n diff %f time %f pos %f %f %f", Length(direction), deltaMs / 1000.f,
									currentPosition[0], currentPosition[1], currentPosition[2]);
								*/
								Normalize(direction);
								mYaw = atan2(direction[1], direction[0]) * (float)GE_C_RAD_TO_DEG;
							}
							else if (mCurrentPlanArc)
							{
								if (mCurrentPlanArc)
									direction = mCurrentPlanArc->GetNode()->GetPosition() - currentPosition;
								else
									direction = mCurrentPlayerData.plan.node->GetPosition() - currentPosition;
								/*
								printf("\n diff %f time %f pos %f %f %f", Length(direction), deltaMs / 1000.f,
									currentPosition[0], currentPosition[1], currentPosition[2]);
								*/
								Normalize(direction);
								mYaw = atan2(direction[1], direction[0]) * (float)GE_C_RAD_TO_DEG;

								if (Length(mStationaryPosition - currentPosition) < 5.f)
								{
									mStationaryTime += deltaMs;
									if (mStationaryTime > 150)
									{
										std::stringstream stuckInfo;
										stuckInfo << "\n STUCK frame " << aiManager->GetFrame() << " player " << mPlayerId;
										aiManager->PrintInfo(stuckInfo.str());
										//printf("\n Player %u got stuck", mPlayerId);

										mCurrentPlanArc = NULL;
										mCurrentPlayerData.plan.id = -1;
										mCurrentPlayerData.plan.path.clear();
										mCurrentPlayerData.plan.node = mPathingGraph->FindClosestNode(currentPosition);

										mStationaryTime = 0;
									}
								}
								else mStationaryTime = 0;
								mStationaryPosition = currentPosition;

								pPlayerActor->GetAction().actionType |= ACTION_RUN;
								pPlayerActor->GetAction().actionType |= ACTION_MOVEFORWARD;
							}
						}
					}
					else if (mBehavior == BT_PATROL)
					{
						Stationary(deltaMs);
						Smooth(deltaMs);
						Cliff();

						pPlayerActor->GetAction().actionType |= ACTION_RUN;
						pPlayerActor->GetAction().actionType |= ACTION_MOVEFORWARD;
					}

					// Calculate the new rotation matrix from the camera
					// yaw and pitch (zrotate and xrotate).
					Matrix4x4<float> yawRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
					Matrix4x4<float> rotation = yawRotation;
					Matrix4x4<float> pitchRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitch * (float)GE_C_DEG_TO_RAD));

					//smoothing rotation
					if (abs(mYawSmooth - mYaw) < 90)
					{
						if (mYaw - mYawSmooth > 180)
							mYawSmooth--;
						else if (mYaw - mYawSmooth < -180)
							mYawSmooth++;
						else if (mYaw > mYawSmooth)
							mYawSmooth++;
						else if (mYaw < mYawSmooth)
							mYawSmooth--;
					}
					else if (mYawSmoothTime >= 0.5f)
					{
						mYawSmooth = mYaw;
						mYawSmoothTime = 0.f;
					}

					yawRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYawSmooth * (float)GE_C_DEG_TO_RAD));
					mAbsoluteTransform.SetRotation(yawRotation * pitchRotation);
					mAbsoluteTransform.SetTranslation(pTransformComponent->GetPosition());

					// This will give us the "look at" vector 
					// in world space - we'll use that to move.
					Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
					atWorld = rotation * atWorld;
#else
					atWorld = atWorld * rotation;
#endif

					if (pPlayerActor->GetAction().actionType & ACTION_JUMP)
					{
						Vector4<float> direction = atWorld;
						direction[AXIS_Y] = 0;
						Normalize(direction);

						velocity[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
						velocity[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
						velocity[AXIS_Y] = mJumpSpeed[AXIS_Y];

						fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
						fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
						fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

					}
					else if ((pPlayerActor->GetAction().actionType & ACTION_RUN) ||
							(pPlayerActor->GetAction().actionType == ACTION_STAND))
					{
						if (pPlayerActor->GetAction().actionType & ACTION_RUN)
						{
							Vector4<float> direction = atWorld;
							direction[AXIS_Y] = 0;
							Normalize(direction);

							velocity = HProject(direction);
							velocity *= mMoveSpeed;
						}

						fall = mGravity;
						
						//neutral position
						mPitchTarget = 0;
						if (!pPlayerActor->IsChangingWeapon() && mCurrentPlayerData.target != INVALID_ACTOR_ID)
						{
							std::shared_ptr<PlayerActor> pPlayerTarget(
								std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mCurrentPlayerData.target).lock()));

							if (pPlayerTarget->GetState().stats[STAT_HEALTH] > 0)
							{
								//set muzzle location relative to pivoting eye
								Vector3<float> playerPos = pPhysicComponent->GetTransform().GetTranslation();
								playerPos += Vector3<float>::Unit(AXIS_Y) * (float)pPlayerActor->GetState().viewHeight;

								std::shared_ptr<PhysicComponent> pTargetPhysicComponent(
									pPlayerTarget->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
								Vector3<float> targetPos = pTargetPhysicComponent->GetTransform().GetTranslation();
								//targetPos += Vector3<float>::Unit(AXIS_Y) * (float)pPlayerTarget->GetState().viewHeight;

								std::vector<ActorId> collisionActors;
								std::vector<Vector3<float>> collisions, collisionNormals;
								GameLogic::Get()->GetGamePhysics()->CastRay(
									playerPos, targetPos, collisionActors, collisions, collisionNormals);

								ActorId closestCollisionId = INVALID_ACTOR_ID;
								std::optional<Vector3<float>> closestCollision = std::nullopt;
								for (unsigned int i = 0; i < collisionActors.size(); i++)
								{
									if (collisionActors[i] != pPlayerActor->GetId())
									{
										if (closestCollision.has_value())
										{
											if (Length(closestCollision.value() - playerPos) > Length(collisions[i] - playerPos))
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

								if (closestCollisionId == pPlayerTarget->GetId())
								{
									if (pPlayerActor->GetState().weapon == WP_ROCKET_LAUNCHER)
									{
										PlayerView playerView;
										aiManager->GetPlayerView(pPlayerActor->GetId(), playerView);
										PlayerGuessView playerGuessView = playerView.guessViews[pPlayerTarget->GetId()];
										if (playerGuessView.data.plan.path.size())
										{
											PathingArc* playerTargetArc = *playerGuessView.data.plan.path.begin();
											targetPos = playerTargetArc->GetNode()->GetPosition();
										}
										targetPos -= Vector3<float>::Unit(AXIS_Y) * (float)pPlayerTarget->GetState().viewHeight / 2.f;

										Transform start;
										start.SetTranslation(playerPos);
										Transform end;
										end.SetTranslation(targetPos);

										std::vector<ActorId> collisionActors;
										std::vector<Vector3<float>> collisions, collisionNormals;
										std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();
										gamePhysics->ConvexSweep(
											mProjectileActor->GetId(), start, end, collisionActors, collisions, collisionNormals);
										
										closestCollision = targetPos;
										for (unsigned int i = 0; i < collisionActors.size(); i++)
										{
											if (collisionActors[i] != pPlayerActor->GetId() && 
												collisionActors[i] != pPlayerTarget->GetId())
											{
												closestCollisionId = INVALID_ACTOR_ID;
												break;
											}
										}
									}
									else if (pPlayerActor->GetState().weapon == WP_RAILGUN || pPlayerActor->GetState().weapon == WP_SHOTGUN)
									{
										Transform start;
										start.SetTranslation(playerPos);
										Transform end;
										end.SetTranslation(targetPos);

										std::vector<ActorId> collisionActors;
										std::vector<Vector3<float>> collisions, collisionNormals;
										std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();
										gamePhysics->ConvexSweep(
											mProjectileActor->GetId(), start, end, collisionActors, collisions, collisionNormals);

										closestCollision = targetPos;
										for (unsigned int i = 0; i < collisionActors.size(); i++)
										{
											if (collisionActors[i] != pPlayerActor->GetId() && 
												collisionActors[i] != pPlayerTarget->GetId())
											{
												closestCollisionId = INVALID_ACTOR_ID;
												break;
											}
										}
									}
								}

								if (closestCollisionId == pPlayerTarget->GetId())
								{
									Vector3<float> direction = closestCollision.value() - playerPos;
									float scale = Length(direction);
									Normalize(direction);

									mYaw = mYawSmooth = atan2(direction[AXIS_Z], direction[AXIS_X]) * (float)GE_C_RAD_TO_DEG;
									mPitchTarget = -asin(direction[AXIS_Y]) * (float)GE_C_RAD_TO_DEG;

									mPitchTarget = std::max(-85.f, std::min(85.f, mPitchTarget));
									mPitch = 90 * ((mPitchTarget + 85.f) / 170.f) - 45.f;

									yawRotation = Rotation<4, float>(
										AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
									pitchRotation = Rotation<4, float>(
										AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitch * (float)GE_C_DEG_TO_RAD));
									mAbsoluteTransform.SetRotation(yawRotation * pitchRotation);

									pPlayerActor->GetAction().actionType |= ACTION_ATTACK;

									std::stringstream weaponInfo;
									weaponInfo << "\n FIRING WEAPON frame " << 
										aiManager->GetFrame() << " player " << mPlayerId << 
										" current weapon " << pPlayerActor->GetState().weapon <<
										" weapon state " << pPlayerActor->GetState().weaponState << 
										" weapon time " << pPlayerActor->GetState().weaponTime;
									aiManager->PrintInfo(weaponInfo.str());
								}
							}
						}
					}

					// update node rotation matrix
					pitchRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitchTarget * (float)GE_C_DEG_TO_RAD));
					Matrix4x4<float> rollRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
					pTransformComponent->SetRotation(yawRotation * pitchRotation * rollRotation);
				}
				pPlayerActor->GetAction().actionType |= ACTION_RUN;
			}
			else
			{
				if (mCurrentPlayerData.plan.node != NULL)
					updatedActionPlan = UpdateActionPlan(true);

				// Calculate the new rotation matrix from the camera
				// yaw and pitch (zrotate and xrotate).
				Matrix4x4<float> yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
				Matrix4x4<float> rotation = yawRotation;
				Matrix4x4<float> pitchRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitch * (float)GE_C_DEG_TO_RAD));

				// This will give us the "look at" vector 
				// in world space - we'll use that to move.
				Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
				atWorld = rotation * atWorld;
#else
				atWorld = atWorld * rotation;
#endif
				Vector4<float> direction = atWorld;
				direction[AXIS_Y] = 0;
				Normalize(direction);

				direction[AXIS_X] *= mFallSpeed[AXIS_X];
				direction[AXIS_Z] *= mFallSpeed[AXIS_Z];
				direction[AXIS_Y] = -mFallSpeed[AXIS_Y];
				velocity = HProject(direction);
				fall = HProject(direction);

				//neutral position
				mPitchTarget = 0;

				// update node rotation matrix
				yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYawSmooth * (float)GE_C_DEG_TO_RAD));
				pitchRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), mPitchTarget * (float)GE_C_DEG_TO_RAD));
				Matrix4x4<float> rollRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
				pTransformComponent->SetRotation(yawRotation * pitchRotation * rollRotation);

				pPlayerActor->GetAction().actionType |= ACTION_FALL;
			}

			QuakeAIManager* aiManager =
				dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
			if (aiManager)
			{
				if (updatedActionPlan)
					aiManager->UpdatePlayerView(mPlayerId, mCurrentPlayerData, false);
				else
					aiManager->UpdatePlayerView(mPlayerId, mCurrentPlayerData);
			}

			mRespawnTimeMs = 0;
			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataRotateActor>(mPlayerId, mYawSmooth, mPitch));

			pPlayerActor->UpdateTimers(deltaMs);
			pPlayerActor->UpdateWeapon(deltaMs);
			pPlayerActor->UpdateMovement(velocity, fall);
		}
	}
	else
	{
		aiManager->SavePlayerView(pPlayerActor->GetId(), PlayerView());

		ResetActionPlan();

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
			pPlayerActor->UpdateMovement(Vector3<float>::Zero(), mGravity);
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
	}
}