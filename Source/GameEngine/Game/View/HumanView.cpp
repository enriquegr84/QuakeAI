//========================================================================
// HumanView.cpp - Implements the class HumanView, which provides a Human interface into the game
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

#include "HumanView.h"

#include "Game/Actor/AudioComponent.h"
#include "Game/Actor/RenderComponent.h"

//events related
#include "Application/GameApplication.h"
#include "Application/System/System.h"

#include "Graphic/Scene/Scene.h"
#include "Graphic/Renderer/Renderer.h"

#include "Core/Event/Event.h"

#include "Core/Process/Process.h"

const GameViewId INVALID_GAME_VIEW_ID = 0xffffffff;

template<class T>
struct SortBySharedPtrContent
{
	bool operator()(const std::shared_ptr<T> &lhs, const std::shared_ptr<T> &rhs) const
	{
		return *lhs < *rhs;
	}
};

//
// HumanView::HumanView - Chapter 10, page 272
//
HumanView::HumanView()
{
	mProcessManager = new ProcessManager();

	mViewId = INVALID_GAME_VIEW_ID;
	mActorId = INVALID_ACTOR_ID;

	// Added post press for move, new, and destroy actor events and others
	RegisterAllDelegates();
	mGameState = BGS_INITIALIZING;		// what is the current game state

	Renderer* renderer = Renderer::Get();
	if (renderer)
	{
		// Create the scene and camera.
		mScene.reset(new ScreenElementScene());

		mCamera.reset(new CameraNode(GameLogic::Get()->GetNewActorID()));
		mCamera->AttachAnimator(std::make_shared<NodeAnimatorFollowCamera>());

		LogAssert(mScene && mCamera, "Out of memory");
		mScene->SetActiveCamera(mCamera);

		mScene->AddChild(mCamera->GetId(), mCamera);
		mScene->GetRootNode()->Update();
	}
}


HumanView::~HumanView()
{
	// [mrmike] - RemoveAllDelegates was added post press to handle move, new, and destroy actor events.
	RemoveAllDelegates();

	while (!mScreenElements.empty())
		mScreenElements.pop_front();

	delete mProcessManager;

	mProcessManager = nullptr;
}


bool HumanView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{ 
	PushElement(mScene);  
	return true; 
}

bool HumanView::LoadGame(tinyxml2::XMLElement* pLevelData)
{
    // call the delegate method
    return LoadGameDelegate(pLevelData);
}

/*
	OnRender method is responsible for rendering the view.
	The loop iterates through the screen layers and if it is visible, it calls its own render method.
*/
void HumanView::OnRender(double time, float elapsedTime )
{
	mScreenElements.sort(SortBySharedPtrContent<BaseScreenElement>());

	for(std::list<std::shared_ptr<BaseScreenElement>>::iterator it =
		mScreenElements.begin(); it!=mScreenElements.end(); ++it)
	{
		if ((*it)->IsVisible())
		{
			(*it)->OnRender(time, elapsedTime);
		}
	}

    RenderText();
}

/*
	OnRestore method is responsible for recreating anything that might be lost while the game
	is running. It typically happens as a result of the operating system responding to something
	application wide, such as restoring the application from a sleep mode or changing the screen
	resolution while the game is running. OnRestore method gets called just after the class is
	instantiated, so this method is just as useful for initialization as it is for restoring lost
	objects. These objects include all of the attached screens
*/
bool HumanView::OnRestore()
{
	bool hr = true;
	for(std::list<std::shared_ptr<BaseScreenElement>>::iterator it =
		mScreenElements.begin(); it != mScreenElements.end(); ++it)
	{
		return ( (*it)->OnRestore() );
	}

	return hr;
}

/*
	OnLostDevice will be called prior to OnRestore(), so it is used to chain the "on lost device"
	event to other objects or simply release the objects so they will be re-created in the call
	OnRestore(). Recursively calls OnLostDevice for everything attached to the HumanView. 
*/
bool HumanView::OnLostDevice() 
{
	for(std::list<std::shared_ptr<BaseScreenElement>>::iterator it =
		mScreenElements.begin(); it!=mScreenElements.end(); ++it)
	{
		return ( (*it)->OnLostDevice() );
	}
	return true;
}


void HumanView::TogglePause(bool active)
{
	// Pause or resume audio
}

