//========================================================================
// DemosView.cpp : source file for the sample game
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

#include "GameDemoStd.h"

#include "Application/Settings.h"

#include "Audio/SoundProcess.h"
#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"

#include "Graphic/Scene/Scene.h"
/*
#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Hierarchy/Camera.h"
*/
#include "Core/Event/Event.h"

#include "Physic/PhysicEventListener.h"

#include "Game/View/HumanView.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/RenderComponent.h"

#include "GameDemo.h"
#include "GameDemoApp.h"
#include "GameDemoView.h"
#include "GameDemoEvents.h"
#include "GameDemoManager.h"
#include "GameDemoNetwork.h"
#include "GameDemoCameraController.h"

//========================================================================
//
// MainMenuUI & MainMenuView implementation
//
//
//========================================================================

#define CID_DEMO_WINDOW					(1)
#define CID_CREATE_GAME_RADIO			(2)
#define CID_NUM_AI_SLIDER				(3)
#define CID_NUM_PLAYER_SLIDER			(4)
#define CID_HOST_LISTEN_PORT			(5)
#define CID_CLIENT_ATTACH_PORT			(6)
#define CID_START_BUTTON				(7)
#define CID_HOST_NAME					(8)
#define CID_NUM_AI_LABEL				(9)
#define CID_NUM_PLAYER_LABEL			(10)
#define CID_HOST_LISTEN_PORT_LABEL		(11)
#define CID_CLIENT_ATTACH_PORT_LABEL	(12)
#define CID_HOST_NAME_LABEL				(13)
#define CID_LEVEL_LABEL					(14)
#define CID_LEVEL_LISTBOX				(15)
#define CID_STATUS_LABEL				(16)
#define CID_DRIVER_LABEL				(17)
#define CID_FULLSCREEN_MODE				(18)
#define CID_SET_GAME_RADIO				(19)

MainMenuUI::MainMenuUI()
{

}

MainMenuUI::~MainMenuUI()
{ 

}

