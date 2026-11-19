/*******************************************************
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#ifndef QUAKEAIMANAGER_H
#define QUAKEAIMANAGER_H

#include "QuakeStd.h"

#include "AI/AIPhysicsSimulation.h"
#include "AI/AIGameSimulation.h"
#include "AI/AIManager.h"
#include "AI/AIData.h"

#include <concurrent_vector.h>

class QuakeAIManager : public AIManager
{
	friend class QuakeLogic;
	friend class QuakeAIView;
	friend class QuakeAIAnalyzerView;

public:
	QuakeAIManager();
	~QuakeAIManager();

	void LoadPathingMap(const std::wstring& path, float weightConversion = 1.0f);
	void LoadPathingMap(const std::wstring& path, std::shared_ptr<PathingGraph>& graph, float weightConversion = 1.0f);

	void UpdateMap(std::shared_ptr<PathingGraph>& graph, ActorId playerId);
	void UpdateMap(ActorId playerId);

	virtual void SaveGraph(const std::string& path);
	virtual void SaveGraph(const std::string& path, std::shared_ptr<PathingGraph>& graph);
	virtual void LoadGraph(const std::wstring& path, float weightConversion = 1.0f);
	virtual void LoadGraph(const std::wstring& path, std::shared_ptr<PathingGraph>& graph, float weightConversion = 1.0f);

	virtual void OnUpdate(unsigned long deltaMs);

	bool IsEnable() { return mEnable; }
	void SetEnable(bool enable) { mEnable = enable; }

	std::map<ActorId, ActorId>& GetGameActors() { return mGameActors; }
	std::map<ActorId, const AIAnalysis::ActorPickup*>& GetGameActorPickups() { return mGameSimulation.mGameActorPickups; }
	const AIAnalysis::ActorPickup* GetGameActorPickup(ActorId actorId) { 
		return mGameSimulation.mGameActorPickups.find(actorId) != 
			mGameSimulation.mGameActorPickups.end() ? 
			mGameSimulation.mGameActorPickups.at(actorId) : NULL; }

	const AIGame::Game& GetGame() { return mGame; }
	void AddGameItem(const AIGame::Item& item) { mGame.states.back().items.push_back(item); }
	void AddGamePlayer(const AIGame::Player& player) { mGame.states.back().players.push_back(player); }
	void AddGameProjectile(const AIGame::Projectile& projectile) { mGame.states.back().projectiles.push_back(projectile); }
	void AddGameEventTrack(const AIGame::EventTrack& track) { mGame.states.back().tracks.push_back(track); }
	void AddGameEvent(const AIGame::Event& evt) { mGame.states.back().tracks.back().events.push_back(evt); }
	void AddGameState(const AIGame::GameState& gameState) { mGame.states.push_back(gameState); }

	AIAnalysis::Simulation* GetCurrentGameSimulation() { return mCurrentGameSimulation; }
	void SetCurrentGameSimulation(AIAnalysis::Simulation* simulation) { mCurrentGameSimulation = simulation; }

	AIAnalysis::GameEvaluation& GetCurrentGameEvaluation() { return mCurrentGameEvaluation; }
	void SetCurrentGameEvaluation(AIAnalysis::GameEvaluation& gameEvaluation) { mCurrentGameEvaluation = gameEvaluation; }

	AIAnalysis::GameAnalysis& GetGameAnalysis() { return mGameAnalysis; }

	AIGameSimulation& GetGameSimulation() { return mGameSimulation; }
	AIPhysicsSimulation& GetPhysicsSimulation() { return mPhysicsSimulation; }

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
	void UpdatePlayerView(ActorId player, const std::map<ActorId, float>& gameItems);
	void UpdatePlayerView(ActorId player, float planWeight);
	void UpdatePlayerGuessView(ActorId player, const PlayerGuessView& playerGuessView, bool isUpdated);

	void UpdatePlayerSimulationView(ActorId player, const PlayerGuessView& playerGuessView);
	void UpdatePlayerSimulationView(ActorId player, const PlayerView& playerView);

	void SpawnActor(ActorId playerId);
	void DetectPlayer(std::shared_ptr<PlayerActor> playerActor);
	void DetectActor(std::shared_ptr<PlayerActor> playerActor, std::shared_ptr<Actor> item);

	void PrintError(std::string data);
	void PrintInfo(std::string data);

protected:

	float CalculatePathingWeight(const PlayerData& playerData);
	Vector3<float> CalculatePathingPosition(const PlayerData& playerData);

	//players viewType
	void OnAttach(GameViewType vtype, ActorId aid)
	{
		mPlayers[vtype] = aid;
		mPlayerEvaluations[aid] = ET_GUESSING;
	}

	bool MakeAIGuessing(PlayerView& aiView);
	bool MakeAIDecision(PlayerView& aiView);
	bool MakeAIGuessingDecision(PlayerView& aiView);
	bool MakeAIAwareDecision(PlayerView& aiView);
	bool MakeHumanGuessing(PlayerView& playerView);
	bool MakeHumanDecision(PlayerView& playerView);
	bool MakeHumanGuessingDecision(PlayerView& playerView);
	bool MakeHumanAwareDecision(PlayerView& playerView);

	void RunAIGuessing();
	void RunAIDecision();
	void RunAIGuessingDecision();
	void RunAIAwareDecision();
	void RunAIAwareness();

	void RunHumanGuessing();
	void RunHumanDecision();
	void RunHumanGuessingDecision();
	void RunHumanAwareDecision();
	void RunHumanAwareness();

	unsigned int GetFrame() { return mUpdateCounter; }

private:

	// AI decision making data
	void GetPlayerInput(const AIAnalysis::PlayerInput& playerInput, PlayerData& playerData);
	void GetPlayerInput(const AIAnalysis::PlayerInput& playerInput, PlayerData& playerData, PlayerData& playerDataOffset);
	void GetPlayerOutput(const AIAnalysis::PlayerOutput& playerOutput, PlayerData& playerData);

	void SetPlayerInput(AIAnalysis::PlayerInput& playerInput, const PlayerData& playerData, const PlayerData& playerDataOffset);
	void SetPlayerOutput(AIAnalysis::PlayerOutput& playerOutput, const PlayerData& playerData);

	void InitializePlayerItems(PlayerView& playerView);
	void InitializePlayerGuessItems(PlayerGuessView& playerGuessView, ActorId playerId);
	void UpdatePlayerItems(unsigned long deltaMs, PathingNode* playerNode, PlayerView& playerView);
	void UpdatePlayerGuessItems(unsigned long deltaMs, PlayerGuessView& playerGuessView);

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

	//ai physics
	AIPhysicsSimulation mPhysicsSimulation;

	//ai game
	AIGame::Game mGame;
	std::map<ActorId, ActorId> mGameActors;

	//analysis data
	AIGameSimulation mGameSimulation;
	AIAnalysis::GameAnalysis mGameAnalysis;

	AIAnalysis::GameEvaluation mCurrentGameEvaluation;
	AIAnalysis::GameDecision mCurrentGameDecision;
	AIAnalysis::Simulation* mCurrentGameSimulation;
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

	std::mutex mUpdateMutex;
	int mUpdateCounter;
	unsigned int mUpdateTimeMs;

};   // QuakeAIManager

#endif
