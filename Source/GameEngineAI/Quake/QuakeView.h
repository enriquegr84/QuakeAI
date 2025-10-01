//========================================================================
// QuakeView.h : Game View Class
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

#ifndef QUAKEVIEW_H
#define QUAKEVIEW_H

#include "QuakeStd.h"
#include "QuakeEvents.h"

#include "Games/Forms/Menu/BaseMenu.h"

#include "Graphics/Hud.h"
#include "Graphics/ProfilerGraph.h"

#include "Graphics/GUI/UIKeyChange.h"
#include "Graphics/GUI/UIVolumeChange.h"

#include "AI/Pathing.h"

#include "Core/Utility/Chat.h"
#include "Core/Utility/EnrichedString.h"

#include "Core/Event/EventManager.h"

#include "Audio/Sound.h"

#include "Game/Level/Level.h"
#include "Game/View/HumanView.h"

#include "Graphic/UI/Element/UIForm.h"
#include "Graphic/UI/Element/UIChatConsole.h"

#define SIZE_TAG "size[11,5.5,true]" // Fixed size on desktop

//! Macro for save Dropping an Element
#define DropElement(x)	if (x) { x->Remove(); x = 0; }

#define STAT_MINUS			10	// num frame for '-' stats digit

#define	NUM_CROSSHAIRS		10

//forwarding
class BaseEventManager;
class SoundProcess;

class QuakeHumanView;
class QuakePlayerController;
class QuakeCameraController;
class PlayerActor;
class Node;

enum FootStep 
{
	FOOTSTEP_NORMAL,
	FOOTSTEP_BOOT,
	FOOTSTEP_FLESH,
	FOOTSTEP_MECH,
	FOOTSTEP_ENERGY,
	FOOTSTEP_METAL,
	FOOTSTEP_SPLASH,

	FOOTSTEP_TOTAL
};

enum ImpactSound
{
	IMPACTSOUND_DEFAULT,
	IMPACTSOUND_METAL,
	IMPACTSOUND_FLESH
};

enum LeType
{
	LE_MARK,
	LE_EXPLOSION,
	LE_SPRITE_EXPLOSION,
	LE_FRAGMENT,
	LE_MOVE_SCALE_FADE,
	LE_FALL_SCALE_FADE,
	LE_FADE_RGB,
	LE_SCALE_FADE,
	LE_SCOREPLUM
};

enum LeFlag
{
	LEF_PUFF_DONT_SCALE = 0x0001,	// do not scale size over time
	LEF_TUMBLE = 0x0002,			// tumble over time, used for ejecting shells
	LEF_SOUND1 = 0x0004,			// sound 1 for kamikaze
	LEF_SOUND2 = 0x0008				// sound 2 for kamikaze
};

enum LeMarkType
{
	LEMT_NONE,
	LEMT_BURN,
	LEMT_BLOOD
};			// fragment local entities can leave marks on walls

enum LeBounceSoundType
{
	LEBS_NONE,
	LEBS_BLOOD,
	LEBS_BRASS
};	// fragment local entities can make sounds on impacts

struct Jitter
{
	float max, min, avg, counter, maxSample, minSample, maxFraction;
};

struct RunStats
{
	unsigned int drawTime;

	Jitter dTimeJitter, busyTimeJitter;
};

struct FpsControl
{
	unsigned int lastTime, busyTime, sleepTime;
};


typedef struct TextureUpdateArgs
{
	unsigned int lastTimeMs;
	unsigned int lastPercent;
	const wchar_t* textBase;

	std::shared_ptr<BlendState> blendState;
	std::shared_ptr<Visual> visual;
	std::shared_ptr<BaseUI> ui;

	Scene* scene;

} TextureUpdateArgs;

void DrawLoadScreen(const std::wstring& text, std::shared_ptr<BaseUI> ui,
	std::shared_ptr<Visual> visual, std::shared_ptr<BlendState> blendState, int percent);
void LimitFps(FpsControl* fpsTimings, float* dTime);

/** Specific implementation of OnDemandSoundFetcher */
class MenuSoundFetcher : public OnDemandSoundFetcher
{
public:
	/**
	 * get sound file paths according to sound name
	 * @param name sound name
	 * @param dst_paths receives possible paths to sound files
	 */
	void FetchSounds(const std::string& name, std::set<std::string>& dstPaths);

private:
	/** set of fetched sound names */
	std::set<std::string> mFetched;
};


