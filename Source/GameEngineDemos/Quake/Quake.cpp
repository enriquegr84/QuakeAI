//========================================================================
//
// QuakeLogic Implementation       - Chapter 21, page 725
//
//========================================================================

#include "Quake.h"
#include "QuakeApp.h"
#include "QuakeView.h"
#include "QuakeAIView.h"
#include "QuakeAIManager.h"
#include "QuakeNetwork.h"
#include "QuakeEvents.h"
#include "QuakeActorFactory.h"
#include "QuakeLevelManager.h"

#include "Data/Database.h"
#include "Data/MetricsBackend.h"

#include "Physic/Physic.h"
#include "Physic/PhysicEventListener.h"
#include "Physic/Importer/PhysicResource.h"

#include "Core/Utility/SHA1.h"
#include "Core/Utility/Profiler.h"

#include "Games/Actors/GrenadeFire.h"
#include "Games/Actors/PlasmaFire.h"
#include "Games/Actors/RocketFire.h"

#include "Games/Actors/PushTrigger.h"
#include "Games/Actors/TeleporterTrigger.h"
#include "Games/Actors/LocationTarget.h"
#include "Games/Actors/SpeakerTarget.h"

#define TEXTURENAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-"


Vector3<float> SoundParams::GetPosition(bool* posExists) const
{
    if (posExists) 
		*posExists = false;
    switch (type) 
    {
        case SP_LOCAL:
        {
            return Vector3<float>::Zero();
        }
		break;

        case SP_POSITIONAL:
        {
            if (posExists) 
				*posExists = true;
            return position;
        }
		break;

        case SP_OBJECT: 
        {
            if (object == 0)
                return  Vector3<float>::Zero();
			std::shared_ptr<Actor> pGameActor(GameLogic::Get()->GetActor(object).lock());
            if (!pGameActor)
                return  Vector3<float>::Zero();
            if (posExists) 
				*posExists = true;
			std::shared_ptr<TransformComponent> pTransformComponent(
				pGameActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
				pTransformComponent->GetPosition();
        }
		break;
    }
    return Vector3<float>::Zero();
}

//
// QuakeLogicThread::Run
//
void* QuakeLogicThread::Run()
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


bool StringToEnum(const EnumString* spec, int& result, const std::string& str)
{
	const EnumString* esp = spec;
	while (esp->str)
	{
		if (!strcmp(str.c_str(), esp->str))
		{
			result = esp->num;
			return true;
		}
		esp++;
	}
	return false;
}

unsigned int AddHud(const std::shared_ptr<PlayerActor>& player)
{
	//Fixed offset in config file
	Vector2<float> offset{ 0, -101 };

	//Dirty trick to avoid collision with Minetest's status text (e.g. “Volume changed to 0%”)
	if (offset[1] >= -167 && offset[1] <= -156)
		offset[1] = -181;

	HudElement* form = new HudElement();
	form->type = HudElementType::HUD_ELEM_TEXT;
	form->position = { 0.5f, 1.f };
	form->offset = offset;
	form->align = { 0.f, 0.f };
	form->number = 0xFFFFFF;

	unsigned int id = player->AddHud(form);
	EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudAdd>(
		id, (uint8_t)form->type, form->position, form->name, form->scale, form->text,
		form->number, form->item, form->direction, form->align, form->offset,
		form->worldPosition, form->size, form->zIndex, form->text2));

	return id;
}

void ChangeHud(unsigned int id, HudElement* elem, const std::string& statstr)
{
	if (elem)
	{
		int statint;
		HudElementStat stat = HUD_STAT_NUMBER;
		stat = StringToEnum(ESHudElementStat, statint, statstr) ? (HudElementStat)statint : stat;

		void* value = nullptr;
		switch (stat)
		{
			case HUD_STAT_POS:
				value = &elem->position;
				break;
			case HUD_STAT_NAME:
				value = &elem->name;
				break;
			case HUD_STAT_SCALE:
				value = &elem->scale;
				break;
			case HUD_STAT_TEXT:
				value = &elem->text;
				break;
			case HUD_STAT_NUMBER:
				value = &elem->number;
				break;
			case HUD_STAT_ITEM:
				value = &elem->item;
				break;
			case HUD_STAT_DIR:
				value = &elem->direction;
				break;
			case HUD_STAT_ALIGN:
				value = &elem->align;
				break;
			case HUD_STAT_OFFSET:
				value = &elem->offset;
				break;
			case HUD_STAT_WORLD_POS:
				value = &elem->worldPosition;
				break;
			case HUD_STAT_SIZE:
				value = &elem->size;
				break;
			case HUD_STAT_Z_INDEX:
				value = &elem->zIndex;
				break;
			case HUD_STAT_TEXT2:
				value = &elem->text2;
				break;
		}

		EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudChange>(id, stat, value));
	}
}

StatBars::StatBars()
{
	//cache setting
	mEnableDamage = Settings::Get()->GetBool("enable_damage");

	mAmmo = new HudElement();
	mAmmo->type = HUD_ELEM_STATBAR;
	mAmmo->position = { 0.f, 1.f };
	mAmmo->text = "ammo.png";
	mAmmo->text2 = "ammo_gone.png";
	mAmmo->number = STAT_AMMO;
	mAmmo->item = STAT_AMMO;
	mAmmo->direction = 0;
	mAmmo->size = { CHAR_WIDTH, CHAR_HEIGHT };
	mAmmo->offset = { ICON_SIZE / 2, ICON_SIZE / 2 };

	mHealth = new HudElement();
	mHealth->type = HUD_ELEM_STATBAR;
	mHealth->position = { 0.2f, 1.f };
	mHealth->text = "heart.png";
	mHealth->text2 = "heart_gone.png";
	mHealth->number = STAT_HEALTH;
	mHealth->item = STAT_HEALTH;
	mHealth->direction = 0;
	mHealth->size = { CHAR_WIDTH, CHAR_HEIGHT };
	mHealth->offset = { ICON_SIZE / 2, ICON_SIZE / 2 };

	mArmor = new HudElement();
	mArmor->type = HUD_ELEM_STATBAR;
	mArmor->position = { 0.4f, 1.f };
	mArmor->text = "armor.png";
	mArmor->text2 = "armor_gone.png";
	mArmor->number = STAT_ARMOR;
	mArmor->item = STAT_ARMOR;
	mArmor->direction = 0;
	mArmor->size = { CHAR_WIDTH, CHAR_HEIGHT };
	mArmor->offset = { ICON_SIZE / 2, ICON_SIZE / 2 };

	mScore = new HudElement();
	mScore->type = HUD_ELEM_STATBAR;
	mScore->position = { 0.8f, 1.f };
	mScore->text = "0";
	mScore->text2 = "0";
	mScore->number = STAT_UNKNOWN;
	mScore->item = STAT_UNKNOWN;
	mScore->direction = 0;
	mScore->size = { CHAR_WIDTH, CHAR_WIDTH };
	mScore->offset = { ICON_SIZE / 2, ICON_SIZE / 2 };
}

StatBars:: ~StatBars()
{
	delete mHealth;
	delete mArmor;
}

