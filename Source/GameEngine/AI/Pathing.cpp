//========================================================================
// Pathing.cpp : Implements a simple pathing system using A*
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

 
#include "Pathing.h"

#include "Core/OS/OS.h"

//--------------------------------------------------------------------------------------------------------
// Cluster
//--------------------------------------------------------------------------------------------------------

void Cluster::RemoveNode(PathingNode* pNode)
{
	LogAssert(pNode, "Invalid node");
	mNodes.erase(pNode->GetId());
}

void Cluster::AddNode(PathingNode* pNode)
{
	LogAssert(pNode, "Invalid node");
	mNodes[pNode->GetId()] = pNode;
}

const PathingNodeMap& Cluster::GetNodes()
{
	return mNodes;
}

void Cluster::RemoveNodeActor(ActorId actorId)
{
	mNodeActors.erase(actorId);
}

void Cluster::AddNodeActor(ActorId actorId, PathingNode* pNode)
{
	mNodeActors[actorId] = pNode;
}

const PathingNodeMap& Cluster::GetNodeActors()
{
	return mNodeActors;
}

PathingNode* Cluster::FindNodeActor(ActorId actorId)
{
	if (mNodeActors.find(actorId) != mNodeActors.end())
		return mNodeActors[actorId];

	return NULL;
}

void Cluster::RemoveVisibleCluster(unsigned int cluster)
{
	mVisibleClusters.erase(cluster);
}

void Cluster::AddVisibleCluster(unsigned int cluster, PathingNode* pNode)
{
	mVisibleClusters[cluster] = pNode;
}

PathingNode* Cluster::FindVisibleCluster(unsigned int cluster)
{
	if (mVisibleClusters.find(cluster) != mVisibleClusters.end())
		return mVisibleClusters[cluster];

	return NULL;
}

bool Cluster::IsVisibleCluster(unsigned int cluster)
{
	return mVisibleClusters.find(cluster) != mVisibleClusters.end();
}

const PathingNodeMap& Cluster::GetVisibileClusters()
{
	return mVisibleClusters;
}

//--------------------------------------------------------------------------------------------------------
// PathingCluster
//--------------------------------------------------------------------------------------------------------

void PathingCluster::LinkClusters(PathingNode* pNode, PathingNode* pTarget)
{
	LogAssert(pNode, "Invalid node");
	LogAssert(pTarget, "Invalid node");

	mNode = pNode;
	mTarget = pTarget;
}

//--------------------------------------------------------------------------------------------------------
// PathingActor
//--------------------------------------------------------------------------------------------------------

void PathingActor::LinkActors(PathingNode* pNode, PathingNode* pTarget)
{
	LogAssert(pNode, "Invalid node");
	LogAssert(pTarget, "Invalid node");

	mNode = pNode;
	mTarget = pTarget;
}

//--------------------------------------------------------------------------------------------------------
// PathingNode
//--------------------------------------------------------------------------------------------------------

void PathingNode::RemoveVisibleNode(PathingNode* pNode)
{
	mVisibleNodes.erase(pNode);
}

void PathingNode::AddVisibleNode(PathingNode* pNode, float value)
{
	mVisibleNodes[pNode] = value;
}

bool PathingNode::IsVisibleNode(PathingNode* pNode)
{
	return mVisibleNodes.find(pNode) != mVisibleNodes.end();
}

const std::unordered_map<PathingNode*, float>& PathingNode::GetVisibileNodes()
{
	return mVisibleNodes;
}

void PathingNode::AddArc(PathingArc* pArc)
{
	LogAssert(pArc, "Invalid arc");
	mArcs[pArc->GetNode()->GetId()] = pArc;
}

void PathingNode::GetArcs(unsigned int arcType, PathingArcVec& outArcs)
{
	if (arcType == AT_NORMAL)
	{
		for (auto const& arc : mArcs)
		{
			PathingArc* pArc = arc.second;
			if (pArc->GetType() == AT_NORMAL)
				outArcs.push_back(pArc);
		}
	}
	else
	{
		for (auto const& arc : mArcs)
		{
			PathingArc* pArc = arc.second;
			if (pArc->GetType() & arcType)
				outArcs.push_back(pArc);
		}
	}
}

PathingArc* PathingNode::FindArc(unsigned int id)
{
	for (PathingArcMap::iterator itArc = mArcs.begin(); itArc != mArcs.end(); ++itArc)
	{
		PathingArc* pArc = itArc->second;
		if (pArc->GetId() == id)
			return pArc;
	}
	return NULL;
}

PathingArc* PathingNode::FindArc(PathingNode* pLinkedNode)
{
	LogAssert(pLinkedNode, "Invalid node");
	PathingArcMap::iterator itArc = mArcs.find(pLinkedNode->GetId());
	return itArc != mArcs.end() ? (itArc->second) : NULL;
}

void PathingNode::RemoveArcs()
{
	for (PathingArcMap::iterator itArc = mArcs.begin(); itArc != mArcs.end(); ++itArc)
	{
		PathingArc* pArc = itArc->second;
		delete pArc;
	}
	mArcs.clear();
}

void PathingNode::RemoveArc(PathingNode* pNode)
{
	LogAssert(pNode, "Invalid node");
	PathingArcMap::iterator itArc = mArcs.find(pNode->GetId());
	if (itArc != mArcs.end())
	{
		PathingArc* pArc = itArc->second;
		mArcs.erase(itArc);
		delete pArc;
	}
}

bool PathingNode::RemoveArc(unsigned int id)
{
	for (PathingArcMap::iterator it = mArcs.begin(); it != mArcs.end(); ++it)
	{
		PathingArc* pArc = it->second;
		if (pArc->GetId() == id)
		{
			mArcs.erase(it);
			delete pArc;
			return true;
		}
	}

	return false;
}