// Locally stored sounds don't need to be preloaded because of this
class GameSoundFetcher : public OnDemandSoundFetcher
{
public:

	/**
	 * get sound file paths according to sound name
	 * @param name sound name
	 * @param dst_paths receives possible paths to sound files
	 */
	void FetchSounds(const std::string& name, std::set<std::string>& dstPaths);

private:

	void PathsInsert(std::set<std::string>& dstPaths,
		const std::string& base, const std::string& name);

	/** set of fetched sound names */
	std::set<std::string> mFetched;
};

class QuakeMainMenuUI : public BaseUI
{
protected:

	std::shared_ptr<BaseMenu> mMenu;
	std::shared_ptr<UIForm> mFormMenu;
	std::shared_ptr<FormSource> mFormSource;
	std::shared_ptr<TextDestination> mTextDestination;

public:
	QuakeMainMenuUI();
	virtual ~QuakeMainMenuUI();

	// IScreenElement Implementation
	virtual bool OnInit();
	virtual bool OnRestore();
	virtual bool OnLostDevice() { return true; }

	virtual void OnUpdate(int deltaMilliseconds) { }

	//! draws all gui elements
	virtual bool OnRender(double time, float elapsedTime);

	virtual bool OnEvent(const Event& evt);
	virtual bool OnMsgProc(const Event& evt);

	virtual int GetZOrder() const { return 1; }
	virtual void SetZOrder(int const zOrder) { }

	std::shared_ptr<BaseMenu> GetMenu() { return mMenu; }
	void SetMenu(std::shared_ptr<BaseMenu> menu) { mMenu = menu; }

	std::shared_ptr<UIForm> GetFormMenu() { return mFormMenu; }
	void SetFormMenu(std::shared_ptr<UIForm> formMenu) { mFormMenu = formMenu; }

	void ResetMenuUI();
	void UpdateMenuUI(std::wstring gamePath);
	bool SetMenuUI(std::wstring id, std::wstring gamePath);

	void ClearMenuUI(std::wstring id);
	bool SetGenericMenuUI(std::wstring id);
};


class QuakeMainMenuView : public HumanView
{

public:

	QuakeMainMenuView();
	~QuakeMainMenuView();

	virtual bool OnMsgProc(const Event& evt);
	virtual void RenderText();

	virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs);

	void OpenGameSelectionDelegate(BaseEventDataPtr pEventData);
	void ChangeGameSelectionDelegate(BaseEventDataPtr pEventData);

protected:

	void UpdateCameraRotation(const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const;

	std::shared_ptr<CloudSystemNode> mMenuCloud;
	std::shared_ptr<QuakeMainMenuUI> mMainMenuUI;

private:

	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);

	MenuSoundFetcher mSoundFetcher;
	std::shared_ptr<BaseSoundManager> mSoundMgr = nullptr;
};


class QuakeUI : public BaseUI
{
	friend class QuakeHumanView;

public:
	QuakeUI(const QuakeHumanView* view);
	virtual ~QuakeUI();

	// IScreenElement Implementation
	virtual bool OnInit();
	virtual bool OnRestore();
	virtual bool OnLostDevice() { return true; }

	virtual void OnUpdate(int deltaMilliseconds) { }

	//! draws all gui elements
	virtual bool OnRender(double time, float elapsedTime);

	virtual bool OnEvent(const Event& evt);
	virtual bool OnMsgProc(const Event& evt);

	virtual int GetZOrder() const { return 1; }
	virtual void SetZOrder(int const zOrder) { }

	void Update(const RunStats& stats, const std::shared_ptr<CameraNode> camera, 
		const std::shared_ptr<UIChatConsole> chatConsole, float dTime);

	inline void SetInfoText(const std::wstring& str) { mInfoText = str; }
	inline void ClearInfoText() { mInfoText.clear(); }

	inline void ShowStatusText(const std::wstring& str)
	{
		mStatusText = str;
		mStatusTextTime = 0.0f;
	}
	void ShowTranslatedStatusText(const char* str);
	inline void ClearStatusText() { mStatusText.clear(); }

	const bool IsChatVisible()
	{
		return mFlags.showChat && mRecentChatCount != 0 && mProfilerCurrentPage == 0;
	}
	void SetChatText(const EnrichedString& chatText, unsigned int recentChatCount);

