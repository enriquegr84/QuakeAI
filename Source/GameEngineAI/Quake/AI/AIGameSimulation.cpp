/*******************************************************
 * AIGameSimulation.cpp : AI Game simulation class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#include "AIGameSimulation.h"

#include "Core/OS/OS.h"

#include "../Quake.h"

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
	Concurrency::parallel_for_each(mNodes.begin(), mNodes.end(), [&](auto& plan)
	//for (ActorToAIPlanNodeMap::iterator it = mNodes.begin(); it != mNodes.end(); ++it)
	{
		Concurrency::parallel_for_each(plan.second.begin(), plan.second.end(), [&](auto& planNode)
		//for (AIPlanNodeVector::iterator itPlan = (*it).second.begin(); itPlan != (*it).second.end(); itPlan++)
		{
			delete planNode;
		});
	});
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
		AIPlanNode* planNode;
		mOpenSet.try_pop(planNode);

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

		// lets put a threshold to the added search margin weight
		float weightMargin = 1.f;

		// we're processing this node so remove it from the open set and add it to the closed set
		AddToClosedSet(planNode);

		// get the neighboring actors
		const PathingActorMap& neighbors = planNode->GetPathingActor() ?
			planNode->GetPathingActor()->GetTarget()->GetActors() : planNode->GetPathingNode()->GetActors();

		// loop though all the neighboring actors and evaluate each one
		Concurrency::parallel_for_each(neighbors.begin(), neighbors.end(), [&](auto& neighbor)
		//for (auto it = neighbors.begin(); it != neighbors.end(); ++it)
		{
			PathingActor* pActorToEvaluate = neighbor.second;
			if (pActorToEvaluate->GetType() != pathingType)
				return;

			if (planNode->GetPathingActor())
			{
				if (planNode->FindActor(pActorToEvaluate->GetActor()))
					return;

				AIPlanNode* pPlanToCompare = mNodes[pActorToEvaluate->GetActor()].front();
				float costForActorPlan = pPlanToCompare->GetWeight() + weightMargin * searchItems.at(pActorToEvaluate->GetActor());

				// figure out the cost for this route through the node
				float costForThisPlan = planNode->GetWeight() +
					planNode->CalculatePathCost(planNode->GetPathingActor()->GetTarget(), pActorToEvaluate);
				if (costForThisPlan > costForActorPlan)
					return;

				AddToOpenSet(planNode->GetPathingActor()->GetTarget(), pActorToEvaluate, planNode);
			}
			else AddToOpenSet(planNode->GetPathingNode(), pActorToEvaluate, planNode);
		});
	}
	/*
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	aiManager->PrintInfo("\n actors");
	for (auto planActor : mNodes)
	{
		aiManager->PrintInfo("\n new plan \n");
		for (auto planNode : planActor.second)
		{
			for (auto planNodeActor : planNode->GetActors())
			{
				std::shared_ptr<Actor> pItemActor(
					GameLogic::Get()->GetActor(planNodeActor.first).lock());
				if (pItemActor->GetType() == "Weapon")
				{
					std::shared_ptr<WeaponPickup> pWeaponPickup =
						pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
					aiManager->PrintInfo(" weapon " + std::to_string(pWeaponPickup->GetCode()));
				}
				else if (pItemActor->GetType() == "Ammo")
				{
					std::shared_ptr<AmmoPickup> pAmmoPickup =
						pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
					aiManager->PrintInfo(" ammo " + std::to_string(pAmmoPickup->GetCode()));
				}
				else if (pItemActor->GetType() == "Armor")
				{
					std::shared_ptr<ArmorPickup> pArmorPickup =
						pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
					aiManager->PrintInfo(" armor " + std::to_string(pArmorPickup->GetCode()));
				}
				else if (pItemActor->GetType() == "Health")
				{
					std::shared_ptr<HealthPickup> pHealthPickup =
						pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
					aiManager->PrintInfo(" health " + std::to_string(pHealthPickup->GetCode()));
				}
				else 
				{ 
					aiManager->PrintInfo(" other " + std::to_string(planNodeActor.first)); 
				}
				aiManager->PrintInfo(" weight " + std::to_string(planNodeActor.second));
			}
			aiManager->PrintInfo(" node " + 
				std::to_string(planNode->GetPathingNode()->GetId()) + " weight " + std::to_string(planNode->GetWeight()) + "\n");
		}
	}
	*/
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
	mOpenSet.push(pThisNode);
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

AIGameSimulation::AIGameSimulation()
{
	mLastPlanId = 0;

}   // AIGameSimulation

//-----------------------------------------------------------------------------

AIGameSimulation::~AIGameSimulation()
{

}   // ~AIGameSimulation

/////////////////////////////////////////////////////////////////////////////
// AI Decision Making
//