bool MainMenuUI::OnInit()
{
	BaseUI::OnInit();

	System* system = System::Get();
	system->GetCursorControl()->SetVisible(true);

	// set a nicer font
	const std::shared_ptr<BaseUIFont>& font = GetFont(L"DefaultFont");
	if (font) GetSkin()->SetFont(font);

	GetSkin()->SetColor(DC_BUTTON_TEXT, SColor(240, 170, 170, 170));
	GetSkin()->SetColor(DC_3D_HIGH_LIGHT, SColor(240, 34, 34, 34));
	GetSkin()->SetColor(DC_3D_FACE, SColor(240, 68, 68, 68));
	GetSkin()->SetColor(DC_EDITABLE, SColor(240, 68, 68, 68));
	GetSkin()->SetColor(DC_FOCUSED_EDITABLE, SColor(240, 84, 84, 84));
	GetSkin()->SetColor(DC_WINDOW, SColor(240, 102, 102, 102));

	//gui size
	Renderer* renderer = Renderer::Get();
	Vector2<unsigned int> screenSize(renderer->GetScreenSize());
	RectangleShape<2, int> screenRectangle;
	screenRectangle.mCenter[0] = screenSize[0] / 2;
	screenRectangle.mCenter[1] = screenSize[1] / 2;
	screenRectangle.mExtent[0] = (int)screenSize[0];
	screenRectangle.mExtent[1] = (int)screenSize[1];

	std::shared_ptr<BaseUIWindow> window = AddWindow(
		screenRectangle, false, L"Demo", 0, CID_DEMO_WINDOW);
	window->GetCloseButton()->SetToolTipText(L"Quit Demo");

	// add a options line
	RectangleShape<2, int> gameSettingsRectangle;
	gameSettingsRectangle.mCenter[0] = 50;
	gameSettingsRectangle.mExtent[0] = 90;
	gameSettingsRectangle.mCenter[1] = 42;
	gameSettingsRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> gameSettingsLine =
		AddStaticText(L"AI NPC:", gameSettingsRectangle, false, false, window, CID_NUM_AI_LABEL, true);
	gameSettingsLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	gameSettingsRectangle.mCenter[0] = 250;
	gameSettingsRectangle.mExtent[0] = 250;
	gameSettingsRectangle.mCenter[1] = 40;
	gameSettingsRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIScrollBar> gameAI = 
		AddScrollBar(true, true, gameSettingsRectangle, window, CID_NUM_AI_SLIDER);
	gameAI->SetMin(0);
	gameAI->SetMax(Settings::Get()->GetInt("max_ais"));
	gameAI->SetSmallStep(1);
	gameAI->SetLargeStep(1);
	gameAI->SetPosition(Settings::Get()->GetInt("num_ais"));
	gameAI->SetToolTipText(L"Set the AI NPC");

	gameSettingsRectangle.mCenter[0] = 50;
	gameSettingsRectangle.mExtent[0] = 90;
	gameSettingsRectangle.mCenter[1] = 82;
	gameSettingsRectangle.mExtent[1] = 16;
	gameSettingsLine =
		AddStaticText(L"Demo NPC:", gameSettingsRectangle, false, false, window, CID_NUM_PLAYER_LABEL, false);
	gameSettingsLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	gameSettingsRectangle.mCenter[0] = 250;
	gameSettingsRectangle.mExtent[0] = 250;
	gameSettingsRectangle.mCenter[1] = 80;
	gameSettingsRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIScrollBar> gamePlayer = 
		AddScrollBar(true, true, gameSettingsRectangle, window, CID_NUM_PLAYER_SLIDER);
	gamePlayer->SetMin(0);
	gamePlayer->SetMax(Settings::Get()->GetInt("max_players"));
	gamePlayer->SetSmallStep(1);
	gamePlayer->SetLargeStep(1);
	gamePlayer->SetPosition(Settings::Get()->GetInt("expected_players"));
	gamePlayer->SetToolTipText(L"Set the Demo NPC");

	gameSettingsRectangle.mCenter[0] = 50;
	gameSettingsRectangle.mExtent[0] = 90;
	gameSettingsRectangle.mCenter[1] = 122;
	gameSettingsRectangle.mExtent[1] = 16;
	gameSettingsLine =
		AddStaticText(L"Demo Host:", gameSettingsRectangle, false, false, window, CID_HOST_NAME_LABEL, false);
	gameSettingsLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	gameSettingsRectangle.mCenter[0] = 220;
	gameSettingsRectangle.mExtent[0] = 190;
	gameSettingsRectangle.mCenter[1] = 120;
	gameSettingsRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIEditBox> gameHost = AddEditBox(
		ToWideString(Settings::Get()->Get("default_game")).c_str(), 
        gameSettingsRectangle, true, true, window, CID_HOST_NAME);

	gameSettingsRectangle.mCenter[0] = 350;
	gameSettingsRectangle.mExtent[0] = 50;
	gameSettingsRectangle.mCenter[1] = 120;
	gameSettingsRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIButton> gameStart = 
		AddButton(gameSettingsRectangle, window, CID_START_BUTTON, L"Start");
	gameStart->SetToolTipText(L"Start Demo");

	// add a status line help text
	RectangleShape<2, int> statusRectangle;
	statusRectangle.mCenter[0] = screenSize[0] / 2 + 5;
	statusRectangle.mExtent[0] = screenSize[0] - 10;
	statusRectangle.mCenter[1] = screenSize[1] - 20;
	statusRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIStaticText> statusLine = AddStaticText(L"", statusRectangle, false, false, window, CID_STATUS_LABEL, true);
	statusLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	RectangleShape<2, int> videoRectangle;
	videoRectangle.mCenter[0] = screenSize[0] - 355;
	videoRectangle.mExtent[0] = 90;
	videoRectangle.mCenter[1] = 42;
	videoRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> videoDriverLine = 
		AddStaticText(L"VideoDriver:", videoRectangle, false, false, window, -1, true);
	videoDriverLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);
	
	videoRectangle.mCenter[0] = screenSize[0] - 155;
	videoRectangle.mExtent[0] = 290;
	videoRectangle.mCenter[1] = 42;
	videoRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> videoDriver;
#if defined(_OPENGL_)
	videoDriver = AddStaticText(L"OPENGL", videoRectangle, false, false, window, CID_DRIVER_LABEL, true);
#else
	videoDriver = AddStaticText(L"DIRECTX", videoRectangle, false, false, window, CID_DRIVER_LABEL, true);
