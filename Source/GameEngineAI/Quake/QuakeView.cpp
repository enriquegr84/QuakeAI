//========================================================================
// QuakeView.cpp : Game View Class
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

#include "Quake.h"
#include "QuakeApp.h"
#include "QuakeView.h"
#include "QuakeAIView.h"
#include "QuakeEvents.h"
#include "QuakeLevelManager.h"
#include "QuakeNetwork.h"
#include "QuakePlayerController.h"
#include "QuakeCameraController.h"

#include "Games/Forms/Menu/Local.h"
#include "Games/Forms/Menu/Online.h"
#include "Games/Forms/Menu/Content.h"
#include "Games/Forms/Menu/DlgDeleteContent.h"
#include "Games/Forms/Menu/DlgContentStore.h"
#include "Games/Forms/Menu/MenuSettings.h"
#include "Games/Forms/Menu/About.h"

#include "Application/Settings.h"

#include "Audio/SoundOpenal.h"
#include "Audio/SoundProcess.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"
#include "Graphic/UI/UIEngine.h"

#include "Graphic/Scene/Scene.h"

#include "Core/OS/OS.h"
#include "Core/Utility/SHA1.h"
#include "Core/Utility/Profiler.h"
#include "Core/Utility/Serialize.h"

#include "Core/Event/Event.h"
#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

#include "Game/View/HumanView.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/RenderComponent.h"

/******************************************************************************/
/** SoundFetcher                                                          */
/******************************************************************************/
void MenuSoundFetcher::FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
{
	if (mFetched.count(name))
		return;
	mFetched.insert(name);

	std::string soundBase = "Art/Quake/audio";
	dstPaths.insert(soundBase + "/" + name + ".ogg");
	for (int i = 0; i < 10; i++)
		dstPaths.insert(soundBase + "/" + name + "." + std::to_string(i) + ".ogg");
	dstPaths.insert(soundBase + "/" + name + ".ogg");
}


void GameSoundFetcher::PathsInsert(std::set<std::string>& dstPaths,
	const std::string& base, const std::string& name)
{
	std::wstring soundPath =
		FileSystem::Get()->GetWorkingDirectory() + L"/../../Assets/Art/Quake/audio";

	std::vector<std::wstring> paths = 
		FileSystem::Get()->GetRecursiveDirectories(soundPath);
	for (const auto& path : paths)
	{
		if (FileSystem::Get()->ExistFile(path + L"/" + ToWideString(name.c_str()) + L".ogg"))
		{
			std::string filePath = ToString(path.substr(soundPath.size()).c_str());
			dstPaths.insert(base + filePath + "/" + name + ".ogg");
			break;
		}
		if (FileSystem::Get()->ExistFile(path + L"/" + ToWideString(name.c_str()) + L".wav"))
		{
			std::string filePath = ToString(path.substr(soundPath.size()).c_str());
			dstPaths.insert(base + filePath + "/" + name + ".wav");
			break;
		}
	}
}

void GameSoundFetcher::FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
{
	if (mFetched.count(name))
		return;

	mFetched.insert(name);

	std::string soundBase = "Art/Quake/audio";
	PathsInsert(dstPaths, soundBase, name);
}


//========================================================================
//
// QuakeMainMenuUI & QuakeMainMenuView implementation
//
//========================================================================

QuakeMainMenuUI::QuakeMainMenuUI()
{

}

QuakeMainMenuUI::~QuakeMainMenuUI()
{ 

}


void QuakeMainMenuUI::ResetMenuUI()
{
	ClearMenuUI(L"overlay");
	ClearMenuUI(L"background");
	ClearMenuUI(L"header");
	ClearMenuUI(L"footer");

	if (!SetGenericMenuUI(L"overlay"))
		SetGenericMenuUI(L"background");

	SetGenericMenuUI(L"header");
	SetGenericMenuUI(L"footer");
}

void QuakeMainMenuUI::UpdateMenuUI(std::wstring gamePath)
{
	ClearMenuUI(L"overlay");
	ClearMenuUI(L"background");
	ClearMenuUI(L"header");
	ClearMenuUI(L"footer");

	if (!SetMenuUI(L"overlay", gamePath))
		if (!SetMenuUI(L"background", gamePath))
			SetGenericMenuUI(L"background");

	SetMenuUI(L"header", gamePath);
	SetMenuUI(L"footer", gamePath);
}

void QuakeMainMenuUI::ClearMenuUI(std::wstring id)
{
	SetTexture(id, L"", false, 16);
}

bool QuakeMainMenuUI::SetGenericMenuUI(std::wstring id)
{
	//default texture dir
	std::wstring path = L"Art/Quake/textures/menu_" + id + L".png";
	if (SetTexture(id, path, false, 16))
		return true;

	return false;
}

bool QuakeMainMenuUI::SetMenuUI(std::wstring id, std::wstring gamePath)
{
	if (gamePath.empty())
		return false;

	//Find out how many randomized textures the game provides
	std::wstring previousCWD = FileSystem::Get()->GetWorkingDirectory();
	std::vector<std::wstring> menuFiles;
	std::wstring path = previousCWD + L"/../../Assets/" + gamePath + L"/menu";
	FileSystem::Get()->GetFileList(menuFiles, path);
	FileSystem::Get()->ChangeWorkingDirectoryTo(previousCWD);

	unsigned int n = 0;
	std::wstring filePath;
	for (unsigned int i = 1; i <= menuFiles.size(); i++)
	{
		filePath = path + L"/" + id + L"." + std::to_wstring(i) + L".png";
		if (!FileSystem::Get()->ExistFile(filePath))
		{
			n = i;
			break;
		}
	}

	//Select random texture, 0 means standard texture
	n = n > 0 ? Randomizer::Rand() % n : 0;
	if (n == 0)
		filePath = id + L".png";
	else
		filePath = id + L"." + std::to_wstring(n) + L".png";

	path = gamePath + L"/menu/" + filePath;
	if (SetTexture(id, path, false, 16))
		return true;

	return false;
}

bool QuakeMainMenuUI::OnInit()
{
	BaseUI::OnInit();

	System* system = System::Get();
	system->GetCursorControl()->SetVisible(true);

	// set a nicer font
	const std::shared_ptr<BaseUIFont>& font = GetFont(L"DefaultFont");
	if (font) GetSkin()->SetFont(font);

	GetSkin()->SetColor(DC_BUTTON_TEXT, SColor(255, 255, 255, 255));
	GetSkin()->SetColor(DC_3D_LIGHT, SColor(0, 0, 0, 0));
	GetSkin()->SetColor(DC_3D_HIGH_LIGHT, SColor(255, 30, 30, 30));
	GetSkin()->SetColor(DC_3D_SHADOW, SColor(255, 0, 0, 0));
	GetSkin()->SetColor(DC_HIGH_LIGHT, SColor(255, 70, 120, 50));
	GetSkin()->SetColor(DC_HIGH_LIGHT_TEXT, SColor(255, 255, 255, 255));
	GetSkin()->SetColor(DC_EDITABLE, SColor(255, 128, 128, 128));
	GetSkin()->SetColor(DC_FOCUSED_EDITABLE, SColor(255, 96, 134, 49));

	//create menu form
	mMenu = std::make_shared<MenuSettings>();
	mTextDestination = std::make_shared<TextDestination>();
	mFormSource = std::make_shared<FormSource>(mMenu->GetForm());

	RectangleShape<2, int> menuRectangle;
	menuRectangle.mCenter = Vector2<int>{ 50, 50 };
	menuRectangle.mExtent = Vector2<int>{ 100, 100 };
	mFormMenu = std::static_pointer_cast<UIForm>(
		AddForm(mFormSource, mTextDestination, "", menuRectangle));

	mFormMenu->AllowClose(false);
	mFormMenu->LockSize(true, Vector2<unsigned int>{800, 600});

	ResetMenuUI();

	return true;
}

bool QuakeMainMenuUI::OnRestore()
{
	return true;
}

bool QuakeMainMenuUI::OnRender(double time, float elapsedTime)
{
	return BaseUI::OnRender(time, elapsedTime);
};

bool QuakeMainMenuUI::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc(evt);
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool QuakeMainMenuUI::OnEvent(const Event& evt)
{
	bool createGame = false;
	if (evt.mEventType == ET_UI_EVENT)
	{
		switch (evt.mUIEvent.mEventType)
		{
			case UIEVT_CHECKBOX_CHANGED:
			{
				const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
				const std::shared_ptr<BaseUICheckBox>& checkbox = std::static_pointer_cast<BaseUICheckBox>(
					root->GetElementFromId(evt.mUIEvent.mCaller->GetID(), true));
				if (mMenu->Handle(mFormMenu->GetName(evt.mUIEvent.mCaller->GetID()), checkbox.get()))
					mFormMenu->SetForm(mMenu->GetForm());
			}
			break;

			case UIEVT_BUTTON_CLICKED:
			{
				const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
				const std::shared_ptr<BaseUIButton>& button = std::static_pointer_cast<BaseUIButton>(
					root->GetElementFromId(evt.mUIEvent.mCaller->GetID(), true));
				mMenu->Handle(mFormMenu->GetName(evt.mUIEvent.mCaller->GetID()), button.get());
			}
			break;

			case UIEVT_TABLE_CHANGED:
			{
				const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
				const std::shared_ptr<BaseUITable>& table = std::static_pointer_cast<BaseUITable>(
					root->GetElementFromId(evt.mUIEvent.mCaller->GetID(), true));
				if (mMenu->Handle(mFormMenu->GetName(evt.mUIEvent.mCaller->GetID()), table.get()))
					mFormMenu->SetForm(mMenu->GetForm());
			}
			break;

			case UIEVT_TAB_CHANGED:
			{
				const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
				const std::shared_ptr<BaseUITabControl>& tabcontrol = std::static_pointer_cast<BaseUITabControl>(
					root->GetElementFromId(evt.mUIEvent.mCaller->GetID(), true));

				if (tabcontrol->GetActiveTab() == 0)
				{
					mMenu = std::make_shared<Local>();
					mFormMenu->SetForm(mMenu->GetForm());
				}
				else if (tabcontrol->GetActiveTab() == 1)
				{
					mMenu = std::make_shared<Online>();
					mFormMenu->SetForm(mMenu->GetForm());
				}
				else if (tabcontrol->GetActiveTab() == 2)
				{
					mMenu = std::make_shared<Content>();
					mFormMenu->SetForm(mMenu->GetForm());
				}
				else if (tabcontrol->GetActiveTab() == 3)
				{
					mMenu = std::make_shared<MenuSettings>();
					mFormMenu->SetForm(mMenu->GetForm());
				}
				else if (tabcontrol->GetActiveTab() == 4)
				{
					mMenu = std::make_shared<About>();
					mFormMenu->SetForm(mMenu->GetForm());
				}
			}
			break;
		}
	}

	return BaseUI::OnEvent(evt);
}


QuakeMainMenuView::QuakeMainMenuView() : HumanView()
{
	if (Settings::Get()->GetBool("enable_sound"))
	{
		//create soundmanager
		LogInformation("Attempting to use OpenAL audio");
		mSoundMgr = std::make_shared<OpenALSoundManager>(
			static_cast<OpenALSoundSystem*>(SoundSystem::Get()), &mSoundFetcher);
		if (!mSoundMgr)
			LogError("Failed to Initialize OpenAL audio");
	}
	else LogInformation("Sound disabled.");

	mSoundMgr->PlaySoundGlobal(SimpleSound("main_menu", 0.5), true);
	// core.sound_play("main_menu", true)

	mMainMenuUI.reset(new QuakeMainMenuUI());
	mMainMenuUI->OnInit();
	PushElement(mMainMenuUI);

	mCamera->GetRelativeTransform().SetTranslation(Vector4<float>::Zero());
	UpdateCameraRotation(Vector4<float>::Zero(), Vector4<float>{0, 60, 100, 0});

	float upFov, aspectRatio, dMin, dMax;
	mCamera->Get()->GetFrustum(upFov, aspectRatio, dMin, dMax);
	mCamera->Get()->SetFrustum(upFov, aspectRatio, dMin, 10000);

	PushElement(mScene);

	RegisterAllDelegates();
}