void PathingNode::AddCluster(PathingCluster* pCluster)
{
	LogAssert(pCluster, "Invalid cluster");

	unsigned int clusterId = pCluster->GetTarget()->GetId() << 16 | pCluster->GetType();
	mClusters[clusterId] = pCluster;
}

void PathingNode::GetClusters(unsigned int pathingType, PathingClusterVec& outClusters)
{
	for (auto const& pathCluster : mClusters)
		if (pathCluster.second->GetType() == pathingType)
			outClusters.push_back(pathCluster.second);
}

void PathingNode::GetClusters(unsigned int pathingType,
	unsigned int clusterLimit, std::map<PathingCluster*, PathingArcVec>& clusterPaths, 
	std::multimap<float, PathingCluster*, std::greater<float>>& clusterPathWeights)
{
	std::multimap<float, PathingCluster*> clusterPathWeightsLimit;
	std::map<PathingCluster*, PathingArcVec> clusterPathsLimit;
	for (auto const& pathingCluster : mClusters)
	{
		if (pathingCluster.second->GetType() != pathingType)
			continue;

		float clusterPathWeight = 0.f;
		PathingNode* currentNode = this;
		while (currentNode != pathingCluster.second->GetTarget())
		{
			PathingCluster* currentCluster = currentNode->FindCluster(pathingType, pathingCluster.second->GetTarget());
			PathingArc* currentArc = currentNode->FindArc(currentCluster->GetNode());

			clusterPathsLimit[pathingCluster.second].push_back(currentArc);
			clusterPathWeight += currentArc->GetWeight();

			currentNode = currentArc->GetNode();
		}
		clusterPathWeightsLimit.insert({ clusterPathWeight, pathingCluster.second });
	}

	unsigned int clusterCount = 0;
	for (auto const& clusterPathWeight : clusterPathWeightsLimit)
	{
		if (clusterLimit <= clusterCount)
			return;

		clusterPaths[clusterPathWeight.second] = clusterPathsLimit[clusterPathWeight.second];
		clusterPathWeights.insert({ clusterPathWeight.first, clusterPathWeight.second });

		clusterCount++;
	}
}

void PathingNode::GetClusters(unsigned int pathingType, unsigned int clusterLimit, 
	std::map<PathingCluster*, PathingArcVec>& clusterPaths, std::map<PathingCluster*, float>& clusterPathWeights)
{
	std::multimap<float, PathingCluster*> clusterPathWeightsLimit;
	std::map<PathingCluster*, PathingArcVec> clusterPathsLimit;
	for (auto const& pathingCluster : mClusters)
	{
		if (pathingCluster.second->GetType() != pathingType)
			continue;

		float clusterPathWeight = 0.f;
		PathingNode* currentNode = this;
		while (currentNode != pathingCluster.second->GetTarget())
		{
			PathingCluster* currentCluster = currentNode->FindCluster(pathingType, pathingCluster.second->GetTarget());
			PathingArc* currentArc = currentNode->FindArc(currentCluster->GetNode());

			clusterPathsLimit[pathingCluster.second].push_back(currentArc);
			clusterPathWeight += currentArc->GetWeight();

			currentNode = currentArc->GetNode();
		}
		clusterPathWeightsLimit.insert({ clusterPathWeight, pathingCluster.second });
	}

	unsigned int clusterCount = 0;
	for (auto const& clusterPathWeight : clusterPathWeightsLimit)
	{
		if (clusterLimit <= clusterCount)
			return;

		clusterPaths[clusterPathWeight.second] = clusterPathsLimit[clusterPathWeight.second];
		clusterPathWeights[clusterPathWeight.second] = clusterPathWeight.first;

		clusterCount++;
	}
}

PathingCluster* PathingNode::FindCluster(unsigned int pathingClusterId)
{
	auto itCluster = mClusters.find(pathingClusterId);
	return itCluster != mClusters.end() ? itCluster->second : NULL;
}

PathingCluster* PathingNode::FindCluster(unsigned int pathingType, unsigned int clusterId)
{
	for (auto const& cluster : mClusters)
	{
		PathingCluster* pathingCluster = cluster.second;
		if (pathingCluster->GetType() == pathingType)
			if (pathingCluster->GetTarget()->GetCluster() == clusterId)
				return pathingCluster;
	}
	return NULL;
}

PathingCluster* PathingNode::FindCluster(unsigned int pathingType, PathingNode* pTargetNode)
{
	LogAssert(pTargetNode, "Invalid node");

	unsigned int pathingClusterId = pTargetNode->GetId() << 16 | pathingType;
	auto itCluster = mClusters.find(pathingClusterId);
	return itCluster != mClusters.end() ? itCluster->second : NULL;
}

void PathingNode::RemoveClusters()
{
	for (PathingClusterMap::iterator itCluster = mClusters.begin(); itCluster != mClusters.end(); ++itCluster)
	{
		PathingCluster* pCluster = itCluster->second;
		delete pCluster;
	}
	mClusters.clear();
}

void PathingNode::AddActor(PathingActor* pActor)
{
	LogAssert(pActor, "Invalid actor");

	unsigned int actorId = pActor->GetTarget()->GetId() << 16 | pActor->GetType();
	mActors[actorId] = pActor;
}

void PathingNode::GetActors(unsigned int pathingType, PathingActorVec& outActors)
{
	for (auto const& pathActor : mActors)
		if (pathActor.second->GetType() == pathingType)
			outActors.push_back(pathActor.second);
}

PathingActor* PathingNode::FindActor(unsigned int pathingActorId)
{
	auto itActor = mActors.find(pathingActorId);
	return itActor != mActors.end() ? itActor->second : NULL;
}

PathingActor* PathingNode::FindActor(unsigned int pathingType, PathingNode* pTargetNode)
{
	LogAssert(pTargetNode, "Invalid node");

	unsigned int actorId = pTargetNode->GetId() << 16 | pathingType;
	auto itActor = mActors.find(actorId);
	return itActor != mActors.end() ? itActor->second : NULL;
}