void StatBars::Update(const std::shared_ptr<PlayerActor>& player)
{
	if (player->GetId() == INVALID_ACTOR_ID)
		return;

	if (std::find(mHudIds.begin(), mHudIds.end(), player->GetId()) == mHudIds.end())
	{
		mHudIds.push_back(player->GetId());
		//flags are not transmitted to visual on connect, we need to make sure
		//our current flags are transmitted by sending them actively
		unsigned int flags = 0;
		unsigned int mask = 0;

		const EnumString* esp = ESHudBuiltinElement;
		for (int i = 0; esp[i].str; i++)
		{
			flags |= esp[i].num;
			mask |= esp[i].num;
		}

		unsigned int setFlags = flags;
		setFlags &= ~(HUD_FLAG_AMMO_VISIBLE | HUD_FLAG_HEALTH_VISIBLE | HUD_FLAG_ARMOR_VISIBLE);

		EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudSetFlags>(mask, setFlags));

		player->GetState().hudFlags &= ~mask;
		player->GetState().hudFlags |= flags;

		EventHandler(player, "hud_changed");
	}

	if (player->GetState().hudFlags & HUD_FLAG_SCORE_VISIBLE)
	{
		if (mScoreIds.find(player->GetId()) == mScoreIds.end())
		{
			HudElement* elem = new HudElement();
			elem->type = mScore->type;
			elem->name = mScore->name;
			elem->scale = mScore->scale;
			elem->text = mScore->text;
			elem->number = STAT_SCORE;
			elem->item = mScore->item;
			elem->direction = mScore->direction;
			elem->size = { mScore->size[0], mScore->size[1] };
			elem->align = { mScore->align[0], mScore->align[1] };
			elem->offset = { mScore->offset[0], mScore->offset[1] };
			elem->position = { mScore->position[0], mScore->position[1] };
			elem->worldPosition = { mScore->worldPosition[0], mScore->worldPosition[1] };
			elem->zIndex = mScore->zIndex;
			elem->text2 = mScore->text2;

			mScoreIds[player->GetId()] = player->AddHud(elem);
			EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudAdd>(
				mScoreIds[player->GetId()], (uint8_t)elem->type, elem->position,
				elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction,
				elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
		}
		else
		{
			unsigned int id = mScoreIds[player->GetId()];
			HudElement* elem = player->GetHud(id);
			if (elem)
				ChangeHud(id, elem, "number");
		}
	}
	else if (mScoreIds.find(player->GetId()) != mScoreIds.end())
	{
		HudElement* removeHud = player->RemoveHud(mScoreIds[player->GetId()]);
		if (removeHud)
		{
			delete removeHud;

			EventManager::Get()->QueueEvent(
				std::make_shared<QuakeEventDataHudRemove>(mScoreIds[player->GetId()]));
		}
		mScoreIds.erase(player->GetId());
	}

	if (player->GetState().hudFlags & HUD_FLAG_AMMO_VISIBLE)
	{
		if (mAmmoIds.find(player->GetId()) == mAmmoIds.end())
		{
			HudElement* elem = new HudElement();
			elem->type = mAmmo->type;
			elem->name = mAmmo->name;
			elem->scale = mAmmo->scale;
			elem->text = mAmmo->text;
			elem->number = STAT_AMMO;
			elem->item = mAmmo->item;
			elem->direction = mAmmo->direction;
			elem->size = { mAmmo->size[0], mAmmo->size[1] };
			elem->align = { mAmmo->align[0], mAmmo->align[1] };
			elem->offset = { mAmmo->offset[0], mAmmo->offset[1] };
			elem->position = { mAmmo->position[0], mAmmo->position[1] };
			elem->worldPosition = { mAmmo->worldPosition[0], mAmmo->worldPosition[1] };
			elem->zIndex = mAmmo->zIndex;
			elem->text2 = mAmmo->text2;

			mAmmoIds[player->GetId()] = player->AddHud(elem);
			EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudAdd>(
				mAmmoIds[player->GetId()], (uint8_t)elem->type, elem->position,
				elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction,
				elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
		}
		else
		{
			unsigned int id = mAmmoIds[player->GetId()];
			HudElement* elem = player->GetHud(id);
			if (elem)
				ChangeHud(id, elem, "number");
		}
	}
	else if (mAmmoIds.find(player->GetId()) != mAmmoIds.end())
	{
		HudElement* removeHud = player->RemoveHud(mAmmoIds[player->GetId()]);
		if (removeHud)
		{
			delete removeHud;

			EventManager::Get()->QueueEvent(
				std::make_shared<QuakeEventDataHudRemove>(mAmmoIds[player->GetId()]));
		}
		mAmmoIds.erase(player->GetId());
	}

	if ((player->GetState().hudFlags & HUD_FLAG_HEALTH_VISIBLE) && mEnableDamage)
	{
		if (mHealthIds.find(player->GetId()) == mHealthIds.end())
		{
			HudElement* elem = new HudElement();
			elem->type = mHealth->type;
			elem->name = mHealth->name;
			elem->scale = mHealth->scale;
			elem->text = mHealth->text;
			elem->number = player->GetState().stats[STAT_HEALTH];
			elem->item = mHealth->item;
			elem->direction = mHealth->direction;
			elem->size = { mHealth->size[0], mHealth->size[1] };
			elem->align = { mHealth->align[0], mHealth->align[1] };
			elem->offset = { mHealth->offset[0], mHealth->offset[1] };
			elem->position = { mHealth->position[0], mHealth->position[1] };
			elem->worldPosition = { mHealth->worldPosition[0], mHealth->worldPosition[1] };
			elem->zIndex = mHealth->zIndex;
			elem->text2 = mHealth->text2;

			mHealthIds[player->GetId()] = player->AddHud(elem);
			EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudAdd>(
				mHealthIds[player->GetId()], (uint8_t)elem->type, elem->position,
				elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction,
				elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
		}
		else
		{
			unsigned int id = mHealthIds[player->GetId()];
			HudElement* elem = player->GetHud(id);
			if (elem)
				ChangeHud(id, elem, "number");
		}
	}
	else if (mHealthIds.find(player->GetId()) != mHealthIds.end())
	{
		HudElement* removeHud = player->RemoveHud(mHealthIds[player->GetId()]);
		if (removeHud)
		{
			delete removeHud;

			EventManager::Get()->QueueEvent(
				std::make_shared<QuakeEventDataHudRemove>(mHealthIds[player->GetId()]));
		}
		mHealthIds.erase(player->GetId());
	}

	bool showArmor =
		(player->GetState().hudFlags & HUD_FLAG_ARMOR_VISIBLE) && mEnableDamage;
	if (showArmor)
	{
		if (mArmorIds.find(player->GetId()) == mArmorIds.end())
		{
			HudElement* elem = new HudElement();
			elem->type = mArmor->type;
			elem->name = mArmor->name;
			elem->scale = mArmor->scale;
			elem->text = mArmor->text;
			elem->number = player->GetState().stats[STAT_ARMOR];
			elem->item = mArmor->item;
			elem->direction = mArmor->direction;
			elem->size = { mArmor->size[0], mArmor->size[1] };
			elem->align = { mArmor->align[0], mArmor->align[1] };
			elem->offset = { mArmor->offset[0], mArmor->offset[1] };
			elem->position = { mArmor->position[0], mArmor->position[1] };
			elem->worldPosition = { mArmor->worldPosition[0], mArmor->worldPosition[1] };
			elem->zIndex = mArmor->zIndex;
			elem->text2 = mArmor->text2;

			mArmorIds[player->GetId()] = player->AddHud(elem);
			EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudAdd>(
				mArmorIds[player->GetId()], (uint8_t)elem->type, elem->position,
				elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction,
				elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
		}
		else
		{
			unsigned int id = mArmorIds[player->GetId()];
			HudElement* elem = player->GetHud(id);
			if (elem)
				ChangeHud(id, elem, "number");
		}
	}
	else if (mArmorIds.find(player->GetId()) != mArmorIds.end())
	{
		//core.after(1, function(player_name, amor_bar), name, hud.id_amorbar)
		HudElement* removeHud = player->RemoveHud(mArmorIds[player->GetId()]);
		if (removeHud)
		{
			delete removeHud;

			EventManager::Get()->QueueEvent(
				std::make_shared<QuakeEventDataHudRemove>(mArmorIds[player->GetId()]));
		}
		mArmorIds.erase(player->GetId());
	}
}

void StatBars::Cleanup(const std::shared_ptr<PlayerActor>& player)
{
	if (player->GetId() == INVALID_ACTOR_ID)
		return;

	mScoreIds.erase(player->GetId());
	mHealthIds.erase(player->GetId());
	mArmorIds.erase(player->GetId());
	mAmmoIds.erase(player->GetId());
	mHudIds.erase(std::remove(mHudIds.begin(), mHudIds.end(), player->GetId()), mHudIds.end());
}

bool StatBars::EventHandler(const std::shared_ptr<PlayerActor>& player, std::string eventName)
{
	if (player->GetId() == INVALID_ACTOR_ID ||
		std::find(mHudIds.begin(), mHudIds.end(), player->GetId()) == mHudIds.end())
	{
		return false;
	}

	if (eventName == "health_changed")
	{
		Update(player);

		if (mHealthIds.find(player->GetId()) != mHealthIds.end())
			return true;
	}

	if (eventName == "armor_changed")
	{
		Update(player);

		if (mArmorIds.find(player->GetId()) != mArmorIds.end())
			return true;
	}

	if (eventName == "ammo_changed")
	{
		Update(player);

		if (mAmmoIds.find(player->GetId()) != mAmmoIds.end())
			return true;
	}

	if (eventName == "score_changed")
	{
		Update(player);

		if (mScoreIds.find(player->GetId()) != mScoreIds.end())
			return true;
	}

	if (eventName == "hud_changed" || eventName == "properties_changed")
	{
		Update(player);
		return true;
	}

	return false;
}

bool StatBars::ReplaceHud(HudElement* hud, std::string hudName)
{
	return false;
}

//
// QuakeLogic::QuakeLogic
//
QuakeLogic::QuakeLogic() : GameLogic(), mSimpleSingleplayerMode(true)
{
	Settings::Get()->Set("default_gravity", "(0,0,-300)");

	mStatBars = std::make_shared<StatBars>();

	mThread = new QuakeLogicThread(this);
	mChatBackend = new ChatBackend();

	mPhysics.reset(CreateGamePhysics());
	RegisterAllDelegates();

	mMetricsBackend = std::make_unique<MetricsBackend>();
	mUptimeCounter = mMetricsBackend->AddCounter("quake_core_server_uptime", "Logic uptime (in seconds)");
}


//
// QuakeLogic::~QuakeLogic
//
QuakeLogic::~QuakeLogic()
{
	RemoveAllDelegates();
	DestroyAllNetworkEventForwarders();

	mChatBackend->AddMessage(L"", L"# Disconnected.");
	mChatBackend->AddMessage(L"", L"");
	mChatLogBuf = std::queue<std::string>();

	delete mChatBackend;

	// Stop threads
	if (mThread)
	{
		Stop();
		delete mThread;
	}
}

void QuakeLogic::Start(tinyxml2::XMLElement* pLevelData)
{
	LogInformation("Loading game world thread ");

	// Stop thread if already running
	mThread->Stop();

	mThread->mLevelData = pLevelData;

	// Start thread
	mThread->Start();
}

void QuakeLogic::Stop()
{
	LogInformation("Stopping and waiting threads");

	// Stop threads (set run=false first so both start stopping)
	mThread->Stop();
	//m_emergethread.setRun(false);
	mThread->Wait();
	//m_emergethread.stop();

	LogInformation("Threads stopped");
}

void QuakeLogic::Step(float dTime)
{
	ScopeProfiler sp2(Profiling, "LogicEnv::step()", SPT_AVG);

	// Update this one
	// NOTE: This is kind of funny on a singleplayer game, but doesn't
	// really matter that much.
	static thread_local const float step =
		Settings::Get()->GetFloat("dedicated_server_step");
	mRecommendedSendInterval = step;

	/*
		Increment game time
	*/
	{
		mGameTimeFractionCounter += dTime;
		unsigned int incTime = (unsigned int)mGameTimeFractionCounter;
		mGameTime += incTime;
		mGameTimeFractionCounter -= (float)incTime;
	}
}


PlayerDatabase* QuakeLogic::OpenPlayerDatabase(const std::string& name, const std::string& savedir)
{
	PlayerDatabase* players = new PlayerDatabase();
	players->LoadPlayers(savedir + "/players.bin");
	return players;
}


AuthDatabase* QuakeLogic::OpenAuthDatabase(const std::string& name, const std::string& savedir)
{
	return new AuthDatabase();
}

// Logic Update
void QuakeLogic::OnUpdate(float time, float deltaMs)
{
	GameLogic::OnUpdate(time, deltaMs);

	// Limit a bit
	if (deltaMs > 2000.f)
		deltaMs = 2000.f;

	{
		MutexAutoLock lock1(mStepDeltaMsMutex);
		mStepDeltaMs += deltaMs;
	}

	//Get chat messages from visual
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
	UpdateChat(deltaMs, screenSize);
}

// Step
void QuakeLogic::AsyncStep()
{
	bool runStep = true;
	{
		MutexAutoLock lock1(mStepDeltaMsMutex);
		if (mStepDeltaMs < 1)
			runStep = false;
	}

	if (!runStep) return;

	float dTime;
	{
		MutexAutoLock lock1(mStepDeltaMsMutex);
		dTime = mStepDeltaMs / 1000.f;
	}

	ScopeProfiler sp(Profiling, "QuakeLogic::AsyncRunStep()", SPT_AVG);
	{
		MutexAutoLock lock1(mStepDeltaMsMutex);
		mStepDeltaMs = 0.f;
	}

	// Update uptime
	mUptimeCounter->Increment(dTime);

	{
		MutexAutoLock lock(mEnvMutex);
		// Figure out and report maximum lag to environment
		float maxLag = GetMaxLagEstimate();
		maxLag *= 0.9998f; // Decrease slowly (about half per 5 minutes)
		if (dTime > maxLag)
		{
			if (dTime > 0.1f && dTime > maxLag * 2.f)
				LogInformation("Maximum lag peaked to " + std::to_string(dTime) + " s");
			maxLag = dTime;
		}
		ReportMaxLagEstimate(maxLag);

		// Write logic map
		if (mLocalDB && mLocalDBSaveInterval.Step(dTime, mCacheSaveInterval))
		{
			mLocalDB->EndSave();
			mLocalDB->BeginSave();
		}
	}

	/*
		Send pending messages on out chat queue
	*/
	if (!mOutChatQueue.empty() && CanSendChatMessage())
	{
		SendChatMessage(mOutChatQueue.front());
		mOutChatQueue.pop();
	}
}

void QuakeLogic::NotifyPlayers(const std::wstring& msg)
{
	SendChatMessage(msg);
}

bool QuakeLogic::CanSendChatMessage() const
{
	unsigned int now = (unsigned int)time(NULL);
	float timePassed = (float)(now - mLastChatMessageSent);

	float virtChatMessageAllowance =
		mChatMessageAllowance + timePassed * (CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
	if (virtChatMessageAllowance < 1.0f)
		return false;

	return true;
}

void QuakeLogic::SendChatMessage(const std::wstring& message)
{
	const short maxQueueSize = Settings::Get()->GetInt16("max_out_chat_queue_size");
	if (CanSendChatMessage())
	{
		unsigned int now = (unsigned int)time(NULL);
		float timePassed = (float)(now - mLastChatMessageSent);
		mLastChatMessageSent = now;

		mChatMessageAllowance += timePassed * (CHAT_MESSAGE_LIMIT_PER_10S / 8.0f);
		if (mChatMessageAllowance > CHAT_MESSAGE_LIMIT_PER_10S)
			mChatMessageAllowance = CHAT_MESSAGE_LIMIT_PER_10S;

		mChatMessageAllowance -= 1.0f;

		std::wstring answerToSender = HandleChat("Player", message, true);
		if (!answerToSender.empty())
		{
			// Send the answer to sender
			HandleChatMessage(ChatMessage(CHATMESSAGE_TYPE_SYSTEM, answerToSender));
		}
	}
	else if (mOutChatQueue.size() < maxQueueSize || maxQueueSize == -1)
	{
		mOutChatQueue.push(message);
	}
	else
	{
		LogInformation("Could not queue chat message because maximum out chat queue size ("
			+ std::to_string(maxQueueSize) + ") is reached.");
	}
}

void QuakeLogic::HandleChatMessage(const ChatMessage& chat)
{
	ChatMessage* chatMessage = new ChatMessage();
	unsigned char version = 1;

	if (version != 1 || chat.type >= CHATMESSAGE_TYPE_MAX)
	{
		delete chatMessage;
		return;
	}

	chatMessage->message = chat.message;
	chatMessage->timestamp = static_cast<std::time_t>(chat.timestamp);
	chatMessage->type = (ChatMessageType)chat.type;

	PushToChatQueue(chatMessage);
}


// Chat message formatter

// Implemented to allow redefinition
std::wstring QuakeLogic::FormatChatMessage(const std::string& name, const std::string& message)
{
	std::string errorStr = "Invalid chat message format - missing %s";
	std::string str = Settings::Get()->Get("chat_message_format");

	char timeBuf[30];
	Timer::RealTimeDate time = Timer::GetRealTimeAndDate();
	snprintf(timeBuf, sizeof(timeBuf), "%uH:%uM:%uS", time.Hour, time.Minute, time.Second);;

	StringReplace(str, "@name", "<" + name + ">");
	StringReplace(str, "@timestamp", timeBuf);
	StringReplace(str, "@message", message);

	return ToWideString(str);
}

// Chat command handler
bool QuakeLogic::OnChatMessage(const std::string& name, const std::string& message)
{
	//core.chatcommands = core.registered_chatcommands // BACKWARDS COMPATIBILITY
	float msgTimeThreshold = 0.1f;
	if (Settings::Get()->Exists("chatcommand_msg_time_threshold"))
	{
		msgTimeThreshold =
			Settings::Get()->GetFloat("chatcommand_msg_time_threshold");
	}

	if (message.front() != '/')
		return false;

	return true;  // Handled chat message
}

bool QuakeLogic::GetChatMessage(std::wstring& res)
{
	if (mChatQueue.empty())
		return false;

	ChatMessage* chatMessage = mChatQueue.front();
	mChatQueue.pop();

	res = L"";

	switch (chatMessage->type)
	{
		case CHATMESSAGE_TYPE_RAW:
		case CHATMESSAGE_TYPE_ANNOUNCE:
		case CHATMESSAGE_TYPE_SYSTEM:
			res = chatMessage->message;
			break;
		case CHATMESSAGE_TYPE_NORMAL:
		{
			if (!chatMessage->sender.empty())
				res = L"<" + chatMessage->sender + L"> " + chatMessage->message;
			else
				res = chatMessage->message;
			break;
		}
		default:
			break;
	}

	delete chatMessage;
	return true;
}

void QuakeLogic::UpdateChat(float dTime, const Vector2<unsigned int>& screensize)
{
	// Get new messages from error log buffer
	while (!mChatLogBuf.empty())
	{
		mChatBackend->AddMessage(L"", ToWideString(mChatLogBuf.front()));
		mChatLogBuf.pop();
	}

	// Get new messages from visual
	std::wstring message;
	while (GetChatMessage(message))
		mChatBackend->AddUnparsedMessage(message);

	// Remove old messages
	mChatBackend->Step(dTime);

	EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataUpdateChat>(
		mChatBackend->GetRecentBuffer().GetLineCount(), mChatBackend->GetRecentChat()));
}

std::wstring QuakeLogic::HandleChat(const std::string& name, std::wstring messageInput, bool checkShoutPriv)
{
	// If something goes wrong, this player is to blame
	//RollbackScopeActor rollback_scope(m_rollback, std::string("player:") + name);

	if (Settings::Get()->GetBool("strip_color_codes"))
		messageInput = UnescapeEnriched(messageInput);

	if (mMaxChatMessageLength > 0 && messageInput.length() > mMaxChatMessageLength)
	{
		return L"Your message exceed the maximum chat message limit set on the logic. "
			L"It was refused. Send a shorter message";
	}

	auto message = Trim(ToString(messageInput));
	if (message.find_first_of("\n\r") != std::wstring::npos)
		return L"Newlines are not permitted in chat messages";

	// Run script hook, exit if script ate the chat message
	if (OnChatMessage(name, message))
		return L"";

	// Line to send
	std::wstring line;
	// Whether to send line to the player that sent the message, or to all players
	bool broadcastLine = true;

	if (!checkShoutPriv)
	{
		line += L"-!- You don't have permission to shout.";
		broadcastLine = false;
	}
	else
	{
		line += FormatChatMessage(name, ToString(messageInput));
	}

	/*
		Tell calling method to send the message to sender
	*/
	if (!broadcastLine)
		return line;

	/*
		Send the message to others
	*/
	LogInformation("CHAT: " + ToString(UnescapeEnriched(line)));

	ChatMessage chatmsg(line);
	HandleChatMessage(chatmsg);

	return L"";
}

bool QuakeLogic::HudSetFlags(const std::shared_ptr<PlayerActor>& player, unsigned int flags, unsigned int mask)
{
	if (!player)
		return false;

	unsigned int setFlags = flags;
	setFlags &= ~(HUD_FLAG_HEALTH_VISIBLE | HUD_FLAG_ARMOR_VISIBLE | HUD_FLAG_AMMO_VISIBLE);

	EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataHudSetFlags>(mask, setFlags));

	player->GetState().hudFlags &= ~mask;
	player->GetState().hudFlags |= flags;

	mStatBars->EventHandler(player, "hud_changed");
	return true;
}


void QuakeLogic::UpdateViewType(const std::shared_ptr<BaseGameView>& pView, bool add)
{
	GameLogic::UpdateViewType(pView, add);

	//  This is commented out because while the view is created and waiting, the player is NOT attached yet. 
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

void QuakeLogic::ResetViewType()
{
	GameLogic::ResetViewType();

	mHumanPlayersAttached = 0;
	mAIPlayersAttached = 0;
}

void QuakeLogic::SetProxy()
{
	GameLogic::SetProxy();
}

//
// QuakeLogic::ChangeState
//
void QuakeLogic::ChangeState(BaseGameState newState)
{
	GameLogic::ChangeState(newState);

	switch (newState)
	{
		case BGS_MAINMENU:
		{
			std::shared_ptr<BaseGameView> menuView(new QuakeMainMenuView());
			GameApplication::Get()->AddView(menuView);

			break;
		}

		case BGS_WAITINGFORPLAYERS:
		{
			// spawn all local players (should only be one, though we might support more in the future)
			LogAssert(mExpectedPlayers == 1, "needs only one player");
			for (int i = 0; i < mExpectedPlayers; ++i)
			{
				std::shared_ptr<BaseGameView> playersView(new QuakeHumanView());
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
				std::shared_ptr<BaseGameView> aiView(new QuakeAIView());
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
					std::shared_ptr<PlayerActor> pPlayerActor = 
						CreatePlayerActor("actors\\quake\\players\\player.xml", NULL);
					if (pPlayerActor)
					{
						pPlayerActor->PlayerSpawn();
						pView->OnAttach(pView->GetId(), pPlayerActor->GetId());

						std::shared_ptr<EventDataNewActor> pNewActorEvent(
							new EventDataNewActor(pPlayerActor->GetId(), pView->GetId()));
						BaseEventManager::Get()->TriggerEvent(pNewActorEvent);
					}
				}
				else if (pView->GetType() == GV_REMOTE)
				{
					std::shared_ptr<NetworkGameView> pNetworkGameView =
						std::static_pointer_cast<NetworkGameView, BaseGameView>(pView);
					std::shared_ptr<PlayerActor> pPlayerActor =
						CreatePlayerActor("actors\\quake\\players\\remote_player.xml", NULL);
					if (pPlayerActor)
					{
						pPlayerActor->PlayerSpawn();
						pView->OnAttach(pView->GetId(), pPlayerActor->GetId());

						std::shared_ptr<EventDataNewActor> pNewActorEvent(
							new EventDataNewActor(pPlayerActor->GetId(), pNetworkGameView->GetId()));
						BaseEventManager::Get()->TriggerEvent(pNewActorEvent);
					}
				}
				else if (pView->GetType() == GV_AI)
				{
					std::shared_ptr<QuakeAIView> pAiView = 
						std::static_pointer_cast<QuakeAIView, BaseGameView>(pView);
					std::shared_ptr<PlayerActor> pPlayerActor =
						CreatePlayerActor("actors\\quake\\players\\ai_player.xml", NULL);
					if (pPlayerActor)
					{
						pPlayerActor->PlayerSpawn();
						pAiView->OnAttach(pView->GetId(), pPlayerActor->GetId());

						std::shared_ptr<EventDataNewActor> pNewActorEvent(
							new EventDataNewActor(pPlayerActor->GetId(), pAiView->GetId()));
						BaseEventManager::Get()->TriggerEvent(pNewActorEvent);
					}
				}
			}
			break;
		}
	}
}

void QuakeLogic::SyncActor(const ActorId id, Transform const &transform)
{
	GameLogic::SyncActor(id, transform);
}

void QuakeLogic::GameInitDelegate(BaseEventDataPtr pEventData)
{
	mGameInit = true;
}

void QuakeLogic::RequestStartGameDelegate(BaseEventDataPtr pEventData)
{
	ChangeState(BGS_WAITINGFORPLAYERS);
}

void QuakeLogic::EnvironmentLoadedDelegate(BaseEventDataPtr pEventData)
{
	++mHumanGamesLoaded;
}

// FUTURE WORK - this isn't Quake specific so it can go into the game agnostic base class
void QuakeLogic::RemoteClientDelegate(BaseEventDataPtr pEventData)
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

void QuakeLogic::NetworkPlayerActorAssignmentDelegate(BaseEventDataPtr pEventData)
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
			std::shared_ptr<QuakeHumanView> pHumanView =
				std::static_pointer_cast<QuakeHumanView, BaseGameView>(pView);
			if (mRemotePlayerId == pCastEventData->GetSocketId())
				pHumanView->SetControlledActor(pCastEventData->GetActorId());

			return;
		}
	}

	LogError("Could not find HumanView to attach actor to!");
}