QuakeMainMenuView::~QuakeMainMenuView()
{
	RemoveAllDelegates();
}

void QuakeMainMenuView::RenderText()
{
	HumanView::RenderText();
}

void QuakeMainMenuView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate(timeMs, deltaMs);
}

bool QuakeMainMenuView::OnMsgProc( const Event& evt )
{
	if (mMainMenuUI->IsVisible() )
	{
		if (HumanView::OnMsgProc(evt))
			return 1;
	}
	return 0;
}

void QuakeMainMenuView::UpdateCameraRotation(
	const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const
{
	EulerAngles<float> rotation;
	rotation.mAxis[1] = 1;
	rotation.mAxis[2] = 2;

	Quaternion<float> q(targetPos - cameraPos);
	rotation = Rotation<4, float>(q)(rotation.mAxis[0], rotation.mAxis[1], rotation.mAxis[2]);
	mCamera->GetRelativeTransform().SetRotation(rotation);
}

void QuakeMainMenuView::OpenGameSelectionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataOpenGameSelection> pCastEventData =
		std::static_pointer_cast<EventDataOpenGameSelection>(pEventData);

	std::shared_ptr<Local> local = std::make_shared<Local>();
	mMainMenuUI->SetMenu(local);
	mMainMenuUI->GetFormMenu()->SetForm(local->GetForm());
}

void QuakeMainMenuView::ChangeGameSelectionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeGameSelection> pCastEventData =
		std::static_pointer_cast<EventDataChangeGameSelection>(pEventData);

	mMainMenuUI->UpdateMenuUI(ToWideString(pCastEventData->GetGamePath()));
	mMainMenuUI->SetTitleText(ToWideString(pCastEventData->GetGameName()));
	mMainMenuUI->GetFormMenu()->SetForm(mMainMenuUI->GetMenu()->GetForm());
}

void QuakeMainMenuView::RegisterAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeMainMenuView::OpenGameSelectionDelegate),
		EventDataOpenGameSelection::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeMainMenuView::ChangeGameSelectionDelegate),
		EventDataChangeGameSelection::skEventType);
}

void QuakeMainMenuView::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeMainMenuView::OpenGameSelectionDelegate),
		EventDataOpenGameSelection::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeMainMenuView::ChangeGameSelectionDelegate),
		EventDataChangeGameSelection::skEventType);
}

//========================================================================
//
// QuakeUI implementation
//
//========================================================================

QuakeUI::QuakeUI(const QuakeHumanView* view) : mGameView(view)
{
	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

	if (GetSkin())
		mStatusTextInitialColor = GetSkin()->GetColor(DC_BUTTON_TEXT);
	else
		mStatusTextInitialColor = SColor(255, 0, 0, 0);
}

QuakeUI::~QuakeUI()
{ 

}

bool QuakeUI::OnInit()
{
	BaseUI::OnInit();

	// set a nicer font
	const std::shared_ptr<BaseUIFont>& font = GetFont(L"DefaultFont");
	if (font) GetSkin()->SetFont(font);

	GetSkin()->SetColor(DC_BUTTON_TEXT, SColor(255, 255, 255, 255));
	GetSkin()->SetColor(DC_3D_LIGHT, SColor(0, 0, 0, 0));
	GetSkin()->SetColor(DC_3D_HIGH_LIGHT, SColor(255, 30, 30, 30));
	GetSkin()->SetColor(DC_3D_SHADOW, SColor(255, 0, 0, 0));
	GetSkin()->SetColor(DC_HIGH_LIGHT, SColor(255, 70, 120, 50));
	GetSkin()->SetColor(DC_HIGH_LIGHT_TEXT, SColor(255, 255, 255, 255));
	GetSkin()->SetColor(DC_EDITABLE, SColor(255, 128, 128, 128));
	GetSkin()->SetColor(DC_FOCUSED_EDITABLE, SColor(255, 96, 134, 49));

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		// Create a vertex buffer for a two-triangles square. The PNG is stored
		// in left-handed coordinates. The texture coordinates are chosen to
		// reflect the texture in the y-direction.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
		vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		// Create an effect for the vertex and pixel shaders. The texture is
		// bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extraRes =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extraRes->GetProgram())
			extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		auto effect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
	}

	// First line of debug text
	mUIText = AddStaticText(L"Quake", RectangleShape<2, int>(), false, false);

	// Second line of debug text
	mUIText2 = AddStaticText(L"", RectangleShape<2, int>(), false, false);

	// Chat text
	mUITextChat = AddStaticText(L"", RectangleShape<2, int>(), false, true);

	uint16_t chatFontSize = Settings::Get()->GetUInt16("chat_font_size");
	if (chatFontSize != 0)
		mUITextChat->SetOverrideFont(GetBuiltInFont());
	//g_fontengine->getFont(chatFontSize, FM_UNSPECIFIED));

	// At the middle of the screen Object infos are shown in this
	int chatFontHeight = mUITextChat->GetActiveFont()->GetDimension(L"Ay")[1];
	uint16_t recentChatMessages = Settings::Get()->GetUInt16("recent_chat_messages");

	RectangleShape<2, int> rect;
	rect.mExtent[0] = 400;
	rect.mExtent[1] = chatFontHeight * 5 + 5;
	rect.mCenter = rect.mExtent / 2;
	rect.mCenter += Vector2<int>{100, chatFontHeight* (recentChatMessages + 3)};
	mUITextInfo = AddStaticText(L"", rect, false, true);

	// Status text (displays info when showing and hiding GUI stuff, etc.)
	mUITextStatus = AddStaticText(
		L"<Status>", RectangleShape<2, int>(), false, false);
	mUITextStatus->SetVisible(false);

	// Profiler text (size is updated when text is updated)
	mUITextProfiler = AddStaticText(
		L"<Profiler>", RectangleShape<2, int>(), false, true);
	mUITextProfiler->SetOverrideFont(GetBuiltInFont());
	/*
	mUITextProfiler->SetOverrideFont(g_fontengine->getFont(
		g_fontengine->getDefaultFontSize() * 0.9f, FM_MONO));
	*/
	mUITextProfiler->SetVisible(false);

	// Chat backend and console
	rect.mExtent = Vector2<int>{ 100, 100 };
	rect.mCenter = rect.mExtent / 2;
	mUIChatConsole = std::make_shared<UIChatConsole>(this, -1, rect);
	mUIChatConsole->SetParent(GetRootUIElement());
	mUIChatConsole->SetVisible(false);

	mFlags = QuakeUI::Flags();
	mFlags.showDebug = Settings::Get()->GetBool("show_debug");

	return true;
}

void QuakeUI::Update(const RunStats& stats, const std::shared_ptr<CameraNode> camera, 
	const std::shared_ptr<UIChatConsole> chatConsole, float dTime)
{
	Vector2<unsigned int> screensize = Renderer::Get()->GetScreenSize();

	if (mFlags.showDebug)
	{
		static float drawTimeAvg = 0;
		drawTimeAvg = drawTimeAvg * 0.95f + stats.drawTime * 0.05f;
		unsigned short fps = (unsigned short)(1.f / stats.dTimeJitter.avg);

		std::ostringstream os(std::ios_base::binary);
		os << std::fixed
			<< "Quake " << " | FPS: " << fps
			<< std::setprecision(0)
			<< " | drawTime: " << drawTimeAvg << "ms"
			<< std::setprecision(1)
			<< " | dtime jitter: "
			<< (stats.dTimeJitter.maxFraction * 100.0) << "%"
			<< std::setprecision(1);
		//<< std::setprecision(2)
		//<< " | RTT: " << (client->getRTT() * 1000.0f) << "ms";
		mUIText->SetText(ToWideString(os.str()).c_str());
		int chatFontHeight = mUITextChat->GetActiveFont()->GetDimension(L"Ay")[1];

		RectangleShape<2, int> rect;
		rect.mExtent[0] = screensize[0] - 5;
		rect.mExtent[1] = chatFontHeight;
		rect.mCenter = rect.mExtent / 2 + Vector2<int>{5, 5};
		mUIText->SetRelativePosition(rect);
	}

	// Finally set the guitext visible depending on the flag
	mUIText->SetVisible(mFlags.showDebug);

	if (mFlags.showDebug)
	{		
		EulerAngles<float> rotation;
		rotation.mAxis[1] = 1;
		rotation.mAxis[2] = 2;
		camera->GetAbsoluteTransform().GetRotation(rotation);
		Vector3<float> position = camera->GetAbsoluteTransform().GetTranslation();

		std::ostringstream os(std::ios_base::binary);
		os << std::setprecision(1) << std::fixed
			<< "pos: (" << position[0] << ", " << position[1] << ", " << position[2]
			<< ") | yaw: " << rotation.mAngle[2] << " "
			<< " | pitch: " << rotation.mAngle[1];
		//<< " | seed: " << ((uint64_t)client->GetMapSeed());

		mUIText2->SetText(ToWideString(os.str()).c_str());
		int chatFontHeight = mUITextChat->GetActiveFont()->GetDimension(L"Ay")[1];

		RectangleShape<2, int> rect;
		rect.mExtent[0] = screensize[0] - 5;
		rect.mExtent[1] = chatFontHeight;
		rect.mCenter = rect.mExtent / 2 + Vector2<int>{5, 5 + chatFontHeight};
		mUIText2->SetRelativePosition(rect);
	}

	mUIText2->SetVisible(mFlags.showDebug);

	mUITextInfo->SetText(mInfoText.c_str());
	mUITextInfo->SetVisible(mFlags.showHud);

	static const float statusTextTimeMax = 1.5f;
	if (!mStatusText.empty())
	{
		mStatusTextTime += dTime;

		if (mStatusTextTime >= statusTextTimeMax)
		{
			ClearStatusText();
			mStatusTextTime = 0.0f;
		}
	}

	mUITextStatus->SetText(mStatusText.c_str());
	mUITextStatus->SetVisible(!mStatusText.empty());

	if (!mStatusText.empty())
	{
		int statusWidth = mUITextStatus->GetTextWidth();
		int statusHeight = mUITextStatus->GetTextHeight();
		int statusY = screensize[1] - 150;
		int statusX = (screensize[0] - statusWidth) / 2;

		RectangleShape<2, int> rect;
		rect.mExtent[0] = statusWidth;
		rect.mExtent[1] = statusHeight;
		rect.mCenter[0] = rect.mExtent[0] / 2 + statusX;
		rect.mCenter[1] = -rect.mExtent[1] / 2 + statusY;
		mUITextStatus->SetRelativePosition(rect);

		// Fade out
		SColor finalColor = mStatusTextInitialColor;
		finalColor.SetAlpha(0);
		SColor fadeColor = mStatusTextInitialColor.GetInterpolatedQuadratic(
			mStatusTextInitialColor, finalColor, mStatusTextTime / statusTextTimeMax);
		mUITextStatus->SetOverrideColor(fadeColor);
		mUITextStatus->EnableOverrideColor(true);
	}

	// Hide chat when console is visible
	mUITextChat->SetVisible(IsChatVisible() && !chatConsole->IsVisible());
}

void QuakeUI::ShowTranslatedStatusText(const char* str)
{
	ShowStatusText(ToWideString(str));
}

