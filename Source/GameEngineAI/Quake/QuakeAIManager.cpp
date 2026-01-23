/*******************************************************
 * QuakeAIManager.cpp : AI Manager class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#include "QuakeAIManager.h"

#include "Core/OS/OS.h"
#include "Core/Logger/Logger.h"

#include "Core/IO/XmlResource.h"

#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "Physic/PhysicEventListener.h"

#include "Games/Actors/LocationTarget.h"

#include "QuakeEvents.h"
#include "QuakeAIView.h"
#include "QuakeView.h"
#include "QuakeApp.h"
#include "Quake.h"

#define MAX_DAMAGE 300

#define GROUND_DISTANCE 16.f
#define FLOATING_DISTANCE 32.f

#define ENGAGE_THRESHOLD -0.05f

//--------------------------------------------------------------------------------------------------------
// AIPlanNode
//--------------------------------------------------------------------------------------------------------
AIPlanNode::AIPlanNode(PathingNode* pNode, PathingActor* pActor, AIPlanNode* pPrevNode)
{
	LogAssert(pNode, "Invalid node");

	mPathingNode = pNode;
	mPathingActor = pActor;
	mPrevNode = pPrevNode;  // NULL is a valid value, though it should only be NULL for the start node

	mClosed = false;
	mWeight = mPrevNode ? pPrevNode->GetWeight() : 0.f;
	mActors = mPrevNode ? pPrevNode->GetActors() : mActors;
	if (mPathingActor)
	{
		mWeight += CalculatePathCost(mPathingNode, mPathingActor);

		if (mActors.find(mPathingActor->GetActor()) != mActors.end())
		{
			if (mActors[mPathingActor->GetActor()] > mWeight)
				mActors[mPathingActor->GetActor()] = mWeight;
		}
		else mActors[mPathingActor->GetActor()] = mWeight;
	}
}

void AIPlanNode::UpdateNode(PathingNode* pNode, PathingActor* pActor, AIPlanNode* pPrev)
{
	LogAssert(pNode, "Invalid node");
	LogAssert(pActor, "Invalid actor");

	mPathingNode = pNode;
	mPathingActor = pActor;
	mPrevNode = pPrev;

	mWeight = pPrev->GetWeight();
	mActors = mPrevNode ? pPrev->GetActors() : mActors;
	if (mPathingActor)
	{
		mWeight += CalculatePathCost(mPathingNode, mPathingActor);

		if (mActors.find(mPathingActor->GetActor()) != mActors.end())
		{
			if (mActors[mPathingActor->GetActor()] > mWeight)
				mActors[mPathingActor->GetActor()] = mWeight;
		}
		else mActors[mPathingActor->GetActor()] = mWeight;
	}
}

float AIPlanNode::CalculatePathCost(PathingNode* pNode, PathingActor* pActor)
{
	float weight = 0.f;
	while (pNode != pActor->GetTarget())
	{
		pActor = pNode->FindActor(pActor->GetType(), pActor->GetTarget());
		PathingArc* pArc = pNode->FindArc(pActor->GetNode());
		weight += pArc->GetWeight();

		pNode = pArc->GetNode();
	}
	return weight;
}

bool AIPlanNode::ContainActors(PathingActorVec pathingActors)
{
	if (mActors.size() == pathingActors.size())
	{
		for (PathingActor* pathingActor : pathingActors)
			if (mActors.find(pathingActor->GetActor()) == mActors.end())
				return false;

		return true;
	}
	else return false;
}

PathingActorVec AIPlanNode::GetPathingActors(void)
{
	PathingActorVec pathingActors;

	AIPlanNode* pNode = this;
	while (pNode)
	{
		if (pNode->GetPathingActor() != NULL)
			pathingActors.insert(pathingActors.begin(), pNode->GetPathingActor());

		pNode = pNode->GetPrev();
	}

	return pathingActors;
}

//--------------------------------------------------------------------------------------------------------
// AIFinder
//--------------------------------------------------------------------------------------------------------
AIFinder::AIFinder(void)
{

}

AIFinder::~AIFinder(void)
{
	Destroy();
}

void AIFinder::Destroy(void)
{
	// destroy all the AIPlanNode objects and clear the map
	for (ActorToAIPlanNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
		for (AIPlanNodeVector::iterator itPlan = (*it).second.begin(); itPlan != (*it).second.end(); itPlan++)
			delete (*itPlan);
	mNodes.clear();

	// clear the open set
	mOpenSet.clear();
}

//
// AIFinder::operator()
//
void AIFinder::operator()(
	PathingNode* pStartNode, const std::map<ActorId, float>& searchItems,
	std::map<PathingActorVec, float>& actorsPathPlans, unsigned int pathingType)
{
	LogAssert(pStartNode, "Invalid node");

	// The open set is a priority queue of the nodes to be evaluated. If it's ever empty, it means 
	// we couldn't find a path to the goal. The start node is the only node that is initially in the open set.
	AddToOpenSet(pStartNode, NULL, NULL);
	while (!mOpenSet.empty())
	{
		// grab the most likely candidate
		AIPlanNode* planNode = mOpenSet.front();

		if (planNode->GetPathingActor())
		{
			bool containsActorPathPlan = false;
			for (auto& actorsPathPlan : actorsPathPlans)
			{
				const PathingActorVec& actorPathPlan = actorsPathPlan.first;
				containsActorPathPlan = planNode->ContainActors(actorPathPlan);
				if (containsActorPathPlan)
				{
					if (planNode->GetWeight() < actorsPathPlan.second)
						actorsPathPlan.second = planNode->GetWeight();

					break;
				}
			}

			if (!containsActorPathPlan)
				actorsPathPlans[planNode->GetPathingActors()] = planNode->GetWeight();
		}

		// we're processing this node so remove it from the open set and add it to the closed set
		mOpenSet.pop_front();
		AddToClosedSet(planNode);

		// get the neighboring actors
		PathingActorMap neighbors = planNode->GetPathingActor() ?
			planNode->GetPathingActor()->GetTarget()->GetActors() : planNode->GetPathingNode()->GetActors();

		// loop though all the neighboring actors and evaluate each one
		for (auto it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			PathingActor* pActorToEvaluate = it->second;
			if (pActorToEvaluate->GetType() != pathingType)
				continue;

			if (planNode->GetPathingActor())
			{
				if (planNode->FindActor(pActorToEvaluate->GetActor()))
					continue;

				AIPlanNode* pPlanToCompare = mNodes[pActorToEvaluate->GetActor()].front();
				float costForActorPlan = pPlanToCompare->GetWeight() + searchItems.at(pActorToEvaluate->GetActor());

				// figure out the cost for this route through the node
				float costForThisPlan = planNode->GetWeight() +
					planNode->CalculatePathCost(planNode->GetPathingActor()->GetTarget(), pActorToEvaluate);
				if (costForThisPlan > costForActorPlan)
					continue;

				AddToOpenSet(planNode->GetPathingActor()->GetTarget(), pActorToEvaluate, planNode);
			}
			else AddToOpenSet(planNode->GetPathingNode(), pActorToEvaluate, planNode);
		}
	}
}

void AIFinder::AddToOpenSet(PathingNode* pNode, PathingActor* pActor, AIPlanNode* pPrevNode)
{
	LogAssert(pNode, "Invalid node");

	// create a new PathPlanNode if necessary
	AIPlanNode* pThisNode = new AIPlanNode(pNode, pActor, pPrevNode);
	if (pActor)
		mNodes[pActor->GetActor()].push_back(pThisNode);
	else
		mNodes[pNode->GetActorId()].push_back(pThisNode);

	// now insert it into the priority queue
	mOpenSet.push_back(pThisNode);
}

void AIFinder::AddToClosedSet(AIPlanNode* pNode)
{
	LogAssert(pNode, "Invalid node");
	pNode->SetClosed();
}

PathingActorVec AIFinder::RebuildPath(AIPlanNode* pGoalNode)
{
	LogAssert(pGoalNode, "Invalid node");

	PathingActorVec pathPlan;

	AIPlanNode* pNode = pGoalNode;
	while (pNode)
	{
		if (pNode->GetPathingActor() != NULL)
			pathPlan.insert(pathPlan.begin(), pNode->GetPathingActor());

		pNode = pNode->GetPrev();
	}

	return pathPlan;
}

QuakeAIManager::QuakeAIManager() : AIManager()
{
	mEnable = false;

	mGameSimulation = nullptr;

	mLastArcId = 0;
	mLastNodeId = 0;
	mLastPlanId = 0;

	mMaxRotateSpeed = 180.f;
	mGravity = Settings::Get()->GetVector3("default_gravity");

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

	mSimulationStep = 1.f / 60.f;
	mUpdateCounter = 0;
	mUpdateTimeMs = 0;

	mLogError = std::ofstream("error.txt", std::ios::out);
	mLogInfo = std::ofstream("info.txt", std::ios::out);

	AIGame::GameState gameState;
	gameState.id = 0;
	gameState.time = "0:0:0";
	AIGame::EventTrack eventTrack;
	eventTrack.elapsedTime = 0;
	gameState.tracks.push_back(eventTrack);

	mGame.states.push_back(gameState);
}   // QuakeAIManager

//-----------------------------------------------------------------------------

QuakeAIManager::~QuakeAIManager()
{
	mLogError.close();
	mLogInfo.close();

	for (auto& actorPickup : mGameActorPickups)
		delete actorPickup.second;

	mGameActors.clear();
	mGameActorPickups.clear();
}   // ~QuakeAIManager

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadGame	
//
//    Loads recorded ai game combat
//
void QuakeAIManager::LoadGame()
{
	// Load the ai game file
	std::string aiGamePath = FileSystem::Get()->GetPath(
		"ai/quake/" + Settings::Get()->Get("selected_world") + "/game.bin");

	std::ifstream is(aiGamePath.c_str(), std::ios::binary);
	if (is.fail())
	{
		LogError(strerror(errno));
		return;
	}
	cereal::BinaryInputArchive archive(is);
	archive(mGame);
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::SaveGame
//
//    Saves ai game combat
//
void QuakeAIManager::SaveGame()
{
	// Save the ai game file
	std::string aiGamePath = FileSystem::Get()->GetPath(
		"ai/quake/" + Settings::Get()->Get("selected_world") + "/game.bin");

	std::ofstream os(aiGamePath.c_str(), std::ios::binary);
	cereal::BinaryOutputArchive archive(os);
	archive(mGame);
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadGameAnalysis	
//
//    Loads recorded ai game combat analsysis
//
void QuakeAIManager::LoadGameAnalysis()
{
	// Load the ai analysis file
	std::string aiGameAnalysisPath = FileSystem::Get()->GetPath(
		"ai/quake/" + Settings::Get()->Get("selected_world") + "/analysis.bin");

	std::ifstream is(aiGameAnalysisPath.c_str(), std::ios::binary);
	if (is.fail())
	{
		LogError(strerror(errno));
		return;
	}
	cereal::BinaryInputArchive archive(is);
	archive(mGameAnalysis);
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::SaveGameAnalysis	
//
//    Saves ai game combat analsysis
//
void QuakeAIManager::SaveGameAnalysis()
{
	mGameAnalysis.decisions.clear();
	for (AIAnalysis::GameDecision& decision : mGameDecisions)
		if (decision.evaluation.playerGuessings.empty())
			mGameAnalysis.decisions.emplace_back(decision);

	// Save the ai game file
	{
		std::string aiGamePath = FileSystem::Get()->GetPath(
			"ai/quake/" + Settings::Get()->Get("selected_world") + "/game.bin");

		std::ofstream os(aiGamePath.c_str(), std::ios::binary);
		cereal::BinaryOutputArchive archive(os);
		archive(mGame);
	}

	// Save the ai analysis file
	{
		std::string aiGameAnalysisPath = FileSystem::Get()->GetPath(
			"ai/quake/" + Settings::Get()->Get("selected_world") + "/analysis.bin");

		std::ofstream os(aiGameAnalysisPath.c_str(), std::ios::binary);
		cereal::BinaryOutputArchive archive(os);
		archive(mGameAnalysis);
	}
}

void QuakeAIManager::SaveGraph(const std::string& path)
{
	//set data
	AIMap::Graph data;

	const PathingNodeMap& pathingNodes = mPathingGraph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* pathNode = (*it).second;

		AIMap::GraphNode node;
		node.id = pathNode->GetId();
		node.actorid = pathNode->GetActorId();
		node.clusterid = pathNode->GetCluster();
		node.tolerance = pathNode->GetTolerance();
		node.position.x = pathNode->GetPosition()[0];
		node.position.y = pathNode->GetPosition()[1];
		node.position.z = pathNode->GetPosition()[2];

		for (auto const& visibilityNode : pathNode->GetVisibileNodes())
		{
			AIMap::VisibleNode visibleNode;
			visibleNode.id = visibilityNode.first->GetId();

			node.visibles.push_back(visibleNode);
		}

		for (auto const& pathArc : pathNode->GetArcs())
		{
			AIMap::ArcNode arcNode;
			arcNode.id = pathArc.second->GetId();
			arcNode.type = pathArc.second->GetType();
			arcNode.nodeid = pathArc.second->GetNode()->GetId();
			arcNode.weight = pathArc.second->GetWeight();

			PathingTransition* pathTransition = pathArc.second->GetTransition();
			if (pathTransition)
			{
				for (PathingNode* pNode : pathTransition->GetNodes())
				{
					arcNode.nodes.push_back(pNode->GetId());
				}
				for (float weight : pathTransition->GetWeights())
				{
					arcNode.weights.push_back(weight);
				}
				for (Vector3<float> position : pathTransition->GetPositions())
				{
					arcNode.positions.push_back(AIMap::Vec3{
						(short)round(position[0]),
						(short)round(position[1]),
						(short)round(position[2]) });
				}
			}

			node.arcs.push_back(arcNode);
		}

		for (auto const& pathCluster : pathNode->GetClusters())
		{
			AIMap::ClusterNode clusterNode;
			clusterNode.type = pathCluster.second->GetType();
			clusterNode.nodeid = pathCluster.second->GetNode()->GetId();
			clusterNode.targetid = pathCluster.second->GetTarget()->GetId();

			node.clusters.push_back(clusterNode);
		}

		for (auto const& pathActor : pathNode->GetActors())
		{
			AIMap::ActorNode actorNode;
			actorNode.type = pathActor.second->GetType();
			actorNode.actorid = pathActor.second->GetActor();
			actorNode.nodeid = pathActor.second->GetNode()->GetId();
			actorNode.targetid = pathActor.second->GetTarget()->GetId();

			node.actors.push_back(actorNode);
		}

		data.nodes.push_back(node);
	}

	const ClusterMap& clusters = mPathingGraph->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* pathCluster = (*it).second;

		AIMap::GraphCluster cluster;
		cluster.id = pathCluster->GetId();
		cluster.node = pathCluster->GetNode() ? pathCluster->GetNode()->GetId() : 0;

		for (auto const& node : pathCluster->GetNodes())
			cluster.nodes.push_back(node.first);

		for (auto const& nodeActor : pathCluster->GetNodeActors())
			cluster.nodeActors[nodeActor.first] = nodeActor.second->GetId();

		for (auto const& visibleCluster : pathCluster->GetVisibileClusters())
			cluster.visibles[visibleCluster.first] = visibleCluster.second->GetId();

		data.clusters.push_back(cluster);
	}

	std::ofstream os(path.c_str(), std::ios::binary);
	cereal::BinaryOutputArchive archive(os);
	archive(data);
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::SaveGraph
//
//    Saves the AI graph information
//
void QuakeAIManager::SaveGraph(const std::string& path, std::shared_ptr<PathingGraph>& graph)
{
	//set data
	AIMap::Graph data;

	const PathingNodeMap& pathingNodes = graph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* pathNode = (*it).second;

		AIMap::GraphNode node;
		node.id = pathNode->GetId();
		node.actorid = pathNode->GetActorId();
		node.clusterid = pathNode->GetCluster();
		node.tolerance = pathNode->GetTolerance();
		node.position.x = pathNode->GetPosition()[0];
		node.position.y = pathNode->GetPosition()[1];
		node.position.z = pathNode->GetPosition()[2];

		for (auto const& visibilityNode : pathNode->GetVisibileNodes())
		{
			AIMap::VisibleNode visibleNode;
			visibleNode.id = visibilityNode.first->GetId();

			node.visibles.push_back(visibleNode);
		}

		for (auto const& pathArc : pathNode->GetArcs())
		{
			AIMap::ArcNode arcNode;
			arcNode.id = pathArc.second->GetId();
			arcNode.type = pathArc.second->GetType();
			arcNode.nodeid = pathArc.second->GetNode()->GetId();
			arcNode.weight = pathArc.second->GetWeight();

			PathingTransition* pathTransition = pathArc.second->GetTransition();
			if (pathTransition)
			{
				for (PathingNode* pNode : pathTransition->GetNodes())
				{
					arcNode.nodes.push_back(pNode->GetId());
				}
				for (float weight : pathTransition->GetWeights())
				{
					arcNode.weights.push_back(weight);
				}
				for (Vector3<float> position : pathTransition->GetPositions())
				{
					arcNode.positions.push_back(AIMap::Vec3{
						(short)round(position[0]),
						(short)round(position[1]),
						(short)round(position[2]) });
				}
			}

			node.arcs.push_back(arcNode);
		}

		for (auto const& pathCluster : pathNode->GetClusters())
		{
			AIMap::ClusterNode clusterNode;
			clusterNode.type = pathCluster.second->GetType();
			clusterNode.nodeid = pathCluster.second->GetNode()->GetId();
			clusterNode.targetid = pathCluster.second->GetTarget()->GetId();

			node.clusters.push_back(clusterNode);
		}

		for (auto const& pathActor : pathNode->GetActors())
		{
			AIMap::ActorNode actorNode;
			actorNode.type = pathActor.second->GetType();
			actorNode.actorid = pathActor.second->GetActor();
			actorNode.nodeid = pathActor.second->GetNode()->GetId();
			actorNode.targetid = pathActor.second->GetTarget()->GetId();

			node.actors.push_back(actorNode);
		}

		data.nodes.push_back(node);
	}

	const ClusterMap& clusters = graph->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* pathCluster = (*it).second;

		AIMap::GraphCluster cluster;
		cluster.id = pathCluster->GetId();
		cluster.node = pathCluster->GetNode() ? pathCluster->GetNode()->GetId() : 0;

		for (auto const& node : pathCluster->GetNodes())
			cluster.nodes.push_back(node.first);

		for (auto const& nodeActor : pathCluster->GetNodeActors())
			cluster.nodeActors[nodeActor.first] = nodeActor.second->GetId();

		for (auto const& visibleCluster : pathCluster->GetVisibileClusters())
			cluster.visibles[visibleCluster.first] = visibleCluster.second->GetId();

		data.clusters.push_back(cluster);
	}

	std::ofstream os(path.c_str(), std::ios::binary);
	cereal::BinaryOutputArchive archive(os);
	archive(data);
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadGraph
//
//    Loads the graph information
//
void QuakeAIManager::LoadGraph(const std::wstring& path)
{
	//set data
	AIMap::Graph data;

	std::ifstream is(path.c_str(), std::ios::binary);
	if (is.fail())
	{
		LogError(strerror(errno));
		return;
	}
	cereal::BinaryInputArchive archive(is);
	archive(data);

	mLastArcId = 0;
	mLastNodeId = 0;

	mPathingGraph = std::make_shared<PathingGraph>();

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{ node.position.x, node.position.y, node.position.z };
		if (mLastNodeId < pathNodeId) mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		mPathingGraph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (AIMap::VisibleNode visibleNode : node.visibles)
		{
			PathingNode* visibilityNode = pathingGraph[visibleNode.id];
			pathNode->AddVisibleNode(
				visibilityNode, Length(visibilityNode->GetPosition() - pathNode->GetPosition()));
			
		}

		for (AIMap::ArcNode arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float weight = arc.weight;
			if (mLastArcId < arcId) mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], weight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (int nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (float weight : arc.weights)
			{
				weights.push_back(weight);
			}
			for (AIMap::Vec3 position : arc.positions)
			{
				positions.push_back(Vector3<float>{(float)position.x, (float)position.y, (float)position.z});
			}

			if (nodes.size())
			{
				PathingTransition* pathTransition = new PathingTransition(nodes, weights, positions);
				pathArc->AddTransition(pathTransition);
			}
		}

		for (AIMap::ClusterNode cluster : node.clusters)
		{
			int pathingType = cluster.type;
			int clusterNode = cluster.nodeid;
			int clusterTarget = cluster.targetid;

			PathingCluster* pathCluster = new PathingCluster(pathingType);
			pathCluster->LinkClusters(pathingGraph[clusterNode], pathingGraph[clusterTarget]);
			pathNode->AddCluster(pathCluster);
		}

		for (AIMap::ActorNode actor : node.actors)
		{
			int pathingType = actor.type;
			int actorId = actor.actorid;
			int actorNode = actor.nodeid;
			int actorTarget = actor.targetid;

			PathingActor* pathActor = new PathingActor(pathingType, actorId);
			pathActor->LinkActors(pathingGraph[actorNode], pathingGraph[actorTarget]);
			pathNode->AddActor(pathActor);
		}
	}

	std::map<unsigned int, Cluster*> clusterGraph;
	for (AIMap::GraphCluster cluster : data.clusters)
	{
		Cluster* pCluster = new Cluster(cluster.id, pathingGraph[cluster.node]);

		for (auto const& clusterNode : cluster.nodes)
			pCluster->AddNode(pathingGraph[clusterNode]);

		for (auto const& nodeActor : cluster.nodeActors)
			pCluster->AddNodeActor(nodeActor.first, pathingGraph[nodeActor.second]);

		mPathingGraph->InsertCluster(pCluster);

		clusterGraph[pCluster->GetId()] = pCluster;
	}

	for (AIMap::GraphCluster cluster : data.clusters)
	{
		for (auto const& clusterVisible : cluster.visibles)
		{
			clusterGraph[cluster.id]->AddVisibleCluster(
				clusterVisible.first, pathingGraph[clusterVisible.second]);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadGraph
//
//    Loads graph information
//
void QuakeAIManager::LoadGraph(const std::wstring& path, std::shared_ptr<PathingGraph>& graph)
{
	//set data
	AIMap::Graph data;

	std::ifstream is(path.c_str(), std::ios::binary);
	if (is.fail())
	{
		LogError(strerror(errno));
		return;
	}
	cereal::BinaryInputArchive archive(is);
	archive(data);

	mLastArcId = 0;
	mLastNodeId = 0;

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{node.position.x, node.position.y, node.position.z };
		if (mLastNodeId < pathNodeId) mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		graph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (AIMap::VisibleNode visibleNode : node.visibles)
		{
			PathingNode* visibilityNode = pathingGraph[visibleNode.id];
			pathNode->AddVisibleNode(
				visibilityNode, Length(visibilityNode->GetPosition() - pathNode->GetPosition()));
		}

		for (AIMap::ArcNode arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float weight = arc.weight;
			if (mLastArcId < arcId) mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], weight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (int nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (float weight : arc.weights)
			{
				weights.push_back(weight);
			}
			for (AIMap::Vec3 position : arc.positions)
			{
				positions.push_back(Vector3<float>{(float)position.x, (float)position.y, (float)position.z});
			}

			if (nodes.size())
			{
				PathingTransition* pathTransition = new PathingTransition(nodes, weights, positions);
				pathArc->AddTransition(pathTransition);
			}
		}

		for (AIMap::ClusterNode cluster : node.clusters)
		{
			int pathingType = cluster.type;
			int clusterNode = cluster.nodeid;
			int clusterTarget = cluster.targetid;

			PathingCluster* pathCluster = new PathingCluster(pathingType);
			pathCluster->LinkClusters(pathingGraph[clusterNode], pathingGraph[clusterTarget]);
			pathNode->AddCluster(pathCluster);
		}

		for (AIMap::ActorNode actor : node.actors)
		{
			int pathingType = actor.type;
			int actorId = actor.actorid;
			int actorNode = actor.nodeid;
			int actorTarget = actor.targetid;

			PathingActor* pathActor = new PathingActor(pathingType, actorId);
			pathActor->LinkActors(pathingGraph[actorNode], pathingGraph[actorTarget]);
			pathNode->AddActor(pathActor);
		}
	}

	std::map<unsigned int, Cluster*> clusterGraph;
	for (AIMap::GraphCluster cluster : data.clusters)
	{
		Cluster* pCluster = new Cluster(cluster.id, pathingGraph[cluster.node]);

		for (auto const& clusterNode : cluster.nodes)
			pCluster->AddNode(pathingGraph[clusterNode]);

		for (auto const& nodeActor : cluster.nodeActors)
			pCluster->AddNodeActor(nodeActor.first, pathingGraph[nodeActor.second]);

		graph->InsertCluster(pCluster);

		clusterGraph[pCluster->GetId()] = pCluster;
	}

	for (AIMap::GraphCluster cluster : data.clusters)
	{
		for (auto const& clusterVisible : cluster.visibles)
		{
			clusterGraph[cluster.id]->AddVisibleCluster(
				clusterVisible.first, pathingGraph[clusterVisible.second]);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadPathingMap
//
//    Loads basic pathing information
//
void QuakeAIManager::LoadPathingMap(const std::wstring& path)
{
	//set data
	AIMap::Graph data;

	std::ifstream is(path.c_str(), std::ios::binary);
	if (is.fail())
	{
		LogError(strerror(errno));
		return;
	}
	cereal::BinaryInputArchive archive(is);
	archive(data);

	mLastArcId = 0;
	mLastNodeId = 0;

	mPathingGraph = std::make_shared<PathingGraph>();

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{node.position.x, node.position.y, node.position.z };
		if (mLastNodeId < pathNodeId) mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		mPathingGraph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (AIMap::ArcNode arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float weight = arc.weight;
			if (mLastArcId < arcId) mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], weight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (int nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (float weight : arc.weights)
			{
				weights.push_back(weight);
			}
			for (AIMap::Vec3 position : arc.positions)
			{
				positions.push_back(Vector3<float>{(float)position.x, (float)position.y, (float)position.z});
			}

			if (nodes.size())
			{
				PathingTransition* pathTransition = new PathingTransition(nodes, weights, positions);
				pathArc->AddTransition(pathTransition);
			}
		}
	}

	for (AIMap::GraphCluster cluster : data.clusters)
	{
		Cluster* pCluster = new Cluster(cluster.id, pathingGraph[cluster.node]);

		for (auto const& clusterNode : cluster.nodes)
			pCluster->AddNode(pathingGraph[clusterNode]);

		for (auto const& nodeActor : cluster.nodeActors)
			pCluster->AddNodeActor(nodeActor.first, pathingGraph[nodeActor.second]);

		mPathingGraph->InsertCluster(pCluster);
	}
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadPathingMap
//
//    Loads basic pathing information
//
void QuakeAIManager::LoadPathingMap(const std::wstring& path, std::shared_ptr<PathingGraph>& graph)
{
	//set data
	AIMap::Graph data;

	std::ifstream is(path.c_str(), std::ios::binary);
	if (is.fail())
	{
		LogError(strerror(errno));
		return;
	}
	cereal::BinaryInputArchive archive(is);
	archive(data);

	mLastArcId = 0;
	mLastNodeId = 0;

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{node.position.x, node.position.y, node.position.z };
		if (mLastNodeId < pathNodeId) mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		graph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (AIMap::GraphNode node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (AIMap::ArcNode arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float weight = arc.weight;
			if (mLastArcId < arcId) mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], weight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (int nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (float weight : arc.weights)
			{
				weights.push_back(weight);
			}
			for (AIMap::Vec3 position : arc.positions)
			{
				positions.push_back(Vector3<float>{(float)position.x, (float)position.y, (float)position.z});
			}

			if (nodes.size())
			{
				PathingTransition* pathTransition = new PathingTransition(nodes, weights, positions);
				pathArc->AddTransition(pathTransition);
			}
		}
	}

	for (AIMap::GraphCluster cluster : data.clusters)
	{
		Cluster* pCluster = new Cluster(cluster.id, pathingGraph[cluster.node]);

		for (auto const& clusterNode : cluster.nodes)
			pCluster->AddNode(pathingGraph[clusterNode]);

		for (auto const& nodeActor : cluster.nodeActors)
			pCluster->AddNodeActor(nodeActor.first, pathingGraph[nodeActor.second]);

		graph->InsertCluster(pCluster);
	}
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::UpdateMap
//
//    Complete AI Map visibility and clustering data
//
void QuakeAIManager::UpdateMap(std::shared_ptr<PathingGraph>& graph, ActorId playerId)
{
	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());

	// we obtain visibility information from pathing graph 
	SimulateVisibility(graph);

	// create transitions associated to closest node
	CreateTransitions(graph);

	// we group the graph nodes in clusters
	CreateClusters(graph, MAX_CLUSTERS);
}

void QuakeAIManager::UpdateMap(ActorId playerId)
{
	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());

	// we obtain visibility information from pathing graph 
	SimulateVisibility(mPathingGraph);

	// create transitions associated to closest node
	CreateTransitions(mPathingGraph);

	// we group the graph nodes in clusters
	CreateClusters(mPathingGraph, MAX_CLUSTERS);
}

void QuakeAIManager::RemovePlayerSimulations(AIAnalysis::GameEvaluation& gameEvaluation)
{
	Concurrency::parallel_for_each(begin(gameEvaluation.playerGuessings),
		end(gameEvaluation.playerGuessings), [&](auto& playerGuessing)
	//for (auto playerGuessing : gameEvaluation.playerGuessings)
	{
		Concurrency::parallel_for_each(begin(playerGuessing->simulations),
			end(playerGuessing->simulations), [&](auto& simulation)
		//for (auto simulation : playerGuessing->simulations)
		{
			delete simulation;
		});

		delete playerGuessing;
		playerGuessing = nullptr;
	});
	gameEvaluation.playerGuessings.clear();

	Concurrency::parallel_for_each(begin(gameEvaluation.playerDecisions),
		end(gameEvaluation.playerDecisions), [&](auto& playerDecision)
	//for (auto playerDecision : gameEvaluation.playerDecisions)
	{
		Concurrency::parallel_for_each(begin(playerDecision->simulations),
			end(playerDecision->simulations), [&](auto& simulation)
		//for (auto simulation : playerDecision->simulations)
		{
			delete simulation;
		});

		delete playerDecision;
		playerDecision = nullptr;
	});
	gameEvaluation.playerDecisions.clear();

	if (gameEvaluation.playerDecision)
	{
		delete gameEvaluation.playerDecision;
		gameEvaluation.playerDecision = nullptr;
	}

	if (gameEvaluation.playerGuessDecision)
	{
		delete gameEvaluation.playerGuessDecision;
		gameEvaluation.playerGuessDecision = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////
// AI Decision Making
//
void QuakeAIManager::Simulation(
	EvaluationType evaluation, const std::map<ActorId, float>& gameItems,
	PlayerData& playerData, const PathingArcVec& playerPathPlan, float playerPathOffset,
	PlayerData& otherPlayerData, const PathingArcVec& otherPlayerPathPlan, float otherPlayerPathOffset)
{
	std::map<ActorId, float> playerActors, otherPlayerActors;
	float playerPathWeight = 0.f, otherPlayerPathWeight = 0.f;
	for (auto pathingArc : playerPathPlan)
	{
		playerPathWeight += pathingArc->GetWeight();
		if (pathingArc->GetNode()->GetActorId() != INVALID_ACTOR_ID)
			if (playerActors.find(pathingArc->GetNode()->GetActorId()) == playerActors.end())
				playerActors[pathingArc->GetNode()->GetActorId()] = playerPathWeight - playerPathOffset;
	}
	for (auto otherPathingArc : otherPlayerPathPlan)
	{
		otherPlayerPathWeight += otherPathingArc->GetWeight();
		if (otherPathingArc->GetNode()->GetActorId() != INVALID_ACTOR_ID)
			if (otherPlayerActors.find(otherPathingArc->GetNode()->GetActorId()) == otherPlayerActors.end())
				otherPlayerActors[otherPathingArc->GetNode()->GetActorId()] = otherPlayerPathWeight - otherPlayerPathOffset;
	}

	std::map<ActorId, float> pathActors;
	std::map<ActorId, float>::const_iterator itActor;
	for (itActor = playerActors.begin(); itActor != playerActors.end(); itActor++)
	{
		if (CanItemBeGrabbed((*itActor).first, (*itActor).second, playerData, gameItems))
		{
			if (otherPlayerActors.find((*itActor).first) != otherPlayerActors.end())
			{
				if (!CanItemBeGrabbed((*itActor).first, (*itActor).second, otherPlayerData, gameItems) ||
					otherPlayerActors.at((*itActor).first) >= (*itActor).second)
				{
					pathActors[(*itActor).first] = (*itActor).second;
				}
			}
			else pathActors[(*itActor).first] = (*itActor).second;
		}
	}
	playerData.plan.ResetPathPlan(playerPathPlan);
	PickupItems(playerData, pathActors, gameItems);

	std::map<ActorId, float> otherPathActors;
	std::map<ActorId, float>::const_iterator itOtherActor;
	for (itOtherActor = otherPlayerActors.begin(); itOtherActor != otherPlayerActors.end(); itOtherActor++)
	{
		if (CanItemBeGrabbed((*itOtherActor).first, (*itOtherActor).second, otherPlayerData, gameItems))
		{
			if (playerActors.find((*itOtherActor).first) != playerActors.end())
			{
				if (!CanItemBeGrabbed((*itOtherActor).first, (*itOtherActor).second, playerData, gameItems) ||
					playerActors.at((*itOtherActor).first) > (*itOtherActor).second)
				{
					otherPathActors[(*itOtherActor).first] = (*itOtherActor).second;
				}
			}
			else otherPathActors[(*itOtherActor).first] = (*itOtherActor).second;
		}
	}
	otherPlayerData.plan.ResetPathPlan(otherPlayerPathPlan);
	PickupItems(otherPlayerData, otherPathActors, gameItems);

	std::map<float, VisibilityData, std::less<float>> playerVisibility, otherPlayerVisibility;
	for (auto const& pathActor : pathActors)
	{
		const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(pathActor.first);
		if (itemPickup->GetType() == "Weapon" || itemPickup->GetType() == "Ammo")
			playerVisibility[pathActor.second] = VisibilityData();
	}

	for (auto const& otherPathActor : otherPathActors)
	{
		const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(otherPathActor.first);
		if (itemPickup->GetType() == "Weapon" || itemPickup->GetType() == "Ammo")
			otherPlayerVisibility[otherPathActor.second] = VisibilityData();
	}

	// weapon firing time will be the beginning from visibility 
	float playerVisibleTime = playerData.weaponTime;
	float otherPlayerVisibleTime = otherPlayerData.weaponTime;

	if (playerData.plan.weight - playerPathOffset < otherPlayerData.plan.weight - otherPlayerPathOffset)
	{
		float planWeight = otherPlayerData.plan.weight - otherPlayerPathOffset;
		for (float currentPlanWeight = 0.f; currentPlanWeight < planWeight; currentPlanWeight += 0.5f)
			otherPlayerVisibility[currentPlanWeight] = VisibilityData();
		otherPlayerVisibility[otherPlayerData.plan.weight - otherPlayerPathOffset] = VisibilityData();

		for (float currentPlanWeight = 0.f; currentPlanWeight < planWeight; currentPlanWeight += 0.5f)
			playerVisibility[currentPlanWeight] = VisibilityData();
		playerVisibility[otherPlayerData.plan.weight - otherPlayerPathOffset] = VisibilityData();
		playerVisibility[playerData.plan.weight - playerPathOffset] = VisibilityData();

		CalculateVisibility(
			otherPlayerData.plan.node, otherPlayerPathOffset, otherPlayerVisibleTime, otherPlayerPathPlan, otherPlayerVisibility,
			playerData.plan.node, playerPathOffset, playerVisibleTime, playerPathPlan, playerVisibility);
	}
	else
	{
		float planWeight = playerData.plan.weight - playerPathOffset;
		for (float currentPlanWeight = 0.f; currentPlanWeight < planWeight; currentPlanWeight += 0.5f)
			playerVisibility[currentPlanWeight] = VisibilityData();
		playerVisibility[playerData.plan.weight - playerPathOffset] = VisibilityData();

		for (float currentPlanWeight = 0.f; currentPlanWeight < planWeight; currentPlanWeight += 0.5f)
			otherPlayerVisibility[currentPlanWeight] = VisibilityData();
		otherPlayerVisibility[otherPlayerData.plan.weight - otherPlayerPathOffset] = VisibilityData();
		otherPlayerVisibility[playerData.plan.weight - playerPathOffset] = VisibilityData();

		CalculateVisibility(playerData.plan.node, playerPathOffset, playerVisibleTime, playerPathPlan, playerVisibility,
			otherPlayerData.plan.node, otherPlayerPathOffset, otherPlayerVisibleTime, otherPlayerPathPlan, otherPlayerVisibility);
	}

	//calculate visibility average
	for (auto& visibility : playerVisibility)
	{
		if (visibility.second.moveTime)
		{
			visibility.second.moveDistance /= visibility.second.moveTime;
			visibility.second.moveHeight /= visibility.second.moveTime;
		}
	}
	for (auto& visibility : otherPlayerVisibility)
	{
		if (visibility.second.moveTime)
		{
			visibility.second.moveDistance /= visibility.second.moveTime;
			visibility.second.moveHeight /= visibility.second.moveTime;
		}
	}

	// calculate damage
	CalculateDamage(playerData, playerVisibility);
	CalculateDamage(otherPlayerData, otherPlayerVisibility);

	//we calculate the heuristic
	CalculateHeuristic(evaluation, playerData, otherPlayerData);
}

bool QuakeAIManager::BuildPath(
	std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans)
{
	std::unordered_map<unsigned int, PathingNode*> clusterNodes, otherClusterNodes;

	std::map<PathingCluster*, PathingArcVec> clusterPaths, otherClusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights, otherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 100, clusterPaths, clusterPathWeights);
	for (auto& clusterPath : clusterPaths)
		clusterNodes[clusterPath.first->GetTarget()->GetCluster()] = clusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	std::map<PathingCluster*, PathingArcVec> jumpClusterPaths, jumpOtherClusterPaths;
	std::map<PathingCluster*, float> jumpClusterPathWeights, jumpOtherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_JUMP, 100, jumpClusterPaths, jumpClusterPathWeights);
	for (auto& jumpClusterPath : jumpClusterPaths)
	{
		if (clusterNodes.find(jumpClusterPath.first->GetTarget()->GetCluster()) == clusterNodes.end())
		{
			clusterPaths[jumpClusterPath.first] = jumpClusterPath.second;
			clusterPathWeights[jumpClusterPath.first] = jumpClusterPathWeights[jumpClusterPath.first];
		}
	}

	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> pathingClusterNodes;
	for (auto& clusterPath : clusterPaths)
	{
		for (auto& clusterPathArc : clusterPath.second)
		{
			PathingTransition* clusterPathTransition = clusterPathArc->GetTransition();
			pathingClusterNodes[clusterPathArc->GetNode()][clusterPath.first] =
				clusterPath.first->GetType() << 28 | clusterPath.first->GetTarget()->GetId();
			for (auto& clusterPathNode : clusterPathTransition->GetNodes())
			{
				pathingClusterNodes[clusterPathNode][clusterPath.first] =
					clusterPath.first->GetType() << 28 | clusterPath.first->GetTarget()->GetId();
			}
		}
	}

	otherClusterNodeStart->GetClusters(AT_MOVE, 100, otherClusterPaths, otherClusterPathWeights);
	for (auto& otherClusterPath : otherClusterPaths)
		otherClusterNodes[otherClusterPath.first->GetTarget()->GetCluster()] = otherClusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	otherClusterNodeStart->GetClusters(AT_JUMP, 100, jumpOtherClusterPaths, jumpOtherClusterPathWeights);
	for (auto& jumpOtherClusterPath : jumpOtherClusterPaths)
	{
		if (otherClusterNodes.find(jumpOtherClusterPath.first->GetTarget()->GetCluster()) == otherClusterNodes.end())
		{
			otherClusterPaths[jumpOtherClusterPath.first] = jumpOtherClusterPath.second;
			otherClusterPathWeights[jumpOtherClusterPath.first] = jumpOtherClusterPathWeights[jumpOtherClusterPath.first];
		}
	}

	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> otherPathingClusterNodes;
	for (auto& otherClusterPath : otherClusterPaths)
	{
		for (auto& otherClusterPathArc : otherClusterPath.second)
		{
			PathingTransition* otherClusterPathTransition = otherClusterPathArc->GetTransition();
			otherPathingClusterNodes[otherClusterPathArc->GetNode()][otherClusterPath.first] =
				otherClusterPath.first->GetType() << 28 | otherClusterPath.first->GetTarget()->GetId();
			for (auto& otherClusterPathNode : otherClusterPathTransition->GetNodes())
			{
				otherPathingClusterNodes[otherClusterPathNode][otherClusterPath.first] =
					otherClusterPath.first->GetType() << 28 | otherClusterPath.first->GetTarget()->GetId();
			}
		}
	}

	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>> visibleClusters;
	Concurrency::parallel_for_each(begin(pathingClusterNodes), end(pathingClusterNodes), [&](auto& pathingClusterNode)
	//for (auto& pathingClusterNode : pathingClusterNodes)
	{
		Concurrency::parallel_for_each(begin(otherPathingClusterNodes), end(otherPathingClusterNodes), [&](auto& otherPathingClusterNode)
		//for (auto& otherPathingClusterNode : otherPathingClusterNodes)
		{
			if (pathingClusterNode.first->IsVisibleNode(otherPathingClusterNode.first))
			{
				for (auto& pathingCluster : pathingClusterNode.second)
				{
					for (auto& otherPathingCluster : otherPathingClusterNode.second)
					{
						unsigned long long clusterCode =
							(unsigned long long)pathingCluster.second << 32 | (unsigned long long)otherPathingCluster.second;
						visibleClusters[clusterCode] = std::make_pair(pathingCluster.first, otherPathingCluster.first);
					}
				}
			}
		});
	});

	if (visibleClusters.size())
	{
		//we will only process those clusters which are visibles from both players
		std::multimap<float, PathingCluster*, std::less<float>> closestClusterPathWeights, otherClosestClusterPathWeights;
		for (auto& visibleCluster : visibleClusters)
		{
			PathingCluster* pathingCluster = visibleCluster.second.first;
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode(pathingCluster->GetTarget()->GetCluster());

			unsigned long long pathingClusterCode =
				(unsigned long long)pathingCluster->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)pathingClusterNodeEnd->GetId() << 32 | (unsigned long long)pathingCluster->GetType() << 28 |
				(unsigned long long)pathingClusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

			//Build path based on cluster pathway
			if (clusterNodePathPlans.find(pathingClusterCode) == clusterNodePathPlans.end())
			{
				clusterNodePathPlans[pathingClusterCode] = clusterPaths[pathingCluster];
				clusterPathings[pathingClusterCode] = { pathingCluster, pathingCluster };

				closestClusterPathWeights.insert({ clusterPathWeights[pathingCluster], pathingCluster });
			}

			PathingCluster* otherPathingCluster = visibleCluster.second.second;
			PathingNode* otherPathingClusterNodeEnd = graph->FindClusterNode(otherPathingCluster->GetTarget()->GetCluster());

			unsigned long long otherPathingClusterCode =
				(unsigned long long)otherPathingCluster->GetType() << 60 | (unsigned long long)otherClusterNodeStart->GetId() << 46 |
				(unsigned long long)otherPathingClusterNodeEnd->GetId() << 32 | (unsigned long long)otherPathingCluster->GetType() << 28 |
				(unsigned long long)otherPathingClusterNodeEnd->GetId() << 14 | (unsigned long long)otherPathingClusterNodeEnd->GetId();

			//Build path based on cluster pathway
			if (otherClusterNodePathPlans.find(otherPathingClusterCode) == otherClusterNodePathPlans.end())
			{
				otherClusterNodePathPlans[otherPathingClusterCode] = otherClusterPaths[otherPathingCluster];
				otherClusterPathings[otherPathingClusterCode] = { otherPathingCluster, otherPathingCluster };

				otherClosestClusterPathWeights.insert({ otherClusterPathWeights[otherPathingCluster], otherPathingCluster });
			}
		}

		unsigned int maxClosestClusters = 30;
		unsigned int maxPathingClusters = 200;
		std::map<PathingCluster*, float> closestClusterPaths;
		auto endItCluster = std::next(
			closestClusterPathWeights.begin(), std::min((size_t)maxClosestClusters, closestClusterPathWeights.size()));
		for (auto itCluster = closestClusterPathWeights.begin(); itCluster != endItCluster; ++itCluster)
			closestClusterPaths[(*itCluster).second] = (*itCluster).first;
		BuildExpandedPath(graph, maxPathingClusters, clusterNodeStart,
			clusterPaths, closestClusterPaths, clusterPathings, clusterNodePathPlans);

		std::map<PathingCluster*, float> otherClosestClusterPaths;
		auto endItOtherCluster = std::next(
			otherClosestClusterPathWeights.begin(), std::min((size_t)maxClosestClusters, otherClosestClusterPathWeights.size()));
		for (auto itOtherCluster = otherClosestClusterPathWeights.begin(); itOtherCluster != endItOtherCluster; ++itOtherCluster)
			otherClosestClusterPaths[(*itOtherCluster).second] = (*itOtherCluster).first;
		BuildExpandedPath(graph, maxPathingClusters, otherClusterNodeStart,
			otherClusterPaths, otherClosestClusterPaths, otherClusterPathings, otherClusterNodePathPlans);
	}
	 
	return visibleClusters.size();
}

bool QuakeAIManager::BuildLongPath(std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans)
{
	std::unordered_map<unsigned int, PathingNode*> clusterNodes, otherClusterNodes;

	std::map<PathingCluster*, PathingArcVec> clusterPaths, otherClusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights, otherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 260, clusterPaths, clusterPathWeights);
	for (auto& clusterPath : clusterPaths)
		clusterNodes[clusterPath.first->GetTarget()->GetCluster()] = clusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	std::map<PathingCluster*, PathingArcVec> jumpClusterPaths, jumpOtherClusterPaths;
	std::map<PathingCluster*, float> jumpClusterPathWeights, jumpOtherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_JUMP, 260, jumpClusterPaths, jumpClusterPathWeights);
	for (auto& jumpClusterPath : jumpClusterPaths)
	{
		if (clusterNodes.find(jumpClusterPath.first->GetTarget()->GetCluster()) == clusterNodes.end())
		{
			clusterPaths[jumpClusterPath.first] = jumpClusterPath.second;
			clusterPathWeights[jumpClusterPath.first] = jumpClusterPathWeights[jumpClusterPath.first];
		}
	}

	for (auto& clusterPath : clusterPaths)
	{
		PathingCluster* pathingCluster = clusterPath.first;
		PathingNode* pathingClusterNodeEnd = graph->FindClusterNode(pathingCluster->GetTarget()->GetCluster());

		unsigned long long pathingClusterCode =
			(unsigned long long)pathingCluster->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
			(unsigned long long)pathingClusterNodeEnd->GetId() << 32 | (unsigned long long)pathingCluster->GetType() << 28 |
			(unsigned long long)pathingClusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

		clusterNodePathPlans[pathingClusterCode] = clusterPath.second;
		clusterPathings[pathingClusterCode] = { pathingCluster, pathingCluster };
	}

	return true;
}

bool QuakeAIManager::BuildLongPath(
	std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans)
{
	std::unordered_map<unsigned int, PathingNode*> clusterNodes, otherClusterNodes;

	std::map<PathingCluster*, PathingArcVec> clusterPaths, otherClusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights, otherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 200, clusterPaths, clusterPathWeights);
	for (auto& clusterPath : clusterPaths)
		clusterNodes[clusterPath.first->GetTarget()->GetCluster()] = clusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	std::map<PathingCluster*, PathingArcVec> jumpClusterPaths, jumpOtherClusterPaths;
	std::map<PathingCluster*, float> jumpClusterPathWeights, jumpOtherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_JUMP, 200, jumpClusterPaths, jumpClusterPathWeights);
	for (auto& jumpClusterPath : jumpClusterPaths)
	{
		if (clusterNodes.find(jumpClusterPath.first->GetTarget()->GetCluster()) == clusterNodes.end())
		{
			clusterPaths[jumpClusterPath.first] = jumpClusterPath.second;
			clusterPathWeights[jumpClusterPath.first] = jumpClusterPathWeights[jumpClusterPath.first];
		}
	}

	//skip top clusters to reduce the number of candidates as they have been inspected previously
	unsigned int skipPathingClusters = 80;
	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> pathingClusterNodes;
	std::multimap<float, PathingCluster*, std::less<float>> closestClusterPathWeights;
	for (auto& clusterPathWeight : clusterPathWeights)
		closestClusterPathWeights.insert({ clusterPathWeight.second, clusterPathWeight.first });

	auto itCluster = closestClusterPathWeights.begin();
	if (closestClusterPathWeights.size() > 80)
	{
		itCluster = std::next(closestClusterPathWeights.begin(), 
			std::min((size_t)skipPathingClusters, closestClusterPathWeights.size()));
	}
	for (; itCluster != closestClusterPathWeights.end(); ++itCluster)
	{
		PathingCluster* clusterPath = (*itCluster).second;
		for (auto& clusterPathArc : clusterPaths[clusterPath])
		{
			PathingTransition* clusterPathTransition = clusterPathArc->GetTransition();
			pathingClusterNodes[clusterPathArc->GetNode()][clusterPath] =
				clusterPath->GetType() << 28 | clusterPath->GetTarget()->GetId();
			for (auto& clusterPathNode : clusterPathTransition->GetNodes())
			{
				pathingClusterNodes[clusterPathNode][clusterPath] =
					clusterPath->GetType() << 28 | clusterPath->GetTarget()->GetId();
			}
		}
	}

	otherClusterNodeStart->GetClusters(AT_MOVE, 200, otherClusterPaths, otherClusterPathWeights);
	for (auto& otherClusterPath : otherClusterPaths)
		otherClusterNodes[otherClusterPath.first->GetTarget()->GetCluster()] = otherClusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	otherClusterNodeStart->GetClusters(AT_JUMP, 200, jumpOtherClusterPaths, jumpOtherClusterPathWeights);
	for (auto& jumpOtherClusterPath : jumpOtherClusterPaths)
	{
		if (otherClusterNodes.find(jumpOtherClusterPath.first->GetTarget()->GetCluster()) == otherClusterNodes.end())
		{
			otherClusterPaths[jumpOtherClusterPath.first] = jumpOtherClusterPath.second;
			otherClusterPathWeights[jumpOtherClusterPath.first] = jumpOtherClusterPathWeights[jumpOtherClusterPath.first];
		}
	}

	//skip top clusters to reduce the number of candidates as they have been inspected previously
	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> otherPathingClusterNodes;
	std::multimap<float, PathingCluster*, std::less<float>> otherClosestClusterPathWeights;
	for (auto& otherClusterPathWeight : otherClusterPathWeights)
		otherClosestClusterPathWeights.insert({ otherClusterPathWeight.second, otherClusterPathWeight.first });

	auto itOtherCluster = otherClosestClusterPathWeights.begin();
	if (otherClosestClusterPathWeights.size() > 80)
	{
		itOtherCluster = std::next(otherClosestClusterPathWeights.begin(),
			std::min((size_t)skipPathingClusters, otherClosestClusterPathWeights.size()));
	}
	for (; itOtherCluster != otherClosestClusterPathWeights.end(); ++itOtherCluster)
	{
		PathingCluster* otherClusterPath = (*itOtherCluster).second;
		for (auto& otherClusterPathArc : otherClusterPaths[otherClusterPath])
		{
			PathingTransition* otherClusterPathTransition = otherClusterPathArc->GetTransition();
			otherPathingClusterNodes[otherClusterPathArc->GetNode()][otherClusterPath] =
				otherClusterPath->GetType() << 28 | otherClusterPath->GetTarget()->GetId();
			for (auto& otherClusterPathNode : otherClusterPathTransition->GetNodes())
			{
				otherPathingClusterNodes[otherClusterPathNode][otherClusterPath] =
					otherClusterPath->GetType() << 28 | otherClusterPath->GetTarget()->GetId();
			}
		}
	}

	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>> visibleClusters;
	Concurrency::parallel_for_each(begin(pathingClusterNodes), end(pathingClusterNodes), [&](auto& pathingClusterNode)
	//for (auto& pathingClusterNode : pathingClusterNodes)
	{
		Concurrency::parallel_for_each(begin(otherPathingClusterNodes), end(otherPathingClusterNodes), [&](auto& otherPathingClusterNode)
		//for (auto& otherPathingClusterNode : otherPathingClusterNodes)
		{
			if (pathingClusterNode.first->IsVisibleNode(otherPathingClusterNode.first))
			{
				for (auto& pathingCluster : pathingClusterNode.second)
				{
					for (auto& otherPathingCluster : otherPathingClusterNode.second)
					{
						unsigned long long clusterCode =
							(unsigned long long)pathingCluster.second << 32 | (unsigned long long)otherPathingCluster.second;
						visibleClusters[clusterCode] = std::make_pair(pathingCluster.first, otherPathingCluster.first);
					}
				}
			}
		});
	});

	if (visibleClusters.size())
	{
		//we will only process those clusters which are visibles from both players
		closestClusterPathWeights.clear();
		otherClosestClusterPathWeights.clear();
		for (auto& visibleCluster : visibleClusters)
		{
			PathingCluster* pathingCluster = visibleCluster.second.first;
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode(pathingCluster->GetTarget()->GetCluster());

			unsigned long long pathingClusterCode =
				(unsigned long long)pathingCluster->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)pathingClusterNodeEnd->GetId() << 32 | (unsigned long long)pathingCluster->GetType() << 28 |
				(unsigned long long)pathingClusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

			//Build path based on cluster pathway
			if (clusterNodePathPlans.find(pathingClusterCode) == clusterNodePathPlans.end())
			{
				clusterNodePathPlans[pathingClusterCode] = clusterPaths[pathingCluster];
				clusterPathings[pathingClusterCode] = { pathingCluster, pathingCluster };

				closestClusterPathWeights.insert({ clusterPathWeights[pathingCluster], pathingCluster });
			}

			PathingCluster* otherPathingCluster = visibleCluster.second.second;
			PathingNode* otherPathingClusterNodeEnd = graph->FindClusterNode(otherPathingCluster->GetTarget()->GetCluster());

			unsigned long long otherPathingClusterCode =
				(unsigned long long)otherPathingCluster->GetType() << 60 | (unsigned long long)otherClusterNodeStart->GetId() << 46 |
				(unsigned long long)otherPathingClusterNodeEnd->GetId() << 32 | (unsigned long long)otherPathingCluster->GetType() << 28 |
				(unsigned long long)otherPathingClusterNodeEnd->GetId() << 14 | (unsigned long long)otherPathingClusterNodeEnd->GetId();

			//Build path based on cluster pathway
			if (otherClusterNodePathPlans.find(otherPathingClusterCode) == otherClusterNodePathPlans.end())
			{
				otherClusterNodePathPlans[otherPathingClusterCode] = otherClusterPaths[otherPathingCluster];
				otherClusterPathings[otherPathingClusterCode] = { otherPathingCluster, otherPathingCluster };

				otherClosestClusterPathWeights.insert({ otherClusterPathWeights[otherPathingCluster], otherPathingCluster });
			}
		}

		unsigned int maxClosestClusters = 30;
		unsigned int maxPathingClusters = 200;
		std::map<PathingCluster*, float> closestClusterPaths;
		auto endItCluster = std::next(
			closestClusterPathWeights.begin(), std::min((size_t)maxClosestClusters, closestClusterPathWeights.size()));
		for (auto itCluster = closestClusterPathWeights.begin(); itCluster != endItCluster; ++itCluster)
			closestClusterPaths[(*itCluster).second] = (*itCluster).first;
		BuildExpandedPath(graph, maxPathingClusters, clusterNodeStart,
			clusterPaths, closestClusterPaths, clusterPathings, clusterNodePathPlans);

		std::map<PathingCluster*, float> otherClosestClusterPaths;
		auto endItOtherCluster = std::next(
			otherClosestClusterPathWeights.begin(), std::min((size_t)maxClosestClusters, otherClosestClusterPathWeights.size()));
		for (auto itOtherCluster = otherClosestClusterPathWeights.begin(); itOtherCluster != endItOtherCluster; ++itOtherCluster)
			otherClosestClusterPaths[(*itOtherCluster).second] = (*itOtherCluster).first;
		BuildExpandedPath(graph, maxPathingClusters, otherClusterNodeStart,
			otherClusterPaths, otherClosestClusterPaths, otherClusterPathings, otherClusterNodePathPlans);

		return true;
	}
	
	return false;
}

bool QuakeAIManager::BuildLongestPath(
	std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans)
{
	std::unordered_map<unsigned int, PathingNode*> clusterNodes, otherClusterNodes;

	std::map<PathingCluster*, PathingArcVec> clusterPaths, otherClusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights, otherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 260, clusterPaths, clusterPathWeights);
	for (auto& clusterPath : clusterPaths)
		clusterNodes[clusterPath.first->GetTarget()->GetCluster()] = clusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	std::map<PathingCluster*, PathingArcVec> jumpClusterPaths, jumpOtherClusterPaths;
	std::map<PathingCluster*, float> jumpClusterPathWeights, jumpOtherClusterPathWeights;
	clusterNodeStart->GetClusters(AT_JUMP, 260, jumpClusterPaths, jumpClusterPathWeights);
	for (auto& jumpClusterPath : jumpClusterPaths)
	{
		if (clusterNodes.find(jumpClusterPath.first->GetTarget()->GetCluster()) == clusterNodes.end())
		{
			clusterPaths[jumpClusterPath.first] = jumpClusterPath.second;
			clusterPathWeights[jumpClusterPath.first] = jumpClusterPathWeights[jumpClusterPath.first];
		}
	}

	//skip top clusters to reduce the number of candidates as they have been inspected previously
	unsigned int skipPathingClusters = 140;
	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> pathingClusterNodes;
	std::multimap<float, PathingCluster*, std::less<float>> closestClusterPathWeights;
	for (auto& clusterPathWeight : clusterPathWeights)
		closestClusterPathWeights.insert({ clusterPathWeight.second, clusterPathWeight.first });

	auto itCluster = closestClusterPathWeights.begin();
	if (closestClusterPathWeights.size() > 140)
	{
		itCluster = std::next(closestClusterPathWeights.begin(),
			std::min((size_t)skipPathingClusters, closestClusterPathWeights.size()));
	}
	for (; itCluster != closestClusterPathWeights.end(); ++itCluster)
	{
		PathingCluster* pathingCluster = (*itCluster).second;
		PathingNode* pathingClusterNodeEnd = graph->FindClusterNode(pathingCluster->GetTarget()->GetCluster());

		unsigned long long pathingClusterCode =
			(unsigned long long)pathingCluster->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
			(unsigned long long)pathingClusterNodeEnd->GetId() << 32 | (unsigned long long)pathingCluster->GetType() << 28 |
			(unsigned long long)pathingClusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

		clusterNodePathPlans[pathingClusterCode] = clusterPaths[pathingCluster];
		clusterPathings[pathingClusterCode] = { pathingCluster, pathingCluster };
	}

	otherClusterNodeStart->GetClusters(AT_MOVE, 260, otherClusterPaths, otherClusterPathWeights);
	for (auto& otherClusterPath : otherClusterPaths)
		otherClusterNodes[otherClusterPath.first->GetTarget()->GetCluster()] = otherClusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	otherClusterNodeStart->GetClusters(AT_JUMP, 260, jumpOtherClusterPaths, jumpOtherClusterPathWeights);
	for (auto& jumpOtherClusterPath : jumpOtherClusterPaths)
	{
		if (otherClusterNodes.find(jumpOtherClusterPath.first->GetTarget()->GetCluster()) == otherClusterNodes.end())
		{
			otherClusterPaths[jumpOtherClusterPath.first] = jumpOtherClusterPath.second;
			otherClusterPathWeights[jumpOtherClusterPath.first] = jumpOtherClusterPathWeights[jumpOtherClusterPath.first];
		}
	}

	//skip top clusters to reduce the number of candidates as they have been inspected previously
	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> otherPathingClusterNodes;
	std::multimap<float, PathingCluster*, std::less<float>> otherClosestClusterPathWeights;
	for (auto& otherClusterPathWeight : otherClusterPathWeights)
		otherClosestClusterPathWeights.insert({ otherClusterPathWeight.second, otherClusterPathWeight.first });

	auto itOtherCluster = otherClosestClusterPathWeights.begin();
	if (otherClosestClusterPathWeights.size() > 140)
	{
		itOtherCluster = std::next(otherClosestClusterPathWeights.begin(),
			std::min((size_t)skipPathingClusters, otherClosestClusterPathWeights.size()));
	}
	for (; itOtherCluster != otherClosestClusterPathWeights.end(); ++itOtherCluster)
	{
		PathingCluster* otherPathingCluster = (*itOtherCluster).second;
		PathingNode* otherPathingClusterNodeEnd = graph->FindClusterNode(otherPathingCluster->GetTarget()->GetCluster());

		unsigned long long otherPathingClusterCode =
			(unsigned long long)otherPathingCluster->GetType() << 60 | (unsigned long long)otherClusterNodeStart->GetId() << 46 |
			(unsigned long long)otherPathingClusterNodeEnd->GetId() << 32 | (unsigned long long)otherPathingCluster->GetType() << 28 |
			(unsigned long long)otherPathingClusterNodeEnd->GetId() << 14 | (unsigned long long)otherPathingClusterNodeEnd->GetId();

		otherClusterNodePathPlans[otherPathingClusterCode] = otherClusterPaths[otherPathingCluster];
		otherClusterPathings[otherPathingClusterCode] = { otherPathingCluster, otherPathingCluster };
	}

	return true;
}

void QuakeAIManager::BuildExpandedPath(
	std::shared_ptr<PathingGraph>& graph, unsigned int maxPathingClusters, PathingNode* clusterNodeStart,
	const std::map<PathingCluster*, PathingArcVec>& clusterPaths, const std::map<PathingCluster*, float>& expandClusterPathWeights,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans)
{
	unsigned int pathingClustersLimit = maxPathingClusters;
	unsigned int clusterPathSize = (unsigned int)expandClusterPathWeights.size();
	if (clusterPathSize > 7)
	{
		pathingClustersLimit = 30;
		maxPathingClusters = maxPathingClusters / clusterPathSize;
	}
	else if (clusterPathSize > 0)
	{
		pathingClustersLimit = maxPathingClusters / clusterPathSize;
		maxPathingClusters = maxPathingClusters / clusterPathSize;
	}

	std::mutex mutex;

	//we will expand only with move type clusters
	Concurrency::parallel_for_each(
		begin(expandClusterPathWeights), end(expandClusterPathWeights), [&](auto const& clusterPathWeight)
	//for (auto& clusterPathWeight : expandClusterPathWeights)
	{
		PathingNode* clusterNodeEnd = graph->FindClusterNode(clusterPathWeight.first->GetTarget()->GetCluster());

		//lets try to add surrounding clusters
		std::map<PathingCluster*, PathingArcVec> pathingClusters;
		std::multimap<float, PathingCluster*, std::greater<float>> pathingClusterWeights;
		clusterPathWeight.first->GetTarget()->GetClusters(AT_MOVE, pathingClustersLimit, pathingClusters, pathingClusterWeights);

		auto endItCluster = std::next(
			pathingClusterWeights.begin(), std::min((size_t)maxPathingClusters, pathingClusterWeights.size()));
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != endItCluster; ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());

			unsigned long long pathingClusterCode =
				(unsigned long long)clusterPathWeight.first->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)(*itCluster).second->GetType() << 28 |
				(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

			mutex.lock();
			//Build path based on cluster pathway
			clusterNodePathPlans[pathingClusterCode] = clusterPaths.at(clusterPathWeight.first);
			clusterNodePathPlans[pathingClusterCode].insert(clusterNodePathPlans[pathingClusterCode].end(),
				pathingClusters[(*itCluster).second].begin(), pathingClusters[(*itCluster).second].end());
			clusterPathings[pathingClusterCode] = { clusterPathWeight.first, (*itCluster).second };
			mutex.unlock();
		}
	});
}

void QuakeAIManager::BuildExpandedActorPath(
	std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters,
	Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics)
{
	//search surrounding clusters
	std::multimap<float, PathingCluster*, std::greater<float>> clusterPathHeuristics;
	std::map<PathingCluster*, PathingArcVec> clusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 100, clusterPaths, clusterPathWeights);
	clusterNodeStart->GetClusters(AT_JUMP, 100, clusterPaths, clusterPathWeights);
	for (auto& clusterPathWeight : clusterPathWeights)
	{
		unsigned int actionType = clusterPathWeight.first->GetType();
		PathingNode* clusterNodeEnd = graph->FindClusterNode(clusterPathWeight.first->GetTarget()->GetCluster());

		unsigned long long clusterCode =
			(unsigned long long)actionType << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
			(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)actionType << 28 |
			(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)clusterNodeEnd->GetId();

		//we only take closest actor clusters
		if (actorPathPlanClusters.find(clusterCode) != actorPathPlanClusters.end())
			clusterPathHeuristics.insert({actorPathPlanClusterHeuristics[clusterCode], clusterPathWeight.first});
	}

	unsigned int maxBestClusters = 0;
	std::map<PathingCluster*, unsigned long long> bestClusterPaths;

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };
	for (auto& actionType : actionTypes)
	{
		maxBestClusters += 1;
		for (auto& clusterPathHeuristic : clusterPathHeuristics)
		{
			if (actionType != clusterPathHeuristic.second->GetType())
				continue;
			
			PathingNode* clusterNodeEnd = graph->FindClusterNode(clusterPathHeuristic.second->GetTarget()->GetCluster());

			unsigned long long clusterCode =
				(unsigned long long)actionType << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)actionType << 28 |
				(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)clusterNodeEnd->GetId();

			bestClusterPaths[clusterPathHeuristic.second] = clusterCode;
			if (bestClusterPaths.size() >= maxBestClusters)
				break;
		}
	}

	std::mutex mutex;

	//we will expand only with move type clusters
	Concurrency::parallel_for_each(begin(bestClusterPaths), end(bestClusterPaths), [&](auto const& bestClusterPath)
	//for (auto& bestClusterPath : bestClusterPaths)
	{
		PathingNode* actorPathNode = actorPathPlanClusters[bestClusterPath.second].empty() ?
			clusterNodeStart : actorPathPlanClusters[bestClusterPath.second].back()->GetNode();
		PathingNode* clusterNodeEnd = graph->FindClusterNode(actorPathNode->GetCluster());

		//lets try to add surrounding clusters
		std::map<PathingCluster*, PathingArcVec> pathingClusters;
		std::multimap<float, PathingCluster*, std::greater<float>> pathingClusterWeights;
		actorPathNode->GetClusters(AT_MOVE, 60, pathingClusters, pathingClusterWeights);

		size_t maxPathingClusters = 20;
		auto endItCluster = std::next(
			pathingClusterWeights.begin(), std::min((size_t)maxPathingClusters, pathingClusterWeights.size()));
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != endItCluster; ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());

			unsigned long long pathingClusterCode =
				(unsigned long long)bestClusterPath.first->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)(*itCluster).second->GetType() << 28 |
				(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

			mutex.lock();
			//Build path based on cluster pathway
			actorPathPlanClusterHeuristics[pathingClusterCode] = actorPathPlanClusterHeuristics[bestClusterPath.second];
			actorPathPlanClusters[pathingClusterCode] = actorPathPlanClusters[bestClusterPath.second];
			actorPathPlanClusters[pathingClusterCode].insert(actorPathPlanClusters[pathingClusterCode].end(),
				pathingClusters[(*itCluster).second].begin(), pathingClusters[(*itCluster).second].end());
			clusterPathings[pathingClusterCode] = { bestClusterPath.first, (*itCluster).second };
			mutex.unlock();
		}
	});
}

void QuakeAIManager::BuildExpandedActorPath(
	std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, float heuristicThreshold,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters,
	Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics)
{
	//search surrounding clusters
	std::multimap<float, PathingCluster*, std::greater<float>> clusterPathHeuristics;
	std::map<PathingCluster*, PathingArcVec> clusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 100, clusterPaths, clusterPathWeights);
	clusterNodeStart->GetClusters(AT_JUMP, 100, clusterPaths, clusterPathWeights);
	for (auto& clusterPathWeight : clusterPathWeights)
	{
		unsigned int actionType = clusterPathWeight.first->GetType();
		PathingNode* clusterNodeEnd = graph->FindClusterNode(clusterPathWeight.first->GetTarget()->GetCluster());

		unsigned long long clusterCode =
			(unsigned long long)actionType << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
			(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)actionType << 28 |
			(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)clusterNodeEnd->GetId();

		//we only take closest actor clusters
		if (actorPathPlanClusters.find(clusterCode) != actorPathPlanClusters.end())
			clusterPathHeuristics.insert({actorPathPlanClusterHeuristics[clusterCode], clusterPathWeight.first});
	}

	std::map<PathingCluster*, unsigned long long> bestClusterPaths;
	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };
	for (auto& actionType : actionTypes)
	{
		for (auto& clusterPathHeuristic : clusterPathHeuristics)
		{
			if (actionType != clusterPathHeuristic.second->GetType())
				continue;
			
			PathingNode* clusterNodeEnd = graph->FindClusterNode(clusterPathHeuristic.second->GetTarget()->GetCluster());

			unsigned long long clusterCode =
				(unsigned long long)actionType << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)actionType << 28 |
				(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)clusterNodeEnd->GetId();

			if (clusterPathHeuristic.first >= heuristicThreshold)
				bestClusterPaths[clusterPathHeuristic.second] = clusterCode;
		}
	}

	std::mutex mutex;

	//we will expand only with move type clusters
	Concurrency::parallel_for_each(begin(bestClusterPaths), end(bestClusterPaths), [&](auto const& bestClusterPath)
	//for (auto& bestClusterPath : bestClusterPaths)
	{
		PathingNode* actorPathNode = actorPathPlanClusters[bestClusterPath.second].empty() ?
			clusterNodeStart : actorPathPlanClusters[bestClusterPath.second].back()->GetNode();
		PathingNode* clusterNodeEnd = graph->FindClusterNode(actorPathNode->GetCluster());

		//lets try to add surrounding clusters
		std::map<PathingCluster*, PathingArcVec> pathingClusters;
		std::multimap<float, PathingCluster*, std::greater<float>> pathingClusterWeights;
		actorPathNode->GetClusters(AT_MOVE, 60, pathingClusters, pathingClusterWeights);

		size_t maxPathingClusters = 20;
		auto endItCluster = std::next(
			pathingClusterWeights.begin(), std::min((size_t)maxPathingClusters, pathingClusterWeights.size()));
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != endItCluster; ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());

			unsigned long long pathingClusterCode =
				(unsigned long long)bestClusterPath.first->GetType() << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 |
				(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)(*itCluster).second->GetType() << 28 |
				(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)pathingClusterNodeEnd->GetId();

			mutex.lock();
			//Build path based on cluster pathway
			actorPathPlanClusterHeuristics[pathingClusterCode] = actorPathPlanClusterHeuristics[bestClusterPath.second];
			actorPathPlanClusters[pathingClusterCode] = actorPathPlanClusters[bestClusterPath.second];
			actorPathPlanClusters[pathingClusterCode].insert(actorPathPlanClusters[pathingClusterCode].end(),
				pathingClusters[(*itCluster).second].begin(), pathingClusters[(*itCluster).second].end());
			clusterPathings[pathingClusterCode] = { bestClusterPath.first, (*itCluster).second };
			mutex.unlock();
		}
	});
}

void QuakeAIManager::BuildActorPath(std::shared_ptr<PathingGraph>& graph,
	unsigned int actionType, const std::map<ActorId, float>& gameItems, const std::map<ActorId, float>& searchItems,
	const PlayerData& player, PathingNode* clusterNodeStart, const PathingArcVec& clusterPathStart, float clusterPathOffset,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters)
{
	std::map<PathingActorVec, float> actorsPathPlans;
	FindPathPlans(clusterNodeStart, searchItems, actorsPathPlans, actionType);

	unsigned short actorIndex = 0;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorWeights;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingActorVec> actorPaths;
	for (auto& actorsPathPlan : actorsPathPlans)
	{
		actorPaths[actorIndex] = std::move(actorsPathPlan.first);
		actorWeights[actorIndex] += std::move(actorsPathPlan.second);
		actorIndex++;
	}

	float clusterPathWeight = 0.f;
	for (auto& clusterPath : clusterPathStart)
		clusterPathWeight += clusterPath->GetWeight();

	std::mutex mutex;

	Concurrency::concurrent_unordered_map<unsigned int, 
		Concurrency::concurrent_unordered_map<unsigned long long, float>> actorClustersHeuristics;
	Concurrency::concurrent_unordered_map<unsigned int, 
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned long long>> actorClustersCodes;
	Concurrency::concurrent_unordered_map<unsigned int, 
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>> actorClustersPaths;
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>> actorClusters;
	Concurrency::parallel_for_each(begin(actorPaths), end(actorPaths), [&](auto const& actorPath)
	//for (auto const& actorPath : actorPaths)
	{
		std::map<ActorId, float> actors;
		PathingArcVec actorPathPlan = clusterPathStart;
		float actorPathWeight = clusterPathWeight - clusterPathOffset;

		PathingNode* currentActorNode = clusterNodeStart;
		for (PathingActor* pathingActor : actorPath.second)
		{
			while (currentActorNode != pathingActor->GetTarget())
			{
				PathingActor* currentActor = currentActorNode->FindActor(actionType, pathingActor->GetTarget());
				PathingArc* currentArc = currentActorNode->FindArc(currentActor->GetNode());

				actorPathPlan.push_back(currentArc);
				actorPathWeight += currentArc->GetWeight();

				currentActorNode = currentArc->GetNode();
			}

			actors[pathingActor->GetActor()] = actorPathWeight;
		}

		PathingNode* actorNodeEnd = graph->FindClusterNode(currentActorNode->GetCluster());

		unsigned long long clusterActorCode = 
			(unsigned long long)actionType << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 | 
			(unsigned long long)actorNodeEnd->GetId() << 32 | (unsigned long long)actionType << 28 | 
			(unsigned long long)actorNodeEnd->GetId() << 14 | (unsigned long long)actorNodeEnd->GetId();

		//printf("\ncluster code %u", clusterActorCode);
		if (clusterNodePathPlans.find(clusterActorCode) == clusterNodePathPlans.end())
		{
			PathingCluster* pathingCluster = clusterNodeStart->FindCluster(actionType, currentActorNode->GetCluster());
			if (pathingCluster)
			{
				PathingNode* clusterNodeEnd = graph->FindClusterNode(pathingCluster->GetTarget()->GetCluster());

				unsigned long long clusterCode =
					(unsigned long long)actionType << 60 | (unsigned long long)clusterNodeStart->GetId() << 46 | 
					(unsigned long long)clusterNodeEnd->GetId() << 32 | (unsigned long long)actionType << 28 | 
					(unsigned long long)clusterNodeEnd->GetId() << 14 | (unsigned long long)clusterNodeEnd->GetId();
				//printf("\ncluster code %u", clusterCode);

				PathingNode* currentNode = clusterNodeStart;
				while (currentNode != pathingCluster->GetTarget())
				{
					PathingCluster* currentCluster = currentNode->FindCluster(actionType, pathingCluster->GetTarget());
					PathingArc* currentArc = currentNode->FindArc(currentCluster->GetNode());

					currentNode = currentArc->GetNode();
				}

				//make sure that all items can be taken
				bool takeItems = true;
				PlayerData playerData(player);
				for (auto& actor : actors)
				{
					if (!CanItemBeGrabbed(actor.first, actor.second, playerData, gameItems))
					{
						takeItems = false;
						break;
					}
				}

				if (takeItems)
				{
					PickupItems(playerData, actors, gameItems);
					float nodeHeuristic = CalculateHeuristicItems(playerData);
					if (nodeHeuristic >= 0.03f)
					{
						mutex.lock();
						actorClusters[clusterCode] = { pathingCluster, pathingCluster };
						actorClustersCodes[currentNode->GetCluster()][actorPath.first] = clusterCode;
						actorClustersHeuristics[currentNode->GetCluster()][actorPath.first] = nodeHeuristic;
						actorClustersPaths[currentNode->GetCluster()][actorPath.first] = std::move(actorPathPlan);
						mutex.unlock();
					}
				}
			}
		}
		else
		{
			//make sure that all items can be taken
			bool takeItems = true;
			PlayerData playerData(player);
			for (auto& actor : actors)
			{
				if (!CanItemBeGrabbed(actor.first, actor.second, playerData, gameItems))
				{
					takeItems = false;
					break;
				}
			}

			if (takeItems)
			{
				PickupItems(playerData, actors, gameItems);
				float nodeHeuristic = CalculateHeuristicItems(playerData);
				if (nodeHeuristic >= 0.03f)
				{
					mutex.lock();
					actorClustersHeuristics[currentActorNode->GetCluster()][actorPath.first] = nodeHeuristic;
					actorClustersCodes[currentActorNode->GetCluster()][actorPath.first] = clusterActorCode;
					actorClustersPaths[currentActorNode->GetCluster()][actorPath.first] = actorPathPlan;
					mutex.unlock();
				}
			}
		}
	});

	//finally store results in output arrays
	for (auto const& actorClusterCodes : actorClustersCodes)
	{
		unsigned long long clusterCode = 0;
		float actorHeuristic = -FLT_MAX;
		for (auto const& actorClusterCode : actorClusterCodes.second)
		{
			if (actorClustersHeuristics[actorClusterCodes.first][actorClusterCode.first] > actorHeuristic)
			{
				clusterCode = actorClusterCode.second;
				actorHeuristic = actorClustersHeuristics[actorClusterCodes.first][actorClusterCode.first];

				actorPathPlanClusters[clusterCode] = 
					std::move(actorClustersPaths[actorClusterCodes.first][actorClusterCode.first]);
				actorPathPlanClusterHeuristics[clusterCode] = actorHeuristic;
			}
		}

		if (actorClusters.find(clusterCode) != actorClusters.end())
			clusterPathings[clusterCode] = actorClusters[clusterCode];
	}

	//printf("\n actor path plans %u", actorsPathPlans.size());
	//printf("\n actor weight %u", actorWeightClusters.size());
}

void QuakeAIManager::BuildPlayerPath(const AIAnalysis::PlayerSimulation& playerSimulation,
	PathingNode* playerNode, float playerPathOffset, PathingArcVec& playerPathPlan)
{
	PathingNode* pathingNode = playerNode;
	for (auto path : playerSimulation.planPath)
	{
		PathingArc* pathingArc = pathingNode->FindArc(path);
		playerPathPlan.push_back(pathingArc);

		pathingNode = pathingArc->GetNode();
	}
}

//ANALYSIS SIMULATIONS

bool QuakeAIManager::SimulatePlayerGuessingDecision(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> otherPlayerPaths;
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<unsigned int, unsigned int>> otherPlayerClusters;

	//PrintInfo("\nGuessing clusters: ");
	if (otherClusterNodeStart)
	{
		unsigned long long otherPlayerIdx = ULLONG_MAX;
		unsigned int otherPlayerCluster = otherPlayerPathPlan.empty() ?
			otherClusterNodeStart->GetCluster() : otherPlayerPathPlan.back()->GetNode()->GetCluster();
		unsigned int otherPlayerClusterType = 0;

		otherPlayerPaths[otherPlayerIdx] = otherPlayerPathPlan;
		otherPlayerClusters[otherPlayerIdx] = std::make_pair(otherPlayerCluster, otherPlayerClusterType);
	}

	std::mutex mutex;

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long, 
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters;

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };
	Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
	//for (unsigned int actionType : actionTypes)
	{
		Concurrency::concurrent_unordered_map<unsigned long long,
			std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
		Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

		//player
		BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
			playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
			clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

		mutex.lock();
		clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

		actorPathPlanClusterHeuristics.insert(
			localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
		actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
		mutex.unlock();
	});

	float bestHeuristic = -FLT_MAX;
	float heuristicThreshold = 0.15f;
	for (auto& actorPathPlanClusterHeuristic : actorPathPlanClusterHeuristics)
		if (actorPathPlanClusterHeuristic.second > bestHeuristic)
			bestHeuristic = actorPathPlanClusterHeuristic.second;

	//if there are worthy items to be taken we will only build items paths, otherwise only normal paths.
	if (bestHeuristic < heuristicThreshold)
	{
		clusterPathings.clear();

		actorPathPlanClusterHeuristics.clear();
		actorPathPlanClusters.clear();

		BuildLongPath(mPathingGraph, clusterNodeStart, clusterPathings, clusterNodePathPlans);
	}
	else
	{
		BuildExpandedActorPath(mPathingGraph, clusterNodeStart, 
			heuristicThreshold, clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
	}

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_vector<AIAnalysis::GameSimulation*> playerDecisions;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		PathingCluster* playerClusterStart = (*itCluster).second.first;
		PathingCluster* playerClusterEnd = (*itCluster).second.second;

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for_each(
			begin(otherPlayerClusters), end(otherPlayerClusters), [&](auto const& otherPlayerCluster)
		//for (auto const& otherPlayerCluster : otherPlayerClusters)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPaths[otherPlayerCluster.first], otherPlayerPathOffset);

			player.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerCluster.second.first);
			simulation->otherPlayerSimulation.action = otherPlayerCluster.second.second;

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
		});

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			gameSimulation->clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			gameSimulation->clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			gameSimulation->action = playerClusterEnd->GetType();
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerDecisions.push_back(gameSimulation);
		}
	});

	if (playerDataIn.valid)
	{
		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for_each(
			begin(otherPlayerClusters), end(otherPlayerClusters), [&](auto const& otherPlayerCluster)
		//for (auto const& otherPathingClusterNode : otherPathingClusterNodes)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPaths[otherPlayerCluster.first], otherPlayerPathOffset );

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else 
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerCluster.second.first);
			simulation->otherPlayerSimulation.action = otherPlayerCluster.second.second;

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
		});

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			if (playerDataIn.plan.path.empty())
				gameSimulation->clusters.push_back(playerDataIn.plan.node->GetCluster());
			else
				gameSimulation->clusters.push_back(playerDataIn.plan.path.back()->GetNode()->GetCluster());
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerDecisions.push_back(gameSimulation);
		}
	}

	for (auto& playerDecision : playerDecisions)
		gameEvaluation.playerDecisions.push_back(std::move(playerDecision));

	gameEvaluation.playerDecision = new AIAnalysis::GameSimulation();
	for (auto const& gameSimulation : gameEvaluation.playerDecisions)
	{
		float playerHeuristic = FLT_MAX;
		float otherPlayerHeuristic = FLT_MAX;

		AIAnalysis::Simulation* playerSimulation = nullptr;
		for (auto const& simulation : gameSimulation->simulations)
		{
			//minimize the other player heuristic according to minimax decision level
			if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
			{
				playerHeuristic = simulation->playerSimulation.heuristic;
				otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;

				playerSimulation = simulation;
			}
		}

		gameEvaluation.playerDecision->simulations.push_back(playerSimulation);
	}
	
	//player decision output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformDecisionMaking(gameEvaluation, playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings, 
		playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);
	
	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 240; //lets add estimation of guessing simulation

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;
	for (PathingArc* playerPathArc : playerPathPlanOffset)
	{
		if (playerDataOut.planWeight <= 0.f)
			break;

		playerDataOut.plan.path.erase(playerDataOut.plan.path.begin());
		playerDataOut.plan.node = playerPathArc->GetNode();
		playerDataOut.planWeight -= playerPathArc->GetWeight();
	}

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	return true;
}

bool QuakeAIManager::SimulatePlayerGuessings(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart || clusterNodeStart == otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	std::map<ActorId, float> otherSearchItems;
	for (ActorId actor : searchActors)
		otherSearchItems[actor] = 0.f;
	CalculateWeightItems(otherPlayerDataIn, otherSearchItems);

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart, 
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart, 
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}
	else
	{
		if (!BuildLongPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);
	if (otherPlayerPathPlanOffset.size())
		for (auto& otherClusterNodePathPlan : otherClusterNodePathPlans)
			for (auto it = otherPlayerPathPlanOffset.rbegin(); it != otherPlayerPathPlanOffset.rend(); it++)
				otherClusterNodePathPlan.second.insert(otherClusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_vector<AIAnalysis::GameSimulation*> playerGuessings;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		PathingCluster* playerClusterStart = (*itCluster).second.first;
		PathingCluster* playerClusterEnd = (*itCluster).second.second;

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
			PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			player.plan.id = -1;
			otherPlayer.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = otherClusterCode;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterStart->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterEnd->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.action = otherPlayerClusterStart->GetType();

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
		});

		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			if (otherPlayer.plan.path.empty())
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.node->GetCluster());
			else
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.path.back()->GetNode()->GetCluster());

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
		}

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			gameSimulation->clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			gameSimulation->clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			gameSimulation->action = playerClusterEnd->GetType();
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerGuessings.push_back(gameSimulation);
		}
	});
	
	if (playerDataIn.valid)
	{
		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
			PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = otherClusterCode;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterStart->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterEnd->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.action = otherPlayerClusterStart->GetType();

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
		});

		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset, 
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			if (otherPlayer.plan.path.empty())
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.node->GetCluster());
			else
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.path.back()->GetNode()->GetCluster());

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
		}

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			if (playerDataIn.plan.path.empty())
				gameSimulation->clusters.push_back(playerDataIn.plan.node->GetCluster());
			else
				gameSimulation->clusters.push_back(playerDataIn.plan.path.back()->GetNode()->GetCluster());
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerGuessings.push_back(gameSimulation);
		}
	}

	for (auto& playerGuessing : playerGuessings)
		gameEvaluation.playerGuessings.push_back(std::move(playerGuessing));
	
	//printf("\n player guessing simulations end");
	//printf("\n player guessing decisions start");

	gameEvaluation.playerGuessDecision = new AIAnalysis::GameSimulation();
	for (auto const& gameSimulation : gameEvaluation.playerGuessings)
	{
		float playerHeuristic = FLT_MAX;
		float otherPlayerHeuristic = FLT_MAX;

		AIAnalysis::Simulation* playerGuessSimulation = nullptr;
		for (auto const& simulation : gameSimulation->simulations)
		{
			//minimize the other player heuristic according to minimax decision level
			if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
			{
				playerHeuristic = simulation->playerSimulation.heuristic;
				otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;

				playerGuessSimulation = simulation;
			}
		}

		gameEvaluation.playerGuessDecision->simulations.push_back(playerGuessSimulation);
	}

	//player guessing output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformGuessingMaking(gameEvaluation, playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings,
		playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);

	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			//other cluster code
			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
				itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
				if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
					itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 100; //lets add estimation of guessing decision simulation

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player guessing decisions end");
	return true;
}

bool QuakeAIManager::SimulatePlayerGuessing(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart || clusterNodeStart == otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	std::map<ActorId, float> otherSearchItems;
	for (ActorId actor : searchActors)
		otherSearchItems[actor] = 0.f;
	CalculateWeightItems(otherPlayerDataIn, otherSearchItems);

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart, 
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart, 
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}
	else
	{
		if (!BuildLongPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);
	if (otherPlayerPathPlanOffset.size())
		for (auto& otherClusterNodePathPlan : otherClusterNodePathPlans)
			for (auto it = otherPlayerPathPlanOffset.rbegin(); it != otherPlayerPathPlanOffset.rend(); it++)
				otherClusterNodePathPlan.second.insert(otherClusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_vector<AIAnalysis::GameSimulation*> playerGuessings;
	Concurrency::concurrent_unordered_map<size_t, Concurrency::concurrent_vector<AIAnalysis::Simulation*>> playerDecisions;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		PathingCluster* playerClusterStart = (*itCluster).second.first;
		PathingCluster* playerClusterEnd = (*itCluster).second.second;

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
			PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			player.plan.id = -1;
			otherPlayer.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = otherClusterCode;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterStart->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterEnd->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.action = otherPlayerClusterStart->GetType();

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerDecisions[otherClusterIdx].push_back(simulation);
		});

		if (otherPlayerDataIn.valid)
		{
			size_t otherClusterIdx = playerSimulations.size();

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			if (otherPlayer.plan.path.empty())
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.node->GetCluster());
			else
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.path.back()->GetNode()->GetCluster());

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerDecisions[otherClusterIdx].push_back(simulation);
		}

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			gameSimulation->clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			gameSimulation->clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			gameSimulation->action = playerClusterEnd->GetType();
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerGuessings.push_back(gameSimulation);
		}
	});
	
	if (playerDataIn.valid)
	{
		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
			PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = otherClusterCode;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterStart->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterEnd->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.action = otherPlayerClusterStart->GetType();

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerDecisions[otherClusterIdx].push_back(simulation);
		});

		if (otherPlayerDataIn.valid)
		{
			size_t otherClusterIdx = playerSimulations.size();

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset, 
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			if (otherPlayer.plan.path.empty())
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.node->GetCluster());
			else
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.path.back()->GetNode()->GetCluster());

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerDecisions[otherClusterIdx].push_back(simulation);
		}

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			if (playerDataIn.plan.path.empty())
				gameSimulation->clusters.push_back(playerDataIn.plan.node->GetCluster());
			else
				gameSimulation->clusters.push_back(playerDataIn.plan.path.back()->GetNode()->GetCluster());
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerGuessings.push_back(gameSimulation);
		}
	}

	for (auto& playerDecision : playerDecisions)
	{
		AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
		gameSimulation->clusters = playerDecision.second.front()->otherPlayerSimulation.clusters;
		gameSimulation->action = playerDecision.second.front()->otherPlayerSimulation.action;

		for (auto& playerDecisionsSimulation : playerDecision.second)
		{
			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation = playerDecisionsSimulation->otherPlayerSimulation;
			simulation->otherPlayerSimulation = playerDecisionsSimulation->playerSimulation;
			simulation->playerSimulation.heuristic = -simulation->playerSimulation.heuristic;
			simulation->otherPlayerSimulation.heuristic = -simulation->otherPlayerSimulation.heuristic;

			gameSimulation->simulations.push_back(simulation);
		}

		gameEvaluation.playerGuessings.push_back(gameSimulation);
	}

	//printf("\n player guessing simulations end");
	//printf("\n player guessing decisions start");
	gameEvaluation.playerGuessDecision = new AIAnalysis::GameSimulation();
	for (auto const& gameSimulation : gameEvaluation.playerGuessings)
	{
		float playerHeuristic = FLT_MAX;
		float otherPlayerHeuristic = FLT_MAX;

		AIAnalysis::Simulation* playerGuessSimulation = nullptr;
		for (auto const& simulation : gameSimulation->simulations)
		{
			//minimize the other player heuristic according to minimax decision level
			if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
			{
				playerHeuristic = simulation->playerSimulation.heuristic;
				otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;

				playerGuessSimulation = simulation;
			}
		}

		gameEvaluation.playerGuessDecision->simulations.push_back(playerGuessSimulation);
	}

	//player decision analysis data
	for (auto& playerGuessing : playerGuessings)
		gameEvaluation.playerDecisions.push_back(std::move(playerGuessing));

	gameEvaluation.playerDecision = new AIAnalysis::GameSimulation();
	for (auto const& gameSimulation : gameEvaluation.playerDecisions)
	{
		float playerHeuristic = FLT_MAX;
		float otherPlayerHeuristic = FLT_MAX;

		AIAnalysis::Simulation* playerDecisionSimulation = nullptr;
		for (auto const& simulation : gameSimulation->simulations)
		{
			//minimize the other player heuristic according to minimax decision level
			if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
			{
				playerHeuristic = simulation->playerSimulation.heuristic;
				otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;

				playerDecisionSimulation = simulation;
			}
		}

		gameEvaluation.playerDecision->simulations.push_back(playerDecisionSimulation);
	}

	//player decision output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformDecisionMaking(gameEvaluation, playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings,
		playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);

	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
				itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
				if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
					itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 40; //lets add marginal time

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;
	for (PathingArc* playerPathArc : playerPathPlanOffset)
	{
		if (playerDataOut.planWeight <= 0.f)
			break;

		playerDataOut.plan.path.erase(playerDataOut.plan.path.begin());
		playerDataOut.plan.node = playerPathArc->GetNode();
		playerDataOut.planWeight -= playerPathArc->GetWeight();
	}

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player guessing decisions end");
	return true;
}

bool QuakeAIManager::SimulatePlayerDecision(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart || clusterNodeStart == otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	std::map<ActorId, float> otherSearchItems;
	for (ActorId actor : searchActors)
		otherSearchItems[actor] = 0.f;
	CalculateWeightItems(otherPlayerDataIn, otherSearchItems);

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart, 
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart, 
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}
	else
	{
		if (!BuildLongPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);
	if (otherPlayerPathPlanOffset.size())
		for (auto& otherClusterNodePathPlan : otherClusterNodePathPlans)
			for (auto it = otherPlayerPathPlanOffset.rbegin(); it != otherPlayerPathPlanOffset.rend(); it++)
				otherClusterNodePathPlan.second.insert(otherClusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_vector<AIAnalysis::GameSimulation*> playerDecisions;
	Concurrency::concurrent_unordered_map<size_t, Concurrency::concurrent_vector<AIAnalysis::Simulation*>> playerGuessings;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		PathingCluster* playerClusterStart = (*itCluster).second.first;
		PathingCluster* playerClusterEnd = (*itCluster).second.second;

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
			PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			player.plan.id = -1;
			otherPlayer.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = otherClusterCode;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterStart->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterEnd->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.action = otherPlayerClusterStart->GetType();

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerGuessings[otherClusterIdx].push_back(simulation);
		});

		if (otherPlayerDataIn.valid)
		{
			size_t otherClusterIdx = playerSimulations.size();

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = clusterCode;
			simulation->playerSimulation.clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			simulation->playerSimulation.clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			simulation->playerSimulation.action = playerClusterStart->GetType();
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			if (otherPlayer.plan.path.empty())
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.node->GetCluster());
			else
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.path.back()->GetNode()->GetCluster());

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerGuessings[otherClusterIdx].push_back(simulation);
		}

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			gameSimulation->clusters.push_back(playerClusterStart->GetTarget()->GetCluster());
			gameSimulation->clusters.push_back(playerClusterEnd->GetTarget()->GetCluster());
			gameSimulation->action = playerClusterEnd->GetType();

			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerDecisions.push_back(gameSimulation);
		}
	});

	if (playerDataIn.valid)
	{
		Concurrency::concurrent_vector<AIAnalysis::Simulation*> playerSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
			PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = otherClusterCode;
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterStart->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.clusters.push_back(otherPlayerClusterEnd->GetTarget()->GetCluster());
			simulation->otherPlayerSimulation.action = otherPlayerClusterStart->GetType();

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerGuessings[otherClusterIdx].push_back(simulation);
		});

		if (otherPlayerDataIn.valid)
		{
			size_t otherClusterIdx = playerSimulations.size();

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation.code = ULLONG_MAX;
			simulation->playerSimulation.planId = player.plan.id;
			if (player.plan.path.empty())
				simulation->playerSimulation.clusters.push_back(player.plan.node->GetCluster());
			else
				simulation->playerSimulation.clusters.push_back(player.plan.path.back()->GetNode()->GetCluster());
			simulation->otherPlayerSimulation.code = ULLONG_MAX;
			simulation->otherPlayerSimulation.planId = otherPlayer.plan.id;
			if (otherPlayer.plan.path.empty())
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.node->GetCluster());
			else
				simulation->otherPlayerSimulation.clusters.push_back(otherPlayer.plan.path.back()->GetNode()->GetCluster());

			SetPlayerSimulation(simulation->playerSimulation, player);
			SetPlayerSimulation(simulation->otherPlayerSimulation, otherPlayer);

			playerSimulations.push_back(simulation);
			playerGuessings[otherClusterIdx].push_back(simulation);
		}

		if (playerSimulations.size())
		{
			AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
			if (playerDataIn.plan.path.empty())
				gameSimulation->clusters.push_back(playerDataIn.plan.node->GetCluster());
			else
				gameSimulation->clusters.push_back(playerDataIn.plan.path.back()->GetNode()->GetCluster());
			for (auto& playerSimulation : playerSimulations)
				gameSimulation->simulations.push_back(std::move(playerSimulation));
			playerDecisions.push_back(gameSimulation);
		}
	}

	//player guessing analysis data
	for (auto& playerGuessing : playerGuessings)
	{
		AIAnalysis::GameSimulation* gameSimulation = new AIAnalysis::GameSimulation();
		gameSimulation->clusters = playerGuessing.second.front()->otherPlayerSimulation.clusters;
		gameSimulation->action = playerGuessing.second.front()->otherPlayerSimulation.action;

		for (auto& playerGuessSimulation : playerGuessing.second)
		{
			AIAnalysis::Simulation* simulation = new AIAnalysis::Simulation();
			simulation->playerSimulation = playerGuessSimulation->otherPlayerSimulation;
			simulation->otherPlayerSimulation = playerGuessSimulation->playerSimulation;
			simulation->playerSimulation.heuristic = -simulation->playerSimulation.heuristic;
			simulation->otherPlayerSimulation.heuristic = -simulation->otherPlayerSimulation.heuristic;

			gameSimulation->simulations.push_back(simulation);
		}

		gameEvaluation.playerGuessings.push_back(gameSimulation);
	}

	gameEvaluation.playerGuessDecision = new AIAnalysis::GameSimulation();
	for (auto const& gameSimulation : gameEvaluation.playerGuessings)
	{
		float playerHeuristic = FLT_MAX;
		float otherPlayerHeuristic = FLT_MAX;

		AIAnalysis::Simulation* playerGuessSimulation = nullptr;
		for (auto const& simulation : gameSimulation->simulations)
		{
			//minimize the other player heuristic according to minimax decision level
			if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
			{
				playerHeuristic = simulation->playerSimulation.heuristic;
				otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;

				playerGuessSimulation = simulation;
			}
		}

		gameEvaluation.playerGuessDecision->simulations.push_back(playerGuessSimulation);
	}

	//player decision analysis data
	for (auto& playerDecision : playerDecisions)
		gameEvaluation.playerDecisions.push_back(std::move(playerDecision));

	gameEvaluation.playerDecision = new AIAnalysis::GameSimulation();
	for (auto const& gameSimulation : gameEvaluation.playerDecisions)
	{
		float playerHeuristic = FLT_MAX;
		float otherPlayerHeuristic = FLT_MAX;

		AIAnalysis::Simulation* playerSimulation = nullptr;
		for (auto const& simulation : gameSimulation->simulations)
		{
			//minimize the other player heuristic according to minimax decision level
			if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
			{
				playerHeuristic = simulation->playerSimulation.heuristic;
				otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;

				playerSimulation = simulation;
			}
		}

		gameEvaluation.playerDecision->simulations.push_back(playerSimulation);
	}

	//player decision output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformDecisionMaking(gameEvaluation, playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings,
		playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);

	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
				itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
				if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
					itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation((EvaluationType)gameEvaluation.type, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation((EvaluationType)gameEvaluation.type, gameItems,
				player, playerPathPlan, playerPathOffset, otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 40; //lets add marginal time

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;
	for (PathingArc* playerPathArc : playerPathPlanOffset)
	{
		if (playerDataOut.planWeight <= 0.f)
			break;

		playerDataOut.plan.path.erase(playerDataOut.plan.path.begin());
		playerDataOut.plan.node = playerPathArc->GetNode();
		playerDataOut.planWeight -= playerPathArc->GetWeight();
	}

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player decisions end");
	return true;
}


//RUNTIME SIMULATIONS

bool QuakeAIManager::SimulatePlayerGuessingDecision(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut, 
	const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> otherPlayerPaths;
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<unsigned int, unsigned int>> otherPlayerClusters;

	//PrintInfo("\nGuessing clusters: ");
	if (otherClusterNodeStart)
	{
		unsigned long long otherPlayerIdx = ULLONG_MAX;
		unsigned int otherPlayerCluster = otherPlayerPathPlan.empty() ?
			otherClusterNodeStart->GetCluster() : otherPlayerPathPlan.back()->GetNode()->GetCluster();
		unsigned int otherPlayerClusterType = 0;

		otherPlayerPaths[otherPlayerIdx] = otherPlayerPathPlan;
		otherPlayerClusters[otherPlayerIdx] = std::make_pair(otherPlayerCluster, otherPlayerClusterType);
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	std::mutex mutex;

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters;

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };
	Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
	//for (unsigned int actionType : actionTypes)
	{
		Concurrency::concurrent_unordered_map<unsigned long long,
			std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
		Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

		//player
		BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
			playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
			clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

		mutex.lock();
		clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

		actorPathPlanClusterHeuristics.insert(
			localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
		actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
		mutex.unlock();
	});

	float bestHeuristic = -FLT_MAX;
	float heuristicThreshold = 0.15f;
	for (auto& actorPathPlanClusterHeuristic : actorPathPlanClusterHeuristics)
		if (actorPathPlanClusterHeuristic.second > bestHeuristic)
			bestHeuristic = actorPathPlanClusterHeuristic.second;

	//if there are worthy items to be taken we will only build items paths, otherwise only normal paths.
	if (bestHeuristic < heuristicThreshold)
	{
		clusterPathings.clear();
		actorPathPlanClusterHeuristics.clear();
		actorPathPlanClusters.clear();

		BuildLongPath(mPathingGraph, clusterNodeStart, clusterPathings, clusterNodePathPlans);
	}
	else
	{
		BuildExpandedActorPath(mPathingGraph, clusterNodeStart, 
			heuristicThreshold, clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_unordered_map<unsigned long long, 
		Concurrency::concurrent_unordered_map<unsigned long long, float>> playerDecisions;
	Concurrency::concurrent_unordered_map<unsigned long long,
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>> playerWeaponDecisions;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for_each(begin(otherPlayerClusters), end(otherPlayerClusters), [&](auto const& otherPlayerCluster)
		//for (auto const& otherPlayerCluster : otherPlayerClusters)
		{
			//we need to stop the simulation if an aware decision making has started
			if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
				return;

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPaths[otherPlayerCluster.first], otherPlayerPathOffset);

			player.plan.id = -1;
			playerSimulations[otherPlayerCluster.first] = player.heuristic;
			playerWeaponSimulations[otherPlayerCluster.first] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});

		playerDecisions[clusterCode].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponDecisions[clusterCode].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	});

	if (playerDataIn.valid)
	{
		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for_each(begin(otherPlayerClusters), end(otherPlayerClusters), [&](auto const& otherPlayerCluster)
		//for (auto const& otherPathingClusterNode : otherPathingClusterNodes)
		{
			//we need to stop the simulation if an aware decision making has started
			if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
				return;

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPaths[otherPlayerCluster.first], otherPlayerPathOffset);

			playerSimulations[otherPlayerCluster.first] = player.heuristic;
			playerWeaponSimulations[otherPlayerCluster.first] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});

		playerDecisions[ULLONG_MAX].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponDecisions[ULLONG_MAX].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	//printf("\n player simulations end");
	//printf("\n player decisions start");

	playerDataOut = playerDataIn;
	playerDataOut.heuristic = -FLT_MAX;

	otherPlayerDataOut = otherPlayerDataIn;
	otherPlayerDataOut.heuristic = -FLT_MAX;

	//player decision output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformDecisionMaking(playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings, 
		playerDecisions, playerWeaponDecisions, playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);

	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPaths[otherPlayerClusterCode], otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 240; //lets add estimation of guessing simulation

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;
	for (PathingArc* playerPathArc : playerPathPlanOffset)
	{
		if (playerDataOut.planWeight <= 0.f)
			break;

		playerDataOut.plan.path.erase(playerDataOut.plan.path.begin());
		playerDataOut.plan.node = playerPathArc->GetNode();
		playerDataOut.planWeight -= playerPathArc->GetWeight();
	}

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	return true;
}

bool QuakeAIManager::SimulatePlayerGuessings(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart || clusterNodeStart == otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	std::map<ActorId, float> otherSearchItems;
	for (ActorId actor : searchActors)
		otherSearchItems[actor] = 0.f;
	CalculateWeightItems(otherPlayerDataIn, otherSearchItems);

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}
	else
	{
		if (!BuildLongPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);
	if (otherPlayerPathPlanOffset.size())
		for (auto& otherClusterNodePathPlan : otherClusterNodePathPlans)
			for (auto it = otherPlayerPathPlanOffset.rbegin(); it != otherPlayerPathPlanOffset.rend(); it++)
				otherClusterNodePathPlan.second.insert(otherClusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_unordered_map<unsigned long long, 
		Concurrency::concurrent_unordered_map<unsigned long long, float>> playerGuessings;
	Concurrency::concurrent_unordered_map<unsigned long long,
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>> playerWeaponGuessings;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			//we need to stop the simulation if an aware decision making has started
			if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
				return;

			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			player.plan.id = -1;
			otherPlayer.plan.id = -1;
			playerSimulations[otherClusterCode] = player.heuristic;
			playerWeaponSimulations[otherClusterCode] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});
		
		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.plan.id = -1;
			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}

		playerGuessings[clusterCode].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponGuessings[clusterCode].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	});

	if (playerDataIn.valid)
	{
		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			//we need to stop the simulation if an aware decision making has started
			if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
				return;

			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			playerSimulations[otherClusterCode] = player.heuristic;
			playerWeaponSimulations[otherClusterCode] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});

		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset, 
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}
		playerGuessings[ULLONG_MAX].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponGuessings[ULLONG_MAX].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	//printf("\n player guessing simulations end");
	//printf("\n player guessing decisions start");

	//player guessing output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformGuessingMaking(playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings, 
		playerGuessings, playerWeaponGuessings, playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);
	
	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			//other cluster code
			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
				itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
				if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
					itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation(evaluation, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation(evaluation, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 100; //lets add estimation of guessing decision simulation

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player guessing decisions end");
	return true;
}

bool QuakeAIManager::SimulatePlayerGuessing(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart || clusterNodeStart == otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	std::map<ActorId, float> otherSearchItems;
	for (ActorId actor : searchActors)
		otherSearchItems[actor] = 0.f;
	CalculateWeightItems(otherPlayerDataIn, otherSearchItems);

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}
	else
	{
		if (!BuildLongPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);
	if (otherPlayerPathPlanOffset.size())
		for (auto& otherClusterNodePathPlan : otherClusterNodePathPlans)
			for (auto it = otherPlayerPathPlanOffset.rbegin(); it != otherPlayerPathPlanOffset.rend(); it++)
				otherClusterNodePathPlan.second.insert(otherClusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_unordered_map<unsigned long long, 
		Concurrency::concurrent_unordered_map<unsigned long long, float>> playerGuessings;
	Concurrency::concurrent_unordered_map<unsigned long long,
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>> playerWeaponGuessings;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			//we need to stop the simulation if an aware decision making has started
			if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
				return;

			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			player.plan.id = -1;
			otherPlayer.plan.id = -1;
			playerSimulations[otherClusterCode] = player.heuristic;
			playerWeaponSimulations[otherClusterCode] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});
		
		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.plan.id = -1;
			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}

		playerGuessings[clusterCode].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponGuessings[clusterCode].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	});

	if (playerDataIn.valid)
	{
		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			//we need to stop the simulation if an aware decision making has started
			if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
				return;

			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			playerSimulations[otherClusterCode] = player.heuristic;
			playerWeaponSimulations[otherClusterCode] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});

		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset, 
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}
		playerGuessings[ULLONG_MAX].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponGuessings[ULLONG_MAX].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && mPlayerEvaluations.at(playerEvaluation) == ET_AWARENESS)
		return false;

	//printf("\n player guessing simulations end");
	//printf("\n player guessing decisions start");

	//player guessing output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformGuessingMaking(playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings, 
		playerGuessings, playerWeaponGuessings, playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);
	
	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			//other cluster code
			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
				itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
				if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
					itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation(evaluation, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation(evaluation, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 40; //lets add marginal time

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;
	for (PathingArc* playerPathArc : playerPathPlanOffset)
	{
		if (playerDataOut.planWeight <= 0.f)
			break;

		playerDataOut.plan.path.erase(playerDataOut.plan.path.begin());
		playerDataOut.plan.node = playerPathArc->GetNode();
		playerDataOut.planWeight -= playerPathArc->GetWeight();
	}

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player guessing decisions end");
	return true;
}

bool QuakeAIManager::SimulatePlayerDecision(
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation)
{
	PathingNode* clusterNodeStart = playerDataIn.plan.node;
	PathingNode* otherClusterNodeStart = otherPlayerDataIn.plan.node;
	if (!clusterNodeStart || !otherClusterNodeStart || clusterNodeStart == otherClusterNodeStart)
		return false;

	unsigned int time = Timer::GetRealTime();

	PathingArcVec playerPathPlan = playerDataIn.plan.path;
	PathingArcVec otherPlayerPathPlan = otherPlayerDataIn.plan.path;

	float playerPathOffset = playerDataOut.plan.weight;
	float otherPlayerPathOffset = otherPlayerDataOut.plan.weight;
	PathingArcVec playerPathPlanOffset = playerDataOut.plan.path;
	PathingArcVec otherPlayerPathPlanOffset = otherPlayerDataOut.plan.path;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<ActorId> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	std::map<ActorId, float> searchItems;
	for (ActorId actor : searchActors)
		searchItems[actor] = 0.f;
	CalculateWeightItems(playerDataIn, searchItems);

	std::map<ActorId, float> otherSearchItems;
	for (ActorId actor : searchActors)
		otherSearchItems[actor] = 0.f;
	CalculateWeightItems(otherPlayerDataIn, otherSearchItems);

	std::vector<unsigned int> actionTypes{ AT_MOVE, AT_JUMP };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart, 
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart, 
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}
	else
	{
		if (!BuildLongPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		std::array<Concurrency::task<void>, 2> tasks =
		{
			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//player
					BuildActorPath(mPathingGraph, actionType, gameItems, searchItems,
						playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
						clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

					mutex.lock();
					clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

					actorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics);
			}),

			Concurrency::create_task([&]
			{
				Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](unsigned int actionType)
				//for (unsigned int actionType : actionTypes)
				{
					Concurrency::concurrent_unordered_map<unsigned long long,
						std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
					Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
					Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

					//other player
					BuildActorPath(mPathingGraph, actionType, gameItems, otherSearchItems,
						otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
						localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
						localActorPathPlanClusters);

					mutex.lock();
					otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

					otherActorPathPlanClusterHeuristics.insert(
						localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
					otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
					mutex.unlock();
				});

				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);
			})
		};

		auto joinTask = when_all(begin(tasks), end(tasks));
		// Wait for the tasks to finish.
		joinTask.wait();
	}

	// 	adding pathing offset to clusters path
	if (playerPathPlanOffset.size())
		for (auto& clusterNodePathPlan : clusterNodePathPlans)
			for (auto it = playerPathPlanOffset.rbegin(); it != playerPathPlanOffset.rend(); it++)
				clusterNodePathPlan.second.insert(clusterNodePathPlan.second.begin(), *it);
	if (otherPlayerPathPlanOffset.size())
		for (auto& otherClusterNodePathPlan : otherClusterNodePathPlans)
			for (auto it = otherPlayerPathPlanOffset.rbegin(); it != otherPlayerPathPlanOffset.rend(); it++)
				otherClusterNodePathPlan.second.insert(otherClusterNodePathPlan.second.begin(), *it);

	Concurrency::concurrent_unordered_map<unsigned long long, 
		Concurrency::concurrent_unordered_map<unsigned long long, float>> playerDecisions;
	Concurrency::concurrent_unordered_map<unsigned long long,
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>> playerWeaponDecisions;
	Concurrency::parallel_for(size_t(0), clusterPathings.size(), [&](size_t clusterIdx)
	{
		auto itCluster = clusterPathings.begin();
		std::advance(itCluster, clusterIdx);

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for(size_t(0), otherClusterPathings.size(), [&](size_t otherClusterIdx)
		{
			auto itOtherCluster = otherClusterPathings.begin();
			std::advance(itOtherCluster, otherClusterIdx);

			//other cluster code
			unsigned long long otherClusterCode = (*itOtherCluster).first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			player.plan.id = -1;
			otherPlayer.plan.id = -1;
			playerSimulations[otherClusterCode] = player.heuristic;
			playerWeaponSimulations[otherClusterCode] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});
		
		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, itClusterNodePathPlan->second, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.plan.id = -1;
			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}
		playerDecisions[clusterCode].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponDecisions[clusterCode].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	});

	if (playerDataIn.valid)
	{
		Concurrency::concurrent_unordered_map<unsigned long long, float> playerSimulations;
		Concurrency::concurrent_unordered_map<unsigned long long, unsigned short> playerWeaponSimulations;
		Concurrency::parallel_for_each(begin(otherClusterPathings), end(otherClusterPathings), [&](auto const& otherClusterPathing)
		//for (auto const& otherClusterPathing : otherClusterPathings)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherClusterPathing.first;
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			playerSimulations[otherClusterCode] = player.heuristic;
			playerWeaponSimulations[otherClusterCode] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		});

		if (otherPlayerDataIn.valid)
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}
		playerDecisions[ULLONG_MAX].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponDecisions[ULLONG_MAX].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	}

	//printf("\n player simulations end");
	//printf("\n player decisions start");

	//player decision output
	WeaponType playerWeapon = WP_NONE;
	WeaponType otherPlayerWeapon = WP_NONE;
	unsigned long long playerClusterCode = 0;
	unsigned long long otherPlayerClusterCode = 0;
	PerformDecisionMaking(playerDataIn, otherPlayerDataIn, clusterPathings, otherClusterPathings, 
		playerDecisions, playerWeaponDecisions, playerWeapon, otherPlayerWeapon, playerClusterCode, otherPlayerClusterCode);

	//Simulate best outcome for each player
	{
		//cluster code
		if (playerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
			itClusterNodePathPlan = actorPathPlanClusters.find(playerClusterCode);
			if (itClusterNodePathPlan == actorPathPlanClusters.end())
				itClusterNodePathPlan = clusterNodePathPlans.find(playerClusterCode);

			//other cluster code
			if (otherPlayerClusterCode != ULLONG_MAX)
			{
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
				itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
				if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
					itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation(evaluation, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.plan.id = -1;
				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
			else
			{
				PlayerData player(playerDataIn);
				PlayerData otherPlayer(otherPlayerDataIn);
				Simulation(evaluation, gameItems,
					player, itClusterNodePathPlan->second, playerPathOffset,
					otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

				player.plan.id = -1;
				player.weaponTime = 0.f;
				player.weapon = playerWeapon;
				player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
				playerDataOut = std::move(player);

				otherPlayer.weaponTime = 0.f;
				otherPlayer.weapon = otherPlayerWeapon;
				otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
				otherPlayerDataOut = std::move(otherPlayer);
			}
		}
		else if (otherPlayerClusterCode != ULLONG_MAX)
		{
			Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
			itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherPlayerClusterCode);
			if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
				itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherPlayerClusterCode);

			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, itOtherClusterNodePathPlan->second, otherPlayerPathOffset);

			otherPlayer.plan.id = -1;
			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);
		}
		else
		{
			PlayerData player(playerDataIn);
			PlayerData otherPlayer(otherPlayerDataIn);
			Simulation(evaluation, gameItems,
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			player.weaponTime = 0.f;
			player.weapon = playerWeapon;
			player.target = player.weapon != WP_NONE ? otherPlayer.player : INVALID_ACTOR_ID;
			playerDataOut = std::move(player);

			otherPlayer.weaponTime = 0.f;
			otherPlayer.weapon = otherPlayerWeapon;
			otherPlayer.target = otherPlayer.weapon != WP_NONE ? player.player : INVALID_ACTOR_ID;
			otherPlayerDataOut = std::move(otherPlayer);
		}
	}

	unsigned int diffTime = Timer::GetRealTime() - time;
	diffTime += 40; //lets add marginal time

	playerDataOut.valid = true;
	otherPlayerDataOut.valid = true;

	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;
	for (PathingArc* playerPathArc : playerPathPlanOffset)
	{
		if (playerDataOut.planWeight <= 0.f)
			break;

		playerDataOut.plan.path.erase(playerDataOut.plan.path.begin());
		playerDataOut.plan.node = playerPathArc->GetNode();
		playerDataOut.planWeight -= playerPathArc->GetWeight();
	}

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player decisions end");
	return true;
}

bool QuakeAIManager::IsCloseAIGuessing()
{
	PlayerView aiView;
	GetPlayerView(mPlayers[GV_AI], aiView);
	if (aiView.guessViews.find(mPlayers[GV_HUMAN]) == aiView.guessViews.end())
		return false;

	PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];
	if (aiView.data.plan.node == NULL || playerGuessView.guessPlayers[mPlayers[GV_AI]].plan.node == NULL)
		return false;

	PathingNode* aiNode = aiView.data.plan.node;
	float aiWeight = CalculatePathWeight(aiView.data);
	if (aiWeight)
	{
		PathingArc* aiArc = *aiView.data.plan.path.begin();
		aiNode = aiArc->GetNode();
		aiWeight = 0.f;
	}
	PathingNode* aiGuessNode = playerGuessView.guessPlayers[mPlayers[GV_AI]].plan.node;
	float aiGuessWeight = CalculatePathWeight(playerGuessView.guessPlayers[mPlayers[GV_AI]]);
	if (aiGuessWeight)
	{
		PathingArc* aiGuessArc = *playerGuessView.guessPlayers[mPlayers[GV_AI]].plan.path.begin();
		aiGuessNode = aiGuessArc->GetNode();
		aiGuessWeight = 0.f;
	}
	if (aiNode->GetCluster() == aiGuessNode->GetCluster())
		return true;

	PathingCluster* pathingCluster = aiNode->FindCluster(AT_JUMP, aiGuessNode->GetCluster());
	while (aiNode != pathingCluster->GetTarget())
	{
		PathingCluster* aiCluster = aiNode->FindCluster(AT_JUMP, pathingCluster->GetTarget());
		PathingArc* aiArc = aiNode->FindArc(aiCluster->GetNode());

		aiWeight += aiArc->GetWeight();
		aiNode = aiArc->GetNode();
	}
	// threshold to accept close guessing players
	if (aiWeight < 0.8f)
		return true;

	pathingCluster = aiGuessNode->FindCluster(AT_JUMP, aiNode->GetCluster());
	while (aiGuessNode != pathingCluster->GetTarget())
	{
		PathingCluster* aiGuessCluster = aiGuessNode->FindCluster(AT_JUMP, pathingCluster->GetTarget());
		PathingArc* aiGuessArc = aiGuessNode->FindArc(aiGuessCluster->GetNode());

		aiGuessWeight += aiGuessArc->GetWeight();
		aiGuessNode = aiGuessArc->GetNode();
	}
	// threshold to accept close guessing players
	if (aiGuessWeight < 0.8f)
		return true;

	return false;
}

bool QuakeAIManager::IsCloseHumanGuessing()
{
	PlayerView playerView;
	GetPlayerView(mPlayers[GV_HUMAN], playerView);
	if (playerView.guessViews.find(mPlayers[GV_AI]) == playerView.guessViews.end())
		return false;

	PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];
	if (playerView.data.plan.node == NULL || aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].plan.node == NULL)
		return false;

	PathingNode* playerNode = playerView.data.plan.node;
	float playerWeight = CalculatePathWeight(playerView.data);
	if (playerWeight)
	{
		PathingArc* playerArc = *playerView.data.plan.path.begin();
		playerNode = playerArc->GetNode();
		playerWeight = 0.f;
	}
	PathingNode* playerGuessNode = aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].plan.node;
	float playerGuessWeight = CalculatePathWeight(aiGuessView.guessPlayers[mPlayers[GV_HUMAN]]);
	if (playerGuessWeight)
	{
		PathingArc* playerGuessArc = *aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].plan.path.begin();
		playerGuessNode = playerGuessArc->GetNode();
		playerGuessWeight = 0.f;
	}
	if (playerNode->GetCluster() == playerGuessNode->GetCluster())
		return true;

	PathingCluster* pathingCluster = playerNode->FindCluster(AT_JUMP, playerGuessNode->GetCluster());
	while (playerNode != pathingCluster->GetTarget())
	{
		PathingCluster* playerCluster = playerNode->FindCluster(AT_JUMP, pathingCluster->GetTarget());
		PathingArc* playerArc = playerNode->FindArc(playerCluster->GetNode());

		playerWeight += playerArc->GetWeight();
		playerNode = playerArc->GetNode();
	}
	// threshold to accept close guessing players
	if (playerWeight < 0.8f)
		return true;

	pathingCluster = playerGuessNode->FindCluster(AT_JUMP, playerNode->GetCluster());
	while (playerGuessNode != pathingCluster->GetTarget())
	{
		PathingCluster* playerGuessCluster = playerGuessNode->FindCluster(AT_JUMP, pathingCluster->GetTarget());
		PathingArc* playerGuessArc = playerGuessNode->FindArc(playerGuessCluster->GetNode());

		playerGuessWeight += playerGuessArc->GetWeight();
		playerGuessNode = playerGuessArc->GetNode();
	}
	// threshold to accept close guessing players
	if (playerGuessWeight < 0.8f)
		return true;

	return false;
}

bool QuakeAIManager::MakeAIGuessing(PlayerView& aiView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_AI], aiView);
	if (aiView.guessViews.find(mPlayers[GV_HUMAN]) == aiView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];
	if (playerGuessView.data.plan.node == NULL || playerGuessView.guessPlayers[mPlayers[GV_AI]].plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PrintInfo("\nAI Guessing Human player guess input before: ");
	PrintPlayerData(playerGuessView.data);

	PrintInfo("\nAI Guessing AI player input before: ");
	PrintPlayerData(aiView.data);

	float aiPathWeightOffset = CalculatePathWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView aiSimulation = aiView;
	aiSimulation.data.planWeight = 0.3f;
	aiSimulation.data.planWeight += aiPathWeightOffset;
	aiSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);

	PrintInfo("\nAI Guessing Human player guess input after: ");
	PrintPlayerData(playerGuessSimulation.data);

	PrintInfo("\nAI Guessing AI player input after: ");
	PrintPlayerData(aiSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_CLOSEGUESSING;
	gameDecision.evaluation.target = GV_AI;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput, playerGuessView.data, playerGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, aiView.data, aiSimulation.data);

	SetPlayerInput(gameDecision.evaluation.playerInput, aiView.data, aiSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, playerGuessView.data, playerGuessSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = aiView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& humanGuessItem : playerGuessView.items)
		if (gameItems.at(humanGuessItem.first) == 0.f)
			gameItems[humanGuessItem.first] = humanGuessItem.second;

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiView.data.ResetItems();
	playerGuessView.data.ResetItems();

	aiView.data.valid = aiSimulation.data.plan.path.empty() ? false : true;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerGuessing(
		aiView.data, aiSimulation.data, playerGuessView.data, playerGuessSimulation.data,
		gameDecision.evaluation.playerGuessItems, mPlayers[GV_AI], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nAI Guessing AI player output: ");
		PrintPlayerData(aiSimulation.data);

		PrintInfo("\nAI Guessing human player guess output: ");
		PrintPlayerData(playerGuessSimulation.data);

		aiView.isUpdated = true;
		aiView.simulation = aiSimulation.data;

		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = aiSimulation.data;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, aiView.simulation);

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, playerGuessView.simulation);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, playerGuessView.guessSimulations[mPlayers[GV_AI]]);

		Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
		gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
		gameDecision.time =
			std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
		mGameDecisions.push_back(std::move(gameDecision));

		mMutex.unlock();

		return true;
	}
	return false;
}

bool QuakeAIManager::MakeAIFastDecision(PlayerView& aiView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_AI], aiView);
	if (aiView.guessViews.find(mPlayers[GV_HUMAN]) == aiView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	if (aiView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];
	if (playerGuessView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PrintInfo("\nAI Decision Human player guess input before: ");
	PrintPlayerData(playerGuessView.data);

	PrintInfo("\nAI Decision AI player input before: ");
	PrintPlayerData(aiView.data);

	float aiPathWeightOffset = CalculatePathWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (max 0.1 seg)
	PlayerView aiSimulation = aiView;
	aiSimulation.data.planWeight = 0.1f;
	aiSimulation.data.planWeight += aiPathWeightOffset;
	aiSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);

	PrintInfo("\nAI Decision Human player guess input after: ");
	PrintPlayerData(playerGuessSimulation.data);

	PrintInfo("\nAI Decision AI player input after: ");
	PrintPlayerData(aiSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_RESPONSIVE;
	gameDecision.evaluation.target = GV_AI;
	SetPlayerInput(gameDecision.evaluation.playerInput, aiView.data, aiSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, playerGuessView.data, playerGuessSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = aiView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& humanGuessItem : playerGuessView.guessItems[mPlayers[GV_HUMAN]])
		if (gameItems.at(humanGuessItem.first) == 0.f)
			gameItems[humanGuessItem.first] = humanGuessItem.second;

	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
	playerGuessView.data.ResetItems();
	aiView.data.ResetItems();

	aiView.data.valid = aiSimulation.data.plan.path.empty() ? false : true;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerDecision(aiView.data, aiSimulation.data, playerGuessView.data,
		playerGuessSimulation.data, gameDecision.evaluation.playerDecisionItems, gameDecision.evaluation);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nAI Decision AI player output: ");
		PrintPlayerData(aiSimulation.data);

		aiView.isUpdated = true;
		aiView.simulation = aiSimulation.data;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, aiView.simulation);

		Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
		gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
		gameDecision.time =
			std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
		mGameDecisions.push_back(std::move(gameDecision));

		mMutex.unlock();

		return true;
	}
	return false;
}

bool QuakeAIManager::MakeAIGuessingDecision(PlayerView& aiView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_AI], aiView);
	if (aiView.guessViews.find(mPlayers[GV_HUMAN]) == aiView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	if (aiView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];
	if (playerGuessView.data.plan.node == NULL || playerGuessView.guessPlayers[mPlayers[GV_AI]].plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PrintInfo("\nAI Guessing Human player guess input before: ");
	PrintPlayerData(playerGuessView.data);

	PrintInfo("\nAI Guessing AI player guess input before: ");
	PrintPlayerData(playerGuessView.guessPlayers[mPlayers[GV_AI]]);

	PrintInfo("\nAI Decision AI player input before: ");
	PrintPlayerData(aiView.data);

	float aiPathWeightOffset = CalculatePathWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;
	float aiGuessPathWeightOffset = 
		playerGuessView.guessPlayers[mPlayers[GV_AI]].planWeight > 0.f ? playerGuessView.guessPlayers[mPlayers[GV_AI]].planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView aiDecisionSimulation = aiView;
	aiDecisionSimulation.data.planWeight = 0.4f;
	aiDecisionSimulation.data.planWeight += aiPathWeightOffset;
	aiDecisionSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiDecisionSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight = 0.f;
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight += aiGuessPathWeightOffset;
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);
	UpdatePlayerGuessState(playerGuessSimulation, mPlayers[GV_AI]);
	PlayerGuessView playerGuessDecisionSimulation = playerGuessSimulation;

	PrintInfo("\nAI Guessing Human player guess input after: ");
	PrintPlayerData(playerGuessSimulation.data);

	PrintInfo("\nAI Guessing AI player guess input after: ");
	PrintPlayerData(playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

	PrintInfo("\nAI Decision AI player input after: ");
	PrintPlayerData(aiDecisionSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_GUESSING;
	gameDecision.evaluation.target = GV_AI;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput, playerGuessView.data, playerGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput,
		playerGuessView.guessPlayers[mPlayers[GV_AI]], playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

	SetPlayerInput(gameDecision.evaluation.playerInput, aiView.data, aiDecisionSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, playerGuessView.data, playerGuessDecisionSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = playerGuessView.items;

	// update the items which are guessed to be taken
	for (auto const& humanGuessItem : playerGuessView.guessItems[mPlayers[GV_HUMAN]])
		if (gameItems.at(humanGuessItem.first) == 0.f)
			gameItems[humanGuessItem.first] = humanGuessItem.second;
	for (auto const& aiGuessItem : playerGuessView.guessItems[mPlayers[GV_AI]])
		if (gameItems.at(aiGuessItem.first) == 0.f)
			gameItems[aiGuessItem.first] = aiGuessItem.second;

	gameDecision.evaluation.playerGuessItems = gameItems;

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	gameItems = aiView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& humanGuessItem : playerGuessView.items)
		if (gameItems.at(humanGuessItem.first) == 0.f)
			gameItems[humanGuessItem.first] = humanGuessItem.second;

	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
	playerGuessView.data.ResetItems();

	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;
	playerGuessView.guessPlayers[mPlayers[GV_AI]].valid = 
		playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerGuessings(
		playerGuessView.data, playerGuessSimulation.data, playerGuessView.guessPlayers[mPlayers[GV_AI]],
		playerGuessSimulation.guessPlayers[mPlayers[GV_AI]], gameDecision.evaluation.playerGuessItems, 
		mPlayers[GV_AI], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = playerGuessSimulation.guessPlayers[mPlayers[GV_AI]];

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, playerGuessView.simulation);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, playerGuessView.guessSimulations[mPlayers[GV_AI]]);

		playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
		playerGuessView.data.ResetItems();
		aiView.data.ResetItems();

		aiView.data.valid = aiDecisionSimulation.data.plan.path.empty() ? false : true;
		playerGuessView.data.valid = playerGuessDecisionSimulation.data.plan.path.empty() ? false : true;

		//simulation
		success = SimulatePlayerGuessingDecision(
			aiView.data, aiDecisionSimulation.data, playerGuessView.data, playerGuessDecisionSimulation.data,
			gameDecision.evaluation.playerDecisionItems, mPlayers[GV_AI], (EvaluationType)gameDecision.evaluation.type);
		if (success)
		{
			mMutex.lock();

			PrintInfo("\nAI Guessing human player guess output: ");
			PrintPlayerData(playerGuessDecisionSimulation.data);

			PrintInfo("\nAI Guessing AI player guess output: ");
			PrintPlayerData(playerGuessDecisionSimulation.guessPlayers[mPlayers[GV_AI]]);

			PrintInfo("\nAI Decision AI player output: ");
			PrintPlayerData(aiDecisionSimulation.data);

			aiView.isUpdated = true;
			aiView.simulation = aiDecisionSimulation.data;

			SetPlayerOutput(gameDecision.evaluation.playerOutput, aiView.simulation);

			Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
			gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
			gameDecision.time =
				std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
			mGameDecisions.push_back(std::move(gameDecision));

			mMutex.unlock();

			return true;
		}
	}

	return false;
}

bool QuakeAIManager::MakeAIAwareDecision(PlayerView& aiView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_AI], aiView);
	if (aiView.guessViews.find(mPlayers[GV_HUMAN]) == aiView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	if (aiView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];
	if (playerGuessView.data.plan.node == NULL || playerGuessView.guessPlayers[mPlayers[GV_AI]].plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	//in awareness run the ai player and guess state are the same
	playerGuessView.guessPlayers[mPlayers[GV_AI]] = aiView.data;

	PrintInfo("\nAI Guessing Human player guess input before: ");
	PrintPlayerData(playerGuessView.data);

	PrintInfo("\nAI Guessing AI player guess input before: ");
	PrintPlayerData(playerGuessView.guessPlayers[mPlayers[GV_AI]]);

	PrintInfo("\nAI Decision AI player input before: ");
	PrintPlayerData(aiView.data);

	float aiPathWeightOffset = CalculatePathWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView aiDecisionSimulation = aiView;
	aiDecisionSimulation.data.planWeight = 0.3f;
	aiDecisionSimulation.data.planWeight += aiPathWeightOffset;
	aiDecisionSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiDecisionSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);

	//in awareness run the ai player and guess state are the same
	playerGuessView.guessPlayers[mPlayers[GV_AI]] = aiView.data;

	PrintInfo("\nAI Guessing Human player guess input after: ");
	PrintPlayerData(playerGuessSimulation.data);

	PrintInfo("\nAI Guessing AI player guess input after: ");
	PrintPlayerData(playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

	PrintInfo("\nAI Decision AI player input after: ");
	PrintPlayerData(aiDecisionSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_AWARENESS;
	gameDecision.evaluation.target = GV_AI;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput, playerGuessView.data, playerGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput,
		playerGuessView.guessPlayers[mPlayers[GV_AI]], playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

	SetPlayerInput(gameDecision.evaluation.playerInput, aiView.data, aiDecisionSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, playerGuessView.data, playerGuessSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = aiView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& humanGuessItem : playerGuessView.guessItems[mPlayers[GV_HUMAN]])
		if (gameItems.at(humanGuessItem.first) == 0.f)
			gameItems[humanGuessItem.first] = humanGuessItem.second;

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
	playerGuessView.data.ResetItems();
	aiView.data.ResetItems();

	aiView.data.valid = aiDecisionSimulation.data.plan.path.empty() ? false : true;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerDecision(aiView.data, aiDecisionSimulation.data, playerGuessView.data,
		playerGuessSimulation.data, gameDecision.evaluation.playerDecisionItems, mPlayers[GV_AI], 
		(EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nAI Guessing human player guess output: ");
		PrintPlayerData(playerGuessSimulation.data);

		PrintInfo("\nAI Guessing AI player guess output: ");
		PrintPlayerData(aiDecisionSimulation.data);

		PrintInfo("\nAI Decision AI player output: ");
		PrintPlayerData(aiDecisionSimulation.data);

		aiView.isUpdated = true;
		aiView.simulation = aiDecisionSimulation.data;

		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = aiDecisionSimulation.data;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, aiView.simulation);

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, playerGuessView.simulation);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, playerGuessView.guessSimulations[mPlayers[GV_AI]]);

		Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
		gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
		gameDecision.time =
			std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
		mGameDecisions.push_back(std::move(gameDecision));

		mMutex.unlock();

		return true;
	}

	return false;
}

bool QuakeAIManager::MakeHumanGuessing(PlayerView& playerView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_HUMAN], playerView);
	if (playerView.guessViews.find(mPlayers[GV_AI]) == playerView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];
	if (aiGuessView.data.plan.node == NULL || aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PrintInfo("\nHuman Guessing AI player guess input before: ");
	PrintPlayerData(aiGuessView.data);

	PrintInfo("\nHuman Guessing Human player input before: ");
	PrintPlayerData(playerView.data);

	float playerPathWeightOffset = CalculatePathWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView playerSimulation = playerView;
	playerSimulation.data.planWeight = 0.3f;
	playerSimulation.data.planWeight += playerPathWeightOffset;
	playerSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);

	PrintInfo("\nHuman Guessing AI guess input after: ");
	PrintPlayerData(aiGuessSimulation.data);

	PrintInfo("\nHuman Guessing Human player input after: ");
	PrintPlayerData(playerSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_CLOSEGUESSING;
	gameDecision.evaluation.target = GV_HUMAN;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput, aiGuessView.data, aiGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, playerView.data, playerSimulation.data);

	SetPlayerInput(gameDecision.evaluation.playerInput, playerView.data, playerSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, aiGuessView.data, aiGuessSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = playerView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& aiGuessItem : aiGuessView.items)
		if (gameItems.at(aiGuessItem.first) == 0.f)
			gameItems[aiGuessItem.first] = aiGuessItem.second;

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerView.data.ResetItems();
	aiGuessView.data.ResetItems();

	playerView.data.valid = playerSimulation.data.plan.path.empty() ? false : true;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerGuessing(playerView.data, playerSimulation.data, aiGuessView.data, aiGuessSimulation.data,
		gameDecision.evaluation.playerGuessItems, mPlayers[GV_HUMAN], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nHuman Guessing Human player output: ");
		PrintPlayerData(playerSimulation.data);

		PrintInfo("\nHuman Guessing AI player guess output: ");
		PrintPlayerData(aiGuessSimulation.data);

		playerView.isUpdated = true;
		playerView.simulation = playerSimulation.data;

		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = playerSimulation.data;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, playerView.simulation);

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, aiGuessView.simulation);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, aiGuessView.guessSimulations[mPlayers[GV_AI]]);

		Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
		gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
		gameDecision.time =
			std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
		mGameDecisions.push_back(std::move(gameDecision));

		mMutex.unlock();

		return true;
	}
	return false;
}

bool QuakeAIManager::MakeHumanFastDecision(PlayerView& playerView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_HUMAN], playerView);
	if (playerView.guessViews.find(mPlayers[GV_AI]) == playerView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	if (playerView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];
	if (aiGuessView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PrintInfo("\nHuman Decision AI player guess input before: ");
	PrintPlayerData(aiGuessView.data);

	PrintInfo("\nHuman Decision Human player input before: ");
	PrintPlayerData(playerView.data);

	float playerPathWeightOffset = CalculatePathWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (max 0.1 seg)
	PlayerView playerSimulation = playerView;
	playerSimulation.data.planWeight = 0.1f;
	playerSimulation.data.planWeight += playerPathWeightOffset;
	playerSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);

	PrintInfo("\nHuamn Decision AI player guess input after: ");
	PrintPlayerData(aiGuessSimulation.data);

	PrintInfo("\nHuman Decision Human player input after: ");
	PrintPlayerData(playerSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_RESPONSIVE;
	gameDecision.evaluation.target = GV_HUMAN;
	SetPlayerInput(gameDecision.evaluation.playerInput, playerView.data, playerSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, aiGuessView.data, aiGuessSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = playerView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& aiGuessItem : aiGuessView.guessItems[mPlayers[GV_AI]])
		if (gameItems.at(aiGuessItem.first) == 0.f)
			gameItems[aiGuessItem.first] = aiGuessItem.second;

	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
	aiGuessView.data.ResetItems();
	playerView.data.ResetItems();

	playerView.data.valid = playerSimulation.data.plan.path.empty() ? false : true;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerDecision(playerView.data, playerSimulation.data,
		aiGuessView.data, aiGuessSimulation.data, gameItems, gameDecision.evaluation);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nHuman Decision Human player output: ");
		PrintPlayerData(playerSimulation.data);

		playerView.isUpdated = true;
		playerView.simulation = playerSimulation.data;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, playerView.simulation);

		Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
		gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
		gameDecision.time =
			std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
		mGameDecisions.push_back(std::move(gameDecision));

		mMutex.unlock();

		return true;
	}
	return false;
}

bool QuakeAIManager::MakeHumanGuessingDecision(PlayerView& playerView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_HUMAN], playerView);
	if (playerView.guessViews.find(mPlayers[GV_AI]) == playerView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	if (playerView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];
	if (aiGuessView.data.plan.node == NULL || aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PrintInfo("\nHuman Guessing AI player guess input before: ");
	PrintPlayerData(aiGuessView.data);

	PrintInfo("\nHuman Guessing Human player guess input before: ");
	PrintPlayerData(aiGuessView.guessPlayers[mPlayers[GV_HUMAN]]);

	PrintInfo("\nHuman Decision Human player input before: ");
	PrintPlayerData(playerView.data);

	float playerPathWeightOffset = CalculatePathWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;
	float playerGuessPathWeightOffset = 
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].planWeight > 0.f ? aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView playerDecisionSimulation = playerView;
	playerDecisionSimulation.data.planWeight = 0.4f;
	playerDecisionSimulation.data.planWeight += playerPathWeightOffset;
	playerDecisionSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerDecisionSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight = 0.f;
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight += playerGuessPathWeightOffset;
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);
	UpdatePlayerGuessState(aiGuessSimulation, mPlayers[GV_HUMAN]);
	PlayerGuessView aiGuessDecisionSimulation = aiGuessSimulation;

	PrintInfo("\nHuman Guessing AI player guess input after: ");
	PrintPlayerData(aiGuessSimulation.data);

	PrintInfo("\nHuman Guessing Human player guess input after: ");
	PrintPlayerData(aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

	PrintInfo("\nHuman Decision Human player input after: ");
	PrintPlayerData(playerDecisionSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_GUESSING;
	gameDecision.evaluation.target = GV_HUMAN;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput, aiGuessView.data, aiGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput,
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]], aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

	SetPlayerInput(gameDecision.evaluation.playerInput, playerView.data, playerDecisionSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, aiGuessView.data, aiGuessDecisionSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = aiGuessView.items;

	// update the items which are guessed to be taken
	for (auto const& aiGuessItem : aiGuessView.guessItems[mPlayers[GV_AI]])
		if (gameItems.at(aiGuessItem.first) == 0.f)
			gameItems[aiGuessItem.first] = aiGuessItem.second;
	for (auto const& humanGuessItem : aiGuessView.guessItems[mPlayers[GV_HUMAN]])
		if (gameItems.at(humanGuessItem.first) == 0.f)
			gameItems[humanGuessItem.first] = humanGuessItem.second;

	gameDecision.evaluation.playerGuessItems = gameItems;

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	gameItems = playerView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& aiGuessItem : aiGuessView.items)
		if (gameItems.at(aiGuessItem.first) == 0.f)
			gameItems[aiGuessItem.first] = aiGuessItem.second;

	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
	aiGuessView.data.ResetItems();

	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].valid = 
		aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerGuessings(
		aiGuessView.data, aiGuessSimulation.data, aiGuessView.guessPlayers[mPlayers[GV_HUMAN]],
		aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]], gameDecision.evaluation.playerGuessItems, 
		mPlayers[GV_HUMAN], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]];

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, aiGuessView.simulation);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, aiGuessView.guessSimulations[mPlayers[GV_HUMAN]]);

		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
		aiGuessView.data.ResetItems();
		playerView.data.ResetItems();

		playerView.data.valid = playerDecisionSimulation.data.plan.path.empty() ? false : true;
		aiGuessView.data.valid = aiGuessDecisionSimulation.data.plan.path.empty() ? false : true;

		//simulation
		success = SimulatePlayerGuessingDecision(
			playerView.data, playerDecisionSimulation.data, aiGuessView.data, aiGuessDecisionSimulation.data,
			gameDecision.evaluation.playerDecisionItems, mPlayers[GV_HUMAN], (EvaluationType)gameDecision.evaluation.type);
		if (success)
		{
			mMutex.lock();

			PrintInfo("\nHuman Guessing AI player guess output: ");
			PrintPlayerData(aiGuessDecisionSimulation.data);

			PrintInfo("\nHuman Guessing Human player guess output: ");
			PrintPlayerData(aiGuessDecisionSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

			PrintInfo("\nHuman Decision Human player output: ");
			PrintPlayerData(playerDecisionSimulation.data);

			playerView.isUpdated = true;
			playerView.simulation = playerDecisionSimulation.data;

			SetPlayerOutput(gameDecision.evaluation.playerOutput, playerView.simulation);

			Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
			gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
			gameDecision.time =
				std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
			mGameDecisions.push_back(std::move(gameDecision));

			mMutex.unlock();

			return true;
		}
	}

	return false;
}

bool QuakeAIManager::MakeHumanAwareDecision(PlayerView& playerView)
{
	mMutex.lock();

	GetPlayerView(mPlayers[GV_HUMAN], playerView);
	if (playerView.guessViews.find(mPlayers[GV_AI]) == playerView.guessViews.end())
	{
		mMutex.unlock();
		return false;
	}

	if (playerView.data.plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];
	if (aiGuessView.data.plan.node == NULL || aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].plan.node == NULL)
	{
		mMutex.unlock();
		return false;
	}

	//in awareness run the human player and guess state must match
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]] = playerView.data;

	PrintInfo("\nHuman Guessing AI player guess input before: ");
	PrintPlayerData(aiGuessView.data);

	PrintInfo("\nHuman Guessing Human player guess input before: ");
	PrintPlayerData(aiGuessView.guessPlayers[mPlayers[GV_HUMAN]]);

	PrintInfo("\nHuman Decision Human player input before: ");
	PrintPlayerData(playerView.data);

	float playerPathWeightOffset = CalculatePathWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView playerDecisionSimulation = playerView;
	playerDecisionSimulation.data.planWeight = 0.3f;
	playerDecisionSimulation.data.planWeight += playerPathWeightOffset;
	playerDecisionSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerDecisionSimulation);

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);

	//in awareness run the human player and guess state must match
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]] = playerView.data;

	PrintInfo("\nHuman Guessing AI player guess input after: ");
	PrintPlayerData(aiGuessSimulation.data);

	PrintInfo("\nHuman Guessing Human player guess input after: ");
	PrintPlayerData(aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

	PrintInfo("\nHuman Decision Human player input after: ");
	PrintPlayerData(playerDecisionSimulation.data);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_AWARENESS;
	gameDecision.evaluation.target = GV_HUMAN;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput, aiGuessView.data, aiGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, 
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]], aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

	SetPlayerInput(gameDecision.evaluation.playerInput, playerView.data, playerDecisionSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, aiGuessView.data, aiGuessSimulation.data);

	// update the guess items from the world
	// for the time being is perfect information to make things easier
	std::map<ActorId, float> gameItems = playerView.gameItems;

	// update the items which are guessed to be taken
	for (auto const& aiGuessItem : aiGuessView.guessItems[mPlayers[GV_AI]])
		if (gameItems.at(aiGuessItem.first) == 0.f)
			gameItems[aiGuessItem.first] = aiGuessItem.second;

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
	aiGuessView.data.ResetItems();
	playerView.data.ResetItems();

	playerView.data.valid = playerDecisionSimulation.data.plan.path.empty() ? false : true;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	//simulation
	bool success = SimulatePlayerDecision(playerView.data, playerDecisionSimulation.data, aiGuessView.data,
		aiGuessSimulation.data, gameDecision.evaluation.playerDecisionItems, mPlayers[GV_HUMAN], 
		(EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nHuman Guessing AI player guess output: ");
		PrintPlayerData(aiGuessSimulation.data);

		PrintInfo("\nHuman Guessing Human player guess output: ");
		PrintPlayerData(playerDecisionSimulation.data);

		PrintInfo("\nHuman Decision Human player output: ");
		PrintPlayerData(playerDecisionSimulation.data);

		playerView.isUpdated = true;
		playerView.simulation = playerDecisionSimulation.data;

		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = playerDecisionSimulation.data;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, playerView.simulation);

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, aiGuessView.simulation);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, aiGuessView.guessSimulations[mPlayers[GV_HUMAN]]);

		Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();
		gameDecision.id = (unsigned short)mGameDecisions.size() + 1;
		gameDecision.time =
			std::to_string(realTime.Hour) + ":" + std::to_string(realTime.Minute) + ":" + std::to_string(realTime.Second);
		mGameDecisions.push_back(std::move(gameDecision));

		mMutex.unlock();

		return true;
	}

	return false;
}

void QuakeAIManager::RunAIFastDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_AI) == mPlayers.end())
				continue;

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_RESPONSIVE)
				continue;

			unsigned int time = Timer::GetRealTime();

			PlayerView aiView;
			if (MakeAIFastDecision(aiView))
			{
				//we enable ai view only if got a plan
				GameApplication* gameApp = (GameApplication*)Application::App;
				const GameViewList& gameViews = gameApp->GetGameViews();
				for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
				{
					std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
					if (pAiView)
						pAiView->SetEnabled(true);
				}

				unsigned int diffTime = Timer::GetRealTime() - time;
				std::stringstream ss;
				ss << "\n ai fast decision total elapsed time " << diffTime;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				//lets sleep the process if the execution took less than 100 ms 
				/*
				unsigned int minimumTime = 100;
				if (diffTime < minimumTime)
					Sleep(minimumTime - diffTime);
				*/
				UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);

				//lets wait to give some time for the AI Manager and AI Views update its status
				Sleep(40);
				//printf("\n Iteration AI Fast Decision %u", iteration);
				//iteration++;
			}

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunAIGuessing()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_AI) == mPlayers.end())
				continue;

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_GUESSING)
				continue;

			unsigned int time = Timer::GetRealTime();

			if (IsCloseAIGuessing())
			{
				PlayerView aiView;
				bool isAIGuessing = MakeAIGuessing(aiView);
				if (isAIGuessing)
				{
					//we enable ai view only if got a plan
					GameApplication* gameApp = (GameApplication*)Application::App;
					const GameViewList& gameViews = gameApp->GetGameViews();
					for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
					{
						std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
						if (pAiView)
							pAiView->SetEnabled(true);
					}

					unsigned int diffTime = Timer::GetRealTime() - time;
					std::stringstream ss;
					ss << "\n ai close guessing total elapsed time " << diffTime;
					PrintInfo(ss.str());
					printf(ss.str().c_str());

					//lets sleep the process if the execution took less than 300 ms 
					/*
					unsigned int minimumTime = 300;
					if (diffTime < minimumTime)
						Sleep(minimumTime - diffTime);
					*/

					PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];

					UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);  //update playerView
					UpdatePlayerSimulationView(mPlayers[GV_AI], playerGuessView); //update guessView

					//lets wait to give some time for the AI Manager and AI Views update its status
					Sleep(40);
					//printf("\n Iteration AI Guessing %u", iteration);
					//iteration++;
				}
			}
			else
			{
				PlayerView aiView;
				bool isAIDecision = MakeAIGuessingDecision(aiView);
				if (isAIDecision)
				{
					//we enable ai view only if got a plan
					GameApplication* gameApp = (GameApplication*)Application::App;
					const GameViewList& gameViews = gameApp->GetGameViews();
					for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
					{
						std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
						if (pAiView)
							pAiView->SetEnabled(true);
					}

					unsigned int diffTime = Timer::GetRealTime() - time;
					std::stringstream ss;
					ss << "\n ai guessing decision total elapsed time " << diffTime;
					PrintInfo(ss.str());
					printf(ss.str().c_str());

					//lets sleep the process if the execution took less than 300 ms 
					/*
					unsigned int minimumTime = 300;
					if (diffTime < minimumTime)
						Sleep(minimumTime - diffTime);
					*/

					PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];

					UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);  //update playerView
					UpdatePlayerSimulationView(mPlayers[GV_AI], playerGuessView); //update guessView

					//lets wait to give some time for the AI Manager and AI Views update its status
					Sleep(40);
					//printf("\n Iteration AI Guessing Decision %u", iteration);
					//iteration++;
				}
			}

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunAIAwareDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_AI) == mPlayers.end())
				continue;

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_AWARENESS)
				continue;

			unsigned int time = Timer::GetRealTime();

			PlayerView aiView;
			bool isAIDecision = MakeAIAwareDecision(aiView);
			if (isAIDecision)
			{
				//we enable ai view only if got a plan
				GameApplication* gameApp = (GameApplication*)Application::App;
				const GameViewList& gameViews = gameApp->GetGameViews();
				for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
				{
					std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
					if (pAiView)
						pAiView->SetEnabled(true);
				}

				unsigned int diffTime = Timer::GetRealTime() - time;
				std::stringstream ss;
				ss << "\n ai aware decision total elapsed time " << diffTime;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				//lets sleep the process if the execution took less than 300 ms 
				/*
				unsigned int minimumTime = 300;
				if (diffTime < minimumTime)
					Sleep(minimumTime - diffTime);
				*/

				PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];

				UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);  //update playerView
				UpdatePlayerSimulationView(mPlayers[GV_AI], playerGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Sleep(40);
				//printf("\n Iteration AI Aware Decision %u", iteration);
				//iteration++;
			}

			//after complete execution we run guessing decision making
			mPlayerEvaluations[mPlayers.at(GV_AI)] = ET_GUESSING;

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunHumanFastDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_HUMAN) == mPlayers.end())
				continue;

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_RESPONSIVE)
				continue;

			unsigned int time = Timer::GetRealTime();

			PlayerView playerView;
			if (MakeHumanFastDecision(playerView))
			{
				//we enable ai view only if got a plan
				GameApplication* gameApp = (GameApplication*)Application::App;
				const GameViewList& gameViews = gameApp->GetGameViews();
				for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
				{
					std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
					if (pAiView)
						pAiView->SetEnabled(true);
				}

				unsigned int diffTime = Timer::GetRealTime() - time;
				std::stringstream ss;
				ss << "\n human fast decision total elapsed time " << diffTime;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				//lets sleep the process if the execution took less than 100 ms 
				/*
				unsigned int minimumTime = 100;
				if (diffTime < minimumTime)
					Sleep(minimumTime - diffTime);
				*/

				PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView);

				//lets wait to give some time for the AI Manager and AI Views update its status
				Sleep(40);
				//printf("\n Iteration Human Decision %u", iteration);
				//iteration++;
			}

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunHumanGuessing()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_HUMAN) == mPlayers.end())
				continue;

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_GUESSING)
				continue;

			unsigned int time = Timer::GetRealTime();

			if (IsCloseHumanGuessing())
			{
				PlayerView playerView;
				bool isHumanDecision = MakeHumanGuessing(playerView);
				if (isHumanDecision)
				{
					//we enable ai view only if got a plan
					GameApplication* gameApp = (GameApplication*)Application::App;
					const GameViewList& gameViews = gameApp->GetGameViews();
					for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
					{
						std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
						if (pAiView)
							pAiView->SetEnabled(true);
					}

					unsigned int diffTime = Timer::GetRealTime() - time;
					std::stringstream ss;
					ss << "\n human close guessing total elapsed time " << diffTime;
					PrintInfo(ss.str());
					printf(ss.str().c_str());

					//lets sleep the process if the execution took less than 300 ms 
					/*
					unsigned int minimumTime = 300;
					if (diffTime < minimumTime)
						Sleep(minimumTime - diffTime);
					*/

					PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

					UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView); //update playerView
					UpdatePlayerSimulationView(mPlayers[GV_HUMAN], aiGuessView); //update guessView

					//lets wait to give some time for the AI Manager and AI Views update its status
					Sleep(40);
					//printf("\n Iteration Human Guessing %u", iteration);
					//iteration++;
				}
			}
			else
			{
				PlayerView playerView;
				bool isHumanDecision = MakeHumanGuessingDecision(playerView);
				if (isHumanDecision)
				{
					//we enable ai view only if got a plan
					GameApplication* gameApp = (GameApplication*)Application::App;
					const GameViewList& gameViews = gameApp->GetGameViews();
					for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
					{
						std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
						if (pAiView)
							pAiView->SetEnabled(true);
					}

					unsigned int diffTime = Timer::GetRealTime() - time;
					std::stringstream ss;
					ss << "\n human guessing decision total elapsed time " << diffTime;
					PrintInfo(ss.str());
					printf(ss.str().c_str());

					//lets sleep the process if the execution took less than 300 ms 
					/*
					unsigned int minimumTime = 300;
					if (diffTime < minimumTime)
						Sleep(minimumTime - diffTime);
					*/

					PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

					UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView); //update playerView
					UpdatePlayerSimulationView(mPlayers[GV_HUMAN], aiGuessView); //update guessView

					//lets wait to give some time for the AI Manager and AI Views update its status
					Sleep(40);
					//printf("\n Iteration Human Guessing Decision %u", iteration);
					//iteration++;
				}
			}
			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunHumanAwareDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_HUMAN) == mPlayers.end())
				continue;

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_AWARENESS)
				continue;

			unsigned int time = Timer::GetRealTime();

			PlayerView playerView;
			bool isHumanDecision = MakeHumanAwareDecision(playerView);
			if (isHumanDecision)
			{
				//we enable ai view only if got a plan
				GameApplication* gameApp = (GameApplication*)Application::App;
				const GameViewList& gameViews = gameApp->GetGameViews();
				for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
				{
					std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
					if (pAiView)
						pAiView->SetEnabled(true);
				}

				unsigned int diffTime = Timer::GetRealTime() - time;
				std::stringstream ss;
				ss << "\n human aware decision total elapsed time " << diffTime;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				//lets sleep the process if the execution took less than 300 ms 
				/*
				unsigned int minimumTime = 300;
				if (diffTime < minimumTime)
					Sleep(minimumTime - diffTime);
				*/

				PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView); //update playerView
				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], aiGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Sleep(40);
				//printf("\n Iteration Human Aware Decision %u", iteration);
				//iteration++;
			}

			//after complete execution we run guessing decision making
			mPlayerEvaluations[mPlayers.at(GV_HUMAN)] = ET_GUESSING;

			SetEnable(true);
		}
	}
}

