//========================================================================
// HumanView.h : Defines the HumanView class of the GameEngine application
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

#ifndef HUMANVIEW_H
#define HUMANVIEW_H

#include "GameEngineStd.h"

#include "GameView.h"
#include "Game/GameLogic.h"

#include "Graphic/UI/UIEngine.h"
#include "Graphic/ScreenElement.h"

#include "Core/Process/ProcessManager.h"
#include "Core/Event/EventManager.h"

#include "Application/System/Keys.h"

class ScreenElementScene;
class CameraNode;
class Renderer;
class System;


class HumanView : public BaseGameView
{
	friend class GameApplication;

public:

	HumanView();

	virtual ~HumanView();

	// Implement the BaseGameView interface, except for the OnRender() method, which is renderer specific
	virtual bool OnRestore();
	virtual bool OnLostDevice();

	virtual void OnRender(double time, float elapsedTime);
	virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs);

	virtual GameViewType GetType() { return GV_HUMAN; }
	virtual GameViewId GetId() const { return mViewId; }
	virtual ActorId GetActorId() const { return mActorId; }

	virtual void OnAttach(GameViewId vid, ActorId aid)
	{
		mViewId = vid; 
		mActorId = aid;
	}
	virtual bool OnMsgProc( const Event& evt);

	// Virtual methods to control the layering of interface elements
	virtual void PushElement(const std::shared_ptr<BaseScreenElement>& pElement);
	virtual void RemoveElement(const std::shared_ptr<BaseScreenElement>& pElement);

	void TogglePause(bool active);
    bool LoadGame(tinyxml2::XMLElement* pLevelData);

	/*
		list of pointers to objects that implement the BaseScreenElement interface.
	*/
	std::list<std::shared_ptr<BaseScreenElement>> mScreenElements; // a game screen entity
																		
	/*
		Interface sensitive objects
		mouse and keyboard handler interpret device messages into game commands
	*/
	std::shared_ptr<BaseMouseHandler> mMouseHandler;
	std::shared_ptr<BaseKeyboardHandler> mKeyboardHandler;

	ProcessManager* GetProcessManager() { return mProcessManager; }

	// Added post press
	std::shared_ptr<ScreenElementScene> mScene;
	std::shared_ptr<CameraNode> mCamera;

	// Added post press - this helps the network system attach views to the right actor.
	virtual void SetControlledActor(ActorId actorId) { mActorId = actorId; }

	// Event delegates
    void GameStateDelegate(BaseEventDataPtr pEventData);

protected:

	/*
		ViewId and ActorId makes easy for the game logic to determine if a view is attached
		to a particular actor in the game universe
	*/
	GameViewId mViewId;
	ActorId mActorId;

	/*
		ProcessManager is a convenient manager for anything that takes multiple game loops
		to accomplish, such as running an animation
	*/
	ProcessManager* mProcessManager;

	BaseGameState mGameState;	// Added post-press - what is the current game state
	
	/*
		This member, once overloaded in an inherited class, is what is called when text-specific
		elements need to be drawn by the view.
	*/
	virtual void RenderText() { };

	/*
		It is reponsible for creating view-specific elements from an XML file that defines all the
		elements in the game. It might include a background music track, which could be appreciated
		by the human playing but is inconsequential for the game logic
	*/
	virtual bool LoadGameDelegate(tinyxml2::XMLElement* pLevelData);

private:
	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);
};


#endif