void QuakeUI::SetChatText(const EnrichedString& chatText, unsigned int recentChatCount)
{

	// Update gui element size and position
	int chatY = 5;
	int chatFontHeight = mUITextChat->GetActiveFont()->GetDimension(L"Ay")[1];
	if (mFlags.showDebug)
		chatY += 2 * chatFontHeight;

	const Vector2<unsigned int>& windowSize = Renderer::Get()->GetScreenSize();

	RectangleShape<2, int> chatSize;
	chatSize.mExtent[0] = windowSize[0] - 30;
	chatSize.mExtent[1] = std::min((int)windowSize[1], mUITextChat->GetTextHeight() + chatY);
	chatSize.mCenter[0] = chatSize.mExtent[0] / 2 + 10;
	chatSize.mCenter[1] = chatSize.mExtent[1] / 2;

	mUITextChat->SetRelativePosition(chatSize);
	mUITextChat->SetText(chatText.C_Str());

	mRecentChatCount = recentChatCount;
}

void QuakeUI::UpdateProfiler()
{
	if (mProfilerCurrentPage != 0)
	{
		std::ostringstream os(std::ios_base::binary);
		os << "   Profiler page " << (int)mProfilerCurrentPage <<
			", elapsed: " << Profiling->GetElapsedTime() << " ms)" << std::endl;

		int lines = Profiling->Print(os, mProfilerCurrentPage, mProfilerMaxPage);
		++lines;

		EnrichedString str(ToWideString(os.str()));
		str.SetBackground(SColor(120, 0, 0, 0));

		Vector2<int> size = mUITextProfiler->GetOverrideFont()->GetDimension(str.C_Str());
		Vector2<int> upperLeft{ 6, 50 };
		Vector2<int> lowerRight = upperLeft;
		lowerRight[0] += size[0] + 10;
		lowerRight[1] += size[1];

		RectangleShape<2, int> rect;
		rect.mExtent = lowerRight - upperLeft;
		rect.mCenter = upperLeft + rect.mExtent / 2;
		mUITextProfiler->SetRelativePosition(rect);

		mUITextProfiler->SetDrawBackground(true);
		mUITextProfiler->SetBackgroundColor(str.GetBackground());
		mUITextProfiler->SetText(str.C_Str());
	}

	mUITextProfiler->SetVisible(mProfilerCurrentPage != 0);
}

void QuakeUI::ToggleChat()
{
	mFlags.showChat = !mFlags.showChat;
	if (mFlags.showChat)
		ShowTranslatedStatusText("Chat shown");
	else
		ShowTranslatedStatusText("Chat hidden");
}

void QuakeUI::ToggleHud()
{
	mFlags.showHud = !mFlags.showHud;
	if (mFlags.showHud)
		ShowTranslatedStatusText("HUD shown");
	else
		ShowTranslatedStatusText("HUD hidden");
}

void QuakeUI::ToggleProfiler()
{
	mProfilerCurrentPage = (mProfilerCurrentPage + 1) % (mProfilerMaxPage + 1);

	// FIXME: This updates the profiler with incomplete values
	UpdateProfiler();

	if (mProfilerCurrentPage != 0)
	{
		wchar_t buf[255];
		wsprintf(buf, L"Profiler shown (page %d of %d)", mProfilerCurrentPage, mProfilerMaxPage);
		ShowStatusText(buf);
	}
	else ShowTranslatedStatusText("Profiler hidden");
}

/*
	Draws a screen with a single text on it.
	Text will be removed when the screen is drawn the next time.
	Additionally, a progressbar can be drawn when percent is set between 0 and 100.
*/
void QuakeUI::ShowOverlayMessage(const std::wstring& text,
	float dTime, int percent, bool drawClouds)
{
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

	Vector2<int> texSize = mUITextChat->GetActiveFont()->GetDimension(text);
	Vector2<int> center{ (int)screenSize[0] / 2, (int)screenSize[1] / 2 };
	RectangleShape<2, int> textRect;
	textRect.mExtent = texSize;
	textRect.mCenter = center;

	std::shared_ptr<BaseUIStaticText> uiText = AddStaticText(text.c_str(), textRect, false, false);
	uiText->SetTextAlignment(UIA_CENTER, UIA_UPPERLEFT);

	// draw progress bar
	if ((percent >= 0) && (percent <= 100))
	{
		std::string texturePath = "Art/Quake/textures/";

		std::shared_ptr<Texture2> progressImg;
		if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar.png")))
		{
			std::shared_ptr<ResHandle> resHandle =
				ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar.png")));
			if (resHandle)
			{
				std::shared_ptr<ImageResourceExtraData> resData =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				if (resData)
				{
					progressImg = resData->GetImage();
					progressImg->AutogenerateMipmaps();
				}
			}
		}

		std::shared_ptr<Texture2> progressImgBG;
		if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar_bg.png")))
		{
			std::shared_ptr<ResHandle> resHandle =
				ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar_bg.png")));
			if (resHandle)
			{
				std::shared_ptr<ImageResourceExtraData> resData =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				if (resData)
				{
					progressImgBG = resData->GetImage();
					progressImgBG->AutogenerateMipmaps();
				}
			}
		}

		if (progressImg && progressImgBG)
		{
			Renderer::Get()->SetBlendState(mBlendState);

			int imgW = std::clamp((int)progressImgBG->GetDimension(0), 200, 600);
			int imgH = std::clamp((int)progressImgBG->GetDimension(1), 24, 72);

			Vector2<int> imgPos{ ((int)screenSize[0] - imgW) / 2, ((int)screenSize[1] - imgH) / 2 };

			auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
			effect->SetTexture(progressImgBG);

			RectangleShape<2, int> rect;
			rect.mExtent = Vector2<int>{ imgW, imgH };
			rect.mCenter = rect.mExtent / 2 + imgPos;

			RectangleShape<2, int> tcoordRect;
			tcoordRect.mExtent = Vector2<int>{
				(int)effect->GetTexture()->GetDimension(0) ,
				(int)effect->GetTexture()->GetDimension(1) };
			tcoordRect.mCenter = tcoordRect.mExtent / 2;

			GetSkin()->Draw2DTextureFilterScaled(mVisual, rect, tcoordRect);

			effect->SetTexture(progressImg);

			rect.mExtent = Vector2<int>{ (percent * imgW) / 100, imgH };
			rect.mCenter = rect.mExtent / 2 + imgPos;

			tcoordRect.mExtent = Vector2<int>{
				(percent * (int)effect->GetTexture()->GetDimension(0)) / 100 ,
				(int)effect->GetTexture()->GetDimension(1) };
			tcoordRect.mCenter = tcoordRect.mExtent / 2;

			GetSkin()->Draw2DTextureFilterScaled(mVisual, rect, tcoordRect);

			Renderer::Get()->SetDefaultBlendState();
		}
	}

	BaseUI::OnRender(0, 0);
	uiText->Remove();
}

bool QuakeUI::IsMenuActive()
{
	return mIsMenuActive;
}

void QuakeUI::SetMenuActive(bool active)
{
	mIsMenuActive = active;
}

bool QuakeUI::OnRestore()
{
	return BaseUI::OnRestore();
}

bool QuakeUI::OnRender(double time, float elapsedTime)
{
	TimeTaker ttDraw("Draw scene");

	RunStats stats = { 0 };
	ProfilerGraph graph = ProfilerGraph(this);

	Vector2<unsigned int> previousScreenSize{
		Settings::Get()->GetUInt16("screen_w"), Settings::Get()->GetUInt16("screen_h") };

	const Vector2<unsigned int>& currentScreenSize = Renderer::Get()->GetScreenSize();
	// Verify if window size has changed and save it if it's the case
	// Ensure evaluating settings->getBool after verifying screensize
	// First condition is cheaper
	if (previousScreenSize != currentScreenSize &&
		currentScreenSize != Vector2<unsigned int>::Zero() &&
		Settings::Get()->GetBool("autosave_screensize"))
	{
		Settings::Get()->SetUInt16("screen_w", currentScreenSize[0]);
		Settings::Get()->SetUInt16("screen_h", currentScreenSize[1]);
		previousScreenSize = currentScreenSize;
	}

	// Prepare render data for next iteration
	ClearInfoText();

	SColor skyColor = Renderer::Get()->GetClearColor().ToSColor();
	
	if (mGameView->mCamera->GetTarget())
	{
		std::shared_ptr<PlayerActor> player(std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(mGameView->mCamera->GetTarget()->GetId()).lock()));
		if (player->GetState().hudFlags & HUD_FLAG_CROSSHAIR_VISIBLE)
			mHud->DrawCrosshair(L"art/quake/gfx/2d/crosshair2.png");
		mHud->DrawElements(player);
	}

	/*
		Profiler graph
	*/
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
	if (mFlags.showProfilerGraph)
		graph.Draw(10, screenSize[1] - 10, GetBuiltInFont());

	/*
		Damage flash
	*/
	if (mDamageFlash > 0.0f)
	{
		RectangleShape<2, int> rect;
		rect.mExtent = Vector2<int>{ (int)screenSize[0], (int)screenSize[1] };
		rect.mCenter = rect.mExtent / 2;
		SColor color((uint32_t)mDamageFlash, 180, 0, 0);

		// Create a vertex buffer for a single triangle.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/ColorEffectVS.glsl");
		path.push_back("Effects/ColorEffectPS.glsl");
#else
		path.push_back("Effects/ColorEffectVS.hlsl");
		path.push_back("Effects/ColorEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

		// Create the geometric object for drawing.
		std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
		GetSkin()->Draw2DRectangle(color, visual, rect,
			&GetRootUIElement()->GetAbsoluteClippingRect());

		mDamageFlash -= 384.0f * elapsedTime / 1000.f;
	}

	if (!BaseUI::OnRender(time, elapsedTime))
		return false;

	Profiling->GraphAdd("Render frame [ms]", (float)ttDraw.Stop(true));

	/* Log times and stuff for visualization */
	Profiler::GraphValues values;
	Profiling->GraphGet(values);
	graph.Put(values);

	return true;
};

bool QuakeUI::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc( evt );
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool QuakeUI::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_UI_EVENT)
	{
		int id = evt.mUIEvent.mCaller->GetID();
	}

	return false;
}


//========================================================================
//
// QuakeHumanView Implementation
//
//========================================================================

void TextureUpdateProgress(void* args, unsigned int progress, unsigned int maxProgress)
{
	TextureUpdateArgs* targs = (TextureUpdateArgs*)args;
	unsigned int curPercent = (unsigned int)ceil(progress / (double)maxProgress * 100.);

	// update the loading menu -- if neccessary
	bool doDraw = false;
	unsigned int timeMs = targs->lastTimeMs;
	if (curPercent != targs->lastPercent)
	{
		targs->lastPercent = curPercent;
		timeMs = Timer::GetRealTime();
		// only draw when the user will notice something:
		doDraw = (timeMs - targs->lastTimeMs > 100);
	}

	if (doDraw)
	{
		targs->lastTimeMs = timeMs;
		std::basic_stringstream<wchar_t> strm;
		strm << targs->textBase << " " << targs->lastPercent << "%...";
		DrawLoadScreen(strm.str(), targs->ui, targs->visual, targs->blendState,
			72 + (uint16_t)((18. / 100.) * (double)targs->lastPercent));
	}
}

