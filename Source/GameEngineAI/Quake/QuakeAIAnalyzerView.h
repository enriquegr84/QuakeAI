//========================================================================
// QuakeAIAnalyzerView.h : Game View Class
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

#ifndef QUAKEAIANALYZERVIEW_H
#define QUAKEAIANALYZERVIEW_H

#include "QuakeStd.h"
#include "QuakeEvents.h"
#include "QuakeAIView.h"

#include "Graphics/Hud.h"
#include "Graphics/ProfilerGraph.h"

#include "Graphics/GUI/UIKeyChange.h"
#include "Graphics/GUI/UIVolumeChange.h"

#include "Editor/CameraController.h"

#define SIZE_TAG "size[11,5.5,true]" // Fixed size on desktop

//forwarding
class BaseEventManager;
class SoundProcess;

class GraphNode;
class PathNode;

class QuakePlayerController;
class QuakeCameraController;

namespace AIAnalyzer
{
	struct Jitter
	{
		float max, min, avg, counter, maxSample, minSample, maxFraction;
	};

	struct RunStats
	{
		std::string gameTime;

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
}

// Locally stored sounds don't need to be preloaded because of this
class AIAnalyzerSoundFetcher : public OnDemandSoundFetcher
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

class QuakeAIAnalyzerUI : public BaseUI
{
	friend class QuakeAIAnalyzerView;

public:
	QuakeAIAnalyzerUI(const QuakeAIAnalyzerView* view);
	virtual ~QuakeAIAnalyzerUI();

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

	void Update(const AIAnalyzer::RunStats& stats, const std::shared_ptr<CameraNode> camera, 
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
		bool showAnalysis = false;
		bool showProfilerGraph = false;
	};

	bool IsMenuActive();
	void SetMenuActive(bool active);

	const QuakeAIAnalyzerView* mAIAnalyzerView;

	Flags mFlags;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<BlendState> mBlendState;

	std::shared_ptr<BaseUIStaticText> mText;  // First line of debug text
	std::shared_ptr<BaseUIStaticText> mText2; // Second line of debug text

	std::shared_ptr<BaseUIStaticText> mTextInfo; // At the middle of the screen
	std::wstring mInfoText;

	std::shared_ptr<BaseUIStaticText> mTextStatus;
	std::wstring mStatusText;
	float mStatusTextTime = 0.0f;
	SColor mStatusTextInitialColor;

	std::shared_ptr<BaseUIStaticText> mTextChat = nullptr; // Chat text
	unsigned int mRecentChatCount = 0;

	std::shared_ptr<BaseUIStaticText> mTextProfiler = nullptr; // Profiler text
	unsigned char mProfilerCurrentPage = 0;
	const unsigned char mProfilerMaxPage = 3;

	// Default: "". If other than "": Empty ShowForm packets will only
	// close the form when the formName matches
	std::string mFormName;
	std::shared_ptr<BaseUIForm> mForm;

	std::shared_ptr<UIChatConsole> mChatConsole = nullptr;
	std::shared_ptr<Hud> mHud = nullptr;

	float mDamageFlash = 0;

	bool mIsMenuActive = false;
};

struct AIGameFormHandler : public TextDestination
{
	AIGameFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "SHOW_GAME")
		{
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				mGameFrame = atoi(fields.at("te_search").c_str());
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataShowGameState>(mGameFrame));
				return;
			}
			if (fields.find("scrbar") != fields.end())
			{
				std::string row = fields.at("scrbar");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CHG:") + 4));
					mGameFrame = atoi(content.c_str());
					EventManager::Get()->TriggerEvent(
						std::make_shared<EventDataShowGameState>(mGameFrame));
					return;
				}
			}
		}
	}

	unsigned short mGameFrame = 0;
};

struct AIGameSimulationFormHandler : public TextDestination
{
	AIGameSimulationFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "SHOW_SIMULATION")
		{
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				if (mTabIndex == 1)
				{
					mGameFrame = atoi(fields.at("te_search").c_str());
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataShowGameSimulation>(mGameFrame));
				}
				else
				{
					mSimulationFrame = atoi(fields.at("te_search").c_str());
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataShowGameSimulation>(mSimulationFrame));
				}
				return;
			}
			if (fields.find("scrbar") != fields.end())
			{
				std::string row = fields.at("scrbar");
				if (row.rfind("CHG:") != std::string::npos)
				{
					if (mTabIndex == 1)
					{
						std::string content = Trim(row.substr(row.rfind("CHG:") + 4));
						mGameFrame = atoi(content.c_str());
						EventManager::Get()->TriggerEvent(
							std::make_shared<EventDataShowGameSimulation>(mGameFrame));
					}
					else
					{
						std::string content = Trim(row.substr(row.rfind("CHG:") + 4));
						mSimulationFrame = atoi(content.c_str());
						EventManager::Get()->TriggerEvent(
							std::make_shared<EventDataShowGameSimulation>(mSimulationFrame));
					}
					return;
				}
			}
			if (fields.find("btn_back") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, mPlayerIndex, 
						mEvaluationCluster, mDecisionCluster, mEvaluationFilter, mDecisionFilter, mTabIndex));
				return;
			}
		}
	}

	unsigned short mGameFrame = 0;
	unsigned short mAnalysisFrame = 0;
	unsigned short mSimulationFrame = 0;
	unsigned short mPlayerIndex = 1;
	unsigned short mTabIndex = 1;
	std::string mDecisionCluster = std::string();
	std::string mEvaluationCluster = std::string();
	std::string mDecisionFilter = std::string();
	std::string mEvaluationFilter = std::string();
};

