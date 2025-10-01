//========================================================================
// Demos.h : source file for the sample game
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
//    http://code.google.com/p/GameEngine/
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

#ifndef GAMEDEMO_H
#define GAMEDEMO_H

#include "Game/Game.h"

class BaseEventManager;
class NetworkEventForwarder;

//---------------------------------------------------------------------------------------------------------------------
// GameDemoLogic class                        - Chapter 21, page 723
//---------------------------------------------------------------------------------------------------------------------
class GameDemoLogic : public GameLogic
{
	friend class GameDemoLogicThread;

public:
	GameDemoLogic();
	virtual ~GameDemoLogic();

	// Demos Methods

	// Update
	virtual void SetProxy();

	virtual void SyncActor(const ActorId id, Transform const &transform);

	virtual void ResetViewType();
	virtual void UpdateViewType(const std::shared_ptr<BaseGameView>& pView, bool add);

	virtual void Start(tinyxml2::XMLElement* pLevelData);
	virtual void Stop();

	void Step(float dTime) { };

	// This is run by logic thread and does the actual processing
	void AsyncStep() { };

	// Overloads
	virtual void ChangeState(BaseGameState newState);
	virtual std::shared_ptr<BaseGamePhysic> GetGamePhysics(void) { return mPhysics; }

	// event delegates
	void RequestStartGameDelegate(BaseEventDataPtr pEventData);
	void RemoteClientDelegate(BaseEventDataPtr pEventData);
	void NetworkPlayerActorAssignmentDelegate(BaseEventDataPtr pEventData);
	void EnvironmentLoadedDelegate(BaseEventDataPtr pEventData);
	void JumpActorDelegate(BaseEventDataPtr pEventData);
	void MoveActorDelegate(BaseEventDataPtr pEventData);
	void RotateActorDelegate(BaseEventDataPtr pEventData);
	void StartThrustDelegate(BaseEventDataPtr pEventData);
	void EndThrustDelegate(BaseEventDataPtr pEventData);
	void StartSteerDelegate(BaseEventDataPtr pEventData);
	void EndSteerDelegate(BaseEventDataPtr pEventData);

protected:

	virtual LevelManager* CreateLevelManager(void);
	virtual AIManager* CreateAIManager(void);

	std::list<NetworkEventForwarder*> mNetworkEventForwarders;

	virtual bool LoadGameDelegate(tinyxml2::XMLElement* pRoot);

	bool LoadGameAsync(tinyxml2::XMLElement* pRoot);

private:
	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);
	void CreateNetworkEventForwarder(const int socketId);
	void DestroyAllNetworkEventForwarders(void);

	/* Threads */

	// The demo logic mainly operates in this thread
	GameDemoLogicThread* mThread = nullptr;
};


class GameDemoLogicThread : public Thread
{
	friend class GameDemoLogic;

public:

	GameDemoLogicThread(GameDemoLogic* logic) : Thread("GameDemoLogic"), mGameLogic(logic)
	{

	}

	virtual void* Run();

protected:

	tinyxml2::XMLElement* mLevelData;

private:
	GameDemoLogic* mGameLogic;
};


#endif