/*
	Draws a screen with a single text on it.
	Text will be removed when the screen is drawn the next time.
	Additionally, a progressbar can be drawn when percent is set between 0 and 100.
*/
void DrawLoadScreen(const std::wstring& text, std::shared_ptr<BaseUI> ui,
	std::shared_ptr<Visual> visual, std::shared_ptr<BlendState> blendState, int percent)
{
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

	Vector2<int> texSize = ui->GetSkin()->GetFont()->GetDimension(text);
	Vector2<int> center{ (int)screenSize[0] / 2, (int)screenSize[1] / 2 };
	RectangleShape<2, int> textRect;
	textRect.mExtent = texSize;
	textRect.mCenter = center;

	std::shared_ptr<BaseUIStaticText> uiText = ui->AddStaticText(text.c_str(), textRect, false, false);
	uiText->SetTextAlignment(UIA_CENTER, UIA_UPPERLEFT);

	Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
	Renderer::Get()->ClearBuffers();

	// draw progress bar
	if ((percent >= 0) && (percent <= 100))
	{
		std::string texturePath = "Art/Quake/textures/";

		std::shared_ptr<Texture2> progressImg;
		if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar.png")))
		{
			std::shared_ptr<ResHandle> resHandle =
				ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar.png")));
			if (resHandle)
			{
				std::shared_ptr<ImageResourceExtraData> resData =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				if (resData)
				{
					progressImg = resData->GetImage();
					progressImg->AutogenerateMipmaps();
				}
			}
		}

		std::shared_ptr<Texture2> progressImgBG;
		if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar_bg.png")))
		{
			std::shared_ptr<ResHandle> resHandle =
				ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar_bg.png")));
			if (resHandle)
			{
				std::shared_ptr<ImageResourceExtraData> resData =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				if (resData)
				{
					progressImgBG = resData->GetImage();
					progressImgBG->AutogenerateMipmaps();
				}
			}
		}

		if (progressImg && progressImgBG)
		{
			Renderer::Get()->SetBlendState(blendState);

			int imgW = std::clamp((int)progressImgBG->GetDimension(0), 200, 600);
			int imgH = std::clamp((int)progressImgBG->GetDimension(1), 24, 72);

			Vector2<int> imgPos{ ((int)screenSize[0] - imgW) / 2, ((int)screenSize[1] - imgH) / 2 };

			auto effect = std::dynamic_pointer_cast<Texture2Effect>(visual->GetEffect());
			effect->SetTexture(progressImgBG);

			RectangleShape<2, int> rect;
			rect.mExtent = Vector2<int>{ imgW, imgH };
			rect.mCenter = rect.mExtent / 2 + imgPos;

			RectangleShape<2, int> tcoordRect;
			tcoordRect.mExtent = Vector2<int>{
				(int)effect->GetTexture()->GetDimension(0) ,
				(int)effect->GetTexture()->GetDimension(1) };
			tcoordRect.mCenter = tcoordRect.mExtent / 2;

			ui->GetSkin()->Draw2DTextureFilterScaled(visual, rect, tcoordRect);

			effect->SetTexture(progressImg);

			Vector2<int> offset = Vector2<int>{ 2, 0 };
			rect.mExtent = Vector2<int>{ (percent * imgW) / 100, imgH } - offset;
			rect.mCenter = rect.mExtent / 2 + imgPos + offset;

			tcoordRect.mExtent = Vector2<int>{
				(percent * (int)effect->GetTexture()->GetDimension(0)) / 100 ,
				(int)effect->GetTexture()->GetDimension(1) };
			tcoordRect.mCenter = tcoordRect.mExtent / 2;

			ui->GetSkin()->Draw2DTextureFilterScaled(visual, rect, tcoordRect);

			Renderer::Get()->SetDefaultBlendState();
		}
	}

	ui->BaseUI::OnRender(0, 0);
	uiText->Remove();

	Renderer::Get()->DisplayColorBuffer(0);
}

/*
On some computers framerate doesn't seem to be automatically limited
 */
void LimitFps(FpsControl* fpsTimings, float* dTime)
{
	// not using getRealTime is necessary for wine
	Timer::Tick(); // Maker sure device time is up-to-date
	unsigned int time = Timer::GetTime();
	unsigned int lastTime = fpsTimings->lastTime;

	if (time > lastTime)  // Make sure time hasn't overflowed
		fpsTimings->busyTime = time - lastTime;
	else
		fpsTimings->busyTime = 0;

	unsigned int frameTimeMin = (unsigned int)(1000 / (System::Get()->IsWindowFocused() ?
		Settings::Get()->GetFloat("fps_max") : Settings::Get()->GetFloat("fps_max_unfocused")));

	if (fpsTimings->busyTime < frameTimeMin)
	{
		fpsTimings->sleepTime = frameTimeMin - fpsTimings->busyTime;
		Sleep(fpsTimings->sleepTime);
	}
	else fpsTimings->sleepTime = 0;

	/* Get the new value of the device timer. Note that device->sleep() may
	 * not sleep for the entire requested time as sleep may be interrupted and
	 * therefore it is arguably more accurate to get the new time from the
	 * device rather than calculating it by adding sleep_time to time.
	 */

	Timer::Tick(); // Update device timer
	time = Timer::GetTime();

	if (time > lastTime)  // Make sure last_time hasn't overflowed
		*dTime = (time - lastTime) / 1000.f;
	else
		*dTime = 0;

	fpsTimings->lastTime = time;
}

void GameSettings::ReadGlobalSettings()
{
	doubletapJump = Settings::Get()->GetBool("doubletap_jump");
	enableClouds = Settings::Get()->GetBool("enable_clouds");
	enableParticles = Settings::Get()->GetBool("enable_particles");
	enableFog = Settings::Get()->GetBool("enable_fog");
	mouseSensitivity = Settings::Get()->GetFloat("mouse_sensitivity");
	repeatPlaceTime = Settings::Get()->GetFloat("repeat_place_time");

	enableNoclip = Settings::Get()->GetBool("noclip");
	enableFreeMove = Settings::Get()->GetBool("free_move");

	fogStart = Settings::Get()->GetFloat("fog_start");

	cameraSmoothing = 0;
	if (Settings::Get()->GetBool("cinematic"))
		cameraSmoothing = 1 - Settings::Get()->GetFloat("cinematic_camera_smoothing");
	else
		cameraSmoothing = 1 - Settings::Get()->GetFloat("camera_smoothing");

	fogStart = std::clamp(fogStart, 0.0f, 0.99f);
	cameraSmoothing = std::clamp(cameraSmoothing, 0.01f, 1.0f);
	mouseSensitivity = std::clamp(mouseSensitivity, 0.001f, 100.0f);
}

void QuakeHumanView::SettingsChangedCallback(const std::string& name, void* data)
{
	((GameSettings*)data)->ReadGlobalSettings();
}


//
// QuakeHumanView::QuakeHumanView	- Chapter 19, page 724
//
QuakeHumanView::QuakeHumanView() : HumanView()
{ 
	mShowUI = true; 
	mDebugMode = DM_OFF;

	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		// Create a vertex buffer for a two-triangles square. The PNG is stored
		// in left-handed coordinates. The texture coordinates are chosen to
		// reflect the texture in the y-direction.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
		vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		// Create an effect for the vertex and pixel shaders. The texture is
		// bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extraRes =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extraRes->GetProgram())
			extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		auto effect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
	}

    RegisterAllDelegates();

	mGameSettings.ReadGlobalSettings();
	// Register game setting callbacks
	for (const std::string& name : mGameSettings.settingNames)
	{
		Settings::Get()->RegisterChangedCallback(name,
			&QuakeHumanView::SettingsChangedCallback, &mGameSettings);
	}
}


QuakeHumanView::~QuakeHumanView()
{
    RemoveAllDelegates();

	// mGameSettings becomes invalid, remove callbacks
	for (const std::string& name : mGameSettings.settingNames)
	{
		Settings::Get()->DeregisterChangedCallback(name,
			&QuakeHumanView::SettingsChangedCallback, &mGameSettings);
	}

	Shutdown();
}

//
// QuakeHumanView::OnMsgProc				- Chapter 19, page 727
//
bool QuakeHumanView::OnMsgProc( const Event& evt )
{
    if (!mGameUI->mUIChatConsole->IsOpen())
    {
        switch (evt.mEventType)
        {
            case ET_UI_EVENT:
                // hey, why is the user sending gui events..?
                break;

            case ET_KEY_INPUT_EVENT:
            {
                if (evt.mKeyInput.mPressedDown)
                {
                    KeyAction key(evt.mKeyInput);
                    int keyType = mKeycache.Find(key);
                    if (keyType != -1)
                    {
                        if (!IsKeyDown((GameKeyType)keyType))
                            mKeyWasPressed.Set(mKeycache.keys[keyType]);

                        mKeyIsDown.Set(mKeycache.keys[keyType]);
                        mKeyWasDown.Set(mKeycache.keys[keyType]);
                    }
                }
                else
                {
                    KeyAction key(evt.mKeyInput);
                    int keyType = mKeycache.Find(key);
                    if (keyType != -1)
                    {
                        if (!IsKeyDown((GameKeyType)keyType))
                            mKeyWasReleased.Set(mKeycache.keys[keyType]);

                        mKeyIsDown.Unset(mKeycache.keys[keyType]);
                    }
                }
            }
            break;

            case ET_MOUSE_INPUT_EVENT:
            {
                KeyAction key;
                switch (evt.mMouseInput.mEvent) 
                {
                    case MIE_LMOUSE_PRESSED_DOWN:
                        key = "KEY_LBUTTON";
                        mKeyIsDown.Set(key);
                        mKeyWasDown.Set(key);
                        mKeyWasPressed.Set(key);
					    break;
                    case MIE_MMOUSE_PRESSED_DOWN:
                        key = "KEY_MBUTTON";
                        mKeyIsDown.Set(key);
                        mKeyWasDown.Set(key);
                        mKeyWasPressed.Set(key);
					    break;
                    case MIE_RMOUSE_PRESSED_DOWN:
                        key = "KEY_RBUTTON";
                        mKeyIsDown.Set(key);
                        mKeyWasDown.Set(key);
                        mKeyWasPressed.Set(key);
					    break;
                    case MIE_LMOUSE_LEFT_UP:
                        key = "KEY_LBUTTON";
                        mKeyIsDown.Unset(key);
                        mKeyWasReleased.Set(key);
                        break;
                    case MIE_MMOUSE_LEFT_UP:
                        key = "KEY_MBUTTON";
                        mKeyIsDown.Unset(key);
                        mKeyWasReleased.Set(key);
					    break;
                    case MIE_RMOUSE_LEFT_UP:
                        key = "KEY_RBUTTON";
                        mKeyIsDown.Unset(key);
                        mKeyWasReleased.Set(key);
                        break;
                    case MIE_MOUSE_WHEEL:
                        mMouseWheel = evt.mMouseInput.mWheel;
                        break;
                }
            }
            break;
        }
    }

    return HumanView::OnMsgProc(evt);
}

//
// QuakeHumanView::RenderText				- Chapter 19, page 727
//
void QuakeHumanView::RenderText()
{
	HumanView::RenderText();
}

//
// QuakeHumanView::OnRender
//
void QuakeHumanView::OnRender(double time, float elapsedTime)
{
	// Drawing begins
	Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));

	HumanView::OnRender(time, elapsedTime);
}

//
// QuakeHumanView::OnUpdate				- Chapter 19, page 730
//
void QuakeHumanView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate( timeMs, deltaMs );

	const Vector2<unsigned int>& currentScreenSize = Renderer::Get()->GetScreenSize();
	// Verify if window size has changed and save it if it's the case
	// Ensure evaluating settings->getBool after verifying screensize
	// First condition is cheaper
	if (mScreenSize != currentScreenSize &&
		currentScreenSize != Vector2<unsigned int>::Zero() &&
		Settings::Get()->GetBool("autosave_screensize"))
	{
		Settings::Get()->SetUInt16("screen_w", currentScreenSize[0]);
		Settings::Get()->SetUInt16("screen_h", currentScreenSize[1]);
		mScreenSize = currentScreenSize;
	}

	// Calculate dtime =
	//    RenderingEngine::run() from this iteration
	//  + Sleep time until the wanted FPS are reached
	LimitFps(&mUpdateTimes, &mDeltaTime);

	// Prepare render data for current iteration

	UpdateStats(&mStats, mUpdateTimes, mDeltaTime);

	UpdateProfilers(mStats, mUpdateTimes, mDeltaTime);
	ProcessUserInput(mDeltaTime);
	// Update camera before player movement to avoid camera lag of one frame
	UpdateControllers(timeMs, deltaMs);
	Step(mDeltaTime);
	UpdateSound(mDeltaTime);
	UpdateFrame(&mStats, mDeltaTime);

	if (Settings::Get()->GetBool("pause_on_lost_focus") &&
		!System::Get()->IsWindowFocused() &&
		!mGameUI->IsMenuActive())
	{
		ShowPauseMenu();
	}

	std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetForm();
	mGameUI->SetMenuActive(formUI ? formUI->IsActive() : false);
}