void AIGameSimulation::PerformGuessingMaking(
	const AIAnalysis::GameEvaluation& gameEvaluation, const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	//lets filter guessings in which the opponent has weapon advantage against all the player actions
	std::unordered_map<unsigned long long, float> playerGuessingHeuristics, otherPlayerGuessingHeuristics;
	for (auto& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto& simulation : playerGuessingSimulation->simulations)
		{
			playerGuessingHeuristics[simulation->playerSimulation.code] = 0;
			otherPlayerGuessingHeuristics[simulation->otherPlayerSimulation.code] = 0;
		}
	}

	//calculate other guessing average 
	for (auto& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto& simulation : playerGuessingSimulation->simulations)
		{
			//other cluster code
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			//invert heuristic for opponent
			otherPlayerGuessingHeuristics[otherClusterCode] -= simulation->otherPlayerSimulation.heuristic;
		}
	}

	//lets assign a strong proportional probability to each opponent guessing based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> otherPlayerGuessingProbabilities;
	if (!otherPlayerGuessingHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			//calculate avg
			otherPlayerGuessingHeuristic.second /= (float)playerGuessingHeuristics.size();
			if (otherPlayerGuessingHeuristic.second < minHeuristicValue)
				minHeuristicValue = otherPlayerGuessingHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			unsigned long long otherClusterCode = otherPlayerGuessingHeuristic.first;

			// Shift to positive
			float shifted = otherPlayerGuessingHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			otherPlayerGuessingProbabilities[otherClusterCode] = std::pow(shifted, power);
			sumTotal += otherPlayerGuessingProbabilities[otherClusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)otherPlayerGuessingProbabilities.size();

		// Normalize to probabilities
		for (auto& otherPlayerGuessingProb : otherPlayerGuessingProbabilities)
		{
			unsigned long long otherClusterCode = otherPlayerGuessingProb.first;
			otherPlayerGuessingProbabilities[otherClusterCode] /= sumTotal;
		}
	}

	//calculate each average and take the best outcome for both players
	for (auto& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto& simulation : playerGuessingSimulation->simulations)
		{
			//other cluster code
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			playerGuessingHeuristics[clusterCode] += simulation->playerSimulation.heuristic * otherPlayerGuessingProbabilities[otherClusterCode];
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
	if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.05f)
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
		if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.05f)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (playerGuessingHeuristics.at(ULLONG_MAX) > playerClusterHeuristic)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> playerGuessingWeapons;
	for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
	{
		unsigned long long clusterCode = playerGuessingHeuristic.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerGuessingWeapons[clusterCode][(WeaponType)weapon] = 0.f;
	}
	for (auto const& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto const& simulation : playerGuessingSimulation->simulations)
		{
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;

			unsigned short playerWeaponType = simulation->playerSimulation.weapon;
			playerGuessingWeapons[clusterCode][(WeaponType)playerWeaponType] += otherPlayerGuessingProbabilities[otherClusterCode];
		}
	}
	float bestWeaponProbability = -FLT_MAX;
	for (auto& playerGuessingWeapon : playerGuessingWeapons[otherPlayerClusterCode])
	{
		if (playerGuessingWeapon.second > bestWeaponProbability)
		{
			playerWeapon = playerGuessingWeapon.first;
			bestWeaponProbability = playerGuessingWeapon.second;
		}
	}



	//calculate player guessing average 
	for (auto& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto const& simulation : playerGuessingSimulation->simulations)
		{
			playerGuessingHeuristics[simulation->playerSimulation.code] = 0;
			otherPlayerGuessingHeuristics[simulation->otherPlayerSimulation.code] = 0;
		}
	}
	for (auto& playerGuessingSimulation : gameEvaluation.playerGuessings)
		for (auto const& simulation : playerGuessingSimulation->simulations)
			playerGuessingHeuristics[simulation->playerSimulation.code] += simulation->playerSimulation.heuristic;

	//lets assign a strong proportional probability to each player guessing based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> playerGuessingProbabilities;
	if (!playerGuessingHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			//calculate avg
			playerGuessingHeuristic.second /= (float)otherPlayerGuessingHeuristics.size();
			if (playerGuessingHeuristic.second < minHeuristicValue)
				minHeuristicValue = playerGuessingHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			unsigned long long clusterCode = playerGuessingHeuristic.first;

			// Shift to positive
			float shifted = playerGuessingHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			playerGuessingProbabilities[clusterCode] = std::pow(shifted, power);
			sumTotal += playerGuessingProbabilities[clusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)playerGuessingProbabilities.size();

		// Normalize to probabilities
		for (auto& playerGuessingProb : playerGuessingProbabilities)
		{
			unsigned long long clusterCode = playerGuessingProb.first;
			playerGuessingProbabilities[clusterCode] /= sumTotal;
		}
	}

	//calculate each average and take the best outcome for the other player
	for (auto& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto& simulation : playerGuessingSimulation->simulations)
		{
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			otherPlayerGuessingHeuristics[otherClusterCode] += simulation->otherPlayerSimulation.heuristic * playerGuessingProbabilities[clusterCode];
		}
	}

	float otherPlayerClusterHeuristic = FLT_MAX;
	for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
	{
		float otherPlayerGuessingHeuristicAvg = otherPlayerGuessingHeuristic.second;
		//lets take the value which minimize players heuristic
		if (otherPlayerGuessingHeuristicAvg < otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
			otherPlayerClusterHeuristic = otherPlayerGuessingHeuristicAvg;
		}
	}
	if (otherPlayerClusterCode != ULLONG_MAX && otherPlayerGuessingHeuristics.find(ULLONG_MAX) != otherPlayerGuessingHeuristics.end())
	{
		//we keep the current plan if the heuristic is close to the best other player heuristic
		if (abs(otherPlayerGuessingHeuristics.at(ULLONG_MAX) - otherPlayerClusterHeuristic) < 0.05f)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerGuessingHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (otherPlayerGuessingHeuristics.at(ULLONG_MAX) > otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerGuessingHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> otherPlayerGuessingWeapons;
	for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
	{
		unsigned long long otherClusterCode = otherPlayerGuessingHeuristic.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerGuessingWeapons[otherClusterCode][(WeaponType)weapon] = 0.f;
	}
	for (auto const& playerGuessingSimulation : gameEvaluation.playerGuessings)
	{
		for (auto const& simulation : playerGuessingSimulation->simulations)
		{
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			unsigned short otherPlayerWeaponType = simulation->otherPlayerSimulation.weapon;
			otherPlayerGuessingWeapons[otherClusterCode][(WeaponType)otherPlayerWeaponType] += playerGuessingProbabilities[clusterCode];
		}
	}
	float bestOtherWeaponProbability = -FLT_MAX;
	for (auto& otherPlayerGuessingWeapon : otherPlayerGuessingWeapons[otherPlayerClusterCode])
	{
		if (otherPlayerGuessingWeapon.second > bestOtherWeaponProbability)
		{
			otherPlayerWeapon = otherPlayerGuessingWeapon.first;
			bestOtherWeaponProbability = otherPlayerGuessingWeapon.second;
		}
	}
}

void AIGameSimulation::PerformDecisionMaking(const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, float>>& playerDecisions,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>>& playerWeaponDecisions,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	//lets filter decisions in which the opponent has weapon advantage against all the player actions
	std::unordered_map<unsigned long long, float> playerDecisionHeuristics, otherPlayerDecisionHeuristics;
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

	//calculate other decision average 
	for (auto& playerDecisionSimulation : playerDecisions)
	{
		//cluster code
		unsigned long long clusterCode = playerDecisionSimulation.first;
		playerDecisionHeuristics[clusterCode] = 0;

		for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
			//invert heuristic for opponent
			otherPlayerDecisionHeuristics[otherClusterCode] -= otherPlayerDecisionSimulation.second;
		}
	}

	//lets assign a strong proportional probability to each opponent decision based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> otherPlayerDecisionProbabilities;
	if (!otherPlayerDecisionHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			//calculate avg
			otherPlayerDecisionHeuristic.second /= (float)playerDecisionHeuristics.size();
			if (otherPlayerDecisionHeuristic.second < minHeuristicValue)
				minHeuristicValue = otherPlayerDecisionHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			unsigned long long otherClusterCode = otherPlayerDecisionHeuristic.first;

			// Shift to positive
			float shifted = otherPlayerDecisionHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			otherPlayerDecisionProbabilities[otherClusterCode] = std::pow(shifted, power);
			sumTotal += otherPlayerDecisionProbabilities[otherClusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)otherPlayerDecisionProbabilities.size();

		// Normalize to probabilities
		for (auto& otherPlayerDecisionProb : otherPlayerDecisionProbabilities)
		{
			unsigned long long otherClusterCode = otherPlayerDecisionProb.first;
			otherPlayerDecisionProbabilities[otherClusterCode] /= sumTotal;
		}
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
			playerDecisionHeuristics[clusterCode] += otherPlayerDecisionSimulation.second * otherPlayerDecisionProbabilities[otherClusterCode];
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
	if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.05f)
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
		if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.05f)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (playerDecisionHeuristics.at(ULLONG_MAX) > playerClusterHeuristic)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> playerDecisionWeapons;
	for (auto& playerWeaponDecision : playerWeaponDecisions)
	{
		//cluster code
		unsigned long long clusterCode = playerWeaponDecision.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerDecisionWeapons[clusterCode][(WeaponType)weapon] = 0.f;

		for (auto& otherPlayerWeaponDecision : playerWeaponDecision.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerWeaponDecision.first;

			unsigned short playerWeaponType = (otherPlayerWeaponDecision.second >> 8) & 0xff;
			playerDecisionWeapons[clusterCode][(WeaponType)playerWeaponType] += otherPlayerDecisionProbabilities[otherClusterCode];
		}
	}
	float bestWeaponProbability = -FLT_MAX;
	for (auto& playerDecisionWeapon : playerDecisionWeapons[playerClusterCode])
	{
		if (playerDecisionWeapon.second > bestWeaponProbability)
		{
			playerWeapon = playerDecisionWeapon.first;
			bestWeaponProbability = playerDecisionWeapon.second;
		}
	}



	//calculate player decision average 
	for (auto& playerDecisionSimulation : playerDecisions)
	{
		//cluster code
		unsigned long long clusterCode = playerDecisionSimulation.first;
		playerDecisionHeuristics[clusterCode] = 0.f;
	}
	for (auto& playerDecisionSimulation : playerDecisions)
	{
		//cluster code
		unsigned long long clusterCode = playerDecisionSimulation.first;

		for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;

			otherPlayerDecisionHeuristics[otherClusterCode] = 0.f;
			playerDecisionHeuristics[clusterCode] += otherPlayerDecisionSimulation.second;
		}
	}

	//lets assign a strong proportional probability to each player decision based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> playerDecisionProbabilities;
	if (!playerDecisionHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			//calculate avg
			playerDecisionHeuristic.second /= (float)otherPlayerDecisionHeuristics.size();
			if (playerDecisionHeuristic.second < minHeuristicValue)
				minHeuristicValue = playerDecisionHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			unsigned long long clusterCode = playerDecisionHeuristic.first;

			// Shift to positive
			float shifted = playerDecisionHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			playerDecisionProbabilities[clusterCode] = std::pow(shifted, power);
			sumTotal += playerDecisionProbabilities[clusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)playerDecisionProbabilities.size();

		// Normalize to probabilities
		for (auto& playerDecisionProb : playerDecisionProbabilities)
		{
			unsigned long long clusterCode = playerDecisionProb.first;
			playerDecisionProbabilities[clusterCode] /= sumTotal;
		}
	}

	//calculate each average and take the best outcome for the other player
	for (auto& playerDecisionSimulation : playerDecisions)
	{
		//cluster code
		unsigned long long clusterCode = playerDecisionSimulation.first;

		for (auto& otherPlayerDecisionSimulation : playerDecisionSimulation.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerDecisionSimulation.first;
			otherPlayerDecisionHeuristics[otherClusterCode] += otherPlayerDecisionSimulation.second * playerDecisionProbabilities[clusterCode];
		}
	}

	float otherPlayerClusterHeuristic = FLT_MAX;
	for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
	{
		float otherPlayerDecisionHeuristicAvg = otherPlayerDecisionHeuristic.second;
		//lets take the value which minimize players heuristic
		if (otherPlayerDecisionHeuristicAvg < otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
			otherPlayerClusterHeuristic = otherPlayerDecisionHeuristicAvg;
		}
	}
	if (otherPlayerClusterCode != ULLONG_MAX && otherPlayerDecisionHeuristics.find(ULLONG_MAX) != otherPlayerDecisionHeuristics.end())
	{
		//we keep the current plan if the heuristic is close to the best other player heuristic
		if (abs(otherPlayerDecisionHeuristics.at(ULLONG_MAX) - otherPlayerClusterHeuristic) < 0.05f)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerDecisionHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (otherPlayerDecisionHeuristics.at(ULLONG_MAX) > otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerDecisionHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> otherPlayerDecisionWeapons;
	for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
	{
		//other cluster code
		unsigned long long otherClusterCode = otherPlayerDecisionHeuristic.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerDecisionWeapons[otherClusterCode][(WeaponType)weapon] = 0.f;
	}
	for (auto& playerWeaponDecision : playerWeaponDecisions)
	{
		//cluster code
		unsigned long long clusterCode = playerWeaponDecision.first;

		for (auto& otherPlayerWeaponDecision : playerWeaponDecision.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerWeaponDecision.first;

			unsigned short otherPlayerWeaponType = otherPlayerWeaponDecision.second & 0xff;
			otherPlayerDecisionWeapons[otherClusterCode][(WeaponType)otherPlayerWeaponType] += playerDecisionProbabilities[clusterCode];
		}
	}
	float bestOtherWeaponProbability = -FLT_MAX;
	for (auto& otherPlayerDecisionWeapon : otherPlayerDecisionWeapons[otherPlayerClusterCode])
	{
		if (otherPlayerDecisionWeapon.second > bestOtherWeaponProbability)
		{
			otherPlayerWeapon = otherPlayerDecisionWeapon.first;
			bestOtherWeaponProbability = otherPlayerDecisionWeapon.second;
		}
	}
}

void AIGameSimulation::PerformGuessingMaking(const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, float>>& playerGuessings,
	const Concurrency::concurrent_unordered_map<unsigned long long, Concurrency::concurrent_unordered_map<unsigned long long, unsigned short>>& playerWeaponGuessings,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	//lets filter guessings in which the opponent has weapon advantage against all the player actions
	std::unordered_map<unsigned long long, float> playerGuessingHeuristics, otherPlayerGuessingHeuristics;
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

	//calculate other guessing average 
	for (auto& playerGuessingSimulation : playerGuessings)
	{
		//cluster code
		unsigned long long clusterCode = playerGuessingSimulation.first;
		playerGuessingHeuristics[clusterCode] = 0;

		for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
			//invert heuristic for opponent
			otherPlayerGuessingHeuristics[otherClusterCode] -= otherPlayerGuessingSimulation.second;
		}
	}

	//lets assign a strong proportional probability to each opponent guessing based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> otherPlayerGuessingProbabilities;
	if (!otherPlayerGuessingHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			//calculate avg
			otherPlayerGuessingHeuristic.second /= (float)playerGuessingHeuristics.size();
			if (otherPlayerGuessingHeuristic.second < minHeuristicValue)
				minHeuristicValue = otherPlayerGuessingHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
		{
			unsigned long long otherClusterCode = otherPlayerGuessingHeuristic.first;

			// Shift to positive
			float shifted = otherPlayerGuessingHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			otherPlayerGuessingProbabilities[otherClusterCode] = std::pow(shifted, power);
			sumTotal += otherPlayerGuessingProbabilities[otherClusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)otherPlayerGuessingProbabilities.size();

		// Normalize to probabilities
		for (auto& otherPlayerGuessingProb : otherPlayerGuessingProbabilities)
		{
			unsigned long long otherClusterCode = otherPlayerGuessingProb.first;
			otherPlayerGuessingProbabilities[otherClusterCode] /= sumTotal;
		}
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
			playerGuessingHeuristics[clusterCode] += otherPlayerGuessingSimulation.second * otherPlayerGuessingProbabilities[otherClusterCode];
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
	if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.05f)
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
		if (abs(playerGuessingHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.05f)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (playerGuessingHeuristics.at(ULLONG_MAX) > playerClusterHeuristic)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerGuessingHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> playerGuessingWeapons;
	for (auto& playerWeaponGuessing : playerWeaponGuessings)
	{
		//cluster code
		unsigned long long clusterCode = playerWeaponGuessing.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerGuessingWeapons[clusterCode][(WeaponType)weapon] = 0.f;

		for (auto& otherPlayerWeaponGuessing : playerWeaponGuessing.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerWeaponGuessing.first;

			unsigned short playerWeaponType = (otherPlayerWeaponGuessing.second >> 8) & 0xff;
			playerGuessingWeapons[clusterCode][(WeaponType)playerWeaponType] += otherPlayerGuessingProbabilities[otherClusterCode];
		}
	}
	float bestWeaponProbability = -FLT_MAX;
	for (auto& playerGuessingWeapon : playerGuessingWeapons[playerClusterCode])
	{
		if (playerGuessingWeapon.second > bestWeaponProbability)
		{
			playerWeapon = playerGuessingWeapon.first;
			bestWeaponProbability = playerGuessingWeapon.second;
		}
	}


	//calculate player guessing average 
	for (auto& playerGuessingSimulation : playerGuessings)
	{
		//cluster code
		unsigned long long clusterCode = playerGuessingSimulation.first;
		playerGuessingHeuristics[clusterCode] = 0.f;
	}
	for (auto& playerGuessingSimulation : playerGuessings)
	{
		//cluster code
		unsigned long long clusterCode = playerGuessingSimulation.first;

		for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;

			otherPlayerGuessingHeuristics[otherClusterCode] = 0.f;
			playerGuessingHeuristics[clusterCode] += otherPlayerGuessingSimulation.second;
		}
	}

	//lets assign a strong proportional probability to each player guessing based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> playerGuessingProbabilities;
	if (!playerGuessingHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			//calculate avg
			playerGuessingHeuristic.second /= (float)otherPlayerGuessingHeuristics.size();
			if (playerGuessingHeuristic.second < minHeuristicValue)
				minHeuristicValue = playerGuessingHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& playerGuessingHeuristic : playerGuessingHeuristics)
		{
			unsigned long long clusterCode = playerGuessingHeuristic.first;

			// Shift to positive
			float shifted = playerGuessingHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			playerGuessingProbabilities[clusterCode] = std::pow(shifted, power);
			sumTotal += playerGuessingProbabilities[clusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)playerGuessingProbabilities.size();

		// Normalize to probabilities
		for (auto& playerGuessingProb : playerGuessingProbabilities)
		{
			unsigned long long clusterCode = playerGuessingProb.first;
			playerGuessingProbabilities[clusterCode] /= sumTotal;
		}
	}

	//calculate each average and take the best outcome for the other player
	for (auto& playerGuessingSimulation : playerGuessings)
	{
		//cluster code
		unsigned long long clusterCode = playerGuessingSimulation.first;

		for (auto& otherPlayerGuessingSimulation : playerGuessingSimulation.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerGuessingSimulation.first;
			otherPlayerGuessingHeuristics[otherClusterCode] += otherPlayerGuessingSimulation.second * playerGuessingProbabilities[clusterCode];
		}
	}

	float otherPlayerClusterHeuristic = FLT_MAX;
	for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
	{
		float otherPlayerGuessingHeuristicAvg = otherPlayerGuessingHeuristic.second;
		//lets take the value which minimize players heuristic
		if (otherPlayerGuessingHeuristicAvg < otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = otherPlayerGuessingHeuristic.first;
			otherPlayerClusterHeuristic = otherPlayerGuessingHeuristicAvg;
		}
	}
	if (otherPlayerClusterCode != ULLONG_MAX && otherPlayerGuessingHeuristics.find(ULLONG_MAX) != otherPlayerGuessingHeuristics.end())
	{
		//we keep the current plan if the heuristic is close to the best other player heuristic
		if (abs(otherPlayerGuessingHeuristics.at(ULLONG_MAX) - otherPlayerClusterHeuristic) < 0.05f)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerGuessingHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (otherPlayerGuessingHeuristics.at(ULLONG_MAX) > otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerGuessingHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> otherPlayerGuessingWeapons;
	for (auto& otherPlayerGuessingHeuristic : otherPlayerGuessingHeuristics)
	{
		//other cluster code
		unsigned long long otherClusterCode = otherPlayerGuessingHeuristic.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerGuessingWeapons[otherClusterCode][(WeaponType)weapon] = 0.f;
	}
	for (auto& playerWeaponGuessing : playerWeaponGuessings)
	{
		//cluster code
		unsigned long long clusterCode = playerWeaponGuessing.first;

		for (auto& otherPlayerWeaponGuessing : playerWeaponGuessing.second)
		{
			//other cluster code
			unsigned long long otherClusterCode = otherPlayerWeaponGuessing.first;

			unsigned short otherPlayerWeaponType = otherPlayerWeaponGuessing.second & 0xff;
			otherPlayerGuessingWeapons[otherClusterCode][(WeaponType)otherPlayerWeaponType] += playerGuessingProbabilities[clusterCode];
		}
	}
	float bestOtherWeaponProbability = -FLT_MAX;
	for (auto& otherPlayerGuessingWeapon : otherPlayerGuessingWeapons[otherPlayerClusterCode])
	{
		if (otherPlayerGuessingWeapon.second > bestOtherWeaponProbability)
		{
			otherPlayerWeapon = otherPlayerGuessingWeapon.first;
			bestOtherWeaponProbability = otherPlayerGuessingWeapon.second;
		}
	}
}

void AIGameSimulation::PerformDecisionMaking(
	const AIAnalysis::GameEvaluation& gameEvaluation, const PlayerData& playerDataIn, const PlayerData& otherPlayerDataIn,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	WeaponType& playerWeapon, WeaponType& otherPlayerWeapon, unsigned long long& playerClusterCode, unsigned long long& otherPlayerClusterCode)
{
	//lets filter decisions in which the opponent has weapon advantage against all the player actions
	std::unordered_map<unsigned long long, float> playerDecisionHeuristics, otherPlayerDecisionHeuristics;
	for (auto& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto& simulation : playerDecisionSimulation->simulations)
		{
			playerDecisionHeuristics[simulation->playerSimulation.code] = 0;
			otherPlayerDecisionHeuristics[simulation->otherPlayerSimulation.code] = 0;
		}
	}

	//calculate other decision average 
	for (auto& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto& simulation : playerDecisionSimulation->simulations)
		{
			//other cluster code
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			//invert heuristic for opponent
			otherPlayerDecisionHeuristics[otherClusterCode] -= simulation->otherPlayerSimulation.heuristic;
		}
	}

	//lets assign a strong proportional probability to each opponent decision based on the calculated average heuristics
	std::multimap<float, unsigned long long, std::greater<float>> sortedOtherPlayerDecisionHeuristics;
	std::multimap<float, unsigned long long, std::greater<float>> sortedOtherPlayerDecisionProbabilities;
	std::unordered_map<unsigned long long, float> otherPlayerDecisionProbabilities;
	if (!otherPlayerDecisionHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			//calculate avg
			otherPlayerDecisionHeuristic.second /= (float)playerDecisionHeuristics.size();
			if (otherPlayerDecisionHeuristic.second < minHeuristicValue)
				minHeuristicValue = otherPlayerDecisionHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
		{
			unsigned long long otherClusterCode = otherPlayerDecisionHeuristic.first;

			// Shift to positive
			float shifted = otherPlayerDecisionHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			otherPlayerDecisionProbabilities[otherClusterCode] = std::pow(shifted, power);
			sumTotal += otherPlayerDecisionProbabilities[otherClusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)otherPlayerDecisionProbabilities.size();

		// Normalize to probabilities
		for (auto& otherPlayerDecisionProb : otherPlayerDecisionProbabilities)
		{
			unsigned long long otherClusterCode = otherPlayerDecisionProb.first;
			otherPlayerDecisionProbabilities[otherClusterCode] /= sumTotal;
			sortedOtherPlayerDecisionHeuristics.insert({ otherPlayerDecisionHeuristics[otherClusterCode], otherClusterCode });
			sortedOtherPlayerDecisionProbabilities.insert({ otherPlayerDecisionProbabilities[otherClusterCode], otherClusterCode });
		}
	}

	//calculate each average and take the best outcome for both players
	for (auto& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto& simulation : playerDecisionSimulation->simulations)
		{
			//other cluster code
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			playerDecisionHeuristics[clusterCode] += simulation->playerSimulation.heuristic * otherPlayerDecisionProbabilities[otherClusterCode];
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
	if (playerClusterJumpHeuristic - playerClusterMoveHeuristic > 0.05f)
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
		if (abs(playerDecisionHeuristics.at(ULLONG_MAX) - playerClusterHeuristic) < 0.05f)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (playerDecisionHeuristics.at(ULLONG_MAX) > playerClusterHeuristic)
		{
			playerClusterCode = ULLONG_MAX;
			playerClusterHeuristic = playerDecisionHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> playerDecisionWeapons;
	for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
	{
		unsigned long long clusterCode = playerDecisionHeuristic.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			playerDecisionWeapons[clusterCode][(WeaponType)weapon] = 0.f;
	}
	for (auto const& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto const& simulation : playerDecisionSimulation->simulations)
		{
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;

			unsigned short playerWeaponType = simulation->playerSimulation.weapon;
			playerDecisionWeapons[clusterCode][(WeaponType)playerWeaponType] += otherPlayerDecisionProbabilities[otherClusterCode];
		}
	}
	float bestWeaponProbability = -FLT_MAX;
	for (auto& playerDecisionWeapon : playerDecisionWeapons[otherPlayerClusterCode])
	{
		if (playerDecisionWeapon.second > bestWeaponProbability)
		{
			playerWeapon = playerDecisionWeapon.first;
			bestWeaponProbability = playerDecisionWeapon.second;
		}
	}


	//calculate player decision average 
	for (auto& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto const& simulation : playerDecisionSimulation->simulations)
		{
			playerDecisionHeuristics[simulation->playerSimulation.code] = 0;
			otherPlayerDecisionHeuristics[simulation->otherPlayerSimulation.code] = 0;
		}
	}
	for (auto& playerDecisionSimulation : gameEvaluation.playerDecisions)
		for (auto const& simulation : playerDecisionSimulation->simulations)
			playerDecisionHeuristics[simulation->playerSimulation.code] += simulation->playerSimulation.heuristic;

	//lets assign a strong proportional probability to each player decision based on the calculated average heuristics
	std::unordered_map<unsigned long long, float> playerDecisionProbabilities;
	if (!playerDecisionHeuristics.empty())
	{
		// Find minimum to handle negative values safely
		float minHeuristicValue = FLT_MAX;
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			//calculate avg
			playerDecisionHeuristic.second /= (float)otherPlayerDecisionHeuristics.size();
			if (playerDecisionHeuristic.second < minHeuristicValue)
				minHeuristicValue = playerDecisionHeuristic.second;
		}

		// Compute linear proportional probabilities and the sum
		float epsilon = 1e-6f;
		float sumTotal = 0.f;
		float power = 5.f; // Adjust this power to make the distribution more or less peaked
		for (auto& playerDecisionHeuristic : playerDecisionHeuristics)
		{
			unsigned long long clusterCode = playerDecisionHeuristic.first;

			// Shift to positive
			float shifted = playerDecisionHeuristic.second - minHeuristicValue + epsilon;

			// Apply power to make best moves stand out much more
			playerDecisionProbabilities[clusterCode] = std::pow(shifted, power);
			sumTotal += playerDecisionProbabilities[clusterCode];
		}

		// Avoid division by zero
		if (sumTotal < epsilon)
			sumTotal = (float)playerDecisionProbabilities.size();

		// Normalize to probabilities
		for (auto& playerDecisionProb : playerDecisionProbabilities)
		{
			unsigned long long clusterCode = playerDecisionProb.first;
			playerDecisionProbabilities[clusterCode] /= sumTotal;
		}
	}

	//calculate each average and take the best outcome for the other player
	for (auto& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto& simulation : playerDecisionSimulation->simulations)
		{
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			otherPlayerDecisionHeuristics[otherClusterCode] += simulation->otherPlayerSimulation.heuristic * playerDecisionProbabilities[clusterCode];
		}
	}

	float otherPlayerClusterHeuristic = FLT_MAX;
	for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
	{
		float otherPlayerDecisionHeuristicAvg = otherPlayerDecisionHeuristic.second;
		//lets take the value which minimize players heuristic
		if (otherPlayerDecisionHeuristicAvg < otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = otherPlayerDecisionHeuristic.first;
			otherPlayerClusterHeuristic = otherPlayerDecisionHeuristicAvg;
		}
	}
	if (otherPlayerClusterCode != ULLONG_MAX && otherPlayerDecisionHeuristics.find(ULLONG_MAX) != otherPlayerDecisionHeuristics.end())
	{
		//we keep the current plan if the heuristic is close to the best other player heuristic
		if (abs(otherPlayerDecisionHeuristics.at(ULLONG_MAX) - otherPlayerClusterHeuristic) < 0.05f)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerDecisionHeuristics.at(ULLONG_MAX);
		}
		//lets take the value which maximize players heuristic
		else if (otherPlayerDecisionHeuristics.at(ULLONG_MAX) > otherPlayerClusterHeuristic)
		{
			otherPlayerClusterCode = ULLONG_MAX;
			otherPlayerClusterHeuristic = otherPlayerDecisionHeuristics.at(ULLONG_MAX);
		}
	}

	std::unordered_map<unsigned long long, std::map<WeaponType, float>> otherPlayerDecisionWeapons;
	for (auto& otherPlayerDecisionHeuristic : otherPlayerDecisionHeuristics)
	{
		unsigned long long otherClusterCode = otherPlayerDecisionHeuristic.first;
		for (unsigned int weapon = 0; weapon <= MAX_WEAPONS; weapon++)
			otherPlayerDecisionWeapons[otherClusterCode][(WeaponType)weapon] = 0.f;
	}
	for (auto const& playerDecisionSimulation : gameEvaluation.playerDecisions)
	{
		for (auto const& simulation : playerDecisionSimulation->simulations)
		{
			unsigned long long clusterCode = simulation->playerSimulation.code;
			unsigned long long otherClusterCode = simulation->otherPlayerSimulation.code;
			unsigned short otherPlayerWeaponType = simulation->otherPlayerSimulation.weapon;
			otherPlayerDecisionWeapons[otherClusterCode][(WeaponType)otherPlayerWeaponType] += playerDecisionProbabilities[clusterCode];
		}
	}
	float bestOtherWeaponProbability = -FLT_MAX;
	for (auto& otherPlayerDecisionWeapon : otherPlayerDecisionWeapons[otherPlayerClusterCode])
	{
		if (otherPlayerDecisionWeapon.second > bestOtherWeaponProbability)
		{
			otherPlayerWeapon = otherPlayerDecisionWeapon.first;
			bestOtherWeaponProbability = otherPlayerDecisionWeapon.second;
		}
	}
}

