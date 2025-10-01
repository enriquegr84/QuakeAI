//========================================================================
// Pathing.h : Implements a simple pathing system using A*
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

#ifndef PATHING_H
#define PATHING_H

#include "GameEngineStd.h"

#include "Core/Logger/Logger.h"
#include "Mathematic/Algebra/Vector3.h"

class PathingTransition;
class PathingCluster;
class PathingActor;
class PathingNode;
class PathingArc;

class PathPlanNode;
class PathFinder;
class PathPlan;

class Cluster;

typedef std::vector<PathingArc*> PathingArcVec;
typedef std::vector<PathingNode*> PathingNodeVec;
typedef std::vector<PathingActor*> PathingActorVec;
typedef std::vector<PathingCluster*> PathingClusterVec;
typedef std::vector<PathingTransition*> PathingTransitionVec;

typedef std::unordered_map<unsigned int, PathingCluster*> PathingClusterMap;
typedef std::unordered_map<unsigned int, PathingActor*> PathingActorMap;
typedef std::unordered_map<unsigned int, PathingArc*> PathingArcMap;
typedef std::unordered_map<unsigned int, PathingNode*> PathingNodeMap;

typedef std::list<PathPlanNode*> PathPlanNodeList;
typedef std::map<PathingNode*, PathPlan*> PathPlanMap;
typedef std::map<PathingNode*, PathPlanNode*> PathingNodeToPathPlanNodeMap;

typedef std::vector<Cluster*> ClusterVec;
typedef std::unordered_map<unsigned int, Cluster*> ClusterMap;
typedef std::unordered_map<unsigned int, PathPlan*> ClusterPlanMap;
typedef std::map<ActorId, PathPlan*> ActorPlanMap;

const float PATHING_DEFAULT_NODE_TOLERANCE = 4.0f;
const float PATHING_MOVEMENT_NODE_TOLERANCE = 2.0f;


//--------------------------------------------------------------------------------------------------------
// class Cluster
// This class represents a cluster graph.
//--------------------------------------------------------------------------------------------------------
class Cluster
{
	unsigned int mId;

	PathingNode* mNode;  //cluster representant (most visible node in the cluster)

	PathingNodeMap mNodes; //nodes within the cluster 
	PathingNodeMap mNodeActors; //actors node within the cluster 
	PathingNodeMap mVisibleClusters; //cluster visibility from a visible node

public:
	explicit Cluster(unsigned int id, PathingNode* pNode)
		: mId(id), mNode(pNode)
	{

	}

	unsigned int GetId(void) const { return mId; }
	PathingNode* GetNode() const { return mNode; }
	void SetNode(PathingNode* pNode) { mNode = pNode; }

	void RemoveNode(PathingNode* pNode);
	void AddNode(PathingNode* pNode);
	const PathingNodeMap& GetNodes();

	void RemoveNodeActor(ActorId actorId);
	void AddNodeActor(ActorId actorId, PathingNode* pNode);
	const PathingNodeMap& GetNodeActors();
	PathingNode* FindNodeActor(ActorId actorId);

	void RemoveVisibleCluster(unsigned int cluster);
	void AddVisibleCluster(unsigned int cluster, PathingNode* pNode);
	const PathingNodeMap& GetVisibileClusters();
	PathingNode* FindVisibleCluster(unsigned int cluster);
	bool IsVisibleCluster(unsigned int cluster);
};

//--------------------------------------------------------------------------------------------------------
// class PathingNode				- Chapter 18, page 636
// This class represents a single node in the pathing graph.
//
//--------------------------------------------------------------------------------------------------------
class PathingNode
{
	unsigned int mId;
	unsigned short mClusterId;
	Vector3<float> mPos;

	PathingClusterMap mClusters;
	PathingActorMap mActors;
	PathingArcMap mArcs;

	std::unordered_map<PathingNode*, float> mVisibleNodes;

	float mTolerance;
	ActorId mActorId;

public:
	explicit PathingNode(unsigned int id, ActorId actorId, 
		const Vector3<float>& pos, float tolerance = PATHING_DEFAULT_NODE_TOLERANCE)
		: mId(id), mActorId(actorId), mPos(pos), mTolerance(tolerance)
	{ }

	unsigned int GetId(void) const { return mId; }
	void SetActorId(ActorId actorId) { mActorId = actorId; }
	ActorId GetActorId(void) const { return mActorId; }
	void SetCluster(unsigned short clusterId) { mClusterId = clusterId; }
	unsigned short GetCluster() { return mClusterId; }
	float GetTolerance(void) const { return mTolerance; }
	const Vector3<float>& GetPosition(void) const { return mPos; }