void QuakeLogic::PushActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataPushActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataPushActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->ApplyForce(pCastEventData->GetDirection());
	}
}

void QuakeLogic::JumpActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataJumpActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataJumpActor>(pEventData);

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock()));
	if (pPlayerActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->KinematicJump(pCastEventData->GetDirection());

		if (pPlayerActor->GetAction().triggerPush != INVALID_ACTOR_ID)
		{
			pPlayerActor->GetAction().triggerPush = INVALID_ACTOR_ID;

			std::shared_ptr<TransformComponent> pPlayerTransform(
				pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

			std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
			Transform cameraTransform = camera->GetAbsoluteTransform();

			SoundParams params;
			params.type = SoundParams::SP_POSITIONAL;
			params.position = cameraTransform.GetTranslation();
			SimpleSound sound;
			sound.name = "jumppad"; //art/quake/audio/sound/world/jumppad.wav
			PlaySound(sound, params, true);
		}
		else
		{
			if (pPlayerActor->GetState().jumpTime == 0)
			{
				pPlayerActor->GetState().jumpTime = 200;

				std::shared_ptr<TransformComponent> pPlayerTransform(
					pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

				std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
				Transform cameraTransform = camera->GetAbsoluteTransform();

				SoundParams params;
				params.type = SoundParams::SP_POSITIONAL;
				params.position = cameraTransform.GetTranslation();
				SimpleSound sound;
				sound.name = "jump1"; //art/quake/audio/sound/player/jump1.wav
				PlaySound(sound, params, true);
			}
		}
	}
}

void QuakeLogic::TeleportActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataTeleportActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataTeleportActor>(pEventData);

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock()));
	if (pPlayerActor)
	{
		std::shared_ptr<Actor> pItemActor(
			std::dynamic_pointer_cast<Actor>(
			GameLogic::Get()->GetActor(pPlayerActor->GetAction().triggerTeleporter).lock()));
		std::shared_ptr<TeleporterTrigger> pTeleporterTrigger =
			pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock();
		pPlayerActor->GetAction().triggerTeleporter = INVALID_ACTOR_ID;

		std::shared_ptr<TransformComponent> pTransformComponent =
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
		if (pTransformComponent)
			pTransformComponent->SetTransform(pTeleporterTrigger->GetTarget());

		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->SetTransform(pTeleporterTrigger->GetTarget());

		if (GameApplication::Get()->GetHumanView()->mCamera->GetTarget() &&
			GameApplication::Get()->GetHumanView()->mCamera->GetTarget()->GetId() == pPlayerActor->GetId())
		{
			SoundParams params;
			params.type = SoundParams::SP_POSITIONAL;
			params.toPlayer = pPlayerActor->GetId();
			SimpleSound sound;
			sound.name = "teleout"; //art/quake/audio/sound/world/teleout.ogg
			PlaySound(sound, params, true);
		}
		else
		{
			std::shared_ptr<TransformComponent> pPlayerTransform(
				pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

			std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
			Transform cameraTransform = camera->GetAbsoluteTransform();

			SoundParams params;
			params.type = SoundParams::SP_POSITIONAL;
			params.position = cameraTransform.GetTranslation();
			SimpleSound sound;
			sound.name = "teleout"; //art/quake/audio/sound/world/teleout.ogg
			PlaySound(sound, params, true);
		}
	}
}

void QuakeLogic::SpawnActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataSpawnActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataSpawnActor>(pEventData);

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock()));
	if (pPlayerActor)
	{
		// find a spawn point
		Transform spawnTransform;
		std::shared_ptr<TransformComponent> pTransformComponent(
			pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
		{
			SelectSpawnPoint(pTransformComponent->GetTransform().GetTranslation(), spawnTransform);
			pTransformComponent->SetTransform(spawnTransform);
		}

		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->SetTransform(spawnTransform);

		if (GameApplication::Get()->GetHumanView()->mCamera->GetTarget() &&
			GameApplication::Get()->GetHumanView()->mCamera->GetTarget()->GetId() == pPlayerActor->GetId())
		{
			SoundParams params;
			params.type = SoundParams::SP_POSITIONAL;
			params.toPlayer = pPlayerActor->GetId();
			SimpleSound sound;
			sound.name = "teleout"; //art/quake/audio/sound/world/teleout.ogg
			PlaySound(sound, params, true);
		}
		else
		{
			std::shared_ptr<TransformComponent> pPlayerTransform(
				pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

			std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
			Transform cameraTransform = camera->GetAbsoluteTransform();

			SoundParams params;
			params.type = SoundParams::SP_POSITIONAL;
			params.position = cameraTransform.GetTranslation();
			SimpleSound sound;
			sound.name = "teleout"; //art/quake/audio/sound/world/teleout.ogg
			PlaySound(sound, params, true);
		}
	}
}

void QuakeLogic::MoveActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataMoveActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataMoveActor>(pEventData);

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock()));
	if (pPlayerActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
		{
			pPhysicalComponent->KinematicMove(pCastEventData->GetDirection());

			if (pPhysicalComponent->OnGround() && 
				pPlayerActor->GetState().moveTime == 0 &&
				Length(pCastEventData->GetDirection()) > 0.f)
			{
				pPlayerActor->GetState().moveTime = 400;

				std::shared_ptr<TransformComponent> pPlayerTransform(
					pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());

				std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
				Transform cameraTransform = camera->GetAbsoluteTransform();

				SoundParams params;
				params.type = SoundParams::SP_POSITIONAL;
				params.position = cameraTransform.GetTranslation();
				SimpleSound sound;
				sound.name = "boot1"; //art/quake/audio/sound/player/footsteps/boot1.ogg
				PlaySound(sound, params, true);
			}
		}
	}
}

void QuakeLogic::FallActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataFallActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataFallActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->KinematicFall(pCastEventData->GetDirection());
	}
}

void QuakeLogic::RotateActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataRotateActor> pCastEventData =
		std::static_pointer_cast<QuakeEventDataRotateActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
			pPhysicalComponent->SetRotation(pCastEventData->GetTransform());
	}
}


void QuakeLogic::PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerEnter> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerEnter>(pEventData);

	std::shared_ptr<Actor> pItemActor(
		GameLogic::Get()->GetActor(pCastEventData->GetTriggerId()).lock());

	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(pCastEventData->GetOtherActor()).lock()));

	if (pPlayerActor)
	{
		// dead players
		if (pPlayerActor->GetState().stats[STAT_HEALTH] <= 0)
			return;

		if (pItemActor->GetType() == "Trigger")
		{
			if (pItemActor->GetComponent<PushTrigger>(PushTrigger::Name).lock())
			{
				pPlayerActor->GetAction().triggerPush = pItemActor->GetId();
			}
			else if (pItemActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock())
			{
				pPlayerActor->GetAction().triggerTeleporter = pItemActor->GetId();
			}
		}

		if (!CanItemBeGrabbed(pItemActor, pPlayerActor))
			return; // can't hold it

		if (pItemActor->GetType() == "Weapon")
		{
			std::shared_ptr<WeaponPickup> pWeaponPickup =
				pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
			if (pWeaponPickup->mRespawnTime)
				return;

			pWeaponPickup->mRespawnTime = (float)PickupWeapon(pPlayerActor, pWeaponPickup);
		}
		else if (pItemActor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> pAmmoPickup =
				pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
			if (pAmmoPickup->mRespawnTime)
				return;

			pAmmoPickup->mRespawnTime = (float)PickupAmmo(pPlayerActor, pAmmoPickup);
		}
		else if (pItemActor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> pArmorPickup =
				pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
			if (pArmorPickup->mRespawnTime)
				return;

			pArmorPickup->mRespawnTime = (float)PickupArmor(pPlayerActor, pArmorPickup);
		}
		else if (pItemActor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> pHealthPickup =
				pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
			if (pHealthPickup->mRespawnTime)
				return;

			pHealthPickup->mRespawnTime = (float)PickupHealth(pPlayerActor, pHealthPickup);
		}
	}
}


void QuakeLogic::PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysTriggerLeave> pCastEventData =
		std::static_pointer_cast<EventDataPhysTriggerLeave>(pEventData);

	std::shared_ptr<Actor> pTrigger(
		GameLogic::Get()->GetActor(pCastEventData->GetTriggerId()).lock());
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(pCastEventData->GetOtherActor()).lock()));
}


void QuakeLogic::PhysicsCollisionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysCollision> pCastEventData =
		std::static_pointer_cast<EventDataPhysCollision>(pEventData);

	std::shared_ptr<Actor> pGameActorA(
		GameLogic::Get()->GetActor(pCastEventData->GetActorA()).lock());
	std::shared_ptr<Actor> pGameActorB(
		GameLogic::Get()->GetActor(pCastEventData->GetActorB()).lock());
	if (pGameActorA && pGameActorB)
	{
		std::shared_ptr<Actor> pItemActor;
		std::shared_ptr<PlayerActor> pPlayerActor;
		if (std::dynamic_pointer_cast<PlayerActor>(pGameActorA) &&
			!std::dynamic_pointer_cast<PlayerActor>(pGameActorB))
		{
			pPlayerActor = std::dynamic_pointer_cast<PlayerActor>(pGameActorA);
			pItemActor = pGameActorB;

		}
		else if (!std::dynamic_pointer_cast<PlayerActor>(pGameActorA) &&
			std::dynamic_pointer_cast<PlayerActor>(pGameActorB))
		{
			pPlayerActor = std::dynamic_pointer_cast<PlayerActor>(pGameActorB);
			pItemActor = pGameActorA;
		}
		else return;

		// dead players
		if (pPlayerActor->GetState().stats[STAT_HEALTH] <= 0)
			return;

		if (pItemActor->GetType() == "Fire")
		{
			if (pItemActor->GetComponent<RocketFire>(RocketFire::Name).lock())
			{
				std::shared_ptr<RocketFire> pRocketFire =
					pItemActor->GetComponent<RocketFire>(RocketFire::Name).lock();
				pRocketFire->mExplosionTime = 1.f;
			}
			else if (pItemActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock())
			{
				std::shared_ptr<PlasmaFire> pPlasmaFire =
					pItemActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock();
				pPlasmaFire->mExplosionTime = 1.f;
			}
		}
	}
	else if (pGameActorA || pGameActorB)
	{
		std::shared_ptr<Actor> pItemActor;
		if (pGameActorA)
			pItemActor = std::dynamic_pointer_cast<Actor>(pGameActorA);
		else
			pItemActor = std::dynamic_pointer_cast<Actor>(pGameActorB);
		if (pItemActor->GetType() == "Fire")
		{
			if (pItemActor->GetComponent<RocketFire>(RocketFire::Name).lock())
			{
				std::shared_ptr<RocketFire> pRocketFire =
					pItemActor->GetComponent<RocketFire>(RocketFire::Name).lock();
				pRocketFire->mExplosionTime = 1.f;
			}
			else if (pItemActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock())
			{
				std::shared_ptr<PlasmaFire> pPlasmaFire =
					pItemActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock();
				pPlasmaFire->mExplosionTime = 1.f;
			}
		}
	}
}

void QuakeLogic::PhysicsSeparationDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPhysSeparation> pCastEventData =
		std::static_pointer_cast<EventDataPhysSeparation>(pEventData);

	std::shared_ptr<Actor> pGameActorA(
		GameLogic::Get()->GetActor(pCastEventData->GetActorA()).lock());
	std::shared_ptr<Actor> pGameActorB(
		GameLogic::Get()->GetActor(pCastEventData->GetActorB()).lock());
	if (pGameActorA && pGameActorB)
	{
		std::shared_ptr<Actor> pItemActor;
		std::shared_ptr<PlayerActor> pPlayerActor;
		if (std::dynamic_pointer_cast<PlayerActor>(pGameActorA) &&
			!std::dynamic_pointer_cast<PlayerActor>(pGameActorB))
		{
			pPlayerActor = std::dynamic_pointer_cast<PlayerActor>(pGameActorA);
			pItemActor = pGameActorB;

		}
		else if (!std::dynamic_pointer_cast<PlayerActor>(pGameActorA) &&
			std::dynamic_pointer_cast<PlayerActor>(pGameActorB))
		{
			pPlayerActor = std::dynamic_pointer_cast<PlayerActor>(pGameActorB);
			pItemActor = pGameActorA;
		}
		else return;
	}
}

void QuakeLogic::HandleChatMessageDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChatMessage> pCastEventData =
		std::static_pointer_cast<EventDataChatMessage>(pEventData);

	// Discard empty line
	if (pCastEventData->GetResource().empty())
		return;

	SendChatMessage(pCastEventData->GetResource());
}

void QuakeLogic::HandleNotifyPlayerDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataNotifyActor> pCastEventData =
		std::static_pointer_cast<EventDataNotifyActor>(pEventData);

	NotifyPlayers(pCastEventData->GetNote());
}

void QuakeLogic::HandleRemoveSoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataRemoveSounds> pCastEventData =
		std::static_pointer_cast<QuakeEventDataRemoveSounds>(pEventData);

	RemoveSounds(pCastEventData->GetIds());
}

void QuakeLogic::RegisterAllDelegates(void)
{
	// FUTURE WORK: Lots of these functions are ok to go into the base game logic!
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::RemoteClientDelegate), 
		EventDataRemoteClient::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::SyncActorDelegate),
		EventDataSyncActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::RequestStartGameDelegate), 
		EventDataRequestStartGame::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::NetworkPlayerActorAssignmentDelegate), 
		EventDataNetworkPlayerActorAssignment::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::EnvironmentLoadedDelegate), 
		EventDataEnvironmentLoaded::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::EnvironmentLoadedDelegate), 
		EventDataRemoteEnvironmentLoaded::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::PhysicsTriggerEnterDelegate),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::PhysicsTriggerLeaveDelegate),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::PhysicsCollisionDelegate),
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::PhysicsSeparationDelegate),
		EventDataPhysSeparation::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::HandleChatMessageDelegate),
		EventDataChatMessage::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::HandleNotifyPlayerDelegate),
		EventDataNotifyActor::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::GameInitDelegate),
		QuakeEventDataGameInit::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::HandleRemoveSoundDelegate),
		QuakeEventDataRemoveSounds::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::FireWeaponDelegate),
		QuakeEventDataFireWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::TeleportActorDelegate),
		QuakeEventDataTeleportActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::SpawnActorDelegate),
		QuakeEventDataSpawnActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::SplashDamageDelegate),
		QuakeEventDataSplashDamage::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::PushActorDelegate),
		QuakeEventDataPushActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::JumpActorDelegate),
		QuakeEventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::MoveActorDelegate),
		QuakeEventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::FallActorDelegate),
		QuakeEventDataFallActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeLogic::RotateActorDelegate),
		QuakeEventDataRotateActor::skEventType);
}