	void UpdateProfiler();

	void ToggleChat();
	void ToggleHud();
	void ToggleProfiler();

	void ShowOverlayMessage(const std::wstring& text,
		float dTime, int percent, bool drawClouds = true);

	std::shared_ptr<BaseUIForm>& UpdateForm(const std::string& formName)
	{
		mFormName = formName;
		return mForm;
	}
	const std::string& GetFormName() { return mFormName; }
	std::shared_ptr<BaseUIForm>& GetForm() { return mForm; }
	void DeleteForm()
	{
		mForm.reset();
		mFormName.clear();
	}

protected:

	// Flags that can, or may, change during main game loop
	struct Flags
	{
		bool showChat = true;
		bool showHud = true;
		bool showDebug = true;
		bool showProfilerGraph = false;
	};

	bool IsMenuActive();
	void SetMenuActive(bool active);

	const QuakeHumanView* mGameView;

	Flags mFlags;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<BlendState> mBlendState;

	std::shared_ptr<BaseUIStaticText> mUIText;  // First line of debug text
	std::shared_ptr<BaseUIStaticText> mUIText2; // Second line of debug text

	std::shared_ptr<BaseUIStaticText> mUITextInfo; // At the middle of the screen
	std::wstring mInfoText;

	std::shared_ptr<BaseUIStaticText> mUITextStatus;
	std::wstring mStatusText;
	float mStatusTextTime = 0.0f;
	SColor mStatusTextInitialColor;

	std::shared_ptr<BaseUIStaticText> mUITextChat = nullptr; // Chat text
	unsigned int mRecentChatCount = 0;

	std::shared_ptr<BaseUIStaticText> mUITextProfiler = nullptr; // Profiler text
	unsigned char mProfilerCurrentPage = 0;
	const unsigned char mProfilerMaxPage = 3;

	// Default: "". If other than "": Empty ShowForm packets will only
	// close the form when the formName matches
	std::string mFormName;
	std::shared_ptr<BaseUIForm> mForm;

	std::shared_ptr<UIChatConsole> mUIChatConsole = nullptr;
	std::shared_ptr<Hud> mHud = nullptr;

	float mDamageFlash = 0;

	bool mIsMenuActive = false;
};