struct AIAnalysisFormHandler : public TextDestination
{
	AIAnalysisFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "ANALYZE_GAME")
		{
			if (fields.find("btn_mp_search_evaluation") != fields.end() && fields.find("te_search_evaluation") != fields.end())
			{
				mEvaluationCluster[mTabIndex - 1] = std::string();
				mEvaluationFilter[mTabIndex - 1] = fields.at("te_search_evaluation");
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
					mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1], 
					mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
				return;
			}
			if (fields.find("btn_mp_search_decision") != fields.end() && fields.find("te_search_decision") != fields.end())
			{
				mDecisionCluster[mTabIndex - 1] = std::string();
				mDecisionFilter[mTabIndex - 1] = fields.at("te_search_decision");
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
					mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
					mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
				return;
			}
			if (fields.find("te_search") != fields.end())
			{
				mAnalysisFrame = atoi(fields.at("te_search").c_str());
			}
			if (fields.find("scrbar") != fields.end())
			{
				std::string row = fields.at("scrbar");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CHG:") + 4));
					mAnalysisFrame = atoi(content.c_str());
					BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataChangeAnalysisFrame>(mAnalysisFrame));
					return;
				}
			}
			if (fields.find("btn_mp_search") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
						mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
						mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
				return;
			}
			if (fields.find("evaluation_cluster") != fields.end())
			{
				std::string row = fields.at("evaluation_cluster");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string rowNr = Trim(row.substr(row.rfind("CHG:") + 4));
					mEvaluationCluster[mTabIndex - 1] = rowNr.substr(0, rowNr.find(":"));
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
							mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
							mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
					return;
				}
			}
			if (fields.find("decision_cluster") != fields.end())
			{
				std::string row = fields.at("decision_cluster");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string rowNr = Trim(row.substr(row.rfind("CHG:") + 4));
					mDecisionCluster[mTabIndex - 1] = rowNr.substr(0, rowNr.find(":"));
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
							mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
							mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
					return;
				}
			}
			if (fields.find("maintab") != fields.end())
			{
				std::string row = fields.at("maintab");

				std::string content = Trim(row);
				mTabIndex = atoi(content.c_str());
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
						mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
						mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
				return;
			}
			if (fields.find("player") != fields.end())
			{
				std::string row = fields.at("player");

				std::string content = Trim(row);
				if (mPlayerIndex != atoi(content.c_str()))
				{
					mPlayerIndex = atoi(content.c_str());
					mDecisionCluster = { std::string(), std::string(), std::string(), std::string() };
					mEvaluationCluster = { std::string(), std::string(), std::string(), std::string() };
					mDecisionFilter = { std::string(), std::string(), std::string(), std::string() };
					mEvaluationFilter = { std::string(), std::string(), std::string(), std::string() };
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataAnalyzeAIGame>(mGameFrame, mAnalysisFrame, 
							mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
							mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
					return;
				}
			}
			if (fields.find("btn_simulate") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataShowAIGameAnalysis>(mGameFrame, mAnalysisFrame, 
						mPlayerIndex, mEvaluationCluster[mTabIndex - 1], mDecisionCluster[mTabIndex - 1],
						mEvaluationFilter[mTabIndex - 1], mDecisionFilter[mTabIndex - 1], mTabIndex));
				return;
			}
		}
	}

	unsigned short mGameFrame = 0;
	unsigned short mAnalysisFrame = 0;
	unsigned short mPlayerIndex = 1;
	unsigned short mTabIndex = 1;

	std::vector<std::string> mDecisionCluster = { std::string(), std::string(), std::string(), std::string(), std::string() };
	std::vector<std::string> mEvaluationCluster = { std::string(), std::string(), std::string(), std::string(), std::string() };
	std::vector<std::string> mDecisionFilter = { std::string(), std::string(), std::string(), std::string(), std::string() };
	std::vector<std::string> mEvaluationFilter = { std::string(), std::string(), std::string(), std::string(), std::string() };
};