void QuakeLogic::RemoveAllDelegates(void)
{
	// FUTURE WORK: See the note in RegisterDelegates above....
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::RemoteClientDelegate), 
		EventDataRemoteClient::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::SyncActorDelegate),
		EventDataSyncActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::RequestStartGameDelegate), 
		EventDataRequestStartGame::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::NetworkPlayerActorAssignmentDelegate), 
		EventDataNetworkPlayerActorAssignment::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::EnvironmentLoadedDelegate), 
		EventDataEnvironmentLoaded::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::EnvironmentLoadedDelegate), 
		EventDataRemoteEnvironmentLoaded::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::PhysicsTriggerEnterDelegate),
		EventDataPhysTriggerEnter::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::PhysicsTriggerLeaveDelegate),
		EventDataPhysTriggerLeave::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::PhysicsCollisionDelegate),
		EventDataPhysCollision::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::PhysicsSeparationDelegate),
		EventDataPhysSeparation::skEventType);
	if (mIsProxy)
	{
		pGlobalEventManager->RemoveListener(
			MakeDelegate(this, &QuakeLogic::RequestNewActorDelegate), 
			EventDataRequestNewActor::skEventType);
	}

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::HandleChatMessageDelegate),
		EventDataChatMessage::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::HandleNotifyPlayerDelegate),
		EventDataNotifyActor::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::GameInitDelegate),
		QuakeEventDataGameInit::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::HandleRemoveSoundDelegate),
		QuakeEventDataRemoveSounds::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::FireWeaponDelegate),
		QuakeEventDataFireWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::TeleportActorDelegate),
		QuakeEventDataTeleportActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::SpawnActorDelegate),
		QuakeEventDataSpawnActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::SplashDamageDelegate),
		QuakeEventDataSplashDamage::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::PushActorDelegate),
		QuakeEventDataPushActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::JumpActorDelegate),
		QuakeEventDataJumpActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::MoveActorDelegate),
		QuakeEventDataMoveActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::FallActorDelegate),
		QuakeEventDataFallActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeLogic::RotateActorDelegate),
		QuakeEventDataRotateActor::skEventType);
}

void QuakeLogic::CreateNetworkEventForwarder(const int socketId)
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
		EventDataRequestNewActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
		EventDataNetworkPlayerActorAssignment::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataChatMessage::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		EventDataNotifyActor::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataGameInit::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataGameReady::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataRemoveSounds::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataFireWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataChangeWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataDeadActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataSplashDamage::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataTeleportActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataSpawnActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataPushActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataFallActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(pNetworkEventForwarder, &NetworkEventForwarder::ForwardEvent),
		QuakeEventDataRotateActor::skEventType);

	mNetworkEventForwarders.push_back(pNetworkEventForwarder);
}

void QuakeLogic::DestroyAllNetworkEventForwarders(void)
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
			EventDataRequestNewActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent), 
			EventDataNetworkPlayerActorAssignment::skEventType);


		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataChatMessage::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			EventDataNotifyActor::skEventType);

		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataGameInit::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataGameReady::skEventType);

		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataRemoveSounds::skEventType);

		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataFireWeapon::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataChangeWeapon::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataDeadActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataSplashDamage::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataTeleportActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataSpawnActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataPushActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataJumpActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataMoveActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataFallActor::skEventType);
		eventManager->RemoveListener(
			MakeDelegate(networkEventForwarder, &NetworkEventForwarder::ForwardEvent),
			QuakeEventDataRotateActor::skEventType);

		delete networkEventForwarder;
	}

	mNetworkEventForwarders.clear();
}

ActorFactory* QuakeLogic::CreateActorFactory(void)
{
	return new QuakeActorFactory();
}

LevelManager* QuakeLogic::CreateLevelManager(void)
{
	QuakeLevelManager* levelManager = new QuakeLevelManager();
	levelManager->AddLevelSearchDir(L"world/quake/");
	levelManager->LoadLevelList(L"*.xml");

	for (auto levelId : levelManager->GetAllLevelIds())
		Settings::Get()->Set("default_game", ToString(levelId.c_str()));
	return levelManager;
}

AIManager* QuakeLogic::CreateAIManager(void)
{
	QuakeAIManager* aiManager = new QuakeAIManager();
	return aiManager;
}

std::shared_ptr<PlayerActor> QuakeLogic::CreatePlayerActor(const std::string &actorResource,
	tinyxml2::XMLElement *overrides, const Transform *initialTransform, const ActorId serversActorId)
{
	QuakeActorFactory* actorFactory = dynamic_cast<QuakeActorFactory*>(mActorFactory);
	LogAssert(actorFactory, "quake actor factory is not initialized");
	if (!mIsProxy && serversActorId != INVALID_ACTOR_ID)
		return std::shared_ptr<PlayerActor>();

	if (mIsProxy && serversActorId == INVALID_ACTOR_ID)
		return std::shared_ptr<PlayerActor>();

	std::shared_ptr<PlayerActor> pActor = actorFactory->CreatePlayerActor(
		ToWideString(actorResource).c_str(), overrides, initialTransform, serversActorId);
	if (pActor)
	{
		mActors.insert(std::make_pair(pActor->GetId(), pActor));
		if (!mIsProxy && (mGameState == BGS_SPAWNINGPLAYERACTORS || mGameState == BGS_RUNNING))
		{
			std::shared_ptr<EventDataRequestNewActor> pNewActor(
				new EventDataRequestNewActor(actorResource, initialTransform, pActor->GetId()));
			BaseEventManager::Get()->TriggerEvent(pNewActor);

			mHuds[pActor->GetId()] = AddHud(pActor);
		}
		return pActor;
	}
	else
	{
		// FUTURE WORK: Log error: couldn't create actor
		return std::shared_ptr<PlayerActor>();
	}
}

bool QuakeLogic::GetGameInit()
{
	while (System::Get()->OnRun())
	{
		// End condition
		if (mGameInit)
			break;
	}

	return true;
}


bool QuakeLogic::AddMediaFile(const std::wstring& fileName, const std::wstring& filePath,
	const std::wstring& fileRelativePath, std::string* fileData, std::string* digestTo)
{
	// If name contains illegal characters, ignore the file
	if (!StringAllowed(ToString(fileName), TEXTURENAME_ALLOWED_CHARS))
	{
		LogWarning(L"Ignoring illegal file name: \"" + fileName + L"\"");
		return false;
	}
	// If name is not in a supported format, ignore it
	const char* supportedExt[] = {
		".png", ".jpg", ".bmp", ".tga", ".pcx",
		".ppm", ".psd", ".wal", ".rgb", ".ogg", 
		".wav", ".bsp", ".pk3", ".md3", ".skin", NULL};
	if (StringRemoveEnd(ToString(fileName), supportedExt).empty())
	{
		LogWarning(L"Ignoring unsupported file extension: \"" + fileName + L"\"");
		return false;
	}
	// Ok, attempt to load the file and add to cache

	// Read data
	BaseReadFile* file = FileSystem::Get()->CreateReadFile(filePath);
	if (!file)
	{
		LogWarning(L"QuakeHumanView::AddMediaFile(): Failed to open \"" + fileName + L"\" for reading");
		return false;
	}

	if (!file->GetSize())
	{
		delete file;

		LogWarning(L"QuakeHumanView::AddMediaFile(): Empty file \"" + filePath + L"\"");
		return false;
	}

	char* filedata = new char[file->GetSize() + 1];
	memset(filedata, 0, file->GetSize() + 1);
	file->Read(filedata, file->GetSize());

	SHA1 sha1;
	sha1.AddBytes(filedata, file->GetSize());

	unsigned char* digest = sha1.GetDigest();
	std::string sha1Base64 = Base64Encode(digest, 20);
	std::string sha1Hex = HexEncode((char*)digest, 20);
	if (digestTo)
		*digestTo = std::string((char*)digest, 20);
	free(digest);

	delete file;

	// Put in list
	mMedia[fileName] = MediaInfo(fileRelativePath, sha1Base64);
	//LogInformation(sha1Hex + " is " + ToString(fileName));

	if (fileData)
		*fileData = std::move(filedata);

	return true;
}

void QuakeLogic::FillMediaCache()
{
	LogInformation("Calculating media file checksums");

	// Collect all media file paths
	std::vector<std::wstring> files;

	// Collect media file information from paths into cache
	const std::set<wchar_t> ignore = { L'.' };
	std::wstring gamePath = ToWideString(mGameSpec.mPath);
	for (const auto& path : FileSystem::Get()->GetRecursiveDirectories(gamePath))
	{
		FileSystem::Get()->GetFileList(files, path, true);
		for (const std::wstring& file : files)
		{
			std::wstring fileName = FileSystem::Get()->GetFileName(file);
			if (mMedia.find(file) != mMedia.end()) // Do not override
				continue;

			if (ignore.count(fileName[0]))
				continue;

			AddMediaFile(fileName, file, path + L"/" + fileName);
		}
	}

	LogInformation(std::to_string(mMedia.size()) + " media files collected");
}

void QuakeLogic::SendMediaData()
{
	std::wstring langSuffix;
	langSuffix.append(L".").append(L".tr");

	std::unordered_map<std::wstring, std::wstring> mediaSent;
	for (const auto& m : mMedia)
	{
		if (StringEndsWith(m.first, L".tr") && !StringEndsWith(m.first, langSuffix))
			continue;
		mediaSent[m.first] = m.second.path;
	}

	EventManager::Get()->TriggerEvent(std::make_shared<QuakeEventDataHandleMedia>(mediaSent));
}

void QuakeLogic::LoadActors(BspLoader& bspLoader)
{
	std::map<std::string, std::string> modelResources;
	std::map<std::string, std::string> triggerResources;
	std::map<std::string, std::string> targetResources;

	modelResources["ammo_bullets"] = "actors/quake/models/ammo/bullet.xml";
	modelResources["ammo_cells"] = "actors/quake/models/ammo/cell.xml";
	modelResources["ammo_grenades"] = "actors/quake/models/ammo/grenade.xml";
	modelResources["ammo_lightning"] = "actors/quake/models/ammo/lightning.xml";
	modelResources["ammo_rockets"] = "actors/quake/models/ammo/rocket.xml";
	modelResources["ammo_shells"] = "actors/quake/models/ammo/shell.xml";
	modelResources["ammo_slugs"] = "actors/quake/models/ammo/slug.xml";
	modelResources["weapon_grenadelauncher"] = "actors/quake/models/weapon/grenadelauncher.xml";
	modelResources["weapon_lightning"] = "actors/quake/models/weapon/lightning.xml";
	modelResources["weapon_machinegun"] = "actors/quake/models/weapon/machinegun.xml";
	modelResources["weapon_plasmagun"] = "actors/quake/models/weapon/plasmagun.xml";
	modelResources["weapon_railgun"] = "actors/quake/models/weapon/railgun.xml";
	modelResources["weapon_shotgun"] = "actors/quake/models/weapon/shotgun.xml";
	modelResources["weapon_rocketlauncher"] = "actors/quake/models/weapon/rocketlauncher.xml";
	modelResources["item_armor_shard"] = "actors/quake/models/armor/armorshard.xml";
	modelResources["item_armor_combat"] = "actors/quake/models/armor/armorcombat.xml";
	modelResources["item_armor_body"] = "actors/quake/models/armor/armorbody.xml";
	modelResources["item_health_mega"] = "actors/quake/models/health/healthmega.xml";
	modelResources["item_health_small"] = "actors/quake/models/health/healthsmall.xml";
	modelResources["item_health_large"] = "actors/quake/models/health/healthlarge.xml";
	modelResources["item_health"] = "actors/quake/models/health/health.xml";
	targetResources["info_player_deathmatch"] = "actors/quake/target/location.xml";
	//targetResources["target_speaker"] = "actors/quake/target/speaker.xml";
	triggerResources["trigger_teleport"] = "actors/quake/trigger/teleporter.xml";
	triggerResources["trigger_push"] = "actors/quake/trigger/push.xml";

	std::map<std::string, BSPEntity> targets;
	for (int i = 0; i < bspLoader.mNumEntities; i++)
	{
		const BSPEntity& entity = bspLoader.mEntities[i];
		std::string target = bspLoader.GetValueForKey(&entity, "targetname");
		if (!target.empty())
			targets[target] = entity;
	}

	for (int i = 0; i < bspLoader.mNumEntities; i++)
	{
		const BSPEntity& entity = bspLoader.mEntities[i];
		std::string className = bspLoader.GetValueForKey(&entity, "classname");

		if (modelResources.find(className) != modelResources.end())
		{
			std::string gameType = bspLoader.GetValueForKey(&entity, "gametype");
			std::string notGameType = bspLoader.GetValueForKey(&entity, "not_gametype");

			if (gameType.empty() && notGameType.empty() ||
				gameType.find("duel") != std::string::npos ||
				notGameType.find("duel") == std::string::npos)
			{
				BSPVector3 origin;
				if (bspLoader.GetVectorForKey(&entity, "origin", origin))
				{
					Transform initTransform;
					initTransform.SetTranslation(origin[0], origin[1], origin[2]);
					std::shared_ptr<Actor> pActor = CreateActor(
						modelResources[className], nullptr, &initTransform);
					if (pActor)
					{
						// fire an event letting everyone else know that we created a new actor
						std::shared_ptr<EventDataNewActor> pNewActorEvent(
							new EventDataNewActor(pActor->GetId()));
						BaseEventManager::Get()->QueueEvent(pNewActorEvent);
					}
				}
			}
		}
		else if (targetResources.find(className) != targetResources.end())
		{
			std::string gameType = bspLoader.GetValueForKey(&entity, "gametype");
			std::string notGameType = bspLoader.GetValueForKey(&entity, "not_gametype");

			if (gameType.empty() && notGameType.empty() ||
				gameType.find("duel") != std::string::npos ||
				notGameType.find("duel") == std::string::npos)
			{
				BSPVector3 origin;
				if (bspLoader.GetVectorForKey(&entity, "origin", origin))
				{
					Transform initTransform;
					initTransform.SetTranslation(origin[0], origin[1], origin[2]);
					std::shared_ptr<Actor> pActor = CreateActor(
						targetResources[className], nullptr, &initTransform);
					if (pActor)
					{
						float angle = bspLoader.GetFloatForKey(&entity, "angle");
						if (angle)
						{
							std::shared_ptr<TransformComponent> pTransformComponent(
								pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
							if (pTransformComponent)
							{
								Matrix4x4<float> yawRotation = Rotation<4, float>(
									AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));
								pTransformComponent->SetRotation(yawRotation);
							}
						}

						const char* target = bspLoader.GetValueForKey(&entity, "targetname");
						if (target)
						{
							if (className != "target_speaker")
							{
								std::shared_ptr<LocationTarget> pLocationTarget(
									pActor->GetComponent<LocationTarget>(LocationTarget::Name).lock());
								if (pLocationTarget)
									pLocationTarget->SetTarget(target);
							}
						}

						std::shared_ptr<AudioComponent> pAudioComponent(
							pActor->GetComponent<AudioComponent>(AudioComponent::Name).lock());
						if (pAudioComponent)
						{
							std::string audios = bspLoader.GetValueForKey(&entity, "noise");
							if (!audios.empty())
							{
								pAudioComponent->ClearAudios();

								audios.erase(std::remove(audios.begin(), audios.end(), '\r'), audios.end());
								audios.erase(std::remove(audios.begin(), audios.end(), '\n'), audios.end());
								audios.erase(std::remove(audios.begin(), audios.end(), '\t'), audios.end());
								size_t audioBegin = 0, audioEnd = 0;
								do
								{
									audioEnd = audios.find(',', audioBegin);
									pAudioComponent->AddAudio("art/quake/audio/" + audios.substr(audioBegin, audioEnd - audioBegin));

									audioBegin = audioEnd + 1;
								} while (audioEnd != std::string::npos);

								pAudioComponent->PostInit();
							}
						}

						// fire an event letting everyone else know that we created a new actor
						std::shared_ptr<EventDataNewActor> pNewActorEvent(
							new EventDataNewActor(pActor->GetId()));
						BaseEventManager::Get()->QueueEvent(pNewActorEvent);
					}
				}
			}
		}
		else if (triggerResources.find(className) != triggerResources.end())
		{
			std::string gameType = bspLoader.GetValueForKey(&entity, "gametype");
			std::string notGameType = bspLoader.GetValueForKey(&entity, "not_gametype");

			if (gameType.empty() && notGameType.empty() ||
				gameType.find("duel") != std::string::npos ||
				notGameType.find("duel") == std::string::npos)
			{
				Transform initTransform;
				std::shared_ptr<Actor> pActor = CreateActor(
					triggerResources[className], nullptr, &initTransform);
				if (pActor)
				{
					float angle = bspLoader.GetFloatForKey(&entity, "angle");
					if (angle)
					{
						std::shared_ptr<TransformComponent> pTransformComponent(
							pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
						if (pTransformComponent)
						{
							Matrix4x4<float> yawRotation = Rotation<4, float>(
								AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));
							pTransformComponent->SetRotation(yawRotation);
						}
					}
				}

				const char* model = bspLoader.GetValueForKey(&entity, "model");
				const char* target = bspLoader.GetValueForKey(&entity, "target");
				if (model && target)
				{
					if (className == "trigger_teleport")
					{
						std::shared_ptr<TeleporterTrigger> pTeleporterTrigger(
							pActor->GetComponent<TeleporterTrigger>(TeleporterTrigger::Name).lock());
						if (pTeleporterTrigger)
						{
							Transform targetTransform;

							BSPVector3 origin;
							if (bspLoader.GetVectorForKey(&targets[target], "origin", origin))
								targetTransform.SetTranslation(origin[0], origin[1], origin[2]);
							float angle = bspLoader.GetFloatForKey(&targets[target], "angle");
							if (angle)
							{
								Matrix4x4<float> yawRotation = Rotation<4, float>(
									AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));
								targetTransform.SetRotation(yawRotation);
							}
							pTeleporterTrigger->SetTarget(targetTransform);
						}
					}
					else if (className == "trigger_push")
					{
						std::shared_ptr<PushTrigger> pPushTrigger(
							pActor->GetComponent<PushTrigger>(PushTrigger::Name).lock());
						if (pPushTrigger)
						{
							Transform targetTransform;

							BSPVector3 origin;
							if (bspLoader.GetVectorForKey(&targets[target], "origin", origin))
								targetTransform.SetTranslation(origin[0], origin[1], origin[2]);
							float angle = bspLoader.GetFloatForKey(&targets[target], "angle");
							if (angle)
							{
								Matrix4x4<float> yawRotation = Rotation<4, float>(
									AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));
								targetTransform.SetRotation(yawRotation);
							}
							pPushTrigger->SetTarget(targetTransform);
						}
					}

					if (strcmp(model, ""))
					{
						// add the model as a brush
						if (model[0] == '*')
						{
							int modelnr = atoi(&model[1]);
							if ((modelnr >= 0) && (modelnr < bspLoader.mNumModels))
							{
								const BSPModel& model = bspLoader.mDModels[modelnr];
								const BSPBrush& brush = bspLoader.mDBrushes[model.firstBrush];
								std::vector<Plane3<float>> planes;
								Vector3<float> scale = { 1.1f, 1.1f, 1.0f };
								for (int p = 0; p < brush.numSides; p++)
								{
									int sideid = brush.firstSide + p;
									BSPBrushSide& brushside = bspLoader.mDBrushsides[sideid];
									int planeid = brushside.planeNum;
									BSPPlane& plane = bspLoader.mDPlanes[planeid];
									Vector3<float> normal = { plane.normal[0], plane.normal[1], plane.normal[2] };
									planes.push_back(Plane3<float>(normal, plane.dist));
								}
								std::shared_ptr<PhysicComponent> pPhysicComponent(
									pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
								if (pPhysicComponent)
								{
									BaseGamePhysic* gamePhysics = GameLogic::Get()->GetGamePhysics().get();
									gamePhysics->AddConvexVertices(planes.data(), planes.size(), scale, pActor,
										pPhysicComponent->GetDensity(), pPhysicComponent->GetMaterial());

									std::shared_ptr<TransformComponent> pTransform(
										pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
									if (pTransform)
										pTransform->SetPosition(gamePhysics->GetCenter(pActor->GetId()));
								}
							}
						}
					}
				}

				// fire an event letting everyone else know that we created a new actor
				std::shared_ptr<EventDataNewActor> pNewActorEvent(new EventDataNewActor(pActor->GetId()));
				BaseEventManager::Get()->QueueEvent(pNewActorEvent);
			}
		}
		else if (className == "worldspawn")
		{
			std::shared_ptr<Actor> pActor = CreateActor("actors/quake/music/music.xml", nullptr);
			if (pActor)
			{
				std::shared_ptr<AudioComponent> pAudioComponent(
					pActor->GetComponent<AudioComponent>(AudioComponent::Name).lock());
				if (pAudioComponent)
				{
					std::string audios = bspLoader.GetValueForKey(&entity, "noise");
					if (!audios.empty())
					{
						pAudioComponent->ClearAudios();

						audios.erase(std::remove(audios.begin(), audios.end(), '\r'), audios.end());
						audios.erase(std::remove(audios.begin(), audios.end(), '\n'), audios.end());
						audios.erase(std::remove(audios.begin(), audios.end(), '\t'), audios.end());
						size_t audioBegin = 0, audioEnd = 0;
						do
						{
							audioEnd = audios.find(',', audioBegin);
							pAudioComponent->AddAudio("art/quake/audio/" + audios.substr(audioBegin, audioEnd - audioBegin));

							audioBegin = audioEnd + 1;
						} while (audioEnd != std::string::npos);

						pAudioComponent->PostInit();
					}
				}

				// fire an event letting everyone else know that we created a new actor
				std::shared_ptr<EventDataNewActor> pNewActorEvent(
					new EventDataNewActor(pActor->GetId()));
				BaseEventManager::Get()->QueueEvent(pNewActorEvent);
			}
		}
	}
}