void PathingNode::RemoveActors()
{
	for (PathingActorMap::iterator itActor = mActors.begin(); itActor != mActors.end(); ++itActor)
	{
		PathingActor* pActor = itActor->second;
		delete pActor;
	}
	mActors.clear();
}

PathingArc::~PathingArc()
{
	RemoveTransition();
}

void PathingArc::AddTransition(PathingTransition* pTransition)
{
	LogAssert(pTransition, "Invalid transition");
	mTransition = pTransition;
}

void PathingArc::RemoveTransition()
{
	if (mTransition)
		delete mTransition;
}

//--------------------------------------------------------------------------------------------------------
// PathPlan
//--------------------------------------------------------------------------------------------------------
bool PathPlan::CheckForNextNode(const Vector3<float>& pos)
{
	if (mIndex == mPath.end())
		return false;

	Vector3<float> diff = pos - (*mIndex)->GetNode()->GetPosition();
	if (Length(diff) <= (float)PATHING_DEFAULT_NODE_TOLERANCE)
	{
		mIndex++;
		return true;
	}

	return false;
}

bool PathPlan::CheckForEnd(void)
{
	if (mIndex == mPath.end())
		return true;
	return false;
}

void PathPlan::AddArc(PathingArc* pArc)
{
	LogAssert(pArc, "Invalid arc");
	mPath.insert(mPath.begin(), pArc);
}

//--------------------------------------------------------------------------------------------------------
// PathPlanNode
//--------------------------------------------------------------------------------------------------------
PathPlanNode::PathPlanNode(PathingArc* pArc, PathPlanNode* pPrevNode, PathingNode* pGoalNode)
{
	LogAssert(pArc, "Invalid arc");
	
	mPathingArc = pArc;
	mPathingNode = pArc->GetNode();
	mPrevNode = pPrevNode;  // NULL is a valid value, though it should only be NULL for the start node
	mGoalNode = pGoalNode;
	mClosed = false;
	UpdatePathCost();
}

PathPlanNode::PathPlanNode(PathingNode* pNode, PathPlanNode* pPrevNode, PathingNode* pGoalNode)
{
	LogAssert(pNode, "Invalid node");

	mPathingArc = NULL;
	mPathingNode = pNode;
	mPrevNode = pPrevNode;  // NULL is a valid value, though it should only be NULL for the start node
	mGoalNode = pGoalNode;
	mClosed = false;
	UpdatePathCost();
}

void PathPlanNode::UpdateNode(PathingArc* pArc, PathPlanNode* pPrev)
{
	LogAssert(pPrev, "Invalid node");
	mPathingArc = pArc;
	mPathingNode = pArc->GetNode();
	mPrevNode = pPrev;
	UpdatePathCost();
}

void PathPlanNode::UpdatePathCost(void)
{
	// total cost (g)
	if (mPrevNode)
		mGoal = mPrevNode->GetGoal() + mPathingArc->GetWeight();
	else
		mGoal = 0;
}


//--------------------------------------------------------------------------------------------------------
// PathFinder
//--------------------------------------------------------------------------------------------------------
PathFinder::PathFinder(void)
{
	mStartNode = NULL;
	mGoalNode = NULL;
}

PathFinder::~PathFinder(void)
{
	Destroy();
}

