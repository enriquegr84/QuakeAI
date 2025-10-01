/*******************************************************
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#ifndef QUAKEAIMANAGER_H
#define QUAKEAIMANAGER_H

#include "QuakeStd.h"

#include "AI/AIManager.h"

#include "Games/Actors/PlayerActor.h"

#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

#include "Mathematic/Algebra/Matrix4x4.h"

#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>

#include <ppl.h>
#include <ppltasks.h>

#include <mutex>
#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>

class Transform;
class AIPlanNode;

typedef std::list<AIPlanNode*> AIPlanNodeList;
typedef std::vector<AIPlanNode*> AIPlanNodeVector;
typedef std::map<ActorId, AIPlanNodeVector> ActorToAIPlanNodeMap;

//
// struct VisibilityData
//
struct VisibilityData
{
	VisibilityData()
	{
		moveTime = 0.f;
		moveHeight = 0.f;
		moveDistance = 0.f;
	}

	float moveTime;
	float moveHeight;
	float moveDistance;
};

//
// struct NodePlan
//
struct NodePlan
{
	NodePlan()
	{
		id = -1;
		node = NULL;
		weight = 0.f;
	}

	NodePlan(PathingNode* playerNode, PathingArcVec& path)
	{
		id = -1;
		node = playerNode;

		weight = 0.f;
		for (PathingArc* pathArc : path)
		{
			weight += pathArc->GetWeight();
			path.push_back(pathArc);
		}
	}

	~NodePlan()
	{

	}

	void AddPathPlan(const PathingArcVec& pathPlan)
	{
		for (PathingArc* pathArc : pathPlan)
		{
			weight += pathArc->GetWeight();
			path.push_back(pathArc);
		}
	}

	void ResetPathPlan(const PathingArcVec& pathPlan)
	{
		weight = 0.f;
		path.clear();
		for (PathingArc* pathArc : pathPlan)
		{
			weight += pathArc->GetWeight();
			path.push_back(pathArc);
		}
	}

	int id;
	float weight;
	PathingNode* node;
	PathingArcVec path;
};

//
// struct PlayerData
//
struct PlayerData
{
	PlayerData()
	{
		valid = false;
		player = INVALID_ACTOR_ID;

		heuristic = 0.f;

		weapon = WP_NONE;
		weaponTime = 0.f;
		target = INVALID_ACTOR_ID;

		planWeight = 0.f;
		plan = NodePlan();

		for (unsigned int i = 0; i < MAX_STATS; i++)
		{
			stats[i] = 0;
		}

		for (unsigned int i = 0; i < MAX_WEAPONS; i++)
		{
			ammo[i] = 0;
			damage[i] = 0;
		}
	}

	PlayerData(std::shared_ptr<PlayerActor> playerActor)
	{
		valid = true;
		player = playerActor->GetId();

		heuristic = 0.f;

		weapon = (WeaponType)playerActor->GetState().weapon;
		weaponTime = playerActor->GetState().weaponTime;
		target = INVALID_ACTOR_ID;

		planWeight = 0.f;
		plan = NodePlan();

		for (unsigned int i = 0; i < MAX_STATS; i++)
		{
			stats[i] = playerActor->GetState().stats[i];
		}

		for (unsigned int i = 0; i < MAX_WEAPONS; i++)
		{
			ammo[i] = playerActor->GetState().ammo[i];
			damage[i] = 0;
		}
	}

	~PlayerData()
	{

	}

	void PlayerData::Update(std::shared_ptr<PlayerActor> playerActor)
	{
		player = playerActor->GetId();
		weapon = (WeaponType)playerActor->GetState().weapon;
		weaponTime = playerActor->GetState().weaponTime;

		for (unsigned int i = 0; i < MAX_STATS; i++)
		{
			stats[i] = playerActor->GetState().stats[i];
		}

		for (unsigned int i = 0; i < MAX_WEAPONS; i++)
		{
			ammo[i] = playerActor->GetState().ammo[i];
			damage[i] = 0;
		}
	}

	void PlayerData::ResetItems()
	{
		items.clear();
		itemAmount.clear();
		itemWeight.clear();
	}

	bool PlayerData::IsWeaponSelectable(int i)
	{
		if (!ammo[i])
			return false;

		if (!(stats[STAT_WEAPONS] & (1 << i)))
			return false;

		return true;
	}

	bool valid;
	ActorId player;

	float heuristic; //calculated from players items, damage and health/armor status.

	NodePlan plan;
	float planWeight; //indicates the current position (or weight) in the plan path

	WeaponType weapon;
	float weaponTime; // in sec
	ActorId target;

	int stats[MAX_STATS];
	int ammo[MAX_WEAPONS];
	int damage[MAX_WEAPONS];

	//list of items that the player plan to take or has taken as result of the simulation
	std::unordered_map<ActorId, float> items; //items respawning time in sec.
	std::unordered_map<ActorId, short> itemAmount;
	std::unordered_map<ActorId, float> itemWeight;
};

struct PlayerGuessView
{
	PlayerGuessView()
	{
		isUpdated = false;
	}

	PlayerGuessView(std::shared_ptr<PlayerActor> playerActor)
	{
		isUpdated = false;

		data = PlayerData(playerActor);
	}

	PlayerGuessView(const PlayerData& playerData)
	{
		isUpdated = false;

		data = PlayerData(playerData);
	}

	~PlayerGuessView()
	{

	}

	bool isUpdated;

	PlayerData data;
	PlayerData simulation;

	//known game items and its respawning time in sec that the player is aware of
	std::map<ActorId, float> items;
	std::map<ActorId, std::map<ActorId, float>> guessItems;

	std::map<ActorId, PlayerData> guessPlayers;
	std::map<ActorId, PlayerData> guessSimulations;
};

struct PlayerView
{
	PlayerView()
	{
		isUpdated = false;
	}

	PlayerView(std::shared_ptr<PlayerActor> playerActor)
	{
		isUpdated = false;

		data = PlayerData(playerActor);
	}

	PlayerView(const PlayerData& playerData)
	{
		isUpdated = false;

		data = PlayerData(playerData);
	}

	~PlayerView()
	{

	}

	bool isUpdated;

	PlayerData data;
	PlayerData simulation;

	//known items and its respawning time in sec that the player is aware of
	std::map<ActorId, float> gameItems;

	std::map<ActorId, PlayerGuessView> guessViews;
};

namespace AIMap
{
	struct Vec3
	{
		short x, y, z;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(x, y, z);
		}
	};

	struct Vec3Float
	{
		float x, y, z;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(x, y, z);
		}
	};

	struct VisibleNode
	{
		unsigned short id;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(id);
		}
	};

	struct ActorNode
	{
		unsigned short type;
		unsigned short actorid;
		unsigned short nodeid;
		unsigned short targetid;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(type, actorid, nodeid, targetid);
		}
	};

	struct ClusterNode
	{
		unsigned short type;
		unsigned short nodeid;
		unsigned short targetid;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(type, nodeid, targetid);
		}
	};

	struct ArcNode
	{
		int id;
		unsigned short type;
		unsigned short nodeid;
		float weight;

		std::vector<float> weights;
		std::vector<unsigned short> nodes;
		std::vector<Vec3> positions;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, type, nodeid, weight, weights, nodes, positions);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, type, nodeid, weight, weights, nodes, positions);
		}
	};

	struct GraphNode
	{
		unsigned short id;
		unsigned short actorid;
		unsigned short clusterid;
		float tolerance;
		Vec3Float position;
		std::vector<ArcNode> arcs;
		std::vector<ActorNode> actors;
		std::vector<ClusterNode> clusters;
		std::vector<VisibleNode> visibles;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, actorid, clusterid, tolerance, position, arcs, actors, clusters, visibles);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, actorid, clusterid, tolerance, position, arcs, actors, clusters, visibles);
		}
	};


	struct GraphCluster
	{
		unsigned short id;

		unsigned short node;  // cluster representant (most visible node in the cluster)

		std::vector<unsigned short> nodes; // node within the cluster 
		std::map<unsigned short, unsigned short> nodeActors; //actors node within the cluster 
		std::map<unsigned short, unsigned short> visibles; //visible clusters

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, node, nodes, nodeActors, visibles);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, node, nodes, nodeActors, visibles);
		}
	};

	struct Graph
	{
		std::vector<GraphNode> nodes;
		std::vector<GraphCluster> clusters;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(nodes, clusters);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(nodes, clusters);
		}
	};
}

namespace AIAnalysis
{
	//
	// class ActorPickup 
	// same info as ItemPickup Component but we are not using shared_ptr for concurrent analysis
	//
	class ActorPickup
	{
		unsigned int mCode;
		std::string mType;

		unsigned int mWait;
		unsigned int mAmount;
		unsigned int mMaximum;

	public:

		ActorPickup(unsigned int code, std::string& type,
			unsigned int wait, unsigned int amount, unsigned int maximum)
			: mCode(code), mType(type), mWait(wait), mAmount(amount), mMaximum(maximum)
		{

		}

		unsigned int GetCode(void) const { return mCode; }
		const std::string& GetType() const { return mType; }

		unsigned int GetWait() const { return mWait; }
		unsigned int GetAmount() const { return mAmount; }
		unsigned int GetMaximum() const { return mMaximum; }

	};

	//
	// class WeaponActorPickup 
	//
	class WeaponActorPickup : public ActorPickup
	{
		unsigned int mAmmo;

	public:

		WeaponActorPickup(unsigned int code, std::string& type,
			unsigned int wait, unsigned int amount, unsigned int maximum, unsigned int ammo)
			: ActorPickup(code, type, wait, amount, maximum), mAmmo(ammo)
		{

		}

		unsigned int GetAmmo() const { return mAmmo; }
	};

	struct PlayerInput
	{
		PlayerInput()
		{
			id = 0;
			frame = 0;

			weapon = WP_NONE;
			weaponTime = 0.f;
			target = INVALID_ACTOR_ID;

			for (unsigned int i = 0; i < MAX_STATS; i++)
				stats[i] = 0;

			for (unsigned int i = 0; i < MAX_WEAPONS; i++)
				ammo[i] = 0;

			planId = -1;
			planOffset = 0.f;

			planNode = 0;
			planNodeOffset = 0;
		}

		unsigned short id;
		unsigned short frame;

		ActorId target;
		WeaponType weapon;
		float weaponTime;

		int stats[MAX_STATS];
		int ammo[MAX_WEAPONS];

		short planId;
		float planOffset;
		unsigned short planNode, planNodeOffset;
		std::vector<int> planPath, planPathOffset;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, frame, target, weapon, weaponTime, stats, ammo, planId, planOffset, planNode, planPath, planNodeOffset, planPathOffset);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, frame, target, weapon, weaponTime, stats, ammo, planId, planOffset, planNode, planPath, planNodeOffset, planPathOffset);
		}
	};

	struct PlayerOutput
	{
		PlayerOutput()
		{
			id = 0;
			frame = 0;

			target = INVALID_ACTOR_ID;
			weapon = WP_NONE;
			damage = 0;

			heuristic = 0.f;

			planId = -1;
			planNode = 0;
		}

		unsigned short id;
		unsigned short frame;

		ActorId target;
		WeaponType weapon;
		unsigned short damage;

		float heuristic;

		//items respawning time
		std::unordered_map<ActorId, float> items;

		short planId;
		unsigned short planNode;
		std::vector<int> planPath;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, frame, target, weapon, damage, heuristic, items, planId, planNode, planPath);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, frame, target, weapon, damage, heuristic, items, planId, planNode, planPath);
		}
	};

	struct PlayerSimulation
	{
		PlayerSimulation()
		{
			code = 0;
			action = 0;

			planId = -1;

			target = INVALID_ACTOR_ID;
			weapon = WP_NONE;
			damage = 0;

			heuristic = 0.f;
		}

		unsigned long long code;

		std::vector<unsigned short> clusters;
		unsigned short action;

		float heuristic;

		short planId;
		std::vector<int> planPath;

		ActorId target;
		WeaponType weapon;
		unsigned short damage;

		//items respawning time
		std::unordered_map<ActorId, float> items;
		std::unordered_map<ActorId, short> itemAmount;
		std::unordered_map<ActorId, float> itemWeight;
	};

	struct Simulation
	{
		PlayerSimulation playerSimulation;
		PlayerSimulation otherPlayerSimulation;
	};

	struct GameSimulation
	{
		GameSimulation()
		{
			action = 0;
		}

		std::vector<unsigned short> clusters;
		unsigned short action;

		std::vector<Simulation*> simulations;
	};

	struct GameEvaluation
	{
		GameEvaluation()
		{
			type = 0;
			target = 0;

			playerDecision = nullptr;
			playerGuessDecision = nullptr;
		}

		unsigned short type;
		unsigned short target;

		//player guessing inputs/output. What the evaluated player is guessing from the oponnent are
		//the parameters for the player guessing simulation
		PlayerInput playerGuessInput;
		PlayerInput otherPlayerGuessInput;
		PlayerOutput playerGuessOutput;
		PlayerOutput otherPlayerGuessOutput;

		//player input/output. PlayerInput + PlayerGuessOutput are taken for the player decision simulation
		PlayerInput playerInput;
		PlayerInput otherPlayerInput;
		PlayerOutput playerOutput;
		PlayerOutput otherPlayerOutput;

		//items status before running the simulations
		std::map<ActorId, float> playerDecisionItems;
		std::map<ActorId, float> playerGuessItems;

		//simulations for both player guessing and decision
		GameSimulation* playerDecision;
		GameSimulation* playerGuessDecision;
		std::vector<GameSimulation*> playerDecisions;
		std::vector<GameSimulation*> playerGuessings;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(type, target, playerGuessInput, otherPlayerGuessInput, playerGuessOutput, otherPlayerGuessOutput, 
				playerInput, otherPlayerInput, playerOutput, otherPlayerOutput, playerDecisionItems, playerGuessItems);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(type, target, playerGuessInput, otherPlayerGuessInput, playerGuessOutput, otherPlayerGuessOutput, 
				playerInput, otherPlayerInput, playerOutput, otherPlayerOutput, playerDecisionItems, playerGuessItems);
		}
	};

	struct GameDecision
	{
		unsigned short id;
		std::string time;

		GameEvaluation evaluation;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(id, time, evaluation);
		}
	};

	struct GameAnalysis
	{
		std::vector<GameDecision> decisions;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(decisions);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(decisions);
		}
	};
}

namespace AIGame
{
	struct Vec3Float
	{
		float x, y, z;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(x, y, z);
		}
	};

	struct Weapon
	{
		unsigned short id;
		unsigned short ammo;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(id, ammo);
		}
	};

	struct Player
	{
		unsigned short id;

		float yaw, pitch;
		Vec3Float position;

		unsigned short health, armor, weapon, score;
		std::vector<Weapon> weapons;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, yaw, pitch, position, health, armor, weapon, score, weapons);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, yaw, pitch, position, health, armor, weapon, score, weapons);
		}
	};

	struct Projectile
	{
		unsigned short id;
		unsigned short code;

		float yaw, pitch;
		Vec3Float position;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(id, code, yaw, pitch, position);
		}
	};

	struct Explosion
	{
		unsigned short id;
		unsigned short code;

		Vec3Float position;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(id, code, position);
		}
	};

	struct Item
	{
		unsigned short id;

		bool visible;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(id, visible);
		}
	};

	struct Event
	{
		std::string type;

		unsigned short weapon;
		unsigned short player;
		unsigned short target;
		unsigned short actor;

		float yaw, pitch;
		Vec3Float position;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(type, weapon, player, target, actor, yaw, pitch, position);
		}
	};

	struct EventTrack
	{
		float elapsedTime;

		std::vector<Event> events;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(elapsedTime, events);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(elapsedTime, events);
		}
	};

	struct GameState
	{
		unsigned int id;
		std::string time;

		std::vector<Projectile> projectiles;
		std::vector<Explosion> explosions;
		std::vector<Player> players;
		std::vector<Item> items;
		std::vector<EventTrack> tracks;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, time, projectiles, explosions, players, items, tracks);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, time, projectiles, explosions, players, items, tracks);
		}
	};

	struct Game
	{
		std::vector<GameState> states;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(states);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(states);
		}
	};
}

enum PlayerActionType
{
	AT_MOVE = 0x0000000,
	AT_PUSH = 0x0000002,
	AT_TELEPORT = 0x0000006,
	AT_FALL = 0x000000A,
	AT_JUMP = 0x00000E
};

enum EvaluationType
{
	ET_CLOSEGUESSING,
	ET_GUESSING,
	ET_AWARENESS,
	ET_RESPONSIVE
};

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

class QuakeAIManager : public AIManager
{
	friend class AIFinder;
	friend class QuakeLogic;
	friend class QuakePhysic;
	friend class QuakeAIView;
	friend class QuakeAIEditorView;
	friend class QuakeAIAnalyzerView;

public:
	QuakeAIManager();
	~QuakeAIManager();

	void LoadPathingMap(const std::wstring& path);
	void LoadPathingMap(const std::wstring& path, std::shared_ptr<PathingGraph>& graph);

	void UpdateMap(std::shared_ptr<PathingGraph>& graph, ActorId playerId);
	void UpdateMap(ActorId playerId);

	virtual void SaveGraph(const std::string& path);
	virtual void SaveGraph(const std::string& path, std::shared_ptr<PathingGraph>& graph);
	virtual void LoadGraph(const std::wstring& path);
	virtual void LoadGraph(const std::wstring& path, std::shared_ptr<PathingGraph>& graph);

	void CreatePathing(ActorId playerId, NodePlan& pathPlan);
	PathingNode* CreatePathingNode(ActorId playerId, std::shared_ptr<PathingGraph>& graph);
	PathingNode* CreatePathingNode(ActorId playerId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph);
	void CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes,
		std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph);
	void CreatePathingMap(ActorId playerId, const PathingNodeVec& pathingNodes, std::shared_ptr<PathingGraph>& graph);

	virtual void OnUpdate(unsigned long deltaMs);

	bool IsEnable() { return mEnable; }
	void SetEnable(bool enable) { mEnable = enable; }

	std::map<ActorId, ActorId>& GetGameActors() { return mGameActors; }
	std::map<ActorId, const AIAnalysis::ActorPickup*>& GetGameActorPickups() { return mGameActorPickups; }
	const AIAnalysis::ActorPickup* GetGameActorPickup(ActorId actorId) { 
		return mGameActorPickups.find(actorId) != mGameActorPickups.end() ? mGameActorPickups.at(actorId) : NULL; }

	const AIGame::Game& GetGame() { return mGame; }
	void AddGameItem(const AIGame::Item& item) { mGame.states.back().items.push_back(item); }
	void AddGamePlayer(const AIGame::Player& player) { mGame.states.back().players.push_back(player); }
	void AddGameProjectile(const AIGame::Projectile& projectile) { mGame.states.back().projectiles.push_back(projectile); }
	void AddGameEventTrack(const AIGame::EventTrack& track) { mGame.states.back().tracks.push_back(track); }
	void AddGameEvent(const AIGame::Event& evt) { mGame.states.back().tracks.back().events.push_back(evt); }
	void AddGameState(const AIGame::GameState& gameState) { mGame.states.push_back(gameState); }

	AIAnalysis::Simulation* GetGameSimulation() { return mGameSimulation; }
	void SetGameSimulation(AIAnalysis::Simulation* simulation) { mGameSimulation = simulation; }

	AIAnalysis::GameEvaluation& GetGameEvaluation() { return mGameEvaluation; }
	void SetGameEvaluation(AIAnalysis::GameEvaluation& gameEvaluation) { mGameEvaluation = gameEvaluation; }

	AIAnalysis::GameAnalysis& GetGameAnalysis() { return mGameAnalysis; }

	void RemovePlayerSimulations(AIAnalysis::GameEvaluation& gameEvaluation);

	void LoadGame();
	void LoadGameAnalysis();
	void SaveGame();
	void SaveGameAnalysis();

	void GetPlayerGround(ActorId player, bool& onGround);
	void SetPlayerGround(ActorId player, bool onGround);

	void GetPlayerView(ActorId player, PlayerView& playerView);

	void SavePlayerView(ActorId player, const PlayerView& playerView);

	void UpdatePlayerView(ActorId player, const PlayerView& playerView);
	void UpdatePlayerView(ActorId player, const PlayerData& playerData);
	void UpdatePlayerView(ActorId player, const PlayerData& playerData, bool update);
	void UpdatePlayerView(ActorId player, const PlayerView& playerView, float planWeight);
	void UpdatePlayerGuessView(ActorId player, const PlayerGuessView& playerGuessView, bool isUpdated);

	void UpdatePlayerSimulationView(ActorId player, const PlayerGuessView& playerGuessView);
	void UpdatePlayerSimulationView(ActorId player, const PlayerView& playerView);

	void SpawnActor(ActorId playerId);
	void DetectActor(std::shared_ptr<PlayerActor> playerActor, std::shared_ptr<Actor> item);

	void PrintError(std::string data);
	void PrintInfo(std::string data);

protected:

	int GetNewPlanID(void) { return ++mLastPlanId; }

	float CalculatePathWeight(const PlayerData& playerData);
	Vector3<float> CalculatePathPosition(const PlayerData& playerData);

	//status is calculated based on health and armor
	float CalculatePlayerStatus(const PlayerData& playerData);
	float CalculatePlayerWeaponStatus(const PlayerData& playerData);
	float CalculateBestHeuristicItem(const PlayerData& playerData);
	float CalculateHeuristicItems(const PlayerData& playerData);
	float CalculateHeuristicItem(const PlayerData& playerData, ActorId item, float itemWeight);
	void CalculateWeightItems(const PlayerData& playerData, std::map<ActorId, float>& searchItems);
	void CalculateHeuristic(EvaluationType evaluation, PlayerData& playerData, PlayerData& otherPlayerData);
	void CalculateDamage(PlayerData& playerData, const std::map<float, VisibilityData>& visibility);
	void CalculateVisibility(
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
		std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans);
	bool BuildLongPath(
		std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
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

	void FindPathPlans(PathingNode* pStartNode, const std::map<ActorId, float>& searchItems,
		std::map<PathingActorVec, float>& actorsPathPlans, unsigned int pathingType);

	PathingNode* FindClosestNode(ActorId playerId, 
		std::shared_ptr<PathingGraph>& graph, float closestDistance, bool skipIsolated = true);

	// AI decision making process
	void Simulation(EvaluationType evaluation, const std::map<ActorId, float>& gameItems, 
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
	bool SimulatePlayerGuessingDecision(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation);
	bool SimulatePlayerGuessings(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation);
	bool SimulatePlayerGuessing(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation);
	bool SimulatePlayerDecision(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, AIAnalysis::GameEvaluation& gameEvaluation);

	// Runtime simulation
	bool SimulatePlayerGuessingDecision(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation);
	bool SimulatePlayerGuessings(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation);
	bool SimulatePlayerGuessing(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation);
	bool SimulatePlayerDecision(
		const PlayerData& playerDataIn, PlayerData& playerDataOut,
		const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
		const std::map<ActorId, float>& gameItems, ActorId playerEvaluation, EvaluationType evaluation);

	//players viewType
	void OnAttach(GameViewType vtype, ActorId aid) 
	{ 
		mPlayers[vtype] = aid; 
		mPlayerEvaluations[aid] = ET_GUESSING;
	}

	bool IsCloseAIGuessing();
	bool IsCloseHumanGuessing();

	bool MakeAIGuessing(PlayerView& aiView);
	bool MakeAIFastDecision(PlayerView& aiView);
	bool MakeAIGuessingDecision(PlayerView& aiView);
	bool MakeAIAwareDecision(PlayerView& aiView);
	bool MakeHumanGuessing(PlayerView& playerView);
	bool MakeHumanFastDecision(PlayerView& playerView);
	bool MakeHumanGuessingDecision(PlayerView& playerView);
	bool MakeHumanAwareDecision(PlayerView& playerView);

	void RunAIGuessing();
	void RunAIFastDecision();
	void RunAIAwareDecision();

	void RunHumanGuessing();
	void RunHumanFastDecision();
	void RunHumanAwareDecision();

	unsigned int GetFrame() { return mUpdateCounter; }

private:

	// AI decision making data
	void GetPlayerInput(const AIAnalysis::PlayerInput& playerInput, PlayerData& playerData);
	void GetPlayerInput(const AIAnalysis::PlayerInput& playerInput, PlayerData& playerData, PlayerData& playerDataOffset);
	void GetPlayerOutput(const AIAnalysis::PlayerOutput& playerOutput, PlayerData& playerData);
	void GetPlayerSimulation(const AIAnalysis::PlayerSimulation& playerSimulation, PlayerData& playerData);

	void SetPlayerInput(AIAnalysis::PlayerInput& playerInput, const PlayerData& playerData, const PlayerData& playerDataOffset);
	void SetPlayerOutput(AIAnalysis::PlayerOutput& playerOutput, const PlayerData& playerData);
	void SetPlayerSimulation(AIAnalysis::PlayerSimulation& playerSimulation, const PlayerData& playerData);

	// physics simulation
	void SimulateJump(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph);
	void SimulateJump(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph);
	void SimulateMove(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph);
	void SimulateMove(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph);
	void SimulateFall(PathingNode* pNode, std::shared_ptr<PathingGraph>& graph);
	void SimulateFall(PathingNode* pNode, Transform transform, std::shared_ptr<PathingGraph>& graph);
	void SimulateStanding(ActorId actorId, const Vector3<float>& position, std::shared_ptr<PathingGraph>& graph);
	void SimulateTriggerPush(PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph);
	void SimulateTriggerTeleport(PathingNode* pNode, const Transform& target, std::shared_ptr<PathingGraph>& graph);

	void SimulatePathing(std::shared_ptr<PathingGraph>& graph);
	void SimulatePathing(std::map<unsigned short, unsigned short>& selectedClusters, std::shared_ptr<PathingGraph>& graph);
	void SimulatePathing(Transform transform, NodePlan& pathPlan, std::shared_ptr<PathingGraph>& graph);
	void SimulateVisibility(std::shared_ptr<PathingGraph>& graph);

	Vector3<float> RayCollisionDetection(const Vector3<float>& startPos, const Vector3<float>& collisionPos);

	bool CheckActorNode(PathingNode* pathNode);

	void CreateTransitions(std::shared_ptr<PathingGraph>& graph);
	void CreateClusters(std::shared_ptr<PathingGraph>& graph, unsigned int totalClusters);

	unsigned int GetNewArcID(void) { return ++mLastArcId; }
	unsigned int GetNewNodeID(void) { return ++mLastNodeId; }

	// event delegates
	void PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData);
	void PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData);
	void PhysicsCollisionDelegate(BaseEventDataPtr pEventData);
	void PhysicsSeparationDelegate(BaseEventDataPtr pEventData);

	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);

	void UpdatePlayerItems(ActorId playerId, PlayerView& playerView);
	void UpdatePlayerGuessItems(PlayerGuessView& playerGuessView);
	void UpdatePlayerGuessItems(ActorId playerId, PlayerGuessView& playerGuessView);
	void UpdatePlayerGuessItems(unsigned long deltaMs, ActorId playerId, PlayerGuessView& playerGuessView);

	void UpdatePlayerGuessState(unsigned long deltaMs, PlayerGuessView& playerGuessView);
	void UpdatePlayerGuessState(unsigned long deltaMs, PlayerGuessView& playerGuessView, ActorId playerId);

	void UpdatePlayerState(PlayerView& playerView);
	void UpdatePlayerGuessState(PlayerGuessView& playerGuessView);
	void UpdatePlayerGuessState(PlayerGuessView& playerGuessView, ActorId playerId);

	void UpdatePlayerGuessPlan(std::shared_ptr<PlayerActor> playerActor,
		const PlayerData& playerData, PlayerData& playerGuessData, PathingNode* playerNode);

	bool CheckPlayerGuessItems(PathingNode* playerNode, PlayerGuessView& playerGuessView);
	bool CheckPlayerGuessItems(PathingNode* playerNode, PlayerGuessView& playerGuessView, ActorId playerId);

	void LogEvents(unsigned long deltaMs);

	void PrintPlayerData(const PlayerData& playerData);

	bool mEnable;

	//logs
	std::ofstream mLogError;
	std::ofstream mLogInfo;

	//ai game
	AIGame::Game mGame;
	std::map<ActorId, ActorId> mGameActors;
	std::map<ActorId, const AIAnalysis::ActorPickup*> mGameActorPickups;

	//analysis data
	AIAnalysis::GameAnalysis mGameAnalysis;
	AIAnalysis::GameEvaluation mGameEvaluation;
	AIAnalysis::GameDecision mGameDecision;
	AIAnalysis::Simulation* mGameSimulation;
	Concurrency::concurrent_vector<AIAnalysis::GameDecision> mGameDecisions;

	std::mutex mMutex;
	std::map<ActorId, unsigned int> mAIStates;

	//ai player decision
	std::map<ActorId, EvaluationType> mPlayerEvaluations;

	//players view types
	std::map<GameViewType, ActorId> mPlayers;

	//player views
	std::map<ActorId, std::mutex> mPlayerViewMutex;
	std::map<ActorId, PlayerView> mPlayerViews;

	//player onground
	std::map<ActorId, std::mutex> mPlayerGoundMutex;
	std::map<ActorId, bool> mPlayerGrounds;

	unsigned int mLastArcId;
	unsigned int mLastNodeId;
	int mLastPlanId;

	//set of nodes to be analized from the ground
	std::vector<PathingNode*> mOpenSet, mClosedSet;

	//positions which contains actors from game
	std::map<Vector3<float>, ActorId> mActorPositions;

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

	std::mutex mUpdateMutex;
	int mUpdateCounter;
	unsigned int mUpdateTimeMs;

	float mSimulationStep;

	std::shared_ptr<PlayerActor> mPlayerActor;

};   // QuakeAIManager

#endif