void QuakeHumanView::Step(float dTime)
{
	mSoundMgr->Step(dTime);


	/*
		Update positions of sounds attached to objects
	*/
	{
		for (auto const& soundsToObject : mSoundsToObjects)
		{
			int clientId = soundsToObject.first;
			uint16_t objectId = soundsToObject.second;
			std::shared_ptr<Actor> pActor = GameLogic::Get()->GetActor(objectId).lock();
			if (!pActor)
				continue;

			std::shared_ptr<TransformComponent> pTransformComponent(
				pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
				mSoundMgr->UpdateSoundPosition(clientId, pTransformComponent->GetPosition());
		}
	}


	/*
		Handle removed remotely initiated sounds
	*/
	mRemoveSoundsCheckTimer += dTime;
	if (mRemoveSoundsCheckTimer >= 2.32)
	{
		mRemoveSoundsCheckTimer = 0;
		// Find removed sounds and clear references to them
		std::vector<int> removeIds;
		for (std::unordered_map<int, int>::iterator it = mSoundsLogicToVisual.begin(); it != mSoundsLogicToVisual.end();)
		{
			int logicId = it->first;
			int visualId = it->second;
			++it;
			if (!mSoundMgr->SoundExists(logicId))
			{
				mSoundsLogicToVisual.erase(visualId);
				mSoundsVisualToLogic.erase(logicId);
				mSoundsToObjects.erase(logicId);
				removeIds.push_back(visualId);
			}
		}

		// Sync to logic
		if (!removeIds.empty())
			EventManager::Get()->TriggerEvent(std::make_shared<EventDataRemoveSounds>(removeIds));
	}
}

//
// QuakeHumanView::OnAttach				- Chapter 19, page 731
//
void QuakeHumanView::OnAttach(GameViewId vid, ActorId aid)
{
	HumanView::OnAttach(vid, aid);
}

bool QuakeHumanView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
	if (!HumanView::LoadGameDelegate(pLevelData))
		return false;

    mGameUI.reset(new QuakeUI(this));
	mGameUI->OnInit();

    PushElement(mGameUI);

	Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
	Renderer::Get()->ClearBuffers();

	mGameUI->ShowOverlayMessage(L"Loading...", 0, 0);

	Renderer::Get()->DisplayColorBuffer(0);

	//Create View
	//mGameUI->ShowOverlayMessage(L"Creating view...", mTextureSrc, 0, 10);

	// Pre-calculated values
	std::shared_ptr<Texture2> texture;
	if (FileSystem::Get()->ExistFile(ToWideString("crack_anylength.png")))
	{
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString("crack_anylength.png")));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			if (resData)
			{
				texture = resData->GetImage();
				texture->AutogenerateMipmaps();
			}
		}
	}
	
	if (texture)
		mCrackAnimationLength = texture->GetDimension(1) / texture->GetDimension(0);
	else 
		mCrackAnimationLength = 5;

	// Set window caption
	std::wstring str = L"Quake";
	System::Get()->SetWindowCaption(str.c_str());

	if (Settings::Get()->GetBool("enable_sound"))
	{
		if (!InitSound())
			return false;
	}

	mStats = { 0 };
	mUpdateTimes = { 0 };
	mUpdateTimes.lastTime = Timer::GetTime();

	/* Clear the profiler */
	Profiler::GraphValues dummyvalues;
	Profiling->GraphGet(dummyvalues);

	mScreenSize = Vector2<unsigned int>{
		Settings::Get()->GetUInt16("screen_w"), Settings::Get()->GetUInt16("screen_h") };

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataGameInit>());

	if (!GetGameContent())
	{
		LogError("Connection failed for unknown reason");
		return false;
	}

	// Update cached textures, meshes and materials
	AfterContentReceived();

	// A movement controller is going to control the camera, 
	// but it could be constructed with any of the objects you see in this function.
	mGameCameraController.reset(new QuakeCameraController(mCamera, 0, (float)GE_C_HALF_PI, true));
	mKeyboardHandler = mGameCameraController;
	mMouseHandler = mGameCameraController;
	mCamera->ClearTarget();

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataGameReady>(GetActorId()));

	mGameUI->mHud = std::make_shared<Hud>(mScene.get(), mGameUI.get());

	const std::string& drawMode = Settings::Get()->Get("mode3d");

	// A movement controller is going to control the camera, 
	// but it could be constructed with any of the objects you see in this function.
	mCamera->GetRelativeTransform().SetTranslation(Vector4<float>::Zero());
	mCamera->ClearTarget();

	mScene->OnRestore();
    return true;
}

//  Run
void QuakeHumanView::UpdateProfilers(
	const RunStats& stats, const FpsControl& updateTimes, float dTime)
{
	float profilerPrintInterval = Settings::Get()->GetFloat("profiler_print_interval");
	bool printToLog = true;

	if (profilerPrintInterval == 0)
	{
		printToLog = false;
		profilerPrintInterval = 3;
	}

	if (mProfilerInterval.Step(dTime, profilerPrintInterval))
	{
		if (printToLog)
		{
			std::stringstream infostream;
			infostream << "Profiler:" << std::endl;
			Profiling->Print(infostream);
			LogInformation(infostream.str());
		}

		mGameUI->UpdateProfiler();
		Profiling->Clear();
	}

	// Update update graphs
	Profiling->GraphAdd("Time update [ms]", (float)(updateTimes.busyTime - stats.drawTime));

	Profiling->GraphAdd("Sleep [ms]", (float)updateTimes.sleepTime);
	Profiling->GraphAdd("FPS", 1.f / dTime);
}

void QuakeHumanView::UpdateStats(RunStats* stats, const FpsControl& updateTimes, float dTime)
{
	float jitter;
	Jitter* jp;

	/* Time average and jitter calculation
	 */
	jp = &stats->dTimeJitter;
	jp->avg = jp->avg * 0.96f + dTime * 0.04f;

	jitter = dTime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;

	jp->counter += dTime;

	if (jp->counter > 0.f)
	{
		jp->counter -= 3.f;
		jp->maxSample = jp->max;
		jp->maxFraction = jp->maxSample / (jp->avg + 0.001f);
		jp->max = 0.f;
	}

	/* Busytime average and jitter calculation
	 */
	jp = &stats->busyTimeJitter;
	jp->avg = jp->avg + updateTimes.busyTime * 0.02f;

	jitter = updateTimes.busyTime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;
	if (jitter < jp->min)
		jp->min = jitter;

	jp->counter += dTime;

	if (jp->counter > 0.f)
	{
		jp->counter -= 3.f;
		jp->maxSample = jp->max;
		jp->minSample = jp->min;
		jp->max = 0.f;
		jp->min = 0.f;
	}
}


bool QuakeHumanView::GetGameContent()
{
	ClearInput();

	FpsControl fpsControl = { 0 };
	float dTime; // in seconds
	float progress = 25.f;

	fpsControl.lastTime = Timer::GetTime();

	while (System::Get()->OnRun())
	{
		LimitFps(&fpsControl, &dTime);

		// End condition
		if (mMediaReceived)
			break;

		// Display status
		const std::wstring text = L"Loading Media...";
		progress += dTime * 8;
		if (progress > 100.f) progress = 100.f;

		DrawLoadScreen(text, mGameUI, mVisual, mBlendState, (int)progress);
	}

	return true;
}

void QuakeHumanView::AfterContentReceived()
{
	LogInformation("QuakeHumanView::AfterContentReceived() started");
	LogAssert(mMediaReceived, "no media received"); // pre-condition

	std::wstring text = L"Loading textures...";

	// Clear cached pre-scaled 2D GUI images, as this cache
	// might have images with the same name but different
	// content from previous sessions.
	mGameUI->GetSkin()->ClearTextureCache();

	// Rebuild inherited images and recreate textures
	LogInformation("- Rebuilding images and textures");
	DrawLoadScreen(text, mGameUI, mVisual, mBlendState, 70);

	// Rebuild shaders
	LogInformation("- Rebuilding shaders");
	text = L"Rebuilding shaders...";
	DrawLoadScreen(text, mGameUI, mVisual, mBlendState, 71);

	// Update node aliases
	LogInformation("- Updating node aliases");
	text = L"Initializing nodes...";
	DrawLoadScreen(text, mGameUI, mVisual, mBlendState, 72);

	// Update node textures and assign shaders to each tile
	LogInformation("- Updating node textures");
	TextureUpdateArgs textureUpdateArgs;
	textureUpdateArgs.ui = mGameUI;
	textureUpdateArgs.scene = mScene.get();
	textureUpdateArgs.lastTimeMs = Timer::GetRealTime();
	textureUpdateArgs.lastPercent = 0;
	textureUpdateArgs.visual = mVisual;
	textureUpdateArgs.blendState = mBlendState;
	textureUpdateArgs.textBase = L"Initializing nodes";

	text = L"Done!";
	DrawLoadScreen(text, mGameUI, mVisual, mBlendState, 100);
	LogInformation("QuakeHumanView::afterContentReceived() done");

	mGameState = BGS_RUNNING;
}


/****************************************************************************
 Input handling
 ****************************************************************************/

void QuakeHumanView::ProcessUserInput(float dTime)
{
	// Reset input if window not active or some menu is active
	if (mGameUI->IsMenuActive() ||
		mGameUI->HasFocus(mGameUI->mUIChatConsole) ||
		!System::Get()->IsWindowActive())
	{
		ClearInput();
	}

	if (!mGameUI->HasFocus(mGameUI->mUIChatConsole) && mGameUI->mUIChatConsole->IsOpen())
		mGameUI->mUIChatConsole->CloseConsoleAtOnce();

	ProcessKeyInput();
}

