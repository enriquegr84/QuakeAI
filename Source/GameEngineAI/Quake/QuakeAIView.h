/*******************************************************
 * QuakeAIView.h : AI Controller class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#ifndef QUAKEAIVIEW_H
#define QUAKEAIVIEW_H

#include "QuakeStd.h"

#include "Core/Event/EventManager.h"

#include "Game/View/GameView.h"

#include "AI/Pathing.h"

#include "Mathematic/Algebra/Transform.h"

enum BehaviorType
{
	BT_STAND,
	BT_PATROL
};

class QuakeAIView : public BaseGameView 
{

public:
	QuakeAIView();
	virtual ~QuakeAIView();

	virtual bool OnRestore() { return true; }
	virtual void OnRender(double time, float elapsedTime) {}
	virtual bool OnLostDevice() { return true; }
	virtual GameViewType GetType() { return GV_AI; }
	virtual GameViewId GetId() const { return mViewId; }
	virtual ActorId GetActorId() const { return mPlayerId; }
	virtual void OnAttach(GameViewId vid, ActorId actorId);
	virtual bool OnMsgProc(const Event& evt) {	return false; }
	virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs);

	ActorId GetProjectileId() const { return mProjectileActor->GetId(); }

	void PlayerSpawn(const Transform& spawnTransform);

	void ResetActionPlan();

	void UpdatePlayerWeapon(const PlayerView& playerView);
	void UpdatePlayerItems(const PlayerData& player);

	bool UpdateActionPlan(bool findPath);
	bool UpdateActionPlan(int actionType);
	bool UpdateActionPlan(const Vector3<float>& currentPosition, float nodeMargin);
	bool CanUpdateActionPlan(const PlayerData& playerView);

	void SetActionPlanType(unsigned int actionPlanType) { mCurrentPlanAction = actionPlanType; }
	int GetActionPlanType() { return mCurrentPlanAction; }
	PathingArc* GetActionPlanArc() { return mCurrentPlanArc; }
	PathingNode* GetActionPlanNode() { return mCurrentPlayerData.plan.node; }
	const PlayerData& GetActionPlayer() { return mCurrentPlayerData; }

	void SetEnabled(bool enabled) { mEnabled = enabled; }
	bool GetEnabled() const { return mEnabled; }

	BehaviorType GetBehavior() { return mBehavior; }
	void SetBehavior(BehaviorType behavior) { mBehavior = behavior; }

	void SetYaw(float yaw, bool synchYaw = true) { mYaw = yaw; if (synchYaw)mYawSmooth = yaw; }

	void SetPathingGraph(const std::shared_ptr<PathingGraph>& pathingGraph) { mPathingGraph = pathingGraph; }
	const std::shared_ptr<PathingGraph>& GetPathingGraph() { return mPathingGraph; }

protected:

	bool IsAvailableWeapon(std::shared_ptr<PlayerActor> pPlayerActor, WeaponType weapon);
	bool IsOptimalWeapon(std::shared_ptr<PlayerActor> pPlayerActor, WeaponType weapon, float shootingDistance);

	void Stationary(unsigned long deltaMs);
	void Avoidance(unsigned long deltaMs);
	void Smooth(unsigned long deltaMs);
	void Cliff();

	bool mEnabled;

	// Behavior
	BehaviorType mBehavior;

	int mOrientation;
	unsigned long mStationaryTime;
	Vector3<float> mStationaryPosition;

	// Orientation Controls
	float mYaw;
	float mYawSmooth;
	float mYawSmoothTime;
	float mPitch;
	float mPitchTarget;

	// Speed Controls
	Vector3<float> mMaxPushSpeed;
	Vector3<float> mMaxJumpSpeed;
	Vector3<float> mMaxFallSpeed;
	float mMaxMoveSpeed;

	Vector3<float> mPushSpeed;
	Vector3<float> mJumpSpeed;
	Vector3<float> mFallSpeed;
	float mMoveSpeed;

	Vector3<float> mGravity;

	unsigned long mRespawnTimeMs;

	GameViewId mViewId;
	ActorId mPlayerId;

	Transform mAbsoluteTransform;

private:

	PathingNode* mGoalNode;

	int mCurrentPlanAction;
	PathingArc* mCurrentPlanArc;
	PlayerData mCurrentPlayerData;

	std::shared_ptr<Actor> mProjectileActor;

	std::shared_ptr<PathingGraph> mPathingGraph;
};

#endif