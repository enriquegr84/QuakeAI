/*******************************************************
 * AIPhysicsSimulation.cpp : AI Physics simulation class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#include "AIPhysicsSimulation.h"

#include "AI/AIManager.h"

#include "Core/OS/OS.h"
#include "Core/Logger/Logger.h"

#include "Core/IO/XmlResource.h"

#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "Physic/PhysicEventListener.h"

#include "../Games/Actors/LocationTarget.h"

#include "../QuakeEvents.h"
#include "../Quake.h"

#include <ppl.h>
#include <ppltasks.h>

#define GROUND_DISTANCE 16.f
#define FLOATING_DISTANCE 32.f

PathingNode* AIPhysicsSimulation::FindClosestNode(ActorId playerId,
	std::shared_ptr<PathingGraph>& graph, float closestDistance, bool skipIsolated)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	std::vector<std::pair<Transform, bool>> interpolations;
	gamePhysics->GetInterpolations(playerId, interpolations);

	PathingNode* closestNode = NULL;
	for (auto const& interpolation : interpolations)
	{
		if (interpolation.second) // check we are on ground
		{
			Vector3<float> currentPosition = interpolation.first.GetTranslation();
			PathingNode* currentNode = graph->FindClosestNode(currentPosition, skipIsolated);
			if (!currentNode) continue;

			if (closestDistance >= Length(currentNode->GetPosition() - currentPosition))
			{
				closestDistance = Length(currentNode->GetPosition() - currentPosition);
				closestNode = currentNode;
			}
		}
	}

	return closestNode;
}

AIPhysicsSimulation::AIPhysicsSimulation() 
{
	mLastArcId = 0;
	mLastNodeId = 0;

	mMaxRotateSpeed = 180.f;

	mGravity = Settings::Get()->GetVector3("default_gravity");

#if defined(PHYSX) && defined(_WIN64)

	mMaxPushSpeed = Vector3<float>{ 0.4f, 0.4f, 1.f };
	mMaxJumpSpeed = Vector3<float>{ 1.f, 1.f, 0.9f };
	mMaxFallSpeed = Vector3<float>{ 8.f, 8.f, 60.f };
	mMaxMoveSpeed = 500.f;

	mSimulationStep = 1.f / 60.f;

#else

	mMaxPushSpeed = Vector3<float>{
		PUSHTRIGGER_JUMP_SPEED_XZ, PUSHTRIGGER_JUMP_SPEED_XZ, PUSHTRIGGER_JUMP_SPEED_Y };
	mMaxJumpSpeed = Vector3<float>{
		DEFAULT_JUMP_SPEED_XZ, DEFAULT_JUMP_SPEED_XZ, DEFAULT_JUMP_SPEED_Y };
	mMaxFallSpeed = Vector3<float>{
		DEFAULT_FALL_SPEED_XZ, DEFAULT_FALL_SPEED_XZ, DEFAULT_FALL_SPEED_Y };
	mMaxMoveSpeed = DEFAULT_MOVE_SPEED;

	mSimulationStep = 1.f / 60.f;

#endif

	mPushSpeed = mMaxPushSpeed;
	mJumpSpeed = mMaxJumpSpeed;
	mFallSpeed = mMaxFallSpeed;
	mMoveSpeed = mMaxMoveSpeed;
}

//path generation via physics simulation
void AIPhysicsSimulation::CreatePathing(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	if (mOpenSet.size())
		SimulatePathing(transform, pathPlan, graph);

	//return to original position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	gamePhysics->OnUpdate(mSimulationStep);

	game->RegisterAllDelegates();
}

//jump pathing generation via physics simulation
void AIPhysicsSimulation::CreatePathingJump(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	if (PathingNode* pStartNode = graph->FindClosestNode(transform.GetTranslation(), false))
	{
		if (PathingNode* pEndNode = SimulateJump(pStartNode, transform, graph))
		{
			int skipArc = AT_MOVE;
			PathPlan* plan = graph->FindPath(pStartNode, pEndNode, skipArc);
			if (plan)
			{
				pathPlan.ResetPathPlan(plan->GetArcs());
				pathPlan.node = pStartNode;
				delete plan;
			}

			SimulateStanding(INVALID_ACTOR_ID, pStartNode->GetPosition(), graph);
		}
	}
	mOpenSet.clear();

	game->RegisterAllDelegates();
}

//run pathing generation via physics simulation
void AIPhysicsSimulation::CreatePathingRun(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	if (PathingNode* pStartNode = graph->FindClosestNode(transform.GetTranslation(), false))
	{
		if (PathingNode* pEndNode = SimulateMove(pStartNode, transform, graph))
		{
			int skipArc = AT_JUMP;
			PathPlan* plan = graph->FindPath(pStartNode, pEndNode, skipArc);
			if (plan)
			{
				pathPlan.ResetPathPlan(plan->GetArcs());
				pathPlan.node = pStartNode;
				delete plan;
			}

			SimulateStanding(INVALID_ACTOR_ID, pStartNode->GetPosition(), graph);
		}
	}
	mOpenSet.clear();

	game->RegisterAllDelegates();
}

//fall pathing generation via physics simulation
void AIPhysicsSimulation::CreatePathingFall(ActorId playerId, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	if (PathingNode* pStartNode = graph->FindClosestNode(transform.GetTranslation(), false))
	{
		if (PathingNode* pEndNode = SimulateFall(pStartNode, transform, graph))
		{
			int skipArc = AT_JUMP;
			PathPlan* plan = graph->FindPath(pStartNode, pEndNode, skipArc);
			if (plan)
			{
				pathPlan.ResetPathPlan(plan->GetArcs());
				pathPlan.node = pStartNode;
				delete plan;
			}

			SimulateStanding(INVALID_ACTOR_ID, pStartNode->GetPosition(), graph);
		}
	}
	mOpenSet.clear();

	game->RegisterAllDelegates();
}

PathingNode* AIPhysicsSimulation::CreatePathingNode(ActorId playerId, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	RegisterAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	std::vector<std::shared_ptr<Actor>> triggers;
	game->GetTriggerActors(triggers);
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			gamePhysics->SetCollisionFlags(trigger->GetId(), true);
#endif

	mActorPositions.clear();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	PathingNode* newNode = nullptr;
	if (mOpenSet.size())
	{
		newNode = mOpenSet.front();
		newNode->SetCluster(USHRT_MAX);
		mOpenSet.pop_back();

		//return to original position
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		gamePhysics->OnUpdate(mSimulationStep);

		// process the item actors which we have met
		std::map<PathingNode*, ActorId> actorNodes;
		std::map<Vector3<float>, ActorId>::iterator itActorPos;
		for (itActorPos = mActorPositions.begin(); itActorPos != mActorPositions.end(); itActorPos++)
		{
			std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(itActorPos->second).lock());
			PathingNode* pClosestNode = graph->FindClosestNode(itActorPos->first, false);
			if (pClosestNode != NULL && actorNodes.find(pClosestNode) == actorNodes.end())
			{
				pClosestNode->SetActorId(pItemActor->GetId());
				if (CheckActorNode(pClosestNode))
					actorNodes[pClosestNode] = pItemActor->GetId();
			}
		}
	}

	mActorPositions.clear();

	RemoveAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			gamePhysics->SetCollisionFlags(trigger->GetId(), false);
#endif

	game->RegisterAllDelegates();

	return newNode;
}

PathingNode* AIPhysicsSimulation::CreatePathingNode(ActorId playerId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	RegisterAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	std::vector<std::shared_ptr<Actor>> triggers;
	game->GetTriggerActors(triggers);
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			gamePhysics->SetCollisionFlags(trigger->GetId(), true);
#endif

	mActorPositions.clear();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	transform.SetTranslation(position);

	//set player position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	gamePhysics->OnUpdate(mSimulationStep);

	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	PathingNode* newNode = mOpenSet.front();
	mOpenSet.pop_back();

	// process the item actors which we have met
	std::map<PathingNode*, ActorId> actorNodes;
	std::map<Vector3<float>, ActorId>::iterator itActorPos;
	for (itActorPos = mActorPositions.begin(); itActorPos != mActorPositions.end(); itActorPos++)
	{
		std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(itActorPos->second).lock());
		PathingNode* pClosestNode = graph->FindClosestNode(itActorPos->first, false);
		if (pClosestNode != NULL && actorNodes.find(pClosestNode) == actorNodes.end())
		{
			pClosestNode->SetActorId(pItemActor->GetId());
			if (CheckActorNode(pClosestNode))
				actorNodes[pClosestNode] = pItemActor->GetId();
		}
	}

	mActorPositions.clear();

	RemoveAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			gamePhysics->SetCollisionFlags(trigger->GetId(), false);
#endif

	game->RegisterAllDelegates();

	return newNode;
}

void AIPhysicsSimulation::CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes,
	std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	mOpenSet = pathingNodes;
	if (mOpenSet.size())
		SimulatePathing(selectedClusters, graph);

	//return to original position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	gamePhysics->OnUpdate(mSimulationStep);

	game->RegisterAllDelegates();
}

//map generation via physics simulation
void AIPhysicsSimulation::CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes, std::shared_ptr<PathingGraph>& graph)
{
	QuakeLogic* game = static_cast<QuakeLogic *>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());

	// we create the waypoint according to the character controller physics system. Every
	// simulation step, it will be generated new waypoints from different actions such as
	// movement, jumping or falling and its conections
	mOpenSet = pathingNodes;
	SimulatePathing(graph);

	//save checkpoint
	std::wstring levelPath = L"ai/quake/" + ToWideString(Settings::Get()->Get("selected_world")) + L"/map.bin";
	GameLogic::Get()->GetAIManager()->SaveGraph(
		FileSystem::Get()->GetPath(ToString(levelPath.c_str())), graph);

	// we obtain visibility information from pathing graph 
	SimulateVisibility(graph);

	// create transitions associated to closest node
	CreateTransitions(graph);

	// we group the graph nodes in clusters
	CreateClusters(graph, MAX_CLUSTERS);
	
	GameLogic::Get()->GetAIManager()->SaveGraph(
		FileSystem::Get()->GetPath(ToString(levelPath.c_str())), graph);

	game->RegisterAllDelegates();
}

void AIPhysicsSimulation::SimulatePathing(std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph)
{
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	RegisterAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	std::vector<std::shared_ptr<Actor>> triggers;
	game->GetTriggerActors(triggers);
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			game->GetGamePhysics()->SetCollisionFlags(trigger->GetId(), true);
#endif

	mActorPositions.clear();

	// simulate the new node
	while (!mOpenSet.empty())
	{
		// grab the candidate
		PathingNode* pNode = mOpenSet.front();
		SimulateMove(pNode, graph);

		// we have processed this node so remove it from the open set
		mClosedSet.push_back(pNode);
		mOpenSet.erase(mOpenSet.begin());
	}

	// simulate the selected clusters nodes
	const PathingNodeMap& graphNodes = graph->GetNodes();
	for (PathingNodeMap::const_iterator it = graphNodes.begin(); it != graphNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (selectedClusters.find(pNode->GetCluster()) == selectedClusters.end())
			continue;

		SimulateMove(pNode, graph);
	}

	RemoveAllDelegates();

	// process the item actors which we have met
	std::map<PathingNode*, ActorId> actorNodes, triggerNodes;
	std::map<Vector3<float>, ActorId>::iterator itActorPos;
	for (itActorPos = mActorPositions.begin(); itActorPos != mActorPositions.end(); itActorPos++)
	{
		std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(itActorPos->second).lock());
		PathingNode* pClosestNode = graph->FindClosestNode(itActorPos->first, false);
		if (pClosestNode != NULL && actorNodes.find(pClosestNode) == actorNodes.end())
		{
			actorNodes[pClosestNode] = pItemActor->GetId();
			if (CheckActorNode(pClosestNode))
			{
				actorNodes[pClosestNode] = pItemActor->GetId();
				if (pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock())
				{
					std::shared_ptr<PushTrigger> pPushTrigger = 
						pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock();

					pClosestNode->RemoveArcs();
					triggerNodes[pClosestNode] = pItemActor->GetId();
					SimulateTriggerPush(pClosestNode, pPushTrigger->GetTarget(), graph);
				}
				else if (pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock())
				{
					std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
						pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();

					pClosestNode->RemoveArcs();
					triggerNodes[pClosestNode] = pItemActor->GetId();
					SimulateTriggerTeleport(pClosestNode, pTeleporterTrigger->GetTarget(), graph);
				}
			}
		}
	}

	while (!mClosedSet.empty())
	{
		// grab the candidate
		PathingNode* pNode = mClosedSet.front();

		// if the node is a trigger we don't simulate it
		if (triggerNodes.find(pNode) == triggerNodes.end())
		{
			SimulateJump(pNode, graph);
			SimulateFall(pNode, graph);
		}

		// we have processed this node so remove it from the closed set
		mClosedSet.erase(mClosedSet.begin());
	}

	const PathingNodeMap& pathNodes = graph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathNodes.begin(); it != pathNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (selectedClusters.find(pNode->GetCluster()) == selectedClusters.end())
			continue;

		// if the node is a trigger we don't simulate it
		if (triggerNodes.find(pNode) == triggerNodes.end())
		{
			SimulateJump(pNode, graph);
			SimulateFall(pNode, graph);
		}
	}

	mActorPositions.clear();

#if defined(PHYSX) && defined(_WIN64)
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			game->GetGamePhysics()->SetCollisionFlags(trigger->GetId(), false);
#endif
}

void AIPhysicsSimulation::SimulatePathing(Transform transform, NodePlan& nodePlan, std::shared_ptr<PathingGraph>& graph)
{
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	RegisterAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	std::vector<std::shared_ptr<Actor>> triggers;
	game->GetTriggerActors(triggers);
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			game->GetGamePhysics()->SetCollisionFlags(trigger->GetId(), true);
#endif

	mActorPositions.clear();

	// grab the candidate
	SimulateMove(mOpenSet.front(), transform, graph);

	RemoveAllDelegates();

	PathingNode* pStartNode = mOpenSet.front();
	PathingNode* pEndNode = mOpenSet.back();
	PathingNodeVec endNodes = { pEndNode };

	// process the item actors which we have met
	std::set<ActorId> addedActorNodes;
	std::map<PathingNode*, ActorId> actorNodes, triggerNodes;
	std::map<Vector3<float>, ActorId>::iterator itActorPos;
	for (itActorPos = mActorPositions.begin(); itActorPos != mActorPositions.end(); itActorPos++)
	{
		std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(itActorPos->second).lock());
		if (addedActorNodes.find(pItemActor->GetId()) == addedActorNodes.end())
		{
			addedActorNodes.insert(pItemActor->GetId());
			if (pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			{
				std::shared_ptr<PushTrigger> pPushTrigger =
					pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock();
				graph->InsertNode(new PathingNode(
					GetNewNodeID(), INVALID_ACTOR_ID, pPushTrigger->GetTarget().GetTranslation()));
			}
			else if (pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock())
			{
				std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
					pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();
				graph->InsertNode(new PathingNode(
					GetNewNodeID(), INVALID_ACTOR_ID, pTeleporterTrigger->GetTarget().GetTranslation()));
			}
		}

		PathingNode* pClosestNode = graph->FindClosestNode(itActorPos->first, false);
		if (pClosestNode != NULL && actorNodes.find(pClosestNode) == actorNodes.end())
		{
			pClosestNode->SetActorId(pItemActor->GetId());
			if (CheckActorNode(pClosestNode))
			{
				actorNodes[pClosestNode] = pItemActor->GetId();
				if (pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock())
				{
					std::shared_ptr<PushTrigger> pPushTrigger =
						pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock();

					pClosestNode->RemoveArcs();
					triggerNodes[pClosestNode] = pItemActor->GetId();
					SimulateTriggerPush(pClosestNode, pPushTrigger->GetTarget(), graph);
					for (auto& pathingArc : pClosestNode->GetArcs())
					{
						PathingArc* pArc = pathingArc.second;
						endNodes.push_back(pArc->GetNode());
					}
				}
				else if (pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock())
				{
					std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
						pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();

					pClosestNode->RemoveArcs();
					triggerNodes[pClosestNode] = pItemActor->GetId();
					SimulateTriggerTeleport(pClosestNode, pTeleporterTrigger->GetTarget(), graph);
					for (auto& pathingArc : pClosestNode->GetArcs())
					{
						PathingArc* pArc = pathingArc.second;
						endNodes.push_back(pArc->GetNode());
					}
				}
			}
		}
	}

	while (!mOpenSet.empty())
	{
		// grab the candidate
		PathingNode* pNode = mOpenSet.front();

		// if the node is a trigger we don't simulate it
		if (triggerNodes.find(pNode) == triggerNodes.end())
		{
			SimulateJump(pNode, transform, graph);
			SimulateFall(pNode, transform, graph);
		}

		// we have processed this node so remove it from the open set
		mOpenSet.erase(mOpenSet.begin());
	}

	int skipArc = -1;// Randomizer::Rand() % 2 ? AT_JUMP : -1;
	PathPlan* pathPlan = graph->FindPath(pStartNode, endNodes, skipArc);
	if (pathPlan)
	{
		nodePlan.ResetPathPlan(pathPlan->GetArcs());
		nodePlan.node = pStartNode;
		delete pathPlan;
	}

	mActorPositions.clear();

#if defined(PHYSX) && defined(_WIN64)
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			game->GetGamePhysics()->SetCollisionFlags(trigger->GetId(), false);
#endif
}

void AIPhysicsSimulation::SimulatePathing(std::shared_ptr<PathingGraph>& graph)
{
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	RegisterAllDelegates();

#if defined(PHYSX) && defined(_WIN64)
	std::vector<std::shared_ptr<Actor>> triggers;
	game->GetTriggerActors(triggers);
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			game->GetGamePhysics()->SetCollisionFlags(trigger->GetId(), true);
#endif

	mActorPositions.clear();

	while (!mOpenSet.empty())
	{
		// grab the candidate
		PathingNode* pNode = mOpenSet.front();
		SimulateMove(pNode, graph);

		// we have processed this node so remove it from the open set
		mClosedSet.push_back(pNode);
		mOpenSet.erase(mOpenSet.begin());
	}

	RemoveAllDelegates();

	// process the item actors which we have met
	std::map<PathingNode*, ActorId> actorNodes, triggerNodes;
	std::map<Vector3<float>, ActorId>::iterator itActorPos;
	for (itActorPos = mActorPositions.begin(); itActorPos != mActorPositions.end(); itActorPos++)
	{
		std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(itActorPos->second).lock());
		PathingNode* pClosestNode = graph->FindClosestNode(itActorPos->first, false);
		if (pClosestNode != NULL && actorNodes.find(pClosestNode) == actorNodes.end())
		{
			pClosestNode->SetActorId(pItemActor->GetId());
			if (CheckActorNode(pClosestNode))
			{
				actorNodes[pClosestNode] = pItemActor->GetId();
				if (pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock())
				{
					std::shared_ptr<PushTrigger> pPushTrigger =
						pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock();

					pClosestNode->RemoveArcs();
					triggerNodes[pClosestNode] = pItemActor->GetId();
					SimulateTriggerPush(pClosestNode, pPushTrigger->GetTarget(), graph);
				}
				else if (pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock())
				{
					std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
						pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();

					pClosestNode->RemoveArcs();
					triggerNodes[pClosestNode] = pItemActor->GetId();
					SimulateTriggerTeleport(pClosestNode, pTeleporterTrigger->GetTarget(), graph);
				}
			}
		}
	}

	while (!mClosedSet.empty())
	{
		// grab the candidate
		PathingNode* pNode = mClosedSet.front();

		// if the node is a trigger we don't simulate it
		if (triggerNodes.find(pNode) == triggerNodes.end())
		{
			SimulateJump(pNode, graph);
			SimulateFall(pNode, graph);
		}

		// we have processed this node so remove it from the closed set
		mClosedSet.erase(mClosedSet.begin());
	}

	mActorPositions.clear();

#if defined(PHYSX) && defined(_WIN64)
	for (auto& trigger : triggers)
		if (trigger->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			game->GetGamePhysics()->SetCollisionFlags(trigger->GetId(), false);
#endif
}

void AIPhysicsSimulation::CreateTransitions(std::shared_ptr<PathingGraph>& graph)
{
	// each arc in the graph has a set of transition nodes that we can’t realistically process for 
	// visibility since there are hundred of millions of pair transition combinations depending 
	// on the size of the map which will take forever to simulate visibility. Thats why we have to 
	// make an aproximation by associating every transition position to its neareast node
	Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
	{
		PathingNode* pathNode = node.second;

		for (auto const& pathArc : pathNode->GetArcs())
		{
			PathingTransition* pathTransition = pathArc.second->GetTransition();
			if (pathTransition)
			{
				PathingNodeVec nodes;
				std::vector<float> weights;
				std::vector<Vector3<float>> positions;
				for (const Vector3<float>& pathPosition : pathTransition->GetPositions())
					nodes.push_back(graph->FindClosestNode(pathPosition, false));
				for (const Vector3<float>& pathPosition : pathTransition->GetPositions())
					positions.push_back(pathPosition);
				for (float pathWeight : pathTransition->GetWeights())
					weights.push_back(pathWeight);

				pathArc.second->RemoveTransition();
				pathArc.second->AddTransition(new PathingTransition(nodes, weights, positions));
			}
			else
			{
				pathArc.second->AddTransition(new PathingTransition(
					{ pathArc.second->GetNode() }, 
					{ pathArc.second->GetWeight() }, 
					{ pathArc.second->GetNode()->GetPosition() }));
			}
		}
	});
}

bool AIPhysicsSimulation::CheckActorNode(PathingNode* pathNode)
{
	Vector3<float> scale = GameLogic::Get()->GetGamePhysics()->GetScale(mPlayerActor->GetId()) / 2.f;
	scale[AXIS_X] = 0.25f;
	scale[AXIS_Z] = 0.25f;
	Vector3<float> minEdge = pathNode->GetPosition() - scale;
	Vector3<float> maxEdge = pathNode->GetPosition() + scale;
	BoundingBox<float> nodeBB(minEdge, maxEdge);
	BoundingBox<float> actorBB = GameLogic::Get()->GetGamePhysics()->GetBoundingBox(pathNode->GetActorId());
	if (!actorBB.Intersect(nodeBB))
	{
		pathNode->SetActorId(INVALID_ACTOR_ID);
		return false;
	}

	return true;
}

void AIPhysicsSimulation::CreateClusters(std::shared_ptr<PathingGraph>& graph, unsigned int totalClusters)
{
	graph->RemoveClusters();

	std::vector<Point> points;
	const PathingNodeMap& pathingNodes = graph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* pathNode = (*it).second;
		std::vector<float> pos{pathNode->GetPosition()[0], pathNode->GetPosition()[1], pathNode->GetPosition()[2] };
		Point point(pathNode->GetId(), pos);
		points.push_back(point);
	}

	//Running K-Means Clustering
	unsigned int iters = 100;
	KMeans kmeans(totalClusters, iters);
	kmeans.Run(points);

	std::map<unsigned int, PathingNodeVec> clusterNodes;
	for (Point point : points)
	{
		PathingNode* pathNode = graph->FindNode(point.GetId());
		pathNode->SetCluster(point.GetCluster());

		clusterNodes[pathNode->GetCluster()].push_back(pathNode);
	}

	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);
	std::map<short, std::map<ActorId, PathingNode*>> clusterActorNodes;
	for (ActorId actor : searchActors)
	{
		std::shared_ptr<Actor> pItemActor(
			std::dynamic_pointer_cast<Actor>(GameLogic::Get()->GetActor(actor).lock()));
		std::shared_ptr<TransformComponent> transformComponent(
			pItemActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

		PathingNode* pNode = graph->FindClosestNode(transformComponent->GetPosition(), false);
		clusterActorNodes[pNode->GetCluster()][actor] = pNode;
	}

	std::map<unsigned short, PathingNode*> searchClusters;
	for (Clustering kCluster : kmeans.GetClusters())
	{
		if (clusterNodes.find(kCluster.GetId()) == clusterNodes.end() || clusterNodes[kCluster.GetId()].empty())
			continue;

		PathingNode* clusterCenterNode = graph->FindClosestNode(
			Vector3<float>{kCluster.GetCenter(0), kCluster.GetCenter(1), kCluster.GetCenter(2)}, false);
		if (clusterCenterNode->GetCluster() != kCluster.GetId())
			clusterCenterNode = clusterNodes[kCluster.GetId()].back(); // search for any cluster node

		Cluster* cluster = new Cluster(kCluster.GetId(), clusterCenterNode);
		graph->InsertCluster(cluster);

		for (auto const& clusterActorNode : clusterActorNodes[kCluster.GetId()])
			cluster->AddNodeActor(clusterActorNode.first, clusterActorNode.second);

		for (auto clusterNode : clusterNodes[kCluster.GetId()])
			cluster->AddNode(clusterNode);

		searchClusters[cluster->GetId()] = cluster->GetNode();
	}

	//find most visible node for each cluster
	for (auto& cluster : graph->GetClusters())
	{
		PathingNode* visibleNode = cluster.second->GetNode();
		int visibleNodeCount = (int)visibleNode->GetVisibileNodes().size();
		for (PathingNode* clusterNode : clusterNodes[cluster.second->GetId()])
		{
			if (visibleNodeCount < clusterNode->GetVisibileNodes().size())
			{
				//lets put a minimum distance between the other cluster representative nodes
				bool updateNode = true;
				for (auto& searchCluster : searchClusters)
				{
					if (searchCluster.first != cluster.first)
					{
						if (Length(searchCluster.second->GetPosition() - clusterNode->GetPosition()) < 40.f)
						{
							updateNode = false;
							break;
						}
					}
				}

				if (updateNode)
				{
					visibleNode = clusterNode;
					visibleNodeCount = (int)clusterNode->GetVisibileNodes().size();
				}
			}
		}

		cluster.second->SetNode(visibleNode);
		searchClusters[cluster.second->GetId()] = visibleNode;
	}

	std::mutex mutex;

	Concurrency::parallel_for_each(begin(graph->GetClusters()), end(graph->GetClusters()), [&](auto& cluster)
	{
		std::map<unsigned short, std::map<PathingNode*, unsigned short>> clustersVisibleNodes;
		for (auto const& clusterNode : cluster.second->GetNodes())
		{
			for (auto const& visibleNode : clusterNode.second->GetVisibileNodes())
				clustersVisibleNodes[visibleNode.first->GetCluster()][clusterNode.second] = 0;
			for (auto const& visibleNode : clusterNode.second->GetVisibileNodes())
				clustersVisibleNodes[visibleNode.first->GetCluster()][clusterNode.second]++;
		}

		for (auto& clusterVisibleNodes : clustersVisibleNodes)
		{
			Cluster* visibleCluster = graph->FindCluster(clusterVisibleNodes.first);

			PathingNode* visibleClusterNode = visibleCluster->GetNode();
			unsigned short visibleClusterNodeCount = 0;
			if (clusterVisibleNodes.second.find(visibleCluster->GetNode()) != clusterVisibleNodes.second.end())
				visibleClusterNodeCount = clusterVisibleNodes.second[visibleCluster->GetNode()];
			for (auto const& clusterVisibleNode : clusterVisibleNodes.second)
			{
				if (visibleClusterNodeCount < clusterVisibleNode.second)
				{
					visibleClusterNode = clusterVisibleNode.first;
					visibleClusterNodeCount = clusterVisibleNode.second;
				}
			}

			if (visibleClusterNodeCount)
			{
				mutex.lock();
				cluster.second->AddVisibleCluster(visibleCluster->GetId(), visibleClusterNode);
				mutex.unlock();
			}
		}
	});

	Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
	{
		PathingNode* pathNode = node.second;

		ClusterPlanMap clusterPlans;
		//add cluster transitions with jumps and moves
		graph->FindPathPlans(pathNode, searchClusters, clusterPlans);

		mutex.lock();
		for (auto const& clusterPlan : clusterPlans)
		{
			PathingArcVec pathArcs = clusterPlan.second->GetArcs();
			if (pathArcs.size())
			{
				PathingNode* pathTarget = pathArcs.back()->GetNode();

				if (pathTarget != pathNode)
				{
					PathingNode* currentNode = pathNode;
					for (PathingArc* pArc : pathArcs)
					{
						bool addCluster = true;
						PathingClusterVec clusters;

						currentNode->GetClusters(AT_JUMP, clusters);
						for (PathingCluster* cluster : clusters)
						{
							if (cluster->GetTarget() == pathTarget)
							{
								addCluster = false;
								break;
							}
						}

						if (addCluster)
						{
							PathingCluster* pathCluster = new PathingCluster(AT_JUMP);
							pathCluster->LinkClusters(pArc->GetNode(), pathTarget);
							currentNode->AddCluster(pathCluster);
						}
						else break;

						currentNode = pArc->GetNode();
					}
				}
			}
			else
			{
				PathingCluster* pathCluster = new PathingCluster(AT_JUMP);
				pathCluster->LinkClusters(pathNode, pathNode);
				pathNode->AddCluster(pathCluster);
			}
			delete clusterPlan.second;
		}
		mutex.unlock();
	});

	Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
	{
		PathingNode* pathNode = node.second;

		ClusterPlanMap clusterPlans;
		//add cluster transitions only with moves
		graph->FindPathPlans(pathNode, searchClusters, clusterPlans, AT_JUMP);

		mutex.lock();
		for (auto const& clusterPlan : clusterPlans)
		{
			PathingArcVec pathArcs = clusterPlan.second->GetArcs();
			if (pathArcs.size())
			{
				PathingNode* pathTarget = pathArcs.back()->GetNode();
				if (pathTarget != pathNode)
				{
					PathingNode* currentNode = pathNode;
					for (PathingArc* pArc : pathArcs)
					{
						bool addCluster = true;
						PathingClusterVec clusters;

						currentNode->GetClusters(AT_MOVE, clusters);
						for (PathingCluster* cluster : clusters)
						{
							if (cluster->GetTarget() == pathTarget)
							{
								addCluster = false;
								break;
							}
						}

						if (addCluster)
						{
							PathingCluster* pathCluster = new PathingCluster(AT_MOVE);
							pathCluster->LinkClusters(pArc->GetNode(), pathTarget);
							currentNode->AddCluster(pathCluster);
						}
						else break;

						currentNode = pArc->GetNode();
					}
				}
			}
			else
			{
				PathingCluster* pathCluster = new PathingCluster(AT_MOVE);
				pathCluster->LinkClusters(pathNode, pathNode);
				pathNode->AddCluster(pathCluster);
			}
			delete clusterPlan.second;
		}
		mutex.unlock();
	});

	Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
	{
		PathingNode* pathNode = node.second;

		ActorPlanMap actorPlans;
		//add actor transitions with jumps and moves
		graph->FindPathPlans(pathNode, searchActors, actorPlans);

		mutex.lock();
		for (auto const& actorPlan : actorPlans)
		{
			PathingArcVec pathArcs = actorPlan.second->GetArcs();
			if (pathArcs.size())
			{
				PathingNode* pathTarget = pathArcs.back()->GetNode();

				if (pathTarget != pathNode)
				{
					PathingNode* currentNode = pathNode;
					for (PathingArc* pArc : pathArcs)
					{
						bool addActor = true;
						PathingActorVec actors;

						currentNode->GetActors(AT_JUMP, actors);
						for (PathingActor* actor : actors)
						{
							if (actor->GetTarget() == pathTarget)
							{
								addActor = false;
								break;
							}
						}

						if (addActor)
						{
							PathingActor* pathActor = new PathingActor(AT_JUMP, actorPlan.first);
							pathActor->LinkActors(pArc->GetNode(), pathTarget);
							currentNode->AddActor(pathActor);
						}
						else break;

						currentNode = pArc->GetNode();
					}
				}
			}
			delete actorPlan.second;
		}
		mutex.unlock();
	});

	Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
	{
		PathingNode* pathNode = node.second;

		ActorPlanMap actorPlans;
		//add actor transitions only with moves
		graph->FindPathPlans(pathNode, searchActors, actorPlans, AT_JUMP);

		mutex.lock();
		for (auto const& actorPlan : actorPlans)
		{
			PathingArcVec pathArcs = actorPlan.second->GetArcs();
			if (pathArcs.size())
			{
				PathingNode* pathTarget = pathArcs.back()->GetNode();
				if (pathTarget != pathNode)
				{
					PathingNode* currentNode = pathNode;
					for (PathingArc* pArc : pathArcs)
					{
						bool addActor = true;
						PathingActorVec actors;

						currentNode->GetActors(AT_MOVE, actors);
						for (PathingActor* actor : actors)
						{
							if (actor->GetTarget() == pathTarget)
							{
								addActor = false;
								break;
							}
						}

						if (addActor)
						{
							PathingActor* pathActor = new PathingActor(AT_MOVE, actorPlan.first);
							pathActor->LinkActors(pArc->GetNode(), pathTarget);
							currentNode->AddActor(pathActor);
						}
						else break;

						currentNode = pArc->GetNode();
					}
				}
			}
			delete actorPlan.second;
		}
		mutex.unlock();
	});
}

#if defined(PHYSX) && defined(_WIN64)

void AIPhysicsSimulation::SimulateStanding(ActorId actorId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Transform transform;
	transform.SetTranslation(position);
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	PathingNode* pClosestNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pClosestNode)
	{
		Vector3<float> diff = pClosestNode->GetPosition() - transform.GetTranslation();
		if (Length(diff) <= PATHING_DEFAULT_NODE_TOLERANCE)
		{
			//if we find any node close to our current position we don't add it
			return;
		}
	}

	PathingNode* pNewNode = new PathingNode(GetNewNodeID(), actorId, transform.GetTranslation());
	graph->InsertNode(pNewNode);
	mOpenSet.push_back(pNewNode);
}

void AIPhysicsSimulation::SimulateTriggerTeleport(
	PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Vector3<float> direction;
	Matrix4x4<float> rotation = target.GetRotation();

	// forward vector
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0.f;
	Normalize(direction);

	direction[AXIS_X] *= mFallSpeed[AXIS_X];
	direction[AXIS_Z] *= mFallSpeed[AXIS_Z];
	direction[AXIS_Y] = -mFallSpeed[AXIS_Y];

	Transform transform;
	transform.SetTranslation(target.GetTranslation());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), direction);
	gamePhysics->OnUpdate(mSimulationStep);

	//falling simulation
	float deltaTime = 0.f, totalTime = 0.f;
	std::vector<std::pair<Vector3<float>, float>> nodePositions;
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		totalTime += mSimulationStep;

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
		gamePhysics->OnUpdate(mSimulationStep);
	}
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	//we store the falling position on ground
	std::vector<std::pair<Transform, bool>> interpolations;
	gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
	for (auto const& interpolation : interpolations)
	{
		if (interpolation.second) // check we are on ground
		{
			transform = interpolation.first;
			break;
		}

		deltaTime += mSimulationStep / 12.f;
	}
	if (deltaTime > 0.f)
	{
		totalTime += deltaTime;

		nodePositions.push_back({ transform.GetTranslation(), deltaTime });
	}
	if (totalTime > 10.f) return;

	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode)
	{
		if (pEndNode != NULL && pNode->FindArc(pEndNode) == NULL)
		{
			if (pEndNode->GetActorId() == INVALID_ACTOR_ID || pNode->GetActorId() != pEndNode->GetActorId())
			{
				PathingArc* pArc = new PathingArc(GetNewArcID(), AT_TELEPORT, pEndNode, totalTime);
				pNode->AddArc(pArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += nodePositions[idx].second;

					if (Length(nodePositions[idx].first - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx].first);

						totalTime = 0.f;
						position = nodePositions[idx].first;
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

void AIPhysicsSimulation::SimulateTriggerPush(
	PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Transform transform;
	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	Vector3<float> direction, jump, fall, move;
	direction = target.GetTranslation() - transform.GetTranslation();
	float push = mPushSpeed[AXIS_Y] + direction[AXIS_Y] * 0.004f;

	direction[AXIS_Y] = 0;
	Normalize(direction);

	move = direction;
	move *= mMoveSpeed;

	jump[AXIS_X] = direction[AXIS_X] * mPushSpeed[AXIS_X];
	jump[AXIS_Z] = direction[AXIS_Z] * mPushSpeed[AXIS_Z];
	jump[AXIS_Y] = push;

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Fall(mPlayerActor->GetId(), fall);
	gamePhysics->Move(mPlayerActor->GetId(), move);
	gamePhysics->Jump(mPlayerActor->GetId(), jump);
	gamePhysics->OnUpdate(mSimulationStep);

	//falling simulation
	float deltaTime = 0.f, totalTime = 0.f;
	std::vector<std::pair<Vector3<float>, float>> nodePositions;
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		totalTime += mSimulationStep;

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
		gamePhysics->OnUpdate(mSimulationStep);
	}
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	//we store the falling position on ground
	std::vector<std::pair<Transform, bool>> interpolations;
	gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
	for (auto const& interpolation : interpolations)
	{
		if (interpolation.second) // check we are on ground
		{
			transform = interpolation.first;
			break;
		}

		deltaTime += mSimulationStep / 12.f;
	}
	if (deltaTime > 0.f)
	{
		totalTime += deltaTime;

		nodePositions.push_back({ transform.GetTranslation(), deltaTime });
	}
	if (totalTime > 10.f) return;

	//we store the jump if we find a landing node
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode)
	{
		if (pEndNode != NULL && pNode->FindArc(pEndNode) == NULL)
		{
			if (pEndNode->GetActorId() == INVALID_ACTOR_ID || pNode->GetActorId() != pEndNode->GetActorId())
			{
				PathingArc* pArc = new PathingArc(GetNewArcID(), AT_PUSH, pEndNode, totalTime);
				pNode->AddArc(pArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += nodePositions[idx].second;

					if (Length(nodePositions[idx].first - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx].first);

						totalTime = 0.f;
						position = nodePositions[idx].first;
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

float FindClosestMovement(std::vector<std::pair<Transform, bool>>& movements, const Vector3<float>& pos)
{
	float length = FLT_MAX;
	std::vector<std::pair<Transform, bool>>::iterator it;
	for (it = movements.begin(); it != movements.end(); it++)
	{
		Vector3<float> diff = pos - (*it).first.GetTranslation();
		if (Length(diff) < length)
			length = Length(diff);
	}

	return length;
}

// Check penetration
bool CheckPenetration(ActorId playerId, const Vector3<float>& translation)
{
	Transform transform;
	transform.SetTranslation(translation);
	GameLogic::Get()->GetGamePhysics()->SetTransform(playerId, transform);
	return GameLogic::Get()->GetGamePhysics()->CheckPenetration(playerId);
}

// Cliff control
bool Cliff(ActorId playerId, const Vector3<float>& translation)
{
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		// This will give us the "look at" vector 
		// in world space - we'll use that to move.
		Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
		atWorld = rotation * atWorld;
#else
		atWorld = atWorld * rotation;
#endif

		Vector3<float> position = translation + HProject(atWorld * 10.f);

		Transform start;
		start.SetRotation(rotation);
		start.SetTranslation(position);

		Transform end;
		end.SetRotation(rotation);
		end.SetTranslation(position - Vector3<float>::Unit(AXIS_Y) * 300.f);

		Vector3<float> collision, collisionNormal;
		collision = end.GetTranslation();
		ActorId actorId = GameLogic::Get()->GetGamePhysics()->CastRay(
			start.GetTranslation(), end.GetTranslation(), collision, collisionNormal, playerId);

		//Check whether we are close to a cliff
		//printf("weight cliff %f \n", abs(collision[AXIS_Y] - position[AXIS_Y]));
		if (abs(collision[AXIS_Y] - position[AXIS_Y]) > 60.f)
			return true;
	}
	return false;
}

PathingNode* AIPhysicsSimulation::SimulateMove(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));


	Matrix4x4<float> rotation = transform.GetRotation();
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	Vector3<float> direction, fall, move;
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0;
	Normalize(direction);

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	move = direction;
	move *= mMoveSpeed;

	gamePhysics->Move(mPlayerActor->GetId(), move);
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	gamePhysics->OnUpdate(mSimulationStep);

	//create movement interpolations on the ground and air
	std::vector<std::pair<Transform, bool>> movements;
	PathingNode* pCurrentNode = pNode;

	do
	{
		if (!gamePhysics->OnGround(mPlayerActor->GetId()))
		{
			gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);

			gamePhysics->Fall(mPlayerActor->GetId(), fall);
			gamePhysics->OnUpdate(mSimulationStep);

			// gravity falling simulation
			float totalTime = mSimulationStep;
			while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
			{
				gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);
				gamePhysics->OnUpdate(mSimulationStep);

				totalTime += mSimulationStep;
			}

			if (totalTime >= 10.f)
				break;
		}
		gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);

		PathingNode* pClosestNode = FindClosestNode(
			mPlayerActor->GetId(), graph, PATHING_MOVEMENT_NODE_TOLERANCE, false);
		if (pClosestNode && pCurrentNode != pClosestNode)
		{
			//if we find any node close to our current position we stop
			break;
		}

		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		gamePhysics->OnUpdate(mSimulationStep);

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	} while (FindClosestMovement(movements, transform.GetTranslation()) >= 4.f); // stalling is a break condition

	if (!movements.empty())
	{
		float deltaTime = 0.f, totalTime = 0.f;
		std::vector<std::pair<Transform, bool>>::iterator itMove = movements.begin();
		std::vector<Vector3<float>> positions{ (*itMove).first.GetTranslation() };
		std::vector<float> weights{ 0 };
		itMove++;

		for (; itMove != movements.end(); itMove++)
		{
			if ((*itMove).second)	// check if we are on ground
			{
				if (!pCurrentNode)
				{
					PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
					Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
					if (Length(diff) >= GROUND_DISTANCE)
					{
						if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
							!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
						{
							PathingNode* pNewNode = new PathingNode(
								GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());
							graph->InsertNode(pNewNode);
							mOpenSet.push_back(pNewNode);
							pCurrentNode = pNewNode;

							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pNewNode->GetPosition() };
							weights = { 0 };
						}
					}
					else if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
					{
						pCurrentNode = pClosestNode;
						deltaTime = 0.f;
						totalTime = 0.f;

						positions = { pClosestNode->GetPosition() };
						weights = { 0 };
					}

					continue;
				}

				totalTime += mSimulationStep / 12.f;
				deltaTime += mSimulationStep / 12.f;

				if (Length((*itMove).first.GetTranslation() - positions.back()) >= GROUND_DISTANCE)
				{
					weights.push_back(deltaTime);
					positions.push_back((*itMove).first.GetTranslation());

					deltaTime = 0.f;
				}

				PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
				Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
				if (Length(diff) >= GROUND_DISTANCE)
				{
					Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

					Transform start;
					start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
					Transform end;
					end.SetTranslation((*itMove).first.GetTranslation() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

					gamePhysics->SetTransform(mPlayerActor->GetId(), start);
					gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
					gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
					gamePhysics->OnUpdate(mSimulationStep);

					std::optional<Vector3<float>> collision, collisionNormal;
					ActorId actorId = gamePhysics->ConvexSweep(
						mPlayerActor->GetId(), start, end, collision, collisionNormal);
					if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
					{
						if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
							!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
						{
							PathingNode* pNewNode = new PathingNode(
								GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());

							//we only consider arcs within certain time range
							if (totalTime >= 0.02f && totalTime <= 0.2f)
							{
								PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pNewNode, totalTime);
								pCurrentNode->AddArc(pNewArc);

								if (positions.size())
								{
									if (deltaTime > 0.f)
									{
										weights.push_back(deltaTime);
										positions.push_back((*itMove).first.GetTranslation());
									}

									//lets interpolate transitions from the already created arc
									pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
								}
							}

							graph->InsertNode(pNewNode);
							mOpenSet.push_back(pNewNode);
							pCurrentNode = pNewNode;

							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pNewNode->GetPosition() };
							weights = { 0 };
						}
						else
						{
							continue; //we don't create a new node if it doesn't pass the cliff control or penetration checking
						}
					}
					else
					{
						break; //we stop processing movements if we find collision
					}
				}
				else if (pCurrentNode != pClosestNode)
				{
					if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
					{
						if (pCurrentNode->FindArc(pClosestNode) == NULL)
						{
							Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

							Transform start;
							start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
							Transform end;
							end.SetTranslation(pClosestNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

							gamePhysics->SetTransform(mPlayerActor->GetId(), start);
							gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
							gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
							gamePhysics->OnUpdate(mSimulationStep);

							std::optional<Vector3<float>> collision, collisionNormal;
							ActorId actorId = gamePhysics->ConvexSweep(
								mPlayerActor->GetId(), start, end, collision, collisionNormal);
							if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
							{
								//we only consider arcs within certain time range
								if (totalTime >= 0.02f && totalTime <= 0.2f)
								{
									PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pClosestNode, totalTime);
									pCurrentNode->AddArc(pNewArc);

									if (positions.size())
									{
										if (deltaTime > 0.f)
										{
											weights.push_back(deltaTime);
											positions.push_back(pClosestNode->GetPosition());
										}

										//lets interpolate transitions from the already created arc
										pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
									}
								}
							}
							else
							{
								break; //we stop processing movements if we find collision
							}
						}
						pCurrentNode = pClosestNode;

						deltaTime = 0.f;
						totalTime = 0.f;

						positions = { pClosestNode->GetPosition() };
						weights = { 0 };
					}
				}
			}
			else pCurrentNode = NULL;
		}
	}

	return pCurrentNode;
}

void AIPhysicsSimulation::SimulateMove(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		Transform transform;
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		Vector3<float> direction, fall, move;
#if defined(GE_USE_MAT_VEC)
		direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
		direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
		direction[AXIS_Y] = 0;
		Normalize(direction);

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		move = direction;
		move *= mMoveSpeed;

		gamePhysics->Move(mPlayerActor->GetId(), move);
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		gamePhysics->OnUpdate(mSimulationStep);

		//create movement interpolations on the ground and air
		std::vector<std::pair<Transform, bool>> movements;
		PathingNode* pCurrentNode = pNode;

		do
		{
			if (!gamePhysics->OnGround(mPlayerActor->GetId()))
			{
				gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);

				gamePhysics->Fall(mPlayerActor->GetId(), fall);
				gamePhysics->OnUpdate(mSimulationStep);

				// gravity falling simulation
				float totalTime = mSimulationStep;
				while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
				{
					gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);
					gamePhysics->OnUpdate(mSimulationStep);

					totalTime += mSimulationStep;
				}

				if (totalTime >= 10.f)
					break;
			}
			gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);

			PathingNode* pClosestNode = FindClosestNode(
				mPlayerActor->GetId(), graph, PATHING_MOVEMENT_NODE_TOLERANCE, false);
			if (pClosestNode && pCurrentNode != pClosestNode)
			{
				//if we find any node close to our current position we stop
				break;
			}

			gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
			gamePhysics->OnUpdate(mSimulationStep);

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		} while (FindClosestMovement(movements, transform.GetTranslation()) >= 4.f); // stalling is a break condition

		if (!movements.empty())
		{
			float deltaTime = 0.f, totalTime = 0.f;
			std::vector<std::pair<Transform, bool>>::iterator itMove = movements.begin();
			std::vector<Vector3<float>> positions{ (*itMove).first.GetTranslation() };
			std::vector<float> weights{ 0 };
			itMove++;

			for (; itMove != movements.end(); itMove++)
			{
				if ((*itMove).second)	// check if we are on ground
				{
					if (!pCurrentNode)
					{
						PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
						Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
						if (Length(diff) >= GROUND_DISTANCE)
						{
							if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
								!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
							{
								PathingNode* pNewNode = new PathingNode(
									GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());
								graph->InsertNode(pNewNode);
								mOpenSet.push_back(pNewNode);
								pCurrentNode = pNewNode;

								deltaTime = 0.f;
								totalTime = 0.f;

								positions = { pNewNode->GetPosition() };
								weights = { 0 };
							}
						}
						else if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
						{
							pCurrentNode = pClosestNode;
							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pClosestNode->GetPosition() };
							weights = { 0 };
						}

						continue;
					}

					totalTime += mSimulationStep / 12.f;
					deltaTime += mSimulationStep / 12.f;

					if (Length((*itMove).first.GetTranslation() - positions.back()) >= GROUND_DISTANCE)
					{
						weights.push_back(deltaTime);
						positions.push_back((*itMove).first.GetTranslation());

						deltaTime = 0.f;
					}

					PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
					Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
					if (Length(diff) >= GROUND_DISTANCE)
					{
						Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

						Transform start;
						start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
						Transform end;
						end.SetTranslation((*itMove).first.GetTranslation() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

						gamePhysics->SetTransform(mPlayerActor->GetId(), start);
						gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
						gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
						gamePhysics->OnUpdate(mSimulationStep);

						std::optional<Vector3<float>> collision, collisionNormal;
						ActorId actorId = gamePhysics->ConvexSweep(
							mPlayerActor->GetId(), start, end, collision, collisionNormal);
						if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
						{
							if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
								!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
							{
								PathingNode* pNewNode = new PathingNode(
									GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());

								//we only consider arcs within certain time range
								if (totalTime >= 0.02f && totalTime <= 0.2f)
								{
									PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pNewNode, totalTime);
									pCurrentNode->AddArc(pNewArc);

									if (positions.size())
									{
										if (deltaTime > 0.f)
										{
											weights.push_back(deltaTime);
											positions.push_back((*itMove).first.GetTranslation());
										}

										//lets interpolate transitions from the already created arc
										pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
									}
								}

								graph->InsertNode(pNewNode);
								mOpenSet.push_back(pNewNode);
								pCurrentNode = pNewNode;

								deltaTime = 0.f;
								totalTime = 0.f;

								positions = { pNewNode->GetPosition() };
								weights = { 0 };
							}
							else
							{
								continue; //we don't create a new node if it doesn't pass the cliff control or penetration checking
							}
						}
						else
						{
							break; //we stop processing movements if we find collision
						}
					}
					else if (pCurrentNode != pClosestNode)
					{
						if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
						{
							if (pCurrentNode->FindArc(pClosestNode) == NULL)
							{
								Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

								Transform start;
								start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
								Transform end;
								end.SetTranslation(pClosestNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

								gamePhysics->SetTransform(mPlayerActor->GetId(), start);
								gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
								gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
								gamePhysics->OnUpdate(mSimulationStep);

								std::optional<Vector3<float>>  collision, collisionNormal;
								ActorId actorId = gamePhysics->ConvexSweep(
									mPlayerActor->GetId(), start, end, collision, collisionNormal);
								if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
								{
									//we only consider arcs within certain time range
									if (totalTime >= 0.02f && totalTime <= 0.2f)
									{
										PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pClosestNode, totalTime);
										pCurrentNode->AddArc(pNewArc);

										if (positions.size())
										{
											if (deltaTime > 0.f)
											{
												weights.push_back(deltaTime);
												positions.push_back(pClosestNode->GetPosition());
											}

											//lets interpolate transitions from the already created arc
											pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
										}
									}
								}
								else
								{
									break; //we stop processing movements if we find collision
								}
							}


							pCurrentNode = pClosestNode;

							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pClosestNode->GetPosition() };
							weights = { 0 };
						}
					}
				}
				else pCurrentNode = NULL;
			}
		}
	}
}

PathingNode* AIPhysicsSimulation::SimulateJump(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	Vector3<float> direction, jump, fall, move;
	Matrix4x4<float> rotation = transform.GetRotation();
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	// forward vector
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0.f;
	Normalize(direction);

	move = direction;
	move *= mMoveSpeed;

	jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
	jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
	jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Move(mPlayerActor->GetId(), move);
	gamePhysics->Jump(mPlayerActor->GetId(), jump);
	gamePhysics->Fall(mPlayerActor->GetId(), fall);
	gamePhysics->OnUpdate(mSimulationStep);

	//falling simulation
	float deltaTime = 0.f, totalTime = 0.f;
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		totalTime += mSimulationStep;

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		gamePhysics->OnUpdate(mSimulationStep);
	}
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	//we store the falling position on ground
	std::vector<std::pair<Transform, bool>> interpolations;
	gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
	for (auto const& interpolation : interpolations)
	{
		if (interpolation.second) // check we are on ground
		{
			transform = interpolation.first;
			break;
		}

		deltaTime += mSimulationStep / 12.f;
	}
	if (deltaTime > 0.f)
		totalTime += deltaTime;
	if (totalTime > 10.f) 
		return NULL;

	//then we do the "real" jump to the closest node we have found from the simulation
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode && pEndNode && pNode->FindArc(pEndNode) == NULL)
	{
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		direction = pEndNode->GetPosition() - pNode->GetPosition();
		direction[AXIS_Y] = 0;
		Normalize(direction);

		move = direction;
		move *= mMoveSpeed;

		jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
		jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
		jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Move(mPlayerActor->GetId(), move);
		gamePhysics->Jump(mPlayerActor->GetId(), jump);
		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->OnUpdate(mSimulationStep);

		//falling simulation
		totalTime = 0.f;
		std::vector<std::pair<Vector3<float>, float>> nodePositions;
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
			gamePhysics->OnUpdate(mSimulationStep);
		}
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());

		//we store the falling position on ground
		deltaTime = 0.f;
		interpolations.clear();
		gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
		for (auto const& interpolation : interpolations)
		{
			if (interpolation.second) // check we are on ground
			{
				transform = interpolation.first;
				break;
			}

			deltaTime += mSimulationStep / 12.f;
		}
		if (deltaTime > 0.f)
		{
			totalTime += deltaTime;

			nodePositions.push_back({ transform.GetTranslation(), deltaTime });
		}
		if (totalTime > 10.f) 
			return NULL;

		//we store the jump if we find a landing node
		if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
		{
			PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_JUMP, pEndNode, totalTime);
			pNode->AddArc(pNewArc);

			//lets interpolate transitions from the already created arc
			totalTime = 0.f;
			Vector3<float> position = pNode->GetPosition();

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
			{
				totalTime += nodePositions[idx].second;

				if (Length(nodePositions[idx].first - position) >= FLOATING_DISTANCE)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(nodePositions[idx].first);

					totalTime = 0.f;
					position = nodePositions[idx].first;
				}
			}

			if (nodes.size())
			{
				if (totalTime > 0.f)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(pEndNode->GetPosition());
				}
				pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
			}
			return pEndNode;
		}
		else return NULL;
	}

	return pEndNode;
}

void AIPhysicsSimulation::SimulateJump(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Vector3<float> direction, jump, fall, move;
	std::map<PathingNode*, bool> visitedNodes;
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		Transform transform;
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		// forward vector
#if defined(GE_USE_MAT_VEC)
		direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
		direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
		direction[AXIS_Y] = 0.f;
		Normalize(direction);

		move = direction;
		move *= mMoveSpeed;

		jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
		jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
		jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->Move(mPlayerActor->GetId(), move);
		gamePhysics->Jump(mPlayerActor->GetId(), jump);
		gamePhysics->OnUpdate(mSimulationStep);

		//falling simulation
		float deltaTime = 0.f, totalTime = 0.f;
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			gamePhysics->OnUpdate(mSimulationStep);
		}
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());

		//we store the falling position on ground
		std::vector<std::pair<Transform, bool>> interpolations;
		gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
		for (auto const& interpolation : interpolations)
		{
			if (interpolation.second) // check we are on ground
			{
				transform = interpolation.first;
				break;
			}

			deltaTime += mSimulationStep / 12.f;
		}
		if (deltaTime > 0.f)
			totalTime += deltaTime;
		if (totalTime > 10.f) return;

		//then we do the "real" jump to the closest node we have found from the simulation
		PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
		if (pNode != pEndNode && pEndNode && visitedNodes.find(pEndNode) == visitedNodes.end())
		{
			visitedNodes[pEndNode] = true;

			transform.SetRotation(rotation);
			transform.SetTranslation(pNode->GetPosition());
			gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
			gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
			gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
			do
			{
				gamePhysics->OnUpdate(mSimulationStep);
			} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

			direction = pEndNode->GetPosition() - pNode->GetPosition();
			direction[AXIS_Y] = 0;
			Normalize(direction);

			move = direction;
			move *= mMoveSpeed;

			jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
			jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
			jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

			gamePhysics->Fall(mPlayerActor->GetId(), fall);
			gamePhysics->Move(mPlayerActor->GetId(), move);
			gamePhysics->Jump(mPlayerActor->GetId(), jump);
			gamePhysics->OnUpdate(mSimulationStep);

			//falling simulation
			totalTime = 0.f;
			std::vector<std::pair<Vector3<float>, float>> nodePositions;
			while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
			{
				totalTime += mSimulationStep;

				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
				gamePhysics->OnUpdate(mSimulationStep);
			}
			transform = gamePhysics->GetTransform(mPlayerActor->GetId());

			//we store the falling position on ground
			deltaTime = 0.f;
			interpolations.clear();
			gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
			for (auto const& interpolation : interpolations)
			{
				if (interpolation.second) // check we are on ground
				{
					transform = interpolation.first;
					break;
				}

				deltaTime += mSimulationStep / 12.f;
			}
			if (deltaTime > 0.f)
			{
				totalTime += deltaTime;

				nodePositions.push_back({ transform.GetTranslation(), deltaTime });
			}
			if (totalTime > 10.f) return;

			//we store the jump if we find a landing node
			if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
			{
				PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_JUMP, pEndNode, totalTime);
				pNode->AddArc(pNewArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += nodePositions[idx].second;

					if (Length(nodePositions[idx].first - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx].first);

						totalTime = 0.f;
						position = nodePositions[idx].first;
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

void AIPhysicsSimulation::SimulateFall(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Vector3<float> direction, fall, move;
	std::map<PathingNode*, bool> visitedNodes;
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		Transform transform;
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());

		// forward vector
#if defined(GE_USE_MAT_VEC)
		direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
		direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
		direction[AXIS_Y] = 0.f;
		Normalize(direction);

		move = direction;
		move *= mMoveSpeed;
		gamePhysics->Move(mPlayerActor->GetId(), move);
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);

		float deltaTime = 0.f;
		while (gamePhysics->OnGround(mPlayerActor->GetId()) && deltaTime <= 0.1f)
		{
			deltaTime += mSimulationStep;

			gamePhysics->OnUpdate(mSimulationStep);
		}
		// we only consider falling positions near to the edge
		if (deltaTime > 0.1f)
			continue;

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->OnUpdate(mSimulationStep);

		//falling simulation
		float totalTime = deltaTime;
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			gamePhysics->OnUpdate(mSimulationStep);
		}
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());

		//we store the falling position on ground
		deltaTime = 0.f;
		std::vector<std::pair<Transform, bool>> interpolations;
		gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
		for (auto const& interpolation : interpolations)
		{
			if (interpolation.second) // check we are on ground
			{
				transform = interpolation.first;
				break;
			}

			deltaTime += mSimulationStep / 12.f;
		}
		if (deltaTime > 0.f)
			totalTime += deltaTime;
		if (totalTime > 10.f) return;

		//then we do the "real" fall to the closest node we have found from the simulation
		PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
		if (pNode != pEndNode && pEndNode && visitedNodes.find(pEndNode) == visitedNodes.end())
		{
			visitedNodes[pEndNode] = true;

			transform.SetRotation(rotation);
			transform.SetTranslation(pNode->GetPosition());
			gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
			gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
			gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
			do
			{
				gamePhysics->OnUpdate(mSimulationStep);
			} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

			direction = pEndNode->GetPosition() - pNode->GetPosition();
			direction[AXIS_Y] = 0;
			Normalize(direction);

			move = direction;
			move *= mMoveSpeed;
			gamePhysics->Move(mPlayerActor->GetId(), move);
			gamePhysics->Fall(mPlayerActor->GetId(), mGravity);

			deltaTime = 0.f;
			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			std::vector<std::pair<Vector3<float>, float>> nodePositions { { transform.GetTranslation(), deltaTime } };
			while (gamePhysics->OnGround(mPlayerActor->GetId()) && deltaTime <= 0.1f)
			{
				deltaTime += mSimulationStep;

				gamePhysics->OnUpdate(mSimulationStep);
				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
			}
			// we only consider falling positions near to the edge
			if (deltaTime > 0.1f)
				continue;

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

			gamePhysics->Fall(mPlayerActor->GetId(), fall);
			gamePhysics->OnUpdate(mSimulationStep);

			//falling simulation
			totalTime = deltaTime;
			while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
			{
				totalTime += mSimulationStep;

				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
				gamePhysics->OnUpdate(mSimulationStep);
			}
			transform = gamePhysics->GetTransform(mPlayerActor->GetId());

			//we store the falling position on ground
			deltaTime = 0.f;
			interpolations.clear();
			gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
			for (auto const& interpolation : interpolations)
			{
				if (interpolation.second) // check we are on ground
				{
					transform = interpolation.first;
					break;
				}

				deltaTime += mSimulationStep / 12.f;
			}
			if (deltaTime > 0.f)
			{
				totalTime += deltaTime;

				nodePositions.push_back({ transform.GetTranslation(), deltaTime });
			}
			if (totalTime > 10.f) return;

			//we store the falling action if we find a landing node
			if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
			{
				PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_FALL, pEndNode, totalTime);
				pNode->AddArc(pNewArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += nodePositions[idx].second;

					if (Length(nodePositions[idx].first - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx].first);

						totalTime = 0.f;
						position = nodePositions[idx].first;
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

PathingNode* AIPhysicsSimulation::SimulateFall(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	Vector3<float> direction, fall, move;
	Matrix4x4<float> rotation = transform.GetRotation();
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	// forward vector
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0.f;
	Normalize(direction);

	move = direction;
	move *= mMoveSpeed;
	gamePhysics->Move(mPlayerActor->GetId(), move);
	gamePhysics->Fall(mPlayerActor->GetId(), mGravity);

	float deltaTime = 0.f;
	while (gamePhysics->OnGround(mPlayerActor->GetId()) && deltaTime <= 0.1f)
	{
		deltaTime += mSimulationStep;

		gamePhysics->OnUpdate(mSimulationStep);
	}
	// we only consider falling positions near to the edge
	if (deltaTime > 0.1f)
		return NULL;

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Fall(mPlayerActor->GetId(), fall);
	gamePhysics->OnUpdate(mSimulationStep);

	//falling simulation
	float totalTime = deltaTime;
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		totalTime += mSimulationStep;

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		gamePhysics->OnUpdate(mSimulationStep);
	}
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	//we store the falling position on ground
	deltaTime = 0.f;
	std::vector<std::pair<Transform, bool>> interpolations;
	gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
	for (auto const& interpolation : interpolations)
	{
		if (interpolation.second) // check we are on ground
		{
			transform = interpolation.first;
			break;
		}

		deltaTime += mSimulationStep / 12.f;
	}
	if (deltaTime > 0.f)
		totalTime += deltaTime;
	if (totalTime > 10.f) 
		return NULL;

	//we store the falling action if we find a landing node
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode && pEndNode && pNode->FindArc(pEndNode) == NULL)
	{
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->Move(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		direction = pEndNode->GetPosition() - pNode->GetPosition();
		direction[AXIS_Y] = 0;
		Normalize(direction);

		move = direction;
		move *= mMoveSpeed;
		gamePhysics->Move(mPlayerActor->GetId(), move);
		gamePhysics->Fall(mPlayerActor->GetId(), mGravity);

		deltaTime = 0.f;
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		std::vector<std::pair<Vector3<float>, float>> nodePositions{ { transform.GetTranslation(), deltaTime } };
		while (gamePhysics->OnGround(mPlayerActor->GetId()) && deltaTime <= 0.1f)
		{
			deltaTime += mSimulationStep;
			gamePhysics->OnUpdate(mSimulationStep);

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
		}

		// we only consider falling positions near to the edge
		if (deltaTime > 0.1f)
			return NULL;

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->OnUpdate(mSimulationStep);

		//falling simulation
		float totalTime = deltaTime;
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back({ transform.GetTranslation(), mSimulationStep });
			gamePhysics->OnUpdate(mSimulationStep);
		}
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());

		//we store the falling position on ground
		deltaTime = 0.f;
		interpolations.clear();
		gamePhysics->GetInterpolations(mPlayerActor->GetId(), interpolations);
		for (auto const& interpolation : interpolations)
		{
			if (interpolation.second) // check we are on ground
			{
				transform = interpolation.first;
				break;
			}

			deltaTime += mSimulationStep / 12.f;
		}
		if (deltaTime > 0.f)
		{
			totalTime += deltaTime;

			nodePositions.push_back({ transform.GetTranslation(), deltaTime });
		}
		if (totalTime > 10.f)
			return NULL;

		//we store the falling action if we find a landing node
		if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
		{
			PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_FALL, pEndNode, totalTime);
			pNode->AddArc(pNewArc);

			//lets interpolate transitions from the already created arc
			totalTime = 0.f;
			Vector3<float> position = pNode->GetPosition();

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
			{
				totalTime += nodePositions[idx].second;

				if (Length(nodePositions[idx].first - position) >= FLOATING_DISTANCE)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(nodePositions[idx].first);

					totalTime = 0.f;
					position = nodePositions[idx].first;
				}
			}

			if (nodes.size())
			{
				if (totalTime > 0.f)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(pEndNode->GetPosition());
				}
				pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
			}

			return pEndNode;
		}
		else return NULL;
	}

	return pEndNode;
}

#else


void AIPhysicsSimulation::SimulateStanding(ActorId actorId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Transform transform;
	transform.SetTranslation(position);
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	PathingNode* pClosestNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pClosestNode)
	{
		Vector3<float> diff = pClosestNode->GetPosition() - transform.GetTranslation();
		if (Length(diff) <= PATHING_DEFAULT_NODE_TOLERANCE)
		{
			//if we find any node close to our current position we don't add it
			return;
		}
	}

	PathingNode* pNewNode = new PathingNode(GetNewNodeID(), actorId, transform.GetTranslation());
	graph->InsertNode(pNewNode);
	mOpenSet.push_back(pNewNode);
}

void AIPhysicsSimulation::SimulateTriggerTeleport(
	PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Vector3<float> direction;
	Matrix4x4<float> rotation = target.GetRotation();

	// forward vector
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0.f;
	Normalize(direction);

	Transform transform;
	transform.SetTranslation(target.GetTranslation());
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->Move(mPlayerActor->GetId(), direction);

	direction[AXIS_X] *= mFallSpeed[AXIS_X];
	direction[AXIS_Z] *= mFallSpeed[AXIS_Z];
	direction[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Fall(mPlayerActor->GetId(), direction);
	gamePhysics->OnUpdate(mSimulationStep);

	// gravity falling simulation
	float totalTime = mSimulationStep;
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	std::vector<Vector3<float>> nodePositions{ transform.GetTranslation() };
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		gamePhysics->OnUpdate(mSimulationStep);

		totalTime += mSimulationStep;

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		nodePositions.push_back(transform.GetTranslation());
	}

	if (totalTime >= 10.f) return;

	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode)
	{
		if (pEndNode != NULL && pNode->FindArc(pEndNode) == NULL)
		{
			if (pEndNode->GetActorId() == INVALID_ACTOR_ID || pNode->GetActorId() != pEndNode->GetActorId())
			{
				PathingArc* pArc = new PathingArc(GetNewArcID(), AT_TELEPORT, pEndNode, totalTime);
				pNode->AddArc(pArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += mSimulationStep;

					if (Length(nodePositions[idx] - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx]);

						totalTime = 0.f;
						position = nodePositions[idx];
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

void AIPhysicsSimulation::SimulateTriggerPush(
	PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Transform transform;
	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	Vector3<float> direction, jump, fall;
	direction = target.GetTranslation() - transform.GetTranslation();
	float push = mPushSpeed[AXIS_Y] + direction[AXIS_Y] * 0.01f;

	direction[AXIS_Y] = 0;
	Normalize(direction);

	jump[AXIS_X] = direction[AXIS_X] * mPushSpeed[AXIS_X];
	jump[AXIS_Z] = direction[AXIS_Z] * mPushSpeed[AXIS_Z];
	jump[AXIS_Y] = push;

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Fall(mPlayerActor->GetId(), fall);
	gamePhysics->Move(mPlayerActor->GetId(), jump);
	gamePhysics->Jump(mPlayerActor->GetId(), jump);
	gamePhysics->OnUpdate(mSimulationStep);

	// gravity falling simulation
	float totalTime = mSimulationStep;
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	std::vector<Vector3<float>> nodePositions{ transform.GetTranslation() };
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		gamePhysics->OnUpdate(mSimulationStep);

		totalTime += mSimulationStep;

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		nodePositions.push_back(transform.GetTranslation());
	}

	if (totalTime >= 10.f) return;

	//we store the jump if we find a landing node
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode)
	{
		if (pEndNode != NULL && pNode->FindArc(pEndNode) == NULL)
		{
			if (pEndNode->GetActorId() == INVALID_ACTOR_ID || pNode->GetActorId() != pEndNode->GetActorId())
			{
				PathingArc* pArc = new PathingArc(GetNewArcID(), AT_PUSH, pEndNode, totalTime);
				pNode->AddArc(pArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += mSimulationStep;

					if (Length(nodePositions[idx] - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx]);

						totalTime = 0.f;
						position = nodePositions[idx];
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

float FindClosestMovement(std::vector<std::pair<Transform, bool>>& movements, const Vector3<float>& pos)
{
	float length = FLT_MAX;
	std::vector<std::pair<Transform, bool>>::iterator it;
	for (it = movements.begin(); it != movements.end(); it++)
	{
		Vector3<float> diff = pos - (*it).first.GetTranslation();
		if (Length(diff) < length)
			length = Length(diff);
	}

	return length;
}

// Check penetration
bool CheckPenetration(ActorId playerId, const Vector3<float>& translation)
{
	Transform transform;
	transform.SetTranslation(translation);
	GameLogic::Get()->GetGamePhysics()->SetTransform(playerId, transform);
	return GameLogic::Get()->GetGamePhysics()->CheckPenetration(playerId);
}

// Cliff control
bool Cliff(ActorId playerId, const Vector3<float>& translation)
{
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		// This will give us the "look at" vector 
		// in world space - we'll use that to move.
		Vector4<float> atWorld = Vector4<float>::Unit(AXIS_X); // forward vector
#if defined(GE_USE_MAT_VEC)
		atWorld = rotation * atWorld;
#else
		atWorld = atWorld * rotation;
#endif

		Vector3<float> position = translation + HProject(atWorld * 10.f);

		Transform start;
		start.SetRotation(rotation);
		start.SetTranslation(position);

		Transform end;
		end.SetRotation(rotation);
		end.SetTranslation(position - Vector3<float>::Unit(AXIS_Y) * 300.f);

		Vector3<float> collision, collisionNormal;
		collision = end.GetTranslation();
		ActorId actorId = GameLogic::Get()->GetGamePhysics()->CastRay(
			start.GetTranslation(), end.GetTranslation(), collision, collisionNormal, playerId);

		//Check whether we are close to a cliff
		//printf("weight cliff %f \n", abs(collision[AXIS_Y] - position[AXIS_Y]));
		if (abs(collision[AXIS_Y] - position[AXIS_Y]) > 60.f)
			return true;
	}
	return false;
}

PathingNode* AIPhysicsSimulation::SimulateMove(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	Matrix4x4<float> rotation = transform.GetRotation();
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	//create movement interpolations on the ground and air
	std::vector<std::pair<Transform, bool>> movements;
	PathingNode* pCurrentNode = pNode;
	bool isOnGround = false;

	do
	{
		if (!gamePhysics->OnGround(mPlayerActor->GetId()))
		{
			isOnGround = false;
			movements.push_back({ transform, isOnGround });

			Vector3<float> direction, fall;
#if defined(GE_USE_MAT_VEC)
			direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
			direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
			direction[AXIS_Y] = 0;
			Normalize(direction);

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

			gamePhysics->Fall(mPlayerActor->GetId(), fall);
			gamePhysics->OnUpdate(mSimulationStep);

			// gravity falling simulation
			float totalTime = mSimulationStep;
			while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
			{
				gamePhysics->OnUpdate(mSimulationStep);

				totalTime += mSimulationStep;
			}

			if (totalTime >= 10.f)
				break;
		}

		if (!isOnGround)
		{
			isOnGround = true;
			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			movements.push_back({ transform, isOnGround });
		}
		else gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);

		PathingNode* pClosestNode = FindClosestNode(
			mPlayerActor->GetId(), graph, PATHING_MOVEMENT_NODE_TOLERANCE, false);
		if (pClosestNode && pCurrentNode != pClosestNode)
		{
			//if we find any node close to our current position we stop
			break;
		}

		Vector3<float> direction; // forward vector
#if defined(GE_USE_MAT_VEC)
		direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
		direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
		direction[AXIS_Y] = 0;
		Normalize(direction);

		gamePhysics->Move(mPlayerActor->GetId(), direction * mMoveSpeed);
		gamePhysics->SetGravity(mPlayerActor->GetId(), mGravity);
		gamePhysics->OnUpdate(mSimulationStep);

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	} while (FindClosestMovement(movements, transform.GetTranslation()) >= 4.f); // stalling is a break condition

	if (!movements.empty())
	{
		float deltaTime = 0.f, totalTime = 0.f;
		std::vector<std::pair<Transform, bool>>::iterator itMove = movements.begin();
		std::vector<Vector3<float>> positions{ (*itMove).first.GetTranslation() };
		std::vector<float> weights{ 0 };
		itMove++;

		for (; itMove != movements.end(); itMove++)
		{
			if ((*itMove).second)	// check if we are on ground
			{
				if (!pCurrentNode)
				{
					PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
					Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
					if (Length(diff) >= GROUND_DISTANCE)
					{
						if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
							!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
						{
							PathingNode* pNewNode = new PathingNode(
								GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());
							graph->InsertNode(pNewNode);
							mOpenSet.push_back(pNewNode);
							pCurrentNode = pNewNode;

							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pNewNode->GetPosition() };
							weights = { 0 };
						}
					}
					else if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
					{
						pCurrentNode = pClosestNode;
						deltaTime = 0.f;
						totalTime = 0.f;

						positions = { pClosestNode->GetPosition() };
						weights = { 0 };
					}

					continue;
				}

				totalTime += mSimulationStep / 4.f;
				deltaTime += mSimulationStep / 4.f;

				if (Length((*itMove).first.GetTranslation() - positions.back()) >= GROUND_DISTANCE)
				{
					weights.push_back(deltaTime);
					positions.push_back((*itMove).first.GetTranslation());

					deltaTime = 0.f;
				}

				PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
				Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
				if (Length(diff) >= GROUND_DISTANCE)
				{
					Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

					Transform start;
					start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
					Transform end;
					end.SetTranslation((*itMove).first.GetTranslation() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

					gamePhysics->SetTransform(mPlayerActor->GetId(), start);
					gamePhysics->OnUpdate(mSimulationStep);

					std::optional<Vector3<float>> collision, collisionNormal;
					ActorId actorId = gamePhysics->ConvexSweep(
						mPlayerActor->GetId(), start, end, collision, collisionNormal);
					if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
					{
						if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
							!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
						{
							PathingNode* pNewNode = new PathingNode(
								GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());

							//we only consider arcs within certain time range
							if (totalTime >= 0.02f && totalTime <= 0.2f)
							{
								PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pNewNode, totalTime);
								pCurrentNode->AddArc(pNewArc);

								if (positions.size())
								{
									if (deltaTime > 0.f)
									{
										weights.push_back(deltaTime);
										positions.push_back((*itMove).first.GetTranslation());
									}

									//lets interpolate transitions from the already created arc
									pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
								}
							}

							graph->InsertNode(pNewNode);
							mOpenSet.push_back(pNewNode);
							pCurrentNode = pNewNode;

							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pNewNode->GetPosition() };
							weights = { 0 };
						}
						else
						{
							continue; //we don't create a new node if it doesn't pass the cliff control or penetration checking
						}
					}
					else
					{
						break; //we stop processing movements if we find collision
					}
				}
				else if (pCurrentNode != pClosestNode)
				{
					if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
					{
						if (pCurrentNode->FindArc(pClosestNode) == NULL)
						{
							Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

							Transform start;
							start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
							Transform end;
							end.SetTranslation(pClosestNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

							gamePhysics->SetTransform(mPlayerActor->GetId(), start);
							gamePhysics->OnUpdate(mSimulationStep);

							std::optional<Vector3<float>> collision, collisionNormal;
							ActorId actorId = gamePhysics->ConvexSweep(
								mPlayerActor->GetId(), start, end, collision, collisionNormal);
							if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
							{
								//we only consider arcs within certain time range
								if (totalTime >= 0.02f && totalTime <= 0.2f)
								{
									PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pClosestNode, totalTime);
									pCurrentNode->AddArc(pNewArc);

									if (positions.size())
									{
										if (deltaTime > 0.f)
										{
											weights.push_back(deltaTime);
											positions.push_back(pClosestNode->GetPosition());
										}

										//lets interpolate transitions from the already created arc
										pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
									}
								}
							}
							else
							{
								break; //we stop processing movements if we find collision
							}
						}
						pCurrentNode = pClosestNode;

						deltaTime = 0.f;
						totalTime = 0.f;

						positions = { pClosestNode->GetPosition() };
						weights = { 0 };
					}
				}
			}
			else pCurrentNode = NULL;
		}
	}

	return pCurrentNode;
}

void AIPhysicsSimulation::SimulateMove(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	// nodes closed to falling position
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		Transform transform;
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		//create movement interpolations on the ground and air
		std::vector<std::pair<Transform, bool>> movements;
		PathingNode* pCurrentNode = pNode;
		bool isOnGround = false;

		do
		{
			if (!gamePhysics->OnGround(mPlayerActor->GetId()))
			{
				isOnGround = false;
				movements.push_back({ transform, isOnGround });

				Vector3<float> direction, fall;
#if defined(GE_USE_MAT_VEC)
				direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
				direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
				direction[AXIS_Y] = 0;
				Normalize(direction);

				fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
				fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
				fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

				gamePhysics->Fall(mPlayerActor->GetId(), fall);
				gamePhysics->OnUpdate(mSimulationStep);

				// gravity falling simulation
				float totalTime = mSimulationStep;
				while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
				{
					gamePhysics->OnUpdate(mSimulationStep);

					totalTime += mSimulationStep;
				}

				if (totalTime >= 10.f)
					break;
			}

			if (!isOnGround)
			{
				isOnGround = true;
				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				movements.push_back({ transform, isOnGround });
			}
			else gamePhysics->GetInterpolations(mPlayerActor->GetId(), movements);

			PathingNode* pClosestNode = FindClosestNode(
				mPlayerActor->GetId(), graph, PATHING_MOVEMENT_NODE_TOLERANCE, false);
			if (pClosestNode && pCurrentNode != pClosestNode)
			{
				//if we find any node close to our current position we stop
				break;
			}

			Vector3<float> direction; // forward vector
#if defined(GE_USE_MAT_VEC)
			direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
			direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
			direction[AXIS_Y] = 0;
			Normalize(direction);

			gamePhysics->Move(mPlayerActor->GetId(), direction * mMoveSpeed);
			gamePhysics->SetGravity(mPlayerActor->GetId(), mGravity);
			gamePhysics->OnUpdate(mSimulationStep);

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		} while (FindClosestMovement(movements, transform.GetTranslation()) >= 4.f); // stalling is a break condition

		if (!movements.empty())
		{
			float deltaTime = 0.f, totalTime = 0.f;
			std::vector<std::pair<Transform, bool>>::iterator itMove = movements.begin();
			std::vector<Vector3<float>> positions{ (*itMove).first.GetTranslation() };
			std::vector<float> weights{ 0 };
			itMove++;

			for (; itMove != movements.end(); itMove++)
			{
				if ((*itMove).second)	// check if we are on ground
				{
					if (!pCurrentNode)
					{
						PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
						Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
						if (Length(diff) >= GROUND_DISTANCE)
						{
							if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
								!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
							{
								PathingNode* pNewNode = new PathingNode(
									GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());
								graph->InsertNode(pNewNode);
								mOpenSet.push_back(pNewNode);
								pCurrentNode = pNewNode;

								deltaTime = 0.f;
								totalTime = 0.f;

								positions = { pNewNode->GetPosition() };
								weights = { 0 };
							}
						}
						else if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
						{
							pCurrentNode = pClosestNode;
							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pClosestNode->GetPosition() };
							weights = { 0 };
						}

						continue;
					}

					totalTime += mSimulationStep / 4.f;
					deltaTime += mSimulationStep / 4.f;

					if (Length((*itMove).first.GetTranslation() - positions.back()) >= GROUND_DISTANCE)
					{
						weights.push_back(deltaTime);
						positions.push_back((*itMove).first.GetTranslation());

						deltaTime = 0.f;
					}

					PathingNode* pClosestNode = graph->FindClosestNode((*itMove).first.GetTranslation(), false);
					Vector3<float> diff = pClosestNode->GetPosition() - (*itMove).first.GetTranslation();
					if (Length(diff) >= GROUND_DISTANCE)
					{
						Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

						Transform start;
						start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
						Transform end;
						end.SetTranslation((*itMove).first.GetTranslation() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

						gamePhysics->SetTransform(mPlayerActor->GetId(), start);
						gamePhysics->OnUpdate(mSimulationStep);

						std::optional<Vector3<float>> collision, collisionNormal;
						ActorId actorId = gamePhysics->ConvexSweep(
							mPlayerActor->GetId(), start, end, collision, collisionNormal);
						if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
						{
							if (!Cliff(mPlayerActor->GetId(), (*itMove).first.GetTranslation()) &&
								!CheckPenetration(mPlayerActor->GetId(), (*itMove).first.GetTranslation()))
							{
								PathingNode* pNewNode = new PathingNode(
									GetNewNodeID(), INVALID_ACTOR_ID, (*itMove).first.GetTranslation());

								//we only consider arcs within certain time range
								if (totalTime >= 0.02f && totalTime <= 0.2f)
								{
									PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pNewNode, totalTime);
									pCurrentNode->AddArc(pNewArc);

									if (positions.size())
									{
										if (deltaTime > 0.f)
										{
											weights.push_back(deltaTime);
											positions.push_back((*itMove).first.GetTranslation());
										}

										//lets interpolate transitions from the already created arc
										pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
									}
								}

								graph->InsertNode(pNewNode);
								mOpenSet.push_back(pNewNode);
								pCurrentNode = pNewNode;

								deltaTime = 0.f;
								totalTime = 0.f;

								positions = { pNewNode->GetPosition() };
								weights = { 0 };
							}
							else
							{
								continue; //we don't create a new node if it doesn't pass the cliff control or penetration checking
							}
						}
						else
						{
							break; //we stop processing movements if we find collision
						}
					}
					else if (pCurrentNode != pClosestNode)
					{
						if (Length(diff) <= PATHING_MOVEMENT_NODE_TOLERANCE)
						{
							if (pCurrentNode->FindArc(pClosestNode) == NULL)
							{
								Vector3<float> scale = gamePhysics->GetScale(mPlayerActor->GetId()) / 2.f;

								Transform start;
								start.SetTranslation(pCurrentNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));
								Transform end;
								end.SetTranslation(pClosestNode->GetPosition() + scale[AXIS_Y] * Vector3<float>::Unit(AXIS_Y));

								gamePhysics->SetTransform(mPlayerActor->GetId(), start);
								gamePhysics->OnUpdate(mSimulationStep);

								std::optional<Vector3<float>>  collision, collisionNormal;
								ActorId actorId = gamePhysics->ConvexSweep(
									mPlayerActor->GetId(), start, end, collision, collisionNormal);
								if (!collision.has_value() || actorId != INVALID_ACTOR_ID)
								{
									//we only consider arcs within certain time range
									if (totalTime >= 0.02f && totalTime <= 0.2f)
									{
										PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_MOVE, pClosestNode, totalTime);
										pCurrentNode->AddArc(pNewArc);

										if (positions.size())
										{
											if (deltaTime > 0.f)
											{
												weights.push_back(deltaTime);
												positions.push_back(pClosestNode->GetPosition());
											}

											//lets interpolate transitions from the already created arc
											pNewArc->AddTransition(new PathingTransition({ pCurrentNode }, weights, positions));
										}
									}
								}
								else
								{
									break; //we stop processing movements if we find collision
								}
							}


							pCurrentNode = pClosestNode;

							deltaTime = 0.f;
							totalTime = 0.f;

							positions = { pClosestNode->GetPosition() };
							weights = { 0 };
						}
					}
				}
				else pCurrentNode = NULL;
			}
		}
	}
}

PathingNode* AIPhysicsSimulation::SimulateJump(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	Vector3<float> direction, jump, fall;
	Matrix4x4<float> rotation = transform.GetRotation();
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	// forward vector
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0.f;
	Normalize(direction);

	jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
	jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
	jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Fall(mPlayerActor->GetId(), fall);
	gamePhysics->Move(mPlayerActor->GetId(), jump);
	gamePhysics->Jump(mPlayerActor->GetId(), jump);
	gamePhysics->OnUpdate(mSimulationStep);

	// gravity falling simulation
	float totalTime = mSimulationStep;
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		gamePhysics->OnUpdate(mSimulationStep);

		totalTime += mSimulationStep;
	}

	if (totalTime > 10.f)
		return NULL;

	//then we do the "real" jump to the closest node we have found from the simulation
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode && pEndNode && pNode->FindArc(pEndNode) == NULL)
	{
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		direction = pEndNode->GetPosition() - pNode->GetPosition();
		direction[AXIS_Y] = 0;
		Normalize(direction);

		jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
		jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
		jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->Move(mPlayerActor->GetId(), jump);
		gamePhysics->Jump(mPlayerActor->GetId(), jump);
		gamePhysics->OnUpdate(mSimulationStep);

		// gravity falling simulation
		totalTime = mSimulationStep;
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		std::vector<Vector3<float>> nodePositions{ transform.GetTranslation() };
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			gamePhysics->OnUpdate(mSimulationStep);

			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back(transform.GetTranslation());
		}

		if (totalTime > 10.f)
			return NULL;

		//we store the jump if we find a landing node
		if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
		{
			PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_JUMP, pEndNode, totalTime);
			pNode->AddArc(pNewArc);

			//lets interpolate transitions from the already created arc
			totalTime = 0.f;
			Vector3<float> position = pNode->GetPosition();

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
			{
				totalTime += mSimulationStep;

				if (Length(nodePositions[idx] - position) >= FLOATING_DISTANCE)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(nodePositions[idx]);

					totalTime = 0.f;
					position = nodePositions[idx];
				}
			}

			if (nodes.size())
			{
				if (totalTime > 0.f)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(pEndNode->GetPosition());
				}
				pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
			}

			return pEndNode;
		}
		else return NULL;
	}

	return pEndNode;
}

void AIPhysicsSimulation::SimulateJump(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Vector3<float> direction, jump, fall;
	std::map<PathingNode*, bool> visitedNodes;
	// we do the jumping simulation by performing uniform jumps around the character
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		Transform transform;
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		// forward vector
#if defined(GE_USE_MAT_VEC)
		direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
		direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
		direction[AXIS_Y] = 0.f;
		Normalize(direction);

		jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
		jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
		jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->Move(mPlayerActor->GetId(), jump);
		gamePhysics->Jump(mPlayerActor->GetId(), jump);
		gamePhysics->OnUpdate(mSimulationStep);

		// gravity falling simulation
		float totalTime = mSimulationStep;
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			gamePhysics->OnUpdate(mSimulationStep);

			totalTime += mSimulationStep;
		}

		if (totalTime > 10.f) continue;

		//then we do the "real" jump to the closest node we have found from the simulation
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
		if (pNode != pEndNode && pEndNode && visitedNodes.find(pEndNode) == visitedNodes.end())
		{
			visitedNodes[pEndNode] = true;

			transform.SetRotation(rotation);
			transform.SetTranslation(pNode->GetPosition());
			gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
			gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
			gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
			do
			{
				gamePhysics->OnUpdate(mSimulationStep);
			} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

			direction = pEndNode->GetPosition() - pNode->GetPosition();
			direction[AXIS_Y] = 0;
			Normalize(direction);

			jump[AXIS_X] = direction[AXIS_X] * mJumpSpeed[AXIS_X];
			jump[AXIS_Z] = direction[AXIS_Z] * mJumpSpeed[AXIS_Z];
			jump[AXIS_Y] = mJumpSpeed[AXIS_Y];

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

			gamePhysics->Fall(mPlayerActor->GetId(), fall);
			gamePhysics->Move(mPlayerActor->GetId(), jump);
			gamePhysics->Jump(mPlayerActor->GetId(), jump);
			gamePhysics->OnUpdate(mSimulationStep);

			// gravity falling simulation
			totalTime = mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			std::vector<Vector3<float>> nodePositions{ transform.GetTranslation() };
			while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
			{
				gamePhysics->OnUpdate(mSimulationStep);

				totalTime += mSimulationStep;

				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				nodePositions.push_back(transform.GetTranslation());
			}

			if (totalTime > 10.f) continue;

			//we store the jump if we find a landing node
			if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
			{
				PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_JUMP, pEndNode, totalTime);
				pNode->AddArc(pNewArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += mSimulationStep;

					if (Length(nodePositions[idx] - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx]);

						totalTime = 0.f;
						position = nodePositions[idx];
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

void AIPhysicsSimulation::SimulateFall(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	Vector3<float> direction, fall;
	std::map<PathingNode*, bool> visitedNodes;
	// we do the fallen simulation by performing uniform falls around the character
	for (int angle = 0; angle < 360; angle += 5)
	{
		Matrix4x4<float> rotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));

		Transform transform;
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		transform = gamePhysics->GetTransform(mPlayerActor->GetId());

		// forward vector
#if defined(GE_USE_MAT_VEC)
		direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
		direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
		direction[AXIS_Y] = 0;
		Normalize(direction);

		gamePhysics->Move(mPlayerActor->GetId(), direction * mMoveSpeed);
		gamePhysics->SetGravity(mPlayerActor->GetId(), mGravity);
		gamePhysics->OnUpdate(mSimulationStep);

		float moveTime = mSimulationStep;
		while (gamePhysics->OnGround(mPlayerActor->GetId()) && moveTime <= 0.1f)
		{
			gamePhysics->OnUpdate(mSimulationStep);

			moveTime += mSimulationStep;
		}

		// we only consider falling positions near to the edge
		if (moveTime > 0.1f)
			continue;

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->OnUpdate(mSimulationStep);

		// gravity falling simulation
		float totalTime = mSimulationStep;
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			gamePhysics->OnUpdate(mSimulationStep);

			totalTime += mSimulationStep;
		}

		if (totalTime > 10.f) continue;

		//then we do the "real" fall to the closest node we have found from the simulation
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
		if (pNode != pEndNode && pEndNode && visitedNodes.find(pEndNode) == visitedNodes.end())
		{
			visitedNodes[pEndNode] = true;

			transform.SetRotation(rotation);
			transform.SetTranslation(pNode->GetPosition());
			gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
			gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
			gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
			do
			{
				gamePhysics->OnUpdate(mSimulationStep);
			} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

			direction = pEndNode->GetPosition() - pNode->GetPosition();
			direction[AXIS_Y] = 0;
			Normalize(direction);

			moveTime = 0.f;
			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			std::vector<Vector3<float>> nodePositions{ transform.GetTranslation() };
			while (gamePhysics->OnGround(mPlayerActor->GetId()) && moveTime <= 0.1f)
			{
				gamePhysics->Move(mPlayerActor->GetId(), direction * mMoveSpeed);
				gamePhysics->SetGravity(mPlayerActor->GetId(), mGravity);
				gamePhysics->OnUpdate(mSimulationStep);

				moveTime += mSimulationStep;

				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				nodePositions.push_back(transform.GetTranslation());
			}

			// we only consider falling positions near to the edge
			if (moveTime > 0.1f)
				continue;

			fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
			fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
			fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

			gamePhysics->Fall(mPlayerActor->GetId(), fall);
			gamePhysics->OnUpdate(mSimulationStep);

			// gravity falling simulation
			float totalTime = mSimulationStep;
			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back(transform.GetTranslation());
			while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
			{
				gamePhysics->OnUpdate(mSimulationStep);

				totalTime += mSimulationStep;

				transform = gamePhysics->GetTransform(mPlayerActor->GetId());
				nodePositions.push_back(transform.GetTranslation());
			}

			if (totalTime > 10.f) continue;

			//we store the falling action if we find a landing node
			if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
			{
				PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_FALL, pEndNode, totalTime);
				pNode->AddArc(pNewArc);

				//lets interpolate transitions from the already created arc
				totalTime = 0.f;
				Vector3<float> position = pNode->GetPosition();

				std::vector<float> weights;
				std::vector<PathingNode*> nodes;
				std::vector<Vector3<float>> positions;
				for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
				{
					totalTime += mSimulationStep;

					if (Length(nodePositions[idx] - position) >= FLOATING_DISTANCE)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(nodePositions[idx]);

						totalTime = 0.f;
						position = nodePositions[idx];
					}
				}

				if (nodes.size())
				{
					if (totalTime > 0.f)
					{
						nodes.push_back(pNode);
						weights.push_back(totalTime);
						positions.push_back(pEndNode->GetPosition());
					}
					pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
				}
			}
		}
	}
}

PathingNode* AIPhysicsSimulation::SimulateFall(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	transform.SetTranslation(pNode->GetPosition());
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	do
	{
		gamePhysics->OnUpdate(mSimulationStep);
	} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

	Vector3<float> direction, fall;
	Matrix4x4<float> rotation = transform.GetRotation();
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	// forward vector
#if defined(GE_USE_MAT_VEC)
	direction = HProject(rotation * Vector4<float>::Unit(AXIS_X));
#else
	direction = HProject(Vector4<float>::Unit(AXIS_X) * rotation);
#endif
	direction[AXIS_Y] = 0.f;
	Normalize(direction);

	gamePhysics->Move(mPlayerActor->GetId(), direction * mMoveSpeed);
	gamePhysics->SetGravity(mPlayerActor->GetId(), mGravity);
	gamePhysics->OnUpdate(mSimulationStep);

	float moveTime = mSimulationStep;
	while (gamePhysics->OnGround(mPlayerActor->GetId()) && moveTime <= 0.1f)
	{
		gamePhysics->OnUpdate(mSimulationStep);

		moveTime += mSimulationStep;
	}

	// we only consider falling positions near to the edge
	if (moveTime > 0.1f)
		return NULL;

	fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
	fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

	gamePhysics->Fall(mPlayerActor->GetId(), fall);
	gamePhysics->OnUpdate(mSimulationStep);

	// gravity falling simulation
	float totalTime = mSimulationStep;
	while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
	{
		gamePhysics->OnUpdate(mSimulationStep);

		totalTime += mSimulationStep;
	}

	if (totalTime > 10.f) 
		return NULL;

	//we store the falling action if we find a landing node
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation(), false);
	if (pNode != pEndNode && pEndNode && pNode->FindArc(pEndNode) == NULL)
	{
		transform.SetRotation(rotation);
		transform.SetTranslation(pNode->GetPosition());
		gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
		gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
		gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
		do
		{
			gamePhysics->OnUpdate(mSimulationStep);
		} while (!gamePhysics->OnGround(mPlayerActor->GetId()));

		direction = pEndNode->GetPosition() - pNode->GetPosition();
		direction[AXIS_Y] = 0;
		Normalize(direction);

		moveTime = 0.f;
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		std::vector<Vector3<float>> nodePositions{ transform.GetTranslation() };
		while (gamePhysics->OnGround(mPlayerActor->GetId()) && moveTime <= 0.1f)
		{
			gamePhysics->Move(mPlayerActor->GetId(), direction * mMoveSpeed);
			gamePhysics->SetGravity(mPlayerActor->GetId(), mGravity);
			gamePhysics->OnUpdate(mSimulationStep);

			moveTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back(transform.GetTranslation());
		}

		// we only consider falling positions near to the edge
		if (moveTime > 0.1f) 
			return NULL;

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->OnUpdate(mSimulationStep);

		// gravity falling simulation
		float totalTime = mSimulationStep;
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		nodePositions.push_back(transform.GetTranslation());
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			gamePhysics->OnUpdate(mSimulationStep);

			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back(transform.GetTranslation());
		}

		if (totalTime > 10.f) 
			return NULL;

		//we store the falling action if we find a landing node
		if (Length(pEndNode->GetPosition() - transform.GetTranslation()) <= PATHING_DEFAULT_NODE_TOLERANCE)
		{
			PathingArc* pNewArc = new PathingArc(GetNewArcID(), AT_FALL, pEndNode, totalTime);
			pNode->AddArc(pNewArc);

			//lets interpolate transitions from the already created arc
			totalTime = 0.f;
			Vector3<float> position = pNode->GetPosition();

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (unsigned int idx = 0; idx < nodePositions.size(); idx++)
			{
				totalTime += mSimulationStep;

				if (Length(nodePositions[idx] - position) >= FLOATING_DISTANCE)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(nodePositions[idx]);

					totalTime = 0.f;
					position = nodePositions[idx];
				}
			}

			if (nodes.size())
			{
				if (totalTime > 0.f)
				{
					nodes.push_back(pNode);
					weights.push_back(totalTime);
					positions.push_back(pEndNode->GetPosition());
				}
				pNewArc->AddTransition(new PathingTransition(nodes, weights, positions));
			}
			return pEndNode;
		}
		else return NULL;
	}
	return pEndNode;
}

#endif


Vector3<float> AIPhysicsSimulation::RayCollisionDetection(const Vector3<float>& startPos, const Vector3<float>& collisionPos)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	//set player relative to pivoting eye
	Vector3<float> start = startPos;
	start[2] += mPlayerActor->GetState().viewHeight;

	Vector3<float> end = collisionPos + (float)mPlayerActor->GetState().viewHeight * Vector3<float>::Unit(AXIS_Y);

	std::vector<ActorId> collisionActors;
	std::vector<Vector3<float>> collisions, collisionNormals;
	gamePhysics->CastRay(start, end, collisionActors, collisions, collisionNormals, mPlayerActor->GetId());

	Vector3<float> collision = NULL;
	for (unsigned int i = 0; i < collisionActors.size(); i++)
		if (collisionActors[i] == INVALID_ACTOR_ID)
			collision = collisions[i];

	return collision;
}

void AIPhysicsSimulation::SimulateVisibility(std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	std::mutex mutex;

	// first we get visibility info from every node by raycasting
	const PathingNodeMap& pathingNodes = graph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* pathNode = (*it).second;
		pathNode->RemoveVisibleNodes();

		Vector3<float> origin = pathNode->GetPosition();
		origin[AXIS_Y] += mPlayerActor->GetState().viewHeight;

		Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
		{
			PathingNode* visibleNode = node.second;
			Vector3<float> end = visibleNode->GetPosition() +
				(float)mPlayerActor->GetState().viewHeight * Vector3<float>::Unit(AXIS_Y);

			std::vector<ActorId> collisionActors;
			std::vector<Vector3<float>> collisions, collisionNormals;
			gamePhysics->CastRay(origin, end, collisionActors, collisions, collisionNormals, mPlayerActor->GetId());

			Vector3<float> collision = NULL;
			for (unsigned int i = 0; i < collisionActors.size(); i++)
				if (collisionActors[i] == INVALID_ACTOR_ID)
					collision = collisions[i];

			if (collision == NULL)
			{
				mutex.lock();
				pathNode->AddVisibleNode(visibleNode, Length(visibleNode->GetPosition() - pathNode->GetPosition()));
				mutex.unlock();
			}
		});
	}
}

void AIPhysicsSimulation::PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerEnter> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerEnter>(pEventData);

	std::shared_ptr<PlayerActor> pPlayerActor(std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(pCastEventData->GetOtherActor()).lock()));
	if (!pPlayerActor) return;

	std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(pCastEventData->GetTriggerId()).lock());

	if (mPlayerActor && mPlayerActor->GetId() == pPlayerActor->GetId())
	{
		std::shared_ptr<PhysicComponent> pPhysicComponent(
			mPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (pPhysicComponent->OnGround())
		{
			Vector3<float> position = pPhysicComponent->GetPosition();
			mActorPositions[position] = pItemActor->GetId();
		}
	}
}

void AIPhysicsSimulation::PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerLeave> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerLeave>(pEventData);
}

void AIPhysicsSimulation::PhysicsCollisionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysCollision> pCastEventData =
		std::static_pointer_cast<EventDataPhysCollision>(pEventData);
}

void AIPhysicsSimulation::PhysicsSeparationDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysSeparation> pCastEventData =
		std::static_pointer_cast<EventDataPhysSeparation>(pEventData);

}

void AIPhysicsSimulation::RegisterAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsTriggerEnterDelegate),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsTriggerLeaveDelegate),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsCollisionDelegate),
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsSeparationDelegate),
		EventDataPhysSeparation::skEventType);
}

void AIPhysicsSimulation::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsTriggerEnterDelegate),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsTriggerLeaveDelegate),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsCollisionDelegate),
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &AIPhysicsSimulation::PhysicsSeparationDelegate),
		EventDataPhysSeparation::skEventType);
}