void PathFinder::Destroy(void)
{
	// destroy all the PathPlanNode objects and clear the map
	for (PathingNodeToPathPlanNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
		delete it->second;
	mNodes.clear();
	
	// clear the open set
	mOpenSet.clear();
	
	// clear the start & goal nodes
	mStartNode = NULL;
	mGoalNode = NULL;
}

//
// PathFinder::operator()					- Chapter 18, page 638
//
PathPlan* PathFinder::operator()(PathingNode* pStartNode, PathingNode* pGoalNode, int skipArc, float threshold)
{
	LogAssert(pStartNode, "Invalid node");
	LogAssert(pGoalNode, "Invalid node");

	// if the start and end nodes are the same, we're close enough to b-line to the goal
	if (pStartNode == pGoalNode)
		return NULL;

	// set our members
	mStartNode = pStartNode;
	mGoalNode = pGoalNode;
		
	// The open set is a priority queue of the nodes to be evaluated.  If it's ever empty, it means 
	// we couldn't find a path to the goal. The start node is the only node that is initially in 
	// the open set.
	AddToOpenSet(mStartNode, NULL);

	while (!mOpenSet.empty())
	{
		// grab the most likely candidate
		PathPlanNode* planNode = mOpenSet.front();

		// lets find out if we successfully found a path.
		if (planNode->GetPathingNode() == mGoalNode)
			return RebuildPath(planNode);

		// we're processing this node so remove it from the open set and add it to the closed set
		mOpenSet.pop_front();
		AddToClosedSet(planNode);

		// get the neighboring nodes
		PathingArcVec neighbors;
		planNode->GetPathingNode()->GetArcs(AT_NORMAL, neighbors);
		planNode->GetPathingNode()->GetArcs(AT_ACTION, neighbors);

		// loop though all the neighboring nodes and evaluate each one
		for (PathingArcVec::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			if (skipArc == (*it)->GetType()) continue;

			PathingNode* pNodeToEvaluate = (*it)->GetNode();

			// Try and find a PathPlanNode object for this node.
			PathingNodeToPathPlanNodeMap::iterator findIt = mNodes.find(pNodeToEvaluate);
			
			// If one exists and it's in the closed list, we've already evaluated the node.  We can
			// safely skip it.
			if (findIt != mNodes.end() && findIt->second->IsClosed())
				continue;

			// figure out the cost for this route through the node
			float costForThisPath = planNode->GetGoal() + (*it)->GetWeight();
			if (costForThisPath >= threshold)
				continue;

			bool isPathBetter = false;

			// Grab the PathPlanNode if there is one.
			PathPlanNode* pPathPlanNodeToEvaluate = NULL;
			if (findIt != mNodes.end())
				pPathPlanNodeToEvaluate = findIt->second;

			// No PathPlanNode means we've never evaluated this pathing node so we need to add it to 
			// the open set, which has the side effect of setting all the cost data.
			if (!pPathPlanNodeToEvaluate)
				pPathPlanNodeToEvaluate = AddToOpenSet((*it), planNode);
			
			// If this node is already in the open set, check to see if this route to it is better than
			// the last.
			else if (costForThisPath < pPathPlanNodeToEvaluate->GetGoal())
				isPathBetter = true;
			
			// If this path is better, relink the nodes appropriately, update the cost data, and
			// reinsert the node into the open list priority queue.
			if (isPathBetter)
			{
				pPathPlanNodeToEvaluate->UpdateNode((*it), planNode);
				ReinsertNode(pPathPlanNodeToEvaluate);
			}
		}
	}
	
	return NULL;
}

//
// PathFinder::operator()					- Chapter 18, page 638
//
PathPlan* PathFinder::operator()(PathingNode* pStartNode, PathingNodeVec& searchNodes, int skipArc, float threshold)
{
	LogAssert(pStartNode, "Invalid node");

	// set our members
	mStartNode = pStartNode;
	mGoalNode = NULL;

	// The open set is a priority queue of the nodes to be evaluated.  If it's ever empty, it means 
	// we couldn't find a path to the goal. The start node is the only node that is initially in 
	// the open set.
	AddToOpenSet(mStartNode, NULL);

	PathPlanNode* pathPlanNode = NULL;
	while (!mOpenSet.empty())
	{
		// grab the most likely candidate
		PathPlanNode* planNode = mOpenSet.front();

		// lets find out if we successfully found a path.
		PathingNodeVec::iterator itNode = 
			std::find(searchNodes.begin(), searchNodes.end(), planNode->GetPathingNode());
		if (itNode != searchNodes.end())
		{
			if (pathPlanNode)
			{
				//find mininum path plan cost
				if (planNode->GetGoal() < pathPlanNode->GetGoal())
					pathPlanNode = planNode;
			}
			else pathPlanNode = planNode;
		}

		// we're processing this node so remove it from the open set and add it to the closed set
		mOpenSet.pop_front();
		AddToClosedSet(planNode);

		// get the neighboring nodes
		PathingArcVec neighbors;
		planNode->GetPathingNode()->GetArcs(AT_NORMAL, neighbors);
		planNode->GetPathingNode()->GetArcs(AT_ACTION, neighbors);

		// loop though all the neighboring nodes and evaluate each one
		for (PathingArcVec::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			if (skipArc == (*it)->GetType()) continue;

			PathingNode* pNodeToEvaluate = (*it)->GetNode();

			// Try and find a PathPlanNode object for this node.
			PathingNodeToPathPlanNodeMap::iterator findIt = mNodes.find(pNodeToEvaluate);

			// If one exists and it's in the closed list, we've already evaluated the node.  We can
			// safely skip it.
			if (findIt != mNodes.end() && findIt->second->IsClosed())
				continue;

			// figure out the cost for this route through the node
			float costForThisPath = planNode->GetGoal() + (*it)->GetWeight();
			if (costForThisPath >= threshold)
				continue;

			if (pathPlanNode && costForThisPath >= pathPlanNode->GetGoal())
				continue;

			bool isPathBetter = false;
			// Grab the PathPlanNode if there is one.
			PathPlanNode* pPathPlanNodeToEvaluate = NULL;
			if (findIt != mNodes.end())
				pPathPlanNodeToEvaluate = findIt->second;

			// No PathPlanNode means we've never evaluated this pathing node so we need to add it to 
			// the open set, which has the side effect of setting all the cost data.
			if (!pPathPlanNodeToEvaluate)
				pPathPlanNodeToEvaluate = AddToOpenSet((*it), planNode);

			// If this node is already in the open set, check to see if this route to it is better than
			// the last.
			else if (costForThisPath < pPathPlanNodeToEvaluate->GetGoal())
				isPathBetter = true;

			// If this path is better, relink the nodes appropriately, update the cost data, and
			// reinsert the node into the open list priority queue.
			if (isPathBetter)
			{
				pPathPlanNodeToEvaluate->UpdateNode((*it), planNode);
				ReinsertNode(pPathPlanNodeToEvaluate);
			}
		}
	}

	return pathPlanNode ? RebuildPath(pathPlanNode) : NULL;
}

void PathFinder::operator()(PathingNode* pStartNode, 
	PathingNodeVec& searchNodes, PathPlanMap& plans, int skipArc, float threshold)
{
	LogAssert(pStartNode, "Invalid node");

	// set our members
	mStartNode = pStartNode;
	mGoalNode = NULL;

	// The open set is a priority queue of the nodes to be evaluated.  If it's ever empty, it means 
	// we couldn't find a path to the goal. The start node is the only node that is initially in 
	// the open set.
	AddToOpenSet(mStartNode, NULL);

	std::map<PathingNode*, PathPlanNode*> foundPlanNodes;
	while (!mOpenSet.empty())
	{
		// grab the most likely candidate
		PathPlanNode* planNode = mOpenSet.front();

		// lets find out if we successfully found a node.
		PathingNodeVec::iterator itNode =
			std::find(searchNodes.begin(), searchNodes.end(), planNode->GetPathingNode());
		if (itNode != searchNodes.end())
		{
			if (plans.find((*itNode)) == plans.end())
			{
				foundPlanNodes[(*itNode)] = planNode;
			}
			else if (planNode->GetGoal() < foundPlanNodes[(*itNode)]->GetGoal())
			{
				foundPlanNodes[(*itNode)] = planNode;
			}
		}

		// we're processing this node so remove it from the open set and add it to the closed set
		mOpenSet.pop_front();
		AddToClosedSet(planNode);

		// get the neighboring nodes
		PathingArcVec neighbors;
		planNode->GetPathingNode()->GetArcs(AT_NORMAL, neighbors);
		planNode->GetPathingNode()->GetArcs(AT_ACTION, neighbors);

		// loop though all the neighboring nodes and evaluate each one
		for (PathingArcVec::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			if (skipArc == (*it)->GetType()) continue;

			PathingNode* pNodeToEvaluate = (*it)->GetNode();

			// Try and find a PathPlanNode object for this node.
			PathingNodeToPathPlanNodeMap::iterator findIt = mNodes.find(pNodeToEvaluate);

			// If one exists and it's in the closed list, we've already evaluated the node.  We can
			// safely skip it.
			if (findIt != mNodes.end() && findIt->second->IsClosed())
				continue;

			// figure out the cost for this route through the node
			float costForThisPath = planNode->GetGoal() + (*it)->GetWeight();
			if (costForThisPath >= threshold)
				continue;

			bool isPathBetter = false;
			// Grab the PathPlanNode if there is one.
			PathPlanNode* pPathPlanNodeToEvaluate = NULL;
			if (findIt != mNodes.end())
				pPathPlanNodeToEvaluate = findIt->second;

			// No PathPlanNode means we've never evaluated this pathing node so we need to add it to 
			// the open set, which has the side effect of setting all the cost data.
			if (!pPathPlanNodeToEvaluate)
				pPathPlanNodeToEvaluate = AddToOpenSet((*it), planNode);

			// If this node is already in the open set, check to see if this route to it is better than
			// the last.
			else if (costForThisPath < pPathPlanNodeToEvaluate->GetGoal())
				isPathBetter = true;

			// If this path is better, relink the nodes appropriately, update the cost data, and
			// reinsert the node into the open list priority queue.
			if (isPathBetter)
			{
				pPathPlanNodeToEvaluate->UpdateNode((*it), planNode);
				ReinsertNode(pPathPlanNodeToEvaluate);
			}
		}
	}

	for (auto& foundPlanNode : foundPlanNodes)
		plans[foundPlanNode.first] = RebuildPath(foundPlanNode.second);
}

void PathFinder::operator()(PathingNode* pStartNode,
	std::map<unsigned short, PathingNode*>& searchClusters, ClusterPlanMap& clusterPlans, int skipArc, float threshold)
{
	LogAssert(pStartNode, "Invalid node");

	// set our members
	mStartNode = pStartNode;
	mGoalNode = NULL;

	// The open set is a priority queue of the nodes to be evaluated.  If it's ever empty, it means 
	// we couldn't find a path to the goal. The start node is the only node that is initially in 
	// the open set.
	AddToOpenSet(mStartNode, NULL);

	std::map<unsigned short, PathPlanNode*> clusterPlanNodes;
	while (!mOpenSet.empty())
	{
		// grab the most likely candidate
		PathPlanNode* planNode = mOpenSet.front();

		// lets find out if we successfully found a cluster.
		std::map<unsigned short, PathingNode*>::iterator itCluster = 
			searchClusters.find(planNode->GetPathingNode()->GetCluster());
		if (itCluster != searchClusters.end())
		{
			if (clusterPlanNodes.find((*itCluster).first) != clusterPlanNodes.end())
			{
				if (clusterPlanNodes[(*itCluster).first]->GetPathingNode() != (*itCluster).second)
				{
					if (planNode->GetPathingNode() == (*itCluster).second)
					{
						clusterPlanNodes[(*itCluster).first] = planNode;
					}
					else if (planNode->GetGoal() < clusterPlanNodes[(*itCluster).first]->GetGoal())
					{
						clusterPlanNodes[(*itCluster).first] = planNode;
					}
				}
				else if (planNode->GetPathingNode() == (*itCluster).second)
				{
					if (planNode->GetGoal() < clusterPlanNodes[(*itCluster).first]->GetGoal())
					{
						clusterPlanNodes[(*itCluster).first] = planNode;
					}
				}
			}
			else clusterPlanNodes[(*itCluster).first] = planNode;
		}

		// we're processing this node so remove it from the open set and add it to the closed set
		mOpenSet.pop_front();
		AddToClosedSet(planNode);

		// get the neighboring nodes
		PathingArcVec neighbors;
		planNode->GetPathingNode()->GetArcs(AT_NORMAL, neighbors);
		planNode->GetPathingNode()->GetArcs(AT_ACTION, neighbors);

		// loop though all the neighboring nodes and evaluate each one
		for (PathingArcVec::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			if (skipArc == (*it)->GetType()) continue;

			PathingNode* pNodeToEvaluate = (*it)->GetNode();

			// Try and find a PathPlanNode object for this node.
			PathingNodeToPathPlanNodeMap::iterator findIt = mNodes.find(pNodeToEvaluate);

			// If one exists and it's in the closed list, we've already evaluated the node.  We can
			// safely skip it.
			if (findIt != mNodes.end() && findIt->second->IsClosed())
				continue;

			// figure out the cost for this route through the node
			float costForThisPath = planNode->GetGoal() + (*it)->GetWeight();
			if (costForThisPath >= threshold)
				continue;

			bool isPathBetter = false;
			// Grab the PathPlanNode if there is one.
			PathPlanNode* pPathPlanNodeToEvaluate = NULL;
			if (findIt != mNodes.end())
				pPathPlanNodeToEvaluate = findIt->second;

			// No PathPlanNode means we've never evaluated this pathing node so we need to add it to 
			// the open set, which has the side effect of setting all the cost data.
			if (!pPathPlanNodeToEvaluate)
				pPathPlanNodeToEvaluate = AddToOpenSet((*it), planNode);

			// If this node is already in the open set, check to see if this route to it is better than
			// the last.
			else if (costForThisPath < pPathPlanNodeToEvaluate->GetGoal())
				isPathBetter = true;

			// If this path is better, relink the nodes appropriately, update the cost data, and
			// reinsert the node into the open list priority queue.
			if (isPathBetter)
			{
				pPathPlanNodeToEvaluate->UpdateNode((*it), planNode);
				ReinsertNode(pPathPlanNodeToEvaluate);
			}
		}
	}

	for (auto& clusterPlanNode : clusterPlanNodes)
		clusterPlans[clusterPlanNode.first] = RebuildPath(clusterPlanNode.second);
}


//
// PathFinder::operator()
//
void PathFinder::operator()(PathingNode* pStartNode, 
	std::vector<ActorId>& searchActors, ActorPlanMap& actorPlans, int skipArc, float threshold)
{
	LogAssert(pStartNode, "Invalid node");

	mStartNode = pStartNode;
	mGoalNode = NULL;

	// The open set is a priority queue of the nodes to be evaluated.  If it's ever empty, it means 
	// we couldn't find a path to the goal. The start node is the only node that is initially in 
	// the open set.
	AddToOpenSet(mStartNode, NULL);

	std::map<unsigned short, PathPlanNode*> actorPathPlans;
	while (!mOpenSet.empty())
	{
		// grab the most likely candidate
		PathPlanNode* planNode = mOpenSet.front();

		// lets find out if we successfully found an actor.
		std::vector<ActorId>::iterator itActor =
			std::find(searchActors.begin(), searchActors.end(), planNode->GetPathingNode()->GetActorId());
		if (itActor != searchActors.end())
		{
			if (actorPathPlans.find((*itActor)) == actorPathPlans.end())
			{
				actorPathPlans[(*itActor)] = planNode;
			}
			else if (planNode->GetGoal() < actorPathPlans[(*itActor)]->GetGoal())
			{
				actorPathPlans[(*itActor)] = planNode;
			}
		}

		// we're processing this node so remove it from the open set and add it to the closed set
		mOpenSet.pop_front();
		AddToClosedSet(planNode);

		// get the neighboring nodes
		PathingArcVec neighbors;
		planNode->GetPathingNode()->GetArcs(AT_NORMAL, neighbors);
		planNode->GetPathingNode()->GetArcs(AT_ACTION, neighbors);

		// loop though all the neighboring nodes and evaluate each one
		for (PathingArcVec::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			if (skipArc == (*it)->GetType()) continue;

			PathingNode* pNodeToEvaluate = (*it)->GetNode();

			// Try and find a PathPlanNode object for this node.
			PathingNodeToPathPlanNodeMap::iterator findIt = mNodes.find(pNodeToEvaluate);

			// If one exists and it's in the closed list, we've already evaluated the node.  We can
			// safely skip it.
			if (findIt != mNodes.end() && findIt->second->IsClosed())
				continue;

			// figure out the cost for this route through the node
			float costForThisPath = planNode->GetGoal() + (*it)->GetWeight();
			if (costForThisPath >= threshold)
				continue;

			bool isPathBetter = false;
			// Grab the PathPlanNode if there is one.
			PathPlanNode* pPathPlanNodeToEvaluate = NULL;
			if (findIt != mNodes.end())
				pPathPlanNodeToEvaluate = findIt->second;

			// No PathPlanNode means we've never evaluated this pathing node so we need to add it to 
			// the open set, which has the side effect of setting all the cost data.
			if (!pPathPlanNodeToEvaluate)
				pPathPlanNodeToEvaluate = AddToOpenSet((*it), planNode);

			// If this node is already in the open set, check to see if this route to it is better than
			// the last.
			else if (costForThisPath < pPathPlanNodeToEvaluate->GetGoal())
				isPathBetter = true;

			// If this path is better, relink the nodes appropriately, update the cost data, and
			// reinsert the node into the open list priority queue.
			if (isPathBetter)
			{
				pPathPlanNodeToEvaluate->UpdateNode((*it), planNode);
				ReinsertNode(pPathPlanNodeToEvaluate);
			}
		}
	}

	for (auto& actorPathPlan : actorPathPlans)
		actorPlans[actorPathPlan.first] = RebuildPath(actorPathPlan.second);
}

PathPlanNode* PathFinder::AddToOpenSet(PathingArc* pArc, PathPlanNode* pPrevNode)
{
	LogAssert(pArc, "Invalid arc");

	// create a new PathPlanNode if necessary
	PathingNode* pNode = pArc->GetNode();
	PathingNodeToPathPlanNodeMap::iterator it = mNodes.find(pNode);
	PathPlanNode* pThisNode = NULL;
	if (it == mNodes.end())
	{
		pThisNode = new PathPlanNode(pArc, pPrevNode, mGoalNode);
		mNodes.insert(std::make_pair(pNode, pThisNode));
	}
	else
	{
		LogWarning("Adding existing PathPlanNode to open set");
		pThisNode = it->second;
		pThisNode->SetClosed(false);
	}
	
	// now insert it into the priority queue
	InsertNode(pThisNode);

	return pThisNode;
}

PathPlanNode* PathFinder::AddToOpenSet(PathingNode* pNode, PathPlanNode* pPrevNode)
{
	LogAssert(pNode, "Invalid node");

	// create a new PathPlanNode if necessary
	PathingNodeToPathPlanNodeMap::iterator it = mNodes.find(pNode);
	PathPlanNode* pThisNode = NULL;
	if (it == mNodes.end())
	{
		pThisNode = new PathPlanNode(pNode, pPrevNode, mGoalNode);
		mNodes.insert(std::make_pair(pNode, pThisNode));
	}
	else
	{
		LogWarning("Adding existing PathPlanNode to open set");
		pThisNode = it->second;
		pThisNode->SetClosed(false);
	}

	// now insert it into the priority queue
	InsertNode(pThisNode);

	return pThisNode;
}

void PathFinder::AddToClosedSet(PathPlanNode* pNode)
{
	LogAssert(pNode, "Invalid node");
	pNode->SetClosed();
}

//
// PathFinder::InsertNode					- Chapter 17, page 636
//
void PathFinder::InsertNode(PathPlanNode* pNode)
{
	LogAssert(pNode, "Invalid node");
	
	// just add the node if the open set is empty
	if (mOpenSet.empty())
	{
		mOpenSet.push_back(pNode);
		return;
	}

	// otherwise, perform an insertion sort	
	PathPlanNodeList::iterator it = mOpenSet.begin();
	PathPlanNode* pCompare = *it;
	while (pCompare->IsBetterChoiceThan(pNode))
	{
		++it;
		
		if (it != mOpenSet.end())
			pCompare = *it;
		else
			break;
	}
	mOpenSet.insert(it,pNode);
}

void PathFinder::ReinsertNode(PathPlanNode* pNode)
{
	LogAssert(pNode, "Invalid node");

	for (PathPlanNodeList::iterator it = mOpenSet.begin(); it != mOpenSet.end(); ++it)
	{
		if (pNode == (*it))
		{
			mOpenSet.erase(it);
			InsertNode(pNode);
			return;
		}
	}

	// if we get here, the node was never in the open set to begin with
    LogWarning("Attemping to reinsert node that was never in the open list");
	InsertNode(pNode);
}

PathPlan* PathFinder::RebuildPath(PathPlanNode* pGoalNode)
{
	LogAssert(pGoalNode, "Invalid node");

	PathPlan* pPlan = new PathPlan();

	PathPlanNode* pNode = pGoalNode;
	while (pNode)
	{
		if (pNode->GetPathingArc() != NULL)
			pPlan->AddArc(pNode->GetPathingArc());
		pNode = pNode->GetPrev();
	}
	
	return pPlan;
}


//--------------------------------------------------------------------------------------------------------
// PathingGraph
//--------------------------------------------------------------------------------------------------------
void PathingGraph::DestroyGraph(void)
{
	// destroy all the nodes
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		// destroy all
		(*it).second->RemoveArcs();
		(*it).second->RemoveActors();
		(*it).second->RemoveClusters();
		delete (*it).second;
	}

	// destroy all clusters
	for (ClusterMap::iterator it = mClusters.begin(); it != mClusters.end(); ++it)
	{
		// delete cluster
		delete (*it).second;
	}

	mNodes.clear();
	mClusters.clear();
}

