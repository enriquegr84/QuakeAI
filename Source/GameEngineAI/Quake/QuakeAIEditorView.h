//========================================================================
// QuakeAIEditorView.h : Game View Class
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

#ifndef QUAKEAIEDITORVIEW_H
#define QUAKEAIEDITORVIEW_H

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

namespace AIEditor
{
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
}

// Locally stored sounds don't need to be preloaded because of this
class AIEditorSoundFetcher : public OnDemandSoundFetcher
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

class QuakeAIEditorUI : public BaseUI
{
	friend class QuakeAIEditorView;

public:
	QuakeAIEditorUI(const QuakeAIEditorView* view);
	virtual ~QuakeAIEditorUI();

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

	void Update(const AIEditor::RunStats& stats, const std::shared_ptr<CameraNode> camera, 
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

	const QuakeAIEditorView* mAIEditorView;

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

struct PathingFormHandler : public TextDestination
{
	PathingFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "PATHING")
		{
			if (fields.find("te_orientation") != fields.end())
			{
				mYaw = (float)atof(fields.at("te_orientation").c_str());
			}
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataCreatePath>(fields.at("te_search")));
				return;
			}
			if (fields.find("graph") != fields.end())
			{
				std::string row = fields.at("graph");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
					{
						mPosition = Vector3<float>{
							(float)atof(values[values.size() - 3].c_str()),
							(float)atof(values[values.size() - 2].c_str()),
							(float)atof(values[values.size() - 1].c_str()) };

						mNodeId = atoi(values[0].c_str());
						EventManager::Get()->TriggerEvent(std::make_shared<EventDataHighlightNode>(mNodeId));
					}
				}
				return;
			}
			if (fields.find("btn_respawn") != fields.end())
			{
				Matrix4x4<float> yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
				Transform spawnTransform;
				spawnTransform.SetTranslation(mPosition);
				spawnTransform.SetRotation(yawRotation);

				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
				std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
				aiView->SetYaw(mYaw);
				EventManager::Get()->TriggerEvent(
					std::make_shared<EventDataSpawnActor>(aiView->GetActorId(), spawnTransform));
				return;
			}

			if (fields.find("btn_pathing") != fields.end())
			{
				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
				std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
				aiView->ResetActionPlan();
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataSimulatePathing>(aiView->GetActorId()));
				return;
			}

			if (fields.find("btn_exploring") != fields.end())
			{
				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
				std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
				aiView->ResetActionPlan();
				/*
				QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
				PlayerView playerView;
				aiManager->GetPlayerView(aiView->GetActorId(), playerView);
				if (!playerView.simulation.plan.path.empty())
				{
					Transform spawnTransform;
					spawnTransform.SetTranslation(playerView.simulation.plan.node->GetPosition());
					EventManager::Get()->TriggerEvent(
						std::make_shared<EventDataSpawnActor>(aiView->GetActorId(), spawnTransform));
				}
				*/
				mNodeId = -1;
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataSimulateExploring>(aiView->GetActorId(), mNodeId));
				return;
			}

			if (fields.find("btn_reset") != fields.end())
			{
				mNodeId = -1;
				QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());

				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
				std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
				aiView->ResetActionPlan();

				PlayerView playerView;
				aiManager->GetPlayerView(aiView->GetActorId(), playerView);

				playerView.isUpdated = true;
				playerView.simulation.plan = NodePlan();
				aiManager->UpdatePlayerSimulationView(aiView->GetActorId(), playerView);

				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataShowPathing>());
				return;
			}
		}
	}

	float mYaw = 0.f;
	int mNodeId = -1;
	Vector3<float> mPosition = Vector3<float>::Zero();
};