bool QuakeLogic::LoadGameAsync(tinyxml2::XMLElement* pRoot)
{
	// Read Textures and calculate sha1 sums
	FillMediaCache();

	if (!GetGameInit())
	{
		LogError("Game init failed for unknown reason");
		return false;
	}

	QuakeActorFactory* actorFactory = dynamic_cast<QuakeActorFactory*>(mActorFactory);
	LogAssert(actorFactory, "actor factory is not initialized");

	// load all initial actors
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
		{
			if (pPhysicalComponent->GetShape() == "BSP")
			{
				std::shared_ptr<ResHandle>& resHandle = ResCache::Get()->GetHandle(
					&BaseResource(ToWideString(pPhysicalComponent->GetMesh())));
				if (resHandle)
				{
					const std::shared_ptr<BspResourceExtraData>& extra =
						std::static_pointer_cast<BspResourceExtraData>(resHandle->GetExtra());
					LoadActors(extra->GetLoader());
					break;
				}
			}
		}
	}

	// Send media
	SendMediaData();

	// Remove stale "recent" chat messages from previous connections
	mChatBackend->ClearRecentChat();

	// Make sure the size of the recent messages buffer is right
	mChatBackend->ApplySettings();

	// chat backend notification
	EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataInitChat>(mChatBackend));

	return true;
}

bool QuakeLogic::LoadGameDelegate(tinyxml2::XMLElement* pRoot)
{
	System::Get()->SetResizable(true);

	mGameSpec = FindSubgame(Settings::Get()->Get("selected_game"));
	LogInformation("Using default gameid [" + mGameSpec.mId + "]");
	if (!mGameSpec.IsValid())
	{
		LogWarning("Game specified in selected_game [" +
			Settings::Get()->Get("selected_game") + "] is invalid.");
		return false;
	}

	// This is the ultimate default world path
	std::string gamePath =
		ToString(FileSystem::Get()->GetWorkingDirectory()) +
		"/../../Assets/Art/Quake/games/" + mGameSpec.mId;
	if (gamePath.empty())
		LogError("Supplied empty game path");

	// Update world information using main menu data
	std::vector<WorldSpecification> worldSpecs = GetAvailableWorlds();
	int worldIndex = Settings::Get()->GetInt("selected_world");
	if (worldIndex >= 0 && worldIndex < (int)worldSpecs.size())
	{
		auto& worldSpec = worldSpecs[worldIndex];

		LogInformation("Selected world: " + worldSpec.mName + " [" + worldSpec.mPath + "]");

		// For singleplayer and local logic
		if (worldSpec.mPath.empty())
		{
			LogWarning("No world selected and no address provided. Nothing to do.");
			return false;
		}

		if (!FileSystem::Get()->ExistDirectory(ToWideString(worldSpec.mPath)))
		{
			LogWarning("Provided world path doesn't exist: " + worldSpec.mPath);
			return false;
		}

		mWorldSpec = worldSpec;
	}

	LogInformation("Game created id " + mGameSpec.mId +
		" - world: " + mWorldSpec.mPath + " - game: " + mGameSpec.mPath);

	Settings::Get()->CreateLayer(SL_GAME);

	// Create world if it doesn't exist
	try
	{
		std::string fileName = ToString(
			FileSystem::Get()->GetFileName(ToWideString(mWorldSpec.mPath)));
		LoadGameConfAndInitWorld(mWorldSpec.mPath, fileName, mGameSpec, false);
	}
	catch (const BaseException& e)
	{
		LogError("Failed to initialize world: " + std::string(e.what()));
	}

	// Initialize Environment
	// Determine which database backend to use
	std::string confPath = mWorldSpec.mPath + "/world.mt";
	std::string playerBackendName = "bin";
	std::string authBackendName = "bin";

	bool succeeded = Settings::Get()->ReadConfigFile(confPath.c_str());

	// If we open world.mt read the backend configurations.
	if (succeeded)
	{
		// Read those values before setting defaults
		bool playerBackendExists = Settings::Get()->Exists("player_backend");
		bool authBackendExists = Settings::Get()->Exists("auth_backend");

		// player backend is not set, assume it's legacy file backend.
		if (!playerBackendExists)
		{
			// fall back to files
			Settings::Get()->Set("player_backend", "files");
			playerBackendName = "files";

			if (!Settings::Get()->UpdateConfigFile(confPath.c_str()))
			{
				LogWarning("LogicEnvironment::LogicEnvironment(): Failed to update world.mt!");
			}
		}
		else
		{
			playerBackendName = Settings::Get()->Get("player_backend");
		}

		// auth backend is not set, assume it's legacy file backend.
		if (!authBackendExists)
		{
			Settings::Get()->Set("auth_backend", "files");
			authBackendName = "files";

			if (!Settings::Get()->UpdateConfigFile(confPath.c_str()))
			{
				LogWarning("LogicEnvironment::LogicEnvironment(): Failed to update world.mt!");
			}
		}
		else
		{
			authBackendName = Settings::Get()->Get("auth_backend");
		}
	}

	if (playerBackendName == "files")
	{
		LogWarning("/!\\ You are using old player file backend. ");
	}

	if (authBackendName == "files")
	{
		LogWarning("/!\\ You are using old auth file backend. ");
	}

	mPlayerDatabase = OpenPlayerDatabase(playerBackendName, mWorldSpec.mPath);
	mAuthDatabase = OpenAuthDatabase(authBackendName, mWorldSpec.mPath);

	return true;
}