/*
	HumanView do process device messages from the application layer. Any conceivable message that
	the game views would want to see should be translated into the generic message form and passed
	on to all the game views. If the game view returns true it means that it has completely consumed
	the message, and no other view should see it. The HumanView class can contain multiple screens,
	but instead of being layered, they will sit side by side. The HumanView class will still grab
	input from all the devices and translate it into game commands, just as you are about to see, but
	in this case, each device will be treated as input for a different player.
	The OnMsgProc iterates through the list of screens attached to it, forward the message on the
	visible ones, and if they don't eat the message, then ask the handlers if they can consume it.
	We can hook own device input device implementation by using the handlers. The existence of the
	handler is always checked before the message is sent to it.
	It is used a reverse iterator for the screens because otherwise the screen on top is going to be
	the last one drawn. User input should always be processed in order of the screens from top to 
	bottom, or reverse order.
*/
bool HumanView::OnMsgProc( const Event& evt )
{
	// Iterate through the screen layers first
	// In reverse order since we'll send input messages to the 
	// screen on top
	for(std::list<std::shared_ptr<BaseScreenElement>>::reverse_iterator it =
		mScreenElements.rbegin(); it!=mScreenElements.rend(); ++it)
	{
		if ( (*it)->IsVisible() )
		{
			if ( (*it)->OnMsgProc( evt ) )
			{
				return true;
			}
		}
	}

	bool result = 0;
    switch (evt.mEventType)
    {
        case ET_UI_EVENT:
            // hey, why is the user sending gui events..?
            break;

        case ET_KEY_INPUT_EVENT:
        {
			if (evt.mKeyInput.mPressedDown)
			{
				if (mKeyboardHandler)
					result = mKeyboardHandler->OnKeyDown(evt.mKeyInput);
			}
			else
			{
				if (mKeyboardHandler)
					result = mKeyboardHandler->OnKeyUp(evt.mKeyInput);
			}
        }
        break;

        case ET_MOUSE_INPUT_EVENT:
        {
			if (mMouseHandler)
			{
				KeyAction key;
				switch (evt.mMouseInput.mEvent)
				{
					case MIE_MOUSE_MOVED:
						result = mMouseHandler->OnMouseMove(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1);
						break;
					case MIE_LMOUSE_PRESSED_DOWN:
						result = mMouseHandler->OnMouseButtonDown(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1, "PointerLeft");
						break;
					case MIE_MMOUSE_PRESSED_DOWN:
						result = mMouseHandler->OnMouseButtonDown(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1, "PointerMiddle");
						break;
					case MIE_RMOUSE_PRESSED_DOWN:
						result = mMouseHandler->OnMouseButtonDown(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1, "PointerRight");
						break;
					case MIE_LMOUSE_LEFT_UP:
						result = mMouseHandler->OnMouseButtonUp(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1, "PointerLeft");
						break;
					case MIE_MMOUSE_LEFT_UP:
						result = mMouseHandler->OnMouseButtonUp(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1, "PointerMiddle");
						break;
					case MIE_RMOUSE_LEFT_UP:
						result = mMouseHandler->OnMouseButtonUp(
							Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}, 1, "PointerRight");
						break;
					case MIE_MOUSE_WHEEL:
						if (evt.mMouseInput.mWheel > 0)
							result = mMouseHandler->OnWheelRollUp();
						else
							result = mMouseHandler->OnWheelRollDown();
						break;
				}
			}
        }
        break;
    }

	return result;
}

/*
	OnUpdate is called once per frame by the application layer so that it can perform non-rendering
	update tasks. The OnUpdate() chain is called as quickly as the game loops and is used to update
	my object attached to the human view. In this case, the ProcessManager is updated, as well as
	any of the screen elements attached to the human view, such as updating the objects in the 3D
	scene, which is itself a screen element. A game object that exists in the game universe and is
	affected by game rules, like physics, belongs to the game logic. Whenever the game object moves
	or change state, events are generated that eventually make their way to the game views, where
	they update their internal representations of these objects. There is a different set of objects
	that only exist visually and have no real effect on the world themselves which is updated in 
	this function. Since the game logic knows nothing about them, they are completely contained in the 
	human view and need some way to be updated if they are animating.
	The audio system is another example of what human perceives but the game logic does not.
	Background music and ambient sound effects have no effect on the game logic per se and therefore
	can safely belong to the human view.
*/
void HumanView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	mProcessManager->UpdateProcesses(deltaMs);

	// This section of code was added post-press. It runs through the screenlist
	// and calls OnUpdate. Some screen elements need to update every frame, one 
	// example of this is a 3D scene attached to the human view.
	//
	for(std::list<std::shared_ptr<BaseScreenElement>>::iterator i =
		mScreenElements.begin(); i!=mScreenElements.end(); ++i)
	{
		(*i)->OnUpdate(timeMs, deltaMs);
	}
}
//
// HumanView::PushElement						- Chapter 10, page 274
//
void HumanView::PushElement(const std::shared_ptr<BaseScreenElement>& pElement)
{
	mScreenElements.push_front(pElement);
}

//
// HumanView::PopElement						- Chapter 10, page 274
//
//   
//
void HumanView::RemoveElement(const std::shared_ptr<BaseScreenElement>& pElement)
{
	mScreenElements.remove(pElement);
}


//
// HumanView::RegisterAllDelegates, HumanView::RemoveAllDelegates		- not described in the book
//
//   Aggregates calls to manage event listeners for the HumanView class.
//
void HumanView::RegisterAllDelegates(void)
{

    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
//	pGlobalEventManager->AddListener(
//		MakeDelegate(this, &HumanView::GameStateDelegate), EventDataGameState::skEventType);
}

void HumanView::RemoveAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
//	pGlobalEventManager->RemoveListener(
//		MakeDelegate(this, &HumanView::GameStateDelegate), EventDataGameState::skEventType);
}

//
// HumanView::GameStateDelegate							- Chapter X, page Y
//
void HumanView::GameStateDelegate(BaseEventDataPtr pEventData)
{
//    std::shared_ptr<EventDataGame_State> pCastEventData = 
//		static_pointer_cast<EventDataGameState>(pEventData);
//    mBaseGameState = pCastEventData->GetGameState(); 
}


//==============================================================
// HumanView::Console - defines the a class to manage a console to type in commands
//
//   Not described in the book. 
//
const int kCursorBlinkTimeMS = 500;

char const * const kExitString = "exit";
char const * const kClearString = "clear";