PathingNode* PathingGraph::FindClosestNode(const Vector3<float>& pos, bool skipIsolated)
{
	// This is a simple brute-force O(n) algorithm that could be made a LOT faster by utilizing
	// spatial partitioning, like an octree (or quadtree for flat worlds) or something similar.
	PathingNode* pClosestNode = NULL;
	float length = FLT_MAX;
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (skipIsolated)
		{
			//lets skip isolated nodes
			if (pNode->GetArcs().empty())
				continue;
		}

		Vector3<float> diff = pos - pNode->GetPosition();
		if (Length(diff) < length)
		{
			pClosestNode = pNode;
			length = Length(diff);
		}
	}
	
	return pClosestNode;
}

PathingNode* PathingGraph::FindFurthestNode(const Vector3<float>& pos, bool skipIsolated)
{
	// This is a simple brute-force O(n) algorithm that could be made a LOT faster by utilizing
	// spatial partitioning, like an octree (or quadtree for flat worlds) or something similar.
	PathingNode* pFurthestNode = NULL;
	float length = 0;
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (skipIsolated)
		{
			//lets skip isolated nodes
			if (pNode->GetArcs().empty())
				continue;
		}

		Vector3<float> diff = pos - pNode->GetPosition();
		if (Length(diff) > length)
		{
			pFurthestNode = pNode;
			length = Length(diff);
		}
	}

	return pFurthestNode;
}