struct EditPathingFormHandler : public TextDestination
{
	EditPathingFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "EDIT_PATHING")
		{
			if (fields.find("te_orientation") != fields.end())
			{
				mYaw = (float)atof(fields.at("te_orientation").c_str());
			}
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataEditPathing>(fields.at("te_search")));
				return;
			}
			if (fields.find("graph") != fields.end())
			{
				std::string row = fields.at("graph");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
					{
						mPosition = Vector3<float>{
							(float)atof(values[values.size() - 3].c_str()),
							(float)atof(values[values.size() - 2].c_str()),
							(float)atof(values[values.size() - 1].c_str()) };

						mNodeId = atoi(values[0].c_str());
						EventManager::Get()->TriggerEvent(std::make_shared<EventDataHighlightNode>(mNodeId));
					}
					return;
				}
			}
			if (fields.find("btn_respawn") != fields.end())
			{
				Matrix4x4<float> yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
				Transform spawnTransform;
				spawnTransform.SetTranslation(mPosition);
				spawnTransform.SetRotation(yawRotation);

				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_HUMAN);
				std::shared_ptr<HumanView> humanView = std::dynamic_pointer_cast<HumanView>(gameView);
				EventManager::Get()->TriggerEvent(
					std::make_shared<EventDataSpawnActor>(humanView->GetActorId(), spawnTransform));
				return;
			}

			if (fields.find("btn_create_node") != fields.end())
			{
				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_HUMAN);
				std::shared_ptr<HumanView> humanView = std::dynamic_pointer_cast<HumanView>(gameView);
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataCreatePathingNode>(humanView->GetActorId()));
				return;
			}

			if (fields.find("btn_pathing") != fields.end())
			{
				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_HUMAN);
				std::shared_ptr<HumanView> humanView = std::dynamic_pointer_cast<HumanView>(gameView);
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataCreatePathing>(humanView->GetActorId()));
				return;
			}

			if (fields.find("btn_save") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataSaveMap>());
				return;
			}

			if (fields.find("btn_reset") != fields.end())
			{
				mNodeId = -1;

				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataEditPathing>());
				return;
			}
		}
	}

	float mYaw = 0.f;
	int mNodeId = -1;
	Vector3<float> mPosition = Vector3<float>::Zero();
};

struct CreatePathingMapFormHandler : public TextDestination
{
	CreatePathingMapFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "CREATE_PATHING")
		{
			if (fields.find("te_orientation") != fields.end())
			{
				mYaw = (float)atof(fields.at("te_orientation").c_str());
			}
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				return;
			}
			if (fields.find("graph") != fields.end())
			{
				std::string row = fields.at("graph");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
					{
						mPosition = Vector3<float>{
							(float)atof(values[values.size() - 3].c_str()),
							(float)atof(values[values.size() - 2].c_str()),
							(float)atof(values[values.size() - 1].c_str()) };

						mNodeId = atoi(values[0].c_str());
						EventManager::Get()->TriggerEvent(std::make_shared<EventDataHighlightNode>(mNodeId));
					}
					return;
				}
			}
			if (fields.find("btn_respawn") != fields.end())
			{
				Matrix4x4<float> yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
				Transform spawnTransform;
				spawnTransform.SetTranslation(mPosition);
				spawnTransform.SetRotation(yawRotation);

				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_HUMAN);
				std::shared_ptr<HumanView> humanView = std::dynamic_pointer_cast<HumanView>(gameView);
				EventManager::Get()->TriggerEvent(
					std::make_shared<EventDataSpawnActor>(humanView->GetActorId(), spawnTransform));
				return;
			}

			if (fields.find("btn_create_node") != fields.end())
			{
				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_HUMAN);
				std::shared_ptr<HumanView> humanView = std::dynamic_pointer_cast<HumanView>(gameView);
				BaseEventManager::Get()->TriggerEvent(
					std::make_shared<EventDataCreatePathingNode>(humanView->GetActorId()));
				return;
			}

			if (fields.find("btn_pathing") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataCreateMap>());
				return;
			}

			if (fields.find("btn_save") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataSaveMap>());
				return;
			}
		}
	}

	float mYaw = 0.f;
	int mNodeId = -1;
	Vector3<float> mPosition = Vector3<float>::Zero();
};

struct MapFormHandler : public TextDestination
{
	MapFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "MAP")
		{
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataShowMap>(fields.at("te_search")));
				return;
			}
			if (fields.find("graph_nodes") != fields.end())
			{
				std::string row = fields.at("graph_nodes");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
					{
						mArcId = -1;
						mNodeId = atoi(values[0].c_str());
						EventManager::Get()->TriggerEvent(std::make_shared<EventDataShowMapNode>(mNodeId));
					}
					return;
				}
			}
			if (fields.find("graph_arcs") != fields.end())
			{
				std::string row = fields.at("graph_arcs");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
						mArcId = atoi(values[0].c_str());
					return;
				}
			}
			if (fields.find("btn_visibility") != fields.end())
			{
				if (mNodeId != INVALID_ACTOR_ID)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataNodeVisibility>(mNodeId));
				}
				return;
			}
			if (fields.find("btn_connection") != fields.end())
			{
				if (mArcId != -1)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataArcConnection>(mArcId));
				}
				else
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataNodeConnection>(mNodeId));
				}
				return;
			}

			if (fields.find("btn_save_all") != fields.end())
			{
				std::shared_ptr<EventDataSaveAll> pEvent(new EventDataSaveAll());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_reset") != fields.end())
			{
				mNodeId = -1;
				mArcId = -1;
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataShowMap>());
				return;
			}
		}
	}

	int mNodeId = -1;
	int mArcId = -1;
};