void QuakeLogic::LookAtKiller(
	const std::shared_ptr<Actor>& inflictor,
	const std::shared_ptr<PlayerActor>& player,
	const std::shared_ptr<PlayerActor>& attacker)
{
	if (attacker && attacker != player)
	{
		Vector4<float> playerTranslation, attackerTranslation;
		std::shared_ptr<TransformComponent> pTransformComponent(
			player->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
			playerTranslation = pTransformComponent->GetTransform().GetTranslationW1();

		pTransformComponent =
			attacker->GetComponent<TransformComponent>(TransformComponent::Name).lock();
		if (pTransformComponent)
			attackerTranslation = pTransformComponent->GetTransform().GetTranslationW1();

		Vector4<float> direction = attackerTranslation - playerTranslation;
		Normalize(direction);
		Matrix4x4<float> rotation = Rotation<4, float>(AxisAngle<4, float>(direction, 0.f));

		Transform transform;
		transform.SetRotation(rotation);
		EventManager::Get()->TriggerEvent(
			std::make_shared<QuakeEventDataRotateActor>(player->GetId(), transform));
	}
	else if (inflictor && inflictor != player)
	{
		Vector4<float> playerTranslation, inflictorTranslation;
		std::shared_ptr<TransformComponent> pTransformComponent(
			player->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
			playerTranslation = pTransformComponent->GetTransform().GetTranslationW1();

		pTransformComponent =
			attacker->GetComponent<TransformComponent>(TransformComponent::Name).lock();
		if (pTransformComponent)
			inflictorTranslation = pTransformComponent->GetTransform().GetTranslationW1();

		Vector4<float> direction = inflictorTranslation - playerTranslation;
		Normalize(direction);
		Matrix4x4<float> rotation = Rotation<4, float>(AxisAngle<4, float>(direction, 0.f));

		Transform transform;
		transform.SetRotation(rotation);
		EventManager::Get()->TriggerEvent(
			std::make_shared<QuakeEventDataRotateActor>(player->GetId(), transform));
	}
	else
	{
		Transform playerTransform;
		std::shared_ptr<TransformComponent> pTransformComponent(
			player->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
			playerTransform = pTransformComponent->GetTransform();

		EventManager::Get()->TriggerEvent(
			std::make_shared<QuakeEventDataRotateActor>(player->GetId(), playerTransform));
	}
}


void QuakeLogic::Die(
	int damage, MeansOfDeath meansOfDeath,
	const std::shared_ptr<Actor>& inflictor,
	const std::shared_ptr<PlayerActor>& player,
	const std::shared_ptr<PlayerActor>& attacker)
{
	if (player->GetState().moveType == PM_DEAD)
		return;

	player->GetState().moveType = PM_DEAD;
	player->GetState().viewHeight = DEAD_VIEWHEIGHT;

	player->GetState().persistant[PERS_KILLED]++;

	if (attacker)
	{
		attacker->GetState().lastKilled = player->GetId();

		if (attacker != player)
		{
			attacker->GetState().persistant[PERS_SCORE] += 1;

			if (meansOfDeath == MOD_GAUNTLET)
			{
				// play humiliation on player
				attacker->GetState().persistant[PERS_GAUNTLET_FRAG_COUNT]++;

				// add the sprite over the player's head
				attacker->GetState().eFlags &= ~(
					EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT |
					EF_AWARD_GAUNTLET | EF_AWARD_ASSIST |
					EF_AWARD_DEFEND | EF_AWARD_CAP);
				attacker->GetState().eFlags |= EF_AWARD_GAUNTLET;

				// also play humiliation on target
				player->GetState().persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
			}
		}
		else
		{
			attacker->GetState().persistant[PERS_SCORE] -= 1;
		}
		mStatBars->EventHandler(attacker, "score_changed");
	}
	else
	{
		player->GetState().persistant[PERS_SCORE] -= 1;
		mStatBars->EventHandler(player, "score_changed");
	}

	// send updated scores to any clients that are following this one,
	// or they would get stale scoreboards
	player->GetState().takeDamage = true;	// can still be gibbed

	player->GetState().weapon = WP_NONE;
	player->GetState().contents = CONTENTS_CORPSE;
	LookAtKiller(inflictor, player, attacker);

	// remove powerups
	memset(player->GetState().powerups, 0, sizeof(player->GetState().powerups));

	// never gib in a nodrop
	int anim = BOTH_DEATH1;

	// for the no-blood option, we need to prevent the health
	// from going to gib level
	if (player->GetState().stats[STAT_HEALTH] <= GIB_HEALTH)
		player->GetState().stats[STAT_HEALTH] = GIB_HEALTH + 1;

	player->GetState().legsAnim = anim;
	player->GetState().torsoAnim = anim;

	// call for animation death
	EventManager::Get()->TriggerEvent(
		std::make_shared<QuakeEventDataDeadActor>(player->GetId()));

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "death1"; //art/quake/audio/sound/player/death1.wav
	PlaySound(sound, params, true);
}


int QuakeLogic::CheckArmor(std::shared_ptr<PlayerActor> playerActor, int damage, int dflags)
{
	if (!damage)
		return 0;

	if (!playerActor)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	// armor
	int save = (int)ceil(damage * ARMOR_PROTECTION);
	if (save >= playerActor->GetState().stats[STAT_ARMOR])
		save = playerActor->GetState().stats[STAT_ARMOR];

	if (!save)
		return 0;

	playerActor->GetState().stats[STAT_ARMOR] -= save;
	mStatBars->EventHandler(playerActor, "armor_changed");

	return save;
}

/*
Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player state
damage values to that player for pain blends and kicks, and
global pain sound events for all players.
*/
void QuakeLogic::DamageFeedback(const std::shared_ptr<PlayerActor>& player)
{
	if (player->GetState().moveType == PM_DEAD)
		return;

	// total points of damage shot at the player this frame
	int count = player->GetState().damageBlood + player->GetState().damageArmor;
	if (count == 0)
		return;		// didn't take any damage

	if (count > 255) count = 255;

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if (player->GetState().damageFromWorld)
	{
		player->GetState().damagePitch = 255;
		player->GetState().damageYaw = 255;

		player->GetState().damageFromWorld = false;
	}

	// play an apropriate pain sound
	player->GetState().damageEvent++;
	player->GetState().damageCount = count;

	//
	// clear totals
	//
	player->GetState().damageBlood = 0;
	player->GetState().damageArmor = 0;
	player->GetState().damageKnockback = 0;
}



/*
============
Damage
============
*/

void QuakeLogic::Damage(
	int damage, int dflags, int mod,
	Vector3<float> dir, Vector3<float> point,
	const std::shared_ptr<PlayerActor>& target,
	const std::shared_ptr<Actor>& inflictor,
	const std::shared_ptr<PlayerActor>& attacker)
{
	if (!target->GetState().takeDamage)
		return;

	// reduce damage by the attacker's handicap value
	// unless they are rocket jumping
	if (attacker && attacker != target)
	{
		int max = attacker->GetState().stats[STAT_MAX_HEALTH];
		damage = damage * max / 100;
	}

	if (dir != Vector3<float>::Zero())
		dflags |= DAMAGE_NO_KNOCKBACK;
	else
		Normalize(dir);

	int knockback = damage;
	if (knockback > 200)
		knockback = 200;

	if (dflags & DAMAGE_NO_KNOCKBACK)
		knockback = 0;

	// figure momentum add, even if the damage won't be taken
	if (knockback && target)
	{
		Vector3<float>	kvel;
		float mass = 200;

		//kvel = dir * (g_knockback.value * (float)knockback / mass));
		//target->GetState().velocity += kvel;

		// set the timer so that the other client can't cancel
		// out the movement immediately
		if (!target->GetState().moveTime)
		{
			int t = knockback * 2;
			if (t < 50) t = 50;
			if (t > 200) t = 200;

			target->GetState().moveTime = t;
			//target->GetState().moveFlags |= PMF_TIME_KNOCKBACK;
		}
	}

	// battlesuit protects from all radius damage (but takes knockback)
	// and protects 50% against all damage
	if (target && target->GetState().powerups[PW_BATTLESUIT])
	{
		//AddEvent(targ, EV_POWERUP_BATTLESUIT, 0);
		if ((dflags & DAMAGE_RADIUS) || (mod == MOD_FALLING))
			return;

		damage = (int)(damage * 0.5f);
	}

	// add to the attacker's hit counter (if the target isn't a general entity like a prox mine)
	if (attacker && target != attacker &&
		target->GetState().stats[STAT_HEALTH] > 0 &&
		target->GetState().eType != ET_MISSILE &&
		target->GetState().eType != ET_GENERAL)
	{
		attacker->GetState().persistant[PERS_HITS]++;
		attacker->GetState().persistant[PERS_ATTACKEE_ARMOR] =
			(target->GetState().stats[STAT_HEALTH] << 8) | (target->GetState().stats[STAT_ARMOR]);
	}

	// always give half damage if hurting self
	// calculated after knockback, so rocket jumping works
	if (target == attacker)
		damage = (int)(damage * 0.5f);

	if (damage < 1)
		damage = 1;

	int take = damage;
	int save = 0;

	// save some from armor
	int asave = CheckArmor(target, take, dflags);
	take -= asave;

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (target)
	{
		if (attacker)
			target->GetState().persistant[PERS_ATTACKER] = attacker->GetId();
		else
			target->GetState().persistant[PERS_ATTACKER] = ENTITYNUM_WORLD;

		target->GetState().damageArmor += asave;
		target->GetState().damageBlood += take;
		target->GetState().damageKnockback += knockback;
		if (dir != Vector3<float>::Zero())
		{
			target->GetState().damageFrom = Vector3<float>{ dir };
			target->GetState().damageFromWorld = false;
		}
		else
		{
			Transform playerTransform;
			std::shared_ptr<TransformComponent> pTransformComponent(
				target->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				target->GetState().damageFrom = pTransformComponent->GetTransform().GetTranslation();
				target->GetState().damageFromWorld = true;
			}
		}
	}

	if (target)
	{
		// set the last client who damaged the target
		target->GetState().lastHurt = attacker->GetId();
		target->GetState().lastHurtMod = mod;
	}

	// do the damage
	if (take)
	{
		target->GetState().stats[STAT_HEALTH] -= take;
		mStatBars->EventHandler(target, "health_changed");

		if (target->GetState().stats[STAT_HEALTH] <= 0)
		{
			//target->GetState().flags |= FL_NO_KNOCKBACK;

			if (target->GetState().stats[STAT_HEALTH] < -999)
				target->GetState().stats[STAT_HEALTH] = -999;

			//targ->enemy = attacker;
			Die(take, (MeansOfDeath)mod, inflictor, target, attacker);
		}
		else //if (targ->pain)
		{
			//targ->pain(targ, attacker, take);
			DamageFeedback(attacker);

			std::shared_ptr<TransformComponent> pPlayerTransform(
				target->GetComponent<TransformComponent>(TransformComponent::Name).lock());

			std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
			Transform cameraTransform = camera->GetAbsoluteTransform();

			if (target->GetState().stats[STAT_HEALTH] < 25)
			{
				SoundParams params;
				params.type = SoundParams::SP_POSITIONAL;
				params.position = cameraTransform.GetTranslation();
				SimpleSound sound;
				sound.name = "pain25_1"; //art/quake/audio/sound/player/pain25_1.wav
				PlaySound(sound, params, true);
			}
			else if (target->GetState().stats[STAT_HEALTH] < 50)
			{
				SoundParams params;
				params.type = SoundParams::SP_POSITIONAL;
				params.position = cameraTransform.GetTranslation();
				SimpleSound sound;
				sound.name = "pain50_1"; //art/quake/audio/sound/player/pain50_1.wav
				PlaySound(sound, params, true);
			}
			else if (target->GetState().stats[STAT_HEALTH] < 75)
			{
				SoundParams params;
				params.type = SoundParams::SP_POSITIONAL;
				params.position = cameraTransform.GetTranslation();
				SimpleSound sound;
				sound.name = "pain75_1"; //art/quake/audio/sound/player/pain75_1.wav
				PlaySound(sound, params, true);
			}
			else
			{
				SoundParams params;
				params.type = SoundParams::SP_POSITIONAL;
				params.position = cameraTransform.GetTranslation();
				SimpleSound sound;
				sound.name = "pain100_1"; //art/quake/audio/sound/player/pain100_1.wav
				PlaySound(sound, params, true);
			}
		}
	}
}

bool QuakeLogic::LogAccuracyHit(
	const std::shared_ptr<PlayerActor>& target,
	const std::shared_ptr<PlayerActor>& attacker)
{
	if (!target->GetState().takeDamage)
		return false;

	if (target == attacker)
		return false;

	if (!target)
		return false;

	if (!attacker)
		return false;

	if (target->GetState().stats[STAT_HEALTH] <= 0)
		return false;

	return true;
}

/*
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
*/
bool QuakeLogic::CanDamage(const std::shared_ptr<PlayerActor>& target, Vector3<float> origin)
{
	return true;
}

bool QuakeLogic::RadiusDamage(float damage, float radius, int mod,
	Vector3<float> origin, const std::shared_ptr<PlayerActor>& attacker)
{
	bool hitClient = false;

	if (radius < 1)
		radius = 1;

	for (ActorMap::const_iterator it = mActors.begin(); it != mActors.end(); ++it)
	{
		std::shared_ptr<PlayerActor> playerActor =
			std::dynamic_pointer_cast<PlayerActor>((*it).second);
		if (playerActor)
		{
			if (!playerActor->GetState().takeDamage)
				continue;

			std::shared_ptr<TransformComponent> pTransformComponent(
				playerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				Vector3<float> location = pTransformComponent->GetTransform().GetTranslation();

				float dist = Length(origin - location);
				if (dist >= radius)
					continue;

				float points = damage * (1.f - dist / radius);
				if (CanDamage(playerActor, origin))
				{
					if (LogAccuracyHit(playerActor, attacker))
						hitClient = true;

					Vector3<float> dir = location - origin;
					// push the center of mass higher than the origin so players
					// get knocked into the air more
					dir[2] += 24;
					Damage((int)points, DAMAGE_RADIUS, mod, dir, origin, playerActor, NULL, attacker);
				}
			}
		}
	}

	return hitClient;
}

void QuakeLogic::SplashDamageDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataSplashDamage> pCastEventData =
		std::static_pointer_cast<QuakeEventDataSplashDamage>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor && pGameActor->GetType() == "Fire")
	{
		if (pGameActor->GetComponent<GrenadeFire>(GrenadeFire::Name).lock())
		{
			std::shared_ptr<GrenadeFire> pGrenadeFire =
				pGameActor->GetComponent<GrenadeFire>(GrenadeFire::Name).lock();
			RadiusDamage(100, 150, MOD_GRENADE, pCastEventData->GetOrigin(), 
				std::dynamic_pointer_cast<PlayerActor>(pGrenadeFire->mAttacker));

			std::shared_ptr<EventDataRequestDestroyActor>
				pRequestDestroyActorEvent(new EventDataRequestDestroyActor(pGameActor->GetId()));
			EventManager::Get()->QueueEvent(pRequestDestroyActorEvent);
		}
		else if (pGameActor->GetComponent<RocketFire>(RocketFire::Name).lock())
		{
			std::shared_ptr<RocketFire> pRocketFire =
				pGameActor->GetComponent<RocketFire>(RocketFire::Name).lock();
			RadiusDamage(100, 120, MOD_ROCKET, pCastEventData->GetOrigin(),
				std::dynamic_pointer_cast<PlayerActor>(pRocketFire->mAttacker));

			std::shared_ptr<EventDataRequestDestroyActor>
				pRequestDestroyActorEvent(new EventDataRequestDestroyActor(pGameActor->GetId()));
			EventManager::Get()->QueueEvent(pRequestDestroyActorEvent);
		}
		else if (pGameActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock())
		{
			std::shared_ptr<PlasmaFire> pPlasmaFire =
				pGameActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock();
			RadiusDamage(20, 60, MOD_PLASMA, pCastEventData->GetOrigin(),
				std::dynamic_pointer_cast<PlayerActor>(pPlasmaFire->mAttacker));

			std::shared_ptr<EventDataRequestDestroyActor>
				pRequestDestroyActorEvent(new EventDataRequestDestroyActor(pGameActor->GetId()));
			EventManager::Get()->QueueEvent(pRequestDestroyActorEvent);
		}
	}
}

/*
======================================================================

GAUNTLET

======================================================================
*/

void QuakeLogic::GauntletAttack(
	const std::shared_ptr<PlayerActor>& player, 
	const Vector3<float>& muzzle, const Vector3<float>& forward)
{
	//set muzzle location relative to pivoting eye
	Vector3<float> end = muzzle + forward * 32.f;

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "fstrun"; //art/quake/audio/sound/weapons/melee/fstrun.ogg
	PlaySound(sound, params, true);

	std::vector<ActorId> collisionActors;
	std::vector<Vector3<float>> collisions, collisionNormals;
	mPhysics->CastRay(muzzle, end, collisionActors, collisions, collisionNormals);

	ActorId closestCollisionId = INVALID_ACTOR_ID;
	Vector3<float> closestCollision = NULL;
	for (unsigned int i = 0; i < collisionActors.size(); i++)
	{
		if (collisionActors[i] != player->GetId())
		{
			if (closestCollision != NULL)
			{
				if (Length(closestCollision - muzzle) > Length(collisions[i] - muzzle))
				{
					closestCollisionId = collisionActors[i];
					closestCollision = collisions[i];
				}
			}
			else
			{
				closestCollisionId = collisionActors[i];
				closestCollision = collisions[i];
			}
		}
	}

	if (closestCollisionId != INVALID_ACTOR_ID &&
		std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]))
	{
		std::shared_ptr<PlayerActor> target =
			std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]);
		if (LogAccuracyHit(target, player))
			player->GetState().accuracyHits++;

		Transform initTransform;
		initTransform.SetTranslation(closestCollision);
		CreateActor("actors/quake/effects/bleed.xml", nullptr, &initTransform);

		int damage = 50;
		Damage(damage, 0, MOD_GAUNTLET, forward, muzzle, target, player, player);
	}
}


/*
======================================================================

MACHINEGUN

======================================================================
*/


void QuakeLogic::BulletFire(
	const std::shared_ptr<PlayerActor>& player,
	const Vector3<float>& muzzle, const Vector3<float>& forward,
	const Vector3<float>& right, const Vector3<float>& up, float spread, int damage)
{
	float r = ((Randomizer::Rand() & 0x7fff) / (float)0x7fff) * (float)GE_C_PI * 2.f;
	float u = sin(r) * (2.f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff) - 0.5f) * spread * 16.f;
	r = cos(r) * (2.f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff) - 0.5f) * spread * 16.f;
	Vector3<float> end = muzzle + forward * 8192.f * 16.f;
	end += right * r;
	end += up * u;

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "ric1"; //art/quake/audio/sound/weapons/machinegun/ric1.ogg
	PlaySound(sound, params, true);

	std::vector<ActorId> collisionActors;
	std::vector<Vector3<float>> collisions, collisionNormals;
	mPhysics->CastRay(muzzle, end, collisionActors, collisions, collisionNormals);

	ActorId closestCollisionId = INVALID_ACTOR_ID;
	Vector3<float> closestCollision = NULL;
	for (unsigned int i = 0; i < collisionActors.size(); i++)
	{
		if (collisionActors[i] != player->GetId())
		{
			if (closestCollision != NULL)
			{
				if (Length(closestCollision - muzzle) > Length(collisions[i] - muzzle))
				{
					closestCollisionId = collisionActors[i];
					closestCollision = collisions[i];
				}
			}
			else
			{
				closestCollisionId = collisionActors[i];
				closestCollision = collisions[i];
			}
		}
	}

	if (closestCollisionId != INVALID_ACTOR_ID &&
		std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]))
	{
		std::shared_ptr<PlayerActor> target =
			std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]);
		if (LogAccuracyHit(target, player))
			player->GetState().accuracyHits++;

		Transform initTransform;
		initTransform.SetTranslation(closestCollision);
		CreateActor("actors/quake/effects/bleed.xml", nullptr, &initTransform);

		Damage(damage, 0, MOD_MACHINEGUN, forward, closestCollision, target, player, player);
	}
	else
	{
		Transform initTransform;
		initTransform.SetTranslation(closestCollision);
		CreateActor("actors/quake/effects/bulletexplosion.xml", nullptr, &initTransform);
	}
}


/*
======================================================================

SHOTGUN

======================================================================
*/

// DEFAULT_SHOTGUN_SPREAD and DEFAULT_SHOTGUN_COUNT	are in bg_public.h, because
// client predicts same spreads
#define	DEFAULT_SHOTGUN_DAMAGE	10