struct AIAnalyzerFormHandler : public TextDestination
{
	AIAnalyzerFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "PAUSE_MENU")
		{
			if (fields.find("btn_simulate_game") != fields.end())
			{
				std::shared_ptr<EventDataSimulateAIGame> pEvent(new EventDataSimulateAIGame());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_analyze_game") != fields.end())
			{
				std::shared_ptr<EventDataAnalyzeAIGame> pEvent(new EventDataAnalyzeAIGame());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_show_game") != fields.end())
			{
				std::shared_ptr<EventDataShowAIGame> pEvent(new EventDataShowAIGame());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_save_game") != fields.end())
			{
				std::shared_ptr<EventDataSaveAIGame> pEvent(new EventDataSaveAIGame());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_exit_menu") != fields.end())
			{
				//Exit();
				return;
			}

			return;
		}
	}
};

class AnalyzerSoundMaker
{
public:

	BaseSoundManager* mSound;

	bool mMakesFootstepSound;
	float mPlayerStepTimer;
	float mPlayerJumpTimer;

	SimpleSound mPlayerStepSound;
	SimpleSound mPlayerLeftPunchSound;
	SimpleSound mPlayerRightPunchSound;

	AnalyzerSoundMaker(BaseSoundManager* sound) :
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
class AIAnalyzerOnDemandSoundFetcher : public OnDemandSoundFetcher
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

struct AIAnalyzerSettings
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

class QuakeAIAnalyzerView : public HumanView
{

public:
	QuakeAIAnalyzerView();
	virtual ~QuakeAIAnalyzerView();

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

	void ChangeAnalysisFrameDelegate(BaseEventDataPtr pEventData);
	void ShowGameSimulationDelegate(BaseEventDataPtr pEventData);
	void ShowGameStateDelegate(BaseEventDataPtr pEventData);

	void SimulateAIGameDelegate(BaseEventDataPtr pEventData);
	void AnalyzeAIGameDelegate(BaseEventDataPtr pEventData);

	void ShowAIGameDelegate(BaseEventDataPtr pEventData);
	void ShowAIGameAnalysisDelegate(BaseEventDataPtr pEventData);

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

	void UpdateGameAIAnalysis(unsigned short tabIndex, unsigned short analysisFrame);

	void UpdateGameAISimulation(unsigned short frame);
	void UpdateGameAIState();

	bool  mShowUI;					// If true, it renders the UI control text
	DebugMode mDebugMode;
	std::string mGameplayText;

	std::shared_ptr<QuakePlayerController> mPlayerController;
	std::shared_ptr<CameraController> mCameraController;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<BlendState> mBlendState;

	std::shared_ptr<QuakeAIAnalyzerUI> mUI;
	std::shared_ptr<Node> mPlayer;
	std::shared_ptr<Node> mHighlightNode;

private:

	void ShowPauseMenu();

	void ShowAIGame(const AIGame::Game& game);
	void ShowAIGameAnalysis(unsigned short tabIndex, 
		unsigned short gameFrame, unsigned short analysisFrame, unsigned short playerIndex,
		const std::string& decisionCluster, const std::string& evaluationCluster,
		const std::string& decisionFilter, const std::string& evaluationFilter);

	void AnalyzeAIGame(unsigned short tabIndex, 
		unsigned short gameFrame, unsigned short analysisFrame, unsigned short playerIndex,
		const std::string& decisionCluster, const std::string& evaluationCluster,
		const std::string& decisionFilter, const std::string& evaluationFilter);

	bool GetGameContent();
	void AfterContentReceived();

	void UpdateProfilers(const AIAnalyzer::RunStats& stats, const AIAnalyzer::FpsControl& updateTimes, float dTime);
	void UpdateStats(AIAnalyzer::RunStats* stats, const AIAnalyzer::FpsControl& updateTimes, float dTime);

	void ProcessUserInput(float dTime);
	void ProcessKeyInput();

	void UpdateFrame(AIAnalyzer::RunStats* stats, float dTime);

	// Insert a media file appropriately into the appropriate manager
	bool LoadMedia(const std::string& filePath, bool fromMediaPush = false);

	std::string GetModStoragePath() const;

    void RegisterAllDelegates(void);
    void RemoveAllDelegates(void);

	void PrintPlayerItems(const std::unordered_map<ActorId, float>& items, std::stringstream& output);

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
	AIAnalyzerSoundFetcher mSoundFetcher;
	std::shared_ptr<BaseSoundManager> mSoundMgr;
	std::shared_ptr<AnalyzerSoundMaker> mSoundMaker;

	// The authentication methods we can use to enter sudo mode (=change password)
	unsigned int mSudoAuthMethods;

	// The seed returned by the logic is stored here
	uint64_t mMapSeed = 0;

	// Pre-calculated values
	int mCrackAnimationLength;

	IntervalLimiter mProfilerInterval;

	AIAnalyzer::RunStats mStats;
	AIAnalyzer::FpsControl mUpdateTimes;
	float mDeltaTime; // in seconds

	Vector2<unsigned int> mScreenSize;

	AIAnalyzerSettings mSettings;

	bool mInvertMouse = false;
	bool mFirstLoopAfterWindowActivation = false;
	bool mCameraOffsetChanged = false;

	bool mMediaReceived = false;

	bool mGameAISimulation = false;
	AIGame::GameState mGameAIState;

	AIAnalysis::PlayerInput mPlayerInput;
	AIAnalysis::PlayerInput mOtherPlayerInput;

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