void QuakeAIManager::GetPlayerInput(const AIAnalysis::PlayerInput& playerInput, PlayerData& playerData)
{
	playerData.player = playerInput.id;

	playerData.weapon = playerInput.weapon;
	playerData.weaponTime = playerInput.weaponTime;
	playerData.target = playerInput.target;

	for (unsigned int st = 0; st < MAX_STATS; st++)
		playerData.stats[st] = playerInput.stats[st];

	for (unsigned int wp = 0; wp < MAX_WEAPONS; wp++)
		playerData.ammo[wp] = playerInput.ammo[wp];

	playerData.plan.id = playerInput.planId;
	playerData.plan.node = mPathingGraph->FindNode(playerInput.planNode);

	playerData.plan.weight = 0.f;
	playerData.plan.path.clear();
	PathingNode* pathingNode = playerData.plan.node;
	for (int pathArc : playerInput.planPath)
	{
		PathingArc* pathingArc = pathingNode->FindArc(pathArc);
		playerData.plan.path.push_back(pathingArc);
		playerData.plan.weight += pathingArc->GetWeight();

		pathingNode = pathingArc->GetNode();
	}

	//valid if there is new path to travel
	playerData.valid = playerInput.planPath.size() == playerInput.planPathOffset.size() ? false : true;
}