#endif
	videoDriver->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	videoRectangle.mCenter[0] = screenSize[0] - 355;
	videoRectangle.mExtent[0] = 90;
	videoRectangle.mCenter[1] = 82;
	videoRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> videoModeLine =
		AddStaticText(L"VideoMode:", videoRectangle, false, false, window, -1, false);
	videoModeLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	videoRectangle.mCenter[0] = screenSize[0] - 155;
	videoRectangle.mExtent[0] = 290;
	videoRectangle.mCenter[1] = 80;
	videoRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIComboBox> videoMode = AddComboBox(videoRectangle, window);
	std::vector<Vector2<unsigned int>> videoResolutions = System::Get()->GetVideoResolutions();
	for (int i = 0; i != videoResolutions.size(); ++i)
	{
		unsigned int w = videoResolutions[i][0];
		unsigned int h = videoResolutions[i][1];
		unsigned int val = w << 16 | h;

		if (videoMode->GetIndexForItemData(val) >= 0)
			continue;

		float aspect = (float)w / (float)h;
		const wchar_t *a = L"";
		if (Function<float>::Equals(aspect, 1.3333333333f)) a = L"4:3";
		else if (Function<float>::Equals(aspect, 1.6666666f)) a = L"15:9 widescreen";
		else if (Function<float>::Equals(aspect, 1.7777777f)) a = L"16:9 widescreen";
		else if (Function<float>::Equals(aspect, 1.6f)) a = L"16:10 widescreen";
		else if (Function<float>::Equals(aspect, 2.133333f)) a = L"20:9 widescreen";

		wchar_t buf[256];
		swprintf(buf, sizeof(buf), L"%d x %d, %s", w, h, a);
		videoMode->AddItem(buf, val);
	}
	videoMode->SetSelected(videoMode->GetIndexForItemData(
        Settings::Get()->GetInt("screen_width") << 16 | Settings::Get()->GetInt("screen_height")));
	videoMode->SetToolTipText(L"Supported Screenmodes");

	screenRectangle.mCenter[0] = screenSize[0] - 350;
	screenRectangle.mExtent[0] = 100;
	screenRectangle.mCenter[1] = 120;
	screenRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUICheckBox> fullScreen = AddCheckBox(
        Settings::Get()->GetBool("fullscreen"), screenRectangle, window, CID_FULLSCREEN_MODE, L"Fullscreen");
	fullScreen->SetToolTipText(L"Set Fullscreen or Window Mode");

	screenRectangle.mCenter[0] = screenSize[0] - 250;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 122;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> videoMultiSampleLine =
		AddStaticText(L"Multisample:", screenRectangle, false, false, window, -1, false);
	videoMultiSampleLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = screenSize[0] - 130;
	screenRectangle.mExtent[0] = 120;
	screenRectangle.mCenter[1] = 120;
	screenRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIScrollBar> multiSample = AddScrollBar(true, true, screenRectangle, window, -1);
	multiSample->SetMin(0);
	multiSample->SetMax(8);
	multiSample->SetSmallStep(1);
	multiSample->SetLargeStep(1);
	multiSample->SetPosition(Settings::Get()->GetInt("fsaa"));
	multiSample->SetToolTipText(L"Set the multisample (disable, 1x, 2x, 4x, 8x )");

	screenRectangle.mCenter[0] = screenSize[0] - 35;
	screenRectangle.mExtent[0] = 50;
	screenRectangle.mCenter[1] = 120;
	screenRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIButton> setVideoMode = AddButton(screenRectangle, window, CID_SET_GAME_RADIO, L"Set");
	setVideoMode->SetToolTipText(L"Set video mode with current values");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = screenSize[1] - 390;
	screenRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIStaticText> levelLine =
		AddStaticText(L"Levels:", screenRectangle, false, false, window, CID_LEVEL_LABEL, false);
	levelLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 190;
	screenRectangle.mExtent[0] = 380;
	screenRectangle.mCenter[1] = screenSize[1] - 210;
	screenRectangle.mExtent[1] = 340;
	std::shared_ptr<BaseUIListBox> level = AddListBox(screenRectangle, window, CID_LEVEL_LISTBOX, true);
	level->SetToolTipText(L"Select the current level.\n Press button to start the level");

	std::vector<Level*> levels = GameLogic::Get()->GetLevelManager()->GetLevels();
	for (std::vector<Level*>::iterator it = levels.begin(); it != levels.end(); ++it)
		level->AddItem((*it)->GetName().c_str());
	level->SetSelected(0);

	// create a setting panel
	screenRectangle.mCenter[0] = screenSize[0] - 350;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = screenSize[1] - 390;
	screenRectangle.mExtent[1] = 20;
	std::shared_ptr<BaseUIStaticText> settingsLine =
		AddStaticText(L"Settings:", screenRectangle, false, false, window, -1, false);
	settingsLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = screenSize[0] - 200;
	screenRectangle.mExtent[0] = 400;
	screenRectangle.mCenter[1] = screenSize[1] - 210;
	screenRectangle.mExtent[1] = 340;
	std::shared_ptr<BaseUIListBox> settings = AddListBox(screenRectangle, window, -1, true);
	settings->SetToolTipText(L"Show the current key settings");
	settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_forward") + " - Move forward")).c_str());
    settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_backward") + " - Move backward")).c_str());
    settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_left") + " - Move left")).c_str());
    settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_right") + " - Move right")).c_str());
    settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_jump") + " - Move down")).c_str());
    settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_jump") + " - Move up")).c_str());
    settings->AddItem(ToWideString(
        (Settings::Get()->Get("keymap_toggle_debug") + " - Show wireframe")).c_str());
	settings->AddItem(L"Key 7 - Show physics box");

	SetUIActive(1);
	return true;
}