void QuakeHumanView::ProcessKeyInput()
{
	if (CancelPressed())
	{
		if (!mGameUI->IsMenuActive() &&
			!mGameUI->mUIChatConsole->IsOpenInhibited())
		{
			ShowPauseMenu();
		}
	}
	else if (WasKeyDown(KeyType::SLOT_6))
	{
		QuakeLogic * twg = static_cast<QuakeLogic*>(GameLogic::Get());
		twg->ToggleRenderDiagnostics();
	}
	else if (WasKeyDown(KeyType::SLOT_7))
	{
		mDebugMode = mDebugMode ? DM_OFF : DM_WIREFRAME;
		for (auto const& child : mScene->GetRootNode()->GetChildren())
			child->SetDebugState(mDebugMode);
	}
	else if (WasKeyDown(KeyType::SLOT_8))
	{
		if (mPlayer)
		{
			const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
			for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
			{
				std::shared_ptr<BaseGameView> pView = *it;
				if (pView->GetType() == GV_HUMAN)
				{
					std::shared_ptr<QuakeHumanView> pHumanView =
						std::static_pointer_cast<QuakeHumanView, BaseGameView>(pView);
					if (pHumanView->GetActorId() != mPlayer->GetId())
					{
						mPlayer = mScene->GetSceneNode(pHumanView->GetActorId());

						if (mGamePlayerController)
							mGamePlayerController->SetEnabled(true);
						if (mGameCameraController)
							mGameCameraController->SetEnabled(false);

						mKeyboardHandler = mGamePlayerController;
						mMouseHandler = mGamePlayerController;
						mCamera->SetTarget(mPlayer);

						EventManager::Get()->QueueEvent(
							std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
						break;
					}
				}
				else if (pView->GetType() == GV_REMOTE)
				{
					std::shared_ptr<NetworkGameView> pNetworkGameView =
						std::static_pointer_cast<NetworkGameView, BaseGameView>(pView);
					if (pNetworkGameView->GetActorId() != mPlayer->GetId())
					{
						mPlayer = mScene->GetSceneNode(pNetworkGameView->GetActorId());

						if (mGamePlayerController)
							mGamePlayerController->SetEnabled(false);
						if (mGameCameraController)
							mGameCameraController->SetEnabled(false);

						mKeyboardHandler = NULL;
						mMouseHandler = NULL;
						mCamera->SetTarget(mPlayer);

						EventManager::Get()->QueueEvent(
							std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
						break;
					}
				}
				else if (pView->GetType() == GV_AI)
				{
					std::shared_ptr<QuakeAIView> pAiView =
						std::static_pointer_cast<QuakeAIView, BaseGameView>(pView);
					if (pAiView->GetActorId() != mPlayer->GetId())
					{
						mPlayer = mScene->GetSceneNode(pAiView->GetActorId());

						if (mGamePlayerController)
							mGamePlayerController->SetEnabled(false);
						if (mGameCameraController)
							mGameCameraController->SetEnabled(false);

						mKeyboardHandler = NULL;
						mMouseHandler = NULL;
						mCamera->SetTarget(mPlayer);

						EventManager::Get()->QueueEvent(
							std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
						break;
					}
				}
			}
		}
		else
		{
			SetControlledActor(mActorId);

			if (mGamePlayerController)
				mGamePlayerController->SetEnabled(true);
			if (mGameCameraController)
				mGameCameraController->SetEnabled(false);

			mKeyboardHandler = mGamePlayerController;
			mMouseHandler = mGamePlayerController;
			mCamera->SetTarget(mPlayer);

			EventManager::Get()->QueueEvent(
				std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
		}
	}
	else if (WasKeyDown(KeyType::SLOT_9))
	{
		if (mGamePlayerController)
			mGamePlayerController->SetEnabled(false);
		if (mGameCameraController)
			mGameCameraController->SetEnabled(true);

		mKeyboardHandler = mGameCameraController;
		mMouseHandler = mGameCameraController;
		mCamera->ClearTarget();

		if (mPlayer)
		{
			EventManager::Get()->QueueEvent(
				std::make_shared<EventDataRemoveControlledActor>(mPlayer->GetId()));
		}
	}
	else if (WasKeyDown(KeyType::CHAT))
	{
		OpenConsole(0.2f, L"");
	}
	else if (WasKeyDown(KeyType::CMD))
	{
		OpenConsole(0.2f, L"/");
	}
	else if (WasKeyDown(KeyType::CMD_LOCAL))
	{
		mGameUI->ShowStatusText(L"Visual side scripting is disabled");
	}
	else if (WasKeyDown(KeyType::CONSOLE))
	{
		OpenConsole(std::clamp(Settings::Get()->GetFloat("console_height"), 0.1f, 1.0f));
	}
	else if (WasKeyDown(KeyType::PITCHMOVE))
	{
		TogglePitchMove();
	}
	else if (WasKeyDown(KeyType::FASTMOVE))
	{
		ToggleFast();
	}
	else if (WasKeyDown(KeyType::NOCLIP))
	{
		ToggleNoClip();
	}
	else if (WasKeyDown(KeyType::MUTE))
	{
		if (Settings::Get()->GetBool("enable_sound"))
		{
			bool newMuteSound = !Settings::Get()->GetBool("mute_sound");
			Settings::Get()->SetBool("mute_sound", newMuteSound);
			if (newMuteSound)
				mGameUI->ShowTranslatedStatusText("Sound muted");
			else
				mGameUI->ShowTranslatedStatusText("Sound unmuted");
		}
		else mGameUI->ShowTranslatedStatusText("Sound system is disabled");
	}
	else if (WasKeyDown(KeyType::INC_VOLUME))
	{
		if (Settings::Get()->GetBool("enable_sound"))
		{
			float newVolume = std::clamp(Settings::Get()->GetFloat("sound_volume") + 0.1f, 0.0f, 1.0f);
			wchar_t buf[100];
			Settings::Get()->SetFloat("sound_volume", newVolume);
			const wchar_t* str = L"Volume changed to %d%%";
			swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, round(newVolume * 100));
			delete[] str;
			mGameUI->ShowStatusText(buf);
		}
		else mGameUI->ShowTranslatedStatusText("Sound system is disabled");
	}
	else if (WasKeyDown(KeyType::DEC_VOLUME))
	{
		if (Settings::Get()->GetBool("enable_sound"))
		{
			float newVolume = std::clamp(Settings::Get()->GetFloat("sound_volume") - 0.1f, 0.0f, 1.0f);
			wchar_t buf[100];
			Settings::Get()->SetFloat("sound_volume", newVolume);
			const wchar_t* str = L"Volume changed to %d%%";
			swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, round(newVolume * 100));
			delete[] str;
			mGameUI->ShowStatusText(buf);
		}
		else mGameUI->ShowTranslatedStatusText("Sound system is disabled");
	}
	else if (WasKeyDown(KeyType::CINEMATIC))
	{
		ToggleCinematic();
	}
	else if (WasKeyDown(KeyType::TOGGLE_HUD))
	{
		mGameUI->ToggleHud();
	}
	else if (WasKeyDown(KeyType::TOGGLE_CHAT))
	{
		mGameUI->ToggleChat();
	}
	else if (WasKeyDown(KeyType::TOGGLE_FOG))
	{
		ToggleFog();
	}
	else if (WasKeyDown(KeyType::TOGGLE_DEBUG))
	{
		ToggleDebug();
	}
	else if (WasKeyDown(KeyType::TOGGLE_PROFILER))
	{
		mGameUI->ToggleProfiler();
	}
	else if (WasKeyDown(KeyType::INCREASE_VIEWING_RANGE))
	{
		IncreaseViewRange();
	}
	else if (WasKeyDown(KeyType::DECREASE_VIEWING_RANGE))
	{
		DecreaseViewRange();
	}
	else if (WasKeyDown(KeyType::RANGESELECT))
	{
		ToggleFullViewRange();
	}
	else if (WasKeyDown(KeyType::ZOOM))
	{
		CheckZoomEnabled();
	}
}

void QuakeHumanView::UpdateFrame(RunStats* stats, float dTime)
{
	TimeTaker ttUpdate("UpdateFrame");

	mGameUI->Update(*stats, mCamera, mGameUI->mUIChatConsole, dTime);

	/*
	   make sure menu is on top
	   1. Delete form menu reference if menu was removed
	   2. Else, make sure form menu is on top
	*/
	do
	{
		// breakable. only runs for one iteration
		if (!mGameUI->GetForm())
			break;

		if (!mGameUI->GetForm()->IsActive())
		{
			mGameUI->DeleteForm();
			break;
		}

		std::shared_ptr<UIForm>& formUI =
			std::static_pointer_cast<UIForm>(mGameUI->GetForm());
		//formUI->QuitForm();

		if (mGameUI->IsMenuActive())
			mGameUI->GetRootUIElement()->BringToFront(formUI);

	} while (false);

	Profiling->GraphAdd("Update frame [ms]", (float)ttUpdate.Stop(true));
}


bool QuakeHumanView::LoadMedia(const std::string& filePath, bool fromMediaPush)
{
	std::string name;

	const char* imageExt[] = {
		".png", ".jpg", ".bmp", ".tga",
		".pcx", ".ppm", ".psd", ".wal", ".rgb", NULL };
	name = StringRemoveEnd(filePath, imageExt);
	if (!name.empty())
	{
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(filePath)));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			auto fileName = FileSystem::Get()->GetFileName(ToWideString(filePath));
			std::shared_ptr<Texture2> texture = resData->GetImage();
			texture->SetName(fileName);
			texture->AutogenerateMipmaps();

			return true;
		}

		LogWarning("Couldn't load image file \"" + filePath + "\"");
		return false;
	}

	const char* soundExt[] = {".wav", ".ogg", NULL };
	name = StringRemoveEnd(filePath, soundExt);
	if (!name.empty())
	{
		auto fileName = FileSystem::Get()->GetFileName(ToWideString(name));
		if (mSoundMgr->LoadSound(ToString(fileName.c_str()), filePath))
			return true;

		LogWarning("Couldn't load sound file \"" + filePath + "\"");
		return false;
	}

	const char* modelExt[] = { ".bsp", ".pk3", ".md3", NULL };
	name = StringRemoveEnd(filePath, modelExt);
	if (!name.empty())
	{
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(filePath)));
		if (resHandle)
			return true;

		LogWarning("Couldn't load model into memory : \"" + filePath + "\"");
		return false;
	}

	const char* translate_ext[] = { ".tr", NULL };
	name = StringRemoveEnd(filePath, translate_ext);
	if (!name.empty())
	{
		if (fromMediaPush)
			return false;

		LogInformation("Loading translation: \"" + filePath + "\"");
		//LoadTranslation(data);
		return true;
	}

	LogError("Unrecognized file format to load \"" + filePath + "\"");
	return false;
}

std::string QuakeHumanView::GetModStoragePath() const
{
	std::string currentDir = ToString(FileSystem::Get()->GetWorkingDirectory());
	return currentDir + "/mod_storage";
}

void QuakeHumanView::SetControlledActor(ActorId actorId)
{
	mPlayer = mScene->GetSceneNode(actorId);
	if (!mPlayer)
	{
		LogError("Invalid player");
		return;
	}

	HumanView::SetControlledActor(actorId);

	AxisAngle<4, float> localRotation;
	mPlayer->GetRelativeTransform().GetRotation(localRotation);
	float yaw = localRotation.mAngle * localRotation.mAxis[AXIS_Y];
	mGamePlayerController.reset(new QuakePlayerController(mPlayer, yaw, 0.f));

	mKeyboardHandler = mGamePlayerController;
	mMouseHandler = mGamePlayerController;
}


void QuakeHumanView::ShowFormDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowForm> pCastEventData =
		std::static_pointer_cast<EventDataShowForm>(pEventData);

	std::string showForm = pCastEventData->GetForm();
	std::string showFormName = pCastEventData->GetFormName();

	if (!showForm.empty())
	{
		std::string formPr;
		std::shared_ptr<FormSource> formSrc(new FormSource(showForm));
		std::shared_ptr<TextDestination> textDst(new TextDestination());

		RectangleShape<2, int> rectangle;
		rectangle.mCenter = Vector2<int>{ 50, 50 };
		rectangle.mExtent = Vector2<int>{ 100, 100 };

		std::shared_ptr<BaseUIForm>& formUI = mGameUI->UpdateForm(showFormName);
		if (formUI)
		{
			formUI->SetFormPrepend(formPr);
			formUI->SetFormSource(formSrc);
			formUI->SetTextDestination(textDst);
		}
		else
		{
			formUI.reset(new UIForm(mGameUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
			formUI->SetParent(mGameUI->GetRootUIElement());
			formUI->OnInit();
		}
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetForm();
		if (formUI && showFormName.empty() || showForm == mGameUI->GetFormName())
			formUI->QuitForm();
	};
}


void QuakeHumanView::InitChatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataInitChat> pCastEventData =
		std::static_pointer_cast<EventDataInitChat>(pEventData);

	mGameUI->mUIChatConsole->SetChat(pCastEventData->GetChat());
}


void QuakeHumanView::UpdateChatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataUpdateChat> pCastEventData =
		std::static_pointer_cast<EventDataUpdateChat>(pEventData);

	// Display all messages in a static text element
	mGameUI->SetChatText(pCastEventData->GetChat(), pCastEventData->GetLineCount());
}

void QuakeHumanView::HandlePlaySoundAtDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPlaySoundAt> pCastEventData =
		std::static_pointer_cast<EventDataPlaySoundAt>(pEventData);

	mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
		pCastEventData->GetGain(), pCastEventData->GetPosition(), pCastEventData->GetPitch());
}