void QuakeAIManager::GetPlayerInput(const AIAnalysis::PlayerInput& playerInput, PlayerData& playerData, PlayerData& playerDataOffset)
{
	playerData.player = playerInput.id;

	playerData.weapon = playerInput.weapon;
	playerData.weaponTime = playerInput.weaponTime;
	playerData.target = playerInput.target;

	for (unsigned int st = 0; st < MAX_STATS; st++)
		playerData.stats[st] = playerInput.stats[st];

	for (unsigned int wp = 0; wp < MAX_WEAPONS; wp++)
		playerData.ammo[wp] = playerInput.ammo[wp];

	playerData.plan.id = playerInput.planId;
	playerData.plan.node = mPathingGraph->FindNode(playerInput.planNode);

	playerData.plan.weight = 0.f;
	playerData.plan.path.clear();
	PathingNode* pathingNode = playerData.plan.node;
	for (int pathArc : playerInput.planPath)
	{
		PathingArc* pathingArc = pathingNode->FindArc(pathArc);
		playerData.plan.path.push_back(pathingArc);
		playerData.plan.weight += pathingArc->GetWeight();

		pathingNode = pathingArc->GetNode();
	}

	//valid if there is new path to travel
	playerData.valid = playerInput.planPath.size() == playerInput.planPathOffset.size() ? false : true;

	playerDataOffset = playerData;
	playerDataOffset.plan.node = mPathingGraph->FindNode(playerInput.planNodeOffset);
	playerDataOffset.plan.weight = playerInput.planOffset;

	playerDataOffset.plan.path.clear();
	pathingNode = playerData.plan.node;
	for (int pathArc : playerInput.planPathOffset)
	{
		PathingArc* pathingArc = pathingNode->FindArc(pathArc);
		playerDataOffset.plan.path.push_back(pathingArc);

		pathingNode = pathingArc->GetNode();
	}
}