	void RemoveVisibleNode(PathingNode* pNode);
	void AddVisibleNode(PathingNode* pNode, float value);
	const std::unordered_map<PathingNode*, float>& GetVisibileNodes();
	bool IsVisibleNode(PathingNode* pNode);

	void AddArc(PathingArc* pArc);
	PathingArc* FindArc(unsigned int id);
	PathingArc* FindArc(PathingNode* pLinkedNode);
	const PathingArcMap& GetArcs() { return mArcs; }
	void GetArcs(unsigned int arcType, PathingArcVec& outArcs);
	bool RemoveArc(unsigned int id);
	void RemoveArc(PathingNode* pNode);
	void RemoveArcs();

	void AddCluster(PathingCluster* pCluster);
	PathingCluster* FindCluster(unsigned int pathingClusterId);
	PathingCluster* FindCluster(unsigned int pathingType, unsigned int clusterId);
	PathingCluster* FindCluster(unsigned int pathingType, PathingNode* pTargetNode);
	void GetClusters(unsigned int pathingType, PathingClusterVec& outClusters);
	void GetClusters(unsigned int pathingType, 
		unsigned int clusterLimit, std::map<PathingCluster*, PathingArcVec>& clusterPaths,
		std::multimap<float, PathingCluster*, std::greater<float>>& clusterPathWeights);
	void GetClusters(unsigned int pathingType, unsigned int clusterLimit, 
		std::map<PathingCluster*, PathingArcVec>& clusterPaths, 
		std::map<PathingCluster*, float>& clusterPathWeights);
	const PathingClusterMap& GetClusters() { return mClusters; }
	void RemoveClusters();

	void AddActor(PathingActor* pActor);
	PathingActor* FindActor(unsigned int pathingActorId);
	PathingActor* FindActor(unsigned int pathingType, PathingNode* pTargetNode);
	void GetActors(unsigned int pathingType, PathingActorVec& outActors);
	const PathingActorMap& GetActors() { return mActors; }
	void RemoveActors();
};


//--------------------------------------------------------------------------------------------------------
// class PathingArc				- Chapter 18, page 636
// This class represents an arc that links two nodes, allowing travel between them.
//--------------------------------------------------------------------------------------------------------
class PathingArc
{
	unsigned int mId;
	unsigned int mType;
	float mWeight;

	PathingNode* mNode;  // node which is linked to
	PathingTransition* mTransition; // transition info

public:
	explicit PathingArc(unsigned int id, unsigned int type, PathingNode* pNode, float weight = 0.f) 
		: mId(id), mType(type), mWeight(weight), mNode(pNode), mTransition(nullptr)
	{ 

	}
	~PathingArc(void);

	unsigned int GetId(void) const { return mId; }
	unsigned int GetType(void) const { return mType; }
	float GetWeight(void) const { return mWeight; }
	PathingNode* GetNode() const { return mNode; }

	void AddTransition(PathingTransition* pTransition);
	PathingTransition* GetTransition() { return mTransition; }
	void RemoveTransition();
};

//--------------------------------------------------------------------------------------------------------
// class PathingTransition
// This class represents the transitions which an arc may do.
//--------------------------------------------------------------------------------------------------------
class PathingTransition
{
	std::vector<float> mWeights;
	std::vector<PathingNode*> mNodes;  // transition nodes
	std::vector<Vector3<float>> mPositions; // transition interpolation

public:
	explicit PathingTransition(const std::vector<PathingNode*>& nodes, 
		const std::vector<float>& weights, const std::vector<Vector3<float>>& positions)
		: mNodes(nodes), mWeights(weights), mPositions(positions)
	{

	}
	
	void RemoveNode(PathingNode* pNode) { 
		//remove all duplications
		mNodes.erase(std::remove(mNodes.begin(), mNodes.end(), pNode), mNodes.end()); 
	}

	bool FindNode(PathingNode* pNode) {
		return std::find(mNodes.begin(), mNodes.end(), pNode) != mNodes.end();
	}

	const std::vector<PathingNode*>& GetNodes(void) const { return mNodes; }
	const std::vector<float>& GetWeights(void) const { return mWeights; }
	const std::vector<Vector3<float>>& GetPositions(void) const { return mPositions; }
};