bool QuakeLogic::ShotgunPellet(const std::shared_ptr<PlayerActor>& player, 
	const Vector3<float>& forward, const Vector3<float>& start, const Vector3<float>& end)
{
	std::vector<ActorId> collisionActors;
	std::vector<Vector3<float>> collisions, collisionNormals;
	mPhysics->CastRay(start, end, collisionActors, collisions, collisionNormals);

	ActorId closestCollisionId = INVALID_ACTOR_ID;
	Vector3<float> closestCollision = NULL;
	for (unsigned int i = 0; i < collisionActors.size(); i++)
	{
		if (collisionActors[i] != player->GetId())
		{
			if (closestCollision != NULL)
			{
				if (Length(closestCollision - start) > Length(collisions[i] - start))
				{
					closestCollisionId = collisionActors[i];
					closestCollision = collisions[i];
				}
			}
			else
			{
				closestCollisionId = collisionActors[i];
				closestCollision = collisions[i];
			}
		}
	}

	if (closestCollisionId != INVALID_ACTOR_ID &&
		std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]))
	{
		std::shared_ptr<PlayerActor> target =
			std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]);
		if (LogAccuracyHit(target, player))
			player->GetState().accuracyHits++;

		Transform initTransform;
		initTransform.SetTranslation(closestCollision);
		CreateActor("actors/quake/effects/bleed.xml", nullptr, &initTransform);

		int damage = DEFAULT_SHOTGUN_DAMAGE;
		Damage(damage, 0, MOD_SHOTGUN, forward, closestCollision, target, player, player);
		return true;
	}
	else
	{
		Transform initTransform;
		initTransform.SetTranslation(closestCollision);
		CreateActor("actors/quake/effects/bulletexplosion.xml", nullptr, &initTransform);
	}

	return false;
}

void QuakeLogic::ShotgunFire(
	const std::shared_ptr<PlayerActor>& player, 
	const Vector3<float>& muzzle, const Vector3<float>& forward,
	const Vector3<float>& right, const Vector3<float>& up)
{
	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "sshotf1b"; //art/quake/audio/sound/weapons/shotgun/sshotf1b.ogg
	PlaySound(sound, params, true);

	// generate the "random" spread pattern
	for (unsigned int i = 0; i < DEFAULT_SHOTGUN_COUNT; i++)
	{
		float r = (2.f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff) - 0.5f) * DEFAULT_SHOTGUN_SPREAD * 16.f;
		float u = (2.f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff) - 0.5f) * DEFAULT_SHOTGUN_SPREAD * 16.f;
		Vector3<float> end = muzzle + forward * 8192.f * 16.f;
		end += right * r;
		end += up * u;

		if (ShotgunPellet(player, forward, muzzle, end))
			player->GetState().accuracyHits++;
	}
}


/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void QuakeLogic::GrenadeLauncherFire(
	const std::shared_ptr<PlayerActor>& player, const Vector3<float>& muzzle, 
	const Vector3<float>& forward, const EulerAngles<float>& viewAngles)
{
	Matrix4x4<float> yawRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), viewAngles.mAngle[2]));
	Matrix4x4<float> pitchRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), viewAngles.mAngle[1] + (float)GE_C_QUARTER_PI));

	Transform initTransform;
	initTransform.SetRotation(yawRotation * pitchRotation);
	initTransform.SetTranslation(muzzle);

	Vector3<float> end = muzzle + forward * 8192.f * 16.f;
	Vector3<float> direction = end - muzzle;
	Normalize(direction);

	std::shared_ptr<Actor> pGameActor = 
		CreateActor("actors/quake/effects/grenadelauncherfire.xml", nullptr, &initTransform);
	if (pGameActor)
	{
		std::shared_ptr<GrenadeFire> pGrenadeFire =
			pGameActor->GetComponent<GrenadeFire>(GrenadeFire::Name).lock();
		if (pGrenadeFire)
			pGrenadeFire->mAttacker = player;

		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
		{
			pPhysicalComponent->SetIgnoreCollision(player->GetId(), true);

			direction[0] *= 800000.f;
			direction[1] *= 800000.f;
			direction[2] *= 500000.f;
			pPhysicalComponent->ApplyForce(direction);
		}
	}

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "grenlf1a"; //art/quake/audio/sound/weapons/grenade/grenlf1a.ogg
	PlaySound(sound, params, true);
}

/*
======================================================================

ROCKET

======================================================================
*/

void QuakeLogic::RocketLauncherFire(
	const std::shared_ptr<PlayerActor>& player, const Vector3<float>& muzzle, 
	const Vector3<float>& forward, const EulerAngles<float>& viewAngles)
{
	Matrix4x4<float> yawRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), viewAngles.mAngle[2]));
	Matrix4x4<float> pitchRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), viewAngles.mAngle[1]));

	Transform initTransform;
	initTransform.SetRotation(yawRotation * pitchRotation);
	initTransform.SetTranslation(muzzle);

	Vector3<float> end = muzzle + forward * 8192.f * 16.f;
	Vector3<float> direction = end - muzzle;
	Normalize(direction);

	std::shared_ptr<Actor> pGameActor =
		CreateActor("actors/quake/effects/rocketlauncherfire.xml", nullptr, &initTransform);
	if (pGameActor)
	{
		std::shared_ptr<RocketFire> pRocketFire =
			pGameActor->GetComponent<RocketFire>(RocketFire::Name).lock();
		if (pRocketFire)
			pRocketFire->mAttacker = player;

		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
		{
			pPhysicalComponent->SetGravity(Vector3<float>::Zero());
			pPhysicalComponent->SetIgnoreCollision(player->GetId(), true);

			direction[0] *= 1000000.f;
			direction[1] *= 1000000.f;
			direction[2] *= 1000000.f;
			pPhysicalComponent->ApplyForce(direction);
		}
	}

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "rocklf1a"; //art/quake/audio/sound/weapons/rocket/rocklf1a.ogg
	PlaySound(sound, params, true);
}


/*
======================================================================

PLASMA GUN

======================================================================
*/

void QuakeLogic::PlasmagunFire(
	const std::shared_ptr<PlayerActor>& player, const Vector3<float>& muzzle, 
	const Vector3<float>& forward, const EulerAngles<float>& viewAngles)
{
	Matrix4x4<float> yawRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), viewAngles.mAngle[2]));
	Matrix4x4<float> pitchRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), viewAngles.mAngle[1]));

	Transform initTransform;
	initTransform.SetRotation(yawRotation * pitchRotation);
	initTransform.SetTranslation(muzzle);

	Vector3<float> end = muzzle + forward * 8192.f * 16.f;
	Vector3<float> direction = end - muzzle;
	Normalize(direction);

	std::shared_ptr<Actor> pGameActor =
		CreateActor("actors/quake/effects/plasmagunfire.xml", nullptr, &initTransform);
	if (pGameActor)
	{
		std::shared_ptr<PlasmaFire> pPlasmaFire =
			pGameActor->GetComponent<PlasmaFire>(PlasmaFire::Name).lock();
		if (pPlasmaFire)
			pPlasmaFire->mAttacker = player;

		std::shared_ptr<PhysicComponent> pPhysicalComponent =
			pGameActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock();
		if (pPhysicalComponent)
		{
			pPhysicalComponent->SetGravity(Vector3<float>::Zero());
			pPhysicalComponent->SetIgnoreCollision(player->GetId(), true);

			direction[0] *= 4000.f;
			direction[1] *= 4000.f;
			direction[2] *= 4000.f;
			pPhysicalComponent->ApplyForce(direction);
		}
	}

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "hyprbf1a"; //art/quake/audio/sound/weapons/plasma/hyprbf1a.ogg
	PlaySound(sound, params, true);
}

/*
======================================================================

RAILGUN

======================================================================
*/

void QuakeLogic::RailgunFire(const std::shared_ptr<PlayerActor>& player,
	const Vector3<float>& muzzle, const Vector3<float>& forward)
{
	Vector3<float> end = muzzle + forward * 8192.f * 16.f;

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "railgf1a"; //art/quake/audio/sound/weapons/railgun/railgf1a.ogg
	PlaySound(sound, params, true);

	std::vector<ActorId> collisionActors;
	std::vector<Vector3<float>> collisions, collisionNormals;
	mPhysics->CastRay(muzzle, end, collisionActors, collisions, collisionNormals);

	ActorId closestCollisionId = INVALID_ACTOR_ID;
	Vector3<float> closestCollision = NULL;
	for (unsigned int i = 0; i < collisionActors.size(); i++)
	{
		if (collisionActors[i] != player->GetId())
		{
			if (closestCollision != NULL)
			{
				if (Length(closestCollision - muzzle) > Length(collisions[i] - muzzle))
				{
					closestCollisionId = collisionActors[i];
					closestCollision = collisions[i];
				}
			}
			else
			{
				closestCollisionId = collisionActors[i];
				closestCollision = collisions[i];
			}
		}
	}

	if (closestCollision != NULL)
	{
		Vector3<float> direction = closestCollision - muzzle;
		float scale = Length(direction);
		Normalize(direction);

		Matrix4x4<float> yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), atan2(direction[1], direction[0])));
		Matrix4x4<float> pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), -asin(direction[2])));

		Transform initTransform;
		initTransform.SetRotation(yawRotation * pitchRotation);
		initTransform.SetScale(Vector3<float>{scale, 4.f, 4.f});
		initTransform.SetTranslation(muzzle + (closestCollision - muzzle) / 2.f);
		CreateActor("actors/quake/effects/railgunfire.xml", nullptr, &initTransform);

		if (closestCollisionId != INVALID_ACTOR_ID &&
			std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]))
		{
			std::shared_ptr<PlayerActor> target =
				std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]);
			if (LogAccuracyHit(target, player))
				player->GetState().accuracyHits++;

			initTransform.MakeIdentity();
			initTransform.SetTranslation(closestCollision);
			CreateActor("actors/quake/effects/bleed.xml", nullptr, &initTransform);

			int damage = 100;
			Damage(damage, 0, MOD_RAILGUN, forward, closestCollision, target, player, player);
		}
	}
}


/*
======================================================================

LIGHTNING GUN

======================================================================
*/

void QuakeLogic::LightningFire(const std::shared_ptr<PlayerActor>& player,
	const Vector3<float>& muzzle, const Vector3<float>& forward)
{
	Vector3<float> end = muzzle + forward * (float)LIGHTNING_RANGE;

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "lg_hum"; //art/quake/audio/sound/weapons/lightning/lg_hum.ogg
	PlaySound(sound, params, true);

	std::vector<ActorId> collisionActors;
	std::vector<Vector3<float>> collisions, collisionNormals;
	mPhysics->CastRay(muzzle, end, collisionActors, collisions, collisionNormals);

	ActorId closestCollisionId = INVALID_ACTOR_ID;
	Vector3<float> closestCollision = end;
	for (unsigned int i = 0; i < collisionActors.size(); i++)
	{
		if (collisionActors[i] != player->GetId())
		{
			if (closestCollision != NULL)
			{
				if (Length(closestCollision - muzzle) > Length(collisions[i] - muzzle))
				{
					closestCollisionId = collisionActors[i];
					closestCollision = collisions[i];
				}
			}
			else
			{
				closestCollisionId = collisionActors[i];
				closestCollision = collisions[i];
			}
		}
	}

	if (closestCollision != NULL)
	{
		Vector3<float> direction = closestCollision - muzzle;
		float scale = Length(direction);
		Normalize(direction);

		Matrix4x4<float> yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), atan2(direction[1], direction[0])));
		Matrix4x4<float> pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), -asin(direction[2])));

		Transform initTransform;
		initTransform.SetRotation(yawRotation * pitchRotation);
		initTransform.SetScale(Vector3<float>{scale, 4.f, 4.f});
		initTransform.SetTranslation(muzzle + (closestCollision - muzzle) / 2.f);
		CreateActor("actors/quake/effects/lightningfire.xml", nullptr, &initTransform);

		if (closestCollisionId != INVALID_ACTOR_ID &&
			std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]))
		{
			std::shared_ptr<PlayerActor> target =
				std::dynamic_pointer_cast<PlayerActor>(mActors[closestCollisionId]);
			if (LogAccuracyHit(target, player))
				player->GetState().accuracyHits++;

			initTransform.MakeIdentity();
			initTransform.SetTranslation(closestCollision);
			CreateActor("actors/quake/effects/bleed.xml", nullptr, &initTransform);

			int damage = 6;
			Damage(damage, 0, MOD_LIGHTNING, forward, closestCollision, target, player, player);
		}
	}
}

void QuakeLogic::FireWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<QuakeEventDataFireWeapon> pCastEventData =
		std::static_pointer_cast<QuakeEventDataFireWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));

	// track shots taken for accuracy tracking. gauntet is just not tracked
	if (pPlayerActor->GetState().weapon != WP_GAUNTLET)
		pPlayerActor->GetState().accuracyShots++;

	// set aiming directions
	Vector3<float> origin;
	Matrix4x4<float> rotation;
	EulerAngles<float> viewAngles;
	std::shared_ptr<TransformComponent> pTransformComponent(
		pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
	if (pTransformComponent)
	{
		viewAngles.mAxis[1] = 1;
		viewAngles.mAxis[2] = 2;
		pTransformComponent->GetTransform().GetRotation(viewAngles);
		origin = pTransformComponent->GetTransform().GetTranslation();
		Matrix4x4<float> yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), viewAngles.mAngle[2]));
		Matrix4x4<float> pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), viewAngles.mAngle[1]));

		rotation = yawRotation * pitchRotation;
	}
	Vector3<float> forward = HProject(rotation * Vector4<float>::Unit(AXIS_X));
	Vector3<float> right = HProject(rotation * Vector4<float>::Unit(AXIS_Z));
	Vector3<float> up = HProject(rotation * Vector4<float>::Unit(AXIS_Y));

	//set muzzle location relative to pivoting eye
	Vector3<float> muzzle = origin;
	muzzle += up * (float)pPlayerActor->GetState().viewHeight;
	muzzle += forward * 10.f;
	muzzle -= right * 11.f;

	// fire the specific weapon
	switch (pPlayerActor->GetState().weapon)
	{
		case WP_GAUNTLET:
			GauntletAttack(pPlayerActor, muzzle, forward);
			break;
		case WP_SHOTGUN:
			ShotgunFire(pPlayerActor, muzzle, forward, right, up);
			break;
		case WP_MACHINEGUN:
			BulletFire(pPlayerActor, muzzle, forward, right, up, MACHINEGUN_SPREAD, MACHINEGUN_DAMAGE);
			break;
		case WP_GRENADE_LAUNCHER:
			GrenadeLauncherFire(pPlayerActor, muzzle, forward, viewAngles);
			break;
		case WP_ROCKET_LAUNCHER:
			RocketLauncherFire(pPlayerActor, muzzle, forward, viewAngles);
			break;
		case WP_PLASMAGUN:
			PlasmagunFire(pPlayerActor, muzzle, forward, viewAngles);
			break;
		case WP_RAILGUN:
			RailgunFire(pPlayerActor, muzzle, forward);
			break;
		case WP_LIGHTNING:
			LightningFire(pPlayerActor, muzzle, forward);
			break;
		default:
			// FIXME Error( "Bad ent->state->weapon" );
			break;
	}
}


bool QuakeLogic::SpotTelefrag(const std::shared_ptr<Actor>& spot)
{
	for (ActorMap::const_iterator it = mActors.begin(); it != mActors.end(); ++it)
	{
		std::shared_ptr<PlayerActor> playerActor = 
			std::dynamic_pointer_cast<PlayerActor>((*it).second);
		if (playerActor)
		{
			std::shared_ptr<TransformComponent> pTransformComponent(
				spot->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				Vector3<float> location = pTransformComponent->GetTransform().GetTranslation();
				if (mPhysics->FindIntersection(playerActor->GetId(), location))
					return true;
			}
		}
	}
	return false;
}

void QuakeLogic::SelectNearestSpawnPoint(const Vector3<float>& from, std::shared_ptr<Actor>& nearestSpot)
{
	float nearestDist = 999999;
	std::shared_ptr<Actor> spot = NULL;
	for (ActorMap::const_iterator it = mActors.begin(); it != mActors.end(); ++it)
	{
		spot = (*it).second;
		if (spot->GetComponent<LocationTarget>(LocationTarget::Name).lock())
		{
			std::shared_ptr<TransformComponent> pTransformComponent(
				spot->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				Vector3<float> delta = pTransformComponent->GetPosition() - from;
				float dist = Length(delta);
				if (dist < nearestDist)
				{
					nearestDist = dist;
					nearestSpot = spot;
				}
			}
		}
	}
}

