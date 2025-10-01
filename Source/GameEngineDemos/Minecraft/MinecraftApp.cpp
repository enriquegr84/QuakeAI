//========================================================================
// Minecraft.cpp : source file for the sample game
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

#include "Game/Game.h"
#include "Games/Games.h"

#include "Physic/Physic.h"
#include "Physic/PhysicEventListener.h"

#include "Core/Logger/LogReporter.h"
#include "Core/IO/BaseEnvironment.h"

#include "Core/Event/Event.h"
#include "Core/Event/EventManager.h"

#include "Minecraft.h"
#include "MinecraftApp.h"
#include "MinecraftView.h"
#include "MinecraftNetwork.h"

//========================================================================
// main - Defines the entry point to the application, the GameApplication handles the 
// initialization. This allows the GameEngine function to live in a library, 
// separating the game engine from game specific code, in this case Minecraft.
//========================================================================

int main()
{
#if defined(_DEBUG)
	LogReporter reporter(
		"",
		Logger::Listener::LISTEN_FOR_NOTHING,
		Logger::Listener::LISTEN_FOR_NOTHING,
		Logger::Listener::LISTEN_FOR_NOTHING,
		Logger::Listener::LISTEN_FOR_ALL);
#endif

	// Application entry point. It is the startup function used for initialization
#ifndef __APPLE__
	Application::ApplicationPath = BaseEnvironment::GetAbsolutePath("");
#else
	// Mac OS X Lion returns NULL on any getenv call (such as the one in
	// BaseEnvironment::GetVariable).  This hack works around the problem.
	if (system("cp ~/.MacOSX/apppath.txt tempapppath.txt") == 0)
	{
		std::ifstream inFile("tempapppath.txt");
		if (inFile)
		{
			getline(inFile, Application::AppPath);
			inFile.close();
			system("rm tempapppath.txt");
		}
	}
#endif
	if (Application::ApplicationPath == "")
	{
		LogError("Unknown application path");
		return INT_MAX;
	}
	Application::ApplicationPath += "/";

	// Initialization
	MinecraftApp* minecraftApp = new MinecraftApp();
	Application::App = minecraftApp;

	int exitCode = -1;
	try
	{
		Application::App->OnRun();
		exitCode = 0;
	}
	catch (...)
	{
		// Catch all exceptions – dangerous!!!  
		// Respond (perhaps only partially) to the exception, then  
		// re-throw to pass the exception to some other handler  
		// throw;
		LogError("An error happend during execution.\n");
	}
	//delete0(Application::TheCommand);

	// Termination
	delete Application::App;

	return exitCode;
}

//========================================================================
//
// MinecraftApp Implementation     - Chapter 21, page 722
//
//========================================================================

//----------------------------------------------------------------------------
MinecraftApp::MinecraftApp()
	: GameApplication("Minecraft", 0, 0, 1024, 600, { 0.392f, 0.584f, 0.929f, 1.0f })
{
}

//----------------------------------------------------------------------------
MinecraftApp::~MinecraftApp()
{
}

bool MinecraftApp::OnInitialize()
{
    if (!GameApplication::OnInitialize())
        return false;

	std::string config = Application::ApplicationPath + "../../Assets/Config/Minecraft/minecraft.conf";
    Settings::Get()->ReadConfigFile(config.c_str());
    return true;
}

void MinecraftApp::OnTerminate()
{
	GameApplication::OnTerminate();

	// Update configuration file
	std::string config = Application::ApplicationPath + "../../Assets/Config/Minecraft/minecraft.conf";
	Settings::Get()->UpdateConfigFile(config.c_str());
}

//
// MinecraftApp::CreateGame
//
void MinecraftApp::CreateGame()
{
	MinecraftLogic* game = new MinecraftLogic();
	game->Init();
}

//
// MinecraftApp::LoadGame
//
bool MinecraftApp::LoadGame()
{
	// Read the game options and see what the current game
	// needs to be - all of the game graphics are initialized by now, too...
	return GameLogic::Get()->LoadGame(Settings::Get()->Get("selected_game").c_str());
}

//
// MinecraftLogic::AddView
//
void MinecraftApp::AddView(const std::shared_ptr<BaseGameView>& pView)
{
	GameApplication::AddView(pView);
	GameLogic::Get()->UpdateViewType(pView, true);
}

//
// MinecraftLogic::RemoveView
//
void MinecraftApp::RemoveView(const std::shared_ptr<BaseGameView>& pView)
{
	GameApplication::RemoveView(pView);
	GameLogic::Get()->UpdateViewType(pView, false);
}

//remove game view
void MinecraftApp::RemoveView()
{
	GameLogic::Get()->UpdateViewType(mGameViews.front(), false);
	GameApplication::RemoveView();
}

// Added this to explicitly remove views from the game logic list.
void MinecraftApp::RemoveViews()
{
	GameLogic::Get()->ResetViewType();
	while (!mGameViews.empty())
		mGameViews.pop_front();
}

void MinecraftApp::RegisterGameEvents(void)
{
    REGISTER_EVENT(EventDataPlayerRespawn);
}

void MinecraftApp::CreateNetworkEventForwarder(void)
{
	GameApplication::CreateNetworkEventForwarder();
    if (mNetworkEventForwarder != NULL)
    {
	    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
		//	FUTURE WORK - Events should have a "classification" that signals if 
		//	they are sent from client to server, from server to client, or both.
		//	Then as events are created, they are automatically added to the right 
		//	network forwarders. 

        pGlobalEventManager->AddListener(
            MakeDelegate(mNetworkEventForwarder.get(), &NetworkEventForwarder::ForwardEvent),
            EventDataPlayerRespawn::skEventType);
	}
}

void MinecraftApp::DestroyNetworkEventForwarder(void)
{
	GameApplication::DestroyNetworkEventForwarder();
    if (mNetworkEventForwarder)
    {
        BaseEventManager* eventManager = BaseEventManager::Get();

        eventManager->RemoveListener(
            MakeDelegate(mNetworkEventForwarder.get(), &NetworkEventForwarder::ForwardEvent),
            EventDataPlayerRespawn::skEventType);

        //delete mNetworkEventForwarder.get();
    }
}