//--------------------------------------------------------------------------------------------------------
// class PathingCluster
// This class represents a transition that link clusters.
//--------------------------------------------------------------------------------------------------------
class PathingCluster
{
	unsigned int mType;

	PathingNode* mNode;  // node transition
	PathingNode* mTarget;  // node target

public:
	explicit PathingCluster(unsigned int type)
		: mType(type)
	{

	}

	unsigned int GetType(void) const { return mType; }
	PathingNode* GetNode() const { return mNode; }
	PathingNode* GetTarget() const { return mTarget; }

	void LinkClusters(PathingNode* pNode, PathingNode* pTarget);
};

//--------------------------------------------------------------------------------------------------------
// class PathingActor
// This class represents a transition that link actors.
//--------------------------------------------------------------------------------------------------------
class PathingActor
{
	unsigned int mType;
	ActorId mActorId;

	PathingNode* mNode;  // node transition
	PathingNode* mTarget;  // node target

public:
	explicit PathingActor(unsigned int type, ActorId actorId)
		: mType(type), mActorId(actorId)
	{

	}

	unsigned int GetType(void) const { return mType; }
	ActorId GetActor(void) const { return mActorId; }
	PathingNode* GetNode() const { return mNode; }
	PathingNode* GetTarget() const { return mTarget; }

	void LinkActors(PathingNode* pNode, PathingNode* pTarget);
};

//--------------------------------------------------------------------------------------------------------
// class PathingPlan				- Chapter 18, page 636
// This class represents a complete path and is used by the higher-level AI to determine where to go.
//--------------------------------------------------------------------------------------------------------
class PathPlan
{
	friend class PathFinder;

	PathingArcVec mPath;
	PathingArcVec::iterator mIndex;

public:
	PathPlan(void) 
	{ 
		mIndex = mPath.end();
	}
	
	void AddArc(PathingArc* pArc);

	const PathingArcVec& GetArcs(void) const { return mPath; }

	void ResetPath(void) 
	{ 
		mIndex = mPath.begin(); 
	}

	PathingArc* GetCurrentArc(void) const
	{
		LogAssert(mIndex != mPath.end(), "Invalid index");
		return (*mIndex);
	}

	bool CheckForNextNode(const Vector3<float>& pos);
	bool CheckForEnd(void);
};


//--------------------------------------------------------------------------------------------------------
// class PathPlanNode						- Chapter 18, page 636
// This class is a helper used in PathingGraph::FindPath().
//--------------------------------------------------------------------------------------------------------
class PathPlanNode
{
	PathPlanNode* mPrevNode;  // node we just came from
	PathingArc* mPathingArc;  // pointer to the pathing arc from the pathing graph
	PathingNode* mPathingNode;  // pointer to the pathing node from the pathing graph
	PathingNode* mGoalNode;  // pointer to the goal node
	bool mClosed;  // the node is closed if it's already been processed
	float mGoal;  // cost of the entire path up to this point (often called g)
	
public:
	explicit PathPlanNode(PathingArc* pArc, PathPlanNode* pPrevNode, PathingNode* pGoalNode);
	explicit PathPlanNode(PathingNode* pNode, PathPlanNode* pPrevNode, PathingNode* pGoalNode);
	PathPlanNode* GetPrev(void) const { return mPrevNode; }
	PathingArc* GetPathingArc(void) const { return mPathingArc; }
	PathingNode* GetPathingNode(void) const { return mPathingNode; }
	bool IsClosed(void) const { return mClosed; }
	float GetGoal(void) const { return mGoal; }
	
	void UpdateNode(PathingArc* pArc, PathPlanNode* pPrev);
	void SetClosed(bool toClose = true) { mClosed = toClose; }
	bool IsBetterChoiceThan(PathPlanNode* pRight) { return (mGoal < pRight->GetGoal()); }
	
private:
	void UpdatePathCost(void);
};


//--------------------------------------------------------------------------------------------------------
// class PathFinder								- Chapter 18, page 638
// This class implements the PathFinder algorithm.
//--------------------------------------------------------------------------------------------------------
class PathFinder
{
	PathingNodeToPathPlanNodeMap mNodes;
	PathingNode* mStartNode;
	PathingNode* mGoalNode;
	PathPlanNodeList mOpenSet;
	
public:
	PathFinder(void);
	~PathFinder(void);
	void Destroy(void);
	