void QuakeAIManager::GetPlayerOutput(const AIAnalysis::PlayerOutput& playerOutput, PlayerData& playerData)
{
	playerData.player = playerOutput.id;
	playerData.heuristic = playerOutput.heuristic;

	playerData.target = playerOutput.target;
	if (playerOutput.weapon != WP_NONE)
	{
		playerData.weapon = playerOutput.weapon;
		playerData.damage[playerData.weapon - 1] = playerOutput.damage;
	}

	playerData.plan.id = playerOutput.planId;
	playerData.plan.node = mPathingGraph->FindNode(playerOutput.planNode);

	PathingArcVec pathPlan;
	PathingNode* pathingNode = playerData.plan.node;
	for (int pathArc : playerOutput.planPath)
	{
		PathingArc* pathingArc = pathingNode->FindArc(pathArc);
		pathPlan.push_back(pathingArc);
		pathingNode = pathingArc->GetNode();
	}
	playerData.plan.ResetPathPlan(pathPlan);

	playerData.valid = true;
}

void QuakeAIManager::GetPlayerSimulation(const AIAnalysis::PlayerSimulation& playerSimulation, PlayerData& playerData)
{
	playerData.heuristic = playerSimulation.heuristic;

	playerData.target = playerSimulation.target;
	if (playerSimulation.weapon != WP_NONE)
	{
		playerData.weapon = playerSimulation.weapon;
		playerData.damage[playerData.weapon - 1] = playerSimulation.damage;
	}

	playerData.plan.id = playerSimulation.planId;

	PathingArcVec pathPlan;
	PathingNode* pathingNode = playerData.plan.node;
	for (int pathArc : playerSimulation.planPath)
	{
		PathingArc* pathingArc = pathingNode->FindArc(pathArc);
		pathPlan.push_back(pathingArc);
		pathingNode = pathingArc->GetNode();
	}
	playerData.plan.ResetPathPlan(pathPlan);

	playerData.items = playerSimulation.items;
	playerData.itemAmount= playerSimulation.itemAmount;
	playerData.itemWeight = playerSimulation.itemWeight;
}