struct LocalFormHandler : public TextDestination
{
	LocalFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "PAUSE_MENU")
		{
			if (fields.find("btn_play_duel") != fields.end())
			{
				std::shared_ptr<EventDataPlayDuelCombat> pEvent(new EventDataPlayDuelCombat());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_sound") != fields.end())
			{
				std::shared_ptr<EventDataChangeVolume> pEvent(new EventDataChangeVolume());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_key_config") != fields.end())
			{
				std::shared_ptr<EventDataChangeMenu> pEvent(new EventDataChangeMenu());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_exit_menu") != fields.end())
			{
				//Disconnect();
				return;
			}

			return;
		}
	}
};

class SoundMaker
{
public:

	BaseSoundManager* mSound;

	bool mMakesFootstepSound;
	float mPlayerStepTimer;
	float mPlayerJumpTimer;

	SimpleSound mPlayerStepSound;
	SimpleSound mPlayerLeftPunchSound;
	SimpleSound mPlayerRightPunchSound;

	SoundMaker(BaseSoundManager* sound) :
		mSound(sound), mMakesFootstepSound(true),
		mPlayerStepTimer(0.0f), mPlayerJumpTimer(0.0f)
	{
	}

	void PlayPlayerStep()
	{
		if (mPlayerStepTimer <= 0 && mPlayerStepSound.Exists())
		{
			mPlayerStepTimer = 0.03f;
			if (mMakesFootstepSound)
				mSound->PlaySoundGlobal(mPlayerStepSound, false);
		}
	}

	void PlayPlayerJump()
	{
		if (mPlayerJumpTimer <= 0.0f)
		{
			mPlayerJumpTimer = 0.2f;
			mSound->PlaySoundGlobal(SimpleSound("player_jump", 0.5f), false);
		}
	}

	void Step(float dTime)
	{
		mPlayerStepTimer -= dTime;
		mPlayerJumpTimer -= dTime;
	}
};

// Locally stored sounds don't need to be preloaded because of this
class GameOnDemandSoundFetcher : public OnDemandSoundFetcher
{
	std::set<std::string> mFetched;

private:

	void PathsInsert(std::set<std::string>& dstPaths,
		const std::string& base, const std::string& name)
	{
		dstPaths.insert(base + "\\sounds\\" + name + ".ogg");
	}
public:
	void FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
	{
		if (mFetched.count(name))
			return;

		mFetched.insert(name);

		//PathsInsert(dstPaths, porting::path_share, name);
		//PathsInsert(dstPaths, porting::path_user, name);
	}
};


/****************************************************************************
 THE GAME
 ****************************************************************************/


struct GameSettings
{
	/*
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool doubletapJump = false;
	bool enableClouds = false;
	bool enableParticles = false;
	bool enableFog = false;
	bool enableNoclip = false;
	bool enableFreeMove = false;
	float mouseSensitivity = false;
	float repeatPlaceTime = false;
	float cameraSmoothing = false;
	float fogStart = false;

	const std::string settingNames[10] = {
		"doubletap_jump", "enable_clouds", "enable_particles", "enable_fog", "noclip",
		"free_move", "mouse_sensitivity", "repeat_place_time", "camera_smoothing", "fog_start"
	};
	void ReadGlobalSettings();
};

class QuakeHumanView : public HumanView
{

public:
	QuakeHumanView();
	virtual ~QuakeHumanView();

	virtual bool OnMsgProc(const Event& evt);
	virtual void RenderText();	

	virtual void OnRender(double time, float elapsedTime);
	virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs);
	virtual void OnAttach(GameViewId vid, ActorId aid);

	virtual void SetControlledActor(ActorId actorId);
	virtual bool LoadGameDelegate(tinyxml2::XMLElement* pLevelData) override;

	// event delegates
    void GameplayUiUpdateDelegate(BaseEventDataPtr pEventData);

	void InitChatDelegate(BaseEventDataPtr pEventData);
	void UpdateChatDelegate(BaseEventDataPtr pEventData);

	void ShowFormDelegate(BaseEventDataPtr pEventData);

	void HandlePlaySoundAtDelegate(BaseEventDataPtr pEventData);
	void HandlePlaySoundDelegate(BaseEventDataPtr pEventData);
	void HandleStopSoundDelegate(BaseEventDataPtr pEventData);
	void HandleFadeSoundDelegate(BaseEventDataPtr pEventData);

	void ChangeVolumeDelegate(BaseEventDataPtr pEventData);
	void ChangeMenuDelegate(BaseEventDataPtr pEventData);

	void HandleMediaDelegate(BaseEventDataPtr pEventData);

	void FireWeaponDelegate(BaseEventDataPtr pEventData);
	void ChangeWeaponDelegate(BaseEventDataPtr pEventData);

	void DeadActorDelegate(BaseEventDataPtr pEventData);
	void SpawnActorDelegate(BaseEventDataPtr pEventData);
	void JumpActorDelegate(BaseEventDataPtr pEventData);
	void MoveActorDelegate(BaseEventDataPtr pEventData);
	void FallActorDelegate(BaseEventDataPtr pEventData);
	void RotateActorDelegate(BaseEventDataPtr pEventData);

	void PlayDuelCombatDelegate(BaseEventDataPtr pEventData);

protected:

	bool InitSound();

	void OpenConsole(float scale, const wchar_t* line = NULL);
	void ToggleFreeMove();
	void ToggleFreeMoveAlt();
	void TogglePitchMove();
	void ToggleFast();
	void ToggleNoClip();
	void ToggleCinematic();
	void ToggleAutoforward();

	void ToggleFog();
	void ToggleDebug();

	void IncreaseViewRange();
	void DecreaseViewRange();
	void ToggleFullViewRange();
	void CheckZoomEnabled();

	// This is run by Thread and does the actual processing
	void Step(float dTime);

	void Shutdown();

	void ExtendedResourceCleanup();

	void ProcessPlayerInteraction(float dTime, bool showHud, bool showDebug);

	void UpdateSound(float dTime);
	void UpdateControllers(unsigned int timeMs, unsigned long deltaMs);

	static void SettingsChangedCallback(const std::string& settingName, void* data);

	float GetMouseWheel()
	{
		float wheel = mMouseWheel;
		mMouseWheel = 0;
		return wheel;
	}

	bool IsKeyDown(GameKeyType key) { return mKeyIsDown[mKeycache.keys[key]]; }

	// Checks whether a key was down and resets the state
	bool WasKeyDown(GameKeyType key)
	{
		bool b = mKeyWasDown[mKeycache.keys[key]];
		if (b)
			mKeyWasDown.Unset(mKeycache.keys[key]);
		return b;
	}

	// Checks whether a key was just pressed. State will be cleared
	// in the subsequent iteration of Game::processPlayerInteraction
	bool WasKeyPressed(GameKeyType key) { return mKeyWasPressed[mKeycache.keys[key]]; }

	// Checks whether a key was just released. State will be cleared
	// in the subsequent iteration of Game::processPlayerInteraction
	bool WasKeyReleased(GameKeyType key) { return mKeyWasReleased[mKeycache.keys[key]]; }

	bool CancelPressed()
	{
		int key = mKeycache.Find(EscapeKey);
		return WasKeyDown((GameKeyType)key);
	}

	void ClearWasKeyPressed() { mKeyWasPressed.Clear(); }

	void ClearWasKeyReleased() { mKeyWasReleased.Clear(); }

	void ClearInput()
	{
		mMouseWheel = 0;

		mKeyIsDown.Clear();
		mKeyWasDown.Clear();
		mKeyWasPressed.Clear();
		mKeyWasReleased.Clear();
	}

	bool  mShowUI;					// If true, it renders the UI control text
	DebugMode mDebugMode;
	std::string mGameplayText;

	std::shared_ptr<QuakePlayerController> mGamePlayerController;
	std::shared_ptr<QuakeCameraController> mGameCameraController;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<BlendState> mBlendState;

	std::shared_ptr<QuakeUI> mGameUI;
	std::shared_ptr<Node> mPlayer;

private:

	void ShowPauseMenu();

	bool GetGameContent();
	void AfterContentReceived();

	void UpdateProfilers(const RunStats& stats, const FpsControl& updateTimes, float dTime);
	void UpdateStats(RunStats* stats, const FpsControl& updateTimes, float dTime);

	void ProcessUserInput(float dTime);
	void ProcessKeyInput();

	void UpdateFrame(RunStats* stats, float dTime);

	// Insert a media file appropriately into the appropriate manager
	bool LoadMedia(const std::string& filePath, bool fromMediaPush = false);

	std::string GetModStoragePath() const;

    void RegisterAllDelegates(void);
    void RemoveAllDelegates(void);

	KeyCache mKeycache;

	// The current state of keys
	KeyList mKeyIsDown;

	// Like mKeyIsDown but only reset when that key is read
	KeyList mKeyWasDown;

	// Whether a key has just been pressed
	KeyList mKeyWasPressed;

	// Whether a key has just been released
	KeyList mKeyWasReleased;

	// mouse wheel state
	float mMouseWheel = 0;

	// Some timers
	float mAvgRTTTimer = 0.0f;
	float mPlayerPositionSendTimer = 0.0f;
	IntervalLimiter mMapTimerAndUnloadInterval;

	// Sounds
	GameSoundFetcher mSoundFetcher;
	std::shared_ptr<BaseSoundManager> mSoundMgr;
	std::shared_ptr<SoundMaker> mSoundMaker;

	// The authentication methods we can use to enter sudo mode (=change password)
	unsigned int mSudoAuthMethods;

	// The seed returned by the logic is stored here
	uint64_t mMapSeed = 0;

	// Pre-calculated values
	int mCrackAnimationLength;

	IntervalLimiter mProfilerInterval;

	RunStats mStats;
	FpsControl mUpdateTimes;
	float mDeltaTime; // in seconds

	Vector2<unsigned int> mScreenSize;

	GameSettings mGameSettings;

	bool mInvertMouse = false;
	bool mFirstLoopAfterWindowActivation = false;
	bool mCameraOffsetChanged = false;

	bool mMediaReceived = false;

	// Sounds
	float mRemoveSoundsCheckTimer = 0.0f;
	// Mapping from logic sound ids to our sound ids
	std::unordered_map<int, int> mSoundsLogicToVisual;
	// And the other way!
	std::unordered_map<int, int> mSoundsVisualToLogic;
	// Relation of visual id to object id
	std::unordered_map<int, uint16_t> mSoundsToObjects;
};

#endif