void PathingGraph::FindNodes(PathingNodeVec& nodes, const Vector3<float>& pos, float radius, bool skipIsolated)
{
	// This is a simple brute-force O(n) algorithm that could be made a LOT faster by utilizing
	// spatial partitioning, like an octree (or quadtree for flat worlds) or something similar.
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (skipIsolated)
		{
			//lets skip isolated nodes
			if (pNode->GetArcs().empty())
				continue;
		}

		Vector3<float> diff = pos - pNode->GetPosition();
		if (Length(diff) <= radius)
			nodes.push_back(pNode);
	}
}

PathingNode* PathingGraph::FindNode(unsigned int nodeId)
{
	PathingNodeMap::iterator it = mNodes.find(nodeId);
	return it != mNodes.end() ? (*it).second : NULL;
}

PathingNode* PathingGraph::FindNode(PathingArc* pArc)
{
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (pNode->FindArc(pArc->GetId()) == pArc)
			return pNode;
	}

	return NULL;
}

PathingNode* PathingGraph::FindRandomNode(void)
{
	// cache this since it's not guaranteed to be constant time
	unsigned int numNodes = (unsigned int)mNodes.size();
	
	// choose a random node
	unsigned int node = (int)(Randomizer::FRand() * numNodes);
	
	// if we're in the lower half of the node list, start from the bottom
	if (node <= numNodes / 2)
	{
		PathingNodeMap::iterator it = mNodes.begin();
		for (unsigned int i = 0; i < node; i++)
			++it;
		return (*it).second;
	}

	// otherwise, start from the top
	else
	{
		PathingNodeMap::iterator it = mNodes.end();
		for (unsigned int i = numNodes; i >= node; i--)
			--it;
		return (*it).second;
	}
}