void QuakeAIManager::SetPlayerInput(AIAnalysis::PlayerInput& playerInput, const PlayerData& playerData, const PlayerData& playerDataOffset)
{
	mUpdateMutex.lock();
	playerInput.frame = mUpdateCounter;
	mUpdateMutex.unlock();

	playerInput.id = playerData.player;

	playerInput.weapon = (WeaponType)playerData.weapon;
	playerInput.weaponTime = playerData.weaponTime;
	playerInput.target = playerData.target;

	for (unsigned int st = 0; st < MAX_STATS; st++)
		playerInput.stats[st] = playerData.stats[st];

	for (unsigned int wp = 0; wp < MAX_WEAPONS; wp++)
		playerInput.ammo[wp] = playerData.ammo[wp];

	playerInput.planId = playerData.plan.id;
	if (playerData.plan.node)
		playerInput.planNode = playerData.plan.node->GetId();
	playerInput.planPath.clear();
	for (PathingArc* pArc : playerData.plan.path)
		playerInput.planPath.push_back(pArc->GetId());

	playerInput.planOffset = playerDataOffset.plan.weight;
	if (playerDataOffset.plan.node)
		playerInput.planNodeOffset = playerDataOffset.plan.node->GetId();
	playerInput.planPathOffset.clear();
	for (PathingArc* pArc : playerDataOffset.plan.path)
		playerInput.planPathOffset.push_back(pArc->GetId());
}

void QuakeAIManager::SetPlayerOutput(AIAnalysis::PlayerOutput& playerOutput, const PlayerData& playerData)
{
	mUpdateMutex.lock();
	playerOutput.frame = mUpdateCounter;
	mUpdateMutex.unlock();

	playerOutput.id = playerData.player;
	playerOutput.heuristic = playerData.heuristic;

	playerOutput.target = playerData.target;
	if (playerData.weapon != WP_NONE)
	{
		playerOutput.weapon = (WeaponType)playerData.weapon;
		playerOutput.damage = playerData.damage[playerData.weapon - 1];
	}

	playerOutput.planId = playerData.plan.id;
	if (playerData.plan.node)
		playerOutput.planNode = playerData.plan.node->GetId();
	playerOutput.planPath.clear();
	for (PathingArc* pArc : playerData.plan.path)
		playerOutput.planPath.push_back(pArc->GetId());

	playerOutput.items = playerData.items;
}

void QuakeAIManager::SetPlayerSimulation(AIAnalysis::PlayerSimulation& playerSimulation, const PlayerData& playerData)
{
	playerSimulation.heuristic = playerData.heuristic;

	playerSimulation.target = playerData.target;
	if (playerData.weapon != WP_NONE)
	{
		playerSimulation.weapon = (WeaponType)playerData.weapon;
		playerSimulation.damage = playerData.damage[playerData.weapon - 1];
	}

	for (PathingArc* pArc : playerData.plan.path)
		playerSimulation.planPath.push_back(pArc->GetId());

	playerSimulation.items = playerData.items;
	playerSimulation.itemAmount = playerData.itemAmount;
	playerSimulation.itemWeight = playerData.itemWeight;
}

void QuakeAIManager::GetPlayerGround(ActorId player, bool& onGround)
{
	mPlayerGoundMutex[player].lock();
	onGround = mPlayerGrounds[player];
	mPlayerGoundMutex[player].unlock();
}

void QuakeAIManager::SetPlayerGround(ActorId player, bool onGround)
{
	mPlayerGoundMutex[player].lock();
	mPlayerGrounds[player] = onGround;
	mPlayerGoundMutex[player].unlock();
}

void QuakeAIManager::GetPlayerView(ActorId player, PlayerView& playerView)
{
	mPlayerViewMutex[player].lock();
	playerView = mPlayerViews[player];
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::SavePlayerView(ActorId player, const PlayerView& playerView)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player] = playerView;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerView(ActorId player, const PlayerView& playerView)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].isUpdated = playerView.isUpdated;
	mPlayerViews[player].data = playerView.data;
	mPlayerViews[player].gameItems = playerView.gameItems;
	for (auto const& guessView : playerView.guessViews)
	{
		const PlayerGuessView& playerGuessView = guessView.second;
		mPlayerViews[player].guessViews[guessView.first].isUpdated = playerGuessView.isUpdated;
		mPlayerViews[player].guessViews[guessView.first].guessPlayers = playerGuessView.guessPlayers;
		mPlayerViews[player].guessViews[guessView.first].guessItems = playerGuessView.guessItems;
		mPlayerViews[player].guessViews[guessView.first].items = playerGuessView.items;
		mPlayerViews[player].guessViews[guessView.first].data = playerGuessView.data;
	}
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerView(ActorId player, const PlayerData& playerData)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].data = playerData;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerView(ActorId player, const PlayerData& playerData, bool update)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].isUpdated = update;
	mPlayerViews[player].data = playerData;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerView(ActorId player, const PlayerView& playerView, float planWeight)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].data.planWeight = planWeight;
	mPlayerViews[player].gameItems = playerView.gameItems;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerGuessView(ActorId player, const PlayerGuessView& playerGuessView, bool isUpdated)
{
	mPlayerViewMutex[player].lock();
	if (isUpdated || !mPlayerViews[player].guessViews[playerGuessView.data.player].isUpdated)
		mPlayerViews[player].guessViews[playerGuessView.data.player] = playerGuessView;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerSimulationView(ActorId player, const PlayerGuessView& playerGuessView)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].guessViews[playerGuessView.data.player].isUpdated = playerGuessView.isUpdated;
	mPlayerViews[player].guessViews[playerGuessView.data.player].simulation = playerGuessView.simulation;
	mPlayerViews[player].guessViews[playerGuessView.data.player].guessSimulations[player] = playerGuessView.guessSimulations.at(player);
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerSimulationView(ActorId player, const PlayerView& playerView)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].isUpdated = playerView.isUpdated;
	mPlayerViews[player].simulation = playerView.simulation;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::SpawnActor(ActorId playerId)
{
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic *>(GameLogic::Get());

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock()));
	if (pPlayerActor)
	{
		PlayerView playerView;
		playerView.isUpdated = false;
		GetPlayerView(pPlayerActor->GetId(), playerView);

		//update game items
		UpdatePlayerItems(pPlayerActor->GetId(), playerView);

		std::shared_ptr<PhysicComponent> pPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (pPhysicComponent)
		{
			playerView.data = PlayerData(pPlayerActor);
			playerView.simulation = PlayerData(pPlayerActor);

			if (mPathingGraph)
			{
				PathingNode* spawnNode = mPathingGraph->FindClosestNode(pPhysicComponent->GetTransform().GetTranslation());
				playerView.data.plan = NodePlan(spawnNode, PathingArcVec());

				//assuming the guessing players has no idea where our player is located, lets take a random spawn spot
				Transform spawnTransform;
				game->SelectRandomFurthestSpawnPoint(spawnNode->GetPosition(), spawnTransform, false);
				spawnNode = mPathingGraph->FindClosestNode(spawnTransform.GetTranslation());

				std::vector<std::shared_ptr<PlayerActor>> playerActors;
				game->GetPlayerActors(playerActors);
				for (std::shared_ptr<PlayerActor> pOtherPlayerActor : playerActors)
				{
					if (pPlayerActor->GetId() == pOtherPlayerActor->GetId())
						continue;

					PlayerGuessView playerGuessView;
					playerGuessView.isUpdated = false;

					playerGuessView.data = PlayerData(pOtherPlayerActor);
					playerGuessView.simulation = PlayerData(pOtherPlayerActor);

					playerGuessView.guessPlayers[pPlayerActor->GetId()] = PlayerData(pPlayerActor);
					playerGuessView.guessSimulations[pPlayerActor->GetId()] = PlayerData(pPlayerActor);

					playerGuessView.data.plan = NodePlan(spawnNode, PathingArcVec());
					playerGuessView.simulation.plan = NodePlan(spawnNode, PathingArcVec());

					//what the guessing player is guessing about the other.
					Transform guessSpawnTransform;
					game->SelectRandomFurthestSpawnPoint(spawnNode->GetPosition(), guessSpawnTransform, false);
					PathingNode* guessSpawnNode = mPathingGraph->FindClosestNode(guessSpawnTransform.GetTranslation());
					playerGuessView.guessPlayers[pPlayerActor->GetId()].plan = NodePlan(guessSpawnNode, PathingArcVec());
					playerGuessView.guessSimulations[pPlayerActor->GetId()].plan = NodePlan(guessSpawnNode, PathingArcVec());

					//update game items
					UpdatePlayerGuessItems(playerGuessView);
					UpdatePlayerGuessItems(pPlayerActor->GetId(), playerGuessView);
					UpdatePlayerGuessItems(pOtherPlayerActor->GetId(), playerGuessView);

					playerView.guessViews[pOtherPlayerActor->GetId()] = playerGuessView;
				}
			}
		}

		SavePlayerView(pPlayerActor->GetId(), playerView);
	}
}

void QuakeAIManager::DetectActor(std::shared_ptr<PlayerActor> pPlayerActor, std::shared_ptr<Actor> pItemActor)
{
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic *>(GameLogic::Get());

	std::map<ActorId, PlayerData> aiViews;
	const GameViewList& gameViews = gameApp->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
		if (pAiView)
			aiViews[pAiView->GetActorId()] = pAiView->GetActionPlayer();
	}

	PlayerView playerView; 
	GetPlayerView(pPlayerActor->GetId(), playerView);

	std::vector<std::shared_ptr<PlayerActor>> playerActors;
	game->GetPlayerActors(playerActors);
	for (std::shared_ptr<PlayerActor> pOtherPlayerActor : playerActors)
	{
		if (pPlayerActor->GetId() == pOtherPlayerActor->GetId())
			continue;

		if (playerView.guessViews.find(pOtherPlayerActor->GetId()) == playerView.guessViews.end())
			continue;

		PlayerGuessView& playerGuessView = playerView.guessViews[pOtherPlayerActor->GetId()];
		if (!playerGuessView.data.plan.node)
			continue;

		// take into consideration within a certain radius
		std::shared_ptr<TransformComponent> pActorTransform(
			pItemActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (Length(pActorTransform->GetPosition() - playerGuessView.data.plan.node->GetPosition()) > 700.f)
			continue;

		std::shared_ptr<TransformComponent> pPlayerTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		PathingNode* playerNode = mPathingGraph->FindClosestNode(pPlayerTransformComponent->GetPosition());

		std::shared_ptr<TransformComponent> pOtherPlayerTransformComponent(
			pOtherPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		PathingNode* otherPlayerNode =
			mPathingGraph->FindClosestNode(pOtherPlayerTransformComponent->GetPosition());

		//if the noise is detected within a range then we reset the guess players status.
		std::stringstream playerInfo;
		playerInfo << "\n NOISE DETECTED by player: " << pPlayerActor->GetId() << " ";
		PrintInfo(playerInfo.str());

		playerGuessView.isUpdated = false;
		//we update the players path plan based on current position. This is actually not right and it should be predicted
		UpdatePlayerGuessPlan(pOtherPlayerActor, aiViews[pOtherPlayerActor->GetId()], playerGuessView.data, otherPlayerNode);

		UpdatePlayerGuessView(pPlayerActor->GetId(), playerGuessView, false);
	}
}

void QuakeAIManager::PrintError(std::string error)
{
	mLogError << error.c_str();
	mLogError.flush();
}

void QuakeAIManager::PrintInfo(std::string info)
{
	mLogInfo << info.c_str();
	mLogInfo.flush();
}

void QuakeAIManager::CalculateWeightItems(const PlayerData& playerData, std::map<ActorId, float>& searchItems)
{
	int maxAmmo = 0;
	int ammo = 0;

	//we calculate the need of each item and give a weight value based on it.
	for (auto& searchItem : searchItems)
	{
		ActorId item = searchItem.first;
		const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(item);
		if (itemPickup->GetType() == "Weapon")
		{
			switch (itemPickup->GetCode())
			{
				case WP_LIGHTNING:
					maxAmmo = 200;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second = 0.8f;

					if (searchItem.second > 0.1f)
						searchItem.second = searchItem.second < 0.6f ? searchItem.second : 0.6f; //threshold based on item importance
					else
						searchItem.second = 0.1f;
					break;
				case WP_SHOTGUN:
					maxAmmo = 20;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.6f;

					if (searchItem.second > 0.1f)
						searchItem.second = searchItem.second < 0.45f ? searchItem.second : 0.45f; //threshold based on item importance
					else
						searchItem.second = 0.1f;
					break;
				case WP_MACHINEGUN:
					maxAmmo = 200;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.4f;

					if (searchItem.second > 0.1f)
						searchItem.second = searchItem.second < 0.3f ? searchItem.second : 0.3f; //threshold based on item importance
					else
						searchItem.second = 0.1f;
					break;
				case WP_PLASMAGUN:
					maxAmmo = 120;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.5f;

					if (searchItem.second > 0.1f)
						searchItem.second = searchItem.second < 0.35f ? searchItem.second : 0.35f; //threshold based on item importance
					else
						searchItem.second = 0.1f;
					break;
				case WP_GRENADE_LAUNCHER:
					maxAmmo = 20;

					//lets calculate the item weight based on the player status
					searchItem.second = 0.f;
					break;
				case WP_ROCKET_LAUNCHER:
					maxAmmo = 20;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.6f;

					if (searchItem.second > 0.1f)
						searchItem.second = searchItem.second < 0.45f ? searchItem.second : 0.45f; //threshold based on item importance
					else
						searchItem.second = 0.1f;
					break;
				case WP_RAILGUN:
					maxAmmo = 20;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.8f;

					if (searchItem.second > 0.1f)
						searchItem.second = searchItem.second < 0.6f ? searchItem.second : 0.6f; //threshold based on item importance
					else
						searchItem.second = 0.1f;
					break;
			}
		}
		else if (itemPickup->GetType() == "Ammo")
		{
			switch (itemPickup->GetCode())
			{
				case WP_LIGHTNING:
					maxAmmo = 200;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.8f;

						if (searchItem.second > 0.1f)
							searchItem.second = searchItem.second < 0.6f ? searchItem.second : 0.6f; //threshold based on item importance
						else
							searchItem.second = 0.1f;
					}
					else searchItem.second = 0.1f;
					break;
				case WP_SHOTGUN:
					maxAmmo = 20;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.6f;

						if (searchItem.second > 0.1f)
							searchItem.second = searchItem.second < 0.45f ? searchItem.second : 0.45f; //threshold based on item importance
						else
							searchItem.second = 0.1f;
					}
					else searchItem.second = 0.1f;
					break;
				case WP_MACHINEGUN:
					maxAmmo = 200;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.4f;

						if (searchItem.second > 0.1f)
							searchItem.second = searchItem.second < 0.3f ? searchItem.second : 0.3f; //threshold based on item importance
						else
							searchItem.second = 0.1f;
					}
					else searchItem.second = 0.1f;
					break;
				case WP_PLASMAGUN:
					maxAmmo = 120;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.5f;

						if (searchItem.second > 0.1f)
							searchItem.second = searchItem.second < 0.35f ? searchItem.second : 0.35f; //threshold based on item importance
						else
							searchItem.second = 0.1f;
					}
					else searchItem.second = 0.1f;
					break;
				case WP_GRENADE_LAUNCHER:
					//lets calculate the item weight based on the player status
					searchItem.second = 0.f;
					break;
				case WP_ROCKET_LAUNCHER:
					maxAmmo = 20;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.6f;

						if (searchItem.second > 0.1f)
							searchItem.second = searchItem.second < 0.45f ? searchItem.second : 0.45f; //threshold based on item importance
						else
							searchItem.second = 0.1f;
					}
					else searchItem.second = 0.1f;
					break;
				case WP_RAILGUN:
					maxAmmo = 20;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.8f;

						if (searchItem.second > 0.1f)
							searchItem.second = searchItem.second < 0.6f ? searchItem.second : 0.6f; //threshold based on item importance
						else
							searchItem.second = 0.1f;
					}
					else searchItem.second = 0.1f;
					break;
			}
		}
		else if (itemPickup->GetType() == "Armor")
		{
			int maxArmor = 100;

			//lets calculate the item weight based on the player status
			searchItem.second = (itemPickup->GetMaximum() - playerData.stats[STAT_ARMOR]) / (float)maxArmor;
			searchItem.second *= 0.8f;

			if (searchItem.second > 0.1f)
			{
				if (itemPickup->GetCode() != 3) //any armor item
					searchItem.second = searchItem.second < 0.6f ? searchItem.second : 0.6f; //threshold based on item importance
				else //armor shard
					searchItem.second = 0.1f; //threshold based on item importance
			}
			else searchItem.second = 0.1f;
		}
		else if (itemPickup->GetType() == "Health")
		{
			int maxHealth = 100;

			//lets calculate the item weight based on the player status
			searchItem.second = (itemPickup->GetMaximum() - playerData.stats[STAT_HEALTH]) / (float)maxHealth;
			searchItem.second *= 0.8f;

			if (searchItem.second > 0.1f)
			{
				if (itemPickup->GetCode() != 4) //any health item
					searchItem.second = searchItem.second < 0.8f ? searchItem.second : 0.8f; //threshold based on item importance
				else //health small
					searchItem.second = 0.1f; //threshold based on item importance
			}
			else searchItem.second = 0.1f;
		}
	}
}

float QuakeAIManager::CalculateHeuristicItem(const PlayerData& playerData, ActorId item, float itemWeight)
{
	float score = 0.f;
	float heuristic = 0.f;
	float maxWeight = 6.0f;
	float weight = 0.f;
	int maxAmmo = 0;
	int ammo = 0;

	const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(item);
	if (itemPickup->GetType() == "Weapon")
	{
		switch (itemPickup->GetCode())
		{
			case WP_LIGHTNING:
				maxAmmo = 200;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.5f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_SHOTGUN:
				maxAmmo = 20;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.4f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_MACHINEGUN:
				maxAmmo = 200;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.2f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_PLASMAGUN:
				maxAmmo = 120;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.275f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_GRENADE_LAUNCHER:
				maxAmmo = 20;
					
				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_ROCKET_LAUNCHER:
				maxAmmo = 20;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.35f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_RAILGUN:
				maxAmmo = 20;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.5f; //how important is this ammo

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
		}
	}
	else if (itemPickup->GetType() == "Ammo")
	{
		switch (itemPickup->GetCode())
		{
			case WP_LIGHTNING:
				maxAmmo = 200;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.5f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
			case WP_SHOTGUN:
				maxAmmo = 20;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.4f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
			case WP_MACHINEGUN:
				maxAmmo = 200;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.2f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
			case WP_PLASMAGUN:
				maxAmmo = 120;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.275f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
			case WP_GRENADE_LAUNCHER:
				maxAmmo = 20;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
			case WP_ROCKET_LAUNCHER:
				maxAmmo = 20;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.35f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
			case WP_RAILGUN:
				maxAmmo = 20;
				if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
				{
					weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.5f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
		}
	}
	else if (itemPickup->GetType() == "Armor")
	{
		if (itemPickup->GetMaximum() > playerData.stats[STAT_ARMOR])
		{
			//ammo
			score = (maxAmmo - ammo) / (float)maxAmmo;
			score *= 0.5f; //how important is this ammo

			score = (itemPickup->GetMaximum() - playerData.stats[STAT_ARMOR]) / (float)itemPickup->GetMaximum();
			if (itemPickup->GetCode() == 1) //armor body
				score *= 0.4f;
			else if (itemPickup->GetCode() == 2) //armor combat
				score *= 0.3f;
			else //armor shard
				score *= 0.02f;

			weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;

			//relation based on item score and distance traversed
			heuristic = score * (1.0f - (weight / (float)maxWeight));
		}
	}
	else if (itemPickup->GetType() == "Health")
	{
		if (itemPickup->GetMaximum() > playerData.stats[STAT_HEALTH])
		{
			score = (itemPickup->GetMaximum() - playerData.stats[STAT_HEALTH]) / (float)itemPickup->GetMaximum();
			if (itemPickup->GetCode() == 1) //health
				score *= 0.3f;
			else if (itemPickup->GetCode() == 2) //health large
				score *= 0.4f;
			else if (itemPickup->GetCode() == 3) //health mega
				score *= 0.5f;
			else //health small
				score *= 0.02f;

			weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;

			//relation based on item score and distance traversed
			heuristic = score * (1.0f - (weight / (float)maxWeight));
		}
	}

	return heuristic;
}


float QuakeAIManager::CalculateHeuristicItems(const PlayerData& playerData)
{
	float heuristic = 0.f;

	//heuristic from picked up items
	for (auto const& item : playerData.items)
		heuristic += CalculateHeuristicItem(playerData, item.first, playerData.itemWeight.at(item.first));

	return heuristic;
}

float QuakeAIManager::CalculateBestHeuristicItem(const PlayerData& playerData)
{
	float score = 0.f;
	float maxWeight = 6.0f;
	float weight = 0.f;
	int maxAmmo = 0;
	int ammo = 0;

	//heuristic from picked up items
	ActorId bestItem = INVALID_ACTOR_ID;
	std::map<ActorId, float> heuristicItems;
	heuristicItems[bestItem] = 0.f;
	for (auto const& item : playerData.items)
	{
		const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(item.first);
		if (itemPickup->GetType() == "Weapon")
		{
			switch (itemPickup->GetCode())
			{
				case WP_LIGHTNING:
					maxAmmo = 200;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.5f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
				case WP_SHOTGUN:
					maxAmmo = 20;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.4f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
				case WP_MACHINEGUN:
					maxAmmo = 200;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.2f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
				case WP_PLASMAGUN:
					maxAmmo = 120;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.275f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
				case WP_GRENADE_LAUNCHER:
					maxAmmo = 20;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
				case WP_ROCKET_LAUNCHER:
					maxAmmo = 20;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.35f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
				case WP_RAILGUN:
					maxAmmo = 20;

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					score = (maxAmmo - ammo) / (float)maxAmmo;
					score *= 0.5f; //how important is this ammo

					weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

					//relation based on item score and distance traversed
					heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
					if (heuristicItems[bestItem] < heuristicItems[item.first])
						bestItem = item.first;
					break;
			}
		}
		else if (itemPickup->GetType() == "Ammo")
		{
			switch (itemPickup->GetCode())
			{
				case WP_LIGHTNING:
					maxAmmo = 200;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.5f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
				case WP_SHOTGUN:
					maxAmmo = 20;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.4f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
				case WP_MACHINEGUN:
					maxAmmo = 200;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.2f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
				case WP_PLASMAGUN:
					maxAmmo = 120;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.275f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
				case WP_GRENADE_LAUNCHER:
					maxAmmo = 20;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
				case WP_ROCKET_LAUNCHER:
					maxAmmo = 20;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.35f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
				case WP_RAILGUN:
					maxAmmo = 20;
					if (playerData.itemAmount.at(item.first) && playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						score = (maxAmmo - ammo) / (float)maxAmmo;
						score *= 0.5f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
			}
		}
		else if (itemPickup->GetType() == "Armor")
		{
			if (playerData.itemAmount.at(item.first))
			{
				score = (itemPickup->GetMaximum() - playerData.stats[STAT_ARMOR]) / (float)itemPickup->GetMaximum();
				if (itemPickup->GetCode() == 1) //armor body
					score *= 0.4f;
				else if (itemPickup->GetCode() == 2) //armor combat
					score *= 0.3f;
				else //armor shard
					score *= 0.02f;

				weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

				//relation based on item score and distance traversed
				heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
				if (heuristicItems[bestItem] < heuristicItems[item.first])
					bestItem = item.first;
			}
		}
		else if (itemPickup->GetType() == "Health")
		{
			if (playerData.itemAmount.at(item.first))
			{
				score = (itemPickup->GetMaximum() - playerData.stats[STAT_HEALTH]) / (float)itemPickup->GetMaximum();
				if (itemPickup->GetCode() == 1) //health
					score *= 0.3f;
				else if (itemPickup->GetCode() == 2) //health large
					score *= 0.4f;
				else if (itemPickup->GetCode() == 3) //health mega
					score *= 0.5f;
				else //health small
					score *= 0.02f;

				weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

				//relation based on item score and distance traversed
				heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
				if (heuristicItems[bestItem] < heuristicItems[item.first])
					bestItem = item.first;
			}
		}
	}

	//heuristic value comes from the best item
	float heuristic = heuristicItems[bestItem];
	heuristicItems.erase(bestItem);
	//the other items add a small heuristic percentage
	for (auto& heuristicItem : heuristicItems)
		heuristic += heuristicItem.second * 0.1f;

	return heuristic;
}

void QuakeAIManager::CalculateHeuristic(EvaluationType evaluation, PlayerData& playerData, PlayerData& otherPlayerData)
{
	float heuristic = 0.f;

	//heuristic from picked up items
	heuristic += CalculateBestHeuristicItem(playerData);
	heuristic -= CalculateBestHeuristicItem(otherPlayerData);

	//heuristic from damage dealing
	int playerMaxDamage = 0, otherPlayerMaxDamage = 0;
	if (playerData.weapon != WP_NONE)
	{
		if (playerData.damage[playerData.weapon - 1] >= MAX_DAMAGE)
		{
			//the player keeps current weapon if it is above the maximum damage
			playerData.target = otherPlayerData.player;
			playerMaxDamage = playerData.damage[playerData.weapon - 1];
		}
	}
	if (playerMaxDamage == 0)
	{
		for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
		{
			if (playerData.damage[weapon - 1] > playerMaxDamage)
			{
				playerData.target = otherPlayerData.player;
				playerData.weapon = (WeaponType)weapon;
				playerMaxDamage = playerData.damage[weapon - 1];
			}
		}
	}
	if (playerMaxDamage == 0)
	{
		playerData.target = INVALID_ACTOR_ID;
		playerData.weapon = WP_NONE;
	}

	if (otherPlayerData.weapon != WP_NONE)
	{
		if (otherPlayerData.damage[otherPlayerData.weapon - 1] >= MAX_DAMAGE)
		{
			//the other player keeps current weapon if it is above the maximum damage
			otherPlayerData.target = playerData.player;
			otherPlayerMaxDamage = otherPlayerData.damage[otherPlayerData.weapon - 1];
		}
	}
	if (otherPlayerMaxDamage == 0)
	{
		for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
		{
			if (otherPlayerData.damage[weapon - 1] > otherPlayerMaxDamage)
			{
				otherPlayerData.target = playerData.player;
				otherPlayerData.weapon = (WeaponType)weapon;
				otherPlayerMaxDamage = otherPlayerData.damage[weapon - 1];
			}
		}
	}
	if (otherPlayerMaxDamage == 0)
	{
		otherPlayerData.target = INVALID_ACTOR_ID;
		otherPlayerData.weapon = WP_NONE;
	}
	playerMaxDamage = playerMaxDamage > MAX_DAMAGE ? (int)MAX_DAMAGE : playerMaxDamage;
	otherPlayerMaxDamage = otherPlayerMaxDamage > MAX_DAMAGE ? (int)MAX_DAMAGE : otherPlayerMaxDamage;

	//prioritize damage heuristic based on players health/armor and weapon status
	float playerStatus = 
		(CalculatePlayerStatus(playerData) > 0.3f && CalculatePlayerWeaponStatus(playerData) > 0.f) ? 1.2f : 0.4f;
	heuristic += (playerMaxDamage / (float)MAX_DAMAGE) * playerStatus;

	float otherPlayerStatus = 
		(CalculatePlayerStatus(otherPlayerData) > 0.3f && CalculatePlayerWeaponStatus(otherPlayerData) > 0.f) ? 1.2f : 0.4f;
	heuristic -= (otherPlayerMaxDamage / (float)MAX_DAMAGE) * otherPlayerStatus;

	playerData.heuristic = heuristic;
	otherPlayerData.heuristic = heuristic;
}

//score is calculated based on health and armor
float QuakeAIManager::CalculatePlayerStatus(const PlayerData& playerData)
{
	//health & armor status
	unsigned int maxHealth = 200;
	unsigned int maxArmor = 200;
	float playerScore = (playerData.stats[STAT_HEALTH] / (float)maxHealth) * 0.6f;
	playerScore += (playerData.stats[STAT_ARMOR] / (float)maxArmor) * 0.4f;
	return playerScore;
}

//weapon score
float QuakeAIManager::CalculatePlayerWeaponStatus(const PlayerData& playerData)
{
	int maxAmmo = 0;
	float score = 0.f;
	for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
	{
		switch (weapon)
		{
			case WP_LIGHTNING:
				maxAmmo = 200;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//top tier wepon
					if (score < 0.6f)
						score = 0.6f;
				}
				break;

			case WP_RAILGUN:
				maxAmmo = 20;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//top tier wepon
					if (score < 0.6f)
						score = 0.6f;
				}
				break;

			case WP_SHOTGUN:
				maxAmmo = 20;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//medium tier weapon
					if (score < 0.3f)
						score = 0.3f;
				}
				break;

			case WP_ROCKET_LAUNCHER:
				maxAmmo = 20;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//medium tier weapon
					if (score < 0.3f)
						score = 0.3f;
				}
				break;

			case WP_PLASMAGUN:
				maxAmmo = 120;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//low tier weapon
					if (score < 0.f)
						score = 0.f;
				}
				break;

			case WP_MACHINEGUN:
				maxAmmo = 200;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//low tier weapon
					if (score < 0.f)
						score = 0.f;
				}
				break;

			case WP_GRENADE_LAUNCHER:
				maxAmmo = 20;
				if (playerData.ammo[weapon] >= maxAmmo * 0.2f && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
				{
					//low tier weapon
					if (score < 0.f)
						score = 0.f;
				}
				break;
		}
	}

	return score;
}

