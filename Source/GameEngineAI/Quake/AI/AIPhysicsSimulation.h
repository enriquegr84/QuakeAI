/*******************************************************
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#ifndef AIPHYSICSSIMULATION_H
#define AIPHYSICSSIMULATION_H

#include "GameEngineStd.h"

#include "AIData.h"

#include "../Games/Actors/PlayerActor.h"

#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

#include "Mathematic/Algebra/Matrix4x4.h"

class AIPhysicsSimulation
{
	friend class QuakeAIManager;

public:
	AIPhysicsSimulation();
	~AIPhysicsSimulation() { };

	void CreatePathingJump(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph);
	void CreatePathingFall(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph);
	void CreatePathingRun(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph);
	void CreatePathing(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph);
	PathingNode* CreatePathingNode(ActorId playerId, std::shared_ptr<PathingGraph>& graph);
	PathingNode* CreatePathingNode(ActorId playerId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph);
	void CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes,
		std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph);
	void CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes, std::shared_ptr<PathingGraph>& graph);

	void CreateTransitions(std::shared_ptr<PathingGraph>& graph);
	void CreateClusters(std::shared_ptr<PathingGraph>& graph, unsigned int totalClusters);

protected:

	unsigned int mLastArcId;
	unsigned int mLastNodeId;

	std::shared_ptr<PlayerActor>& GetPlayerActor() { return mPlayerActor; }

private:

	// physics simulation
	void SimulateJump(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph);
	void SimulateMove(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph);
	void SimulateFall(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph);
	PathingNode* SimulateJump(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph);
	PathingNode* SimulateMove(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph);
	PathingNode* SimulateFall(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph);
	void SimulateStanding(ActorId actorId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph);
	void SimulateTriggerPush(PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph);
	void SimulateTriggerTeleport(PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph);

	void SimulatePathing(std::shared_ptr<PathingGraph>& graph);
	void SimulatePathing(std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph);
	void SimulatePathing(Transform transform, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph);
	void SimulateVisibility(std::shared_ptr<PathingGraph>& graph);

	Vector3<float> RayCollisionDetection(const Vector3<float>& startPos, const Vector3<float>& collisionPos);

	bool CheckActorNode(PathingNode* pathNode);

	PathingNode* FindClosestNode(ActorId playerId,
		std::shared_ptr<PathingGraph>& graph, float closestDistance, bool skipIsolated = true);

	unsigned int GetNewArcID(void) { return ++mLastArcId; }
	unsigned int GetNewNodeID(void) { return ++mLastNodeId; }

	// event delegates
	void PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData);
	void PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData);
	void PhysicsCollisionDelegate(BaseEventDataPtr pEventData);
	void PhysicsSeparationDelegate(BaseEventDataPtr pEventData);

	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);

	//positions which contains actors from game
	std::map<Vector3<float>, ActorId> mActorPositions;

	//set of nodes to be analized from the ground
	std::vector<PathingNode*> mOpenSet, mClosedSet;

	// Speed Controls
	Vector3<float> mMaxPushSpeed;
	Vector3<float> mMaxJumpSpeed;
	Vector3<float> mMaxFallSpeed;
	float mMaxMoveSpeed;
	float mMaxRotateSpeed;

	Vector3<float> mPushSpeed;
	Vector3<float> mJumpSpeed;
	Vector3<float> mFallSpeed;
	float mMoveSpeed;

	Vector3<float> mGravity;

	float mSimulationStep;

	std::shared_ptr<PlayerActor> mPlayerActor;

};   // AIPhysicsSimulation

#endif