void QuakeHumanView::HandlePlaySoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPlaySoundType> pCastEventData =
		std::static_pointer_cast<EventDataPlaySoundType>(pEventData);

	// Start playing
	int soundId = -1;
	Vector3<float> pos = pCastEventData->GetPosition();
	switch (pCastEventData->GetType())
	{
		case 0: // local
			soundId = mSoundMgr->PlaySoundGlobal(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
				pCastEventData->GetGain(), pCastEventData->GetFade(), pCastEventData->GetPitch());
			break;
		case 1: // positional
			soundId = mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
				pCastEventData->GetGain(), pCastEventData->GetPosition(), pCastEventData->GetPitch());
			break;
		case 2:
		{
			// object
			std::shared_ptr<Actor> pActor = GameLogic::Get()->GetActor(pCastEventData->GetObjectId()).lock();
			if (pActor)
			{
				std::shared_ptr<TransformComponent> pTransformComponent(
					pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
				if (pTransformComponent)
					pos = pTransformComponent->GetPosition();
			}

			soundId = mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
				pCastEventData->GetGain(), pos, pCastEventData->GetPitch());
			break;
		}
		default:
			break;
	}

	if (soundId != -1)
	{
		// for ephemeral sounds, id is not meaningful
		if (!pCastEventData->IsEphemeral())
		{
			mSoundsLogicToVisual[pCastEventData->GetId()] = soundId;
			mSoundsVisualToLogic[soundId] = pCastEventData->GetId();
		}
		if (pCastEventData->GetObjectId() != 0)
			mSoundsToObjects[soundId] = pCastEventData->GetObjectId();
	}
}

void QuakeHumanView::HandleStopSoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataStopSound> pCastEventData =
		std::static_pointer_cast<EventDataStopSound>(pEventData);

	std::unordered_map<int, int>::iterator itSound = mSoundsLogicToVisual.find(pCastEventData->GetId());
	if (itSound != mSoundsLogicToVisual.end())
	{
		int soundId = itSound->second;
		mSoundMgr->StopSound(soundId);
	}
}

void QuakeHumanView::HandleFadeSoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFadeSound> pCastEventData =
		std::static_pointer_cast<EventDataFadeSound>(pEventData);

	std::unordered_map<int, int>::const_iterator itSound = mSoundsLogicToVisual.find(pCastEventData->GetId());
	if (itSound != mSoundsLogicToVisual.end())
	{
		int soundId = itSound->second;
		mSoundMgr->FadeSound(soundId, pCastEventData->GetStep(), pCastEventData->GetGain());
	}
}

void QuakeHumanView::ChangeVolumeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeVolume> pCastEventData =
		std::static_pointer_cast<EventDataChangeVolume>(pEventData);

	RectangleShape<2, int> rect;
	rect.mExtent = Vector2<int>{ 100, 100 };
	rect.mCenter = rect.mExtent / 2;

	std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetForm();
	formUI = std::make_shared<UIVolumeChange>(mGameUI.get(), -1, rect);
	formUI->SetParent(mGameUI->GetRootUIElement());
	formUI->OnInit();
}

void QuakeHumanView::HandleMediaDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataHandleMedia> pCastEventData =
		std::static_pointer_cast<EventDataHandleMedia>(pEventData);

	// Check media cache
	for (auto const& media : pCastEventData->GetMedia())
		LoadMedia(ToString(media.second));

	mMediaReceived = true;
}

void QuakeHumanView::ChangeMenuDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeMenu> pCastEventData =
		std::static_pointer_cast<EventDataChangeMenu>(pEventData);

	RectangleShape<2, int> rect;
	rect.mExtent = Vector2<int>{ 100, 100 };
	rect.mCenter = rect.mExtent / 2;

	std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetForm();
	formUI = std::make_shared<UIKeyChange>(mGameUI.get(), -1, rect);
	formUI->SetParent(mGameUI->GetRootUIElement());
	formUI->OnInit();
}


void QuakeHumanView::GameplayUiUpdateDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGameplayUIUpdate> pCastEventData =
		std::static_pointer_cast<EventDataGameplayUIUpdate>(pEventData);
    if (!pCastEventData->GetUiString().empty())
        mGameplayText = pCastEventData->GetUiString();
    else
		mGameplayText.clear();
}

void QuakeHumanView::FireWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFireWeapon> pCastEventData =
		std::static_pointer_cast<EventDataFireWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim ||
					mesh->GetCurrentFrame() == mesh->GetAnimation(torsoAnim).mEndFrame)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeHumanView::ChangeWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeWeapon> pCastEventData =
		std::static_pointer_cast<EventDataChangeWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);

		int weaponIdx = 0;
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetParent() && mesh->GetParent()->GetName() == "tag_weapon")
			{
				weaponIdx++;
				if (pPlayerActor->GetState().weapon == weaponIdx)
					mesh->SetRenderMesh(true);
				else
					mesh->SetRenderMesh(false);
			}

			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeHumanView::DeadActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataDeadActor> pCastEventData =
		std::static_pointer_cast<EventDataDeadActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);

		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetParent() && mesh->GetParent()->GetName() == "tag_weapon")
				mesh->SetRenderMesh(false);

			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				mesh->SetCurrentAnimation(legsAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				mesh->SetCurrentAnimation(torsoAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
			}
		}
	}
}

void QuakeHumanView::SpawnActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSpawnActor> pCastEventData =
		std::static_pointer_cast<EventDataSpawnActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);

		int weaponIdx = 0;
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetParent() && mesh->GetParent()->GetName() == "tag_weapon")
			{
				weaponIdx++;
				if (pPlayerActor->GetState().weapon == weaponIdx)
					mesh->SetRenderMesh(true);
				else
					mesh->SetRenderMesh(false);
			}

			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				mesh->SetCurrentAnimation(legsAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				mesh->SetCurrentAnimation(torsoAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
			}
		}
	}
}

void QuakeHumanView::JumpActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataJumpActor> pCastEventData =
		std::static_pointer_cast<EventDataJumpActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				mesh->SetCurrentAnimation(legsAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				mesh->SetCurrentAnimation(torsoAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
			}
		}
	}
}

void QuakeHumanView::MoveActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataMoveActor> pCastEventData =
		std::static_pointer_cast<EventDataMoveActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
	if (pPlayerActor->GetState().weaponState != WEAPON_READY)
		return;

	std::shared_ptr<PhysicComponent> pPhysicComponent(
		pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
	if (pPhysicComponent)
		if (!pPhysicComponent->OnGround())
			return;

	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode = 
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeHumanView::FallActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFallActor> pCastEventData =
		std::static_pointer_cast<EventDataFallActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(actorId).lock()));
	if (pPlayerActor->GetState().weaponState != WEAPON_READY)
		return;

	std::shared_ptr<PhysicComponent> pPhysicComponent(
		pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
	if (pPhysicComponent)
		if (!pPhysicComponent->OnGround())
			return;

	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeHumanView::RotateActorDelegate(BaseEventDataPtr pEventData)
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
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), pCastEventData->GetYaw() * (float)GE_C_DEG_TO_RAD));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), pCastEventData->GetPitch() * (float)GE_C_DEG_TO_RAD));

			pNode->GetRelativeTransform().SetRotation(yawRotation * pitchRotation);
		}
	}
}

void QuakeHumanView::PlayDuelCombatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPlayDuelCombat> pCastEventData =
		std::static_pointer_cast<EventDataPlayDuelCombat>(pEventData);

	std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
	std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
	aiView->SetBehavior(BT_PATROL);
}

void QuakeHumanView::RegisterAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::InitChatDelegate),
		EventDataInitChat::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::UpdateChatDelegate),
		EventDataUpdateChat::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::ShowFormDelegate),
		EventDataShowForm::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::HandlePlaySoundAtDelegate),
		EventDataPlaySoundAt::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::HandlePlaySoundDelegate),
		EventDataPlaySoundType::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::HandleStopSoundDelegate),
		EventDataStopSound::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::HandleFadeSoundDelegate),
		EventDataFadeSound::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::ChangeVolumeDelegate),
		EventDataChangeVolume::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::ChangeMenuDelegate),
		EventDataChangeMenu::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::HandleMediaDelegate),
		EventDataHandleMedia::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::FireWeaponDelegate),
		EventDataFireWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::ChangeWeaponDelegate),
		EventDataChangeWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::DeadActorDelegate),
		EventDataDeadActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::SpawnActorDelegate),
		EventDataSpawnActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::MoveActorDelegate),
		EventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::FallActorDelegate),
		EventDataFallActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::RotateActorDelegate),
		EventDataRotateActor::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeHumanView::PlayDuelCombatDelegate),
		EventDataPlayDuelCombat::skEventType);
}

void QuakeHumanView::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::InitChatDelegate),
		EventDataInitChat::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::UpdateChatDelegate),
		EventDataUpdateChat::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::ShowFormDelegate),
		EventDataShowForm::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::HandlePlaySoundAtDelegate),
		EventDataPlaySoundAt::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::HandlePlaySoundDelegate),
		EventDataPlaySoundType::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::HandleStopSoundDelegate),
		EventDataStopSound::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::HandleFadeSoundDelegate),
		EventDataFadeSound::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::ChangeVolumeDelegate),
		EventDataChangeVolume::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::ChangeMenuDelegate),
		EventDataChangeMenu::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::HandleMediaDelegate),
		EventDataHandleMedia::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::FireWeaponDelegate),
		EventDataFireWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::ChangeWeaponDelegate),
		EventDataChangeWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::DeadActorDelegate),
		EventDataDeadActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::SpawnActorDelegate),
		EventDataSpawnActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::MoveActorDelegate),
		EventDataMoveActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::FallActorDelegate),
		EventDataFallActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::RotateActorDelegate),
		EventDataRotateActor::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeHumanView::PlayDuelCombatDelegate),
		EventDataPlayDuelCombat::skEventType);
}

void QuakeHumanView::OpenConsole(float scale, const wchar_t* line)
{
	LogAssert(scale > 0.0f && scale <= 1.0f, "invalid scale");

	if (mGameUI->mUIChatConsole->IsOpenInhibited())
		return;

	mGameUI->mUIChatConsole->OpenConsole(scale);
	if (line)
	{
		mGameUI->mUIChatConsole->SetCloseOnEnter(true);
		mGameUI->mUIChatConsole->ReplaceAndAddToHistory(line);
	}
}

void QuakeHumanView::ToggleFreeMove()
{
	bool freeMove = !Settings::Get()->GetBool("free_move");
	Settings::Get()->Set("free_move", freeMove ? "true" : "false");

	if (freeMove)
	{
		const bool hasFlyPrivs = false;// mEnvironment->CheckPrivilege("fly");
		if (hasFlyPrivs)
			mGameUI->ShowTranslatedStatusText("Fly mode enabled");
		else
			mGameUI->ShowTranslatedStatusText("Fly mode enabled (note: no 'fly' privilege)");
	}
	else
	{
		mGameUI->ShowTranslatedStatusText("Fly mode disabled");
	}
}

void QuakeHumanView::ToggleFreeMoveAlt()
{
	if (mGameSettings.doubletapJump)
		ToggleFreeMove();
}

void QuakeHumanView::TogglePitchMove()
{
	bool pitchMove = !Settings::Get()->GetBool("pitch_move");
	Settings::Get()->Set("pitch_move", pitchMove ? "true" : "false");

	if (pitchMove)
		mGameUI->ShowTranslatedStatusText("Pitch move mode enabled");
	else
		mGameUI->ShowTranslatedStatusText("Pitch move mode disabled");
}