void QuakeAIManager::CalculateDamage(PlayerData& playerData, const std::map<float, VisibilityData>& visibility)
{
	for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
	{
		if (weapon != WP_GAUNTLET)
		{
			bool weaponAvailable = false;
			int ammo = 0, damage = 0, shotCount = 0, maxAmmo = 200;
			float fireTime = 0.f, visibleTime = 0.f, weaponTime = FLT_MAX, rangeDistance = 0.f;

			if (playerData.ammo[weapon] && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
			{
				weaponTime = 0.f;
				weaponAvailable = true;
			}

			std::map<float, int> itemAmmo;
			for (auto const& item : playerData.items)
			{
				const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(item.first);
				if (itemPickup->GetType() == "Weapon")
				{
					const AIAnalysis::WeaponActorPickup* weaponPickup = static_cast<const AIAnalysis::WeaponActorPickup*>(itemPickup);
					if (weaponPickup->GetCode() == weapon)
					{
						itemAmmo[playerData.itemWeight[item.first]] = weaponPickup->GetAmmo();

						weaponAvailable = true;
						if (weaponTime > playerData.itemWeight[item.first])
							weaponTime = playerData.itemWeight[item.first];
					}
				}
				else if (itemPickup->GetType() == "Ammo")
				{
					if (itemPickup->GetCode() == weapon)
					{
						itemAmmo[playerData.itemWeight[item.first]] = itemPickup->GetAmount();
						if (playerData.stats[STAT_WEAPONS] & (1 << weapon))
						{
							weaponAvailable = true;
							if (weaponTime > playerData.itemWeight[item.first])
								weaponTime = playerData.itemWeight[item.first];
						}
					}
				}
			}

			if (!weaponAvailable)
				continue;

			if (weaponTime < playerData.weaponTime)
				weaponTime = playerData.weaponTime;
			if (playerData.weapon != weapon)
			{
				//it means that we need to add the weapon switch time (0.5 sec)
				weaponTime += 0.5f;
			}

			switch (weapon)
			{
				case WP_LIGHTNING:
					damage = 6;
					fireTime = 0.05f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (auto& visible : visibility)
					{
						if (itemAmmo.find(visible.first) != itemAmmo.end())
						{
							ammo += itemAmmo[visible.first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if (visible.first < weaponTime)
							continue;

						// lets assign a firing threshold
						if (visible.second.moveTime > 0.1f)
						{
							if (visible.second.moveDistance <= 800)
							{
								shotCount = (int)ceil(visible.second.moveTime / fireTime);
								shotCount = shotCount > ammo ? ammo : shotCount;
								ammo -= shotCount;
								playerData.damage[weapon - 1] += damage * shotCount;
							}
						}
					}
					break;

				case WP_SHOTGUN:
					damage = 110;
					fireTime = 1.0f;
					visibleTime = 0.f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (auto& visible : visibility)
					{
						if (itemAmmo.find(visible.first) != itemAmmo.end())
						{
							ammo += itemAmmo[visible.first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if (visible.first < weaponTime || visible.first + visible.second.moveTime < visibleTime)
							continue;

						// lets assign a firing threshold
						if (visible.second.moveTime >= 0.1f)
						{
							visibleTime = visible.first + visible.second.moveTime + fireTime;

							shotCount = (int)ceil(visible.second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							rangeDistance = visible.second.moveDistance > 500 ? visible.second.moveDistance : 500;
							playerData.damage[weapon - 1] += (int)round(damage *
								(1.f - (visible.second.moveDistance / rangeDistance)) * shotCount);
						}
					}
					break;

				case WP_MACHINEGUN:
					damage = 5;
					fireTime = 0.1f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (auto& visible : visibility)
					{
						if (itemAmmo.find(visible.first) != itemAmmo.end())
						{
							ammo += itemAmmo[visible.first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if (visible.first < weaponTime)
							continue;

						// lets assign a firing threshold
						if (visible.second.moveTime >= 0.1f)
						{
							shotCount = (int)ceil(visible.second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							rangeDistance = visible.second.moveDistance > 500 ? visible.second.moveDistance : 500;
							playerData.damage[weapon - 1] += (int)round(damage *
								(1.f - (visible.second.moveDistance / rangeDistance)) * shotCount);
						}
					}
					break;

				case WP_GRENADE_LAUNCHER:
					damage = 100;
					fireTime = 0.8f;
					playerData.damage[weapon - 1] = 0;
					break;

				case WP_ROCKET_LAUNCHER:
					damage = 100;
					fireTime = 0.8f;
					visibleTime = 0.f;
					playerData.damage[weapon - 1] = 0;
					
					ammo = playerData.ammo[weapon];
					for (auto& visible : visibility)
					{
						if (itemAmmo.find(visible.first) != itemAmmo.end())
						{
							ammo += itemAmmo[visible.first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if (visible.first < weaponTime || visible.first + visible.second.moveTime < visibleTime)
							continue;

						// lets assign a firing threshold
						if (visible.second.moveTime >= 0.1f)
						{
							visibleTime = visible.first + visible.second.moveTime + fireTime;

							shotCount = (int)ceil(visible.second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							if (visible.second.moveHeight <= 30.f)
								rangeDistance = visible.second.moveDistance > 500 ? visible.second.moveDistance : 500;
							else
								rangeDistance = visible.second.moveDistance > 700 ? visible.second.moveDistance : 700;
							playerData.damage[weapon - 1] += (int)round(damage *
								(1.f - (visible.second.moveDistance / rangeDistance)) * shotCount);
						}
					}
					break;

				case WP_PLASMAGUN:
					damage = 10;
					fireTime = 0.1f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (auto& visible : visibility)
					{
						if (itemAmmo.find(visible.first) != itemAmmo.end())
						{
							ammo += itemAmmo[visible.first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if (visible.first < weaponTime)
							continue;

						// lets assign a firing threshold
						if (visible.second.moveTime >= 0.1f)
						{
							shotCount = (int)ceil(visible.second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							rangeDistance = visible.second.moveDistance > 400 ? visible.second.moveDistance : 400;
							playerData.damage[weapon - 1] += (int)round(damage *
								(1.f - (visible.second.moveDistance / rangeDistance)) * shotCount);
						}
					}
					break;

				case WP_RAILGUN:
					damage = 100;
					fireTime = 1.5f;
					visibleTime = 0.f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (auto& visible : visibility)
					{
						if (itemAmmo.find(visible.first) != itemAmmo.end())
						{
							ammo += itemAmmo[visible.first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if (visible.first < weaponTime || visible.first + visible.second.moveTime < visibleTime)
							continue;

						// lets assign a firing threshold
						if (visible.second.moveTime >= 0.1f)
						{
							visibleTime = visible.first + visible.second.moveTime + fireTime;

							shotCount = (int)ceil(visible.second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							if (visible.second.moveDistance > 500)
								playerData.damage[weapon - 1] += (damage + 50) * shotCount;
							else if (visible.second.moveDistance < 300)
								playerData.damage[weapon - 1] += (damage - 50) * shotCount;
							else
								playerData.damage[weapon - 1] += damage * shotCount;
						}
					}
					break;
			}
		}
		else
		{
			int damage = 50;
			int shotCount = 0;
			float fireTime = 1.5f;
			float visibleTime = 0.f;
			playerData.damage[weapon - 1] = 0;

			float weaponTime = playerData.weaponTime;
			if (playerData.weapon != weapon)
			{
				//it means that we need to add the weapon switch time (0.5 sec)
				weaponTime += 0.5f;
			}

			for (auto& visible : visibility)
			{
				if (visible.first < weaponTime || visible.first + visible.second.moveTime < visibleTime)
					continue;

				// lets assign a firing threshold
				if (visible.second.moveTime >= 0.1f)
				{
					if (visible.second.moveDistance <= 20.f)
					{
						visibleTime = visible.first + visible.second.moveTime + fireTime;

						shotCount = (int)ceil(visible.second.moveTime / fireTime);
						playerData.damage[weapon - 1] += damage * shotCount;
					}
				}
			}
		}
	}
}

void QuakeAIManager::CalculateVisibility(
	PathingNode* playerNode, float playerPathOffset, float playerVisibleTime,
	const PathingArcVec& playerPathPlan, std::map<float, VisibilityData>& playerVisibility, 
	PathingNode* otherPlayerNode, float otherPlayerPathOffset, float otherPlayerVisibleTime,
	const PathingArcVec& otherPlayerPathPlan, std::map<float, VisibilityData>& otherPlayerVisibility)
{
	float totalWeight = 0.f, totalArcWeight = 0.f;
	unsigned int index = 0, otherIndex = 0, otherPathIndex = 0;

	PathingNode* currentNode = playerNode;
	PathingArc* currentArc = NULL;

	PathingNode* otherCurrentNode = otherPlayerNode;
	PathingArc* otherCurrentArc = NULL;

	for (; otherPathIndex < otherPlayerPathPlan.size(); otherPathIndex++)
	{
		otherCurrentArc = otherPlayerPathPlan[otherPathIndex];
		auto otherTransitionNodes = otherCurrentArc->GetTransition()->GetNodes();
		auto otherTransitionPositions = otherCurrentArc->GetTransition()->GetPositions();

		bool foundPathOffset = false;
		for (; otherIndex < otherTransitionPositions.size(); otherIndex++)
		{
			float otherCurrentWeight = otherCurrentArc->GetTransition()->GetWeights()[otherIndex];
			if (totalWeight + otherCurrentWeight > otherPlayerPathOffset)
			{
				foundPathOffset = true;
				break;
			}

			totalWeight += otherCurrentWeight;
		}

		if (foundPathOffset)
			break;

		otherIndex = 0;
		otherCurrentNode = otherCurrentArc->GetNode();
	}

	totalWeight = 0.f;
	PathingArcVec::const_iterator itPathPlan = playerPathPlan.begin();
	for (; itPathPlan != playerPathPlan.end(); itPathPlan++)
	{
		currentArc = *itPathPlan;
		auto transitionNodes = currentArc->GetTransition()->GetNodes();
		auto transitionPositions = currentArc->GetTransition()->GetPositions();

		bool foundPathOffset = false;
		for (; index < transitionPositions.size(); index++)
		{
			float currentWeight = currentArc->GetTransition()->GetWeights()[index];
			if (totalWeight + currentWeight > playerPathOffset)
			{
				foundPathOffset = true;
				break;
			}

			totalWeight += currentWeight;
		}

		if (foundPathOffset)
			break;

		index = 0;
		currentNode = currentArc->GetNode();
	}

	auto visibilityIt = playerVisibility.begin();
	auto otherVisibilityIt = otherPlayerVisibility.begin();

	totalWeight = 0.f;
	//lets calculate the visibility for simultaneous path travelling.
	if (otherCurrentArc != NULL)
	{
		bool otherPathPlanEnd = false;
		for (; itPathPlan != playerPathPlan.end(); itPathPlan++)
		{
			currentArc = *itPathPlan;
			auto transitionNodes = currentArc->GetTransition()->GetNodes();
			auto otherTransitionNodes = otherCurrentArc->GetTransition()->GetNodes();
			auto transitionPositions = currentArc->GetTransition()->GetPositions();
			auto otherTransitionPositions = otherCurrentArc->GetTransition()->GetPositions();

			for (; index < transitionPositions.size(); index++)
			{
				float currentWeight = currentArc->GetTransition()->GetWeights()[index];

				//we only do ray casting for players which are either standing or moving (not jumping, falling...)
				if (otherPathPlanEnd || currentArc->GetType() == AT_MOVE || otherCurrentArc->GetType() == AT_MOVE)
				{
					if (transitionNodes[index]->IsVisibleNode(otherTransitionNodes[otherIndex]))
						//if (RayCollisionDetection(transitionPositions[index], otherTransitionPositions[otherIndex]) == NULL)
					{
						if (currentArc->GetType() == AT_MOVE)
						{
							for (auto visIt = std::next(visibilityIt); visIt != playerVisibility.end(); visIt++)
							{
								if (visIt->first > totalWeight)
									break;

								visibilityIt++;
							}

							//lets check that we are in the visibile time frame
							if (visibilityIt != playerVisibility.end() && visibilityIt->first >= playerVisibleTime)
							{
								visibilityIt->second.moveDistance += Length(
									otherTransitionPositions[otherIndex] - transitionPositions[index]) * currentWeight;
								visibilityIt->second.moveHeight +=
									(transitionPositions[index][AXIS_Y] - otherTransitionPositions[otherIndex][AXIS_Y]) * currentWeight;
								visibilityIt->second.moveTime += currentWeight;
							}
						}

						if (otherPathPlanEnd || otherCurrentArc->GetType() == AT_MOVE)
						{
							for (auto otherVisIt = std::next(otherVisibilityIt); otherVisIt != otherPlayerVisibility.end(); otherVisIt++)
							{
								if (otherVisIt->first > totalWeight)
									break;

								otherVisibilityIt++;
							}

							//lets check that we are in the visibile time frame
							if (otherVisibilityIt != otherPlayerVisibility.end() && otherVisibilityIt->first >= otherPlayerVisibleTime)
							{
								otherVisibilityIt->second.moveDistance += Length(
									otherTransitionPositions[otherIndex] - transitionPositions[index]) * currentWeight;
								otherVisibilityIt->second.moveHeight +=
									(otherTransitionPositions[otherIndex][AXIS_Y] - transitionPositions[index][AXIS_Y]) * currentWeight;
								otherVisibilityIt->second.moveTime += currentWeight;
							}
						}
					}
				}

				while (totalArcWeight <= totalWeight)
				{
					totalArcWeight += otherCurrentArc->GetTransition()->GetWeights()[otherIndex];
					if (otherIndex + 1 >= otherTransitionPositions.size())
					{
						if (otherPathIndex + 1 < otherPlayerPathPlan.size())
						{
							otherPathIndex++;
							otherCurrentArc = otherPlayerPathPlan[otherPathIndex];
							otherTransitionPositions = otherCurrentArc->GetTransition()->GetPositions();
							otherTransitionNodes = otherCurrentArc->GetTransition()->GetNodes();

							otherIndex = 0;
							otherCurrentNode = otherPlayerPathPlan[otherPathIndex]->GetNode();

						}
						else
						{
							otherPathPlanEnd = true;
							break;
						}
					}
					else otherIndex++;
				}
				totalWeight += currentWeight;
				// set timelimit; any time further is likely to be unrealistic simulation
				if (totalWeight > 3.f)
					return;
			}

			currentNode = currentArc->GetNode();
			index = 0;
		}
	}
	else
	{
		for (; itPathPlan != playerPathPlan.end(); itPathPlan++)
		{
			currentArc = *itPathPlan;
			auto transitionNodes = currentArc->GetTransition()->GetNodes();
			auto transitionPositions = currentArc->GetTransition()->GetPositions();
			for (; index < transitionPositions.size(); index++)
			{
				float currentWeight = currentArc->GetTransition()->GetWeights()[index];

				//we only do ray casting for players which are either standing or moving (not jumping, falling...)

				if (transitionNodes[index]->IsVisibleNode(otherCurrentNode))
					//if (RayCollisionDetection(transitionPositions[index], otherTransitionPositions[otherIndex]) == NULL)
				{
					if (currentArc->GetType() == AT_MOVE)
					{
						for (auto visIt = std::next(visibilityIt); visIt != playerVisibility.end(); visIt++)
						{
							if (visIt->first > totalWeight)
								break;

							visibilityIt++;
						}

						//lets check that we are in the visibile time frame
						if (visibilityIt != playerVisibility.end() && visibilityIt->first >= playerVisibleTime)
						{
							visibilityIt->second.moveDistance += Length(
								otherCurrentNode->GetPosition() - transitionPositions[index]) * currentWeight;
							visibilityIt->second.moveHeight +=
								(transitionPositions[index][AXIS_Y] - otherCurrentNode->GetPosition()[AXIS_Y]) * currentWeight;
							visibilityIt->second.moveTime += currentWeight;
						}
					}

					for (auto otherVisIt = std::next(otherVisibilityIt); otherVisIt != otherPlayerVisibility.end(); otherVisIt++)
					{
						if (otherVisIt->first > totalWeight)
							break;

						otherVisibilityIt++;
					}

					//lets check that we are in the visibile time frame
					if (otherVisibilityIt != otherPlayerVisibility.end() && otherVisibilityIt->first >= otherPlayerVisibleTime)
					{
						otherVisibilityIt->second.moveDistance += Length(
							otherCurrentNode->GetPosition() - transitionPositions[index]) * currentWeight;
						otherVisibilityIt->second.moveHeight +=
							(otherCurrentNode->GetPosition()[AXIS_Y] - transitionPositions[index][AXIS_Y]) * currentWeight;
						otherVisibilityIt->second.moveTime += currentWeight;
					}
				}

				totalWeight += currentWeight;
				// set timelimit; any time further is likely to be unrealistic simulation
				if (totalWeight > 3.f)
					return;
			}

			currentNode = currentArc->GetNode();
			index = 0;
		}
	}

	float totalVisibleWeight= 0.f;
	for (auto& visibility : playerVisibility)
		totalVisibleWeight += visibility.second.moveTime;

	if (totalVisibleWeight < 1.5f)
	{
		//we need to add visible time if the total visible move time is short
		if (currentNode->IsVisibleNode(otherCurrentNode))
			/*if (RayCollisionDetection(currentNode->GetPosition(), otherCurrentNode->GetPosition()) == NULL)*/
		{
			float currentWeight = 0.5f;
			totalVisibleWeight = totalVisibleWeight < 0.75f ? 2.f : 1.f;
			if (visibilityIt != playerVisibility.end())
			{
				float minimumVisibleWeight = playerVisibleTime < otherPlayerVisibleTime ? playerVisibleTime : otherPlayerVisibleTime;
				float currentVisibleWeight = (*playerVisibility.rbegin()).first > minimumVisibleWeight ? 
					(*playerVisibility.rbegin()).first : minimumVisibleWeight;
				for (float visibleWeight = 0.f; visibleWeight < totalVisibleWeight; visibleWeight += currentWeight)
				{
					VisibilityData visibility;
					//lets check that we are in the visibile time frame
					if (currentVisibleWeight + visibleWeight + currentWeight > playerVisibleTime)
					{
						visibility.moveDistance = Length(
							otherCurrentNode->GetPosition() - currentNode->GetPosition()) * currentWeight;
						visibility.moveHeight =
							(currentNode->GetPosition()[AXIS_Y] - otherCurrentNode->GetPosition()[AXIS_Y]) * currentWeight;
						visibility.moveTime = currentWeight;
					}
					playerVisibility[currentVisibleWeight + visibleWeight] = visibility;
				}
			}

			if (otherVisibilityIt != otherPlayerVisibility.end())
			{
				float minimumVisibleWeight = playerVisibleTime < otherPlayerVisibleTime ? playerVisibleTime : otherPlayerVisibleTime;
				float otherCurrentVisibleWeight = (*otherPlayerVisibility.rbegin()).first > minimumVisibleWeight ?
					(*otherPlayerVisibility.rbegin()).first : minimumVisibleWeight;
				for (float otherVisibleWeight = 0.f; otherVisibleWeight < totalVisibleWeight; otherVisibleWeight += currentWeight)
				{
					VisibilityData otherVisibility;
					//lets check that we are in the visibile time frame
					if (otherCurrentVisibleWeight + otherVisibleWeight + currentWeight > otherPlayerVisibleTime)
					{
						otherVisibility.moveDistance = Length(
							otherCurrentNode->GetPosition() - currentNode->GetPosition()) * currentWeight;
						otherVisibility.moveHeight =
							(otherCurrentNode->GetPosition()[AXIS_Y] - currentNode->GetPosition()[AXIS_Y]) * currentWeight;
						otherVisibility.moveTime = currentWeight;
					}
					otherPlayerVisibility[otherCurrentVisibleWeight + otherVisibleWeight] = otherVisibility;
				}
			}
		}
	}
}

float QuakeAIManager::CalculatePathWeight(const PlayerData& playerData)
{
	float closestWeight = 0.f;
	if (playerData.plan.path.size())
	{
		std::shared_ptr<PlayerActor> playerActor =
			std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerData.player).lock());

		std::shared_ptr<PhysicComponent> pPhysicComponent(
			playerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		Vector3<float> currentPosition = pPhysicComponent->GetTransform().GetTranslation();

		PathingArc* currentArc = *playerData.plan.path.begin();
		auto transitionWeights = currentArc->GetTransition()->GetWeights();
		auto transitionPositions = currentArc->GetTransition()->GetPositions();

		float currentWeight = 0.f;
		float closestLength = FLT_MAX;
		for (unsigned int index = 0; index < transitionPositions.size(); index++)
		{
			currentWeight += transitionWeights[index];
			if (Length(currentPosition - transitionPositions[index]) < closestLength)
			{
				closestWeight = currentWeight;
				closestLength = Length(currentPosition - transitionPositions[index]);
			}
		}
	}
	return closestWeight;
}

Vector3<float> QuakeAIManager::CalculatePathPosition(const PlayerData& playerData)
{
	float pathingWeight = 0.f;
	PathingNode* pathingNode = playerData.plan.node;
	for (auto& pathingArc : playerData.plan.path)
	{
		if (pathingWeight + pathingArc->GetWeight() >= playerData.planWeight)
		{
			const std::vector<float>& frameWeights = pathingArc->GetTransition()->GetWeights();
			const std::vector<Vector3<float>>& framePositions = pathingArc->GetTransition()->GetPositions();

			unsigned short frameIdx = 0;
			for (; frameIdx < frameWeights.size(); frameIdx++)
			{
				pathingWeight += frameWeights[frameIdx];
				if (pathingWeight >= playerData.planWeight)
					break;
			}

			return framePositions[frameIdx];
		}

		pathingWeight += pathingArc->GetWeight();
		pathingNode = pathingArc->GetNode();
	}
	return pathingNode->GetPosition();
}

void QuakeAIManager::PerformGuessingMaking(
	const AIAnalysis::GameEvaluation& gameEvaluation, const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	bool isConservativeGuessing = CalculatePlayerStatus(playerDataIn) > 0.3f ? false : true;
	if (!isConservativeGuessing)
	{
		float playerWeaponStatus = CalculatePlayerWeaponStatus(playerDataIn);
		float otherPlayerWeaponStatus = CalculatePlayerWeaponStatus(otherPlayerDataIn);
		//if the opponent has top tier weapon and we dont then we run conservative
		if (playerWeaponStatus < 0.6f && otherPlayerWeaponStatus >= 0.6f)
			isConservativeGuessing = true;
	}

	if (!isConservativeGuessing)
	{
		std::unordered_map<unsigned long long, float> playerGuessingHeuristics, otherPlayerGuessingHeuristics;
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				playerGuessingHeuristics[simulation->playerSimulation.code] = 0;
				break;
			}
		}

		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				otherPlayerGuessingHeuristics[simulation->otherPlayerSimulation.code] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				playerGuessingHeuristics[simulation->playerSimulation.code] += simulation->playerSimulation.heuristic;
				otherPlayerGuessingHeuristics[simulation->otherPlayerSimulation.code] += simulation->otherPlayerSimulation.heuristic;
			}
		}

		float playerClusterHeuristic = -FLT_MAX;
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			float playerGuessingHeuristicAvg = playerGuessingHeuristic.second / (float)otherPlayerGuessingHeuristics.size();
			if (playerGuessingHeuristic.first == ULLONG_MAX)
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerGuessingHeuristicAvg - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = playerGuessingHeuristic.first;
					playerClusterHeuristic = playerGuessingHeuristicAvg;
				}
				//lets take the value which maximize players heuristic
				else if (playerGuessingHeuristicAvg > playerClusterHeuristic)
				{
					playerClusterCode = playerGuessingHeuristic.first;
					playerClusterHeuristic = playerGuessingHeuristicAvg;
				}
			}
			else if (playerGuessingHeuristicAvg > playerClusterHeuristic)
			{
				playerClusterCode = playerGuessingHeuristic.first;
				playerClusterHeuristic = playerGuessingHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> playerGuessingWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			for (auto const& simulation : gameSimulation->simulations)
				playerGuessingWeapons[simulation->playerSimulation.code].push_back(simulation->playerSimulation.weapon);

		unsigned int playerWeaponCount = 0;
		std::map<WeaponType, unsigned int> playerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerWeapons[(WeaponType)weapon] = 0;
		for (auto& playerGuessingWeapon : playerGuessingWeapons[playerClusterCode])
			playerWeapons[playerGuessingWeapon]++;
		for (auto& playerWeaponInstance : playerWeapons)
		{
			if (playerWeaponInstance.second > playerWeaponCount)
			{
				playerWeapon = playerWeaponInstance.first;
				playerWeaponCount =playerWeaponInstance.second;
			}
		}

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			float otherPlayerGuessingHeuristicAvg = otherPlayerGuessingHeuristic.second / (float)playerGuessingHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerGuessingHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerGuessingHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerGuessingWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			for (auto const& simulation : gameSimulation->simulations)
				otherPlayerGuessingWeapons[simulation->otherPlayerSimulation.code].push_back(simulation->otherPlayerSimulation.weapon);

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerGuessingWeapon : otherPlayerGuessingWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerGuessingWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best playerCluster simulations
		if (playerClusterCode != ULLONG_MAX)
		{
			playerGuessingHeuristics.clear();
			unsigned short playerCluster = clusterPathings.at(playerClusterCode).second->GetTarget()->GetCluster();
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code != ULLONG_MAX &&
						clusterPathings.at(simulation->playerSimulation.code).second->GetTarget()->GetCluster() == playerCluster)
					{
						playerGuessingHeuristics[simulation->playerSimulation.code] = FLT_MAX;
					}
					break;
				}
			}

			//calculate each average and take the best outcome for both players
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code != ULLONG_MAX &&
						clusterPathings.at(simulation->playerSimulation.code).second->GetTarget()->GetCluster() == playerCluster)
					{
						if (playerGuessingHeuristics[simulation->playerSimulation.code] > simulation->playerSimulation.heuristic)
							playerGuessingHeuristics[simulation->playerSimulation.code] = simulation->playerSimulation.heuristic;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
			{
				if (playerGuessingHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerGuessingHeuristic.first;
						playerClusterMoveHeuristic = playerGuessingHeuristic.second;
					}
				}
				else if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerGuessingHeuristic.first;
						playerClusterJumpHeuristic = playerGuessingHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			playerClusterHeuristic = -FLT_MAX;
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerGuessingHeuristics.find(ULLONG_MAX) != playerGuessingHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
				}
			}
		}
		else
		{
			playerGuessingHeuristics.clear();
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code == playerClusterCode)
						playerGuessingHeuristics[simulation->playerSimulation.code] = FLT_MAX;
					break;
				}
			}

			//calculate each average and take the best outcome for both players
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code == playerClusterCode)
					{
						if (playerGuessingHeuristics[simulation->playerSimulation.code] > simulation->playerSimulation.heuristic)
							playerGuessingHeuristics[simulation->playerSimulation.code] = simulation->playerSimulation.heuristic;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
			{
				if (playerGuessingHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerGuessingHeuristic.first;
						playerClusterMoveHeuristic = playerGuessingHeuristic.second;
					}
				}
				else if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerGuessingHeuristic.first;
						playerClusterJumpHeuristic = playerGuessingHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			playerClusterHeuristic = -FLT_MAX;
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerGuessingHeuristics.find(ULLONG_MAX) != playerGuessingHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
				}
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerGuessingHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerGuessingHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerGuessingHeuristics.clear();
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerGuessingHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
	}
	else
	{
		//conservative decision making
		std::unordered_map<unsigned long long, float> playerGuessingHeuristics, otherPlayerGuessingHeuristics;
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				playerGuessingHeuristics[simulation->playerSimulation.code] = FLT_MAX;
				break;
			}
		}

		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				otherPlayerGuessingHeuristics[simulation->otherPlayerSimulation.code] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		std::unordered_map<unsigned long long, WeaponType> playerGuessingWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				if (playerGuessingHeuristics[simulation->playerSimulation.code] > simulation->playerSimulation.heuristic)
				{
					playerGuessingWeapons[simulation->playerSimulation.code] = simulation->playerSimulation.weapon;
					playerGuessingHeuristics[simulation->playerSimulation.code] = simulation->playerSimulation.heuristic;
				}
				otherPlayerGuessingHeuristics[simulation->otherPlayerSimulation.code] += simulation->otherPlayerSimulation.heuristic;
			}
		}

		float playerClusterJumpHeuristic = -FLT_MAX;
		float playerClusterMoveHeuristic = -FLT_MAX;
		unsigned long long playerClusterJumpCode = ULLONG_MAX;
		unsigned long long playerClusterMoveCode = ULLONG_MAX;
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			if (playerGuessingHeuristic.first == ULLONG_MAX)
				continue;

			if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_MOVE)
			{
				//lets take the value which maximize players heuristic
				if (playerGuessingHeuristic.second > playerClusterMoveHeuristic)
				{
					playerClusterMoveCode = playerGuessingHeuristic.first;
					playerClusterMoveHeuristic = playerGuessingHeuristic.second;
				}
			}
			else if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_JUMP)
			{
				//lets take the value which maximize players heuristic
				if (playerGuessingHeuristic.second > playerClusterJumpHeuristic)
				{
					playerClusterJumpCode = playerGuessingHeuristic.first;
					playerClusterJumpHeuristic = playerGuessingHeuristic.second;
				}
			}
		}

		//jumping decision takes less priority
		float playerClusterHeuristic = -FLT_MAX;
		if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
		{
			playerClusterCode = playerClusterJumpCode;
			playerClusterHeuristic = playerClusterJumpHeuristic;
		}
		else
		{
			playerClusterCode = playerClusterMoveCode;
			playerClusterHeuristic = playerClusterMoveHeuristic;
		}

		if (playerGuessingHeuristics.find(ULLONG_MAX) != playerGuessingHeuristics.end())
		{
			//we keep the current plan if the heuristic is close to the best player heuristic
			if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
			{
				playerClusterCode = ULLONG_MAX;
				playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
			}
		}
		playerWeapon = playerGuessingWeapons[playerClusterCode];

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			float otherPlayerGuessingHeuristicAvg = otherPlayerGuessingHeuristic.second / (float)playerGuessingHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerGuessingHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerGuessingHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerGuessingWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			for (auto const& simulation : gameSimulation->simulations)
				otherPlayerGuessingWeapons[simulation->otherPlayerSimulation.code].push_back(simulation->otherPlayerSimulation.weapon);

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerGuessingWeapon : otherPlayerGuessingWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerGuessingWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerGuessingHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerGuessingHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerGuessingHeuristics.clear();
			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerGuessings)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerGuessingHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
	}
}

void QuakeAIManager::PerformDecisionMaking(const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, float>>& playerDecisions,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>>& playerWeaponDecisions,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	bool isConservativeDecision = CalculatePlayerStatus(playerDataIn) > 0.3f ? false : true;
	if (!isConservativeDecision)
	{
		float playerWeaponStatus = CalculatePlayerWeaponStatus(playerDataIn);
		float otherPlayerWeaponStatus = CalculatePlayerWeaponStatus(otherPlayerDataIn);
		//if the opponent has top tier weapon and we dont then we run conservative
		if (playerWeaponStatus < 0.6f && otherPlayerWeaponStatus >= 0.6f)
			isConservativeDecision = true;
	}

	if (!isConservativeDecision)
	{
		std::unordered_map<unsigned long long, float> playerDecisionHeuristics, otherPlayerDecisionHeuristics;
		for (auto& playerDecisionSimulation : playerDecisions)
		{
			//cluster code
			unsigned long long clusterCode = playerDecisionSimulation.first;
			playerDecisionHeuristics[clusterCode] = 0;
		}

		for (auto& playerDecisionSimulation : playerDecisions)
		{
			for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
				otherPlayerDecisionHeuristics[otherClusterCode] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		for (auto& playerDecisionSimulation : playerDecisions)
		{
			//cluster code
			unsigned long long clusterCode = playerDecisionSimulation.first;

			for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
				playerDecisionHeuristics[clusterCode] += otherPlayerDecisionSimulation.second;
				otherPlayerDecisionHeuristics[otherClusterCode] += otherPlayerDecisionSimulation.second;
			}
		}

		float playerClusterHeuristic = -FLT_MAX;
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			float playerDecisionHeuristicAvg = playerDecisionHeuristic.second / (float)otherPlayerDecisionHeuristics.size();
			if (playerDecisionHeuristic.first == ULLONG_MAX)
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerDecisionHeuristicAvg - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = playerDecisionHeuristic.first;
					playerClusterHeuristic = playerDecisionHeuristicAvg;
				}
				//lets take the value which maximize players heuristic
				else if (playerDecisionHeuristicAvg > playerClusterHeuristic)
				{
					playerClusterCode = playerDecisionHeuristic.first;
					playerClusterHeuristic = playerDecisionHeuristicAvg;
				}
			}
			else if (playerDecisionHeuristicAvg > playerClusterHeuristic)
			{
				playerClusterCode = playerDecisionHeuristic.first;
				playerClusterHeuristic = playerDecisionHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> playerDecisionWeapons;
		for (auto& playerWeaponDecision : playerWeaponDecisions)
		{
			//cluster code
			unsigned long long clusterCode = playerWeaponDecision.first;
			for (auto& otherPlayerWeaponDecision : playerWeaponDecision.second)
				playerDecisionWeapons[clusterCode].push_back((WeaponType)((otherPlayerWeaponDecision.second >> 8) & 0xff));
		}

		unsigned int playerWeaponCount = 0;
		std::map<WeaponType, unsigned int> playerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerWeapons[(WeaponType)weapon] = 0;
		for (auto& playerDecisionWeapon : playerDecisionWeapons[playerClusterCode])
			playerWeapons[playerDecisionWeapon]++;
		for (auto& playerWeaponInstance : playerWeapons)
		{
			if (playerWeaponInstance.second > playerWeaponCount)
			{
				playerWeapon = playerWeaponInstance.first;
				playerWeaponCount = playerWeaponInstance.second;
			}
		}

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			float otherPlayerDecisionHeuristicAvg = otherPlayerDecisionHeuristic.second / (float)playerDecisionHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerDecisionHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerDecisionHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerDecisionWeapons;
		for (auto& playerWeaponDecision : playerWeaponDecisions)
		{
			for (auto& otherPlayerWeaponDecision : playerWeaponDecision.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerWeaponDecision.first;
				otherPlayerDecisionWeapons[otherClusterCode].push_back((WeaponType)(otherPlayerWeaponDecision.second & 0xff));
			}
		}

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerDecisionWeapon : otherPlayerDecisionWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerDecisionWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best playerCluster simulations
		if (playerClusterCode != ULLONG_MAX)
		{
			playerDecisionHeuristics.clear();
			unsigned short playerCluster = clusterPathings.at(playerClusterCode).second->GetTarget()->GetCluster();
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;
				if (clusterCode != ULLONG_MAX && clusterPathings.at(clusterCode).second->GetTarget()->GetCluster() == playerCluster)
					playerDecisionHeuristics[clusterCode] = FLT_MAX;
			}

			//calculate each average and take the best outcome for both players
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;

				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (clusterCode != ULLONG_MAX && clusterPathings.at(clusterCode).second->GetTarget()->GetCluster() == playerCluster)
					{
						if (playerDecisionHeuristics[clusterCode] > otherPlayerDecisionSimulation.second)
							playerDecisionHeuristics[clusterCode] = otherPlayerDecisionSimulation.second;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
			{
				if (playerDecisionHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerDecisionHeuristic.first;
						playerClusterMoveHeuristic = playerDecisionHeuristic.second;
					}
				}
				else if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerDecisionHeuristic.first;
						playerClusterJumpHeuristic = playerDecisionHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			playerClusterHeuristic = -FLT_MAX;
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerDecisionHeuristics.find(ULLONG_MAX) != playerDecisionHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
				}
			}
		}
		else
		{
			playerDecisionHeuristics.clear();
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;
				if (clusterCode == playerClusterCode)
					playerDecisionHeuristics[clusterCode] = FLT_MAX;
			}

			//calculate each average and take the best outcome for both players
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;
				if (clusterCode == playerClusterCode)
				{
					for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
					{
						//other cluster code
						unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
						if (playerDecisionHeuristics[clusterCode] > otherPlayerDecisionSimulation.second)
							playerDecisionHeuristics[clusterCode] = otherPlayerDecisionSimulation.second;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
			{
				if (playerDecisionHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerDecisionHeuristic.first;
						playerClusterMoveHeuristic = playerDecisionHeuristic.second;
					}
				}
				else if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerDecisionHeuristic.first;
						playerClusterJumpHeuristic = playerDecisionHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			playerClusterHeuristic = -FLT_MAX;
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerDecisionHeuristics.find(ULLONG_MAX) != playerDecisionHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
				}
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerDecisionHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;

				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < otherPlayerDecisionSimulation.second)
							otherPlayerDecisionHeuristics[otherClusterCode] = otherPlayerDecisionSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerDecisionHeuristics.clear();
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;

				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < otherPlayerDecisionSimulation.second)
							otherPlayerDecisionHeuristics[otherClusterCode] = otherPlayerDecisionSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
	}
	else
	{
		//conservative decision making
		std::unordered_map<unsigned long long, float> playerDecisionHeuristics, otherPlayerDecisionHeuristics;
		for (auto& playerDecisionSimulation : playerDecisions)
		{
			//cluster code
			unsigned long long clusterCode = playerDecisionSimulation.first;
			playerDecisionHeuristics[clusterCode] = FLT_MAX;
		}

		for (auto& playerDecisionSimulation : playerDecisions)
		{
			for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
				otherPlayerDecisionHeuristics[otherClusterCode] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		std::unordered_map<unsigned long long, WeaponType> playerDecisionWeapons;
		for (auto& playerDecisionSimulation : playerDecisions)
		{
			//cluster code
			unsigned long long clusterCode = playerDecisionSimulation.first;

			for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;

				if (playerDecisionHeuristics[clusterCode] > otherPlayerDecisionSimulation.second)
				{
					playerDecisionWeapons[clusterCode] = 
						(WeaponType)((playerWeaponDecisions.at(clusterCode).at(otherClusterCode) >> 8) & 0xff);
					playerDecisionHeuristics[clusterCode] = otherPlayerDecisionSimulation.second;
				}
				otherPlayerDecisionHeuristics[otherClusterCode] += otherPlayerDecisionSimulation.second;
			}
		}

		float playerClusterJumpHeuristic = -FLT_MAX;
		float playerClusterMoveHeuristic = -FLT_MAX;
		unsigned long long playerClusterJumpCode = ULLONG_MAX;
		unsigned long long playerClusterMoveCode = ULLONG_MAX;
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			if (playerDecisionHeuristic.first == ULLONG_MAX)
				continue;

			if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_MOVE)
			{
				//lets take the value which maximize players heuristic
				if (playerDecisionHeuristic.second > playerClusterMoveHeuristic)
				{
					playerClusterMoveCode = playerDecisionHeuristic.first;
					playerClusterMoveHeuristic = playerDecisionHeuristic.second;
				}
			}
			else if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_JUMP)
			{
				//lets take the value which maximize players heuristic
				if (playerDecisionHeuristic.second > playerClusterJumpHeuristic)
				{
					playerClusterJumpCode = playerDecisionHeuristic.first;
					playerClusterJumpHeuristic = playerDecisionHeuristic.second;
				}
			}
		}

		//jumping decision takes less priority
		float playerClusterHeuristic = -FLT_MAX;
		if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
		{
			playerClusterCode = playerClusterJumpCode;
			playerClusterHeuristic = playerClusterJumpHeuristic;
		}
		else
		{
			playerClusterCode = playerClusterMoveCode;
			playerClusterHeuristic = playerClusterMoveHeuristic;
		}

		if (playerDecisionHeuristics.find(ULLONG_MAX) != playerDecisionHeuristics.end())
		{
			//we keep the current plan if the heuristic is close to the best player heuristic
			if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
			{
				playerClusterCode = ULLONG_MAX;
				playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
			}
		}
		playerWeapon = playerDecisionWeapons[playerClusterCode];

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			float otherPlayerDecisionHeuristicAvg = otherPlayerDecisionHeuristic.second / (float)playerDecisionHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerDecisionHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerDecisionHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerDecisionWeapons;
		for (auto& playerWeaponDecision : playerWeaponDecisions)
		{
			for (auto& otherPlayerWeaponDecision : playerWeaponDecision.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerWeaponDecision.first;
				otherPlayerDecisionWeapons[otherClusterCode].push_back((WeaponType)(otherPlayerWeaponDecision.second & 0xff));
			}
		}

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerDecisionWeapon : otherPlayerDecisionWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerDecisionWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerDecisionHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;

				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < otherPlayerDecisionSimulation.second)
							otherPlayerDecisionHeuristics[otherClusterCode] = otherPlayerDecisionSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerDecisionHeuristics.clear();
			for (auto& playerDecisionSimulation : playerDecisions)
			{
				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerDecisionSimulation : playerDecisions)
			{
				//cluster code
				unsigned long long clusterCode = playerDecisionSimulation.first;

				for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < otherPlayerDecisionSimulation.second)
							otherPlayerDecisionHeuristics[otherClusterCode] = otherPlayerDecisionSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
	}
}

