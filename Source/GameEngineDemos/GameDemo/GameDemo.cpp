//========================================================================
//
// GameDemoLogic Implementation       - Chapter 21, page 725
//
//========================================================================

#include "GameDemoView.h"

#include "Physic/Physic.h"
#include "Physic/PhysicEventListener.h"

#include "GameDemo.h"
#include "GameDemoApp.h"
#include "GameDemoNetwork.h"
#include "GameDemoEvents.h"
#include "GameDemoManager.h"
#include "GameDemoAIManager.h"


//
// QuakeLogicThread::Run
//
void* GameDemoLogicThread::Run()
{

	/*
	 * The real business of the server happens on the QuakeLogicThread.
	 * How this works:
	 * LoadGameAsync() runs the game logical loading
	 * AsyncRunStep() runs an actual server step as soon as enough time has
	 * passed (dedicated_server_loop keeps track of that).
	 */

	mGameLogic->LoadGameAsync(mLevelData);

	while (!StopRequested())
	{
		try {
			mGameLogic->AsyncStep();
		}
		catch (std::exception&)
		{
			throw;
		}
	}

	return nullptr;
}


//
// GameDemoLogic::GameDemoLogic
//
GameDemoLogic::GameDemoLogic() : GameLogic()
{
	Settings::Get()->Set("default_gravity", "(0,-300,0)");

	mThread = new GameDemoLogicThread(this);

	mPhysics.reset(CreateGamePhysics());
	RegisterAllDelegates();
}


//
// GameDemoLogic::~GameDemoLogic
//
GameDemoLogic::~GameDemoLogic()
{
	RemoveAllDelegates();
	DestroyAllNetworkEventForwarders();

	// Stop threads
	if (mThread)
	{
		Stop();
		delete mThread;
	}
}

void GameDemoLogic::Start(tinyxml2::XMLElement* pLevelData)
{
	LogInformation("Loading game world thread ");

	// Stop thread if already running
	mThread->Stop();

	mThread->mLevelData = pLevelData;

	// Start thread
	mThread->Start();
}

void GameDemoLogic::Stop()
{
	LogInformation("Stopping and waiting threads");

	// Stop threads (set run=false first so both start stopping)
	mThread->Stop();
	//m_emergethread.setRun(false);
	mThread->Wait();
	//m_emergethread.stop();

	LogInformation("Threads stopped");
}

void GameDemoLogic::UpdateViewType(const std::shared_ptr<BaseGameView>& pView, bool add)
{
	GameLogic::UpdateViewType(pView, add);

	//  This is commented out because while the view is created and waiting, the player has NOT attached yet. 
	/*
	if (pView->GetType() == GV_REMOTE)
	{
		mHumanPlayersAttached += add ? 1 : -1;
	}
	*/
	if (pView->GetType() == GV_HUMAN)
	{
		mHumanPlayersAttached += add ? 1 : -1;
	}
	else if (pView->GetType() == GV_AI)
	{
		mAIPlayersAttached += add ? 1 : -1;
	}
}

void GameDemoLogic::ResetViewType()
{
	GameLogic::ResetViewType();

	mHumanPlayersAttached = 0;
	mAIPlayersAttached = 0;
}

void GameDemoLogic::SetProxy()
{
	GameLogic::SetProxy();
}