struct EditMapFormHandler : public TextDestination
{
	EditMapFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "EDIT_MAP")
		{
			if (fields.find("btn_mp_search") != fields.end() && fields.find("te_search") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataEditMap>(fields.at("te_search")));
				return;
			}
			if (fields.find("graph_nodes") != fields.end())
			{
				std::string row = fields.at("graph_nodes");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
					{
						mArcId = -1;
						mNodeId = atoi(values[0].c_str());
						EventManager::Get()->TriggerEvent(std::make_shared<EventDataEditMapNode>(mNodeId));
					}
					return;
				}
			}
			if (fields.find("graph_arcs") != fields.end())
			{
				std::string row = fields.at("graph_arcs");
				if (row.rfind("CHG:") != std::string::npos)
				{
					std::string content = Trim(row.substr(row.rfind("CNT:") + 4));
					std::vector<std::string> values = StringSplit(content, ' ');
					if (values.size() > 2)
						mArcId = atoi(values[0].c_str());
					return;
				}
			}
			if (fields.find("btn_clear") != fields.end())
			{
				mNodeId = -1;
				mArcId = -1;
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataClear>());
				return;
			}
			if (fields.find("btn_connection") != fields.end())
			{
				if (mArcId != -1)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataArcConnection>(mArcId));
				}
				else if (mNodeId != -1)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataNodeConnection>(mNodeId));
				}
				return;
			}
			if (fields.find("btn_remove") != fields.end())
			{
				if (mArcId != -1)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataRemoveArc>(mArcId));
				}
				else if (mNodeId != -1)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataRemoveNode>(mNodeId));
				}
				return;
			}
			if (fields.find("btn_remove_type") != fields.end())
			{
				if (mArcId != -1)
				{
					BaseEventManager::Get()->TriggerEvent(
						std::make_shared<EventDataRemoveArcType>(mArcId));
				}
				return;
			}
			if (fields.find("btn_save") != fields.end())
			{
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataSaveMap>());
				return;
			}
		}
	}

	int mNodeId = -1;
	int mArcId = -1;
};

struct EditorFormHandler : public TextDestination
{
	EditorFormHandler(const std::string& formName)
	{
		mFormName = formName;
	}

