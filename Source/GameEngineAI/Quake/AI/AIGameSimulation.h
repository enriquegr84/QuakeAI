/*******************************************************
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#ifndef AIGAMEASIMULATION_H
#define AIGAMEASIMULATION_H

#include "GameEngineStd.h"

#include "AIData.h"

#include <ppl.h>
#include <ppltasks.h>

#include <mutex>
#include <concurrent_queue.h>
#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>

class Transform;
class AIPlanNode;

typedef Concurrency::concurrent_queue<AIPlanNode*> AIPlanNodeList;
typedef Concurrency::concurrent_vector<AIPlanNode*> AIPlanNodeVector;
typedef Concurrency::concurrent_unordered_map<ActorId, AIPlanNodeVector> ActorToAIPlanNodeMap;

//--------------------------------------------------------------------------------------------------------
// class AIPlanNode
// This class is a helper used in AIManager::FindPath().
//--------------------------------------------------------------------------------------------------------
class AIPlanNode
{
	AIPlanNode* mPrevNode;  // node we just came from
	PathingNode* mPathingNode;  // pointer to the pathing node from the pathing graph
	PathingActor* mPathingActor;  // pointer to the pathing arc from the pathing graph

	std::map<ActorId, float> mActors;  // vector of traversing actors up to this point

	bool mClosed;  // the node is closed if it's already been processed
	float mWeight;  // weight of the entire path up to this point

public:
	explicit AIPlanNode(PathingNode* pNode, PathingActor* pActor, AIPlanNode* pPrevNode);
	AIPlanNode* GetPrev(void) const { return mPrevNode; }
	PathingNode* GetPathingNode(void) const { return mPathingNode; }
	PathingActor* GetPathingActor(void) const { return mPathingActor; }
	PathingActorVec GetPathingActors(void);

	bool FindActor(ActorId actor) { return mActors.find(actor) != mActors.end(); }
	const std::map<ActorId, float>& GetActors() { return mActors; }
	bool ContainActors(PathingActorVec pathingActors);

	bool IsClosed(void) const { return mClosed; }
	float GetWeight(void) const { return mWeight; }

	void UpdateNode(PathingNode* pNode, PathingActor* pActor, AIPlanNode* pPrev);
	void SetClosed(bool toClose = true) { mClosed = toClose; }

	float CalculatePathCost(PathingNode* pNode, PathingActor* pActor);
};


//--------------------------------------------------------------------------------------------------------
// class AIFinder
// This class implements the AIFinder algorithm.
//--------------------------------------------------------------------------------------------------------
class AIFinder
{

public:
	AIFinder(void);
	~AIFinder(void);
	void Destroy(void);

	void operator()(PathingNode* pStartNode, const std::map<ActorId, float>& searchItems,
		std::map<PathingActorVec, float>& actorsPathPlans, unsigned int pathingType);

protected:

	ActorToAIPlanNodeMap mNodes;
	AIPlanNodeList mOpenSet;

private:

	void AddToOpenSet(PathingNode* pNode, PathingActor* pActor, AIPlanNode* pPrevNode);
	void AddToClosedSet(AIPlanNode* pNode);
	PathingActorVec RebuildPath(AIPlanNode* pGoalNode);
};

class AIGameSimulation
{
	friend class AIFinder;
	friend class QuakeAIManager;
	friend class QuakeAIEditorView;

public:
	AIGameSimulation();
	~AIGameSimulation();

	//status is calculated based on health and armor
	float CalculatePlayerStatus(const PlayerData& playerData);
	float CalculatePlayerWeaponStatus(const PlayerData& playerData);
	float CalculateBestHeuristicItem(const PlayerData& playerData);
	float CalculateHeuristicItem(const PlayerData& playerData, ActorId item, float itemWeight);
	void CalculateWeightItems(const PlayerData& playerData, std::map<ActorId, float>& searchItems);
	void CalculateHeuristic(EvaluationType evaluation, unsigned short threatLevel, PlayerData& playerData, PlayerData& otherPlayerData);
	void CalculateDamage(PlayerData& playerData, const std::map<float, VisibilityData>& visibility);
	void CalculateVisibility(unsigned short threatLevel,
		PathingNode* playerNode, float playerPathOffset, float playerVisibleTime,
		const PathingArcVec& playerPathPlan, std::map<float, VisibilityData>& playerVisibility,
		PathingNode* otherPlayerNode, float otherPlayerPathOffset, float otherPlayerVisibleTime,
		const PathingArcVec& otherPlayerPathPlan, std::map<float, VisibilityData>& otherPlayerVisibility);

	bool CanItemBeGrabbed(ActorId itemId, float itemTime, PlayerData& playerData, const std::map<ActorId, float>& gameItems);
	void PickupItems(PlayerData& playerData, const std::map<ActorId, float>& actors, const std::map<ActorId, float>& gameItems);

	void BuildPlayerPath(const AIAnalysis::PlayerSimulation& playerSimulation,
		PathingNode* playerNode, float playerPathOffset, PathingArcVec& playerPathPlan);
	void BuildExpandedPath(
		std::shared_ptr<PathingGraph>& graph, unsigned int maxPathingClusters, PathingNode* clusterNodeStart,
		const std::map<PathingCluster*, PathingArcVec>& clusterPaths, const std::map<PathingCluster*, float>& expandClusterPathWeights,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans);
	void BuildExpandedActorPath(
		std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters,
		Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics);
	void BuildExpandedActorPath(
		std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, float heuristicThreshold,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters,
		Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics);
	void BuildActorPath(std::shared_ptr<PathingGraph>& graph,
		unsigned int actionType, const std::map<ActorId, float>& gameItems, const std::map<ActorId, float>& searchItems,
		const PlayerData& player, PathingNode* clusterNodeStart, const PathingArcVec& clusterPathStart, float clusterPathOffset,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
		Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters);
	bool BuildPath(
		unsigned short& threatLevel, std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans);
	bool BuildLongPath(
		unsigned short& threatLevel, std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans);
	bool BuildLongPath(
		std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans);
	bool BuildLongestPath(
		std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans);

	PathingNode* FindClosestNode(ActorId playerId,
		std::shared_ptr<PathingGraph>& graph, float closestDistance, bool skipIsolated = true);

	// AI decision making process
	void Simulation(
		EvaluationType evaluation, const std::map<ActorId, float>& gameItems, unsigned short threatLevel,
		PlayerData& playerData, const PathingArcVec& playerPathPlan, float playerPathOffset,
		PlayerData& otherPlayerData, const PathingArcVec& otherPlayerPathPlan, float otherPlayerPathOffset);

	void PerformDecisionMaking(const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, float>>& playerDecisions,
		const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>>& playerWeaponDecisions,
		WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode);
	void PerformGuessingMaking(const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, float>>& playerGuessings,
		const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>>& playerWeaponGuessings,
		WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode);

	void PerformDecisionMaking(
		const AIAnalysis::GameEvaluation& gameEvaluation, const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode);
	void PerformGuessingMaking(
		const AIAnalysis::GameEvaluation& gameEvaluation, const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode);

	// Analysis simulation
	bool SimulatePlayerGuessing(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation);
	bool SimulatePlayerDecision(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation);

	// Runtime simulation
	bool SimulatePlayerGuessing(const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut, unsigned short& threatLevel,
		const std::map<ActorId, float>& gameItems, EvaluationType* playerEvaluation, EvaluationType evaluation);
	bool SimulatePlayerDecision(const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut, unsigned short& threatLevel,
		const std::map<ActorId, float>& gameItems, EvaluationType* playerEvaluation, EvaluationType evaluation);

	bool SimulateClusterPathing(
		const std::map<ActorId, float>& gameItems,
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut, 
		std::unordered_set<PathingNode*>& playerClusterPathings,
		std::unordered_set<PathingNode*>& playerClusterExpandedPathings,
		std::unordered_set<PathingNode*>& otherPlayerClusterPathings,
		std::unordered_set<PathingNode*>& otherPlayerClusterExpandedPathings);

protected:

	int GetNewPlanID(void) { return ++mLastPlanId; }

	std::shared_ptr<PathingGraph> mPathingGraph;
	std::map<ActorId, const AIAnalysis::ActorPickup*> mGameActorPickups;

private:

	void GetPlayerSimulation(const AIAnalysis::PlayerSimulation& playerSimulation, PlayerData& playerData);
	void SetPlayerSimulation(AIAnalysis::PlayerSimulation& playerSimulation, const PlayerData& playerData);

	int mLastPlanId;

};   // AIGameSimulation

#endif