PathingArc* PathingGraph::FindArc(unsigned int arcId)
{
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		PathingArc* pArc = pNode->FindArc(arcId);
		if (pArc)
			return pArc;
	}
	return NULL;
}

void PathingGraph::FindPathPlans(PathingNode* pStartNode,
	std::map<unsigned short, PathingNode*>& searchClusters, ClusterPlanMap& plans, int skipArc, float threshold)
{
	// find the best path using an A* search algorithm
	PathFinder pathFinder;
	pathFinder(pStartNode, searchClusters, plans, skipArc, threshold);
}

void PathingGraph::FindPathPlans(PathingNode* pStartNode,
	std::vector<ActorId>& searchActors, ActorPlanMap& actorPlans, int skipArc, float threshold)
{
	// find the best path using an A* search algorithm
	PathFinder pathFinder;
	pathFinder(pStartNode, searchActors, actorPlans, skipArc, threshold);
}

void PathingGraph::FindPathPlans(PathingNode* pStartNode,
	PathingNodeVec& searchNodes, PathPlanMap& plans, int skipArc, float threshold)
{
	// find the best path using an A* search algorithm
	PathFinder pathFinder;
	pathFinder(pStartNode, searchNodes, plans, skipArc, threshold);
}

PathPlan* PathingGraph::FindPath(
	const Vector3<float>& startPoint, const Vector3<float>& endPoint, int skipArc, float threshold)
{
	PathingNode* pStart = FindClosestNode(startPoint);
	PathingNode* pGoal = FindClosestNode(endPoint);
	return FindPath(pStart, pGoal, skipArc, threshold);
}