void AIGameSimulation::CalculateWeightItems(const PlayerData& playerData, std::map<ActorId, float>& searchItems)
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
					searchItem.second *= 0.8f;
					if (searchItem.second < 0.15f)
						searchItem.second = 0.15f;
					break;
				case WP_SHOTGUN:
					maxAmmo = 20;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.8f;
					if (searchItem.second < 0.15f)
						searchItem.second = 0.15f;
					break;
				case WP_MACHINEGUN:
					maxAmmo = 200;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.6f;
					if (searchItem.second < 0.15f)
						searchItem.second = 0.15f;
					break;
				case WP_PLASMAGUN:
					maxAmmo = 120;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.6f;
					if (searchItem.second < 0.15f)
						searchItem.second = 0.15f;
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
					searchItem.second *= 0.8f;
					if (searchItem.second < 0.15f)
						searchItem.second = 0.15f;
					break;
				case WP_RAILGUN:
					maxAmmo = 20;

					//lets calculate the item weight based on the player status
					ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

					//ammo
					searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
					searchItem.second *= 0.8f;
					if (searchItem.second < 0.15f)
						searchItem.second = 0.15f;
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
						if (searchItem.second < 0.15f)
							searchItem.second = 0.15f;
					}
					else searchItem.second = 0.15f;
					break;
				case WP_SHOTGUN:
					maxAmmo = 20;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.8f;
						if (searchItem.second < 0.15f)
							searchItem.second = 0.15f;
					}
					else searchItem.second = 0.15f;
					break;
				case WP_MACHINEGUN:
					maxAmmo = 200;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.6f;
						if (searchItem.second < 0.15f)
							searchItem.second = 0.15f;
					}
					else searchItem.second = 0.15f;
					break;
				case WP_PLASMAGUN:
					maxAmmo = 120;
					if (playerData.stats[STAT_WEAPONS] & (1 << itemPickup->GetCode()))
					{
						//lets calculate the item weight based on the player status
						ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

						//ammo
						searchItem.second = (maxAmmo - ammo) / (float)maxAmmo;
						searchItem.second *= 0.6f;
						if (searchItem.second < 0.15f)
							searchItem.second = 0.15f;
					}
					else searchItem.second = 0.15f;
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
						searchItem.second *= 0.8f;
						if (searchItem.second < 0.15f)
							searchItem.second = 0.15f;
					}
					else searchItem.second = 0.15f;
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
						if (searchItem.second < 0.15f)
							searchItem.second = 0.15f;
					}
					else searchItem.second = 0.15f;
					break;
			}
		}
		else if (itemPickup->GetType() == "Health")
		{
			if (itemPickup->GetCode() != 3)
			{
				int maxHealth = itemPickup->GetMaximum();
				int health = playerData.stats[STAT_HEALTH] < maxHealth ? maxHealth - playerData.stats[STAT_HEALTH] : 0;

				//lets calculate the item weight based on the player status
				searchItem.second = health / (float)maxHealth;
				searchItem.second *= 0.8f;

				if (itemPickup->GetCode() == 4 || searchItem.second < 0.15f)
					searchItem.second = 0.15f;
			}
			else
			{
				int maxHealth = 100;
				int health = playerData.stats[STAT_HEALTH] < maxHealth ? maxHealth : itemPickup->GetMaximum() - playerData.stats[STAT_HEALTH];

				//lets calculate the item weight based on the player status
				searchItem.second = health / (float)maxHealth;
				searchItem.second *= 0.8f;

				if (searchItem.second < 0.15f)
					searchItem.second = 0.15f;
			}
		}
		else if (itemPickup->GetType() == "Armor")
		{
			int maxArmor = 100;
			int armor = playerData.stats[STAT_ARMOR] < maxArmor ? maxArmor : itemPickup->GetMaximum() - playerData.stats[STAT_ARMOR];

			//lets calculate the item weight based on the player status
			searchItem.second = armor / (float)maxArmor;
			searchItem.second *= 0.8f;

			if (itemPickup->GetCode() == 3 || searchItem.second < 0.15f)
				searchItem.second = 0.15f;
		}
	}
}