#define	MAX_SPAWN_POINTS	128
void QuakeLogic::SelectRandomSpawnPoint(std::shared_ptr<Actor>& spot)
{
	Transform transform;
	std::shared_ptr<Actor> spots[MAX_SPAWN_POINTS];

	int count = 0;
	for (ActorMap::const_iterator it = mActors.begin(); it != mActors.end(); ++it)
	{
		spot = (*it).second;
		if (spot->GetComponent<LocationTarget>(LocationTarget::Name).lock())
		{
			if (SpotTelefrag(spot))
				continue;

			spots[count] = spot;
			count++;
		}
		else spot = NULL;
	}

	if (count)
	{
		int selection = Randomizer::Rand() % count;
		spot = spots[selection];
	}
}

void QuakeLogic::SelectRandomFurthestSpawnPoint(const Vector3<float>& avoidPoint, Transform& transform)
{
	float dists[64];

	int numSpots = 0;
	std::shared_ptr<Actor> spot = NULL;
	std::shared_ptr<Actor> spots[64];
	for (ActorMap::const_iterator it = mActors.begin(); it != mActors.end(); ++it)
	{
		spot = (*it).second;
		if (spot->GetComponent<LocationTarget>(LocationTarget::Name).lock())
		{
			if (SpotTelefrag(spot))
				continue;

			std::shared_ptr<TransformComponent> pTransformComponent(
				spot->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				Vector3<float> location = pTransformComponent->GetTransform().GetTranslation();
				Vector3<float> delta = location - avoidPoint;
				float dist = Length(delta);
				int i;
				for (i = 0; i < numSpots; i++)
				{
					if (dist > dists[i])
					{
						if (numSpots >= 64)
							numSpots = 64 - 1;
						for (int j = numSpots; j > i; j--)
						{
							dists[j] = dists[j - 1];
							spots[j] = spots[j - 1];
						}
						dists[i] = dist;
						spots[i] = spot;
						numSpots++;
						if (numSpots > 64)
							numSpots = 64;
						break;
					}
				}
				if (i >= numSpots && numSpots < 64)
				{
					dists[numSpots] = dist;
					spots[numSpots] = spot;
					numSpots++;
				}
			}
		}
		else spot = NULL;
	}
	if (!numSpots)
	{
		if (!spot)
			LogError("Couldn't find a spawn point");

		SelectSpawnPoint(Vector3<float>::Zero(), transform);
	}
	else
	{
		// select a random spot from the spawn points furthest away
		int rnd = (int)(((Randomizer::Rand() & 0x7fff) / (float)0x7fff) * (numSpots / 2));

		std::shared_ptr<TransformComponent> pTransformComponent(
			spots[rnd]->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
		{
			transform.SetTranslation(pTransformComponent->GetTransform().GetTranslation());
			transform.SetRotation(pTransformComponent->GetTransform().GetRotation());
		}
	}
}

void QuakeLogic::SelectSpawnPoint(const Vector3<float>& avoidPoint, Transform& transform)
{
	return SelectRandomFurthestSpawnPoint(avoidPoint, transform);
}

void QuakeLogic::SelectInitialSpawnPoint(Transform& transform)
{
	std::shared_ptr<Actor> spot = NULL;
	for (ActorMap::const_iterator it = mActors.begin(); it != mActors.end(); ++it)
	{
		spot = (*it).second;
		if (spot->GetComponent<LocationTarget>(LocationTarget::Name).lock())
		{
			if (SpotTelefrag(spot))
			{
				SelectSpawnPoint(Vector3<float>::Zero(), transform);
				return;
			}
			break;
		}
		else spot = NULL;
	}

	if (spot)
	{
		std::shared_ptr<TransformComponent> pTransformComponent(
			spot->GetComponent<TransformComponent>(TransformComponent::Name).lock());
		if (pTransformComponent)
		{
			transform.SetTranslation(pTransformComponent->GetTransform().GetTranslation());
			transform.SetRotation(pTransformComponent->GetTransform().GetRotation());
		}
	}
	else SelectSpawnPoint(Vector3<float>::Zero(), transform);
}

int QuakeLogic::PickupAmmo(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<AmmoPickup>& ammo)
{
	player->GetState().ammo[ammo->GetCode()] += ammo->GetAmount();
	if (player->GetState().ammo[ammo->GetCode()] > 200)
		player->GetState().ammo[ammo->GetCode()] = 200;
	mStatBars->EventHandler(player, "ammo_changed");

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "am_pkup"; //art/quake/audio/sound/misc/am_pkup.wav
	PlaySound(sound, params, true);

	return ammo->GetWait();
}

int QuakeLogic::PickupWeapon(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<WeaponPickup>& weapon)
{
	// add the weapon
	player->GetState().stats[STAT_WEAPONS] |= (1 << weapon->GetCode());

	// add ammo
	player->GetState().ammo[weapon->GetCode()] += weapon->GetAmmo();
	if (player->GetState().ammo[weapon->GetCode()] > 200)
		player->GetState().ammo[weapon->GetCode()] = 200;
	mStatBars->EventHandler(player, "ammo_changed");

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	SoundParams params;
	params.type = SoundParams::SP_POSITIONAL;
	params.position = cameraTransform.GetTranslation();
	SimpleSound sound;
	sound.name = "w_pkup"; //art/quake/audio/sound/misc/w_pkup.wav
	PlaySound(sound, params, true);

	return weapon->GetWait();
}

int QuakeLogic::PickupHealth(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<HealthPickup>& health)
{
	int max;
	if (health->GetAmount() != 5 && health->GetAmount() != 100)
		max = player->GetState().stats[STAT_MAX_HEALTH];
	else
		max = player->GetState().stats[STAT_MAX_HEALTH] * 2;

	player->GetState().stats[STAT_HEALTH] += health->GetAmount();
	if (player->GetState().stats[STAT_HEALTH] > max)
		player->GetState().stats[STAT_HEALTH] = max;
	mStatBars->EventHandler(player, "health_changed");

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	// play health pickup sound
	if (health->GetCode() == 1)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "n_health"; //art/quake/audio/sound/items/n_health.wav
		PlaySound(sound, params, true);
	}
	else if (health->GetCode() == 2)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "l_health"; //art/quake/audio/sound/items/l_health.wav
		PlaySound(sound, params, true);
	}
	else if (health->GetCode() == 3)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "m_health"; //art/quake/audio/sound/items/m_health.wav
		PlaySound(sound, params, true);
	}
	else if (health->GetCode() == 4)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "s_health"; //art/quake/audio/sound/items/s_health.wav
		PlaySound(sound, params, true);
	}

	return health->GetWait();
}

int QuakeLogic::PickupArmor(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<ArmorPickup>& armor)
{
	player->GetState().stats[STAT_ARMOR] += armor->GetAmount();
	if (player->GetState().stats[STAT_ARMOR] > player->GetState().stats[STAT_MAX_HEALTH] * 2)
		player->GetState().stats[STAT_ARMOR] = player->GetState().stats[STAT_MAX_HEALTH] * 2;
	mStatBars->EventHandler(player, "armor_changed");

	std::shared_ptr<TransformComponent> pPlayerTransform(
		player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

	std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mCamera;
	Transform cameraTransform = camera->GetAbsoluteTransform();

	// play armor pickup sound
	if (armor->GetCode() == 1)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "ar2_pkup"; //art/quake/audio/sound/misc/ar2_pkup.wav
		PlaySound(sound, params, true);
	}
	else if (armor->GetCode() == 2)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "ar2_pkup"; //art/quake/audio/sound/misc/ar2_pkup.wav
		PlaySound(sound, params, true);
	}
	else if (armor->GetCode() == 3)
	{
		SoundParams params;
		params.type = SoundParams::SP_POSITIONAL;
		params.position = cameraTransform.GetTranslation();
		SimpleSound sound;
		sound.name = "ar1_pkup"; //art/quake/audio/sound/misc/ar1_pkup.wav
		PlaySound(sound, params, true);
	}

	return armor->GetWait();
}

/*
CanItemBeGrabbed
Returns false if the item should not be picked up.
*/
bool QuakeLogic::CanItemBeGrabbed(const std::shared_ptr<Actor>& item, const std::shared_ptr<PlayerActor>& player)
{
	if (item->GetType() == "Ammo")
	{
		std::shared_ptr<AmmoPickup> pAmmoPickup = 
			item->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
		if (pAmmoPickup)
		{
			if (player->GetState().ammo[pAmmoPickup->GetCode()] >= 200)
				return false;		// can't hold any more

			return true;
		}
	}
	else if (item->GetType() == "Armor")
	{
		if (player->GetState().stats[STAT_ARMOR] >= player->GetState().stats[STAT_MAX_HEALTH] * 2)
			return false;

		return true;
	}
	else if (item->GetType() == "Health")
	{
		// small and mega healths will go over the max, otherwise
		// don't pick up if already at max
		std::shared_ptr<HealthPickup> pHealthPickup =
			item->GetComponent<HealthPickup>(HealthPickup::Name).lock();
		if (pHealthPickup)
		{
			if (pHealthPickup->GetAmount() == 5 || pHealthPickup->GetAmount() == 100)
			{
				if (player->GetState().stats[STAT_HEALTH] >= player->GetState().stats[STAT_MAX_HEALTH] * 2)
					return false;

				return true;
			}

			if (player->GetState().stats[STAT_HEALTH] >= player->GetState().stats[STAT_MAX_HEALTH])
				return false;

			return true;
		}
	}
	else if (item->GetType() == "Weapon")
	{
		return true;	// weapons are always picked up
	}
	else
	{ 
		LogError("Unknown type " + item->GetType());
	}

	return false;
}

// Quake Actors
std::shared_ptr<Actor> QuakeLogic::GetRandomActor()
{
	std::vector<std::shared_ptr<Actor>> actors;
	GetAmmoActors(actors);
	GetWeaponActors(actors);
	GetHealthActors(actors);
	GetArmorActors(actors);

	int selection = Randomizer::Rand() % actors.size();
	return actors[selection];
}

void QuakeLogic::GetAmmoActors(std::vector<std::shared_ptr<Actor>>& ammo)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Ammo")
			ammo.push_back(pActor);
	}
}

void QuakeLogic::GetArmorActors(std::vector<std::shared_ptr<Actor>>& armor)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Armor")
			armor.push_back(pActor);
	}
}

void QuakeLogic::GetWeaponActors(std::vector<std::shared_ptr<Actor>>& weapon)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Weapon")
			weapon.push_back(pActor);
	}
}

void QuakeLogic::GetHealthActors(std::vector<std::shared_ptr<Actor>>& health)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Health")
			health.push_back(pActor);
	}
}

void QuakeLogic::GetPlayerActors(std::vector<std::shared_ptr<PlayerActor>>& player)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Player")
			player.push_back(std::dynamic_pointer_cast<PlayerActor>(pActor));
	}
}

void QuakeLogic::GetTriggerActors(std::vector<std::shared_ptr<Actor>>& trigger)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Trigger")
			trigger.push_back(pActor);
	}
}

void QuakeLogic::GetTargetActors(std::vector<std::shared_ptr<Actor>>& target)
{
	for (auto actor : mActors)
	{
		std::shared_ptr<Actor> pActor = actor.second;
		if (pActor->GetType() == "Target")
			target.push_back(pActor);
	}
}

void QuakeLogic::SendShowFormMessage(ActorId actorId, const std::string& form, const std::string& formName)
{
	if (form.empty())
	{
		//the visual should close the form
		//but make sure there wasn't another one open in meantime
		const auto it = mFormStateData.find(actorId);
		if (it != mFormStateData.end() && it->second == formName)
			mFormStateData.erase(actorId);
	}
	else mFormStateData[actorId] = formName;

	EventManager::Get()->QueueEvent(std::make_shared<QuakeEventDataShowForm>(form, formName));
}

int QuakeLogic::NextSoundId()
{
	int ret = mNextSoundId;
	if (mNextSoundId == 0x7FFFFFFF)
		mNextSoundId = 0; // signed overflow is undefined
	else
		mNextSoundId++;
	return ret;
}

int QuakeLogic::PlaySound(const SimpleSound& sound, const SoundParams& params, bool ephemeral)
{
	// Find out initial position of sound
	bool posExists = false;
	Vector3<float> pos = params.GetPosition(&posExists);
	// If position is not found while it should be, cancel sound
	if (posExists != (params.type != SoundParams::SP_LOCAL))
		return -1;

	// Filter destination clients
	std::vector<ActorId> dstActors;
	if (params.toPlayer)
	{
		std::shared_ptr<Actor> player = GetActor(params.toPlayer).lock();
		if (!player)
		{
			LogInformation("Player \"" + std::to_string(params.toPlayer) + "\" not found");
			return -1;
		}
		if (player->GetId() == INVALID_ACTOR_ID)
		{
			LogInformation("Player \"" + std::to_string(params.toPlayer) + "\" not connected");
			return -1;
		}
		dstActors.push_back(player->GetId());
	}
	else
	{
		std::vector<ActorId> players;
		for (const ActorId playerId : players)
		{
			std::shared_ptr<Actor> player = GetActor(playerId).lock();
			if (!player)
				continue;

			if (params.excludePlayer && params.excludePlayer == playerId)
				continue;

			std::shared_ptr<PlayerActor> playerActor = std::static_pointer_cast<PlayerActor>(player);
			if (!playerActor)
				continue;

			if (posExists)
			{
				std::shared_ptr<TransformComponent> pPlayerTransform(
					playerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
				if (Length(pPlayerTransform->GetPosition() - pos) > params.maxHearDistance)
					continue;
			}
			dstActors.push_back(playerId);
		}
	}

	if (dstActors.empty())
		return -1;

	// Create the sound
	int id;
	SoundPlaying* pSound = nullptr;
	if (!ephemeral)
	{
		id = NextSoundId();
		// The sound will exist as a reference in mPlayingSounds
		mPlayingSounds[id] = SoundPlaying();
		pSound = &mPlayingSounds[id];
		pSound->params = params;
		pSound->sound = sound;
	}
	else
	{
		id = -1; // old visuals will still use this, so pick a reserved ID
	}

	float gain = params.gain * sound.gain;
	bool asReliable = !ephemeral;

	for (const uint16_t dstActor : dstActors)
		if (pSound)
			pSound->actors.insert(dstActor);

	EventManager::Get()->TriggerEvent(std::make_shared<QuakeEventDataPlaySoundType>(id, sound.name,
		params.type, pos, params.object, gain, params.fade, params.pitch, ephemeral, params.loop));
	return id;
}

void QuakeLogic::StopSound(int handle)
{
	// Get sound reference
	std::unordered_map<int, SoundPlaying>::iterator itSound = mPlayingSounds.find(handle);
	if (itSound == mPlayingSounds.end())
		return;

	//Remove sound reference
	mPlayingSounds.erase(itSound);

	EventManager::Get()->TriggerEvent(std::make_shared<QuakeEventDataStopSound>(handle));
}

void QuakeLogic::RemoveSounds(const std::vector<int>& soundList)
{
	for (int soundId : soundList)
		mPlayingSounds.erase(soundId);
}

void QuakeLogic::FadeSound(int handle, float step, float gain)
{
	// Get sound reference
	std::unordered_map<int, SoundPlaying>::iterator itSound = mPlayingSounds.find(handle);
	if (itSound == mPlayingSounds.end())
		return;

	SoundPlaying& pSound = itSound->second;
	pSound.params.gain = gain;

	// Backwards compability
	bool playSound = gain > 0;
	SoundPlaying compatPlaySound = pSound;
	compatPlaySound.actors.clear();

	for (std::unordered_set<ActorId>::iterator it = pSound.actors.begin(); it != pSound.actors.end();)
	{
		uint16_t protoVersion = 39;
		if (protoVersion >= 32)
		{
			// Send as reliable
			EventManager::Get()->TriggerEvent(std::make_shared<QuakeEventDataStopSound>(*it));

			++it;
		}
		else
		{
			compatPlaySound.actors.insert(*it);

			pSound.actors.erase(it++);

			// Stop old sound
			EventManager::Get()->TriggerEvent(std::make_shared<QuakeEventDataStopSound>(*it));
		}
	}

	EventManager::Get()->TriggerEvent(std::make_shared<QuakeEventDataFadeSound>(handle, step, gain));

	// Remove sound reference
	if (!playSound || pSound.actors.empty())
		mPlayingSounds.erase(itSound);

	if (playSound && !compatPlaySound.actors.empty())
	{
		// Play new sound volume on older clients
		PlaySound(compatPlaySound.sound, compatPlaySound.params);
	}
}