void QuakeAIManager::PerformGuessingMaking(const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, float>>& playerGuessings,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>>& playerWeaponGuessings,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	bool isConservativeGuessing = CalculatePlayerStatus(playerDataIn) > 0.3f ? false : true;
	if (!isConservativeGuessing)
	{
		float playerWeaponStatus = CalculatePlayerWeaponStatus(playerDataIn);
		float otherPlayerWeaponStatus = CalculatePlayerWeaponStatus(otherPlayerDataIn);
		//if the opponent has top tier weapon and we dont then we run conservative
		if (playerWeaponStatus < 0.6f && otherPlayerWeaponStatus >= 0.6f)
			isConservativeGuessing = true;
	}

	if (!isConservativeGuessing)
	{
		std::unordered_map<unsigned long long, float> playerGuessingHeuristics, otherPlayerGuessingHeuristics;
		for (auto& playerGuessingSimulation : playerGuessings)
		{
			//cluster code
			unsigned long long clusterCode = playerGuessingSimulation.first;
			playerGuessingHeuristics[clusterCode] = 0;
		}

		for (auto& playerGuessingSimulation : playerGuessings)
		{
			for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
				otherPlayerGuessingHeuristics[otherClusterCode] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		for (auto& playerGuessingSimulation : playerGuessings)
		{
			//cluster code
			unsigned long long clusterCode = playerGuessingSimulation.first;

			for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
				playerGuessingHeuristics[clusterCode] += otherPlayerGuessingSimulation.second;
				otherPlayerGuessingHeuristics[otherClusterCode] += otherPlayerGuessingSimulation.second;
			}
		}

		float playerClusterHeuristic = -FLT_MAX;
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			float playerGuessingHeuristicAvg = playerGuessingHeuristic.second / (float)otherPlayerGuessingHeuristics.size();
			if (playerGuessingHeuristic.first == ULLONG_MAX)
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerGuessingHeuristicAvg - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = playerGuessingHeuristic.first;
					playerClusterHeuristic = playerGuessingHeuristicAvg;
				}
				//lets take the value which maximize players heuristic
				else if (playerGuessingHeuristicAvg > playerClusterHeuristic)
				{
					playerClusterCode = playerGuessingHeuristic.first;
					playerClusterHeuristic = playerGuessingHeuristicAvg;
				}
			}
			else if (playerGuessingHeuristicAvg > playerClusterHeuristic)
			{
				playerClusterCode = playerGuessingHeuristic.first;
				playerClusterHeuristic = playerGuessingHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> playerGuessingWeapons;
		for (auto& playerWeaponGuessing : playerWeaponGuessings)
		{
			//cluster code
			unsigned long long clusterCode = playerWeaponGuessing.first;
			for (auto& otherPlayerWeaponGuessing : playerWeaponGuessing.second)
				playerGuessingWeapons[clusterCode].push_back((WeaponType)((otherPlayerWeaponGuessing.second >> 8) & 0xff));
		}

		unsigned int playerWeaponCount = 0;
		std::map<WeaponType, unsigned int> playerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerWeapons[(WeaponType)weapon] = 0;
		for (auto& playerGuessingWeapon : playerGuessingWeapons[playerClusterCode])
			playerWeapons[playerGuessingWeapon]++;
		for (auto& playerWeaponInstance : playerWeapons)
		{
			if (playerWeaponInstance.second > playerWeaponCount)
			{
				playerWeapon = playerWeaponInstance.first;
				playerWeaponCount = playerWeaponInstance.second;
			}
		}

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			float otherPlayerGuessingHeuristicAvg = otherPlayerGuessingHeuristic.second / (float)playerGuessingHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerGuessingHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerGuessingHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerGuessingWeapons;
		for (auto& playerWeaponGuessing : playerWeaponGuessings)
		{
			for (auto& otherPlayerWeaponGuessing : playerWeaponGuessing.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerWeaponGuessing.first;
				otherPlayerGuessingWeapons[otherClusterCode].push_back((WeaponType)(otherPlayerWeaponGuessing.second & 0xff));
			}
		}

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerGuessingWeapon : otherPlayerGuessingWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerGuessingWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best playerCluster simulations
		if (playerClusterCode != ULLONG_MAX)
		{
			playerGuessingHeuristics.clear();
			unsigned short playerCluster = clusterPathings.at(playerClusterCode).second->GetTarget()->GetCluster();
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;
				if (clusterCode != ULLONG_MAX && clusterPathings.at(clusterCode).second->GetTarget()->GetCluster() == playerCluster)
					playerGuessingHeuristics[clusterCode] = FLT_MAX;
			}

			//calculate each average and take the best outcome for both players
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;

				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (clusterCode != ULLONG_MAX && clusterPathings.at(clusterCode).second->GetTarget()->GetCluster() == playerCluster)
					{
						if (playerGuessingHeuristics[clusterCode] > otherPlayerGuessingSimulation.second)
							playerGuessingHeuristics[clusterCode] = otherPlayerGuessingSimulation.second;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
			{
				if (playerGuessingHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerGuessingHeuristic.first;
						playerClusterMoveHeuristic = playerGuessingHeuristic.second;
					}
				}
				else if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerGuessingHeuristic.first;
						playerClusterJumpHeuristic = playerGuessingHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			playerClusterHeuristic = -FLT_MAX;
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerGuessingHeuristics.find(ULLONG_MAX) != playerGuessingHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
				}
			}
		}
		else
		{
			playerGuessingHeuristics.clear();
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;
				if (clusterCode == playerClusterCode)
					playerGuessingHeuristics[clusterCode] = FLT_MAX;
			}

			//calculate each average and take the best outcome for both players
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;
				if (clusterCode == playerClusterCode)
				{
					for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
					{
						//other cluster code
						unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
						if (playerGuessingHeuristics[clusterCode] > otherPlayerGuessingSimulation.second)
							playerGuessingHeuristics[clusterCode] = otherPlayerGuessingSimulation.second;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
			{
				if (playerGuessingHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerGuessingHeuristic.first;
						playerClusterMoveHeuristic = playerGuessingHeuristic.second;
					}
				}
				else if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerGuessingHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerGuessingHeuristic.first;
						playerClusterJumpHeuristic = playerGuessingHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			playerClusterHeuristic = -FLT_MAX;
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerGuessingHeuristics.find(ULLONG_MAX) != playerGuessingHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
				}
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerGuessingHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;

				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < otherPlayerGuessingSimulation.second)
							otherPlayerGuessingHeuristics[otherClusterCode] = otherPlayerGuessingSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerGuessingHeuristics.clear();
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;

				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < otherPlayerGuessingSimulation.second)
							otherPlayerGuessingHeuristics[otherClusterCode] = otherPlayerGuessingSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
	}
	else
	{
		//conservative Guessing making
		std::unordered_map<unsigned long long, float> playerGuessingHeuristics, otherPlayerGuessingHeuristics;
		for (auto& playerGuessingSimulation : playerGuessings)
		{
			//cluster code
			unsigned long long clusterCode = playerGuessingSimulation.first;
			playerGuessingHeuristics[clusterCode] = FLT_MAX;
		}

		for (auto& playerGuessingSimulation : playerGuessings)
		{
			for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
				otherPlayerGuessingHeuristics[otherClusterCode] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		std::unordered_map<unsigned long long, WeaponType> playerGuessingWeapons;
		for (auto& playerGuessingSimulation : playerGuessings)
		{
			//cluster code
			unsigned long long clusterCode = playerGuessingSimulation.first;

			for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;

				if (playerGuessingHeuristics[clusterCode] > otherPlayerGuessingSimulation.second)
				{
					playerGuessingWeapons[clusterCode] =
						(WeaponType)((playerWeaponGuessings.at(clusterCode).at(otherClusterCode) >> 8) & 0xff);
					playerGuessingHeuristics[clusterCode] = otherPlayerGuessingSimulation.second;
				}
				otherPlayerGuessingHeuristics[otherClusterCode] += otherPlayerGuessingSimulation.second;
			}
		}

		float playerClusterJumpHeuristic = -FLT_MAX;
		float playerClusterMoveHeuristic = -FLT_MAX;
		unsigned long long playerClusterJumpCode = ULLONG_MAX;
		unsigned long long playerClusterMoveCode = ULLONG_MAX;
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			if (playerGuessingHeuristic.first == ULLONG_MAX)
				continue;

			if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_MOVE)
			{
				//lets take the value which maximize players heuristic
				if (playerGuessingHeuristic.second > playerClusterMoveHeuristic)
				{
					playerClusterMoveCode = playerGuessingHeuristic.first;
					playerClusterMoveHeuristic = playerGuessingHeuristic.second;
				}
			}
			else if (((playerGuessingHeuristic.first >> 60) & 0xff) == AT_JUMP)
			{
				//lets take the value which maximize players heuristic
				if (playerGuessingHeuristic.second > playerClusterJumpHeuristic)
				{
					playerClusterJumpCode = playerGuessingHeuristic.first;
					playerClusterJumpHeuristic = playerGuessingHeuristic.second;
				}
			}
		}

		//jumping decision takes less priority
		float playerClusterHeuristic = -FLT_MAX;
		if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
		{
			playerClusterCode = playerClusterJumpCode;
			playerClusterHeuristic = playerClusterJumpHeuristic;
		}
		else
		{
			playerClusterCode = playerClusterMoveCode;
			playerClusterHeuristic = playerClusterMoveHeuristic;
		}

		if (playerGuessingHeuristics.find(ULLONG_MAX) != playerGuessingHeuristics.end())
		{
			//we keep the current plan if the heuristic is close to the best player heuristic
			if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
			{
				playerClusterCode = ULLONG_MAX;
				playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
			}
		}
		playerWeapon = playerGuessingWeapons[playerClusterCode];

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			float otherPlayerGuessingHeuristicAvg = otherPlayerGuessingHeuristic.second / (float)playerGuessingHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerGuessingHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerGuessingHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerGuessingWeapons;
		for (auto& playerWeaponGuessing : playerWeaponGuessings)
		{
			for (auto& otherPlayerWeaponGuessing : playerWeaponGuessing.second)
			{
				//other cluster code
				unsigned long long otherClusterCode = otherPlayerWeaponGuessing.first;
				otherPlayerGuessingWeapons[otherClusterCode].push_back((WeaponType)(otherPlayerWeaponGuessing.second & 0xff));
			}
		}

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerGuessingWeapon : otherPlayerGuessingWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerGuessingWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerGuessingHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;

				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < otherPlayerGuessingSimulation.second)
							otherPlayerGuessingHeuristics[otherClusterCode] = otherPlayerGuessingSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerGuessingHeuristics.clear();
			for (auto& playerGuessingSimulation : playerGuessings)
			{
				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerGuessingHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto& playerGuessingSimulation : playerGuessings)
			{
				//cluster code
				unsigned long long clusterCode = playerGuessingSimulation.first;

				for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
				{
					//other cluster code
					unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerGuessingHeuristics[otherClusterCode] < otherPlayerGuessingSimulation.second)
							otherPlayerGuessingHeuristics[otherClusterCode] = otherPlayerGuessingSimulation.second;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
			{
				if (otherPlayerGuessingHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerGuessingHeuristic.second;
				}
			}
		}
	}
}

void QuakeAIManager::PerformDecisionMaking(
	const AIAnalysis::GameEvaluation& gameEvaluation, const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	bool isConservativeDecision = CalculatePlayerStatus(playerDataIn) > 0.3f ? false : true;
	if (!isConservativeDecision)
	{
		float playerWeaponStatus = CalculatePlayerWeaponStatus(playerDataIn);
		float otherPlayerWeaponStatus = CalculatePlayerWeaponStatus(otherPlayerDataIn);
		//if the opponent has top tier weapon and we dont then we run conservative
		if (playerWeaponStatus < 0.6f && otherPlayerWeaponStatus >= 0.6f)
			isConservativeDecision = true;
	}

	if (!isConservativeDecision)
	{
		std::unordered_map<unsigned long long, float> playerDecisionHeuristics, otherPlayerDecisionHeuristics;
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				playerDecisionHeuristics[simulation->playerSimulation.code] = 0;
				break;
			}
		}

		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				otherPlayerDecisionHeuristics[simulation->otherPlayerSimulation.code] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				playerDecisionHeuristics[simulation->playerSimulation.code] += simulation->playerSimulation.heuristic;
				otherPlayerDecisionHeuristics[simulation->otherPlayerSimulation.code] += simulation->otherPlayerSimulation.heuristic;
			}
		}

		float playerClusterHeuristic = -FLT_MAX;
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			float playerDecisionHeuristicAvg = playerDecisionHeuristic.second / (float)otherPlayerDecisionHeuristics.size();
			if (playerDecisionHeuristic.first == ULLONG_MAX)
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerDecisionHeuristicAvg - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = playerDecisionHeuristic.first;
					playerClusterHeuristic = playerDecisionHeuristicAvg;
				}
				//lets take the value which maximize players heuristic
				else if (playerDecisionHeuristicAvg > playerClusterHeuristic)
				{
					playerClusterCode = playerDecisionHeuristic.first;
					playerClusterHeuristic = playerDecisionHeuristicAvg;
				}
			}
			else if (playerDecisionHeuristicAvg > playerClusterHeuristic)
			{
				playerClusterCode = playerDecisionHeuristic.first;
				playerClusterHeuristic = playerDecisionHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> playerDecisionWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			for (auto const& simulation : gameSimulation->simulations)
				playerDecisionWeapons[simulation->playerSimulation.code].push_back(simulation->playerSimulation.weapon);

		unsigned int playerWeaponCount = 0;
		std::map<WeaponType, unsigned int> playerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerWeapons[(WeaponType)weapon] = 0;
		for (auto& playerDecisionWeapon : playerDecisionWeapons[playerClusterCode])
			playerWeapons[playerDecisionWeapon]++;
		for (auto& playerWeaponInstance : playerWeapons)
		{
			if (playerWeaponInstance.second > playerWeaponCount)
			{
				playerWeapon = playerWeaponInstance.first;
				playerWeaponCount = playerWeaponInstance.second;
			}
		}

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			float otherPlayerDecisionHeuristicAvg = otherPlayerDecisionHeuristic.second / (float)playerDecisionHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerDecisionHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerDecisionHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerDecisionWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			for (auto const& simulation : gameSimulation->simulations)
				otherPlayerDecisionWeapons[simulation->otherPlayerSimulation.code].push_back(simulation->otherPlayerSimulation.weapon);

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerDecisionWeapon : otherPlayerDecisionWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerDecisionWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best playerCluster simulations
		if (playerClusterCode != ULLONG_MAX)
		{
			playerDecisionHeuristics.clear();
			unsigned short playerCluster = clusterPathings.at(playerClusterCode).second->GetTarget()->GetCluster();
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code != ULLONG_MAX &&
						clusterPathings.at(simulation->playerSimulation.code).second->GetTarget()->GetCluster() == playerCluster)
					{
						playerDecisionHeuristics[simulation->playerSimulation.code] = FLT_MAX;
					}
					break;
				}
			}

			//calculate each average and take the best outcome for both players
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code != ULLONG_MAX &&
						clusterPathings.at(simulation->playerSimulation.code).second->GetTarget()->GetCluster() == playerCluster)
					{
						if (playerDecisionHeuristics[simulation->playerSimulation.code] > simulation->playerSimulation.heuristic)
							playerDecisionHeuristics[simulation->playerSimulation.code] = simulation->playerSimulation.heuristic;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
			{
				if (playerDecisionHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerDecisionHeuristic.first;
						playerClusterMoveHeuristic = playerDecisionHeuristic.second;
					}
				}
				else if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerDecisionHeuristic.first;
						playerClusterJumpHeuristic = playerDecisionHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerDecisionHeuristics.find(ULLONG_MAX) != playerDecisionHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
				}
			}
		}
		else
		{
			playerDecisionHeuristics.clear();
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code == playerClusterCode)
						playerDecisionHeuristics[simulation->playerSimulation.code] = FLT_MAX;
					break;
				}
			}

			//calculate each average and take the best outcome for both players
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					if (simulation->playerSimulation.code == playerClusterCode)
					{
						if (playerDecisionHeuristics[simulation->playerSimulation.code] > simulation->playerSimulation.heuristic)
							playerDecisionHeuristics[simulation->playerSimulation.code] = simulation->playerSimulation.heuristic;
					}
				}
			}

			float playerClusterJumpHeuristic = -FLT_MAX;
			float playerClusterMoveHeuristic = -FLT_MAX;
			unsigned long long playerClusterJumpCode = ULLONG_MAX;
			unsigned long long playerClusterMoveCode = ULLONG_MAX;
			for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
			{
				if (playerDecisionHeuristic.first == ULLONG_MAX)
					continue;

				if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_MOVE)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterMoveHeuristic)
					{
						playerClusterMoveCode = playerDecisionHeuristic.first;
						playerClusterMoveHeuristic = playerDecisionHeuristic.second;
					}
				}
				else if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_JUMP)
				{
					//lets take the value which maximize players heuristic
					if (playerDecisionHeuristic.second > playerClusterJumpHeuristic)
					{
						playerClusterJumpCode = playerDecisionHeuristic.first;
						playerClusterJumpHeuristic = playerDecisionHeuristic.second;
					}
				}
			}

			//jumping decision takes less priority
			if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
			{
				playerClusterCode = playerClusterJumpCode;
				playerClusterHeuristic = playerClusterJumpHeuristic;
			}
			else
			{
				playerClusterCode = playerClusterMoveCode;
				playerClusterHeuristic = playerClusterMoveHeuristic;
			}

			if (playerDecisionHeuristics.find(ULLONG_MAX) != playerDecisionHeuristics.end())
			{
				//we keep the current plan if the heuristic is close to the best player heuristic
				if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
				{
					playerClusterCode = ULLONG_MAX;
					playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
				}
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerDecisionHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerDecisionHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerDecisionHeuristics.clear();
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerDecisionHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
	}
	else
	{
		//conservative decision making
		std::unordered_map<unsigned long long, float> playerDecisionHeuristics, otherPlayerDecisionHeuristics;
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				playerDecisionHeuristics[simulation->playerSimulation.code] = FLT_MAX;
				break;
			}
		}

		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				otherPlayerDecisionHeuristics[simulation->otherPlayerSimulation.code] = 0;
			}
			break;
		}

		//calculate each average and take the best outcome for both players
		std::unordered_map<unsigned long long, WeaponType> playerDecisionWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
		{
			for (auto const& simulation : gameSimulation->simulations)
			{
				if (playerDecisionHeuristics[simulation->playerSimulation.code] > simulation->playerSimulation.heuristic)
				{
					playerDecisionWeapons[simulation->playerSimulation.code] = simulation->playerSimulation.weapon;
					playerDecisionHeuristics[simulation->playerSimulation.code] = simulation->playerSimulation.heuristic;
				}
				otherPlayerDecisionHeuristics[simulation->otherPlayerSimulation.code] += simulation->otherPlayerSimulation.heuristic;
			}
		}

		float playerClusterJumpHeuristic = -FLT_MAX;
		float playerClusterMoveHeuristic = -FLT_MAX;
		unsigned long long playerClusterJumpCode = ULLONG_MAX;
		unsigned long long playerClusterMoveCode = ULLONG_MAX;
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			if (playerDecisionHeuristic.first == ULLONG_MAX)
				continue;

			if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_MOVE)
			{
				//lets take the value which maximize players heuristic
				if (playerDecisionHeuristic.second > playerClusterMoveHeuristic)
				{
					playerClusterMoveCode = playerDecisionHeuristic.first;
					playerClusterMoveHeuristic = playerDecisionHeuristic.second;
				}
			}
			else if (((playerDecisionHeuristic.first >> 60) & 0xff) == AT_JUMP)
			{
				//lets take the value which maximize players heuristic
				if (playerDecisionHeuristic.second > playerClusterJumpHeuristic)
				{
					playerClusterJumpCode = playerDecisionHeuristic.first;
					playerClusterJumpHeuristic = playerDecisionHeuristic.second;
				}
			}
		}

		//jumping decision takes less priority
		float playerClusterHeuristic = -FLT_MAX;
		if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.02f)
		{
			playerClusterCode = playerClusterJumpCode;
			playerClusterHeuristic = playerClusterJumpHeuristic;
		}
		else
		{
			playerClusterCode = playerClusterMoveCode;
			playerClusterHeuristic = playerClusterMoveHeuristic;
		}

		if (playerDecisionHeuristics.find(ULLONG_MAX) != playerDecisionHeuristics.end())
		{
			//we keep the current plan if the heuristic is close to the best player heuristic
			if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.02f)
			{
				playerClusterCode = ULLONG_MAX;
				playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
			}
		}
		playerWeapon = playerDecisionWeapons[playerClusterCode];

		float otherPlayerClusterHeuristic = FLT_MAX;
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			float otherPlayerDecisionHeuristicAvg = otherPlayerDecisionHeuristic.second / (float)playerDecisionHeuristics.size();
			//lets take the value which minimize players heuristic
			if (otherPlayerDecisionHeuristicAvg < otherPlayerClusterHeuristic)
			{
				otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
				otherPlayerClusterHeuristic = otherPlayerDecisionHeuristicAvg;
			}
		}

		std::unordered_map<unsigned long long, std::vector<WeaponType>> otherPlayerDecisionWeapons;
		for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			for (auto const& simulation : gameSimulation->simulations)
				otherPlayerDecisionWeapons[simulation->otherPlayerSimulation.code].push_back(simulation->otherPlayerSimulation.weapon);

		unsigned int otherPlayerWeaponCount = 0;
		std::map<WeaponType, unsigned int> otherPlayerWeapons;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerWeapons[(WeaponType)weapon] = 0;
		for (auto& otherPlayerDecisionWeapon : otherPlayerDecisionWeapons[otherPlayerClusterCode])
			otherPlayerWeapons[otherPlayerDecisionWeapon]++;
		for (auto& otherPlayerWeaponInstance : otherPlayerWeapons)
		{
			if (otherPlayerWeaponInstance.second > otherPlayerWeaponCount)
			{
				otherPlayerWeapon = otherPlayerWeaponInstance.first;
				otherPlayerWeaponCount = otherPlayerWeaponInstance.second;
			}
		}

		//run minimax with the best otherPlayerCluster simulations
		if (otherPlayerClusterCode != ULLONG_MAX)
		{
			otherPlayerDecisionHeuristics.clear();
			unsigned short otherPlayerCluster = otherClusterPathings.at(otherPlayerClusterCode).second->GetTarget()->GetCluster();
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode != ULLONG_MAX && otherClusterPathings.at(otherClusterCode).second->GetTarget()->GetCluster() == otherPlayerCluster)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerDecisionHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
		else
		{
			otherPlayerDecisionHeuristics.clear();
			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
						otherPlayerDecisionHeuristics[otherClusterCode] = -FLT_MAX;
				}
				break;
			}

			for (auto const& gameSimulation : gameEvaluation.playerDecisions)
			{
				for (auto const& simulation : gameSimulation->simulations)
				{
					//other cluster code
					unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
					if (otherClusterCode == otherPlayerClusterCode)
					{
						if (otherPlayerDecisionHeuristics[otherClusterCode] < simulation->otherPlayerSimulation.heuristic)
							otherPlayerDecisionHeuristics[otherClusterCode] = simulation->otherPlayerSimulation.heuristic;
					}
				}
			}

			otherPlayerClusterHeuristic = FLT_MAX;
			for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
			{
				if (otherPlayerDecisionHeuristic.second < otherPlayerClusterHeuristic)
				{
					otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
					otherPlayerClusterHeuristic = otherPlayerDecisionHeuristic.second;
				}
			}
		}
	}
}

/*
CanItemBeGrabbed
Returns false if the item can not be picked up.
Note: the logic assumes that the item respawn time is known 
*/
bool QuakeAIManager::CanItemBeGrabbed(ActorId itemId, 
	float itemTime, PlayerData& playerData, const std::map<ActorId, float>& gameItems)
{
	if (gameItems.find(itemId) == gameItems.end() || gameItems.at(itemId) > itemTime)
		return false;

	const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(itemId);
	if (itemPickup->GetType() == "Weapon")
	{
		if (playerData.ammo[itemPickup->GetCode()] >= 200)
			return false;		// can't hold any more

		return true;
	}
	else if (itemPickup->GetType() == "Ammo")
	{
		if (playerData.ammo[itemPickup->GetCode()] >= 200)
			return false;		// can't hold any more

		return true;
	}
	else if (itemPickup->GetType() == "Armor")
	{
		if (playerData.stats[STAT_ARMOR] >= playerData.stats[STAT_MAX_HEALTH] * 2)
			return false;		// can't hold any more

		return true;
	}
	else if (itemPickup->GetType() == "Health")
	{
		// small and mega healths will go over the max, otherwise
		// don't pick up if already at max
		if (itemPickup->GetAmount() == 5 || itemPickup->GetAmount() == 100)
		{
			if (playerData.stats[STAT_HEALTH] >= playerData.stats[STAT_MAX_HEALTH] * 2)
				return false;		// can't hold any more

			return true;
		}

		if (playerData.stats[STAT_HEALTH] >= playerData.stats[STAT_MAX_HEALTH])
			return false;		// can't hold any more

		return true;
	}
	return false;
}

//Note: the logic assumes that the item respawn time is known 
void QuakeAIManager::PickupItems(PlayerData& playerData, 
	const std::map<ActorId, float>& actors, const std::map<ActorId, float>& gameItems)
{
	for (auto const& actor : actors)
	{
		const AIAnalysis::ActorPickup* itemPickup = mGameActorPickups.at(actor.first);
		if (gameItems.find(actor.first) == gameItems.end() || gameItems.at(actor.first) > actor.second)
			continue;

		if (itemPickup->GetType() == "Weapon")
		{
			const AIAnalysis::WeaponActorPickup* weaponPickup = static_cast<const AIAnalysis::WeaponActorPickup*>(itemPickup);
			int ammo = playerData.ammo[weaponPickup->GetCode()] + weaponPickup->GetAmmo();
			if (ammo > 200)
			{
				//add amount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = weaponPickup->GetAmmo() - (ammo - 200);
			}
			else
			{
				//add amount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = weaponPickup->GetAmmo();
			}
		}
		else if (itemPickup->GetType() == "Ammo")
		{
			int ammo = playerData.ammo[itemPickup->GetCode()] + itemPickup->GetAmount();
			if (ammo > 200)
			{
				//add ammunt and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = itemPickup->GetAmount() - (ammo - 200);
			}
			else
			{
				//add amount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = itemPickup->GetAmount();
			}
		}
		else if (itemPickup->GetType() == "Armor")
		{
			int armor = playerData.stats[STAT_ARMOR] + itemPickup->GetAmount();
			if (armor > playerData.stats[STAT_MAX_HEALTH] * 2)
			{
				//add ammount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = itemPickup->GetAmount() - (armor - playerData.stats[STAT_MAX_HEALTH] * 2);
				if (playerData.itemAmount[actor.first] < 0)
					playerData.itemAmount[actor.first] = 0;
			}
			else
			{
				//add ammount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = itemPickup->GetAmount();
			}

		}
		else if (itemPickup->GetType() == "Health")
		{
			int max;
			if (itemPickup->GetAmount() != 5 && itemPickup->GetAmount() != 100)
				max = playerData.stats[STAT_MAX_HEALTH];
			else
				max = playerData.stats[STAT_MAX_HEALTH] * 2;

			int health = playerData.stats[STAT_HEALTH] + itemPickup->GetAmount();
			if (health > max)
			{
				//add ammount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = itemPickup->GetAmount() - (health - max);
				if (playerData.itemAmount[actor.first] < 0)
					playerData.itemAmount[actor.first] = 0;
			}
			else
			{
				//add ammount and weight
				playerData.items[actor.first] = 0.f;
				playerData.itemWeight[actor.first] = actor.second;
				playerData.itemAmount[actor.first] = itemPickup->GetAmount();
			}
		}
	}
}

void QuakeAIManager::FindPathPlans(PathingNode* pStartNode, const std::map<ActorId, 
	float>& searchItems, std::map<PathingActorVec, float>& actorsPathPlans,unsigned int pathingType)
{
	// find the best path using an A* search algorithm
	AIFinder aiFinder;
	aiFinder(pStartNode, searchItems, actorsPathPlans, pathingType);
}