//
// GameDemoLogic::ChangeState
//
void GameDemoLogic::ChangeState(BaseGameState newState)
{
	GameLogic::ChangeState(newState);

	switch (newState)
	{
		case BGS_MAINMENU:
		{
			std::shared_ptr<BaseGameView> menuView(new MainMenuView());
			GameApplication::Get()->AddView(menuView);

			break;
		}

		case BGS_WAITINGFORPLAYERS:
		{
			// spawn all local players (should only be one, though we might support more in the future)
			LogAssert(mExpectedPlayers == 1, "needs only one player");
			for (int i = 0; i < mExpectedPlayers; ++i)
			{
				std::shared_ptr<BaseGameView> playersView(new GameDemoHumanView());
				GameApplication::Get()->AddView(playersView);

				if (mIsProxy)
				{
					// if we are a remote player, all we have to do is spawn our view - the server will do the rest.
					return;
				}
			}
			// spawn all remote player's views on the game
			for (int i = 0; i < mExpectedRemotePlayers; ++i)
			{
				std::shared_ptr<BaseGameView> remoteGameView(new NetworkGameView());
				GameApplication::Get()->AddView(remoteGameView);
			}

			// spawn all AI's views on the game
			for (int i = 0; i < mExpectedAI; ++i)
			{
				std::shared_ptr<BaseGameView> aiView(new AIPlayerView(std::shared_ptr<PathingGraph>()));
				GameApplication::Get()->AddView(aiView);
			}

			break;
		}

		case BGS_SPAWNINGPLAYERACTORS:
		{
			if (mIsProxy)
			{
				// only the server needs to do this.
				return;
			}

            const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
            for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
            {
                std::shared_ptr<BaseGameView> pView = *it;
                if (pView->GetType() == GV_HUMAN)
                {
                    std::shared_ptr<Actor> pActor = CreateActor("actors\\demo\\player.xml", NULL);
                    if (pActor)
                    {
                        pView->OnAttach(pView->GetId(), pActor->GetId());

                        std::shared_ptr<EventDataNewActor> pNewActorEvent(
                            new EventDataNewActor(pActor->GetId(), pView->GetId()));

                        // [rez] This needs to happen asap because the constructor function for scripting 
                        // (which is called in through CreateActor()) queues an event that expects 
                        // this event to have been handled
                        BaseEventManager::Get()->TriggerEvent(pNewActorEvent);
                    }
                }
                else if (pView->GetType() == GV_REMOTE)
                {
                    std::shared_ptr<NetworkGameView> pNetworkGameView =
                        std::static_pointer_cast<NetworkGameView, BaseGameView>(pView);
                    std::shared_ptr<Actor> pActor = CreateActor("actors\\remote_player.xml", NULL);
                    if (pActor)
                    {
                        pView->OnAttach(pView->GetId(), pActor->GetId());

                        std::shared_ptr<EventDataNewActor> pNewActorEvent(
                            new EventDataNewActor(pActor->GetId(), pNetworkGameView->GetId()));
                        BaseEventManager::Get()->QueueEvent(pNewActorEvent);
                    }
                }
                else if (pView->GetType() == GV_AI)
                {
                    std::shared_ptr<AIPlayerView> pAiView =
                        std::static_pointer_cast<AIPlayerView, BaseGameView>(pView);
                    std::shared_ptr<Actor> pActor = CreateActor("actors\\demo\\ai_player.xml", NULL);
                    if (pActor)
                    {
                        pView->OnAttach(pView->GetId(), pActor->GetId());

                        std::shared_ptr<EventDataNewActor> pNewActorEvent(
                            new EventDataNewActor(pActor->GetId(), pAiView->GetId()));
                        BaseEventManager::Get()->QueueEvent(pNewActorEvent);
                    }
                }
            }

			break;
		}
	}
}

void GameDemoLogic::SyncActor(const ActorId id, Transform const &transform)
{
	GameLogic::SyncActor(id, transform);
}

void GameDemoLogic::RequestStartGameDelegate(BaseEventDataPtr pEventData)
{
	ChangeState(BGS_WAITINGFORPLAYERS);
}

void GameDemoLogic::EnvironmentLoadedDelegate(BaseEventDataPtr pEventData)
{
	++mHumanGamesLoaded;
}


// FUTURE WORK - this isn't Demos specific so it can go into the game agnostic base class
void GameDemoLogic::RemoteClientDelegate(BaseEventDataPtr pEventData)
{
	// This event is always sent from clients to the game server.

	std::shared_ptr<EventDataRemoteClient> pCastEventData = 
		std::static_pointer_cast<EventDataRemoteClient>(pEventData);
	const int sockID = pCastEventData->GetSocketId();
	const int ipAddress = pCastEventData->GetIpAddress();

	// go find a NetworkGameView that doesn't have a socket ID, and attach this client to that view.
	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<BaseGameView> pView = *it;
		if (pView->GetType() == GV_REMOTE)
		{
			std::shared_ptr<NetworkGameView> pNetworkGameView = 
				std::static_pointer_cast<NetworkGameView, BaseGameView>(pView);
			if (!pNetworkGameView->HasRemotePlayerAttached())
			{
				pNetworkGameView->AttachRemotePlayer(sockID);
				CreateNetworkEventForwarder(sockID);
				mHumanPlayersAttached++;

				return;
			}
		}
	}

}