PathPlan* PathingGraph::FindPath(
	PathingNode* pStartNode, PathingNodeVec& searchNodes, int skipArc, float threshold)
{
	// find the best path using an A* search algorithm
	PathFinder pathFinder;
	return pathFinder(pStartNode, searchNodes, skipArc, threshold);
}

PathPlan* PathingGraph::FindPath(
	const Vector3<float>& startPoint, PathingNode* pGoalNode, int skipArc, float threshold)
{
	PathingNode* pStart = FindClosestNode(startPoint);
	return FindPath(pStart, pGoalNode, skipArc, threshold);
}

PathPlan* PathingGraph::FindPath(
	PathingNode* pStartNode, const Vector3<float>& endPoint, int skipArc, float threshold)
{
	PathingNode* pGoal = FindClosestNode(endPoint);
	return FindPath(pStartNode, pGoal, skipArc, threshold);
}

PathPlan* PathingGraph::FindPath(
	PathingNode* pStartNode, PathingNode* pGoalNode, int skipArc, float threshold)
{
	// find the best path using an A* search algorithm
	PathFinder pathFinder;
	return pathFinder(pStartNode, pGoalNode, skipArc, threshold);
}

PathingNode* PathingGraph::FindClusterNode(unsigned int clusterId)
{
	ClusterMap::iterator it = mClusters.find(clusterId);
	if (it != mClusters.end())
	{
		Cluster* pCluster = (*it).second;
		return pCluster->GetNode();
	}

	return NULL;
}

PathingNode* PathingGraph::FindClusterNodeActor(ActorId actorId)
{
	for (ClusterMap::iterator it = mClusters.begin(); it != mClusters.end(); ++it)
	{
		Cluster* pCluster = (*it).second;
		PathingNode* pNode = pCluster->FindNodeActor(actorId);
		if (pNode)
			return pNode;
	}

	return NULL;
}

Cluster* PathingGraph::FindCluster(unsigned int clusterId)
{
	ClusterMap::iterator it = mClusters.find(clusterId);
	return it != mClusters.end() ? (*it).second : NULL;
}

void PathingGraph::InsertNode(PathingNode* pNode)
{
	LogAssert(pNode, "Invalid node");

	mNodes[pNode->GetId()] = pNode;
}

void PathingGraph::InsertCluster(Cluster* pCluster)
{
	LogAssert(pCluster, "Invalid cluster");

	mClusters[pCluster->GetId()] = pCluster;
}

void PathingGraph::RemoveNode(PathingNode* pNode)
{
	LogAssert(pNode, "Invalid node");

	for (PathingNodeMap::iterator itNode = mNodes.begin(); itNode != mNodes.end();)
	{
		if ((*itNode).second != pNode)
		{
			// destroy all the arcs pointing to the node
			(*itNode).second->RemoveVisibleNode(pNode);
			(*itNode).second->RemoveArc(pNode);
			itNode++;
		}
		else
		{
			(*itNode).second->RemoveArcs();
			(*itNode).second->RemoveActors();
			(*itNode).second->RemoveClusters();
			itNode = mNodes.erase(itNode);
		}
	}

	bool removeCluster = false;
	ClusterMap::iterator itCluster = mClusters.find(pNode->GetCluster());
	if (itCluster != mClusters.end())
	{
		Cluster* pCluster = (*itCluster).second;
		pCluster->RemoveNode(pNode);
		if (pCluster->GetNodes().empty())
		{
			itCluster = mClusters.erase(itCluster);
			delete pCluster;
			removeCluster = true;
		}
	}

	if (removeCluster)
		for (ClusterMap::iterator itCluster = mClusters.begin(); itCluster != mClusters.end(); ++itCluster)
			(*itCluster).second->RemoveVisibleCluster(pNode->GetCluster());

	delete pNode;
}

void PathingGraph::RemoveArc(PathingArc* pArc)
{
	for (PathingNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		PathingNode* pNode = (*it).second;
		if (pNode->RemoveArc(pArc->GetId()))
			return;
	}
}

void PathingGraph::RemoveCluster(Cluster* pCluster)
{
	for (ClusterMap::iterator itCluster = mClusters.begin(); itCluster != mClusters.end(); ++itCluster)
	{
		if ((*itCluster).second != pCluster)
		{
			itCluster = mClusters.erase(itCluster);
			break;
		}
	}

	delete pCluster;
}

void PathingGraph::RemoveClusters()
{
	// destroy all clusters
	for (ClusterMap::iterator itCluster = mClusters.begin(); itCluster != mClusters.end(); ++itCluster)
	{
		// delete cluster
		delete (*itCluster).second;
	}
	mClusters.clear();
}