PathingNode* QuakeAIManager::FindClosestNode(ActorId playerId, 
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

void QuakeAIManager::UpdatePlayerGuessPlan(std::shared_ptr<PlayerActor> playerActor, 
	const PlayerData& playerData, PlayerData& playerGuessData, PathingNode* playerNode)
{
	if (playerData.plan.node)
	{
		//we take the current player plan
		playerGuessData = PlayerData(playerActor);
		playerGuessData.plan = playerData.plan;
		if (playerData.plan.path.size())
		{
			playerGuessData.plan.ResetPathPlan({ *playerData.plan.path.begin() });
			playerGuessData.planWeight = CalculatePathWeight(playerGuessData);
		}

		playerGuessData.items = playerData.items;
		playerGuessData.itemAmount = playerData.itemAmount;
		playerGuessData.itemWeight = playerData.itemWeight;

		std::stringstream playerInfo;
		playerInfo << "\n UPDATE TO player guess plan: " << playerActor->GetId() << " ";
		PrintInfo(playerInfo.str());
		PrintPlayerData(playerGuessData);
	}
	else
	{
		playerGuessData = PlayerData(playerActor);
		playerGuessData.plan = NodePlan(playerNode, PathingArcVec());

		std::stringstream playerInfo;
		playerInfo << "\n UPDATE TO player guess node: " << playerActor->GetId() << " ";
		PrintInfo(playerInfo.str());
		PrintPlayerData(playerGuessData);
	}
}

void QuakeAIManager::UpdatePlayerState(PlayerView& playerView)
{
	//lets collect items along the way from the simulation
	PathingArcVec playerPath;
	PathingArcVec::iterator itArc = playerView.data.plan.path.begin();
	for (; itArc != playerView.data.plan.path.end(); itArc++)
	{
		if (playerView.data.planWeight < (*itArc)->GetWeight() * 0.1f)
			break;

		playerPath.push_back(*itArc);

		playerView.data.plan.node = (*itArc)->GetNode();
		playerView.data.planWeight -= (*itArc)->GetWeight();

		ActorId itemId = (*itArc)->GetNode()->GetActorId();
		if (playerView.data.items.find(itemId) != playerView.data.items.end())
		{
			if (playerView.data.items[itemId] > 0.f)
				continue;

			std::shared_ptr<Actor> pItemActor = GameLogic::Get()->GetActor(itemId).lock();
			if (pItemActor->GetType() == "Weapon")
			{
				std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

				// add the weapon
				playerView.data.stats[STAT_WEAPONS] |= (1 << pWeaponPickup->GetCode());

				// add ammo
				playerView.data.ammo[pWeaponPickup->GetCode()] += playerView.data.itemAmount[itemId];

				playerView.data.items[itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
				playerView.gameItems[itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Ammo")
			{
				std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();

				playerView.data.ammo[pAmmoPickup->GetCode()] += playerView.data.itemAmount[itemId];

				playerView.data.items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
				playerView.gameItems[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Armor")
			{
				std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();

				playerView.data.stats[STAT_ARMOR] += playerView.data.itemAmount[itemId];

				playerView.data.items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
				playerView.gameItems[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Health")
			{
				std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();

				playerView.data.stats[STAT_HEALTH] += playerView.data.itemAmount[itemId];

				playerView.data.items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
				playerView.gameItems[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
			}
		}
	}

	playerView.data.plan.path = playerPath;
}

void QuakeAIManager::UpdatePlayerGuessState(PlayerGuessView& playerGuessView, ActorId playerId)
{
	//lets collect items along the way from the simulation
	PathingArcVec guessPath;
	PathingArcVec::iterator itArc = playerGuessView.guessPlayers[playerId].plan.path.begin();
	for (; itArc != playerGuessView.guessPlayers[playerId].plan.path.end(); itArc++)
	{
		if (playerGuessView.guessPlayers[playerId].planWeight < (*itArc)->GetWeight() * 0.1f)
			break;

		guessPath.push_back(*itArc);

		playerGuessView.guessPlayers[playerId].plan.node = (*itArc)->GetNode();
		playerGuessView.guessPlayers[playerId].planWeight -= (*itArc)->GetWeight();

		ActorId itemId = (*itArc)->GetNode()->GetActorId();
		if (playerGuessView.guessPlayers[playerId].items.find(itemId) != playerGuessView.guessPlayers[playerId].items.end())
		{
			if (playerGuessView.guessPlayers[playerId].items[itemId] > 0.f)
				continue;

			std::shared_ptr<Actor> pItemActor = GameLogic::Get()->GetActor(itemId).lock();
			if (pItemActor->GetType() == "Weapon")
			{
				std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

				// add the weapon
				playerGuessView.guessPlayers[playerId].stats[STAT_WEAPONS] |= (1 << pWeaponPickup->GetCode());

				// add ammo
				playerGuessView.guessPlayers[playerId].ammo[pWeaponPickup->GetCode()] +=
					playerGuessView.guessPlayers[playerId].itemAmount[itemId];

				playerGuessView.guessPlayers[playerId].items[itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerId][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Ammo")
			{
				std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();

				playerGuessView.guessPlayers[playerId].ammo[pAmmoPickup->GetCode()] +=
					playerGuessView.guessPlayers[playerId].itemAmount[itemId];

				playerGuessView.guessPlayers[playerId].items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerId][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Armor")
			{
				std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();

				playerGuessView.guessPlayers[playerId].stats[STAT_ARMOR] +=
					playerGuessView.guessPlayers[playerId].itemAmount[itemId];

				playerGuessView.guessPlayers[playerId].items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerId][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Health")
			{
				std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();

				playerGuessView.guessPlayers[playerId].stats[STAT_HEALTH] +=
					playerGuessView.guessPlayers[playerId].itemAmount[itemId];

				playerGuessView.guessPlayers[playerId].items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerId][itemId] = (float)pHealthPickup->GetWait() / 1000.f;
			}
		}
	}

	playerGuessView.guessPlayers[playerId].plan.path = guessPath;
}

void QuakeAIManager::UpdatePlayerGuessState(PlayerGuessView& playerGuessView)
{
	//lets collect items along the way from the simulation
	PathingArcVec guessPath;
	PathingArcVec::iterator itArc = playerGuessView.data.plan.path.begin();
	for (; itArc != playerGuessView.data.plan.path.end(); itArc++)
	{
		if (playerGuessView.data.planWeight < (*itArc)->GetWeight() * 0.1f)
			break;
			
		guessPath.push_back(*itArc);

		playerGuessView.data.plan.node = (*itArc)->GetNode();
		playerGuessView.data.planWeight -= (*itArc)->GetWeight();

		ActorId itemId = (*itArc)->GetNode()->GetActorId();
		if (playerGuessView.data.items.find(itemId) != playerGuessView.data.items.end())
		{
			if (playerGuessView.data.items[itemId] > 0)
				continue;

			std::shared_ptr<Actor> pItemActor = GameLogic::Get()->GetActor(itemId).lock();
			if (pItemActor->GetType() == "Weapon")
			{
				std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

				// add the weapon
				playerGuessView.data.stats[STAT_WEAPONS] |= (1 << pWeaponPickup->GetCode());

				// add ammo
				playerGuessView.data.ammo[pWeaponPickup->GetCode()] += playerGuessView.data.itemAmount[itemId];

				playerGuessView.data.items[itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Ammo")
			{
				std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();

				playerGuessView.data.ammo[pAmmoPickup->GetCode()] += playerGuessView.data.itemAmount[itemId];

				playerGuessView.data.items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
			}
			else if (pItemActor->GetType() == "Armor")
			{
				std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();

				playerGuessView.data.stats[STAT_ARMOR] += playerGuessView.data.itemAmount[itemId];

				playerGuessView.data.items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pArmorPickup->GetWait() / 1000.f;

			}
			else if (pItemActor->GetType() == "Health")
			{
				std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();

				playerGuessView.data.stats[STAT_HEALTH] += playerGuessView.data.itemAmount[itemId];

				playerGuessView.data.items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
				playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pHealthPickup->GetWait() / 1000.f;
			}
		}
	}

	playerGuessView.data.plan.path = guessPath;
}

void QuakeAIManager::UpdatePlayerGuessState(unsigned long deltaMs, PlayerGuessView& playerGuessView, ActorId playerId)
{
	if (playerGuessView.guessPlayers[playerId].plan.path.size())
	{
		playerGuessView.guessPlayers[playerId].planWeight += deltaMs / 1000.f;

		//lets collect items along the way from the simulation
		PathingArcVec::iterator itArc = playerGuessView.guessPlayers[playerId].plan.path.begin();
		for (; itArc != playerGuessView.guessPlayers[playerId].plan.path.end(); itArc++)
		{
			if (playerGuessView.guessPlayers[playerId].planWeight < (*itArc)->GetWeight())
				break;

			playerGuessView.guessPlayers[playerId].plan.node = (*itArc)->GetNode();
			playerGuessView.guessPlayers[playerId].planWeight -= (*itArc)->GetWeight();

			ActorId itemId = (*itArc)->GetNode()->GetActorId();
			if (playerGuessView.guessPlayers[playerId].items.find(itemId) != playerGuessView.guessPlayers[playerId].items.end())
			{
				if (playerGuessView.guessPlayers[playerId].items[itemId] > 0.f)
					continue;

				std::shared_ptr<Actor> pItemActor = GameLogic::Get()->GetActor(itemId).lock();
				if (pItemActor->GetType() == "Weapon")
				{
					std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

					// add the weapon
					playerGuessView.guessPlayers[playerId].stats[STAT_WEAPONS] |= (1 << pWeaponPickup->GetCode());

					// add ammo
					playerGuessView.guessPlayers[playerId].ammo[pWeaponPickup->GetCode()] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
				else if (pItemActor->GetType() == "Ammo")
				{
					std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
					playerGuessView.guessPlayers[playerId].ammo[pAmmoPickup->GetCode()] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
				else if (pItemActor->GetType() == "Armor")
				{
					std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
					playerGuessView.guessPlayers[playerId].stats[STAT_ARMOR] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
				else if (pItemActor->GetType() == "Health")
				{
					std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
					playerGuessView.guessPlayers[playerId].stats[STAT_HEALTH] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pHealthPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
			}
		}

		PathingArcVec guessPath;
		for (; itArc != playerGuessView.guessPlayers[playerId].plan.path.end(); itArc++)
			guessPath.push_back(*itArc);
		playerGuessView.guessPlayers[playerId].plan.ResetPathPlan(guessPath);
	}
	else if (playerGuessView.guessPlayers[playerId].planWeight < 0.f)
		playerGuessView.guessPlayers[playerId].planWeight += deltaMs / 1000.f;
}

void QuakeAIManager::UpdatePlayerGuessState(unsigned long deltaMs, PlayerGuessView& playerGuessView)
{
	if (playerGuessView.data.plan.path.size())
	{
		playerGuessView.data.planWeight += deltaMs / 1000.f;

		//lets collect items along the way from the simulation
		PathingArcVec::iterator itArc = playerGuessView.data.plan.path.begin();
		for (; itArc != playerGuessView.data.plan.path.end(); itArc++)
		{
			if (playerGuessView.data.planWeight < (*itArc)->GetWeight())
				break;

			playerGuessView.data.plan.node = (*itArc)->GetNode();
			playerGuessView.data.planWeight -= (*itArc)->GetWeight();

			ActorId itemId = (*itArc)->GetNode()->GetActorId();
			if (playerGuessView.data.items.find(itemId) != playerGuessView.data.items.end())
			{
				if (playerGuessView.data.items[itemId] > 0.f)
					continue;

				std::shared_ptr<Actor> pItemActor = GameLogic::Get()->GetActor(itemId).lock();
				if (pItemActor->GetType() == "Weapon")
				{
					std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

					// add the weapon
					playerGuessView.data.stats[STAT_WEAPONS] |= (1 << pWeaponPickup->GetCode());

					// add ammo
					playerGuessView.data.ammo[pWeaponPickup->GetCode()] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
				else if (pItemActor->GetType() == "Ammo")
				{
					std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
					playerGuessView.data.ammo[pAmmoPickup->GetCode()] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
				else if (pItemActor->GetType() == "Armor")
				{
					std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
					playerGuessView.data.stats[STAT_ARMOR] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
				else if (pItemActor->GetType() == "Health")
				{
					std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
					playerGuessView.data.stats[STAT_HEALTH] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pHealthPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
			}
		}

		PathingArcVec guessPath;
		for (; itArc != playerGuessView.data.plan.path.end(); itArc++)
			guessPath.push_back(*itArc);
		playerGuessView.data.plan.ResetPathPlan(guessPath);
	}
	else if (playerGuessView.data.planWeight < 0.f)
		playerGuessView.data.planWeight += deltaMs / 1000.f;
}

bool QuakeAIManager::CheckPlayerGuessItems(PathingNode* playerNode, PlayerGuessView& playerGuessView)
{
	bool resetGuessItem = false;
	if (playerGuessView.data.items.find(playerGuessView.data.plan.node->GetActorId()) != playerGuessView.data.items.end())
	{
		if (playerGuessView.data.items[playerGuessView.data.plan.node->GetActorId()] > 0.f)
			return resetGuessItem;

		std::shared_ptr<Actor> pItemActor
		(GameLogic::Get()->GetActor(playerGuessView.data.plan.node->GetActorId()).lock());
		std::shared_ptr<TransformComponent> pItemTransform(
			pItemActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		PathingNode* itemNode = mPathingGraph->FindClosestNode(pItemTransform->GetTransform().GetTranslation());

		if (playerNode->IsVisibleNode(itemNode))
		{
			//check if the item is visible which means that the player couldn't possibly have taken it
			if (pItemActor->GetType() == "Weapon")
			{
				std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
				if (pWeaponPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
			else if (pItemActor->GetType() == "Ammo")
			{
				std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
				if (pAmmoPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
			else if (pItemActor->GetType() == "Armor")
			{
				std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
				if (pArmorPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
			else if (pItemActor->GetType() == "Health")
			{
				std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
				if (pHealthPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
		}
	}

	return resetGuessItem;
}

bool QuakeAIManager::CheckPlayerGuessItems(PathingNode* playerNode, PlayerGuessView& playerGuessView, ActorId playerId)
{
	bool resetGuessItem = false;
	if (playerGuessView.guessPlayers[playerId].items.find(playerGuessView.data.plan.node->GetActorId()) != 
		playerGuessView.guessPlayers[playerId].items.end())
	{
		if (playerGuessView.guessPlayers[playerId].items[playerGuessView.data.plan.node->GetActorId()] > 0.f)
			return resetGuessItem;

		std::shared_ptr<Actor> pItemActor(GameLogic::Get()->GetActor(
			playerGuessView.data.plan.node->GetActorId()).lock());
		std::shared_ptr<TransformComponent> pItemTransform(
			pItemActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		PathingNode* itemNode =
			mPathingGraph->FindClosestNode(pItemTransform->GetTransform().GetTranslation());

		if (playerNode->IsVisibleNode(itemNode))
		{
			//check if the item is visible which means that the player couldn't possibly have taken it
			if (pItemActor->GetType() == "Weapon")
			{
				std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
				if (pWeaponPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
			else if (pItemActor->GetType() == "Ammo")
			{
				std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
				if (pAmmoPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
			else if (pItemActor->GetType() == "Armor")
			{
				std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
				if (pArmorPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
			else if (pItemActor->GetType() == "Health")
			{
				std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
				if (pHealthPickup->mRespawnTime <= 250.f) //lets put a minimum margin time
				{
					//distrust the guessing plan and reset guess player status
					resetGuessItem = true;
				}
			}
		}
	}

	return resetGuessItem;
}

void QuakeAIManager::UpdatePlayerItems(ActorId playerId, PlayerView& playerView)
{
	//for the moment we take perfect information but the goal is to have
	//an accurate system to predict items availability and respawning time estimation
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);
	for (std::shared_ptr<Actor> pItemActor : searchActors)
	{
		if (pItemActor->GetType() == "Weapon")
		{
			std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
			playerView.gameItems[pItemActor->GetId()] = pWeaponPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
			playerView.gameItems[pItemActor->GetId()] = pAmmoPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
			playerView.gameItems[pItemActor->GetId()] = pArmorPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
			playerView.gameItems[pItemActor->GetId()] = pHealthPickup->mRespawnTime / 1000.f;
		}
	}
}

void QuakeAIManager::UpdatePlayerGuessItems(PlayerGuessView& playerGuessView)
{
	//for the moment we take perfect information but the goal is to have
	//an accurate system to predict items availability and respawning time estimation
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);
	for (std::shared_ptr<Actor> pItemActor : searchActors)
	{
		if (pItemActor->GetType() == "Weapon")
		{
			std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
			playerGuessView.items[pItemActor->GetId()] = pWeaponPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
			playerGuessView.items[pItemActor->GetId()] = pAmmoPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
			playerGuessView.items[pItemActor->GetId()] = pArmorPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
			playerGuessView.items[pItemActor->GetId()] = pHealthPickup->mRespawnTime / 1000.f;
		}
	}
}

void QuakeAIManager::UpdatePlayerGuessItems(ActorId playerId, PlayerGuessView& playerGuessView)
{
	//for the moment we take perfect information but the goal is to have
	//an accurate system to predict items availability and respawning time estimation
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);
	for (std::shared_ptr<Actor> pItemActor : searchActors)
	{
		if (pItemActor->GetType() == "Health" ||
			pItemActor->GetType() == "Weapon" ||
			pItemActor->GetType() == "Armor" ||
			pItemActor->GetType() == "Ammo")
		{
			playerGuessView.guessItems[playerId][pItemActor->GetId()] = 0.f;
		}
	}
}

void QuakeAIManager::UpdatePlayerGuessItems(unsigned long deltaMs, ActorId playerId, PlayerGuessView& playerGuessView)
{
	//for the moment we take perfect information but the goal is to have
	//an accurate system to predict items availability and respawning time estimation
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);
	for (std::shared_ptr<Actor> pItemActor : searchActors)
	{
		if (pItemActor->GetType() == "Weapon")
		{
			std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

			if (playerGuessView.data.items.find(pItemActor->GetId()) != playerGuessView.data.items.end())
			{
				if (playerGuessView.data.items[pItemActor->GetId()] <= 0)
					playerGuessView.items[pItemActor->GetId()] = pWeaponPickup->mRespawnTime / 1000.f;
				else
					playerGuessView.items[pItemActor->GetId()] -= deltaMs / 1000.f;
			}
			else playerGuessView.items[pItemActor->GetId()] = pWeaponPickup->mRespawnTime / 1000.f;
			if (playerGuessView.items[pItemActor->GetId()] < 0)
				playerGuessView.items[pItemActor->GetId()] = 0.f;

			playerGuessView.guessItems[playerId][pItemActor->GetId()] -= deltaMs / 1000.f;
			playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] -= deltaMs / 1000.f;
			if (playerGuessView.guessItems[playerId][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerId][pItemActor->GetId()] = 0.f;
			if (playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] = 0.f;
		}
		else if (pItemActor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();

			if (playerGuessView.data.items.find(pItemActor->GetId()) != playerGuessView.data.items.end())
			{
				if (playerGuessView.data.items[pItemActor->GetId()] <= 0)
					playerGuessView.items[pItemActor->GetId()] = pAmmoPickup->mRespawnTime / 1000.f;
				else
					playerGuessView.items[pItemActor->GetId()] -= deltaMs / 1000.f;
			}
			else playerGuessView.items[pItemActor->GetId()] = pAmmoPickup->mRespawnTime / 1000.f;
			if (playerGuessView.items[pItemActor->GetId()] < 0)
				playerGuessView.items[pItemActor->GetId()] = 0.f;

			playerGuessView.guessItems[playerId][pItemActor->GetId()] -= deltaMs / 1000.f;
			playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] -= deltaMs / 1000.f;
			if (playerGuessView.guessItems[playerId][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerId][pItemActor->GetId()] = 0.f;
			if (playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] = 0.f;
		}
		else if (pItemActor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();

			if (playerGuessView.data.items.find(pItemActor->GetId()) != playerGuessView.data.items.end())
			{
				if (playerGuessView.data.items[pItemActor->GetId()] <= 0)
					playerGuessView.items[pItemActor->GetId()] = pArmorPickup->mRespawnTime / 1000.f;
				else
					playerGuessView.items[pItemActor->GetId()] -= deltaMs / 1000.f;
			}
			else playerGuessView.items[pItemActor->GetId()] = pArmorPickup->mRespawnTime / 1000.f;
			if (playerGuessView.items[pItemActor->GetId()] < 0)
				playerGuessView.items[pItemActor->GetId()] = 0.f;

			playerGuessView.guessItems[playerId][pItemActor->GetId()] -= deltaMs / 1000.f;
			playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] -= deltaMs / 1000.f;
			if (playerGuessView.guessItems[playerId][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerId][pItemActor->GetId()] = 0.f;
			if (playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] = 0.f;
		}
		else if (pItemActor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();

			if (playerGuessView.data.items.find(pItemActor->GetId()) != playerGuessView.data.items.end())
			{
				if (playerGuessView.data.items[pItemActor->GetId()] <= 0)
					playerGuessView.items[pItemActor->GetId()] = pHealthPickup->mRespawnTime / 1000.f;
				else
					playerGuessView.items[pItemActor->GetId()] -= deltaMs / 1000.f;
			}
			else playerGuessView.items[pItemActor->GetId()] = pHealthPickup->mRespawnTime / 1000.f;
			if (playerGuessView.items[pItemActor->GetId()] < 0)
				playerGuessView.items[pItemActor->GetId()] = 0.f;

			playerGuessView.guessItems[playerId][pItemActor->GetId()] -= deltaMs / 1000.f;
			playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] -= deltaMs / 1000.f;
			if (playerGuessView.guessItems[playerId][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerId][pItemActor->GetId()] = 0.f;
			if (playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] < 0)
				playerGuessView.guessItems[playerGuessView.data.player][pItemActor->GetId()] = 0.f;
		}
	}
}

void QuakeAIManager::OnUpdate(unsigned long deltaMs)
{
	if (!mEnable || !mPathingGraph)
		return;

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::map<ActorId, PlayerData> gameAIViews;
	const GameViewList& gameViews = gameApp->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<QuakeAIView> pAIView = std::dynamic_pointer_cast<QuakeAIView>(*it);
		if (pAIView)
			gameAIViews[pAIView->GetActorId()] = pAIView->GetActionPlayer();
	}

	std::vector<std::shared_ptr<PlayerActor>> playerActors;
	game->GetPlayerActors(playerActors);
	for (std::shared_ptr<PlayerActor> pPlayerActor : playerActors)
	{
		if (pPlayerActor->GetState().moveType == PM_DEAD)
			continue;

		PlayerView playerView; 
		GetPlayerView(pPlayerActor->GetId(), playerView);
		playerView.data.planWeight += deltaMs / 1000.f;

		//update player items
		UpdatePlayerItems(pPlayerActor->GetId(), playerView);
		UpdatePlayerView(pPlayerActor->GetId(), playerView, playerView.data.planWeight);

		//aware decision making
		bool runDecisionMaking = false;

		std::shared_ptr<TransformComponent> pPlayerTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		for (std::shared_ptr<PlayerActor> pOtherPlayerActor : playerActors)
		{
			if (pPlayerActor->GetId() == pOtherPlayerActor->GetId())
				continue;

			if (playerView.guessViews.find(pOtherPlayerActor->GetId()) == playerView.guessViews.end())
				continue;

			PlayerGuessView& playerGuessView = playerView.guessViews[pOtherPlayerActor->GetId()];
			bool isPlayerGuessUpdated = playerGuessView.isUpdated;
			if (playerGuessView.isUpdated)
			{
				// update what the player is guessing about the otherplayer
				playerGuessView.isUpdated = false;

				//update to player guess simulation if current guess path is empty
				if (playerGuessView.data.plan.path.empty())
				//if (playerGuessView.data.plan.id != -1)
				{
					playerGuessView.data.plan = playerGuessView.simulation.plan;
					playerGuessView.data.planWeight = playerGuessView.simulation.planWeight;
					if (!playerGuessView.data.IsWeaponSelectable(playerGuessView.simulation.weapon))
						playerGuessView.data.weapon = playerGuessView.simulation.weapon;
				}

				if (playerGuessView.guessSimulations[pPlayerActor->GetId()].plan.id != -1)
				{
					//update player guess simulation
					playerGuessView.guessPlayers[pPlayerActor->GetId()].plan = playerGuessView.guessSimulations[pPlayerActor->GetId()].plan;
					playerGuessView.guessPlayers[pPlayerActor->GetId()].planWeight = playerGuessView.guessSimulations[pPlayerActor->GetId()].planWeight;
					if (!playerGuessView.guessPlayers[pPlayerActor->GetId()].IsWeaponSelectable(playerGuessView.guessSimulations[pPlayerActor->GetId()].weapon))
						playerGuessView.guessPlayers[pPlayerActor->GetId()].weapon = playerGuessView.guessSimulations[pPlayerActor->GetId()].weapon;
				}
			}

			if (pPlayerTransformComponent)
			{
				PathingNode* playerNode = mPathingGraph->FindClosestNode(pPlayerTransformComponent->GetPosition());
				if (playerNode)
				{
					bool resetGuessItem = CheckPlayerGuessItems(playerNode, playerGuessView, pPlayerActor->GetId());

					std::shared_ptr<TransformComponent> pOtherPlayerTransformComponent(
						pOtherPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
					if (pOtherPlayerTransformComponent)
					{
						PathingNode* otherPlayerNode = mPathingGraph->FindClosestNode(pOtherPlayerTransformComponent->GetPosition());
						bool resetOtherGuessItem = CheckPlayerGuessItems(otherPlayerNode, playerGuessView);

						if (playerNode->IsVisibleNode(otherPlayerNode))
							//if (RayCollisionDetection(currentPosition, pOtherPlayerTransformComponent->GetPosition()) == NULL)
						{
							//distrust the guessing plan and reset guess player
							playerGuessView.isUpdated = false;
							UpdatePlayerGuessItems(playerGuessView);
							UpdatePlayerGuessItems(pPlayerActor->GetId(), playerGuessView);
							UpdatePlayerGuessItems(pOtherPlayerActor->GetId(), playerGuessView);

							std::stringstream updatePlayer;
							updatePlayer << "\n visible nodes for both players ";
							PrintInfo(updatePlayer.str());

							UpdatePlayerGuessPlan(pOtherPlayerActor,
								gameAIViews[pOtherPlayerActor->GetId()], playerGuessView.data, otherPlayerNode);

							//what the guessing player is guessing about the player
							UpdatePlayerGuessPlan(pPlayerActor,
								gameAIViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);


							//if players can see each other, then we run aware decision making
							runDecisionMaking = true;
						}
						else
						{
							if (resetGuessItem)
							{
								//distrust the guessing plan and reset guess player 
								playerGuessView.isUpdated = false;
								UpdatePlayerGuessItems(playerGuessView);
								UpdatePlayerGuessItems(pPlayerActor->GetId(), playerGuessView);

								std::stringstream updatePlayer;
								updatePlayer << "\n reset items for player guess: " << pPlayerActor->GetId() << " ";
								PrintInfo(updatePlayer.str());
								//we update the players path plan based on current position. This is actually not right and it should be predicted
								UpdatePlayerGuessPlan(pPlayerActor,
									gameAIViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);
							}
							else if (playerNode->IsVisibleNode(playerGuessView.data.plan.node))
								//else if(RayCollisionDetection(currentPosition, playerGuessView.data.plan.node->GetPosition()) == NULL)
							{
								//distrust the guessing plan and reset guess player node
								playerGuessView.isUpdated = false;
								UpdatePlayerGuessItems(playerGuessView);
								UpdatePlayerGuessItems(pOtherPlayerActor->GetId(), playerGuessView);

								std::stringstream updatePlayer;
								updatePlayer << "\n visible node for player guess: " << pOtherPlayerActor->GetId() << " ";
								PrintInfo(updatePlayer.str());
								//we update the players path plan based on current position. This is actually not right and it should be predicted
								UpdatePlayerGuessPlan(pOtherPlayerActor,
									gameAIViews[pOtherPlayerActor->GetId()], playerGuessView.data, otherPlayerNode);
							}

							if (resetOtherGuessItem)
							{
								//distrust the guessing plan and reset guess player 
								playerGuessView.isUpdated = false;
								UpdatePlayerGuessItems(playerGuessView);
								UpdatePlayerGuessItems(pOtherPlayerActor->GetId(), playerGuessView);

								std::stringstream updatePlayer;
								updatePlayer << "\n reset other items for player guess: " << pOtherPlayerActor->GetId() << " ";
								PrintInfo(updatePlayer.str());
								//we update the players path plan based on current position. This is actually not right and it should be predicted
								UpdatePlayerGuessPlan(pOtherPlayerActor,
									gameAIViews[pOtherPlayerActor->GetId()], playerGuessView.data, otherPlayerNode);
							}
							else if (playerGuessView.data.plan.node &&
								playerGuessView.data.plan.node->IsVisibleNode(playerGuessView.guessPlayers[pPlayerActor->GetId()].plan.node))
							{
								//distrust the guessing plan and reset guess player
								playerGuessView.isUpdated = false;
								UpdatePlayerGuessItems(playerGuessView);
								UpdatePlayerGuessItems(pPlayerActor->GetId(), playerGuessView);

								std::stringstream updatePlayer;
								updatePlayer << "\n visible other node for player guess: " << pPlayerActor->GetId() << " ";
								PrintInfo(updatePlayer.str());
								//we update the players path plan based on current position. This is actually not right and it should be predicted
								UpdatePlayerGuessPlan(pPlayerActor,
									gameAIViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);
							}
						}
					}
				}
			}

			//update player guess
			UpdatePlayerGuessState(deltaMs, playerGuessView);
			UpdatePlayerGuessState(deltaMs, playerGuessView, pPlayerActor->GetId());

			//update guess items
			UpdatePlayerGuessItems(deltaMs, pPlayerActor->GetId(), playerGuessView);

			UpdatePlayerGuessView(pPlayerActor->GetId(), playerGuessView, isPlayerGuessUpdated);
		}

		if (runDecisionMaking)
			mPlayerEvaluations[pPlayerActor->GetId()] = ET_AWARENESS;
	}

	mUpdateTimeMs += deltaMs;
	if (mUpdateTimeMs < 200)
	{
		AIGame::EventTrack eventTrack;
		eventTrack.elapsedTime = (float)deltaMs;
		AddGameEventTrack(eventTrack);
		return;
	}
	mUpdateTimeMs -= 200;

	//log ai guessing system
	LogEvents(deltaMs);
}

void QuakeAIManager::LogEvents(unsigned long deltaMs)
{
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();

	AIGame::GameState gameState;

	mUpdateMutex.lock();
	gameState.id = ++mUpdateCounter;
	mUpdateMutex.unlock();

	gameState.time = 
		std::to_string(realTime.Hour) + ":" +
		std::to_string(realTime.Minute) + ":" + 
		std::to_string(realTime.Second);
	AddGameState(gameState);

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetArmorActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetFiringActors(searchActors);
	for (std::shared_ptr<Actor> actor : searchActors)
	{
		if (actor->GetType() == "Weapon")
		{
			std::shared_ptr<WeaponPickup> weaponPickup = actor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();

			AIGame::Item item;
			item.id = actor->GetId();
			item.visible = weaponPickup->mRespawnTime > 0 ? false : true;
			AddGameItem(item);
		}
		else if (actor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> ammoPickup = actor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();

			AIGame::Item item;
			item.id = actor->GetId();
			item.visible = ammoPickup->mRespawnTime > 0 ? false : true;
			AddGameItem(item);
		}
		else if (actor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> armorPickup = actor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();

			AIGame::Item item;
			item.id = actor->GetId();
			item.visible = armorPickup->mRespawnTime > 0 ? false : true;
			AddGameItem(item);
		}
		else if (actor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> healthPickup = actor->GetComponent<HealthPickup>(HealthPickup::Name).lock();

			AIGame::Item item;
			item.id = actor->GetId();
			item.visible = healthPickup->mRespawnTime > 0 ? false : true;
			AddGameItem(item);
		}
		else if (actor->GetType() == "Fire")
		{
			std::shared_ptr<TransformComponent> transformComponent(
				actor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

			EulerAngles<float> viewAngles;
			viewAngles.mAxis[1] = 1;
			viewAngles.mAxis[2] = 2;
			transformComponent->GetTransform().GetRotation(viewAngles);
			Vector3<float> position = transformComponent->GetTransform().GetTranslation();
			float yaw = viewAngles.mAngle[AXIS_Y];
			float pitch = viewAngles.mAngle[AXIS_Z];

			if (!actor->GetComponent<GrenadeFire>(GrenadeFire::Name).expired())
			{
				std::shared_ptr<GrenadeFire> grenadeFire = actor->GetComponent<GrenadeFire>(GrenadeFire::Name).lock();

				AIGame::Projectile projectile;
				projectile.id = actor->GetId();
				projectile.code = grenadeFire->GetCode();
				projectile.yaw = yaw;
				projectile.pitch = pitch;
				projectile.position = { position[0], position[1], position[2] };
				AddGameProjectile(projectile);
			}
			else if (!actor->GetComponent<PlasmaFire>(PlasmaFire::Name).expired())
			{
				std::shared_ptr<PlasmaFire> plasmaFire = actor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock();
				
				AIGame::Projectile projectile;
				projectile.id = actor->GetId();
				projectile.code = plasmaFire->GetCode();
				projectile.yaw = yaw;
				projectile.pitch = pitch;
				projectile.position = { position[0], position[1], position[2] };
				AddGameProjectile(projectile);
			}
			else if (!actor->GetComponent<RocketFire>(RocketFire::Name).expired())
			{
				std::shared_ptr<RocketFire> rocketFire = actor->GetComponent<RocketFire>(RocketFire::Name).lock();

				AIGame::Projectile projectile;
				projectile.id = actor->GetId();
				projectile.code = rocketFire->GetCode();
				projectile.yaw = yaw;
				projectile.pitch = pitch;
				projectile.position = { position[0], position[1], position[2] };
				AddGameProjectile(projectile);
			}
		}
	}

	std::vector<std::shared_ptr<PlayerActor>> playerActors;
	game->GetPlayerActors(playerActors);
	for (std::shared_ptr<PlayerActor> pPlayerActor : playerActors)
	{
		std::shared_ptr<TransformComponent> pTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

		PlayerView playerView;
		GetPlayerView(pPlayerActor->GetId(), playerView);

		EulerAngles<float> viewAngles;
		viewAngles.mAxis[1] = 1;
		viewAngles.mAxis[2] = 2;
		pTransformComponent->GetTransform().GetRotation(viewAngles);
		Vector3<float> position = pTransformComponent->GetTransform().GetTranslation();
		float yaw = viewAngles.mAngle[AXIS_Y];
		float pitch = viewAngles.mAngle[AXIS_Z];

		AIGame::Player player;
		player.id = pPlayerActor->GetId();
		player.score = pPlayerActor->GetState().persistant[PERS_SCORE];
		player.health = pPlayerActor->GetState().stats[STAT_HEALTH];
		player.armor = pPlayerActor->GetState().stats[STAT_ARMOR];
		player.weapon = pPlayerActor->GetState().weapon;
		player.yaw = yaw;
		player.pitch = pitch;
		player.position = { position[0], position[1], position[2] };
		for (unsigned int i = 0; i < MAX_WEAPONS; i++)
		{
			if (pPlayerActor->GetState().stats[STAT_WEAPONS] & (1 << i))
			{
				AIGame::Weapon weapon;
				weapon.id = i;
				weapon.ammo = pPlayerActor->GetState().ammo[i];
				player.weapons.push_back(weapon);
			}
		}
		AddGamePlayer(player);
	}

	AIGame::EventTrack eventTrack;
	eventTrack.elapsedTime = (float)deltaMs;
	AddGameEventTrack(eventTrack);
}

void QuakeAIManager::PrintPlayerData(const PlayerData& playerData)
{
	std::stringstream playerInfo;

	mUpdateMutex.lock();
	playerInfo << "frame " << mUpdateCounter;
	mUpdateMutex.unlock();

	playerInfo << " id " << playerData.player << " heuristic " << playerData.heuristic << 
		" plan " << playerData.plan.id << " plan time " << playerData.planWeight;
	if (playerData.plan.node)
		playerInfo << " node " << playerData.plan.node->GetId();
	playerInfo << " arcs";
	for (PathingArc* pArc : playerData.plan.path)
		playerInfo << " " << pArc->GetId();
	playerInfo << " weapon " << playerData.weapon << " weapon target " << playerData.target;
	if (playerData.weapon != WP_NONE)
		playerInfo << " damage " << playerData.damage[playerData.weapon - 1];
	playerInfo << " items";
	for (auto const& item : playerData.items)
		playerInfo << " " << item.first;
	playerInfo << " stats";
	for (unsigned int st = 0; st < MAX_STATS; st++)
		playerInfo << " " << playerData.stats[st];
	playerInfo << " ammo";
	for (unsigned int wp = 0; wp < MAX_WEAPONS; wp++)
		playerInfo << " " << playerData.ammo[wp];

	PrintInfo(playerInfo.str());
}

//path generation via physics simulation
void QuakeAIManager::CreatePathing(ActorId playerId, NodePlan& pathPlan)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	if (mPathingGraph)
		mPathingGraph->DestroyGraph();
	else
		mPathingGraph = std::make_shared<PathingGraph>();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), mPathingGraph);

	if (mOpenSet.size())
		SimulatePathing(transform, pathPlan, mPathingGraph);

	//return to original position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	gamePhysics->OnUpdate(mSimulationStep);

	game->RegisterAllDelegates();
}

PathingNode* QuakeAIManager::CreatePathingNode(ActorId playerId, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	RegisterAllDelegates();

	mActorPositions.clear();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	PathingNode* newNode = mOpenSet.front();
	newNode->SetCluster(USHRT_MAX);
	mOpenSet.pop_back();

	//return to original position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	gamePhysics->OnUpdate(mSimulationStep);

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
				pClosestNode->SetActorId(pItemActor->GetId());
		}
	}

	mActorPositions.clear();

	RemoveAllDelegates();

	game->RegisterAllDelegates();

	return newNode;
}

PathingNode* QuakeAIManager::CreatePathingNode(ActorId playerId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	RegisterAllDelegates();

	mActorPositions.clear();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	transform.SetTranslation(position);

	//set player position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	gamePhysics->OnUpdate(mSimulationStep);

	SimulateStanding(INVALID_ACTOR_ID, transform.GetTranslation(), graph);

	PathingNode* newNode = mOpenSet.front();
	mOpenSet.pop_back();

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
				pClosestNode->SetActorId(pItemActor->GetId());
		}
	}

	mActorPositions.clear();

	RemoveAllDelegates();

	game->RegisterAllDelegates();

	return newNode;
}

void QuakeAIManager::CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes,
	std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
	game->RemoveAllDelegates();

	mPlayerActor = std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());
	Transform transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	mOpenSet = pathingNodes;
	if (mOpenSet.size())
		SimulatePathing(selectedClusters, graph);

	//return to original position
	gamePhysics->SetTransform(mPlayerActor->GetId(), transform);
	gamePhysics->SetVelocity(mPlayerActor->GetId(), Vector3<float>::Zero());
	gamePhysics->Fall(mPlayerActor->GetId(), -Vector3<float>::Unit(AXIS_Y) * mFallSpeed[AXIS_Y]);
	gamePhysics->OnUpdate(mSimulationStep);

	game->RegisterAllDelegates();
}

//map generation via physics simulation
void QuakeAIManager::CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes, std::shared_ptr<PathingGraph>& graph)
{
	GameApplication* gameApp = (GameApplication*)Application::App;
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

void QuakeAIManager::SimulatePathing(std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph)
{
	RegisterAllDelegates();

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
}

void QuakeAIManager::SimulatePathing(Transform transform, NodePlan& nodePlan, std::shared_ptr<PathingGraph>& graph)
{
	RegisterAllDelegates();

	mActorPositions.clear();

	// grab the candidate
	SimulateMove(mOpenSet.front(), transform, graph);

	RemoveAllDelegates();

	PathingNode* pStartNode = mOpenSet.front();
	PathingNode* pEndNode = mOpenSet.back();

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
	PathPlan* pathPlan = graph->FindPath(pStartNode, pEndNode, skipArc);
	if (pathPlan)
	{
		nodePlan.ResetPathPlan(pathPlan->GetArcs());
		nodePlan.node = pStartNode;
		delete pathPlan;
	}

	mActorPositions.clear();
}

void QuakeAIManager::SimulatePathing(std::shared_ptr<PathingGraph>& graph)
{
	RegisterAllDelegates();

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
				pClosestNode->SetActorId(pItemActor->GetId());

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
}

void QuakeAIManager::CreateTransitions(std::shared_ptr<PathingGraph>& graph)
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
					nodes.push_back(graph->FindClosestNode(pathPosition));
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

bool QuakeAIManager::CheckActorNode(PathingNode* pathNode)
{
	Vector3<float> center = GameLogic::Get()->GetGamePhysics()->GetCenter(pathNode->GetActorId());
	Vector3<float> scale = GameLogic::Get()->GetGamePhysics()->GetScale(pathNode->GetActorId()) / 2.f;
	BoundingBox<float> actorBB(center - scale, center + scale);

	scale = GameLogic::Get()->GetGamePhysics()->GetScale(mPlayerActor->GetId()) / 2.f;
	scale[AXIS_X] = 0.25f;
	scale[AXIS_Z] = 0.25f;
	Vector3<float> minEdge = pathNode->GetPosition() - scale;
	Vector3<float> maxEdge = pathNode->GetPosition() + scale;
	BoundingBox<float> nodeBB(minEdge, maxEdge);
	if (!actorBB.Intersect(nodeBB))
	{
		pathNode->SetActorId(INVALID_ACTOR_ID);
		return false;
	}

	return true;
}

void QuakeAIManager::CreateClusters(std::shared_ptr<PathingGraph>& graph, unsigned int totalClusters)
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

	GameApplication* gameApp = (GameApplication*)Application::App;
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

		PathingNode* pNode = graph->FindClosestNode(transformComponent->GetPosition());
		clusterActorNodes[pNode->GetCluster()][actor] = pNode;
	}

	std::map<unsigned short, PathingNode*> searchClusters;
	for (Clustering kCluster : kmeans.GetClusters())
	{
		if (clusterNodes.find(kCluster.GetId()) == clusterNodes.end() || clusterNodes[kCluster.GetId()].empty())
			continue;

		PathingNode* clusterCenterNode = graph->FindClosestNode(
			Vector3<float>{kCluster.GetCenter(0), kCluster.GetCenter(1), kCluster.GetCenter(2)});
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

void QuakeAIManager::SimulateStanding(ActorId actorId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph)
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

void QuakeAIManager::SimulateTriggerTeleport(
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

	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation());
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

void QuakeAIManager::SimulateTriggerPush(
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
	Vector3<float> fallSpeed = {
		PUSHTRIGGER_FALL_SPEED_XZ, PUSHTRIGGER_FALL_SPEED_XZ, PUSHTRIGGER_FALL_SPEED_Y };
	direction = target.GetTranslation() - transform.GetTranslation();
	float push = mPushSpeed[AXIS_Y] + direction[AXIS_Y] * 0.01f;

	direction[AXIS_Y] = 0;
	Normalize(direction);

	jump[AXIS_X] = direction[AXIS_X] * mPushSpeed[AXIS_X];
	jump[AXIS_Z] = direction[AXIS_Z] * mPushSpeed[AXIS_Z];
	jump[AXIS_Y] = push;

	fall[AXIS_X] = direction[AXIS_X] * fallSpeed[AXIS_X];
	fall[AXIS_Z] = direction[AXIS_Z] * fallSpeed[AXIS_Z];
	fall[AXIS_Y] = -fallSpeed[AXIS_Y];

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
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation());
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

void QuakeAIManager::SimulateMove(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
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

	// nodes closed to falling position
	{
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
		} while (FindClosestMovement(movements, transform.GetTranslation()) >=  4.f); // stalling is a break condition

		if (!movements.empty())
		{
			float deltaTime = 0.f, totalTime = 0.f;
			std::vector<std::pair<Transform, bool>>::iterator itMove = movements.begin();
			std::vector<Vector3<float>> positions{ (*itMove).first.GetTranslation()};
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

								//we only consider arcs with certain minimum and maximum length
								if (totalTime >= 0.04f && totalTime <= 0.2f)
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
									//we only consider arcs with certain minimum and maximum length
									if (totalTime >= 0.04f && totalTime <= 0.2f)
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

void QuakeAIManager::SimulateMove(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
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
				movements.push_back({transform, isOnGround });

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

								//we only consider arcs with certain minimum and maximum length
								if (totalTime >= 0.04f && totalTime <= 0.2f)
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
									//we only consider arcs with certain minimum and maximum length
									if (totalTime >= 0.04f && totalTime <= 0.2f)
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

void QuakeAIManager::SimulateJump(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
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

	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	Vector3<float> direction, jump, fall;
	Matrix4x4<float> rotation = transform.GetRotation();

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
		return;

	//then we do the "real" jump to the closest node we have found from the simulation
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation());
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
			return;

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

void QuakeAIManager::SimulateJump(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
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
		PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation());
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

void QuakeAIManager::SimulateFall(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph)
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
		PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation());
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

void QuakeAIManager::SimulateFall(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph)
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

	transform = gamePhysics->GetTransform(mPlayerActor->GetId());

	Vector3<float> direction, fall;
	Matrix4x4<float> rotation = transform.GetRotation();

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
		return;

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

	if (totalTime > 10.f) return;

	//we store the falling action if we find a landing node
	transform = gamePhysics->GetTransform(mPlayerActor->GetId());
	PathingNode* pEndNode = graph->FindClosestNode(transform.GetTranslation());
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
		if (moveTime > 0.1f) return;

		fall[AXIS_X] = direction[AXIS_X] * mFallSpeed[AXIS_X];
		fall[AXIS_Z] = direction[AXIS_Z] * mFallSpeed[AXIS_Z];
		fall[AXIS_Y] = -mFallSpeed[AXIS_Y];

		gamePhysics->Fall(mPlayerActor->GetId(), fall);
		gamePhysics->OnUpdate(mSimulationStep);

		// gravity falling simulation
		float totalTime = mSimulationStep;
		transform = gamePhysics->GetTransform(mPlayerActor->GetId());
		nodePositions.push_back( transform.GetTranslation() );
		while (!gamePhysics->OnGround(mPlayerActor->GetId()) && totalTime <= 10.f)
		{
			gamePhysics->OnUpdate(mSimulationStep);

			totalTime += mSimulationStep;

			transform = gamePhysics->GetTransform(mPlayerActor->GetId());
			nodePositions.push_back(transform.GetTranslation());
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

Vector3<float> QuakeAIManager::RayCollisionDetection(const Vector3<float>& startPos, const Vector3<float>& collisionPos)
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

void QuakeAIManager::SimulateVisibility(std::shared_ptr<PathingGraph>& graph)
{
	std::shared_ptr<BaseGamePhysic> gamePhysics = GameLogic::Get()->GetGamePhysics();

	std::mutex mutex;

	// first we get visibility info from every node by raycasting
	const PathingNodeMap& pathingNodes = graph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* pathNode = (*it).second;
		//set muzzle location relative to pivoting eye
		Vector3<float> muzzle = pathNode->GetPosition();
		muzzle[2] += mPlayerActor->GetState().viewHeight;
		muzzle -= Vector3<float>::Unit(AXIS_Z) * 11.f;
		Concurrency::parallel_for_each(begin(graph->GetNodes()), end(graph->GetNodes()), [&](auto& node)
		{
			PathingNode* visibleNode = node.second;
			Vector3<float> end = visibleNode->GetPosition() +
				(float)mPlayerActor->GetState().viewHeight * Vector3<float>::Unit(AXIS_Y);

			std::vector<ActorId> collisionActors;
			std::vector<Vector3<float>> collisions, collisionNormals;
			gamePhysics->CastRay(muzzle, end, collisionActors, collisions, collisionNormals, mPlayerActor->GetId());

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

void QuakeAIManager::PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData)
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
			Vector3<float> position = pPhysicComponent->GetTransform().GetTranslation();
			mActorPositions[position] = pItemActor->GetId();
		}
	}
}

void QuakeAIManager::PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerLeave> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerLeave>(pEventData);
}

void QuakeAIManager::PhysicsCollisionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysCollision> pCastEventData =
		std::static_pointer_cast<EventDataPhysCollision>(pEventData);
}

void QuakeAIManager::PhysicsSeparationDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysSeparation> pCastEventData =
		std::static_pointer_cast<EventDataPhysSeparation>(pEventData);

}

void QuakeAIManager::RegisterAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsTriggerEnterDelegate),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsTriggerLeaveDelegate),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsCollisionDelegate),
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsSeparationDelegate),
		EventDataPhysSeparation::skEventType);
}

void QuakeAIManager::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsTriggerEnterDelegate),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsTriggerLeaveDelegate),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsCollisionDelegate),
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIManager::PhysicsSeparationDelegate),
		EventDataPhysSeparation::skEventType);
}