void GameDemoLogic::NetworkPlayerActorAssignmentDelegate(BaseEventDataPtr pEventData)
{
	if (!mIsProxy)
		return;

	// we're a remote client getting an actor assignment.
	// the server assigned us a playerId when we first attached (the server's socketId, actually)
	std::shared_ptr<EventDataNetworkPlayerActorAssignment> pCastEventData =
		std::static_pointer_cast<EventDataNetworkPlayerActorAssignment>(pEventData);

	if (pCastEventData->GetActorId() == INVALID_ACTOR_ID)
	{
		mRemotePlayerId = pCastEventData->GetSocketId();
		return;
	}

	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<BaseGameView> pView = *it;
		if (pView->GetType() == GV_HUMAN)
		{
			std::shared_ptr<GameDemoHumanView> pHumanView =
				std::static_pointer_cast<GameDemoHumanView, BaseGameView>(pView);
			if (mRemotePlayerId == pCastEventData->GetSocketId())
				pHumanView->SetControlledActor(pCastEventData->GetActorId());

			return;
		}
	}

	LogError("Could not find HumanView to attach actor to!");
}

void GameDemoLogic::JumpActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataJumpActor> pCastEventData =
		std::static_pointer_cast<EventDataJumpActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->KinematicJump(pCastEventData->GetDirection());
	}
}

void GameDemoLogic::MoveActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataMoveActor> pCastEventData =
		std::static_pointer_cast<EventDataMoveActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->KinematicMove(pCastEventData->GetDirection());
	}
}

void GameDemoLogic::RotateActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataRotateActor> pCastEventData =
		std::static_pointer_cast<EventDataRotateActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicComponent)
		{
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(1), pCastEventData->GetYaw() * (float)GE_C_DEG_TO_RAD));

			Transform transform;
			transform.SetRotation(yawRotation);
			pPhysicComponent->SetRotation(transform);
		}
	}
}

void GameDemoLogic::StartThrustDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataStartThrust> pCastEventData =
		std::static_pointer_cast<EventDataStartThrust>(pEventData);
	std::shared_ptr<Actor> pActor = GetActor(pCastEventData->GetActorId()).lock();
	if (pActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent = 
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->ApplyAcceleration(pCastEventData->GetAcceleration());
	}
}

void GameDemoLogic::EndThrustDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataStartThrust> pCastEventData =
		std::static_pointer_cast<EventDataStartThrust>(pEventData);
	std::shared_ptr<Actor> pActor = GetActor(pCastEventData->GetActorId()).lock();
	if (pActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->RemoveAcceleration();
	}
}

void GameDemoLogic::StartSteerDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataStartSteer> pCastEventData =
		std::static_pointer_cast<EventDataStartSteer>(pEventData);
	std::shared_ptr<Actor> pActor = GetActor(pCastEventData->GetActorId()).lock();
	if (pActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent = 
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->ApplyAngularAcceleration(pCastEventData->GetAcceleration());
	}
}

void GameDemoLogic::EndSteerDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataStartSteer> pCastEventData =
		std::static_pointer_cast<EventDataStartSteer>(pEventData);
	std::shared_ptr<Actor> pActor = GetActor(pCastEventData->GetActorId()).lock();
	if (pActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->RemoveAngularAcceleration();
	}
}


void GameDemoLogic::RegisterAllDelegates(void)
{
	// FUTURE WORK: Lots of these functions are ok to go into the base game logic!
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::RemoteClientDelegate), 
		EventDataRemoteClient::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::SyncActorDelegate),
		EventDataSyncActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::MoveActorDelegate), 
		EventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::RotateActorDelegate),
		EventDataRotateActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::RequestStartGameDelegate), 
		EventDataRequestStartGame::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::NetworkPlayerActorAssignmentDelegate), 
		EventDataNetworkPlayerActorAssignment::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::EnvironmentLoadedDelegate), 
		EventDataEnvironmentLoaded::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::EnvironmentLoadedDelegate), 
		EventDataRemoteEnvironmentLoaded::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::StartThrustDelegate), 
		EventDataStartThrust::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::EndThrustDelegate), 
		EventDataEndThrust::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::StartSteerDelegate), 
		EventDataStartSteer::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoLogic::EndSteerDelegate), 
		EventDataEndSteer::skEventType);
}