void QuakeHumanView::ToggleFast()
{
	bool fastMove = !Settings::Get()->GetBool("fast_move");
	Settings::Get()->Set("fast_move", fastMove ? "true" : "false");

	if (fastMove)
	{
		const bool hasFastPrivs = false; //mEnvironment->CheckPrivilege("fast");
		if (hasFastPrivs)
			mGameUI->ShowTranslatedStatusText("Fast mode enabled");
		else
			mGameUI->ShowTranslatedStatusText("Fast mode enabled (note: no 'fast' privilege)");
	}
	else
		mGameUI->ShowTranslatedStatusText("Fast mode disabled");
}

void QuakeHumanView::ToggleNoClip()
{
	bool noClip = !Settings::Get()->GetBool("noclip");
	Settings::Get()->Set("noclip", noClip ? "true" : "false");

	if (noClip)
	{
		const bool hasNoClipPrivs = false; //mEnvironment->CheckPrivilege("noclip");
		if (hasNoClipPrivs)
			mGameUI->ShowTranslatedStatusText("Noclip mode enabled");
		else
			mGameUI->ShowTranslatedStatusText("Noclip mode enabled (note: no 'noClip' privilege)");
	}
	else
		mGameUI->ShowTranslatedStatusText("Noclip mode disabled");
}

void QuakeHumanView::ToggleCinematic()
{
	bool cinematic = !Settings::Get()->GetBool("cinematic");
	Settings::Get()->Set("cinematic", cinematic ? "true" : "false");

	if (cinematic)
		mGameUI->ShowTranslatedStatusText("Cinematic mode enabled");
	else
		mGameUI->ShowTranslatedStatusText("Cinematic mode disabled");
}

// Autoforward by toggling continuous forward.
void QuakeHumanView::ToggleAutoforward()
{
	bool autorunEnabled = !Settings::Get()->GetBool("continuous_forward");
	Settings::Get()->Set("continuous_forward", autorunEnabled ? "true" : "false");

	if (autorunEnabled)
		mGameUI->ShowTranslatedStatusText("Automatic forward enabled");
	else
		mGameUI->ShowTranslatedStatusText("Automatic forward disabled");
}

void QuakeHumanView::ToggleFog()
{
	bool fogEnabled = Settings::Get()->GetBool("enable_fog");
	Settings::Get()->SetBool("enable_fog", !fogEnabled);
	if (fogEnabled)
		mGameUI->ShowTranslatedStatusText("Fog disabled");
	else
		mGameUI->ShowTranslatedStatusText("Fog enabled");
}

void QuakeHumanView::ToggleDebug()
{
	const bool hasDebugPrivs = true; //mEnvironment->CheckPrivilege("debug");

	// Initial / 4x toggle: Chat only
	// 1x toggle: Debug text with chat
	// 2x toggle: Debug text with profiler graph
	// 3x toggle: Debug text and wireframe
	if (!mGameUI->mFlags.showDebug)
	{
		mGameUI->mFlags.showDebug = true;
		mGameUI->mFlags.showProfilerGraph = false;
		mGameUI->ShowTranslatedStatusText("Debug info shown");
	}
	else if (!mGameUI->mFlags.showProfilerGraph)
	{
		mGameUI->mFlags.showProfilerGraph = true;
		mGameUI->ShowTranslatedStatusText("Profiler graph shown");
	}
	else if (hasDebugPrivs)
	{
		mGameUI->mFlags.showProfilerGraph = false;
		mGameUI->ShowTranslatedStatusText("Wireframe shown");
	}
	else
	{
		mGameUI->mFlags.showDebug = false;
		mGameUI->mFlags.showProfilerGraph = false;
		if (hasDebugPrivs)
			mGameUI->ShowTranslatedStatusText("Debug info, profiler graph, and wireframe hidden");
		else
			mGameUI->ShowTranslatedStatusText("Debug info and profiler graph hidden");
	}
}

void QuakeHumanView::IncreaseViewRange()
{
	int16_t range = Settings::Get()->GetInt16("viewing_range");
	int16_t rangeNew = range + 10;

	wchar_t buf[255];
	const wchar_t* str;
	if (rangeNew > 4000)
	{
		rangeNew = 4000;
		str = L"Viewing range is at maximum: %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mGameUI->ShowStatusText(buf);
	}
	else
	{
		str = L"Viewing range changed to %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mGameUI->ShowStatusText(buf);
	}
	Settings::Get()->Set("viewing_range", std::to_string(rangeNew));
}

void QuakeHumanView::DecreaseViewRange()
{
	int16_t range = Settings::Get()->GetInt16("viewing_range");
	int16_t rangeNew = range - 10;

	wchar_t buf[255];
	const wchar_t* str;
	if (rangeNew < 20)
	{
		rangeNew = 20;
		str = L"Viewing range is at minimum: %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mGameUI->ShowStatusText(buf);
	}
	else
	{
		str = L"Viewing range changed to %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mGameUI->ShowStatusText(buf);
	}
	Settings::Get()->Set("viewingRange", std::to_string(rangeNew));
}

void QuakeHumanView::ToggleFullViewRange()
{
	mGameUI->ShowTranslatedStatusText("Disabled unlimited viewing range");
}

void QuakeHumanView::CheckZoomEnabled()
{

}


/****************************************************************************/
/****************************************************************************
 Shutdown / cleanup
 ****************************************************************************/
 /****************************************************************************/

void QuakeHumanView::ExtendedResourceCleanup()
{
	// Extended resource accounting
	LogInformation("Game resources after cleanup:");
}

void QuakeHumanView::Shutdown()
{
	std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetForm();
	if (formUI)
		formUI->QuitForm();

	Renderer::Get()->SetClearColor(SColor(255, 0, 0, 0));
	Renderer::Get()->ClearBuffers();

	mGameUI->ShowOverlayMessage(L"Shutting down...", 0, false);

	Renderer::Get()->DisplayColorBuffer(0);

	/* cleanup menus */
	if (mGameUI->GetForm())
		mGameUI->GetForm()->SetVisible(false);
	mGameUI->DeleteForm();

	Sleep(100);

	ExtendedResourceCleanup();
}


void QuakeHumanView::ShowPauseMenu()
{
	static const std::string controlTextTemplate = "Controls:\n"
		"- %s: move forwards\n"
		"- %s: move backwards\n"
		"- %s: move left\n"
		"- %s: move right\n"
		"- %s: jump/climb up\n"
		"- %s: dig/punch\n"
		"- %s: place/use\n"
		"- %s: sneak/climb down\n"
		"- %s: drop item\n"
		"- %s: inventory\n"
		"- Mouse: turn/look\n"
		"- Mouse wheel: select item\n"
		"- %s: chat\n";

	char controlTextBuf[600];

	snprintf(controlTextBuf, sizeof(controlTextBuf), controlTextTemplate.c_str(),
		GetKeySetting("keymap_forward").Name(),
		GetKeySetting("keymap_backward").Name(),
		GetKeySetting("keymap_left").Name(),
		GetKeySetting("keymap_right").Name(),
		GetKeySetting("keymap_jump").Name(),
		GetKeySetting("keymap_dig").Name(),
		GetKeySetting("keymap_place").Name(),
		GetKeySetting("keymap_sneak").Name(),
		GetKeySetting("keymap_drop").Name(),
		GetKeySetting("keymap_inventory").Name(),
		GetKeySetting("keymap_chat").Name()
	);

	std::string controlText = controlTextBuf;
	StringFormEscape(controlText);

	float yPos = 0.7f;
	std::ostringstream os;

	os << "form_version[1]" << SIZE_TAG
		<< "button_exit[4," << (yPos++) << ";3,0.5;btn_continue;"
		<< "Continue" << "]" << "field[4.95,0;5,1.5;;" << "Game paused" << ";]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_play_duel;"
		<< "Play Duel" << "]";

	if (Settings::Get()->GetBool("enable_sound"))
	{
		os << "button_exit[4," << (yPos++) << ";3,0.5;btn_sound;"
			<< "Sound Volume" << "]";
	}
	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_key_config;"
		<< "Change Keys" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_exit_menu;"
		<< "Exit to Menu" << "]"
		<< "textarea[7.5,0.25;3.9,6.25;;" << controlText << ";]"
		<< "textarea[0.4,0.25;3.9,6.25;;" << "Quake \n"
		<< "\n" << "Game info:" << "\n";
	os << ";]";

	/* Create menu */
	/* Note: FormSource and LocalFormHandler  *
	 * are deleted by FormMenu                     */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(os.str());
	std::shared_ptr<LocalFormHandler> textDst = std::make_shared<LocalFormHandler>("PAUSE_MENU");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetForm();
	if (formUI)
	{
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		formUI.reset(new UIForm(mGameUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mGameUI->GetRootUIElement());
		formUI->OnInit();
	}

	formUI->SetFocus("btn_continue");
}

void  QuakeHumanView::UpdateControllers(unsigned int timeMs, unsigned long deltaMs)
{
	if (System::Get()->IsWindowActive() &&
		System::Get()->IsWindowFocused() &&
		!mGameUI->IsMenuActive())
	{
		// Mac OSX gets upset if this is set every frame
		if (System::Get()->GetCursorControl()->IsVisible())
			System::Get()->GetCursorControl()->SetVisible(false);

		if (mGameCameraController)
			mGameCameraController->OnUpdate(timeMs, deltaMs);

		if (mGamePlayerController)
			mGamePlayerController->OnUpdate(timeMs, deltaMs);
	}
	else
	{
		// Mac OSX gets upset if this is set every frame
		if (!System::Get()->GetCursorControl()->IsVisible())
			System::Get()->GetCursorControl()->SetVisible(true);
	}
}

void QuakeHumanView::UpdateSound(float dTime)
{
	// Update sound listener
	mSoundMgr->UpdateListener(
		mCamera->GetRelativeTransform().GetTranslation(),
		Vector3<float>::Zero(), HProject(mCamera->Get()->GetDVector()),
		HProject(mCamera->Get()->GetUVector()));

	bool muteSound = Settings::Get()->GetBool("mute_sound");
	if (!muteSound)
	{
		// Check if volume is in the proper range, else fix it.
		float oldVolume = Settings::Get()->GetFloat("sound_volume");
		float newVolume = std::clamp(oldVolume, 0.0f, 1.0f);
		mSoundMgr->SetListenerGain(newVolume);

		if (oldVolume != newVolume)
			Settings::Get()->SetFloat("sound_volume", newVolume);
	}
	else
	{
		mSoundMgr->SetListenerGain(0.0f);
	}
	/*
	VisualPlayer* player = mEnvironment->GetPlayer();

	// Tell the sound maker whether to make footstep sounds
	mSoundMaker->mMakesFootstepSound = player->mMakesFootstepSound;

	//	Update sound maker
	if (player->mMakesFootstepSound)
		mSoundMaker->Step(dTime);

	std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
	MapNode node = map->GetNode(player->GetFootstepNodePosition());
	mSoundMaker->mPlayerStepSound = mEnvironment->GetNodeManager()->Get(node).soundFootstep;
	*/
}

void QuakeHumanView::ProcessPlayerInteraction(float dTime, bool showHud, bool showDebug)
{
	ClearWasKeyPressed();
	ClearWasKeyReleased();
}

bool QuakeHumanView::InitSound()
{
	if (Settings::Get()->GetBool("enable_sound"))
	{
		//create soundmanager
		LogInformation("Attempting to use OpenAL audio");
		mSoundMgr = std::make_shared<OpenALSoundManager>(
			static_cast<OpenALSoundSystem*>(SoundSystem::Get()), &mSoundFetcher);
		if (!mSoundMgr)
		{
			LogError("Failed to Initialize OpenAL audio");
			return false;
		}
	}
	else LogInformation("Sound disabled.");

	mSoundMaker = std::make_shared<SoundMaker>(mSoundMgr.get());
	if (!mSoundMaker)
	{
		LogError("Failed to Initialize OpenAL audio");
		return false;
	}

	//mSoundMaker->RegisterReceiver(mEventMgr);

	return true;
}