	void GotText(const StringMap& fields)
	{
		if (mFormName == "PAUSE_MENU")
		{
			if (fields.find("btn_edit_map") != fields.end())
			{
				std::shared_ptr<EventDataEditMap> pEvent(new EventDataEditMap());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_create_path") != fields.end())
			{
				std::shared_ptr<EventDataCreatePath> pEvent(new EventDataCreatePath());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_edit_path") != fields.end())
			{
				std::shared_ptr<EventDataEditPathing> pEvent(new EventDataEditPathing());
				BaseEventManager::Get()->TriggerEvent(pEvent);
				return;
			}

			if (fields.find("btn_create_map") != fields.end())
			{
				std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_HUMAN);
				std::shared_ptr<HumanView> humanView = std::dynamic_pointer_cast<HumanView>(gameView);
				BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataCreatePathingMap>(humanView->GetActorId()));
				return;
			}

			if (fields.find("btn_show_map") != fields.end())
			{
				std::shared_ptr<EventDataShowMap> pEvent(new EventDataShowMap());
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

class EditorSoundMaker
{
public:

	BaseSoundManager* mSound;

	bool mMakesFootstepSound;
	float mPlayerStepTimer;
	float mPlayerJumpTimer;

	SimpleSound mPlayerStepSound;
	SimpleSound mPlayerLeftPunchSound;
	SimpleSound mPlayerRightPunchSound;

	EditorSoundMaker(BaseSoundManager* sound) :
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
class AIEditorOnDemandSoundFetcher : public OnDemandSoundFetcher
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

struct AIEditorSettings
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

class QuakeAIEditorView : public HumanView
{

public:
	QuakeAIEditorView();
	virtual ~QuakeAIEditorView();

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

	void ClearMapDelegate(BaseEventDataPtr pEventData);
	void RemoveArcTypeDelegate(BaseEventDataPtr pEventData);
	void RemoveArcDelegate(BaseEventDataPtr pEventData);
	void RemoveNodeDelegate(BaseEventDataPtr pEventData);
	void HighlightNodeDelegate(BaseEventDataPtr pEventData);
	void EditMapNodeDelegate(BaseEventDataPtr pEventData);
	void ShowMapNodeDelegate(BaseEventDataPtr pEventData);

	void EditPathingGraphDelegate(BaseEventDataPtr pEventData);
	void ShowPathingGraphDelegate(BaseEventDataPtr pEventData);
	void CreatePathingGraphDelegate(BaseEventDataPtr pEventData);
	void CreatePathingMapDelegate(BaseEventDataPtr pEventData);
	void CreatePathingNodeDelegate(BaseEventDataPtr pEventData);
	void SimulateExploringDelegate(BaseEventDataPtr pEventData);
	void SimulatePathingDelegate(BaseEventDataPtr pEventData);

	void ShowNodeVisibilityDelegate(BaseEventDataPtr pEventData);
	void ShowNodeConnectionDelegate(BaseEventDataPtr pEventData);
	void ShowArcConnectionDelegate(BaseEventDataPtr pEventData);

	void SaveMapDelegate(BaseEventDataPtr pEventData);
	void EditMapDelegate(BaseEventDataPtr pEventData);
	void ShowMapDelegate(BaseEventDataPtr pEventData);
	void CreateMapDelegate(BaseEventDataPtr pEventData);
	void CreatePathDelegate(BaseEventDataPtr pEventData);

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

	std::shared_ptr<QuakePlayerController> mPlayerController;
	std::shared_ptr<CameraController> mCameraController;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<BlendState> mBlendState;

	std::shared_ptr<QuakeAIEditorUI> mUI;
	std::shared_ptr<Node> mPlayer;
	std::shared_ptr<Node> mHighlightNode;

private:

	void EditMap(PathingNode* pNode);
	void EditMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter = "");
	void EditPathingMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter = "");
	void ShowPathingMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter = "");
	void ShowMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter = "");
	void ShowMap(PathingNode* pNode);
	void CreatePathingMap(const std::string& filter = "");

	void ShowPauseMenu();

	bool GetGameContent();
	void AfterContentReceived();

	void UpdateProfilers(const AIEditor::RunStats& stats, const AIEditor::FpsControl& updateTimes, float dTime);
	void UpdateStats(AIEditor::RunStats* stats, const AIEditor::FpsControl& updateTimes, float dTime);

	void ProcessUserInput(float dTime);
	void ProcessKeyInput();

	void UpdateFrame(AIEditor::RunStats* stats, float dTime);

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

	std::shared_ptr<PathNode> mPathNode;
	std::shared_ptr<GraphNode> mGraphNode;
	std::shared_ptr<PathingGraph> mPathingMap;
	std::shared_ptr<PathingGraph> mMap;

	std::map<unsigned short, unsigned short> mSelectedClusters;
	std::map<unsigned int, BoundingBox<float>> mClustersBB;
	PathingNodeVec mCreatedNodes;

	// Some timers
	float mAvgRTTTimer = 0.0f;
	float mPlayerPositionSendTimer = 0.0f;
	IntervalLimiter mMapTimerAndUnloadInterval;

	// Sounds
	AIEditorSoundFetcher mSoundFetcher;
	std::shared_ptr<BaseSoundManager> mSoundMgr;
	std::shared_ptr<EditorSoundMaker> mSoundMaker;

	// The authentication methods we can use to enter sudo mode (=change password)
	unsigned int mSudoAuthMethods;

	// The seed returned by the logic is stored here
	uint64_t mMapSeed = 0;

	// Pre-calculated values
	int mCrackAnimationLength;

	IntervalLimiter mProfilerInterval;

	AIEditor::RunStats mStats;
	AIEditor::FpsControl mUpdateTimes;
	float mDeltaTime; // in seconds

	Vector2<unsigned int> mScreenSize;

	AIEditorSettings mSettings;

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