void MainMenuUI::Set()
{
	const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
	const std::shared_ptr<BaseUIElement>& window = root->GetElementFromId(CID_DEMO_WINDOW);
	const std::shared_ptr<BaseUIButton>& createGame = 
		std::static_pointer_cast<BaseUIButton>(root->GetElementFromId(CID_CREATE_GAME_RADIO, true));
	const std::shared_ptr<BaseUIButton>& setGame =
		std::static_pointer_cast<BaseUIButton>(root->GetElementFromId(CID_SET_GAME_RADIO, true));
	const std::shared_ptr<BaseUIScrollBar>& numAI = 
		std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_NUM_AI_SLIDER, true));
	const std::shared_ptr<BaseUIScrollBar>& numPlayer = 
		std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_NUM_PLAYER_SLIDER, true));
	const std::shared_ptr<BaseUIEditBox>& hostPort = 
		std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_HOST_LISTEN_PORT, true));
	const std::shared_ptr<BaseUIEditBox>& clientPort = 
		std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_CLIENT_ATTACH_PORT, true));
	const std::shared_ptr<BaseUIButton>& startGame = 
		std::static_pointer_cast<BaseUIButton>(root->GetElementFromId(CID_START_BUTTON, true));
	const std::shared_ptr<BaseUIEditBox>& hostName = 
		std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_HOST_NAME, true));
	const std::shared_ptr<BaseUIListBox>& level = 
		std::static_pointer_cast<BaseUIListBox>(root->GetElementFromId(CID_LEVEL_LISTBOX, true));

    Settings::Get()->SetInt("num_ais", numAI->GetPosition());
    Settings::Get()->SetInt("expected_players", numPlayer->GetPosition());
    Settings::Get()->Set("selected_game",  ToString(hostName->GetText()));
}

// enable GUI elements
void MainMenuUI::SetUIActive(int command)
{
	bool inputState = false;

	int guiActive = -1;
	switch (command)
	{
		case 0: guiActive = 0; inputState = !guiActive; break;
		case 1: guiActive = 1; inputState = !guiActive; break;
		case 2: guiActive ^= 1; inputState = !guiActive; break;
		case 3: break;
	}

	const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
	const std::shared_ptr<BaseUIElement>& window = root->GetElementFromId(CID_DEMO_WINDOW);
	if (window)
		window->SetVisible(guiActive != 0);

	SetFocus(guiActive ? window : 0);
}

bool MainMenuUI::OnRestore()
{
	return true;
}

bool MainMenuUI::OnRender(double time, float elapsedTime)
{
	const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
	const std::shared_ptr<BaseUIStaticText>& statusLabel =
		std::static_pointer_cast<BaseUIStaticText>(root->GetElementFromId(CID_STATUS_LABEL));
	if (statusLabel)
		statusLabel->SetText(std::wstring(L"Press set button to change settings").c_str());

	return BaseUI::OnRender(time, elapsedTime);
};