	PathPlan* operator()(PathingNode* pStartNode, PathingNode* pGoalNode, 
		int skipArc = -1, float threshold = FLT_MAX);
	PathPlan* operator()(PathingNode* pStartNode, PathingNodeVec& searchNodes, 
		int skipArc = -1, float threshold = FLT_MAX);

	void operator()(PathingNode* pStartNode, std::map<unsigned short, PathingNode*>& searchClusters,
		ClusterPlanMap& clusterPlans, int skipArc = -1, float threshold = FLT_MAX);
	void operator()(PathingNode* pStartNode, std::vector<ActorId>& searchActors,
		ActorPlanMap& actorPlans, int skipArc = -1, float threshold = FLT_MAX);
	void operator()(PathingNode* pStartNode, PathingNodeVec& searchNodes,
		PathPlanMap& plans, int skipArc = -1, float threshold = FLT_MAX);
private:
	PathPlanNode* AddToOpenSet(PathingArc* pArc, PathPlanNode* pPrevNode);
	PathPlanNode* AddToOpenSet(PathingNode* pNode, PathPlanNode* pPrevNode);
	void AddToClosedSet(PathPlanNode* pNode);
	void InsertNode(PathPlanNode* pNode);
	void ReinsertNode(PathPlanNode* pNode);
	PathPlan* RebuildPath(PathPlanNode* pGoalNode);
};


//--------------------------------------------------------------------------------------------------------
// class PathingGraph					- Chapter 18, 636
// This class is the main interface into the pathing system.  It holds the pathing graph itself and owns
// all the PathingNode and Pathing Arc objects.
//--------------------------------------------------------------------------------------------------------
class PathingGraph
{	
public:
	PathingGraph(void) {}
	~PathingGraph(void) { DestroyGraph(); }
	void DestroyGraph(void);

	void FindNodes(PathingNodeVec&, const Vector3<float>& pos, float radius, bool skipIsolated = true);
	PathingNode* FindClosestNode(const Vector3<float>& pos, bool skipIsolated = true);
	PathingNode* FindFurthestNode(const Vector3<float>& pos, bool skipIsolated = true);
	PathingNode* FindNode(unsigned int nodeId);
	PathingNode* FindNode(PathingArc* pArc);
	PathingNode* FindRandomNode(void);
	PathingArc* FindArc(unsigned int arcId);

	void FindPathPlans(PathingNode* pStartNode, std::map<unsigned short, PathingNode*>& searchClusters,
		ClusterPlanMap& plans, int skipArc = -1, float threshold = FLT_MAX);
	void FindPathPlans(PathingNode* pStartNode, std::vector<ActorId>& searchActors,
		ActorPlanMap& actorPlans, int skipArc = -1, float threshold = FLT_MAX);
	void FindPathPlans(PathingNode* pStartNode, PathingNodeVec& searchNodes,
		PathPlanMap& plans, int skipArc = -1, float threshold = FLT_MAX);
	PathPlan* FindPath(PathingNode* pStartNode, PathingNodeVec& searchNodes, 
		int skipArc = -1, float threshold = FLT_MAX);
	PathPlan* FindPath(const Vector3<float>& startPoint, const Vector3<float>& endPoint, 
		int skipArc = -1, float threshold = FLT_MAX);
	PathPlan* FindPath(const Vector3<float>& startPoint, PathingNode* pGoalNode, 
		int skipArc = -1, float threshold = FLT_MAX);
	PathPlan* FindPath(PathingNode* pStartNode, const Vector3<float>& endPoint, 
		int skipArc = -1, float threshold = FLT_MAX);
	PathPlan* FindPath(PathingNode* pStartNode, PathingNode* pGoalNode, 
		int skipArc = -1, float threshold = FLT_MAX);

	PathingNode* FindClusterNodeActor(ActorId actorId);
	PathingNode* FindClusterNode(unsigned int clusterId);
	Cluster* FindCluster(unsigned int clusterId);

	void InsertNode(PathingNode* pNode);
	void RemoveNode(PathingNode* pNode);
	void RemoveArc(PathingArc* pArc);

	void InsertCluster(Cluster* pCluster);
	void RemoveCluster(Cluster* pCluster);
	void RemoveClusters();

	const ClusterMap& GetClusters() { return mClusters; }
	const PathingNodeMap& GetNodes() { return mNodes; }

private:

	PathingNodeMap mNodes; // master list of all nodes
	ClusterMap mClusters; // master list of all clusters
};


#endif