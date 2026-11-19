/*******************************************************
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#ifndef AIDATA_H
#define AIDATA_H

#include "GameEngineStd.h"

#include "AI/Pathing.h"

#include "../Games/Actors/PlayerActor.h"

class Transform;
class AIPlanNode;

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
	NodePlan();
	NodePlan(PathingNode* playerNode, PathingArcVec& path);

	~NodePlan();

	void AddPathPlan(const PathingArcVec& pathPlan);
	void ResetPathPlan(const PathingArcVec& pathPlan);

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
	PlayerData();
	PlayerData(std::shared_ptr<PlayerActor> playerActor);

	~PlayerData();

	void Update(std::shared_ptr<PlayerActor> playerActor);

	void ResetItems();

	bool IsWeaponSelectable(int i);

	bool valid;
	ActorId player;

	float heuristic; //calculated from players items, damage and health/armor status.

	NodePlan plan;
	float planWeight; //indicates the current position (or weight) in the plan path

	WeaponType weapon;
	float weaponTime; // in sec
	ActorId target;

	std::array<int, MAX_STATS> stats;
	std::array<int, MAX_WEAPONS> ammo;
	std::array<int, MAX_WEAPONS> damage;

	//list of items that the player plan to take or has taken as result of the simulation
	std::unordered_map<ActorId, float> items; //items respawning time in sec.
	std::unordered_map<ActorId, short> itemAmount;
	std::unordered_map<ActorId, float> itemWeight;
};

struct PlayerGuessView
{
	PlayerGuessView()
	{
		threat = 0;
		isUpdated = false;
	}

	PlayerGuessView(std::shared_ptr<PlayerActor> playerActor)
	{
		threat = 0;
		isUpdated = false;

		data = PlayerData(playerActor);
	}

	PlayerGuessView(const PlayerData& playerData)
	{
		threat = 0;
		isUpdated = false;

		data = PlayerData(playerData);
	}

	~PlayerGuessView()
	{

	}

	bool isUpdated;
	unsigned short threat; // threat level of a combat (0 lowest - 4 highest)

	PlayerData data;
	PlayerData simulation;

	//known game items and its respawning time in sec that the player is aware of
	std::map<ActorId, std::map<ActorId, float>> items;
	std::map<ActorId, std::map<ActorId, float>> guessItems;

	std::map<ActorId, PlayerData> guessPlayers;
	std::map<ActorId, PlayerData> guessSimulations;
};

struct PlayerView
{
	PlayerView()
	{
		threat = 0;
		isUpdated = false;
	}

	PlayerView(std::shared_ptr<PlayerActor> playerActor)
	{
		threat = 0;
		isUpdated = false;

		data = PlayerData(playerActor);
	}

	PlayerView(const PlayerData& playerData)
	{
		threat = 0;
		isUpdated = false;

		data = PlayerData(playerData);
	}

	~PlayerView()
	{

	}

	bool isUpdated;
	unsigned short threat; // threat level of a combat (0 lowest - 4 highest)

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
		PathingNode* mNode;

		unsigned int mWait;
		unsigned int mAmount;
		unsigned int mMaximum;

	public:

		ActorPickup(unsigned int code, std::string& type, PathingNode* pNode,
			unsigned int wait, unsigned int amount, unsigned int maximum)
			: mCode(code), mType(type), mNode(pNode), mWait(wait), mAmount(amount), mMaximum(maximum)
		{

		}

		unsigned int GetCode(void) const { return mCode; }
		const std::string& GetType() const { return mType; }
		PathingNode* GetNode() const { return mNode; }

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

		WeaponActorPickup(unsigned int code, std::string& type, PathingNode* pNode,
			unsigned int wait, unsigned int amount, unsigned int maximum, unsigned int ammo)
			: ActorPickup(code, type, pNode, wait, amount, maximum), mAmmo(ammo)
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
			planWeight = 0.f;
			planOffset = 0.f;

			planNode = 0;
			planNodeOffset = 0;
		}

		unsigned short id;
		unsigned short frame;

		ActorId target;
		WeaponType weapon;
		float weaponTime;

		std::array<int, MAX_STATS> stats;
		std::array<int, MAX_WEAPONS> ammo;

		short planId;
		float planWeight, planOffset;
		unsigned short planNode, planNodeOffset;
		std::vector<int> planPath, planPathOffset;

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(id, frame, target, weapon, weaponTime, stats, ammo, planId, planWeight, planOffset, planNode, planPath, planNodeOffset, planPathOffset);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(id, frame, target, weapon, weaponTime, stats, ammo, planId, planWeight, planOffset, planNode, planPath, planNodeOffset, planPathOffset);
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

			threat = 0;

			playerDecision = nullptr;
			playerGuessDecision = nullptr;
		}

		unsigned short type;
		unsigned short target;

		unsigned short threat; // threat level of a combat (0 lowest - 4 highest)

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
			ar(type, target, threat, playerGuessInput, otherPlayerGuessInput, playerGuessOutput, otherPlayerGuessOutput,
				playerInput, otherPlayerInput, playerOutput, otherPlayerOutput, playerDecisionItems, playerGuessItems);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(type, target, threat, playerGuessInput, otherPlayerGuessInput, playerGuessOutput, otherPlayerGuessOutput,
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
	ET_DECISION
};

#endif