float AIGameSimulation::CalculateHeuristicItem(const PlayerData& playerData, ActorId item, float itemWeight)
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
				score *= 0.8f; //how important is this weapon

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_SHOTGUN:
				maxAmmo = 20;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.6f; //how important is this weapon

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_MACHINEGUN:
				maxAmmo = 200;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.2f; //how important is this weapon

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_PLASMAGUN:
				maxAmmo = 120;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.3f; //how important is this weapon

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_GRENADE_LAUNCHER:
				maxAmmo = 20;
					
				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.f; //how important is this weapon

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_ROCKET_LAUNCHER:
				maxAmmo = 20;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.5f; //how important is this weapon

				//relation based on item score and distance traversed
				heuristic = score * (1.0f - (weight / (float)maxWeight));
				break;
			case WP_RAILGUN:
				maxAmmo = 20;

				weight = (itemWeight < maxWeight) ? itemWeight : maxWeight;
				ammo = (playerData.ammo[itemPickup->GetCode()] < maxAmmo) ? playerData.ammo[itemPickup->GetCode()] : maxAmmo;

				//ammo
				score = (maxAmmo - ammo) / (float)maxAmmo;
				score *= 0.8f; //how important is this weapon

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
					score *= 0.8f; //how important is this ammo

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
					score *= 0.6f; //how important is this ammo

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
					score *= 0.3f; //how important is this ammo

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
					score *= 0.5f; //how important is this ammo

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
					score *= 0.8f; //how important is this ammo

					//relation based on item score and distance traversed
					heuristic = score * (1.0f - (weight / (float)maxWeight));
				}
				break;
		}
	}
	else if (itemPickup->GetType() == "Armor")
	{
		if ((int)itemPickup->GetMaximum() > playerData.stats[STAT_ARMOR])
		{
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
		if ((int)itemPickup->GetMaximum() > playerData.stats[STAT_HEALTH])
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

float AIGameSimulation::CalculateBestHeuristicItem(const PlayerData& playerData)
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
	float margin = 0.001f;
	for (auto const& item : playerData.items)
	{
		if (playerData.itemWeight.at(item.first) <= playerData.planWeight + margin)
			continue;

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
					score *= 0.8f; //how important is this weapon

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
					score *= 0.6f; //how important is this weapon

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
					score *= 0.2f; //how important is this weapon

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
					score *= 0.3f; //how important is this weapon

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
					score *= 0.f; //how important is this weapon

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
					score *= 0.5f; //how important is this weapon

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
					score *= 0.8f; //how important is this weapon

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
						score *= 0.8f; //how important is this ammo

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
						score *= 0.6f; //how important is this ammo

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
						score *= 0.3f; //how important is this ammo

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
						score *= 0.5f; //how important is this ammo

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
						score *= 0.8f; //how important is this ammo

						weight = (playerData.itemWeight.at(item.first) < maxWeight) ? playerData.itemWeight.at(item.first) : maxWeight;

						//relation based on item score and distance traversed
						heuristicItems[item.first] = score * (1.0f - (weight / (float)maxWeight));
						if (heuristicItems[bestItem] < heuristicItems[item.first])
							bestItem = item.first;
					}
					break;
			}
		}
		else if (itemPickup->GetType() == "Health")
		{
			if (playerData.itemAmount.at(item.first))
			{
				if (itemPickup->GetCode() != 3)
				{
					int maxHealth = itemPickup->GetMaximum();
					int health = playerData.stats[STAT_HEALTH] < maxHealth ? maxHealth - playerData.stats[STAT_HEALTH] : 0;

					score = health / (float)maxHealth;
				}
				else
				{
					int maxHealth = 100;
					int health = playerData.stats[STAT_HEALTH] < maxHealth ? maxHealth : itemPickup->GetMaximum() - playerData.stats[STAT_HEALTH];

					score = health / (float)maxHealth;
				}
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
		else if (itemPickup->GetType() == "Armor")
		{
			if (playerData.itemAmount.at(item.first))
			{
				int maxArmor = 100;
				int armor = playerData.stats[STAT_ARMOR] < maxArmor ? maxArmor : itemPickup->GetMaximum() - playerData.stats[STAT_ARMOR];

				score = armor / (float)maxArmor;
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
	}

	//heuristic value comes from the best item
	float heuristic = heuristicItems[bestItem];
	heuristicItems.erase(bestItem);
	//the other items add a small heuristic percentage
	for (auto& heuristicItem : heuristicItems)
		heuristic += heuristicItem.second * 0.1f;

	return heuristic;
}

void AIGameSimulation::CalculateHeuristic(EvaluationType evaluation, unsigned short threatLevel, PlayerData& playerData, PlayerData& otherPlayerData)
{
	float heuristic = 0.f;
	int maxDamage = threatLevel > 1 ? 500 : 400;

	//heuristic from picked up items
	heuristic += CalculateBestHeuristicItem(playerData);
	heuristic -= CalculateBestHeuristicItem(otherPlayerData);

	//heuristic from damage dealing
	int playerMaxDamage = 0, otherPlayerMaxDamage = 0;
	if (playerData.weapon != WP_NONE)
	{
		if (playerData.damage[playerData.weapon - 1] >= maxDamage)
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
		if (otherPlayerData.damage[otherPlayerData.weapon - 1] >= maxDamage)
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

	//calculate damage based on players health/armor and weapon status
	float playerDamage, otherPlayerDamage;
	if (CalculatePlayerStatus(playerData) > 0.3f)
	{
		if (CalculatePlayerStatus(otherPlayerData) > 0.3f)
		{
			//remains the same
			playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
			otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
		}
		else if (CalculatePlayerStatus(otherPlayerData) > 0.15f)
		{
			if (CalculatePlayerWeaponStatus(playerData) > 0.f)
			{
				playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
				otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
			}
			else if (CalculatePlayerWeaponStatus(otherPlayerData) > 0.3f)
			{
				//remains the same
				playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
				otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
			}
			else
			{
				playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
				otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
			}
		}
		else
		{
			playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
			otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
		}
	}
	else if (CalculatePlayerStatus(playerData) > 0.15f)
	{
		if (CalculatePlayerStatus(otherPlayerData) > 0.3f)
		{
			if (CalculatePlayerWeaponStatus(otherPlayerData) > 0.f)
			{
				playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
				otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
			}
			else if (CalculatePlayerWeaponStatus(playerData) > 0.3f)
			{
				//remains the same
				playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
				otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
			}
			else
			{
				playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
				otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
			}
		}
		else if (CalculatePlayerStatus(otherPlayerData) > 0.15f)
		{
			if (CalculatePlayerWeaponStatus(playerData) > 0.f)
			{
				if (CalculatePlayerWeaponStatus(otherPlayerData) > 0.f)
				{
					playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
					otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
				}
				else
				{
					playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
					otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
				}
			}
			else
			{
				if (CalculatePlayerWeaponStatus(otherPlayerData) > 0.f)
				{
					playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
					otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
				}
				else
				{
					playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
					otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
				}
			}
		}
		else
		{
			if (CalculatePlayerWeaponStatus(playerData) > 0.f)
				playerDamage = playerMaxDamage > maxDamage ? (float)maxDamage : (float)playerMaxDamage;
			else
				playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
			otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
		}
	}
	else
	{
		if (CalculatePlayerStatus(otherPlayerData) > 0.3f)
		{
			playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
			otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
		}
		else if (CalculatePlayerStatus(otherPlayerData) > 0.15f)
		{
			playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
			if (CalculatePlayerWeaponStatus(otherPlayerData) > 0.f)
				otherPlayerDamage = otherPlayerMaxDamage > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage;
			else
				otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
		}
		else
		{
			playerDamage = playerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)playerMaxDamage * 0.25f;
			otherPlayerDamage = otherPlayerMaxDamage * 0.25f > maxDamage ? (float)maxDamage : (float)otherPlayerMaxDamage * 0.25f;
		}
	}

	//calculate total damage as difference between inflicted and taken damage
	float heuristicDamage = (playerDamage - otherPlayerDamage) / (float)maxDamage;

	//reward taking less damage
	if (playerDamage > otherPlayerDamage)
		heuristicDamage += (0.8f * heuristicDamage) * (playerDamage - otherPlayerDamage) / playerDamage;
	else if (otherPlayerDamage > 0.f)
		heuristicDamage -= (0.8f * heuristicDamage) * (playerDamage - otherPlayerDamage) / otherPlayerDamage;

	heuristic += heuristicDamage;
	playerData.heuristic = heuristic;
	otherPlayerData.heuristic = heuristic;
}

//score is calculated based on health and armor
float AIGameSimulation::CalculatePlayerStatus(const PlayerData& playerData)
{
	//health & armor status
	unsigned int maxHealth = 200;
	unsigned int maxArmor = 200;
	float playerScore = (playerData.stats[STAT_HEALTH] / (float)maxHealth) * 0.6f;
	playerScore += (playerData.stats[STAT_ARMOR] / (float)maxArmor) * 0.4f;
	return playerScore;
}

//weapon score
float AIGameSimulation::CalculatePlayerWeaponStatus(const PlayerData& playerData)
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

void AIGameSimulation::CalculateDamage(PlayerData& playerData, const std::map<float, VisibilityData>& visibility)
{
	std::map<float, int> itemAmmo;

	for (int weapon = 1; weapon <= MAX_WEAPONS; weapon++)
	{
		if (weapon != WP_GAUNTLET)
		{
			bool weaponAvailable = false;
			int ammo = 0, damage = 0, currentDamage = 0, shotCount = 0, maxAmmo = 200;
			float fireTime = 0.f, visibleTime = 0.f, weaponTime = FLT_MAX, rangeDistance = 0.f;

			if (playerData.ammo[weapon] && (playerData.stats[STAT_WEAPONS] & (1 << weapon)))
			{
				weaponTime = 0.f;
				weaponAvailable = true;
			}

			itemAmmo.clear();
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

			std::map<float, VisibilityData>::const_iterator itVisibility, itOptimalVisibility;
			switch (weapon)
			{
				case WP_LIGHTNING:
					damage = 6;
					fireTime = 0.05f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (itVisibility = visibility.begin(); itVisibility != visibility.end(); itVisibility++)
					{
						if (itemAmmo.find((*itVisibility).first) != itemAmmo.end())
						{
							ammo += itemAmmo[(*itVisibility).first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if ((*itVisibility).first < weaponTime)
							continue;

						// lets assign a firing threshold
						if ((*itVisibility).second.moveTime > 0.1f)
						{
							if ((*itVisibility).second.moveDistance <= 800)
							{
								shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
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
					playerData.damage[weapon - 1] = 0;

					//optimal damage calculation
					for (itOptimalVisibility = visibility.begin(); itOptimalVisibility != visibility.end(); itOptimalVisibility++)
					{
						visibleTime = 0.f;
						currentDamage = 0;
						ammo = playerData.ammo[weapon];
						for (itVisibility = itOptimalVisibility; itVisibility != visibility.end(); itVisibility++)
						{
							if (itemAmmo.find((*itVisibility).first) != itemAmmo.end())
							{
								ammo += itemAmmo[(*itVisibility).first];
								if (ammo > maxAmmo) ammo = maxAmmo;
							}

							if ((*itVisibility).first < weaponTime || (*itVisibility).first + (*itVisibility).second.moveTime < visibleTime)
								continue;

							// lets assign a firing threshold
							if ((*itVisibility).second.moveTime >= 0.1f)
							{
								visibleTime = (*itVisibility).first + (*itVisibility).second.moveTime + fireTime;

								shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
								shotCount = shotCount > ammo ? ammo : shotCount;
								ammo -= shotCount;
								rangeDistance = (*itVisibility).second.moveDistance > 500 ? (*itVisibility).second.moveDistance : 500;

								if ((*itVisibility).second.moveDistance < 120)
									currentDamage += (damage + 50) * shotCount;
								else
									currentDamage += (int)round(damage * (1.f - ((*itVisibility).second.moveDistance / rangeDistance)) * shotCount);
							}
						}

						if (playerData.damage[weapon - 1] < currentDamage)
							playerData.damage[weapon - 1] = currentDamage;
					}
					break;

				case WP_MACHINEGUN:
					damage = 5;
					fireTime = 0.1f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (itVisibility = visibility.begin(); itVisibility != visibility.end(); itVisibility++)
					{
						if (itemAmmo.find((*itVisibility).first) != itemAmmo.end())
						{
							ammo += itemAmmo[(*itVisibility).first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if ((*itVisibility).first < weaponTime)
							continue;

						// lets assign a firing threshold
						if ((*itVisibility).second.moveTime >= 0.1f)
						{
							shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							rangeDistance = (*itVisibility).second.moveDistance > 400 ? (*itVisibility).second.moveDistance : 400;
							playerData.damage[weapon - 1] += (int)round(damage *
								(1.f - ((*itVisibility).second.moveDistance / rangeDistance)) * shotCount);
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
					playerData.damage[weapon - 1] = 0;

					//optimal damage calculation
					for (itOptimalVisibility = visibility.begin(); itOptimalVisibility != visibility.end(); itOptimalVisibility++)
					{
						visibleTime = 0.f;
						currentDamage = 0;
						ammo = playerData.ammo[weapon];

						for (itVisibility = itOptimalVisibility; itVisibility != visibility.end(); itVisibility++)
						{
							if (itemAmmo.find((*itVisibility).first) != itemAmmo.end())
							{
								ammo += itemAmmo[(*itVisibility).first];
								if (ammo > maxAmmo) ammo = maxAmmo;
							}

							if ((*itVisibility).first < weaponTime || (*itVisibility).first + (*itVisibility).second.moveTime < visibleTime)
								continue;

							// lets assign a firing threshold
							if ((*itVisibility).second.moveTime >= 0.1f)
							{
								visibleTime = (*itVisibility).first + (*itVisibility).second.moveTime + fireTime;

								shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
								shotCount = shotCount > ammo ? ammo : shotCount;
								ammo -= shotCount;
								if ((*itVisibility).second.moveHeight <= 30.f)
									rangeDistance = (*itVisibility).second.moveDistance > 500 ? (*itVisibility).second.moveDistance : 500;
								else
									rangeDistance = (*itVisibility).second.moveDistance > 700 ? (*itVisibility).second.moveDistance : 700;
								currentDamage += (int)round(damage * (1.f - ((*itVisibility).second.moveDistance / rangeDistance)) * shotCount);
							}
						}

						if (playerData.damage[weapon - 1] < currentDamage)
							playerData.damage[weapon - 1] = currentDamage;
					}
					break;

				case WP_PLASMAGUN:
					damage = 10;
					fireTime = 0.1f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (itVisibility = visibility.begin(); itVisibility != visibility.end(); itVisibility++)
					{
						if (itemAmmo.find((*itVisibility).first) != itemAmmo.end())
						{
							ammo += itemAmmo[(*itVisibility).first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if ((*itVisibility).first < weaponTime)
							continue;

						// lets assign a firing threshold
						if ((*itVisibility).second.moveTime >= 0.1f)
						{
							shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							rangeDistance = (*itVisibility).second.moveDistance > 300 ? (*itVisibility).second.moveDistance : 300;
							playerData.damage[weapon - 1] += (int)round(damage *
								(1.f - ((*itVisibility).second.moveDistance / rangeDistance)) * shotCount);
						}
					}
					break;

				case WP_RAILGUN:
					damage = 100;
					fireTime = 1.5f;
					visibleTime = 0.f;
					playerData.damage[weapon - 1] = 0;

					ammo = playerData.ammo[weapon];
					for (itVisibility = visibility.begin(); itVisibility != visibility.end(); itVisibility++)
					{
						if (itemAmmo.find((*itVisibility).first) != itemAmmo.end())
						{
							ammo += itemAmmo[(*itVisibility).first];
							if (ammo > maxAmmo) ammo = maxAmmo;
						}

						if ((*itVisibility).first < weaponTime || (*itVisibility).first + (*itVisibility).second.moveTime < visibleTime)
							continue;

						// lets assign a firing threshold
						if ((*itVisibility).second.moveTime >= 0.1f)
						{
							visibleTime = (*itVisibility).first + (*itVisibility).second.moveTime + fireTime;

							shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
							shotCount = shotCount > ammo ? ammo : shotCount;
							ammo -= shotCount;
							if ((*itVisibility).second.moveDistance > 500)
								playerData.damage[weapon - 1] += (damage + 50) * shotCount;
							else if ((*itVisibility).second.moveDistance < 300)
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

			std::map<float, VisibilityData>::const_iterator itVisibility;
			for (itVisibility = visibility.begin(); itVisibility != visibility.end(); itVisibility++)
			{
				if ((*itVisibility).first < weaponTime || (*itVisibility).first + (*itVisibility).second.moveTime < visibleTime)
					continue;

				// lets assign a firing threshold
				if ((*itVisibility).second.moveTime >= 0.1f)
				{
					if ((*itVisibility).second.moveDistance <= 30.f)
					{
						visibleTime = (*itVisibility).first + (*itVisibility).second.moveTime + fireTime;

						shotCount = (int)ceil((*itVisibility).second.moveTime / fireTime);
						playerData.damage[weapon - 1] += damage * shotCount;
					}
				}
			}
		}
	}
}

void AIGameSimulation::CalculateVisibility(unsigned short threatLevel,
	PathingNode* playerNode, float playerPathOffset, float playerVisibleTime,
	const PathingArcVec& playerPathPlan, std::map<float, VisibilityData>& playerVisibility, 
	PathingNode* otherPlayerNode, float otherPlayerPathOffset, float otherPlayerVisibleTime,
	const PathingArcVec& otherPlayerPathPlan, std::map<float, VisibilityData>& otherPlayerVisibility)
{
	if (threatLevel == 0)
		return;

	float totalWeight = 0.f, totalArcWeight = 0.f;
	unsigned int index = 0, otherIndex = 0, otherPathIndex = 0;

	PathingNode* currentNode = playerNode;
	PathingArc* currentArc = NULL;

	PathingNode* otherCurrentNode = otherPlayerNode;
	PathingArc* otherCurrentArc = NULL;

	for (; otherPathIndex < otherPlayerPathPlan.size(); otherPathIndex++)
	{
		otherCurrentArc = otherPlayerPathPlan[otherPathIndex];
		const PathingNodeVec& otherTransitionNodes = otherCurrentArc->GetTransition()->GetNodes();
		const std::vector<Vector3<float>>& otherTransitionPositions = otherCurrentArc->GetTransition()->GetPositions();

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
		const PathingNodeVec& transitionNodes = currentArc->GetTransition()->GetNodes();
		const std::vector<Vector3<float>>& transitionPositions = currentArc->GetTransition()->GetPositions();

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
	float totalVisibleWeight = 0.f;
	float weightLimit = threatLevel > 1 ? 4.f : 3.f;
	//lets calculate the visibility for simultaneous path travelling.
	if (otherCurrentArc != NULL)
	{
		bool otherPathPlanEnd = false;
		for (; itPathPlan != playerPathPlan.end(); itPathPlan++)
		{
			currentArc = *itPathPlan;
			std::reference_wrapper<const PathingNodeVec> transitionNodes = currentArc->GetTransition()->GetNodes();
			std::reference_wrapper<const PathingNodeVec> otherTransitionNodes = otherCurrentArc->GetTransition()->GetNodes();
			std::reference_wrapper<const std::vector<Vector3<float>>> transitionPositions = currentArc->GetTransition()->GetPositions();
			std::reference_wrapper<const std::vector<Vector3<float>>> otherTransitionPositions = otherCurrentArc->GetTransition()->GetPositions();

			for (; index < transitionPositions.get().size(); index++)
			{
				float currentWeight = currentArc->GetTransition()->GetWeights()[index];
				if (transitionNodes.get()[index]->IsVisibleNode(otherTransitionNodes.get()[otherIndex]))
				//if (RayCollisionDetection(transitionPositions[index], otherTransitionPositions[otherIndex]) == NULL)
				{
					for (auto visIt = std::next(visibilityIt); visIt != playerVisibility.end(); visIt++)
					{
						if (visIt->first > totalWeight)
							break;

						visibilityIt++;
					}

					if (currentArc->GetType() == AT_MOVE)
					{
						//lets check that we are in the visibile time frame
						if (visibilityIt != playerVisibility.end() && visibilityIt->first >= playerVisibleTime)
						{
							visibilityIt->second.moveDistance += Length(
								otherTransitionPositions.get()[otherIndex] - transitionPositions.get()[index]) * currentWeight;
							visibilityIt->second.moveHeight +=
								(transitionPositions.get()[index][AXIS_Y] - otherTransitionPositions.get()[otherIndex][AXIS_Y]) * currentWeight;
							visibilityIt->second.moveTime += currentWeight;
						}
					}

					for (auto otherVisIt = std::next(otherVisibilityIt); otherVisIt != otherPlayerVisibility.end(); otherVisIt++)
					{
						if (otherVisIt->first > totalWeight)
							break;

						otherVisibilityIt++;
					}

					if (otherPathPlanEnd || otherCurrentArc->GetType() == AT_MOVE)
					{
						//lets check that we are in the visibile time frame
						if (otherVisibilityIt != otherPlayerVisibility.end() && otherVisibilityIt->first >= otherPlayerVisibleTime)
						{
							otherVisibilityIt->second.moveDistance += Length(
								otherTransitionPositions.get()[otherIndex] - transitionPositions.get()[index]) * currentWeight;
							otherVisibilityIt->second.moveHeight +=
								(otherTransitionPositions.get()[otherIndex][AXIS_Y] - transitionPositions.get()[index][AXIS_Y]) * currentWeight;
							otherVisibilityIt->second.moveTime += currentWeight;
						}
					}

					if (visibilityIt->first >= playerVisibleTime && otherVisibilityIt->first >= otherPlayerVisibleTime)
						totalVisibleWeight += currentWeight;
				}

				while (totalArcWeight <= totalWeight)
				{
					totalArcWeight += otherCurrentArc->GetTransition()->GetWeights()[otherIndex];
					if (otherIndex + 1 >= otherTransitionPositions.get().size())
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
				// set weightlimit; any time further is likely to be unrealistic simulation
				if (totalWeight > weightLimit)
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
			const PathingNodeVec& transitionNodes = currentArc->GetTransition()->GetNodes();
			const std::vector<Vector3<float>>& transitionPositions = currentArc->GetTransition()->GetPositions();
			for (; index < transitionPositions.size(); index++)
			{
				float currentWeight = currentArc->GetTransition()->GetWeights()[index];
				if (transitionNodes[index]->IsVisibleNode(otherCurrentNode))
					//if (RayCollisionDetection(transitionPositions[index], otherTransitionPositions[otherIndex]) == NULL)
				{
					for (auto visIt = std::next(visibilityIt); visIt != playerVisibility.end(); visIt++)
					{
						if (visIt->first > totalWeight)
							break;

						visibilityIt++;
					}

					if (currentArc->GetType() == AT_MOVE)
					{
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

					if (visibilityIt->first >= playerVisibleTime && otherVisibilityIt->first >= otherPlayerVisibleTime)
						totalVisibleWeight += currentWeight;
				}

				totalWeight += currentWeight;
				// set weightlimit; any time further is likely to be unrealistic simulation
				if (totalWeight > weightLimit)
					return;
			}

			currentNode = currentArc->GetNode();
			index = 0;
		}
	}

	float visibleWeightLimit = threatLevel > 1 ? 2.f : 1.5f;
	if (totalVisibleWeight < visibleWeightLimit)
	{
		//we need to add visible time if the total visible move time is short
		if (currentNode->IsVisibleNode(otherCurrentNode))
			/*if (RayCollisionDetection(currentNode->GetPosition(), otherCurrentNode->GetPosition()) == NULL)*/
		{
			float currentWeight = 0.5f;
			visibleWeightLimit = threatLevel > 1 ? 1.f : 0.75f;
			totalVisibleWeight = totalVisibleWeight < visibleWeightLimit ? 2.f : 1.f;
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

/*
CanItemBeGrabbed
Returns false if the item can not be picked up.
Note: the logic assumes that the item respawn time is known
*/
bool AIGameSimulation::CanItemBeGrabbed(ActorId itemId,
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
void AIGameSimulation::PickupItems(PlayerData& playerData,
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

void AIGameSimulation::Simulation(
	EvaluationType evaluation, const std::map<ActorId, float>& gameItems, unsigned short threatLevel,
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

		CalculateVisibility(threatLevel,
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

		CalculateVisibility(threatLevel, playerData.plan.node, playerPathOffset, playerVisibleTime, playerPathPlan, playerVisibility,
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
	CalculateHeuristic(evaluation, threatLevel, playerData, otherPlayerData);
}

bool AIGameSimulation::BuildPath(
	unsigned short& threatLevel, std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans)
{
	std::unordered_map<unsigned int, PathingNode*> clusterNodes, otherClusterNodes;

	std::map<PathingCluster*, PathingArcVec> clusterPaths, jumpClusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights, jumpClusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 100, clusterPaths, clusterPathWeights);
	for (auto& clusterPath : clusterPaths)
		clusterNodes[clusterPath.first->GetTarget()->GetCluster()] = clusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
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

	std::map<PathingCluster*, PathingArcVec> otherClusterPaths, otherJumpClusterPaths;
	std::map<PathingCluster*, float> otherClusterPathWeights, otherJumpClusterPathWeights;
	otherClusterNodeStart->GetClusters(AT_MOVE, 100, otherClusterPaths, otherClusterPathWeights);
	for (auto& otherClusterPath : otherClusterPaths)
		otherClusterNodes[otherClusterPath.first->GetTarget()->GetCluster()] = otherClusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	otherClusterNodeStart->GetClusters(AT_JUMP, 100, otherJumpClusterPaths, otherJumpClusterPathWeights);
	for (auto& otherJumpClusterPath : otherJumpClusterPaths)
	{
		if (otherClusterNodes.find(otherJumpClusterPath.first->GetTarget()->GetCluster()) == otherClusterNodes.end())
		{
			otherClusterPaths[otherJumpClusterPath.first] = otherJumpClusterPath.second;
			otherClusterPathWeights[otherJumpClusterPath.first] = otherJumpClusterPathWeights[otherJumpClusterPath.first];
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

	//add paths if there is at least one visible node 
	if (visibleClusters.size())
	{
		threatLevel = 4; // maximum threat level if there is at least one close visible node

		std::multimap<float, PathingCluster*, std::less<float>> closestClusterPathWeights, otherClosestClusterPathWeights;
		for (auto& clusterPathWeight : clusterPathWeights)
			closestClusterPathWeights.insert({ clusterPathWeight.second, clusterPathWeight.first });
		for (auto& otherClusterPathWeight : otherClusterPathWeights)
			otherClosestClusterPathWeights.insert({ otherClusterPathWeight.second, otherClusterPathWeight.first });
		for (size_t maxClusters = 30; maxClusters <= 90; maxClusters += 30)
		{
			auto itClusterWeightEnd = std::next(closestClusterPathWeights.begin(), std::min(maxClusters, closestClusterPathWeights.size()));
			auto itOtherClusterWeightEnd = std::next(otherClosestClusterPathWeights.begin(), std::min(maxClusters, otherClosestClusterPathWeights.size()));

			unsigned short visibleClustersCount = 0;
			for (auto itClusterWeight = closestClusterPathWeights.begin(); itClusterWeight != itClusterWeightEnd; ++itClusterWeight)
			{
				unsigned int clusterCode = itClusterWeight->second->GetType() << 28 | itClusterWeight->second->GetTarget()->GetId();
				for (auto itOtherClusterWeight = otherClosestClusterPathWeights.begin(); itOtherClusterWeight != itOtherClusterWeightEnd; ++itOtherClusterWeight)
				{
					unsigned int otherClusterCode = itOtherClusterWeight->second->GetType() << 28 | itOtherClusterWeight->second->GetTarget()->GetId();
					if (visibleClusters.find(((unsigned long long)clusterCode << 32 | (unsigned long long)otherClusterCode)) != visibleClusters.end())
					{
						visibleClustersCount++;
						break;
					}
				}

				if (visibleClustersCount)
					break;
			}

			if (visibleClustersCount)
				break;

			threatLevel--;
		}

		for (auto& clusterPath : clusterPaths)
		{
			PathingCluster* pathingCluster = clusterPath.first;
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
			}
		}

		for (auto& otherClusterPath : otherClusterPaths)
		{
			PathingCluster* otherPathingCluster = otherClusterPath.first;
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
			}
		}

		//we will only expand those clusters which are visibles from both players
		closestClusterPathWeights.clear();
		otherClosestClusterPathWeights.clear();
		for (auto& visibleCluster : visibleClusters)
		{
			closestClusterPathWeights.insert({ clusterPathWeights[visibleCluster.second.first], visibleCluster.second.first });
			otherClosestClusterPathWeights.insert({ otherClusterPathWeights[visibleCluster.second.second], visibleCluster.second.second });
		}

		unsigned int maxClosestClusters = 30;
		unsigned int maxPathingClusters = 200;
		std::map<PathingCluster*, float> closestClusterPaths;
		for (auto itCluster = closestClusterPathWeights.begin(); itCluster != closestClusterPathWeights.end(); ++itCluster)
		{
			closestClusterPaths[(*itCluster).second] = (*itCluster).first;
			if (closestClusterPaths.size() >= maxClosestClusters) 
				break;
		}
		BuildExpandedPath(graph, maxPathingClusters, clusterNodeStart, 
			clusterPaths, closestClusterPaths, clusterPathings, clusterNodePathPlans);

		std::map<PathingCluster*, float> otherClosestClusterPaths;
		for (auto itOtherCluster = otherClosestClusterPathWeights.begin(); itOtherCluster != otherClosestClusterPathWeights.end(); ++itOtherCluster)
		{
			otherClosestClusterPaths[(*itOtherCluster).second] = (*itOtherCluster).first;
			if (otherClosestClusterPaths.size() >= maxClosestClusters)
				break;
		}
		BuildExpandedPath(graph, maxPathingClusters, otherClusterNodeStart,
			otherClusterPaths, otherClosestClusterPaths, otherClusterPathings, otherClusterNodePathPlans);
	}
	 
	return visibleClusters.size();
}

bool AIGameSimulation::BuildLongPath(std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans)
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

bool AIGameSimulation::BuildLongPath(
	unsigned short& threatLevel, std::shared_ptr<PathingGraph>& graph, PathingNode* clusterNodeStart, PathingNode* otherClusterNodeStart,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& otherClusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& otherClusterNodePathPlans)
{
	std::unordered_map<unsigned int, PathingNode*> clusterNodes, otherClusterNodes;

	std::map<PathingCluster*, PathingArcVec> clusterPaths, jumpClusterPaths;
	std::map<PathingCluster*, float> clusterPathWeights, jumpClusterPathWeights;
	clusterNodeStart->GetClusters(AT_MOVE, 150, clusterPaths, clusterPathWeights);
	for (auto& clusterPath : clusterPaths)
		clusterNodes[clusterPath.first->GetTarget()->GetCluster()] = clusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	clusterNodeStart->GetClusters(AT_JUMP, 150, jumpClusterPaths, jumpClusterPathWeights);
	for (auto& jumpClusterPath : jumpClusterPaths)
	{
		if (clusterNodes.find(jumpClusterPath.first->GetTarget()->GetCluster()) == clusterNodes.end())
		{
			clusterPaths[jumpClusterPath.first] = jumpClusterPath.second;
			clusterPathWeights[jumpClusterPath.first] = jumpClusterPathWeights[jumpClusterPath.first];
		}
	}

	//skip closest clusters to reduce the number of candidates as they have been inspected previously
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

	std::map<PathingCluster*, PathingArcVec> otherClusterPaths, jumpOtherClusterPaths;
	std::map<PathingCluster*, float> otherClusterPathWeights, jumpOtherClusterPathWeights;
	otherClusterNodeStart->GetClusters(AT_MOVE, 150, otherClusterPaths, otherClusterPathWeights);
	for (auto& otherClusterPath : otherClusterPaths)
		otherClusterNodes[otherClusterPath.first->GetTarget()->GetCluster()] = otherClusterPath.first->GetTarget();
	//we will only consider jumps which are not reachable on moving
	otherClusterNodeStart->GetClusters(AT_JUMP, 150, jumpOtherClusterPaths, jumpOtherClusterPathWeights);
	for (auto& jumpOtherClusterPath : jumpOtherClusterPaths)
	{
		if (otherClusterNodes.find(jumpOtherClusterPath.first->GetTarget()->GetCluster()) == otherClusterNodes.end())
		{
			otherClusterPaths[jumpOtherClusterPath.first] = jumpOtherClusterPath.second;
			otherClusterPathWeights[jumpOtherClusterPath.first] = jumpOtherClusterPathWeights[jumpOtherClusterPath.first];
		}
	}

	//skip closest clusters to reduce the number of candidates as they have been inspected previously
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

	//add paths if there is at least one visible node 
	if (visibleClusters.size())
	{
		threatLevel = 1; // lower threat level if there is at least one visible node

		for (auto& clusterPath : clusterPaths)
		{
			PathingCluster* pathingCluster = clusterPath.first;
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
			}
		}

		for (auto& otherClusterPath : otherClusterPaths)
		{
			PathingCluster* otherPathingCluster = otherClusterPath.first;
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
			}
		}

		//we will only expand those clusters which are visibles from both players
		closestClusterPathWeights.clear();
		otherClosestClusterPathWeights.clear();
		for (auto& visibleCluster : visibleClusters)
		{
			closestClusterPathWeights.insert({ clusterPathWeights[visibleCluster.second.first], visibleCluster.second.first });
			otherClosestClusterPathWeights.insert({ otherClusterPathWeights[visibleCluster.second.second], visibleCluster.second.second });
		}

		unsigned int maxClosestClusters = 30;
		unsigned int maxPathingClusters = 200;
		std::map<PathingCluster*, float> closestClusterPaths;
		for (auto itCluster = closestClusterPathWeights.begin(); itCluster != closestClusterPathWeights.end(); ++itCluster)
		{
			closestClusterPaths[(*itCluster).second] = (*itCluster).first;
			if (closestClusterPaths.size() >= maxClosestClusters)
				break;
		}
		BuildExpandedPath(graph, maxPathingClusters, clusterNodeStart,
			clusterPaths, closestClusterPaths, clusterPathings, clusterNodePathPlans);

		std::map<PathingCluster*, float> otherClosestClusterPaths;
		for (auto itOtherCluster = otherClosestClusterPathWeights.begin(); itOtherCluster != otherClosestClusterPathWeights.end(); ++itOtherCluster)
		{
			otherClosestClusterPaths[(*itOtherCluster).second] = (*itOtherCluster).first;
			if (otherClosestClusterPaths.size() >= maxClosestClusters)
				break;
		}
		BuildExpandedPath(graph, maxPathingClusters, otherClusterNodeStart,
			otherClusterPaths, otherClosestClusterPaths, otherClusterPathings, otherClusterNodePathPlans);
		return true;
	}
	else threatLevel = 0; // no threat level if there is no visible node
	
	return false;
}

bool AIGameSimulation::BuildLongestPath(
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
	unsigned int skipPathingClusters = 120;
	std::unordered_map<PathingNode*, std::unordered_map<PathingCluster*, unsigned int>> pathingClusterNodes;
	std::multimap<float, PathingCluster*, std::less<float>> closestClusterPathWeights;
	for (auto& clusterPathWeight : clusterPathWeights)
		closestClusterPathWeights.insert({ clusterPathWeight.second, clusterPathWeight.first });

	auto itCluster = closestClusterPathWeights.begin();
	if (closestClusterPathWeights.size() > 120)
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
	if (otherClosestClusterPathWeights.size() > 120)
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

void AIGameSimulation::BuildExpandedPath(
	std::shared_ptr<PathingGraph>& graph, unsigned int maxPathingClusters, PathingNode* clusterNodeStart,
	const std::map<PathingCluster*, PathingArcVec>& clusterPaths, const std::map<PathingCluster*, float>& expandClusterPathWeights,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans)
{
	std::mutex mutex;

	//we will expand only with move type clusters
	unsigned int pathingClustersLimit = maxPathingClusters / (unsigned int)expandClusterPathWeights.size();
	Concurrency::parallel_for_each(
		begin(expandClusterPathWeights), end(expandClusterPathWeights), [&](auto const& clusterPathWeight)
	//for (auto& clusterPathWeight : expandClusterPathWeights)
	{
		PathingNode* clusterNodeEnd = graph->FindClusterNode(clusterPathWeight.first->GetTarget()->GetCluster());

		//lets try to add surrounding clusters
		std::map<PathingCluster*, PathingArcVec> pathingClusters;
		std::multimap<float, PathingCluster*, std::less<float>> pathingClusterWeights;
		clusterPathWeight.first->GetTarget()->GetClusters(AT_MOVE, pathingClustersLimit, pathingClusters, pathingClusterWeights);
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != pathingClusterWeights.end(); ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());
			if (clusterNodeEnd == pathingClusterNodeEnd)
				continue;

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

void AIGameSimulation::BuildExpandedActorPath(
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

	//we will expand clusters
	Concurrency::parallel_for_each(begin(bestClusterPaths), end(bestClusterPaths), [&](auto const& bestClusterPath)
	//for (auto& bestClusterPath : bestClusterPaths)
	{
		PathingNode* actorPathNode = actorPathPlanClusters[bestClusterPath.second].empty() ?
			clusterNodeStart : actorPathPlanClusters[bestClusterPath.second].back()->GetNode();
		PathingNode* clusterNodeEnd = graph->FindClusterNode(actorPathNode->GetCluster());

		//lets try to add surrounding clusters
		std::unordered_set<PathingNode*> pathingClusterNodes;
		std::map<PathingCluster*, PathingArcVec> pathingClusters;
		std::multimap<float, PathingCluster*, std::less<float>> pathingClusterWeights;
		actorPathNode->GetClusters(AT_MOVE, 40, pathingClusters, pathingClusterWeights);
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != pathingClusterWeights.end(); ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());
			if (clusterNodeEnd == pathingClusterNodeEnd)
				continue;

			pathingClusterNodes.insert(pathingClusterNodeEnd);

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

		pathingClusters.clear();
		pathingClusterWeights.clear();
		actorPathNode->GetClusters(AT_JUMP, 30, pathingClusters, pathingClusterWeights);
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != pathingClusterWeights.end(); ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());
			if (clusterNodeEnd == pathingClusterNodeEnd)
				continue;

			if (pathingClusterNodes.find(pathingClusterNodeEnd) != pathingClusterNodes.end())
				continue;

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

void AIGameSimulation::BuildExpandedActorPath(
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

	//we will expand clusters
	Concurrency::parallel_for_each(begin(bestClusterPaths), end(bestClusterPaths), [&](auto const& bestClusterPath)
	//for (auto& bestClusterPath : bestClusterPaths)
	{
		PathingNode* actorPathNode = actorPathPlanClusters[bestClusterPath.second].empty() ?
			clusterNodeStart : actorPathPlanClusters[bestClusterPath.second].back()->GetNode();
		PathingNode* clusterNodeEnd = graph->FindClusterNode(actorPathNode->GetCluster());

		//lets try to add surrounding clusters
		std::unordered_set<PathingNode*> pathingClusterNodes;
		std::map<PathingCluster*, PathingArcVec> pathingClusters;
		std::multimap<float, PathingCluster*, std::less<float>> pathingClusterWeights;
		actorPathNode->GetClusters(AT_MOVE, 40, pathingClusters, pathingClusterWeights);
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != pathingClusterWeights.end(); ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());
			if (clusterNodeEnd == pathingClusterNodeEnd)
				continue;

			pathingClusterNodes.insert(pathingClusterNodeEnd);

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

		pathingClusters.clear();
		pathingClusterWeights.clear();
		actorPathNode->GetClusters(AT_JUMP, 30, pathingClusters, pathingClusterWeights);
		for (auto itCluster = pathingClusterWeights.begin(); itCluster != pathingClusterWeights.end(); ++itCluster)
		{
			PathingNode* pathingClusterNodeEnd = graph->FindClusterNode((*itCluster).second->GetTarget()->GetCluster());
			if (clusterNodeEnd == pathingClusterNodeEnd)
				continue;

			if (pathingClusterNodes.find(pathingClusterNodeEnd) != pathingClusterNodes.end())
				continue;

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

void AIGameSimulation::BuildActorPath(std::shared_ptr<PathingGraph>& graph,
	unsigned int actionType, const std::map<ActorId, float>& gameItems, const std::map<ActorId, float>& searchItems,
	const PlayerData& player, PathingNode* clusterNodeStart, const PathingArcVec& clusterPathStart, float clusterPathOffset,
	Concurrency::concurrent_unordered_map<unsigned long long, std::pair<PathingCluster*, PathingCluster*>>& clusterPathings,
	const Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& clusterNodePathPlans,
	Concurrency::concurrent_unordered_map<unsigned long long, float>& actorPathPlanClusterHeuristics,
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>& actorPathPlanClusters)
{
	// find the best path using an A* search algorithm
	AIFinder aiFinder;
	std::map<PathingActorVec, float> actorsPathPlans;
	aiFinder(clusterNodeStart, searchItems, actorsPathPlans, actionType);

	unsigned short actorIndex = 0;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingActorVec> actorPaths;
	for (auto& actorsPathPlan : actorsPathPlans)
	{
		actorPaths[actorIndex] = std::move(actorsPathPlan.first);
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

					//heuristic from picked up items
					float nodeHeuristic = 0.f;
					for (auto const& item : playerData.items)
						nodeHeuristic += CalculateHeuristicItem(playerData, item.first, playerData.itemWeight.at(item.first));
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

				//heuristic from picked up items
				float nodeHeuristic = 0.f;
				for (auto const& item : playerData.items)
					nodeHeuristic += CalculateHeuristicItem(playerData, item.first, playerData.itemWeight.at(item.first));
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

void AIGameSimulation::BuildPlayerPath(const AIAnalysis::PlayerSimulation& playerSimulation,
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
bool AIGameSimulation::SimulatePlayerGuessing(
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

	std::vector<std::pair<ActorId, unsigned int>> actionTypes
		{ { playerDataIn.player, AT_MOVE }, { playerDataIn.player, AT_JUMP },
		{ otherPlayerDataIn.player, AT_MOVE }, { otherPlayerDataIn.player, AT_JUMP } };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	//threat level for visibility/damage calculation.
	unsigned short threatLevel = gameEvaluation.threat;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics); }
		);
	}
	else
	{
		if (!BuildLongPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
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
				threatLevel, player, itClusterNodePathPlan->second, playerPathOffset,
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
			Simulation((EvaluationType)gameEvaluation.type, gameItems, threatLevel,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
					threatLevel, player, itClusterNodePathPlan->second, playerPathOffset,
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
					threatLevel, player, itClusterNodePathPlan->second, playerPathOffset,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;

	otherPlayerDataOut.valid = true;
	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player guessing decisions end");
	return true;
}

bool AIGameSimulation::SimulatePlayerDecision(
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

	std::vector<std::pair<ActorId, unsigned int>> actionTypes
		{ { playerDataIn.player, AT_MOVE }, { playerDataIn.player, AT_JUMP },
		{ otherPlayerDataIn.player, AT_MOVE }, { otherPlayerDataIn.player, AT_JUMP } };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	//threat level for visibility/damage calculation.
	unsigned short threatLevel = gameEvaluation.threat;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
	}
	else
	{
		if (!BuildLongPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
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
			Simulation((EvaluationType)gameEvaluation.type, gameItems, threatLevel,
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
			Simulation((EvaluationType)gameEvaluation.type, gameItems, threatLevel,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
			Simulation((EvaluationType)gameEvaluation.type, gameItems, threatLevel,
				player, playerPathPlan, playerPathOffset, otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

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
				Simulation((EvaluationType)gameEvaluation.type, gameItems, threatLevel,
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
				Simulation((EvaluationType)gameEvaluation.type, gameItems, threatLevel,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
			Simulation((EvaluationType)gameEvaluation.type, 
				gameItems, threatLevel, player, playerPathPlan, playerPathOffset,
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
	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;

	otherPlayerDataOut.valid = true;
	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player decisions end");
	return true;
}


//RUNTIME SIMULATIONS

bool AIGameSimulation::SimulatePlayerGuessing(const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut, unsigned short& threatLevel,
	const std::map<ActorId, float>& gameItems, EvaluationType* playerEvaluation, EvaluationType evaluation)
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

	std::vector<std::pair<ActorId, unsigned int>> actionTypes
		{ { playerDataIn.player, AT_MOVE }, { playerDataIn.player, AT_JUMP },
		{ otherPlayerDataIn.player, AT_MOVE }, { otherPlayerDataIn.player, AT_JUMP } };

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && (*playerEvaluation) == ET_AWARENESS)
		return false;

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
	}
	else
	{
		if (!BuildLongPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && (*playerEvaluation) == ET_AWARENESS)
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
			if (evaluation != ET_AWARENESS && (*playerEvaluation) == ET_AWARENESS)
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
			Simulation(evaluation, gameItems, threatLevel, 
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
			Simulation(evaluation, gameItems, threatLevel,
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
			if (evaluation != ET_AWARENESS && (*playerEvaluation) == ET_AWARENESS)
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
			Simulation(evaluation, gameItems, threatLevel, 
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
			Simulation(evaluation, gameItems, threatLevel, 
				player, playerPathPlan, playerPathOffset,
				otherPlayer, otherPlayerPathPlan, otherPlayerPathOffset);

			playerSimulations[ULLONG_MAX] = player.heuristic;
			playerWeaponSimulations[ULLONG_MAX] = (unsigned short)player.weapon << 8 | (unsigned short)otherPlayer.weapon;
		}
		playerGuessings[ULLONG_MAX].insert(playerSimulations.begin(), playerSimulations.end());
		playerWeaponGuessings[ULLONG_MAX].insert(playerWeaponSimulations.begin(), playerWeaponSimulations.end());
	}

	//we need to stop the simulation if an aware decision making has started
	if (evaluation != ET_AWARENESS && (*playerEvaluation) == ET_AWARENESS)
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
				Simulation(evaluation, gameItems, threatLevel, 
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
				Simulation(evaluation, gameItems, threatLevel,
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
			Simulation(evaluation, gameItems, threatLevel, 
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
			Simulation(evaluation, gameItems, threatLevel, 
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
	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;

	otherPlayerDataOut.valid = true;
	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player guessing decisions end");
	return true;
}

bool AIGameSimulation::SimulatePlayerDecision(const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut, unsigned short& threatLevel,
	const std::map<ActorId, float>& gameItems, EvaluationType* playerEvaluation, EvaluationType evaluation)
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

	std::vector<std::pair<ActorId, unsigned int>> actionTypes
		{ { playerDataIn.player, AT_MOVE }, { playerDataIn.player, AT_JUMP },
		{ otherPlayerDataIn.player, AT_MOVE }, { otherPlayerDataIn.player, AT_JUMP } };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
	}
	else
	{
		if (!BuildLongPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
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
			Simulation(evaluation, gameItems, threatLevel,
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
			Simulation(evaluation, gameItems, threatLevel,
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
			Simulation(evaluation, gameItems, threatLevel,
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
			Simulation(evaluation, gameItems, threatLevel, 
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
				Simulation(evaluation, gameItems, threatLevel, 
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
				Simulation(evaluation, gameItems, threatLevel, 
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
			Simulation(evaluation, gameItems, threatLevel, 
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
			Simulation(evaluation, gameItems, threatLevel, 
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
	if (playerDataOut.plan.id == -1)
		playerDataOut.plan.id = GetNewPlanID();

	playerDataOut.planWeight = playerPathOffset;
	playerDataOut.planWeight += diffTime / 1000.f;

	otherPlayerDataOut.valid = true;
	otherPlayerDataOut.heuristic = -otherPlayerDataOut.heuristic;
	if (otherPlayerDataOut.plan.id == -1)
		otherPlayerDataOut.plan.id = GetNewPlanID();

	otherPlayerDataOut.planWeight = otherPlayerPathOffset;
	otherPlayerDataOut.planWeight += diffTime / 1000.f;

	//printf("\n player decisions end");
	return true;
}


bool AIGameSimulation::SimulateClusterPathing(
	const std::map<ActorId, float>& gameItems,
	const PlayerData& playerDataIn, PlayerData& playerDataOut,
	const PlayerData& otherPlayerDataIn, PlayerData& otherPlayerDataOut,
	std::unordered_set<PathingNode*>& playerClusterPathings, 
	std::unordered_set<PathingNode*>& playerClusterExpandedPathings,
	std::unordered_set<PathingNode*>& otherPlayerClusterPathings, 
	std::unordered_set<PathingNode*>& otherPlayerClusterExpandedPathings)
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

	std::vector<std::pair<ActorId, unsigned int>> actionTypes
		{ { playerDataIn.player, AT_MOVE }, { playerDataIn.player, AT_JUMP },
		{ otherPlayerDataIn.player, AT_MOVE }, { otherPlayerDataIn.player, AT_JUMP } };

	//cluster node offset
	clusterNodeStart = playerDataOut.plan.node;
	otherClusterNodeStart = otherPlayerDataOut.plan.node;

	//threat level for visibility/damage calculation.
	unsigned short threatLevel = 4;

	Concurrency::concurrent_unordered_map<unsigned long long,
		std::pair<PathingCluster*, PathingCluster*>> clusterPathings, otherClusterPathings;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> clusterNodePathPlans, otherClusterNodePathPlans;
	Concurrency::concurrent_unordered_map<unsigned long long, float> actorPathPlanClusterHeuristics, otherActorPathPlanClusterHeuristics;
	Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> actorPathPlanClusters, otherActorPathPlanClusters;

	if (BuildPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart, 
		clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
	{
		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
	}
	else
	{
		if (!BuildLongPath(threatLevel, mPathingGraph, clusterNodeStart, otherClusterNodeStart,
			clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans))
		{
			BuildLongestPath(mPathingGraph, clusterNodeStart, otherClusterNodeStart,
				clusterPathings, otherClusterPathings, clusterNodePathPlans, otherClusterNodePathPlans);
		}

		std::mutex mutex;

		Concurrency::parallel_for_each(begin(actionTypes), end(actionTypes), [&](auto& actionType)
		//for (auto& actionType : actionTypes)
		{
			if (actionType.first == playerDataIn.player)
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, searchItems,
					playerDataIn, clusterNodeStart, playerPathPlanOffset, playerPathOffset, localClusterPathings,
					clusterNodePathPlans, localActorPathPlanClusterHeuristics, localActorPathPlanClusters);

				mutex.lock();
				clusterPathings.insert(localClusterPathings.begin(), localClusterPathings.end());

				actorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				actorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
			else
			{
				Concurrency::concurrent_unordered_map<unsigned long long,
					std::pair<PathingCluster*, PathingCluster*>> localOtherClusterPathings;
				Concurrency::concurrent_unordered_map<unsigned long long, float> localActorPathPlanClusterHeuristics;
				Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec> localActorPathPlanClusters;

				//other player
				BuildActorPath(mPathingGraph, actionType.second, gameItems, otherSearchItems,
					otherPlayerDataIn, otherClusterNodeStart, otherPlayerPathPlanOffset, otherPlayerPathOffset,
					localOtherClusterPathings, otherClusterNodePathPlans, localActorPathPlanClusterHeuristics,
					localActorPathPlanClusters);

				mutex.lock();
				otherClusterPathings.insert(localOtherClusterPathings.begin(), localOtherClusterPathings.end());

				otherActorPathPlanClusterHeuristics.insert(
					localActorPathPlanClusterHeuristics.begin(), localActorPathPlanClusterHeuristics.end());
				otherActorPathPlanClusters.insert(localActorPathPlanClusters.begin(), localActorPathPlanClusters.end());
				mutex.unlock();
			}
		});

		Concurrency::parallel_invoke(
			[&] {			
				//player
				BuildExpandedActorPath(mPathingGraph, clusterNodeStart,
					clusterPathings, actorPathPlanClusters, actorPathPlanClusterHeuristics); },
			[&] {
				//other player
				BuildExpandedActorPath(mPathingGraph, otherClusterNodeStart,
					otherClusterPathings, otherActorPathPlanClusters, otherActorPathPlanClusterHeuristics);}
		);
	}

	for (auto& itCluster = clusterPathings.begin(); itCluster != clusterPathings.end(); itCluster++)
	{
		PathingCluster* playerClusterStart = (*itCluster).second.first;
		PathingCluster* playerClusterEnd = (*itCluster).second.second;

		//cluster code
		unsigned long long clusterCode = (*itCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itClusterNodePathPlan;
		itClusterNodePathPlan = actorPathPlanClusters.find(clusterCode);
		if (itClusterNodePathPlan == actorPathPlanClusters.end())
			itClusterNodePathPlan = clusterNodePathPlans.find(clusterCode);

		PathingNode* pathingClusterNodeEnd = mPathingGraph->FindClusterNode(playerClusterStart->GetTarget()->GetCluster());
		for (auto& itPathArc = itClusterNodePathPlan->second.begin(); itPathArc != itClusterNodePathPlan->second.end(); itPathArc++)
		{
			playerClusterPathings.insert((*itPathArc)->GetNode());
			if (pathingClusterNodeEnd == (*itPathArc)->GetNode() ||
				playerClusterExpandedPathings.find(pathingClusterNodeEnd) != playerClusterExpandedPathings.end())
			{
				playerClusterExpandedPathings.insert((*itPathArc)->GetNode());
			}
		}
	}

	for (auto& itOtherCluster = otherClusterPathings.begin(); itOtherCluster != otherClusterPathings.end(); itOtherCluster++)
	{
		PathingCluster* otherPlayerClusterStart = (*itOtherCluster).second.first;
		PathingCluster* otherPlayerClusterEnd = (*itOtherCluster).second.second;

		//other cluster code
		unsigned long long otherClusterCode = (*itOtherCluster).first;
		Concurrency::concurrent_unordered_map<unsigned long long, PathingArcVec>::iterator itOtherClusterNodePathPlan;
		itOtherClusterNodePathPlan = otherActorPathPlanClusters.find(otherClusterCode);
		if (itOtherClusterNodePathPlan == otherActorPathPlanClusters.end())
			itOtherClusterNodePathPlan = otherClusterNodePathPlans.find(otherClusterCode);

		PathingNode* otherPathingClusterNodeEnd = mPathingGraph->FindClusterNode(otherPlayerClusterStart->GetTarget()->GetCluster());
		for (auto& itOtherPathArc = itOtherClusterNodePathPlan->second.begin(); itOtherPathArc != itOtherClusterNodePathPlan->second.end(); itOtherPathArc++)
		{
			otherPlayerClusterPathings.insert((*itOtherPathArc)->GetNode());
			if (otherPathingClusterNodeEnd == (*itOtherPathArc)->GetNode() ||
				otherPlayerClusterExpandedPathings.find(otherPathingClusterNodeEnd) != otherPlayerClusterExpandedPathings.end())
			{
				otherPlayerClusterExpandedPathings.insert((*itOtherPathArc)->GetNode());
			}
		}
	}

	return true;
}

void AIGameSimulation::GetPlayerSimulation(const AIAnalysis::PlayerSimulation& playerSimulation, PlayerData& playerData)
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
	playerData.itemAmount = playerSimulation.itemAmount;
	playerData.itemWeight = playerSimulation.itemWeight;
}

void AIGameSimulation::SetPlayerSimulation(AIAnalysis::PlayerSimulation& playerSimulation, const PlayerData& playerData)
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