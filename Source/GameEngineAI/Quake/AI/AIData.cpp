/*******************************************************
 * AIData.cpp : AI Data class
 * Copyright (C) GameEngineAI - All Rights Reserved
 * Written by Enrique González Rodríguez <enriquegr84@hotmail.es>, 2019-2025
 *******************************************************/

#include "AIData.h"

 // NodePlan
NodePlan::NodePlan()
{
	id = -1;
	node = NULL;
	weight = 0.f;
}

NodePlan::NodePlan(PathingNode* playerNode, PathingArcVec& path)
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

NodePlan::~NodePlan()
{

}

void NodePlan::AddPathPlan(const PathingArcVec& pathPlan)
{
	for (PathingArc* pathArc : pathPlan)
	{
		weight += pathArc->GetWeight();
		path.push_back(pathArc);
	}
}

void NodePlan::ResetPathPlan(const PathingArcVec& pathPlan)
{
	weight = 0.f;
	path.clear();
	for (PathingArc* pathArc : pathPlan)
	{
		weight += pathArc->GetWeight();
		path.push_back(pathArc);
	}
}


// PlayerData
PlayerData::PlayerData()
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

	ResetItems();
}

PlayerData::PlayerData(std::shared_ptr<PlayerActor> playerActor)
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

	ResetItems();
}

PlayerData::~PlayerData()
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