bool MainMenuUI::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc(evt);
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool MainMenuUI::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_UI_EVENT)
	{
		switch (evt.mUIEvent.mCaller->GetID())
		{
			case CID_CREATE_GAME_RADIO:
			{
				if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
					mCreatingGame = true;
				break;
			}

			case CID_DEMO_WINDOW:
				break;
			case CID_SET_GAME_RADIO:
			{
				/*
				if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUICheckBox>& fullscreen =
						std::static_pointer_cast<BaseUICheckBox>(root->GetElementFromId(CID_FULLSCREEN_MODE, true));
					if (!System::Get()->IsFullscreen() && fullscreen->IsChecked())
						System::Get()->SetFullscreen(true);
					else if (System::Get()->IsFullscreen() && !fullscreen->IsChecked())
						System::Get()->SetFullscreen(false);
				}
				*/
				break;
			}

			case CID_LEVEL_LISTBOX:
			case CID_NUM_AI_SLIDER:
			case CID_NUM_PLAYER_SLIDER:
			case CID_HOST_LISTEN_PORT:
			case CID_CLIENT_ATTACH_PORT:
			case CID_HOST_NAME:
			{
				break;
			}

			case CID_NUM_AI_LABEL:
			case CID_NUM_PLAYER_LABEL:
			case CID_HOST_LISTEN_PORT_LABEL:
			case CID_CLIENT_ATTACH_PORT_LABEL:
			case CID_HOST_NAME_LABEL:
			case CID_LEVEL_LABEL:
			case CID_STATUS_LABEL:
			{
				break;
			}

			case CID_START_BUTTON:
			{
				if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
				{
					Set();
					SetVisible(false);

					std::shared_ptr<EventDataRequestStartGame> pRequestStartGameEvent(new EventDataRequestStartGame());
					EventManager::Get()->QueueEvent(pRequestStartGameEvent);
				}

				break;
			}

			default:
			{
				LogWarning("Unknown control.");
			}
		}
	}

	return BaseUI::OnEvent(evt);
}


MainMenuView::MainMenuView() : HumanView()
{
	mMainMenuUI.reset(new MainMenuUI()); 
	mMainMenuUI->OnInit();
	PushElement(mMainMenuUI);
}


MainMenuView::~MainMenuView()
{
}


void MainMenuView::RenderText()
{
	HumanView::RenderText();
}


void MainMenuView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate(timeMs, deltaMs);
}


bool MainMenuView::OnMsgProc( const Event& evt )
{
	if (mMainMenuUI->IsVisible() )
	{
		if (HumanView::OnMsgProc(evt))
			return 1;
	}
	return 0;
}

//========================================================================
//
// StandardHUD implementation
//
//
//========================================================================

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3

StandardHUD::StandardHUD()
{

}


StandardHUD::~StandardHUD() 
{ 

}

bool StandardHUD::OnInit()
{
	BaseUI::OnInit();

	return true;
}

bool StandardHUD::OnRestore()
{
	return BaseUI::OnRestore();
}

bool StandardHUD::OnRender(double time, float elapsedTime)
{
	return BaseUI::OnRender(time, elapsedTime);
};


bool StandardHUD::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc( evt );
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool StandardHUD::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_UI_EVENT)
	{
		int id = evt.mUIEvent.mCaller->GetID();
	}

	return false;
}

//========================================================================
//
// GameDemoHumanView Implementation
//
//========================================================================

//
// GameDemoHumanView::GameDemoHumanView	- Chapter 19, page 724
//
GameDemoHumanView::GameDemoHumanView() 
	: HumanView()
{ 
	mShowUI = true; 
	mDebugMode = DM_OFF;
    RegisterAllDelegates();
}


GameDemoHumanView::~GameDemoHumanView()
{
    RemoveAllDelegates();
}

//
// GameDemoHumanView::OnMsgProc				- Chapter 19, page 727
//
bool GameDemoHumanView::OnMsgProc( const Event& evt )
{
	HumanView::OnMsgProc(evt);

	switch(evt.mEventType)
	{
		case ET_UI_EVENT:
			// hey, why is the user sending gui events..?
			break;

		case ET_KEY_INPUT_EVENT:
		{
			if (evt.mKeyInput.mPressedDown)
			{
				switch (evt.mKeyInput.mKey)
				{
					case KEY_KEY_6:
					{
						mDebugMode = mDebugMode ? DM_OFF : DM_WIREFRAME;
						for (auto child : mScene->GetRootNode()->GetChildren())
							child->SetDebugState(mDebugMode);
						return true;
					}

					case KEY_KEY_7:
					{
						GameDemoLogic* twg = static_cast<GameDemoLogic *>(GameLogic::Get());
						twg->ToggleRenderDiagnostics();
						return true;
					}	

					case KEY_ESCAPE:
						GameApplication::Get()->SetQuitting(true);
						return true;
				}
			}
		}
	}

	return false;
}

