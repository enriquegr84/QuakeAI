/*******************************************************
 * QuakeAIManager.cpp : AI Manager class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#include "QuakeAIManager.h"

#include "Quake.h"
#include "QuakeApp.h"
#include "QuakeAIView.h"

#include <cereal/types/map.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>

float QuakeAIManager::CalculatePathingWeight(const PlayerData& playerData)
{
	float closestWeight = 0.f;
	if (playerData.plan.path.size())
	{
		std::shared_ptr<PlayerActor> playerActor =
			std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerData.player).lock());

		std::shared_ptr<PhysicComponent> pPhysicComponent(
			playerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		Vector3<float> currentPosition = pPhysicComponent->GetPosition();

		PathingArc* currentArc = *playerData.plan.path.begin();
		const std::vector<float>& transitionWeights = currentArc->GetTransition()->GetWeights();
		const std::vector<Vector3<float>>& transitionPositions = currentArc->GetTransition()->GetPositions();

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

Vector3<float> QuakeAIManager::CalculatePathingPosition(const PlayerData& playerData)
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


QuakeAIManager::QuakeAIManager() : AIManager()
{
	mEnable = false;

	mCurrentGameSimulation = nullptr;
	mGameSimulation = AIGameSimulation();
	mPhysicsSimulation = AIPhysicsSimulation();

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

	for (auto& actorPickup : mGameSimulation.mGameActorPickups)
		delete actorPickup.second;

	mGameActors.clear();
	mGameSimulation.mGameActorPickups.clear();
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
void QuakeAIManager::LoadGraph(const std::wstring& path, float weightConversion)
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

	mPhysicsSimulation.mLastArcId = 0;
	mPhysicsSimulation.mLastNodeId = 0;

	mPathingGraph = std::make_shared<PathingGraph>();
	mGameSimulation.mPathingGraph = mPathingGraph;

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (auto const& node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{ node.position.x, node.position.y, node.position.z };
		if (mPhysicsSimulation.mLastNodeId < pathNodeId) 
			mPhysicsSimulation.mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		mPathingGraph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (auto const& node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (AIMap::VisibleNode visibleNode : node.visibles)
		{
			PathingNode* visibilityNode = pathingGraph[visibleNode.id];
			pathNode->AddVisibleNode(
				visibilityNode, Length(visibilityNode->GetPosition() - pathNode->GetPosition()));

		}

		for (auto const& arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float arcWeight = arc.weight * weightConversion;
			if (mPhysicsSimulation.mLastArcId < arcId) 
				mPhysicsSimulation.mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], arcWeight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (auto const& nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (auto const& weight : arc.weights)
			{
				weights.push_back(weight * weightConversion);
			}
			for (auto const& position : arc.positions)
			{
				positions.push_back(Vector3<float>{(float)position.x, (float)position.y, (float)position.z});
			}

			if (nodes.size())
			{
				PathingTransition* pathTransition = new PathingTransition(nodes, weights, positions);
				pathArc->AddTransition(pathTransition);
			}
		}

		for (auto const& cluster : node.clusters)
		{
			int pathingType = cluster.type;
			int clusterNode = cluster.nodeid;
			int clusterTarget = cluster.targetid;

			PathingCluster* pathCluster = new PathingCluster(pathingType);
			pathCluster->LinkClusters(pathingGraph[clusterNode], pathingGraph[clusterTarget]);
			pathNode->AddCluster(pathCluster);
		}

		for (auto const& actor : node.actors)
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
	for (auto const& cluster : data.clusters)
	{
		Cluster* pCluster = new Cluster(cluster.id, pathingGraph[cluster.node]);

		for (auto const& clusterNode : cluster.nodes)
			pCluster->AddNode(pathingGraph[clusterNode]);

		for (auto const& nodeActor : cluster.nodeActors)
			pCluster->AddNodeActor(nodeActor.first, pathingGraph[nodeActor.second]);

		mPathingGraph->InsertCluster(pCluster);

		clusterGraph[pCluster->GetId()] = pCluster;
	}

	for (auto const& cluster : data.clusters)
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
void QuakeAIManager::LoadGraph(const std::wstring& path, std::shared_ptr<PathingGraph>& graph, float weightConversion)
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

	mPhysicsSimulation.mLastArcId = 0;
	mPhysicsSimulation.mLastNodeId = 0;

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (auto const& node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{ node.position.x, node.position.y, node.position.z };
		if (mPhysicsSimulation.mLastNodeId < pathNodeId) 
			mPhysicsSimulation.mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		graph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (auto const& node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (auto const& visibleNode : node.visibles)
		{
			PathingNode* visibilityNode = pathingGraph[visibleNode.id];
			pathNode->AddVisibleNode(
				visibilityNode, Length(visibilityNode->GetPosition() - pathNode->GetPosition()));
		}

		for (auto const& arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float arcWeight = arc.weight * weightConversion;
			if (mPhysicsSimulation.mLastArcId < arcId) 
				mPhysicsSimulation.mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], arcWeight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (auto const& nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (auto const& weight : arc.weights)
			{
				weights.push_back(weight * weightConversion);
			}
			for (auto const& position : arc.positions)
			{
				positions.push_back(Vector3<float>{(float)position.x, (float)position.y, (float)position.z});
			}

			if (nodes.size())
			{
				PathingTransition* pathTransition = new PathingTransition(nodes, weights, positions);
				pathArc->AddTransition(pathTransition);
			}
		}

		for (auto const& cluster : node.clusters)
		{
			int pathingType = cluster.type;
			int clusterNode = cluster.nodeid;
			int clusterTarget = cluster.targetid;

			PathingCluster* pathCluster = new PathingCluster(pathingType);
			pathCluster->LinkClusters(pathingGraph[clusterNode], pathingGraph[clusterTarget]);
			pathNode->AddCluster(pathCluster);
		}

		for (auto const& actor : node.actors)
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
	for (auto const& cluster : data.clusters)
	{
		Cluster* pCluster = new Cluster(cluster.id, pathingGraph[cluster.node]);

		for (auto const& clusterNode : cluster.nodes)
			pCluster->AddNode(pathingGraph[clusterNode]);

		for (auto const& nodeActor : cluster.nodeActors)
			pCluster->AddNodeActor(nodeActor.first, pathingGraph[nodeActor.second]);

		graph->InsertCluster(pCluster);

		clusterGraph[pCluster->GetId()] = pCluster;
	}

	for (auto const& cluster : data.clusters)
	{
		for (auto const& clusterVisible : cluster.visibles)
			clusterGraph[cluster.id]->AddVisibleCluster(clusterVisible.first, pathingGraph[clusterVisible.second]);
	}
}

/////////////////////////////////////////////////////////////////////////////
// QuakeAIManager::LoadPathingMap
//
//    Loads basic pathing information
//
void QuakeAIManager::LoadPathingMap(const std::wstring& path, float weightConversion)
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

	mPhysicsSimulation.mLastArcId = 0;
	mPhysicsSimulation.mLastNodeId = 0;

	mPathingGraph = std::make_shared<PathingGraph>();

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (auto const& node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{ node.position.x, node.position.y, node.position.z };
		if (mPhysicsSimulation.mLastNodeId < pathNodeId) 
			mPhysicsSimulation.mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		mPathingGraph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (auto const& node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (auto const& arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float arcWeight = arc.weight * weightConversion;
			if (mPhysicsSimulation.mLastArcId < arcId) 
				mPhysicsSimulation.mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], arcWeight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (auto const& nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (auto const& weight : arc.weights)
			{
				weights.push_back(weight * weightConversion);
			}
			for (auto const& position : arc.positions)
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

	for (auto const& cluster : data.clusters)
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
void QuakeAIManager::LoadPathingMap(const std::wstring& path, std::shared_ptr<PathingGraph>& graph, float weightConversion)
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

	mPhysicsSimulation.mLastArcId = 0;
	mPhysicsSimulation.mLastNodeId = 0;

	std::map<unsigned int, PathingNode*> pathingGraph;
	for (auto const& node : data.nodes)
	{
		unsigned short clusterId = node.clusterid;
		unsigned short pathNodeId = node.id;
		ActorId actorId = node.actorid;
		float tolerance = node.tolerance;
		Vector3<float> position{ node.position.x, node.position.y, node.position.z };
		if (mPhysicsSimulation.mLastNodeId < pathNodeId) 
			mPhysicsSimulation.mLastNodeId = pathNodeId;

		PathingNode* pathNode = new PathingNode(pathNodeId, actorId, position, tolerance);
		pathNode->SetCluster(clusterId);
		graph->InsertNode(pathNode);

		pathingGraph[pathNodeId] = pathNode;
	}

	for (auto const& node : data.nodes)
	{
		unsigned short pathNodeId = node.id;
		PathingNode* pathNode = pathingGraph[pathNodeId];

		for (auto const& arc : node.arcs)
		{
			unsigned int arcId = arc.id;
			unsigned short arcType = arc.type;
			int arcNode = arc.nodeid;
			float arcWeight = arc.weight * weightConversion;
			if (mPhysicsSimulation.mLastArcId < arcId) 
				mPhysicsSimulation.mLastArcId = arcId;

			PathingArc* pathArc = new PathingArc(arcId, arcType, pathingGraph[arcNode], arcWeight);
			pathNode->AddArc(pathArc);

			std::vector<float> weights;
			std::vector<PathingNode*> nodes;
			std::vector<Vector3<float>> positions;
			for (auto const& nodeid : arc.nodes)
			{
				nodes.push_back(pathingGraph[nodeid]);
			}
			for (auto const& weight : arc.weights)
			{
				weights.push_back(weight * weightConversion);
			}
			for (auto const& position : arc.positions)
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

	for (auto const& cluster : data.clusters)
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
	mPhysicsSimulation.GetPlayerActor() = 
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());

	// we obtain visibility information from pathing graph 
	mPhysicsSimulation.SimulateVisibility(graph);

	// create transitions associated to closest node
	mPhysicsSimulation.CreateTransitions(graph);

	// we group the graph nodes in clusters
	mPhysicsSimulation.CreateClusters(graph, MAX_CLUSTERS);
}

void QuakeAIManager::UpdateMap(ActorId playerId)
{
	mPhysicsSimulation.GetPlayerActor() = 
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock());

	// we obtain visibility information from pathing graph 
	mPhysicsSimulation.SimulateVisibility(mPathingGraph);

	// create transitions associated to closest node
	mPhysicsSimulation.CreateTransitions(mPathingGraph);

	// we group the graph nodes in clusters
	mPhysicsSimulation.CreateClusters(mPathingGraph, MAX_CLUSTERS);
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

	float aiPathWeightOffset = CalculatePathingWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView aiSimulation = aiView;
	aiSimulation.data.planWeight = 0.24f;
	aiSimulation.data.planWeight += aiPathWeightOffset;
	aiSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiSimulation);
	aiSimulation.data.planWeight = 0.f;
	for (PathingArc* aiSimulationArc : aiSimulation.data.plan.path)
		aiSimulation.data.planWeight += aiSimulationArc->GetWeight();
	aiSimulation.data.planWeight -= aiPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);
	playerGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* playerGuessSimulationArc : playerGuessSimulation.data.plan.path)
		playerGuessSimulation.data.planWeight += playerGuessSimulationArc->GetWeight();
	playerGuessSimulation.data.planWeight -= playerGuessPathWeightOffset;

	playerGuessSimulation.data.planWeight = 0.f;

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
	std::map<ActorId, float> gameItems = aiView.gameItems;

	if (aiSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiView.data.items)
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!aiSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}

		// update the items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : playerGuessView.items[mPlayers[GV_HUMAN]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!aiSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}
	}

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiView.data.ResetItems();
	aiView.data.planWeight = aiSimulation.data.planWeight;
	aiView.data.valid = aiSimulation.data.plan.path.empty() ? false : true;

	playerGuessView.data.ResetItems();
	playerGuessView.data.planWeight = playerGuessSimulation.data.planWeight;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	PathingArcVec aiSimulationPath = aiSimulation.data.plan.path;

	//simulation
	bool success = mGameSimulation.SimulatePlayerGuessing(
		aiView.data, aiSimulation.data, playerGuessView.data, playerGuessSimulation.data, gameDecision.evaluation.threat,
		gameDecision.evaluation.playerGuessItems, &mPlayerEvaluations[mPlayers[GV_AI]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		//advance the path plan for the ai player
		for (PathingArc* aiSimulationPathArc : aiSimulationPath)
		{
			if (aiSimulation.data.planWeight <= 0.f)
				break;

			aiSimulation.data.plan.path.erase(aiSimulation.data.plan.path.begin());
			aiSimulation.data.plan.node = aiSimulationPathArc->GetNode();
			aiSimulation.data.planWeight -= aiSimulationPathArc->GetWeight();
		}

		mMutex.lock();

		PrintInfo("\nAI Guessing AI player output: ");
		PrintPlayerData(aiSimulation.data);

		PrintInfo("\nAI Guessing human player guess output: ");
		PrintPlayerData(playerGuessSimulation.data);

		aiView.isUpdated = true;
		aiView.simulation = aiSimulation.data;
		aiView.threat = gameDecision.evaluation.threat;

		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = aiSimulation.data;
		playerGuessView.threat = gameDecision.evaluation.threat;

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

bool QuakeAIManager::MakeAIDecision(PlayerView& aiView)
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

	float aiPathWeightOffset = CalculatePathingWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed
	PlayerView aiSimulation = aiView;
	aiSimulation.data.planWeight = 0.24f;
	aiSimulation.data.planWeight += aiPathWeightOffset;
	aiSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiSimulation);
	aiSimulation.data.planWeight = 0.f;
	for (PathingArc* aiSimulationArc : aiSimulation.data.plan.path)
		aiSimulation.data.planWeight += aiSimulationArc->GetWeight();
	aiSimulation.data.planWeight -= aiPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);
	playerGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* playerGuessSimulationArc : playerGuessSimulation.data.plan.path)
		playerGuessSimulation.data.planWeight += playerGuessSimulationArc->GetWeight();
	playerGuessSimulation.data.planWeight -= playerGuessPathWeightOffset;

	PrintInfo("\nAI Decision Human player guess input after: ");
	PrintPlayerData(playerGuessSimulation.data);

	PrintInfo("\nAI Decision AI player input after: ");
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
	std::map<ActorId, float> gameItems = aiView.gameItems;

	if (aiSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiView.data.items)
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!aiSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}

		// update the items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : playerGuessView.items[mPlayers[GV_HUMAN]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!aiSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}
	}

	gameDecision.evaluation.playerDecisionItems = gameItems;
	gameDecision.evaluation.playerGuessItems = gameItems;

	playerGuessView.data.ResetItems();
	playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
	playerGuessView.data.planWeight = playerGuessSimulation.data.planWeight;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	aiView.data.ResetItems();
	aiView.data.planWeight = aiSimulation.data.planWeight;
	aiView.data.valid = aiSimulation.data.plan.path.empty() ? false : true;

	PathingArcVec aiSimulationPath = aiSimulation.data.plan.path;

	//simulation
	bool success = mGameSimulation.SimulatePlayerGuessing(
		aiView.data, aiSimulation.data, playerGuessView.data, playerGuessSimulation.data, gameDecision.evaluation.threat,
		gameDecision.evaluation.playerGuessItems, &mPlayerEvaluations[mPlayers[GV_AI]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		//advance the path plan for the ai player
		for (PathingArc* aiSimulationPathArc : aiSimulationPath)
		{
			if (aiSimulation.data.planWeight <= 0.f)
				break;

			aiSimulation.data.plan.path.erase(aiSimulation.data.plan.path.begin());
			aiSimulation.data.plan.node = aiSimulationPathArc->GetNode();
			aiSimulation.data.planWeight -= aiSimulationPathArc->GetWeight();
		}

		mMutex.lock();

		PrintInfo("\nAI Decision AI player output: ");
		PrintPlayerData(aiSimulation.data);

		PrintInfo("\nAI Decision human player guess output: ");
		PrintPlayerData(playerGuessSimulation.data);

		aiView.isUpdated = true;
		aiView.simulation = aiSimulation.data;
		aiView.threat = gameDecision.evaluation.threat;

		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = aiSimulation.data;
		playerGuessView.threat = gameDecision.evaluation.threat;

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

	PrintInfo("\nAI Guessing Decision Human player guess input before: ");
	PrintPlayerData(playerGuessView.data);

	PrintInfo("\nAI Guessing Decision AI player guess input before: ");
	PrintPlayerData(playerGuessView.guessPlayers[mPlayers[GV_AI]]);

	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;
	float aiGuessPathWeightOffset = 
		playerGuessView.guessPlayers[mPlayers[GV_AI]].planWeight > 0.f ? playerGuessView.guessPlayers[mPlayers[GV_AI]].planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.24f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);
	playerGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* playerGuessSimulationArc : playerGuessSimulation.data.plan.path)
		playerGuessSimulation.data.planWeight += playerGuessSimulationArc->GetWeight();
	playerGuessSimulation.data.planWeight -= playerGuessPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight = 0.f;
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight += aiGuessPathWeightOffset;
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation, mPlayers[GV_AI]);
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight = 0.f;
	for (PathingArc* aiGuessSimulationArc : playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].plan.path)
		playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight += aiGuessSimulationArc->GetWeight();
	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight -= aiGuessPathWeightOffset;

	playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight = 0.f;

	PrintInfo("\nAI Guessing Decision Human player guess input after: ");
	PrintPlayerData(playerGuessSimulation.data);

	PrintInfo("\nAI Guessing Decision AI player guess input after: ");
	PrintPlayerData(playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_CLOSEGUESSING;
	gameDecision.evaluation.target = GV_HUMAN;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput,
		playerGuessView.guessPlayers[mPlayers[GV_AI]], playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, playerGuessView.data, playerGuessSimulation.data);

	SetPlayerInput(gameDecision.evaluation.playerInput, playerGuessView.data, playerGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput,
		playerGuessView.guessPlayers[mPlayers[GV_AI]], playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

	// update the guess items from the world
	std::map<ActorId, float> gameItems = playerGuessView.guessItems[mPlayers[GV_HUMAN]];

	if (playerGuessSimulation.data.plan.node)
	{
		// exclude items which are guessed to be take by the human player
		for (auto const& humanGuessItem : playerGuessView.items[mPlayers[GV_HUMAN]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!playerGuessSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}

		// update the items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : playerGuessView.items[mPlayers[GV_AI]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!playerGuessSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}
	}

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerGuessView.data.ResetItems();
	playerGuessView.data.planWeight = playerGuessSimulation.data.planWeight;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
	playerGuessView.guessPlayers[mPlayers[GV_AI]].planWeight = 
		playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].planWeight;
	playerGuessView.guessPlayers[mPlayers[GV_AI]].valid = 
		playerGuessSimulation.guessPlayers[mPlayers[GV_AI]].plan.path.empty() ? false : true;

	//simulation
	bool success = mGameSimulation.SimulatePlayerDecision(playerGuessView.data, playerGuessSimulation.data,
		playerGuessView.guessPlayers[mPlayers[GV_AI]], playerGuessSimulation.guessPlayers[mPlayers[GV_AI]], 
		gameDecision.evaluation.threat, gameDecision.evaluation.playerGuessItems, &mPlayerEvaluations[mPlayers[GV_AI]], 
		(EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nAI Guessing Decision human player guess output: ");
		PrintPlayerData(playerGuessSimulation.data);

		PrintInfo("\nAI Guessing Decision AI player guess output: ");
		PrintPlayerData(playerGuessSimulation.guessPlayers[mPlayers[GV_AI]]);

		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = playerGuessSimulation.guessPlayers[mPlayers[GV_AI]];
		playerGuessView.threat = gameDecision.evaluation.threat;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, playerGuessView.simulation);

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, playerGuessView.guessSimulations[mPlayers[GV_AI]]);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, playerGuessView.simulation);

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

	float aiPathWeightOffset = CalculatePathingWeight(aiView.data);
	float playerGuessPathWeightOffset = playerGuessView.data.planWeight > 0.f ? playerGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView aiDecisionSimulation = aiView;
	aiDecisionSimulation.data.planWeight = 0.24f;
	aiDecisionSimulation.data.planWeight += aiPathWeightOffset;
	aiDecisionSimulation.data.plan.weight = aiPathWeightOffset;
	UpdatePlayerState(aiDecisionSimulation);
	aiDecisionSimulation.data.planWeight = 0.f;
	for (PathingArc* aiSimulationArc : aiDecisionSimulation.data.plan.path)
		aiDecisionSimulation.data.planWeight += aiSimulationArc->GetWeight();
	aiDecisionSimulation.data.planWeight -= aiPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView playerGuessSimulation = playerGuessView;
	playerGuessSimulation.data.planWeight = 0.f;
	playerGuessSimulation.data.planWeight += playerGuessPathWeightOffset;
	playerGuessSimulation.data.plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(playerGuessSimulation);
	playerGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* playerGuessSimulationArc : playerGuessSimulation.data.plan.path)
		playerGuessSimulation.data.planWeight += playerGuessSimulationArc->GetWeight();
	playerGuessSimulation.data.planWeight -= playerGuessPathWeightOffset;

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
	std::map<ActorId, float> gameItems = aiView.gameItems;

	if (aiDecisionSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiView.data.items)
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!aiDecisionSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}

		// update the items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : playerGuessView.items[mPlayers[GV_HUMAN]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!aiDecisionSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}
	}

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerGuessView.guessPlayers[mPlayers[GV_AI]].ResetItems();
	playerGuessView.data.ResetItems();
	playerGuessView.data.planWeight = playerGuessSimulation.data.planWeight;
	playerGuessView.data.valid = playerGuessSimulation.data.plan.path.empty() ? false : true;

	aiView.data.ResetItems();
	aiView.data.planWeight = aiDecisionSimulation.data.planWeight;
	aiView.data.valid = aiDecisionSimulation.data.plan.path.empty() ? false : true;

	PathingArcVec aiDecisionSimulationPath = aiDecisionSimulation.data.plan.path;

	//simulation
	bool success = mGameSimulation.SimulatePlayerDecision(
		aiView.data, aiDecisionSimulation.data, playerGuessView.data, playerGuessSimulation.data, gameDecision.evaluation.threat,
		gameDecision.evaluation.playerDecisionItems, &mPlayerEvaluations[mPlayers[GV_AI]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		//advance the path plan for the ai player
		for (PathingArc* aiDecisionSimulationPathArc : aiDecisionSimulationPath)
		{
			if (aiDecisionSimulation.data.planWeight <= 0.f)
				break;

			aiDecisionSimulation.data.plan.path.erase(aiDecisionSimulation.data.plan.path.begin());
			aiDecisionSimulation.data.plan.node = aiDecisionSimulationPathArc->GetNode();
			aiDecisionSimulation.data.planWeight -= aiDecisionSimulationPathArc->GetWeight();
		}

		mMutex.lock();

		PrintInfo("\nAI Guessing human player guess output: ");
		PrintPlayerData(playerGuessSimulation.data);

		PrintInfo("\nAI Guessing AI player guess output: ");
		PrintPlayerData(aiDecisionSimulation.data);

		PrintInfo("\nAI Decision AI player output: ");
		PrintPlayerData(aiDecisionSimulation.data);

		aiView.isUpdated = true;
		aiView.simulation = aiDecisionSimulation.data;
		aiView.threat = gameDecision.evaluation.threat;

		playerGuessView.isUpdated = true;
		playerGuessView.simulation = playerGuessSimulation.data;
		playerGuessView.guessSimulations[mPlayers[GV_AI]] = aiDecisionSimulation.data;
		playerGuessView.threat = gameDecision.evaluation.threat;

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

	float playerPathWeightOffset = CalculatePathingWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView playerSimulation = playerView;
	playerSimulation.data.planWeight = 0.24f;
	playerSimulation.data.planWeight += playerPathWeightOffset;
	playerSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerSimulation);
	playerSimulation.data.planWeight = 0.f;
	for (PathingArc* playerSimulationArc : playerSimulation.data.plan.path)
		playerSimulation.data.planWeight += playerSimulationArc->GetWeight();
	playerSimulation.data.planWeight -= playerPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);
	aiGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* aiGuessSimulationArc : aiGuessSimulation.data.plan.path)
		aiGuessSimulation.data.planWeight += aiGuessSimulationArc->GetWeight();
	aiGuessSimulation.data.planWeight -= aiGuessPathWeightOffset;

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
	std::map<ActorId, float> gameItems = playerView.gameItems;

	if (playerSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : playerView.data.items)
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!playerSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}

		// update the items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiGuessView.items[mPlayers[GV_AI]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!playerSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}
	}

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	playerView.data.ResetItems();
	playerView.data.planWeight = playerSimulation.data.planWeight;
	playerView.data.valid = playerSimulation.data.plan.path.empty() ? false : true;

	aiGuessView.data.ResetItems();
	aiGuessView.data.planWeight = aiGuessSimulation.data.planWeight;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	PathingArcVec playerSimulationPath = playerSimulation.data.plan.path;

	//simulation
	bool success = mGameSimulation.SimulatePlayerGuessing(playerView.data,
		playerSimulation.data, aiGuessView.data, aiGuessSimulation.data, gameDecision.evaluation.threat, 
		gameDecision.evaluation.playerGuessItems, &mPlayerEvaluations[mPlayers[GV_HUMAN]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		//advance the path plan for the human player
		for (PathingArc* playerSimulationPathArc : playerSimulationPath)
		{
			if (playerSimulation.data.planWeight <= 0.f)
				break;

			playerSimulation.data.plan.path.erase(playerSimulation.data.plan.path.begin());
			playerSimulation.data.plan.node = playerSimulationPathArc->GetNode();
			playerSimulation.data.planWeight -= playerSimulationPathArc->GetWeight();
		}

		mMutex.lock();

		PrintInfo("\nHuman Guessing Human player output: ");
		PrintPlayerData(playerSimulation.data);

		PrintInfo("\nHuman Guessing AI player guess output: ");
		PrintPlayerData(aiGuessSimulation.data);

		playerView.isUpdated = true;
		playerView.simulation = playerSimulation.data;
		playerView.threat = gameDecision.evaluation.threat;

		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = playerSimulation.data;
		aiGuessView.threat = gameDecision.evaluation.threat;

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

bool QuakeAIManager::MakeHumanDecision(PlayerView& playerView)
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

	float playerPathWeightOffset = CalculatePathingWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed
	PlayerView playerSimulation = playerView;
	playerSimulation.data.planWeight = 0.24f;
	playerSimulation.data.planWeight += playerPathWeightOffset;
	playerSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerSimulation);
	playerSimulation.data.planWeight = 0.f;
	for (PathingArc* playerSimulationArc : playerSimulation.data.plan.path)
		playerSimulation.data.planWeight += playerSimulationArc->GetWeight();
	playerSimulation.data.planWeight -= playerPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);
	aiGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* aiGuessSimulationArc : aiGuessSimulation.data.plan.path)
		aiGuessSimulation.data.planWeight += aiGuessSimulationArc->GetWeight();
	aiGuessSimulation.data.planWeight -= aiGuessPathWeightOffset;

	PrintInfo("\nHuamn Decision AI player guess input after: ");
	PrintPlayerData(aiGuessSimulation.data);

	PrintInfo("\nHuman Decision Human player input after: ");
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
	std::map<ActorId, float> gameItems = playerView.gameItems;

	if (playerSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : playerView.data.items)
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!playerSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}

		// update the items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiGuessView.items[mPlayers[GV_AI]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!playerSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}
	}

	gameDecision.evaluation.playerDecisionItems = gameItems;
	gameDecision.evaluation.playerGuessItems = gameItems;

	playerView.data.ResetItems();
	playerView.data.planWeight = playerSimulation.data.planWeight;
	playerView.data.valid = playerSimulation.data.plan.path.empty() ? false : true;

	aiGuessView.data.ResetItems();
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
	aiGuessView.data.planWeight = aiGuessSimulation.data.planWeight;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	PathingArcVec playerSimulationPath = playerSimulation.data.plan.path;

	//simulation
	bool success = mGameSimulation.SimulatePlayerGuessing(playerView.data, playerSimulation.data, 
		aiGuessView.data, aiGuessSimulation.data, gameDecision.evaluation.threat, gameItems, 
		&mPlayerEvaluations[mPlayers[GV_HUMAN]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		//advance the path plan for the human player
		for (PathingArc* playerSimulationPathArc : playerSimulationPath)
		{
			if (playerSimulation.data.planWeight <= 0.f)
				break;

			playerSimulation.data.plan.path.erase(playerSimulation.data.plan.path.begin());
			playerSimulation.data.plan.node = playerSimulationPathArc->GetNode();
			playerSimulation.data.planWeight -= playerSimulationPathArc->GetWeight();
		}

		mMutex.lock();

		PrintInfo("\nHuman Decision Human player output: ");
		PrintPlayerData(playerSimulation.data);

		PrintInfo("\nHuman Decision AI player guess output: ");
		PrintPlayerData(aiGuessSimulation.data);

		playerView.isUpdated = true;
		playerView.simulation = playerSimulation.data;
		playerView.threat = gameDecision.evaluation.threat;

		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = playerSimulation.data;
		aiGuessView.threat = gameDecision.evaluation.threat;

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

	PrintInfo("\nHuman Guessing Decision AI player guess input before: ");
	PrintPlayerData(aiGuessView.data);

	PrintInfo("\nHuman Guessing Decision Human player guess input before: ");
	PrintPlayerData(aiGuessView.guessPlayers[mPlayers[GV_HUMAN]]);

	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;
	float playerGuessPathWeightOffset = 
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].planWeight > 0.f ? aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.24f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);
	aiGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* aiGuessSimulationArc : aiGuessSimulation.data.plan.path)
		aiGuessSimulation.data.planWeight += aiGuessSimulationArc->GetWeight();
	aiGuessSimulation.data.planWeight -= aiGuessPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight = 0.f;
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight += playerGuessPathWeightOffset;
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].plan.weight = playerGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation, mPlayers[GV_HUMAN]);
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight = 0.f;
	for (PathingArc* playerGuessSimulationArc : aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].plan.path)
		aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight += playerGuessSimulationArc->GetWeight();
	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight -= playerGuessPathWeightOffset;

	aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight = 0.f;

	PrintInfo("\nHuman Guessing Decision AI player guess input after: ");
	PrintPlayerData(aiGuessSimulation.data);

	PrintInfo("\nHuman Guessing Decision Human player guess input after: ");
	PrintPlayerData(aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

	mMutex.unlock();

	AIAnalysis::GameDecision gameDecision = AIAnalysis::GameDecision();
	gameDecision.evaluation.type = ET_CLOSEGUESSING;
	gameDecision.evaluation.target = GV_AI;
	SetPlayerInput(gameDecision.evaluation.playerGuessInput,
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]], aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);
	SetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, aiGuessView.data, aiGuessSimulation.data);

	SetPlayerInput(gameDecision.evaluation.playerInput, aiGuessView.data, aiGuessSimulation.data);
	SetPlayerInput(gameDecision.evaluation.otherPlayerInput, 
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]], aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

	// update the guess items from the world
	std::map<ActorId, float> gameItems = aiGuessView.guessItems[mPlayers[GV_AI]];

	if (aiGuessSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiGuessView.items[mPlayers[GV_AI]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!aiGuessSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}

		// update the items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : aiGuessView.items[mPlayers[GV_HUMAN]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!aiGuessSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}
	}

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiGuessView.data.ResetItems();
	aiGuessView.data.planWeight = aiGuessSimulation.data.planWeight;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].planWeight =
		aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].planWeight;
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].valid = 
		aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]].plan.path.empty() ? false : true;

	//simulation
	bool success = mGameSimulation.SimulatePlayerDecision(aiGuessView.data, aiGuessSimulation.data,
		aiGuessView.guessPlayers[mPlayers[GV_HUMAN]], aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]], gameDecision.evaluation.threat, 
		gameDecision.evaluation.playerGuessItems, &mPlayerEvaluations[mPlayers[GV_HUMAN]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		mMutex.lock();

		PrintInfo("\nHuman Guessing Decision AI player guess output: ");
		PrintPlayerData(aiGuessSimulation.data);

		PrintInfo("\nHuman Guessing Decision Human player guess output: ");
		PrintPlayerData(aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]]);

		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = aiGuessSimulation.guessPlayers[mPlayers[GV_HUMAN]];
		aiGuessView.threat = gameDecision.evaluation.threat;

		SetPlayerOutput(gameDecision.evaluation.playerOutput, aiGuessView.simulation);

		SetPlayerOutput(gameDecision.evaluation.playerGuessOutput, aiGuessView.guessSimulations[mPlayers[GV_HUMAN]]);
		SetPlayerOutput(gameDecision.evaluation.otherPlayerGuessOutput, aiGuessView.simulation);

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

	float playerPathWeightOffset = CalculatePathingWeight(playerView.data);
	float aiGuessPathWeightOffset = aiGuessView.data.planWeight > 0.f ? aiGuessView.data.planWeight : 0.f;

	//we need to advance the players path plan total time exactly what it takes the decision making algorithm to be executed (in sec)
	PlayerView playerDecisionSimulation = playerView;
	playerDecisionSimulation.data.planWeight = 0.24f;
	playerDecisionSimulation.data.planWeight += playerPathWeightOffset;
	playerDecisionSimulation.data.plan.weight = playerPathWeightOffset;
	UpdatePlayerState(playerDecisionSimulation);
	playerDecisionSimulation.data.planWeight = 0.f;
	for (PathingArc* playerSimulationArc : playerDecisionSimulation.data.plan.path)
		playerDecisionSimulation.data.planWeight += playerSimulationArc->GetWeight();
	playerDecisionSimulation.data.planWeight -= playerPathWeightOffset;

	//we need to advance the opponents path plan total time only to the otherplayer's arc target position
	PlayerGuessView aiGuessSimulation = aiGuessView;
	aiGuessSimulation.data.planWeight = 0.f;
	aiGuessSimulation.data.planWeight += aiGuessPathWeightOffset;
	aiGuessSimulation.data.plan.weight = aiGuessPathWeightOffset;
	UpdatePlayerGuessState(aiGuessSimulation);
	aiGuessSimulation.data.planWeight = 0.f;
	for (PathingArc* aiGuessSimulationArc : aiGuessSimulation.data.plan.path)
		aiGuessSimulation.data.planWeight += aiGuessSimulationArc->GetWeight();
	aiGuessSimulation.data.planWeight -= aiGuessPathWeightOffset;

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
	std::map<ActorId, float> gameItems = playerView.gameItems;

	if (playerDecisionSimulation.data.plan.node)
	{
		// exclude items which are guessed to be taken by the human player
		for (auto const& humanGuessItem : playerView.data.items)
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(humanGuessItem.first);
			if (itemPickup)
				if (!playerDecisionSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[humanGuessItem.first] <= 0.f)
					gameItems[humanGuessItem.first] = humanGuessItem.second;
		}

		// update the items which are guessed to be taken by the ai player
		for (auto const& aiGuessItem : aiGuessView.guessItems[mPlayers[GV_AI]])
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(aiGuessItem.first);
			if (itemPickup)
				if (!playerDecisionSimulation.data.plan.node->IsVisibleNode(itemPickup->GetNode()) && gameItems[aiGuessItem.first] <= 0.f)
					gameItems[aiGuessItem.first] = aiGuessItem.second;
		}
	}

	gameDecision.evaluation.playerGuessItems = gameItems;
	gameDecision.evaluation.playerDecisionItems = gameItems;

	aiGuessView.data.ResetItems();
	aiGuessView.guessPlayers[mPlayers[GV_HUMAN]].ResetItems();
	aiGuessView.data.planWeight = aiGuessSimulation.data.planWeight;
	aiGuessView.data.valid = aiGuessSimulation.data.plan.path.empty() ? false : true;

	playerView.data.ResetItems();
	playerView.data.planWeight = playerDecisionSimulation.data.planWeight;
	playerView.data.valid = playerDecisionSimulation.data.plan.path.empty() ? false : true;

	PathingArcVec playerDecisionSimulationPath = playerDecisionSimulation.data.plan.path;

	//simulation
	bool success = mGameSimulation.SimulatePlayerDecision(playerView.data,
		playerDecisionSimulation.data, aiGuessView.data, aiGuessSimulation.data, gameDecision.evaluation.threat, 
		gameDecision.evaluation.playerDecisionItems, &mPlayerEvaluations[mPlayers[GV_HUMAN]], (EvaluationType)gameDecision.evaluation.type);
	if (success)
	{
		//advance the path plan for the human player
		for (PathingArc* playerDecisionSimulationPathArc : playerDecisionSimulationPath)
		{
			if (playerDecisionSimulation.data.planWeight <= 0.f)
				break;

			playerDecisionSimulation.data.plan.path.erase(playerDecisionSimulation.data.plan.path.begin());
			playerDecisionSimulation.data.plan.node = playerDecisionSimulationPathArc->GetNode();
			playerDecisionSimulation.data.planWeight -= playerDecisionSimulationPathArc->GetWeight();
		}

		mMutex.lock();

		PrintInfo("\nHuman Guessing AI player guess output: ");
		PrintPlayerData(aiGuessSimulation.data);

		PrintInfo("\nHuman Guessing Human player guess output: ");
		PrintPlayerData(playerDecisionSimulation.data);

		PrintInfo("\nHuman Decision Human player output: ");
		PrintPlayerData(playerDecisionSimulation.data);

		playerView.isUpdated = true;
		playerView.simulation = playerDecisionSimulation.data;
		playerView.threat = gameDecision.evaluation.threat;

		aiGuessView.isUpdated = true;
		aiGuessView.simulation = aiGuessSimulation.data;
		aiGuessView.guessSimulations[mPlayers[GV_HUMAN]] = playerDecisionSimulation.data;
		aiGuessView.threat = gameDecision.evaluation.threat;

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

void QuakeAIManager::RunAIDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_AI) == mPlayers.end())
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_GUESSING)
			{
				std::this_thread::yield();
				continue;
			}

			unsigned int time = Timer::GetRealTime();

			PlayerView aiView;
			if (MakeAIDecision(aiView))
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
				ss << "\n ai decision total elapsed time " << diffTime;// << " threat level " << aiView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration AI Decision %u", iteration);
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
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_GUESSING)
			{
				std::this_thread::yield();
				continue;
			}

			unsigned int time = Timer::GetRealTime();

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
				ss << "\n ai close guessing total elapsed time " << diffTime;// << " threat level " << aiView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];

				UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);  //update playerView
				UpdatePlayerSimulationView(mPlayers[GV_AI], playerGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration AI Guessing %u", iteration);
				//iteration++;
			}

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunAIGuessingDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_AI) == mPlayers.end())
			{
				std::this_thread::yield();
				continue;
			}

			unsigned int time = Timer::GetRealTime();

			PlayerView aiView;
			bool isAIGuessing = MakeAIGuessingDecision(aiView);
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
				ss << "\n ai guessing decision total elapsed time " << diffTime;// << " threat level " << aiView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];

				UpdatePlayerSimulationView(mPlayers[GV_AI], playerGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration AI Guessing %u", iteration);
				//iteration++;
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
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_AWARENESS)
			{
				std::this_thread::yield();
				continue;
			}

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
				ss << "\n ai aware decision total elapsed time " << diffTime;// << " threat level " << aiView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& playerGuessView = aiView.guessViews[mPlayers[GV_HUMAN]];

				UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);  //update playerView
				UpdatePlayerSimulationView(mPlayers[GV_AI], playerGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration AI Aware Decision %u", iteration);
				//iteration++;
			}

			//after complete execution we run guessing decision making
			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) == ET_CLOSEGUESSING)
				mPlayerEvaluations[mPlayers.at(GV_AI)] = ET_GUESSING;

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunAIAwareness()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_AI) == mPlayers.end())
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) != ET_AWARENESS)
			{
				std::this_thread::yield();
				continue;
			}

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
				ss << "\n ai aware total elapsed time " << diffTime;// << " threat level " << aiView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				UpdatePlayerSimulationView(mPlayers[GV_AI], aiView);  //update playerView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration AI Aware Decision %u", iteration);
				//iteration++;
			}

			//after complete execution we run guessing decision making
			if (mPlayerEvaluations.at(mPlayers.at(GV_AI)) == ET_CLOSEGUESSING)
				mPlayerEvaluations[mPlayers.at(GV_AI)] = ET_GUESSING;

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunHumanDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_HUMAN) == mPlayers.end())
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_GUESSING)
			{
				std::this_thread::yield();
				continue;
			}

			unsigned int time = Timer::GetRealTime();

			PlayerView playerView;
			if (MakeHumanDecision(playerView))
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
				ss << "\n human decision total elapsed time " << diffTime;// << " threat level " << playerView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView);

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
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
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_GUESSING)
			{
				std::this_thread::yield();
				continue;
			}

			unsigned int time = Timer::GetRealTime();

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
				ss << "\n human close guessing total elapsed time " << diffTime;// << " threat level " << playerView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView); //update playerView
				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], aiGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration Human Guessing %u", iteration);
				//iteration++;
			}
			
			SetEnable(true);
		}
	}
}


void QuakeAIManager::RunHumanGuessingDecision()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_HUMAN) == mPlayers.end())
			{
				std::this_thread::yield();
				continue;
			}

			unsigned int time = Timer::GetRealTime();

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
				ss << "\n human guessing decision total elapsed time " << diffTime;// << " threat level " << playerView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], aiGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration Human Guessing %u", iteration);
				//iteration++;
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
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_AWARENESS)
			{
				std::this_thread::yield();
				continue;
			}

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
				ss << "\n human aware decision total elapsed time " << diffTime;// << " threat level " << playerView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				PlayerGuessView& aiGuessView = playerView.guessViews[mPlayers[GV_AI]];

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView); //update playerView
				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], aiGuessView); //update guessView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration Human Aware Decision %u", iteration);
				//iteration++;
			}

			//after complete execution we run guessing decision making
			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) == ET_CLOSEGUESSING)
				mPlayerEvaluations[mPlayers.at(GV_HUMAN)] = ET_GUESSING;

			SetEnable(true);
		}
	}
}

void QuakeAIManager::RunHumanAwareness()
{
	unsigned int iteration = 0;

	while (true)
	{
		if (GameLogic::Get()->GetState() == BGS_RUNNING)
		{
			if (mPlayers.find(GV_HUMAN) == mPlayers.end())
			{
				std::this_thread::yield();
				continue;
			}

			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) != ET_AWARENESS)
			{
				std::this_thread::yield();
				continue;
			}

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
				ss << "\n human aware total elapsed time " << diffTime;// << " threat level " << playerView.threat;
				PrintInfo(ss.str());
				printf(ss.str().c_str());

				UpdatePlayerSimulationView(mPlayers[GV_HUMAN], playerView); //update playerView

				//lets wait to give some time for the AI Manager and AI Views update its status
				Timer::Sleep(30);
				//printf("\n Iteration Human Aware Decision %u", iteration);
				//iteration++;
			}

			//after complete execution we run guessing decision making
			if (mPlayerEvaluations.at(mPlayers.at(GV_HUMAN)) == ET_CLOSEGUESSING)
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

	playerData.planWeight = playerInput.planWeight;
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

	playerData.planWeight = playerInput.planWeight;
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

	playerInput.planWeight = playerDataOffset.planWeight;
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

void QuakeAIManager::UpdatePlayerView(ActorId player, float planWeight)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].data.planWeight = planWeight;
	mPlayerViewMutex[player].unlock();
}

void QuakeAIManager::UpdatePlayerView(ActorId player, const std::map<ActorId, float>& gameItems)
{
	mPlayerViewMutex[player].lock();
	mPlayerViews[player].gameItems = gameItems;
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
	QuakeLogic* game = static_cast<QuakeLogic *>(GameLogic::Get());

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(playerId).lock()));
	if (pPlayerActor)
	{
		PlayerView playerView;
		playerView.isUpdated = false;
		GetPlayerView(pPlayerActor->GetId(), playerView);

		//initialize game items
		InitializePlayerItems(playerView);

		std::shared_ptr<PhysicComponent> pPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (pPhysicComponent)
		{
			playerView.data = PlayerData(pPlayerActor);
			playerView.simulation = PlayerData(pPlayerActor);

			if (mPathingGraph)
			{
				PathingNode* spawnNode = mPathingGraph->FindClosestNode(pPhysicComponent->GetPosition(), false);
				playerView.data.plan = NodePlan(spawnNode, PathingArcVec());
				playerView.data.ResetItems();

				//assuming the guessing players has no idea where our player is located, lets take a random spawn spot
				Transform spawnTransform;
				game->SelectRandomFurthestSpawnPoint(spawnNode->GetPosition(), spawnTransform, false);
				spawnNode = mPathingGraph->FindClosestNode(spawnTransform.GetTranslation(), false);

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

					playerGuessView.guessPlayers[pOtherPlayerActor->GetId()] = PlayerData(pOtherPlayerActor);
					playerGuessView.guessSimulations[pOtherPlayerActor->GetId()] = PlayerData(pOtherPlayerActor);

					playerGuessView.data.plan = NodePlan(spawnNode, PathingArcVec());
					playerGuessView.simulation.plan = NodePlan(spawnNode, PathingArcVec());

					//what the guessing player is guessing about the other.
					Transform guessSpawnTransform;
					game->SelectRandomFurthestSpawnPoint(spawnNode->GetPosition(), guessSpawnTransform, false);
					PathingNode* guessSpawnNode = mPathingGraph->FindClosestNode(guessSpawnTransform.GetTranslation(), false);
					playerGuessView.guessPlayers[pPlayerActor->GetId()].plan = NodePlan(guessSpawnNode, PathingArcVec());
					playerGuessView.guessSimulations[pPlayerActor->GetId()].plan = NodePlan(guessSpawnNode, PathingArcVec());

					Transform otherGuessSpawnTransform;
					game->SelectRandomFurthestSpawnPoint(spawnNode->GetPosition(), otherGuessSpawnTransform, false);
					PathingNode* otherGuessSpawnNode = mPathingGraph->FindClosestNode(otherGuessSpawnTransform.GetTranslation(), false);
					playerGuessView.guessPlayers[pOtherPlayerActor->GetId()].plan = NodePlan(otherGuessSpawnNode, PathingArcVec());
					playerGuessView.guessSimulations[pOtherPlayerActor->GetId()].plan = NodePlan(otherGuessSpawnNode, PathingArcVec());

					//initialize game items
					InitializePlayerGuessItems(playerGuessView, pPlayerActor->GetId());

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

		std::shared_ptr<PhysicComponent> pPlayerPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		PathingNode* playerNode = mPathingGraph->FindClosestNode(pPlayerPhysicComponent->GetPosition(), false);

		//if the noise is detected within a range then we reset the guess players status.
		std::stringstream playerInfo;
		playerInfo << "\n ITEM NOISE MADE by player: " << pPlayerActor->GetId() << " ";
		PrintInfo(playerInfo.str());

		playerGuessView.isUpdated = false;
		playerGuessView.items[pPlayerActor->GetId()].clear();

		//we update the players path plan based on current position.
		UpdatePlayerGuessPlan(pPlayerActor, aiViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);

		//initialize player guess items
		InitializePlayerGuessItems(playerGuessView, pPlayerActor->GetId());
		UpdatePlayerGuessView(pPlayerActor->GetId(), playerGuessView, true);

		//we also update the other player's guess view about this player in case that the noise is within range.
		PlayerView otherPlayerView;
		GetPlayerView(pOtherPlayerActor->GetId(), otherPlayerView);

		if (!otherPlayerView.data.plan.node)
			continue;

		// take into consideration within a certain radius
		if (Length(pActorTransform->GetPosition() - otherPlayerView.data.plan.node->GetPosition()) > 700.f)
			continue;

		if (otherPlayerView.guessViews.find(pPlayerActor->GetId()) == otherPlayerView.guessViews.end())
			continue;

		PlayerGuessView& otherPlayerGuessView = otherPlayerView.guessViews[pPlayerActor->GetId()];
		otherPlayerGuessView.isUpdated = false;
		otherPlayerGuessView.items[pPlayerActor->GetId()].clear();

		//initialize player items
		InitializePlayerItems(otherPlayerView);
		UpdatePlayerView(pOtherPlayerActor->GetId(), otherPlayerView.gameItems);

		//we update the players path plan based on current position. This is actually not right and it should be predicted
		UpdatePlayerGuessPlan(pPlayerActor, aiViews[pPlayerActor->GetId()], otherPlayerGuessView.data, playerNode);

		//we update the players path plan based on current position.
		UpdatePlayerGuessPlan(pPlayerActor, aiViews[pPlayerActor->GetId()], otherPlayerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);

		//initialize player guess items
		InitializePlayerGuessItems(otherPlayerGuessView, pPlayerActor->GetId());
		UpdatePlayerGuessView(pOtherPlayerActor->GetId(), otherPlayerGuessView, true);
	}
}

void QuakeAIManager::DetectPlayer(std::shared_ptr<PlayerActor> pPlayerActor)
{
	GameApplication* gameApp = (GameApplication*)Application::App;
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

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
		std::shared_ptr<PhysicComponent> pPlayerPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (Length(pPlayerPhysicComponent->GetPosition() - playerGuessView.data.plan.node->GetPosition()) > 700.f)
			continue;
		PathingNode* playerNode = mPathingGraph->FindClosestNode(pPlayerPhysicComponent->GetPosition(), false);

		//if the noise is detected within a range then we reset the guess players status.
		std::stringstream playerInfo;
		playerInfo << "\n NOISE MADE by player: " << pPlayerActor->GetId() << " ";
		PrintInfo(playerInfo.str());

		playerGuessView.isUpdated = false;
		playerGuessView.items[pPlayerActor->GetId()].clear();

		//we update the players path plan based on current position.
		UpdatePlayerGuessPlan(pPlayerActor, aiViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);

		//initialize player guess items
		InitializePlayerGuessItems(playerGuessView, pPlayerActor->GetId());
		UpdatePlayerGuessView(pPlayerActor->GetId(), playerGuessView, true);

		//we also update the other player's guess view about this player in case that the noise is within range.
		PlayerView otherPlayerView;
		GetPlayerView(pOtherPlayerActor->GetId(), otherPlayerView);

		if (!otherPlayerView.data.plan.node)
			continue;

		// take into consideration within a certain radius
		if (Length(pPlayerPhysicComponent->GetPosition() - otherPlayerView.data.plan.node->GetPosition()) > 700.f)
			continue;

		if (otherPlayerView.guessViews.find(pPlayerActor->GetId()) == otherPlayerView.guessViews.end())
			continue;

		PlayerGuessView& otherPlayerGuessView = otherPlayerView.guessViews[pPlayerActor->GetId()];
		otherPlayerGuessView.isUpdated = false;
		otherPlayerGuessView.items[pPlayerActor->GetId()].clear();

		//initialize player items
		InitializePlayerItems(otherPlayerView);
		UpdatePlayerView(pOtherPlayerActor->GetId(), otherPlayerView.gameItems);

		//we update the players path plan based on current position. This is actually not right and it should be predicted
		UpdatePlayerGuessPlan(pPlayerActor, aiViews[pPlayerActor->GetId()], otherPlayerGuessView.data, playerNode);

		//we update the players path plan based on current position.
		UpdatePlayerGuessPlan(pPlayerActor, aiViews[pPlayerActor->GetId()], otherPlayerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);

		//initialize player guess items
		InitializePlayerGuessItems(otherPlayerGuessView, pPlayerActor->GetId());
		UpdatePlayerGuessView(pOtherPlayerActor->GetId(), otherPlayerGuessView, true);
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

void QuakeAIManager::UpdatePlayerGuessPlan(std::shared_ptr<PlayerActor> playerActor, 
	const PlayerData& playerData, PlayerData& playerGuessData, PathingNode* playerNode)
{
	if (playerData.plan.node)
	{
		//we take the current player plan
		playerGuessData = PlayerData(playerActor);
		playerGuessData.plan = playerData.plan;
		playerGuessData.planWeight = CalculatePathingWeight(playerGuessData);

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
					playerGuessView.items[playerId][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
				else if (pItemActor->GetType() == "Ammo")
				{
					std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
					playerGuessView.guessPlayers[playerId].ammo[pAmmoPickup->GetCode()] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.items[playerId][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
				else if (pItemActor->GetType() == "Armor")
				{
					std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
					playerGuessView.guessPlayers[playerId].stats[STAT_ARMOR] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.items[playerId][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerId][itemId] -= playerGuessView.guessPlayers[playerId].planWeight;
				}
				else if (pItemActor->GetType() == "Health")
				{
					std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
					playerGuessView.guessPlayers[playerId].stats[STAT_HEALTH] += 
						playerGuessView.guessPlayers[playerId].itemAmount[itemId];

					playerGuessView.guessPlayers[playerId].items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
					playerGuessView.items[playerId][itemId] = (float)pHealthPickup->GetWait() / 1000.f;
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

	playerGuessView.guessPlayers[playerId].weaponTime -= deltaMs / 1000.f;
	if (playerGuessView.guessPlayers[playerId].weaponTime < 0)
		playerGuessView.guessPlayers[playerId].weaponTime = 0.f;
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
					playerGuessView.items[playerGuessView.data.player][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pWeaponPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
				else if (pItemActor->GetType() == "Ammo")
				{
					std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
					playerGuessView.data.ammo[pAmmoPickup->GetCode()] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.items[playerGuessView.data.player][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pAmmoPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
				else if (pItemActor->GetType() == "Armor")
				{
					std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
					playerGuessView.data.stats[STAT_ARMOR] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.items[playerGuessView.data.player][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] = (float)pArmorPickup->GetWait() / 1000.f;
					playerGuessView.guessItems[playerGuessView.data.player][itemId] -= playerGuessView.data.planWeight;
				}
				else if (pItemActor->GetType() == "Health")
				{
					std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
					playerGuessView.data.stats[STAT_HEALTH] += playerGuessView.data.itemAmount[itemId];

					playerGuessView.data.items[itemId] = (float)pHealthPickup->GetWait() / 1000.f;
					playerGuessView.items[playerGuessView.data.player][itemId] = (float)pHealthPickup->GetWait() / 1000.f;
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

	playerGuessView.data.weaponTime -= deltaMs / 1000.f;
	if (playerGuessView.data.weaponTime < 0)
		playerGuessView.data.weaponTime = 0.f;
}

bool QuakeAIManager::CheckPlayerGuessItems(PathingNode* playerNode, PlayerGuessView& playerGuessView)
{
	bool resetGuessItem = false;
	if (playerGuessView.data.items.find(playerGuessView.data.plan.node->GetActorId()) != playerGuessView.data.items.end())
	{
		if (playerGuessView.data.items[playerGuessView.data.plan.node->GetActorId()] > 0.f)
			return resetGuessItem;

		std::shared_ptr<Actor> pItemActor(
			GameLogic::Get()->GetActor(playerGuessView.data.plan.node->GetActorId()).lock());
		std::shared_ptr<TransformComponent> pItemTransform(
			pItemActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		PathingNode* itemNode = mPathingGraph->FindClosestNode(pItemTransform->GetTransform().GetTranslation(), false);

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

		std::shared_ptr<Actor> pItemActor(
			GameLogic::Get()->GetActor(playerGuessView.data.plan.node->GetActorId()).lock());
		std::shared_ptr<TransformComponent> pItemTransform(
			pItemActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		PathingNode* itemNode = mPathingGraph->FindClosestNode(pItemTransform->GetTransform().GetTranslation(), false);
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

void QuakeAIManager::InitializePlayerItems(PlayerView& playerView)
{
	//for the moment we take perfect information but the goal is to have
	//an accurate system to predict items availability and respawning time estimation
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

void QuakeAIManager::InitializePlayerGuessItems(PlayerGuessView& playerGuessView, ActorId playerId)
{
	//for the moment we take perfect information but the goal is to have
	//an accurate system to predict items availability and respawning time estimation
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
			playerGuessView.guessItems[playerId][pItemActor->GetId()] = pWeaponPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
			playerGuessView.guessItems[playerId][pItemActor->GetId()] = pAmmoPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
			playerGuessView.guessItems[playerId][pItemActor->GetId()] = pArmorPickup->mRespawnTime / 1000.f;
		}
		else if (pItemActor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
			playerGuessView.guessItems[playerId][pItemActor->GetId()] = pHealthPickup->mRespawnTime / 1000.f;
		}
	}
}

void QuakeAIManager::UpdatePlayerItems(unsigned long deltaMs, PathingNode* playerNode, PlayerView& playerView)
{
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);
	for (std::shared_ptr<Actor> pItemActor : searchActors)
	{
		if (mGameSimulation.mGameActorPickups.find(pItemActor->GetId()) != mGameSimulation.mGameActorPickups.end())
		{
			const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(pItemActor->GetId());
			if (playerNode->IsVisibleNode(itemPickup->GetNode()))
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
			else
			{
				playerView.gameItems[pItemActor->GetId()] -= deltaMs / 1000.f;
				if (playerView.gameItems[pItemActor->GetId()] < 0)
					playerView.gameItems[pItemActor->GetId()] = 0.f;
			}
		}
	}
}

void QuakeAIManager::UpdatePlayerGuessItems(unsigned long deltaMs, PlayerGuessView& playerGuessView)
{
	QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());

	std::vector<std::shared_ptr<Actor>> searchActors;
	game->GetAmmoActors(searchActors);
	game->GetWeaponActors(searchActors);
	game->GetHealthActors(searchActors);
	game->GetArmorActors(searchActors);

	for (auto guessPlayer : playerGuessView.guessPlayers)
	{
		for (std::shared_ptr<Actor> pItemActor : searchActors)
		{
			if (mGameSimulation.mGameActorPickups.find(pItemActor->GetId()) != mGameSimulation.mGameActorPickups.end())
			{
				const AIAnalysis::ActorPickup* itemPickup = mGameSimulation.mGameActorPickups.at(pItemActor->GetId());
				if (guessPlayer.second.plan.node->IsVisibleNode(itemPickup->GetNode()))
				{
					if (pItemActor->GetType() == "Weapon")
					{
						std::shared_ptr<WeaponPickup> pWeaponPickup = pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
						playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] = pWeaponPickup->mRespawnTime / 1000.f;
					}
					else if (pItemActor->GetType() == "Ammo")
					{
						std::shared_ptr<AmmoPickup> pAmmoPickup = pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
						playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] = pAmmoPickup->mRespawnTime / 1000.f;
					}
					else if (pItemActor->GetType() == "Armor")
					{
						std::shared_ptr<ArmorPickup> pArmorPickup = pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
						playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] = pArmorPickup->mRespawnTime / 1000.f;
					}
					else if (pItemActor->GetType() == "Health")
					{
						std::shared_ptr<HealthPickup> pHealthPickup = pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
						playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] = pHealthPickup->mRespawnTime / 1000.f;
					}
				}
				else
				{
					playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] -= deltaMs / 1000.f;
					if (playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] < 0)
						playerGuessView.guessItems[guessPlayer.first][pItemActor->GetId()] = 0.f;
				}
			}
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
		{
			SavePlayerView(pPlayerActor->GetId(), PlayerView());
			continue;
		}

		std::shared_ptr<PhysicComponent> pPlayerPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		PathingNode* playerNode = mPathingGraph->FindClosestNode(pPlayerPhysicComponent->GetPosition(), false);

		PlayerView playerView;
		GetPlayerView(pPlayerActor->GetId(), playerView);
		playerView.data.planWeight += deltaMs / 1000.f;
		UpdatePlayerView(pPlayerActor->GetId(), playerView.data.planWeight);

		//update player items
		UpdatePlayerItems(deltaMs, playerNode, playerView);
		UpdatePlayerView(pPlayerActor->GetId(), playerView.gameItems);

		//aware decision making
		bool runAwareDecision = false;

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

				//update to player guess simulation
				if (playerGuessView.data.plan.path.empty() || playerGuessView.threat > 3)
				//if (playerGuessView.data.plan.id != -1)
				{
					playerGuessView.data.plan = playerGuessView.simulation.plan;
					playerGuessView.data.planWeight = playerGuessView.simulation.planWeight;

					playerGuessView.data.ResetItems();
					for (auto const& playerGuessItem : playerGuessView.simulation.items)
					{
						if (playerGuessView.items[pOtherPlayerActor->GetId()].find(playerGuessItem.first) == playerGuessView.items[pOtherPlayerActor->GetId()].end())
						{
							playerGuessView.data.items[playerGuessItem.first] = playerGuessItem.second;
							playerGuessView.data.itemWeight[playerGuessItem.first] = playerGuessView.simulation.itemWeight[playerGuessItem.first];
							playerGuessView.data.itemAmount[playerGuessItem.first] = playerGuessView.simulation.itemAmount[playerGuessItem.first];
						}
					}
				}

				if (playerGuessView.data.plan.id != -1 && playerGuessView.data.IsWeaponSelectable(playerGuessView.simulation.weapon))
					playerGuessView.data.weapon = playerGuessView.simulation.weapon;

				if (playerGuessView.guessSimulations[pPlayerActor->GetId()].plan.id != -1)
				{
					//update player guess simulation
					playerGuessView.guessPlayers[pPlayerActor->GetId()].plan = playerGuessView.guessSimulations[pPlayerActor->GetId()].plan;
					playerGuessView.guessPlayers[pPlayerActor->GetId()].planWeight = playerGuessView.guessSimulations[pPlayerActor->GetId()].planWeight;

					playerGuessView.guessPlayers[pPlayerActor->GetId()].ResetItems();
					for (auto const& playerGuessItem : playerGuessView.guessSimulations[pPlayerActor->GetId()].items)
					{
						if (playerGuessView.items[pPlayerActor->GetId()].find(playerGuessItem.first) == playerGuessView.items[pPlayerActor->GetId()].end())
						{
							playerGuessView.guessPlayers[pPlayerActor->GetId()].items[playerGuessItem.first] = playerGuessItem.second;
							playerGuessView.guessPlayers[pPlayerActor->GetId()].itemWeight[playerGuessItem.first] = 
								playerGuessView.guessSimulations[pPlayerActor->GetId()].itemWeight[playerGuessItem.first];
							playerGuessView.guessPlayers[pPlayerActor->GetId()].itemAmount[playerGuessItem.first] = 
								playerGuessView.guessSimulations[pPlayerActor->GetId()].itemAmount[playerGuessItem.first];
						}
					}
					if (playerGuessView.guessPlayers[pPlayerActor->GetId()].IsWeaponSelectable(playerGuessView.guessSimulations[pPlayerActor->GetId()].weapon))
						playerGuessView.guessPlayers[pPlayerActor->GetId()].weapon = playerGuessView.guessSimulations[pPlayerActor->GetId()].weapon;
				}
			}

			if (playerNode)
			{
				bool resetGuessItem = CheckPlayerGuessItems(playerNode, playerGuessView, pPlayerActor->GetId());

				std::shared_ptr<PhysicComponent> pOtherPlayerPhysicComponent(
					pOtherPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
				if (pOtherPlayerPhysicComponent)
				{
					PathingNode* otherPlayerNode = mPathingGraph->FindClosestNode(pOtherPlayerPhysicComponent->GetPosition(), false);
					bool resetOtherGuessItem = CheckPlayerGuessItems(otherPlayerNode, playerGuessView);

					if (playerNode->IsVisibleNode(otherPlayerNode))
						//if (RayCollisionDetection(currentPosition, pOtherPlayerPhysicComponent->GetPosition()) == NULL)
					{
						//distrust the guessing plan and reset guess player
						playerGuessView.isUpdated = false;
						playerGuessView.items[pPlayerActor->GetId()].clear();
						playerGuessView.items[pOtherPlayerActor->GetId()].clear();

						//initialize player items
						InitializePlayerItems(playerView);
						UpdatePlayerView(pPlayerActor->GetId(), playerView.gameItems);

						//initialize player guess items
						InitializePlayerGuessItems(playerGuessView, pPlayerActor->GetId());
						InitializePlayerGuessItems(playerGuessView, pOtherPlayerActor->GetId());

						std::stringstream updatePlayer;
						updatePlayer << "\n visible nodes for both players ";
						PrintInfo(updatePlayer.str());

						UpdatePlayerGuessPlan(pOtherPlayerActor,
							gameAIViews[pOtherPlayerActor->GetId()], playerGuessView.data, otherPlayerNode);

						//what the guessing player is guessing about the player
						UpdatePlayerGuessPlan(pPlayerActor,
							gameAIViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);

						//if players can see each other, then we run aware decision making
						runAwareDecision = true;
					}
					else
					{
						if (resetGuessItem)
						{
							//distrust the guessing plan and reset guess player 
							playerGuessView.isUpdated = false;
							playerGuessView.items[pPlayerActor->GetId()].clear();

							//initialize player items
							InitializePlayerItems(playerView);
							UpdatePlayerView(pPlayerActor->GetId(), playerView.gameItems);

							//initialize player guess items
							InitializePlayerGuessItems(playerGuessView, pPlayerActor->GetId());

							std::stringstream updatePlayer;
							updatePlayer << "\n reset items for player guess: " << pPlayerActor->GetId() << " ";
							PrintInfo(updatePlayer.str());

							UpdatePlayerGuessPlan(pOtherPlayerActor,
								gameAIViews[pOtherPlayerActor->GetId()], playerGuessView.data, otherPlayerNode);

							//we update the players path plan based on current position. This is actually not right and it should be predicted
							UpdatePlayerGuessPlan(pPlayerActor,
								gameAIViews[pPlayerActor->GetId()], playerGuessView.guessPlayers[pPlayerActor->GetId()], playerNode);
						}
						else if (playerNode->IsVisibleNode(playerGuessView.data.plan.node))
							//else if(RayCollisionDetection(currentPosition, playerGuessView.data.plan.node->GetPosition()) == NULL)
						{
							//distrust the guessing plan and reset guess player node
							playerGuessView.isUpdated = false;
							playerGuessView.items[pOtherPlayerActor->GetId()].clear();

							//initialize player items
							InitializePlayerItems(playerView);
							UpdatePlayerView(pPlayerActor->GetId(), playerView.gameItems);

							//initialize player guess items
							InitializePlayerGuessItems(playerGuessView, pOtherPlayerActor->GetId());

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
							playerGuessView.items[pOtherPlayerActor->GetId()].clear();

							//initialize player guess items
							InitializePlayerGuessItems(playerGuessView, pOtherPlayerActor->GetId());

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
							playerGuessView.items[pPlayerActor->GetId()].clear();

							//initialize player guess items
							InitializePlayerGuessItems(playerGuessView, pPlayerActor->GetId());

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

			//update player guess
			UpdatePlayerGuessState(deltaMs, playerGuessView);
			UpdatePlayerGuessState(deltaMs, playerGuessView, pPlayerActor->GetId());

			//update guess items
			UpdatePlayerGuessItems(deltaMs, playerGuessView);

			UpdatePlayerGuessView(pPlayerActor->GetId(), playerGuessView, isPlayerGuessUpdated);
		}

		if (runAwareDecision)
			mPlayerEvaluations[pPlayerActor->GetId()] = ET_AWARENESS;
		else if (mPlayerEvaluations.at(pPlayerActor->GetId()) == ET_AWARENESS)
			mPlayerEvaluations[pPlayerActor->GetId()] = ET_CLOSEGUESSING;
	}

	mUpdateTimeMs += deltaMs;
	if (mUpdateTimeMs < 200)
	{
		AIGame::EventTrack eventTrack;
		eventTrack.elapsedTime = (float)deltaMs;
		AddGameEventTrack(eventTrack);
	}
	else
	{
		mUpdateTimeMs -= 200;

		//log ai guessing system
		LogEvents(deltaMs);
	}
}

void QuakeAIManager::LogEvents(unsigned long deltaMs)
{
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
			Transform actorTransform = transformComponent->GetTransform();

			EulerAngles<float> viewAngles;
			viewAngles.mAxis[1] = 1;
			viewAngles.mAxis[2] = 2;
			actorTransform.GetRotation(viewAngles);
			float yaw = viewAngles.mAngle[AXIS_Y];
			float pitch = viewAngles.mAngle[AXIS_Z];
			Vector3<float> position = actorTransform.GetTranslation();

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
		std::shared_ptr<PhysicComponent> pPhysicComponent(
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());

		PlayerView playerView;
		GetPlayerView(pPlayerActor->GetId(), playerView);

		EulerAngles<float> viewAngles;
		viewAngles.mAxis[1] = 1;
		viewAngles.mAxis[2] = 2;
		pTransformComponent->GetTransform().GetRotation(viewAngles);
		Vector3<float> position = pPhysicComponent->GetPosition();
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