void GameDemoLogic::RemoveAllDelegates(void)
{
	// FUTURE WORK: See the note in RegisterDelegates above....
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::RemoteClientDelegate), 
		EventDataRemoteClient::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::SyncActorDelegate),
		EventDataSyncActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::MoveActorDelegate), 
		EventDataMoveActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::RotateActorDelegate),
		EventDataRotateActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::RequestStartGameDelegate), 
		EventDataRequestStartGame::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::NetworkPlayerActorAssignmentDelegate), 
		EventDataNetworkPlayerActorAssignment::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::EnvironmentLoadedDelegate), 
		EventDataEnvironmentLoaded::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::EnvironmentLoadedDelegate), 
		EventDataRemoteEnvironmentLoaded::skEventType);
	if (mIsProxy)
	{
		pGlobalEventManager->RemoveListener(
			MakeDelegate(this, &GameDemoLogic::RequestNewActorDelegate), 
			EventDataRequestNewActor::skEventType);
	}

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::StartThrustDelegate), 
		EventDataStartThrust::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::EndThrustDelegate), 
		EventDataEndThrust::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::StartSteerDelegate), 
		EventDataStartSteer::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoLogic::EndSteerDelegate), 
		EventDataEndSteer::skEventType);
}

void GameDemoLogic::CreateNetworkEventForwarder(const int socketId)
{
	NetworkEventForwarder* pNetworkEventForwarder = new NetworkEventForwarder(socketId);

	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();

	// then add those events that need to be sent along to amy attached clients
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataPhysSeparation::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataDestroyActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataEnvironmentLoaded::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataNewActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataSyncActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataRotateActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataRequestNewActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataNetworkPlayerActorAssignment::skEventType);

	mNetworkEventForwarders.push_back(pNetworkEventForwarder);
}

void GameDemoLogic::DestroyAllNetworkEventForwarders(void)
{
	for (auto it = mNetworkEventForwarders.begin(); it != mNetworkEventForwarders.end(); ++it)
	{
		NetworkEventForwarder* networkEventForwarder = (*it);

		BaseEventManager* eventManager = BaseEventManager::Get();
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataPhysTriggerEnter::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataPhysTriggerLeave::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataPhysCollision::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataPhysSeparation::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataDestroyActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataEnvironmentLoaded::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataNewActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataSyncActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataJumpActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataMoveActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataRotateActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataRequestNewActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataNetworkPlayerActorAssignment::skEventType);

		delete networkEventForwarder;
	}

	mNetworkEventForwarders.clear();
}

LevelManager* GameDemoLogic::CreateLevelManager(void)
{
	GameDemoManager* levelManager = new GameDemoManager();
	levelManager->AddLevelSearchDir(L"world/demo/");
	levelManager->LoadLevelList(L"*.xml");

	for (auto levelId : levelManager->GetAllLevelIds())
		Settings::Get()->Set("default_game", ToString(levelId.c_str()));
	return levelManager;
}

AIManager* GameDemoLogic::CreateAIManager(void)
{
	GameDemoAIManager* demoAIManager = new GameDemoAIManager();
	return demoAIManager;
}


bool GameDemoLogic::LoadGameAsync(tinyxml2::XMLElement* pRoot)
{
	// pre and post load scripts
	const char* preLoadScript = NULL;
	const char* postLoadScript = NULL;

	// parse the pre & post script attributes
	tinyxml2::XMLElement* pScriptElement = pRoot->FirstChildElement("Script");
	if (pScriptElement)
	{
		preLoadScript = pScriptElement->Attribute("preLoad");
		postLoadScript = pScriptElement->Attribute("postLoad");
	}

	// load all initial actors
	tinyxml2::XMLElement* pActorsNode = pRoot->FirstChildElement("StaticActors");
	if (pActorsNode)
	{
		tinyxml2::XMLElement* pNode = pActorsNode->FirstChildElement();
		for (; pNode; pNode = pNode->NextSiblingElement())
		{
			const char* actorResource = pNode->Attribute("resource");

			std::shared_ptr<Actor> pActor = CreateActor(actorResource, pNode);
			if (pActor)
			{
				// fire an event letting everyone else know that we created a new actor
				std::shared_ptr<EventDataNewActor> pNewActorEvent(new EventDataNewActor(pActor->GetId()));
				BaseEventManager::Get()->QueueEvent(pNewActorEvent);
			}
		}
	}

	return true;
}

bool GameDemoLogic::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
	return true;
}