//
// GameDemoHumanView::RenderText				- Chapter 19, page 727
//
void GameDemoHumanView::RenderText()
{
	HumanView::RenderText();
}


//
// GameDemoHumanView::OnUpdate				- Chapter 19, page 730
//
void GameDemoHumanView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate( timeMs, deltaMs );

	if (mGameCameraController)
	{
		mGameCameraController->OnUpdate(timeMs, deltaMs);
	}
}

//
// GameDemoHumanView::OnAttach				- Chapter 19, page 731
//
void GameDemoHumanView::OnAttach(GameViewId vid, ActorId aid)
{
	HumanView::OnAttach(vid, aid);
}

bool GameDemoHumanView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
	if (!HumanView::LoadGameDelegate(pLevelData))
		return false;

    mStandardHUD.reset(new StandardHUD());
	mStandardHUD->OnInit();
    PushElement(mStandardHUD);

    // A movement controller is going to control the camera, 
    // but it could be constructed with any of the objects you see in this function.
	mCamera->GetAbsoluteTransform().SetTranslation(Vector4<float>::Zero());
    mGameCameraController.reset(new GameDemoCameraController(mCamera, 0, 0, false));
	mKeyboardHandler = mGameCameraController;
	mMouseHandler = mGameCameraController;
	mCamera->ClearTarget();

    mScene->OnRestore();
    return true;
}

void GameDemoHumanView::SetControlledActor(ActorId actorId)
{ 
	mPlayer = mScene->GetSceneNode(actorId);
    if (!mPlayer)
    {
        LogError("Invalid player");
        return;
    }

	HumanView::SetControlledActor(actorId);
}

void GameDemoHumanView::GameplayUiUpdateDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGameplayUIUpdate> pCastEventData = 
		std::static_pointer_cast<EventDataGameplayUIUpdate>(pEventData);
    if (!pCastEventData->GetUiString().empty())
        mGameplayText = pCastEventData->GetUiString();
    else
		mGameplayText.clear();
}

void GameDemoHumanView::SetControlledActorDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataSetControlledActor> pCastEventData = 
		std::static_pointer_cast<EventDataSetControlledActor>(pEventData);
    //SetControlledActor(pCastEventData->GetActorId());

	HumanView::SetControlledActor(mGameCameraController->GetCamera()->GetId());
	mKeyboardHandler = mGameCameraController;
	mMouseHandler = mGameCameraController;
}

void GameDemoHumanView::RotateActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataRotateActor> pCastEventData =
		std::static_pointer_cast<EventDataRotateActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		ActorId actorId = pCastEventData->GetId();
		std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
		if (pNode)
		{
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(1), pCastEventData->GetYaw() * (float)GE_C_DEG_TO_RAD));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(2), pCastEventData->GetPitch() * (float)GE_C_DEG_TO_RAD));

			pNode->GetRelativeTransform().SetRotation(yawRotation * pitchRotation);
		}
	}
}

void GameDemoHumanView::RegisterAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoHumanView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);
    pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoHumanView::SetControlledActorDelegate), 
		EventDataSetControlledActor::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &GameDemoHumanView::RotateActorDelegate),
		EventDataRotateActor::skEventType);
}

void GameDemoHumanView::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoHumanView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);
    pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoHumanView::SetControlledActorDelegate), 
		EventDataSetControlledActor::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &GameDemoHumanView::RotateActorDelegate),
		EventDataRotateActor::skEventType);
}

///////////////////////////////////////////////////////////////////////////////
//
// AIPlayerView::AIPlayerView					- Chapter 19, page 737
//
AIPlayerView::AIPlayerView(std::shared_ptr<PathingGraph> pPathingGraph) 
	: BaseGameView(), mPathingGraph(pPathingGraph)
{
    //
}

//
// AIPlayerView::~AIPlayerView					- Chapter 19, page 737
//
AIPlayerView::~AIPlayerView(void)
{
    LogInformation("AI Destroying AIPlayerView");
}