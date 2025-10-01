//========================================================================
// MinecraftView.cpp : Game View Class
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

#include "Minecraft.h"
#include "MinecraftApp.h"
#include "MinecraftView.h"
#include "MinecraftNetwork.h"

#include "Graphics/Actors/VisualPlayer.h"
#include "Graphics/Actors/ContentVisualActiveObject.h"

#include "Graphics/Drawing/Plain.h"
#include "Graphics/Drawing/Anaglyph.h"
#include "Graphics/Drawing/Interlaced.h"
#include "Graphics/Drawing/Sidebyside.h"

#include "Graphics/Particles.h"
#include "Graphics/AnimatedObjectMesh.h"

#include "Graphics/Map/VisualMap.h"

#include "Games/Map/Emerge.h"
#include "Games/Map/MapGenerator.h"
#include "Games/Map/MapSector.h"
#include "Games/Map/MapBlock.h"
#include "Games/Map/Map.h"

#include "Games/Forms/Menu/Local.h"
#include "Games/Forms/Menu/Online.h"
#include "Games/Forms/Menu/Content.h"
#include "Games/Forms/Menu/DlgDeleteContent.h"
#include "Games/Forms/Menu/DlgContentStore.h"
#include "Games/Forms/Menu/MenuSettings.h"
#include "Games/Forms/Menu/About.h"

#include "Utils/Util.h"
#include "Utils/FacePositionCache.h"

#include "Data/Database.h"

#include "Physics/Raycast.h"

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

#include "Physic/PhysicEventListener.h"

#include "Game/View/HumanView.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/RenderComponent.h"

#include <iomanip>


const Vector3<short> Face6D[6] =
{
    // +right, +top, +back
    Vector3<short>{0, 0, 1}, // back
    Vector3<short>{0, 1, 0}, // top
    Vector3<short>{1, 0, 0}, // right
    Vector3<short>{0, 0,-1}, // front
    Vector3<short>{0,-1, 0}, // bottom
    Vector3<short>{-1, 0, 0} // left
};

/****************************************************************************
 Misc
 ****************************************************************************/

 /* On some computers framerate doesn't seem to be automatically limited
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


/******************************************************************************/
/** SoundFetcher                                                          */
/******************************************************************************/
void MenuSoundFetcher::FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
{
    if (mFetched.count(name))
        return;
    mFetched.insert(name);

    std::string soundBase = "Art/Minecraft/Audio";
    dstPaths.insert(soundBase + "/" + name + ".ogg");
    for (int i = 0; i < 10; i++)
        dstPaths.insert(soundBase + "/" + name + "." + std::to_string(i) + ".ogg");
    dstPaths.insert(soundBase + "/" + name + ".ogg");
}


void GameSoundFetcher::PathsInsert(std::set<std::string>& dstPaths, 
    const std::string& base, const std::string& name)
{
    dstPaths.insert(base + name + ".ogg");
    dstPaths.insert(base + name + ".0.ogg");
    dstPaths.insert(base + name + ".1.ogg");
    dstPaths.insert(base + name + ".2.ogg");
    dstPaths.insert(base + name + ".3.ogg");
    dstPaths.insert(base + name + ".4.ogg");
    dstPaths.insert(base + name + ".5.ogg");
    dstPaths.insert(base + name + ".6.ogg");
    dstPaths.insert(base + name + ".7.ogg");
    dstPaths.insert(base + name + ".8.ogg");
    dstPaths.insert(base + name + ".9.ogg");
}

void GameSoundFetcher::FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
{
    if (mFetched.count(name))
        return;

    mFetched.insert(name);

    Subgame gameSpec = FindSubgame(Settings::Get()->Get("selected_game"));
    std::string soundBase = gameSpec.mRelativePath + "/sounds/";
    PathsInsert(dstPaths, soundBase, name);
}


//========================================================================
//
// MinecrafView implementation
//
//
//========================================================================


MinecraftMainMenuUI::MinecraftMainMenuUI()
{

}

MinecraftMainMenuUI::~MinecraftMainMenuUI()
{

}

void MinecraftMainMenuUI::ResetMenuUI()
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

void MinecraftMainMenuUI::UpdateMenuUI(std::wstring gameId)
{
    ClearMenuUI(L"overlay");
    ClearMenuUI(L"background");
    ClearMenuUI(L"header");
    ClearMenuUI(L"footer");

    if (!SetMenuUI(L"overlay", gameId))
        if (!SetMenuUI(L"background", gameId))
            SetGenericMenuUI(L"background");

    SetMenuUI(L"header", gameId);
    SetMenuUI(L"footer", gameId);
}

void MinecraftMainMenuUI::ClearMenuUI(std::wstring id)
{
    SetTexture(id, L"", false, 16);
}

bool MinecraftMainMenuUI::SetGenericMenuUI(std::wstring id)
{
    //default texture dir
    std::wstring path = L"Art/Minecraft/textures/menu_" + id + L".png";
    if (SetTexture(id, path, false, 16)) 
        return true;

    return false;
}

bool MinecraftMainMenuUI::SetMenuUI(std::wstring id, std::wstring gameId)
{
    if (gameId.empty())
        return false;

    //Find out how many randomized textures the game provides
    std::wstring previousCWD = FileSystem::Get()->GetWorkingDirectory();
    std::vector<std::wstring> menuFiles;
    std::wstring path = 
        previousCWD + L"/../../Assets/Art/Minecraft/games/" + gameId + L"/menu";
    FileSystem::Get()->GetFileList(menuFiles, path);
    FileSystem::Get()->ChangeWorkingDirectoryTo(previousCWD);

    unsigned int n = 0;
    std::wstring fileName;
    for (unsigned int i = 1; i <= menuFiles.size(); i++)
    {
        fileName = path + L"/" + id + L"." + std::to_wstring(i) + L".png";
        if (!FileSystem::Get()->ExistFile(fileName))
        {
            n = i;
            break;
        }
    }

    //Select random texture, 0 means standard texture
    n = n > 0 ? Randomizer::Rand() % n : 0;
    if (n == 0)
        fileName = id + L".png";
    else
        fileName = id + L"."  + std::to_wstring(n) + L".png";
    
    path = L"Art/Minecraft/games/" + gameId + L"/menu/" + fileName;
    if (SetTexture(id, path, false, 16))
        return true;

    return false;
}

bool MinecraftMainMenuUI::OnInit()
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

bool MinecraftMainMenuUI::OnRestore()
{
    return true;
}

bool MinecraftMainMenuUI::OnRender(double time, float elapsedTime)
{
    return BaseUI::OnRender(time, elapsedTime);
};

bool MinecraftMainMenuUI::OnMsgProc(const Event& evt)
{
    return BaseUI::OnMsgProc(evt);
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool MinecraftMainMenuUI::OnEvent(const Event& evt)
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
                    mMenu = std::make_shared<Local>(mTitleText->GetText());
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


MinecraftMainMenuView::MinecraftMainMenuView() : HumanView()
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

    mMinecraftMainMenuUI.reset(new MinecraftMainMenuUI());
    mMinecraftMainMenuUI->OnInit();
    PushElement(mMinecraftMainMenuUI);

    mCamera->GetRelativeTransform().SetTranslation(Vector4<float>::Zero());
    UpdateCameraRotation(Vector4<float>::Zero(), Vector4<float>{0, 60, 100, 0});

    float upFov, aspectRatio, dMin, dMax;
    mCamera->Get()->GetFrustum(upFov, aspectRatio, dMin, dMax);
    mCamera->Get()->SetFrustum(upFov, aspectRatio, dMin, 10000);

    mMenuCloud = std::static_pointer_cast<CloudSystemNode>(mScene->AddCloudSystemNode(0));
    mMenuCloud->SetHeight(100.0f);
    mMenuCloud->SetMaterialType(MT_SOLID);
    mMenuCloud->SetEffect(
        Settings::Get()->GetUInt("cloud_radius"), 
        Settings::Get()->GetBool("enable_3d_clouds"));
    mMenuCloud->Update(Vector3<float>::Zero(), SColor(255, 240, 240, 255));

    Matrix4x4<float> rotation = Rotation<4, float>(
        AxisAngle<4, float>(Vector4<float>::Unit(0), 45.f * (float)GE_C_DEG_TO_RAD));
    mMenuCloud->GetRelativeTransform().SetRotation(rotation);

    PushElement(mScene);

    RegisterAllDelegates();
}


MinecraftMainMenuView::~MinecraftMainMenuView()
{
    RemoveAllDelegates();
}


void MinecraftMainMenuView::RenderText()
{
	HumanView::RenderText();
}


void MinecraftMainMenuView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate(timeMs, deltaMs);

    mMenuCloud->Update(deltaMs * 3.f);
}


void MinecraftMainMenuView::OnRender(double timeMs, float deltaMs)
{
    HumanView::OnRender(timeMs, deltaMs);
}


bool MinecraftMainMenuView::OnMsgProc( const Event& evt )
{
	if (mMinecraftMainMenuUI->IsVisible() )
	{
		if (HumanView::OnMsgProc(evt))
			return 1;
	}
	return 0;
}

void MinecraftMainMenuView::UpdateCameraRotation(
    const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const
{
    EulerAngles<float> rotation;
    rotation.mAxis[1] = 1;
    rotation.mAxis[2] = 2;

    Quaternion<float> q(targetPos - cameraPos);
    rotation = Rotation<4, float>(q)(rotation.mAxis[0], rotation.mAxis[1], rotation.mAxis[2]);
    mCamera->GetRelativeTransform().SetRotation(rotation);
}

void MinecraftMainMenuView::OpenContentStoreDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataOpenContentStore> pCastEventData =
        std::static_pointer_cast<EventDataOpenContentStore>(pEventData);

    std::shared_ptr<DlgContentStore> dlgContentStore = std::make_shared<DlgContentStore>();
    mMinecraftMainMenuUI->SetMenu(dlgContentStore);
    mMinecraftMainMenuUI->GetFormMenu()->SetForm(dlgContentStore->GetForm());
}

void MinecraftMainMenuView::DeleteContentStoreDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataDeleteContentStore> pCastEventData =
        std::static_pointer_cast<EventDataDeleteContentStore>(pEventData);

    std::shared_ptr<DlgDeleteContent> dlgDeleteContent =
        std::make_shared<DlgDeleteContent>(ToWideString(pCastEventData->GetLevel()));
    mMinecraftMainMenuUI->SetMenu(dlgDeleteContent);
    mMinecraftMainMenuUI->GetFormMenu()->SetForm(dlgDeleteContent->GetForm());
}

void MinecraftMainMenuView::OpenGameSelectionDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataOpenGameSelection> pCastEventData =
        std::static_pointer_cast<EventDataOpenGameSelection>(pEventData);

    std::shared_ptr<Local> local = std::make_shared<Local>();
    mMinecraftMainMenuUI->SetMenu(local);
    mMinecraftMainMenuUI->GetFormMenu()->SetForm(local->GetForm());
}

void MinecraftMainMenuView::ChangeGameSelectionDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataChangeGameSelection> pCastEventData =
        std::static_pointer_cast<EventDataChangeGameSelection>(pEventData);

    mMinecraftMainMenuUI->UpdateMenuUI(ToWideString(pCastEventData->GetGameId()));
    mMinecraftMainMenuUI->SetTitleText(ToWideString(pCastEventData->GetGameName()));
    mMinecraftMainMenuUI->GetFormMenu()->SetForm(mMinecraftMainMenuUI->GetMenu()->GetForm());
}

void MinecraftMainMenuView::RegisterAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftMainMenuView::OpenContentStoreDelegate),
        EventDataOpenContentStore::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftMainMenuView::DeleteContentStoreDelegate),
        EventDataDeleteContentStore::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftMainMenuView::OpenGameSelectionDelegate),
        EventDataOpenGameSelection::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftMainMenuView::ChangeGameSelectionDelegate),
        EventDataChangeGameSelection::skEventType);
}

void MinecraftMainMenuView::RemoveAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftMainMenuView::OpenContentStoreDelegate),
        EventDataOpenContentStore::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftMainMenuView::DeleteContentStoreDelegate),
        EventDataDeleteContentStore::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftMainMenuView::OpenGameSelectionDelegate),
        EventDataOpenGameSelection::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftMainMenuView::ChangeGameSelectionDelegate),
        EventDataChangeGameSelection::skEventType);
}


//========================================================================
//
// GameUI implementation
//
//
//========================================================================


inline static const char* YawToDirectionString(int yaw)
{
    static const char *direction[4] =
    { "North +Z", "West -X", "South -Z", "East +X" };

    yaw = (int)WrapDegrees360((float)yaw);
    yaw = (yaw + 45) % 360 / 90;

    return direction[yaw];
}

MinecraftUI::MinecraftUI()
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


MinecraftUI::~MinecraftUI()
{

}

bool MinecraftUI::OnInit()
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
    mUIText = AddStaticText(L"Minetest", RectangleShape<2, int>(), false, false);

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
    rect.mCenter += Vector2<int>{100, chatFontHeight * (recentChatMessages + 3)};
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

    mFlags = MinecraftUI::Flags();
    mFlags.showDebug = Settings::Get()->GetBool("show_debug");

    return true;
}

void MinecraftUI::Update(const RunStats& stats, 
    std::shared_ptr<MapDrawControl> drawControl, VisualEnvironment* visualEnv,
    const PlayerCameraOrientation& cam, const PointedThing& pointedOld,
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
            << "Minetest " << "version 1.0"
            << " | FPS: " << fps
            << std::setprecision(0)
            << " | drawTime: " << drawTimeAvg << "ms"
            << std::setprecision(1)
            << " | dtime jitter: "
            << (stats.dTimeJitter.maxFraction * 100.0) << "%"
            << std::setprecision(1)
            << " | view range: "
            << (drawControl->rangeAll ? "All" : std::to_string(drawControl->wantedRange));
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
        Vector3<float> playerPosition = visualEnv->GetPlayer()->GetPosition();

        std::ostringstream os(std::ios_base::binary);
        os << std::setprecision(1) << std::fixed
            << "pos: (" << (playerPosition[0] / BS)
            << ", " << (playerPosition[1] / BS)
            << ", " << (playerPosition[2] / BS)
            << ") | yaw: " << (WrapDegrees360(cam.cameraYaw)) << "° "
            << YawToDirectionString((int)cam.cameraYaw)
            << " | pitch: " << (-WrapDegrees180(cam.cameraPitch)) << "°";
        //<< " | seed: " << ((uint64_t)client->GetMapSeed());

        if (pointedOld.type == POINTEDTHING_NODE)
        {
            std::shared_ptr<VisualMap> map = visualEnv->GetVisualMap();
            const NodeManager* nodeMgr = visualEnv->GetNodeManager();
            MapNode node = map->GetNode(pointedOld.nodeUndersurface);

            if (node.GetContent() != CONTENT_IGNORE && nodeMgr->Get(node).name != "unknown")
            {
                os << ", pointed: " << nodeMgr->Get(node).name
                    << ", param2: " << (uint64_t)node.GetParam2();
            }
        }

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
    mUITextInfo->SetVisible(mFlags.showHud);// && g_menumgr.menuCount() == 0);

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


void MinecraftUI::ShowMinimap(bool show)
{
    mFlags.showMinimap = show;
}

void MinecraftUI::ShowTranslatedStatusText(const char* str)
{
    ShowStatusText(ToWideString(str));
}

void MinecraftUI::SetChatText(const EnrichedString& chatText, unsigned int recentChatCount)
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

void MinecraftUI::UpdateProfiler()
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

void MinecraftUI::ToggleChat()
{
    mFlags.showChat = !mFlags.showChat;
    if (mFlags.showChat)
        ShowTranslatedStatusText("Chat shown");
    else
        ShowTranslatedStatusText("Chat hidden");
}

void MinecraftUI::ToggleHud()
{
    mFlags.showHud = !mFlags.showHud;
    if (mFlags.showHud)
        ShowTranslatedStatusText("HUD shown");
    else
        ShowTranslatedStatusText("HUD hidden");
}

void MinecraftUI::ToggleProfiler()
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
void MinecraftUI::ShowOverlayMessage(const std::wstring& text,
    std::shared_ptr<BaseTextureSource> texSource, float dTime, int percent, bool drawClouds)
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
        std::shared_ptr<Texture2> progressImg = texSource->GetTexture("progress_bar.png");
        std::shared_ptr<Texture2> progressImgBG = texSource->GetTexture("progress_bar_bg.png");

        if (progressImg && progressImgBG) 
        {
            Renderer::Get()->SetBlendState(mBlendState);

            int imgW = std::clamp((int)progressImgBG->GetDimension(0), 200, 600);
            int imgH = std::clamp((int)progressImgBG->GetDimension(1), 24, 72);

            Vector2<int> imgPos{ ((int)screenSize[0] - imgW) / 2, ((int)screenSize[1] - imgH) / 2 };

            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
            effect->SetTexture(progressImgBG);

            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{imgW, imgH};
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

void MinecraftUI::DeleteFormUI()
{
    // delete all children
    mForm.reset();
    mFormName.clear();
}

bool MinecraftUI::IsMenuActive()
{
    return mIsMenuActive;
}

void MinecraftUI::SetMenuActive(bool active)
{
    mIsMenuActive = active;
}

bool MinecraftUI::OnRestore()
{
    return BaseUI::OnRestore();
}

bool MinecraftUI::OnRender(double time, float elapsedTime)
{
    TimeTaker ttDraw("Draw scene");

    RunStats stats = { 0 };
    ProfilerGraph graph = ProfilerGraph(this);

    SetLightTable(Settings::Get()->GetFloat("display_gamma"));

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
    mHud->ResizeHotbar();

    bool drawWieldTool = (mFlags.showHud &&
        (mHud->mPlayer->mHudFlags & HUD_FLAG_WIELDITEM_VISIBLE) &&
        (mHud->mPlayerCamera->GetCameraMode() == CAMERA_MODE_FIRST));
    bool drawCrosshair = (
        (mHud->mPlayer->mHudFlags & HUD_FLAG_CROSSHAIR_VISIBLE) &&
        (mHud->mPlayerCamera->GetCameraMode() != CAMERA_MODE_THIRD_FRONT));

    SColor skyColor = Renderer::Get()->GetClearColor().ToSColor();
    mDrawingCore->Draw(skyColor, mFlags.showHud,
        mFlags.showMinimap, drawWieldTool, drawCrosshair);

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

    /*
        End scene
    */
    if (++mResetHWBufferCounter > 500)
    {
        /*
          Periodically remove all mesh HW buffers.

          Work around for a quirk in game engine where a HW buffer is only
          released after 20000 iterations (triggered from endScene()).

          Without this, all loaded but unused meshes will retain their HW
          buffers for at least 5 minutes, at which point looking up the HW buffers
          becomes a bottleneck and the framerate drops (as much as 30%).

          Tests showed that numbers between 50 and 1000 are good, so picked 500.
          There are no other public Game Engine APIs that allow interacting with the
          HW buffers without tracking the status of every individual mesh.

          The HW buffers for _visible_ meshes will be reInitialized in the next frame.
        */
        LogInformation("OnRender(): Removing all HW buffers.");
        //driver->removeAllHardwareBuffers();
        mResetHWBufferCounter = 0;

    }
    //driver->endScene();

    Profiling->GraphAdd("Render frame [ms]", (float)ttDraw.Stop(true));

    /* Log times and stuff for visualization */
    Profiler::GraphValues values;
    Profiling->GraphGet(values);
    graph.Put(values);

    return true;
};


bool MinecraftUI::OnMsgProc(const Event& evt)
{
    return BaseUI::OnMsgProc(evt);
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool MinecraftUI::OnEvent(const Event& evt)
{
    if (evt.mEventType == ET_UI_EVENT)
    {
        int id = evt.mUIEvent.mCaller->GetID();
    }

    return false;
}

//========================================================================
//
// MinecraftHumanView Implementation
//
//========================================================================

/*
    Draws a screen with a single text on it.
    Text will be removed when the screen is drawn the next time.
    Additionally, a progressbar can be drawn when percent is set between 0 and 100.
*/
void DrawLoadScreen(const std::wstring& text, std::shared_ptr<BaseUI> ui,
    std::shared_ptr<CloudSystemNode> cloud, std::shared_ptr<Visual> visual, 
    std::shared_ptr<BlendState> blendState, BaseTextureSource* textureSrc, 
    Scene* pScene, float dTime, int percent)
{
    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

    Vector2<int> texSize = ui->GetSkin()->GetFont()->GetDimension(text);
    Vector2<int> center{ (int)screenSize[0] / 2, (int)screenSize[1] / 2 };
    RectangleShape<2, int> textRect;
    textRect.mExtent = texSize;
    textRect.mCenter = center;

    std::shared_ptr<BaseUIStaticText> uiText = ui->AddStaticText(text.c_str(), textRect, false, false);
    uiText->SetTextAlignment(UIA_CENTER, UIA_UPPERLEFT);

    bool cloudMenuBackground = cloud && Settings::Get()->GetBool("menu_clouds");
    if (cloudMenuBackground)
    {
        cloud->Update(dTime * 3000);
        cloud->PreRender(pScene);
        cloud->Render(pScene);

        Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
        Renderer::Get()->ClearBuffers();
        pScene->OnRender();
    }
    else
    {
        Renderer::Get()->SetClearColor(SColor(255, 0, 0, 0));
        Renderer::Get()->ClearBuffers();
    }

    // draw progress bar
    if ((percent >= 0) && (percent <= 100))
    {
        std::shared_ptr<Texture2> progressImg = textureSrc->GetTexture("progress_bar.png");
        std::shared_ptr<Texture2> progressImgBG = textureSrc->GetTexture("progress_bar_bg.png");

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

            Vector2<int> offset = Vector2<int>{2, 0};
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

typedef struct TextureUpdateArgs
{
    unsigned int lastTimeMs;
    unsigned int lastPercent;
    const wchar_t* textBase;

    std::shared_ptr<CloudSystemNode> cloud; 
    std::shared_ptr<BlendState> blendState;
    std::shared_ptr<Visual> visual;
    std::shared_ptr<BaseUI> ui;

    BaseTextureSource* textureSrc;
    Scene* scene;

} TextureUpdateArgs;

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
        DrawLoadScreen(strm.str(), 
            targs->ui, targs->cloud, targs->visual, targs->blendState, targs->textureSrc, 
            targs->scene, 0, 72 + (uint16_t)((18. / 100.) * (double)targs->lastPercent));
    }
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

void MinecraftHumanView::SettingsChangedCallback(const std::string& name, void* data)
{
    ((GameSettings*)data)->ReadGlobalSettings();
}

//
// MinecraftHumanView::MinecraftHumanView	- Chapter 19, page 724
//
MinecraftHumanView::MinecraftHumanView() : HumanView()
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
            &MinecraftHumanView::SettingsChangedCallback, &mGameSettings);
    }
}


MinecraftHumanView::~MinecraftHumanView()
{
    RemoveAllDelegates();

    // mGameSettings becomes invalid, remove callbacks
    for (const std::string& name : mGameSettings.settingNames)
    {
        Settings::Get()->DeregisterChangedCallback(name,
            &MinecraftHumanView::SettingsChangedCallback, &mGameSettings);
    }

    Shutdown();
}

//
// MinecraftHumanView::OnMsgProc				- Chapter 19, page 727
//
bool MinecraftHumanView::OnMsgProc(const Event& evt)
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
// MinecraftHumanView::RenderText				- Chapter 19, page 727
//
void MinecraftHumanView::RenderText()
{
    HumanView::RenderText();
}

//
// MinecraftHumanView::OnRender
//
void MinecraftHumanView::OnRender(double time, float elapsedTime)
{
    /*
        Drawing begins
    */
    const SColor& skycolor = mSky->GetSkyColor();
    Renderer::Get()->SetClearColor(skycolor);

    mGameUI->OnRender(time, elapsedTime);
}

//
// MinecraftHumanView::OnUpdate				- Chapter 19, page 730
//
void MinecraftHumanView::OnUpdate(unsigned int timeMs, unsigned long deltaTimeMs)
{
    HumanView::OnUpdate(timeMs, deltaTimeMs);

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
    UpdateInteractTimers(mDeltaTime);

    ProcessQueues();

    UpdateProfilers(mStats, mUpdateTimes, mDeltaTime);
    ProcessUserInput(mDeltaTime);
    // Update camera before player movement to avoid camera lag of one frame
    UpdatePlayerCameraDirection(&mCamViewTarget, mDeltaTime);
    mCamView.cameraYaw += (mCamViewTarget.cameraYaw - mCamView.cameraYaw) * mGameSettings.cameraSmoothing;
    mCamView.cameraPitch += (mCamViewTarget.cameraPitch - mCamView.cameraPitch) * mGameSettings.cameraSmoothing;
    UpdatePlayerControl(mCamView);
    Step(mDeltaTime);
    ProcessVisualEvents(&mCamViewTarget);
    UpdatePlayerCamera(mUpdateTimes.busyTime, mDeltaTime);
    UpdateSound(mDeltaTime);
    ProcessPlayerInteraction(mDeltaTime, mGameUI->mFlags.showHud, mGameUI->mFlags.showDebug);
    UpdateFrame(&mStats, mDeltaTime, mCamView);

    // Update if minimap has been disabled by the logic
    //mFlags.showMinimap &= ShouldShowMinimap();

    if (Settings::Get()->GetBool("pause_on_lost_focus") &&
        !System::Get()->IsWindowFocused() &&
        !mGameUI->IsMenuActive())
    {
        ShowPauseMenu();
    }

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    mGameUI->SetMenuActive(formUI ? formUI->IsActive() : false);
}


void MinecraftHumanView::Step(float dTime)
{
    bool canBeAndIsPaused = false;// = (simpleSingleplayerMode && g_menumgr.pausesGame());

    if (!canBeAndIsPaused)
    {
        if (mSimpleSingleplayerMode && !mPausedAnimatedNodes.empty())
            ResumeAnimation();

        // Limit a bit
        if (dTime > 2.f)
            dTime = 2.f;

        mTimeOfDayUpdateTimer += dTime;

        /*
            Run Map's timers and unload unused data
        */
        const float mapTimerAndUnloadDeltaTime = 5.25;
        if (mMapTimerAndUnloadInterval.Step(dTime, mapTimerAndUnloadDeltaTime))
        {
            std::vector<Vector3<short>> deletedBlocks;
            mEnvironment->GetMap()->TimerUpdate(mapTimerAndUnloadDeltaTime,
                Settings::Get()->GetFloat("client_unload_unused_data_timeout"),
                Settings::Get()->GetInt("client_mapblock_limit"),
                &deletedBlocks);

            /*
                Send info to logic
                NOTE: This loop is intentionally iterated the way it is.
            */

            std::vector<Vector3<short>>::iterator i = deletedBlocks.begin();
            std::vector<Vector3<short>> sendlist;
            for (;;)
            {
                if (sendlist.size() == 255 || i == deletedBlocks.end())
                {
                    if (sendlist.empty())
                        break;
                    /*
                        [0] unsigned short command
                        [2] unsigned char count
                        [3] Vector3<short> pos_0
                        [3+6] Vector3<short> pos_1
                        ...
                    */

                    EventManager::Get()->QueueEvent(
                        std::make_shared<EventDataDeletedBlocks>(sendlist));

                    if (i == deletedBlocks.end())
                        break;

                    sendlist.clear();
                }

                sendlist.push_back(*i);
                ++i;
            }
        }

        /*
            Handle environment
        */
        VisualPlayer* player = mEnvironment->GetPlayer();

        // Step environment (also handles player controls)
        mEnvironment->Step(dTime);
        mSoundMgr->Step(dTime);

        /*
            Get events
        */
        while (mEnvironment->HasEnvironmentEvents())
        {
            VisualEnvironmentEvent envEvent = mEnvironment->GetEnvironmentEvent();

            if (envEvent.type == VEE_PLAYER_DAMAGE)
            {
                uint16_t damage = envEvent.playerDamage.amount;

                if (envEvent.playerDamage.sendToLogic)
                {
                    EventManager::Get()->QueueEvent(
                        std::make_shared<EventDataPlayerDamage>(player->GetId(), damage));
                }


                // Add to VisualEvent queue
                VisualEvent* evt = new VisualEvent();
                evt->type = VE_PLAYER_DAMAGE;
                evt->PlayerDamage.amount = damage;
                mVisualEventQueue.push(evt);
            }
        }

        /*
            Print some info
        */
        float& counter = mAvgRTTTimer;
        counter += dTime;
        if (counter >= 10)
        {
            counter = 0.0;
            // connectedAndInitialized() is true, peer exists.
        }

        /*
            Send player position to logic
        */
        {
            float& counter = mPlayerPositionSendTimer;
            counter += dTime;
            if (mGameState == BGS_RUNNING && counter >= mRecommendedSendInterval)
            {
                counter = 0.0;
                SendPlayerPosition();
            }
        }

        /*
            Replace updated meshes
        */
        {
            int numProcessedMeshes = 0;
            std::vector<Vector3<short>> blocksToAck;
            while (!mEnvironment->mMeshUpdateThread.mQueueOut.Empty())
            {
                numProcessedMeshes++;

                MinimapMapblock *minimapMapBlock = NULL;
                bool doMapperUpdate = true;

                MeshUpdateResult r = mEnvironment->mMeshUpdateThread.mQueueOut.PopFrontNoEx();
                MapBlock* block = mEnvironment->GetMap()->GetBlockNoCreateNoEx(r.position);
                if (block)
                {
                    // Delete the old mesh
                    block->mMesh.reset();
                    block->mMesh = nullptr;
                    if (r.mesh)
                    {
                        minimapMapBlock = r.mesh->MoveMinimapMapblock();
                        if (minimapMapBlock == NULL)
                            doMapperUpdate = false;

                        bool isEmpty = true;
                        for (int l = 0; l < MAX_TILE_LAYERS; l++)
                            if (r.mesh->GetMesh(l)->GetMeshBufferCount() != 0)
                                isEmpty = false;

                        if (isEmpty)
                            r.mesh.reset();
                        else
                            // Replace with the new mesh
                            block->mMesh = r.mesh;
                    }
                }
                else r.mesh.reset();

                if (mGameUI->mMinimap && doMapperUpdate)
                    mGameUI->mMinimap->AddBlock(r.position, minimapMapBlock);

                if (r.ackBlockToLogic)
                {
                    if (blocksToAck.size() == 255)
                    {
                        EventManager::Get()->QueueEvent(
                            std::make_shared<EventDataGotBlocks>(blocksToAck));

                        blocksToAck.clear();
                    }

                    blocksToAck.emplace_back(r.position);
                }
            }
            if (blocksToAck.size() > 0)
            {
                // Acknowledge block(s)
                EventManager::Get()->QueueEvent(
                    std::make_shared<EventDataGotBlocks>(blocksToAck));
            }

            if (numProcessedMeshes > 0)
                Profiling->GraphAdd("numProcessedMeshes", (float)numProcessedMeshes);
        }

        /*
            If the logic didn't update the inventory in a while, revert
            the local inventory (so the player notices the lag problem
            and knows something is wrong).
        */
        if (mInventoryFromLogic)
        {
            float interval = 10.0f;
            float countBefore = std::floor(mInventoryFromLogicAge / interval);

            mInventoryFromLogicAge += dTime;

            float countAfter = std::floor(mInventoryFromLogicAge / interval);

            if (countAfter != countBefore)
            {
                // Do this every <interval> seconds after TOCLIENT_INVENTORY
                // Reset the locally changed inventory to the authoritative inventory
                player->mInventory = *mInventoryFromLogic;
                mUpdateWieldedItem = true;
            }
        }

        /*
            Update positions of sounds attached to objects
        */
        {
            for (auto& soundsToObject : mSoundsToObjects)
            {
                int clientId = soundsToObject.first;
                uint16_t objectId = soundsToObject.second;
                VisualActiveObject* vao = mEnvironment->GetActiveObject(objectId);
                if (!vao)
                    continue;
                mSoundMgr->UpdateSoundPosition(clientId, vao->GetPosition());
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
    else
    {
        // This is for a singleplayer logic
        dTime = 0; // No time passes
    }
}


//
// MinecraftHumanView::OnAttach				- Chapter 19, page 731
//
void MinecraftHumanView::OnAttach(GameViewId vid, ActorId aid)
{
    HumanView::OnAttach(vid, aid);
}


DrawingCore* CreateDrawingCore(const std::string& stereoMode, 
    BaseUI* ui, VisualEnvironment* vEnv, Scene* scene, Hud *hud)
{
    if (stereoMode == "none")
        return new DrawingCorePlain(ui, vEnv, scene, hud);
    else if (stereoMode == "anaglyph")
        return new DrawingCoreAnaglyph(ui, vEnv, scene, hud);
    else if (stereoMode == "interlaced")
        return new DrawingCoreInterlaced(ui, vEnv, scene, hud);
    else if (stereoMode == "sidebyside")
        return new DrawingCoreSideBySide(ui, vEnv, scene, hud);
    else if (stereoMode == "topbottom")
        return new DrawingCoreSideBySide(ui, vEnv, scene, hud, true);
    else if (stereoMode == "crossview")
        return new DrawingCoreSideBySide(ui, vEnv, scene, hud, false, true);

    // fallback to plain renderer
    LogWarning("Invalid rendering mode: " + stereoMode);
    return new DrawingCorePlain(ui, vEnv, scene, hud);
}


void MinecraftHumanView::UpdateCameraRotation(
    const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const
{
    EulerAngles<float> rotation;
    rotation.mAxis[1] = 1;
    rotation.mAxis[2] = 2;

    Quaternion<float> q(cameraPos - targetPos);
    rotation = Rotation<4, float>(q)(rotation.mAxis[0], rotation.mAxis[1], rotation.mAxis[2]);
    mCloudMgr->GetActiveCamera()->GetRelativeTransform().SetRotation(rotation);
};


bool MinecraftHumanView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
    if (!HumanView::LoadGameDelegate(pLevelData))
        return false;

    mTextureSrc = CreateTextureSource();
    mShaderSrc = CreateShaderSource();

    if (!(mTextureSrc && mShaderSrc))
        return false;

    mGameUI.reset(new MinecraftUI());
    mGameUI->OnInit();

    mInvertMouse = Settings::Get()->GetBool("invert_mouse");
    mFirstLoopAfterWindowActivation = true;

    PushElement(mGameUI);

    // Clouds
    mGameSettings.enableClouds = Settings::Get()->GetBool("enable_clouds");
    if (mGameSettings.enableClouds)
    {
        mCloudMgr.reset(new Scene());
        mCloudMgr->AddCameraNode();
        mCloudMgr->GetActiveCamera()->GetRelativeTransform().SetTranslation(Vector4<float>::Zero());
        UpdateCameraRotation(Vector4<float>::Zero(), Vector4<float>{0, 60, 100, 0});

        mClouds = std::static_pointer_cast<CloudSystemNode>(mCloudMgr->AddCloudSystemNode(0));
        mClouds->SetHeight(100.0f);
        mClouds->SetSpeed(Vector2<float>{0.f, -6.f});
        mClouds->SetMaterialType(MT_SOLID);
        mClouds->SetEffect(
            Settings::Get()->GetUInt("cloud_radius"),
            Settings::Get()->GetBool("enable_3d_clouds"));
        mClouds->Update(Vector3<float>::Zero(), SColor(255, 240, 240, 255));

        Matrix4x4<float> rotation = Rotation<4, float>(
            AxisAngle<4, float>(Vector4<float>::Unit(0), 45.f * (float)GE_C_DEG_TO_RAD));
        mClouds->GetRelativeTransform().SetRotation(rotation);
    }

    bool cloudMenuBackground = Settings::Get()->GetBool("menu_clouds");
    if (cloudMenuBackground)
    {
        mClouds->Update(0);
        mClouds->PreRender(mScene.get());
        mClouds->Render(mScene.get());

        Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
        Renderer::Get()->ClearBuffers();
        mCloudMgr->OnRender();
    }
    else
    {
        Renderer::Get()->SetClearColor(SColor(255, 0, 0, 0));
        Renderer::Get()->ClearBuffers();
    }

    mGameUI->ShowOverlayMessage(L"Loading...", mTextureSrc, 0, 0);

    Renderer::Get()->DisplayColorBuffer(0);

    // Reinit runData
    mRunData = GameRunData();
    mRunData.timeFromLastPunch = 10.0;

    //Create View
    //mGameUI->ShowOverlayMessage(L"Creating view...", mTextureSrc, 0, 10);

    mGameUI->mDrawControl = std::make_shared<MapDrawControl>();
    if (!mGameUI->mDrawControl)
        return false;

    // Pre-calculated values
    std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture("crack_anylength.png");
    if (texture)
    {
        Vector2<unsigned int> size = mTextureSrc->GetTextureOriginalSize("crack_anylength.png");
        mCrackAnimationLength = size[1] / size[0];
    }
    else mCrackAnimationLength = 5;

    // Set window caption
    std::wstring str = L"Minecraft";
    str += L" [";
    if (mSimpleSingleplayerMode)
        str += L"Singleplayer";
    else
        str += L"Multiplayer";
    str += L"]";
    System::Get()->SetWindowCaption(str.c_str());

    VisualMap* visualMap = new VisualMap(INVALID_ACTOR_ID, mScene.get(), mGameUI->mDrawControl.get());
    mEnvironment = std::make_unique<VisualEnvironment>(visualMap, mTextureSrc.get(), mShaderSrc.get());
    mScene->AddSceneNode(INVALID_ACTOR_ID, mEnvironment->GetVisualMap());
    visualMap->SetEnvironment(mEnvironment.get());

    mEnvironment->SetVisualPlayer(new VisualPlayer(
        GameLogic::Get()->GetNewActorID(), "singleplayer", mEnvironment.get()));
    if (Settings::Get()->GetBool("enable_minimap"))
    {
        mGameUI->mMinimap = new Minimap(mEnvironment.get());
        mEnvironment->SetMinimap(mGameUI->mMinimap);
        //client->getScript()->on_minimap_ready(mMinimap);
    }

    if (Settings::Get()->GetBool("enable_sound"))
    {
        if (!InitSound())
            return false;
    }

    mStats = { 0 };
    mCamViewTarget = { 0 };
    mCamView = { 0 };
    mUpdateTimes = { 0 };
    mUpdateTimes.lastTime = Timer::GetTime();

    /* Clear the profiler */
    Profiler::GraphValues dummyvalues;
    Profiling->GraphGet(dummyvalues);

    SetLightTable(Settings::Get()->GetFloat("display_gamma"));

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

    // Apply texture overrides from texturepack/override.txt
    BaseWritableItemManager* itemMgr = static_cast<BaseWritableItemManager*>(mEnvironment->GetItemManager());
    
    std::wstring texturePath = ToWideString(Settings::Get()->Get("texture_path"));
    for (const auto& path : FileSystem::Get()->GetRecursiveDirectories(texturePath))
    {
        TextureOverrideSource overrideSource(ToString(path) + "/override.txt");
        mEnvironment->GetNodeManager()->ApplyTextureOverrides(overrideSource.GetNodeTileOverrides());
        itemMgr->ApplyTextureOverrides(overrideSource.GetItemTextureOverrides());
    }

    // PlayerCamera
    mPlayerCamera = std::make_shared<PlayerCamera>(
        mEnvironment.get(), mScene.get(), mGameUI.get(), mGameUI->mDrawControl.get());
    if (!mPlayerCamera->SuccessfullyCreated())
        return false;
    mEnvironment->SetPlayerCamera(mPlayerCamera.get());

    // Skybox
    mSky = std::make_shared<Sky>(INVALID_ACTOR_ID, mTextureSrc.get(), mShaderSrc.get());
    mScene->AddSceneNode(INVALID_ACTOR_ID, mSky);
    mSkybox = NULL;	// This is used/set later on in the main run loop
    mEnvironment->SetSky(mSky.get());

    mParticleMgr = new ParticleManager(mScene.get(), mEnvironment.get());

    VisualPlayer* player = mEnvironment->GetPlayer();
    player->mHurtTiltTimer = 0;
    player->mHurtTiltStrength = 0;

    EventManager::Get()->TriggerEvent(std::make_shared<EventDataGameReady>(player->GetId()));

    mGameUI->mHud = std::make_shared<Hud>(mScene.get(), mGameUI.get(), 
        mEnvironment.get(), player, mPlayerCamera.get(), &player->mInventory);

    const std::string& drawMode = Settings::Get()->Get("mode3d");
    mGameUI->mDrawingCore.reset(CreateDrawingCore(
        drawMode, mGameUI.get(), mEnvironment.get(), mScene.get(), mGameUI->mHud.get()));
    mGameUI->mDrawingCore->Initialize();

    if (BaseGame::Get()->ModsLoaded())
        BaseGame::Get()->OnMinimapReady(mGameUI->mMinimap);

    // A movement controller is going to control the camera, 
    // but it could be constructed with any of the objects you see in this function.
    mCamera->GetRelativeTransform().SetTranslation(Vector4<float>::Zero());
    mCamera->ClearTarget();

    mScene->OnRestore();
    return true;
}

void MinecraftHumanView::SendPlayerPosition()
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    if (!player)
        return;

    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    float cameraFov = map->GetCameraFov();
    float wantedRange = map->GetControl()->wantedRange;

    // Save bandwidth by only updating position when
    // player is not dead and something changed

    if (mActiveObjectsReceived && player->IsDead())
        return;

    if (player->mLastPosition == player->GetPosition() &&
        player->mLastSpeed == player->GetSpeed() &&
        player->mLastPitch == player->GetPitch() &&
        player->mLastYaw == player->GetYaw() &&
        player->mLastKeyPressed == player->mKeyPressed &&
        player->mLastPlayerCameraFov == cameraFov &&
        player->mLastWantedRange == wantedRange)
        return;

    player->mLastPosition = player->GetPosition();
    player->mLastSpeed = player->GetSpeed();
    player->mLastPitch = player->GetPitch();
    player->mLastYaw = player->GetYaw();
    player->mLastKeyPressed = player->mKeyPressed;
    player->mLastPlayerCameraFov = cameraFov;
    player->mLastWantedRange = wantedRange;

    Vector3<int> playerPosition{ 
        (int)(player->mLastPosition[0] * 100),
        (int)(player->mLastPosition[1] * 100),
        (int)(player->mLastPosition[2] * 100) };
    Vector3<int> playerSpeed{
        (int)(player->mLastSpeed[0] * 100),
        (int)(player->mLastSpeed[1] * 100),
        (int)(player->mLastSpeed[2] * 100) };

    EventManager::Get()->QueueEvent(std::make_shared<EventDataPlayerPosition>(
        player->GetId(), player->mKeyPressed, playerPosition, playerSpeed, 
        (int)(player->GetPitch() * 100), (int)(player->GetYaw() * 100), 
        (uint8_t)(cameraFov * 80), (uint8_t)std::min(255, (int)ceil(wantedRange / MAP_BLOCKSIZE))));
}

//  Run
void MinecraftHumanView::UpdateProfilers(
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

void MinecraftHumanView::UpdateStats(RunStats* stats, const FpsControl& updateTimes, float dTime)
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


bool MinecraftHumanView::GetGameContent()
{
    ClearInput();

    FpsControl fpsControl = { 0 };
    float dTime; // in seconds
    float progress = 25.f;

    fpsControl.lastTime = Timer::GetTime();

    while (System::Get()->OnRun()) 
    {
        LimitFps(&fpsControl, &dTime);

        // Update visual
        if (mNodeReceived)
            mEnvironment->Step(dTime);

        // End condition
        if (mItemReceived && mNodeReceived && mMediaReceived)
            break;

        // Display status
        progress += dTime * 10;
        if (progress > 100.f) progress = 100.f;

        if (!mItemReceived)
        {
            const std::wstring text = L"Loading Items...";
            DrawLoadScreen(text, 
                mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), dTime, progress);
        }
        else if (!mNodeReceived)
        {
            const std::wstring text = L"Loading Nodes...";
            DrawLoadScreen(text, 
                mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), dTime, progress);
        }
        else 
        {
            const std::wstring text = L"Loading Media...";
            DrawLoadScreen(text,
                mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), dTime, progress);
        }
    }

    return true;
}

void MinecraftHumanView::AfterContentReceived()
{
    LogInformation("MinecraftHumanView::AfterContentReceived() started");
    LogAssert(mItemReceived, "no item received"); // pre-condition
    LogAssert(mNodeReceived, "no node received"); // pre-condition
    LogAssert(mMediaReceived, "no media received"); // pre-condition

    std::wstring text = L"Loading textures...";

    // Clear cached pre-scaled 2D GUI images, as this cache
    // might have images with the same name but different
    // content from previous sessions.
    mGameUI->GetSkin()->ClearTextureCache();

    // Rebuild inherited images and recreate textures
    LogInformation("- Rebuilding images and textures");
    DrawLoadScreen(text, mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), 0, 70);
    mTextureSrc->RebuildImagesAndTextures();

    // Rebuild shaders
    LogInformation("- Rebuilding shaders");
    text = L"Rebuilding shaders...";
    DrawLoadScreen(text, mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), 0, 71);
    mShaderSrc->RebuildShaders();

    // Update node aliases
    LogInformation("- Updating node aliases");
    text = L"Initializing nodes...";
    DrawLoadScreen(text, mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), 0, 72);

    mEnvironment->GetNodeManager()->UpdateAliases(mEnvironment->GetItemManager());
    std::wstring texturePath = ToWideString(Settings::Get()->Get("texture_path"));
    BaseWritableItemManager* itemMgr = static_cast<BaseWritableItemManager*>(mEnvironment->GetItemManager());
    for (const auto& path : FileSystem::Get()->GetRecursiveDirectories(texturePath))
    {
        TextureOverrideSource overrideSource(ToString(path) + "/override.txt");
        mEnvironment->GetNodeManager()->ApplyTextureOverrides(overrideSource.GetNodeTileOverrides());
        itemMgr->ApplyTextureOverrides(overrideSource.GetItemTextureOverrides());
    }
    mEnvironment->GetNodeManager()->SetNodeRegistrationStatus(true);
    mEnvironment->GetNodeManager()->RunNodeResolveCallbacks();

    // Update node textures and assign shaders to each tile
    LogInformation("- Updating node textures");
    TextureUpdateArgs textureUpdateArgs;
    textureUpdateArgs.ui = mGameUI;
    textureUpdateArgs.cloud = mClouds;
    textureUpdateArgs.scene = mScene.get();
    textureUpdateArgs.textureSrc = mTextureSrc.get();
    textureUpdateArgs.lastTimeMs = Timer::GetRealTime();
    textureUpdateArgs.lastPercent = 0;
    textureUpdateArgs.visual = mVisual;
    textureUpdateArgs.blendState = mBlendState;
    textureUpdateArgs.textBase = L"Initializing nodes";
    mEnvironment->GetNodeManager()->UpdateTextures(mEnvironment.get(), TextureUpdateProgress, &textureUpdateArgs);

    // Start mesh update thread after setting up content definitions
    LogInformation("- Starting mesh update thread");
    mEnvironment->mMeshUpdateThread.Start();

    text = L"Done!";
    DrawLoadScreen(text, mGameUI, mClouds, mVisual, mBlendState, mTextureSrc.get(), mCloudMgr.get(), 0, 100);
    LogInformation("MinecraftHumanView::afterContentReceived() done");

    mGameState = BGS_RUNNING;
}

void MinecraftHumanView::UpdateInteractTimers(float dTime)
{
    if (mRunData.nodigDelayTimer >= 0)
        mRunData.nodigDelayTimer -= dTime;

    if (mRunData.objectHitDelayTimer >= 0)
        mRunData.objectHitDelayTimer -= dTime;

    mRunData.timeFromLastPunch += dTime;
}


void MinecraftHumanView::ProcessQueues()
{
    BaseWritableItemManager* itemMgr = 
        static_cast<BaseWritableItemManager*>(mEnvironment->GetItemManager());
    mTextureSrc->ProcessQueue();
    itemMgr->ProcessQueue(mEnvironment.get());
    mShaderSrc->ProcessQueue();
}


/****************************************************************************
 Input handling
 ****************************************************************************/

void MinecraftHumanView::ProcessUserInput(float dTime)
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

    // Input handler step() (used by the random input generator)
    //input->step(dtime);

    // Increase timer for double tap of "keymap_jump"
    if (mGameSettings.doubletapJump && mRunData.jumpTimer <= 0.2f)
        mRunData.jumpTimer += dTime;

    ProcessKeyInput();
    ProcessItemSelection(&mRunData.newPlayerItem);
}


void MinecraftHumanView::ProcessKeyInput()
{
    if (WasKeyDown(KeyType::DROP)) 
    {
        DropSelectedItem(IsKeyDown(KeyType::SNEAK));
    }
    else if (WasKeyDown(KeyType::AUTOFORWARD)) 
    {
        ToggleAutoforward();
    }
    else if (WasKeyDown(KeyType::BACKWARD))
    {
        if (Settings::Get()->GetBool("continuous_forward"))
            ToggleAutoforward();
    }
    else if (WasKeyDown(KeyType::INVENTORY)) 
    {
        if (!mGameUI->IsMenuActive())
            OpenInventory();
    }
    else if (CancelPressed())
    {
        if (!mGameUI->IsMenuActive() &&
            !mGameUI->mUIChatConsole->IsOpenInhibited())
        {
            ShowPauseMenu();
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
        if (BaseGame::Get()->ModsLoaded())
            OpenConsole(0.2f, L".");
        else
            mGameUI->ShowStatusText(L"Visual side scripting is disabled");
    }
    else if (WasKeyDown(KeyType::CONSOLE)) 
    {
        OpenConsole(std::clamp(Settings::Get()->GetFloat("console_height"), 0.1f, 1.0f));
    }
    else if (WasKeyDown(KeyType::FREEMOVE))
    {
        ToggleFreeMove();
    }
    else if (WasKeyDown(KeyType::JUMP)) 
    {
        ToggleFreeMoveAlt();
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
    else if (WasKeyDown(KeyType::SCREENSHOT)) 
    {
        MakeScreenshot();
    }
    else if (WasKeyDown(KeyType::TOGGLE_HUD)) 
    {
        mGameUI->ToggleHud();
    }
    else if (WasKeyDown(KeyType::MINIMAP)) 
    {
        ToggleMinimap(IsKeyDown(KeyType::SNEAK));
    }
    else if (WasKeyDown(KeyType::TOGGLE_CHAT)) 
    {
        mGameUI->ToggleChat();
    }
    else if (WasKeyDown(KeyType::TOGGLE_FOG))
    {
        ToggleFog();
    }
    else if (WasKeyDown(KeyType::TOGGLE_UPDATE_CAMERA)) 
    {
        ToggleUpdatePlayerCamera();
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

    if (!IsKeyDown(KeyType::JUMP) && mRunData.resetJumpTimer) 
    {
        mRunData.resetJumpTimer = false;
        mRunData.jumpTimer = 0.0f;
    }
}

void MinecraftHumanView::ProcessItemSelection(unsigned short* newPlayerItem)
{
    VisualPlayer* player = mEnvironment->GetPlayer();

    // Item selection using mouse wheel
    *newPlayerItem = player->GetWieldIndex();

    int wheel = (int)GetMouseWheel();
    unsigned short maxItem = std::min(PLAYER_INVENTORY_SIZE - 1, player->mHudHotbarItemCount - 1);

    int dir = wheel;
    if (WasKeyDown(KeyType::HOTBAR_NEXT))
        dir = -1;

    if (WasKeyDown(KeyType::HOTBAR_PREV))
        dir = 1;

    if (dir < 0)
        *newPlayerItem = *newPlayerItem < maxItem ? *newPlayerItem + 1 : 0;
    else if (dir > 0)
        *newPlayerItem = *newPlayerItem > 0 ? *newPlayerItem - 1 : maxItem;
    // else dir == 0

    // Item selection using hotbar slot keys
    for (unsigned short i = 0; i <= maxItem; i++) 
    {
        if (WasKeyDown((GameKeyType)(KeyType::SLOT_1 + i))) 
        {
            *newPlayerItem = i;
            break;
        }
    }
}

void MinecraftHumanView::UpdateFrame(RunStats* stats, float dTime, const PlayerCameraOrientation& cam)
{
    TimeTaker ttUpdate("UpdateFrame()");
    VisualPlayer* player = mEnvironment->GetPlayer();

    // Fog range
    if (mGameUI->mDrawControl->rangeAll)
    {
        mRunData.fogRange = 100000 * BS;
    }
    else 
    {
        mRunData.fogRange = mGameUI->mDrawControl->wantedRange * BS;
    }

    // Calculate general brightness
    unsigned int dayNightRatio = mEnvironment->GetDayNightRatio();
    float timeBrightness = DecodeLight(dayNightRatio / 1000.f);
    float directBrightness;
    bool sunlightSeen;

    if (mGameSettings.enableNoclip && mGameSettings.enableFreeMove)
    {
        directBrightness = timeBrightness;
        sunlightSeen = true;
    }
    else 
    {
        float oldBrightness = mSky->GetBrightness();
        directBrightness = mEnvironment->GetVisualMap()->GetBackgroundBrightness(
            std::min(mRunData.fogRange * 1.2f, 60 * BS), dayNightRatio, 
            (int)(oldBrightness * 255.5f), &sunlightSeen) / 255.f;
    }

    float timeOfDaySmooth = mRunData.timeOfDaySmooth;
    float timeOfDay = mEnvironment->GetTimeOfDayFloat();

    static const float maxsm = 0.05f;
    static const float todsm = 0.05f;

    if (std::fabs(timeOfDay - timeOfDaySmooth) > maxsm &&
        std::fabs(timeOfDay - timeOfDaySmooth + 1.0) > maxsm &&
        std::fabs(timeOfDay - timeOfDaySmooth - 1.0) > maxsm)
        timeOfDaySmooth = timeOfDay;

    if (timeOfDaySmooth > 0.8f && timeOfDay < 0.2f)
        timeOfDaySmooth = timeOfDaySmooth * (1.f - todsm) + (timeOfDay + 1.f) * todsm;
    else
        timeOfDaySmooth = timeOfDaySmooth * (1.f - todsm) + timeOfDay * todsm;

    mRunData.timeOfDaySmooth = timeOfDaySmooth;

    mSky->Update(timeOfDaySmooth, timeBrightness, directBrightness,
        sunlightSeen, mPlayerCamera->GetCameraMode(), player->GetYaw(),
        player->GetPitch());

    // Update clouds
    if (mClouds) 
    {
        if (mSky->GetCloudsVisible()) 
        {
            mClouds->SetVisible(true);
            mClouds->Update(dTime);
            // camera->GetPosition is not enough for 3rd person views
            Vector3<float> cameraNodePosition =
                mPlayerCamera->GetCameraNode()->GetRelativeTransform().GetTranslation();
            Vector3<short> cameraOffset = mPlayerCamera->GetOffset();
            cameraNodePosition[0] = cameraNodePosition[0] + cameraOffset[0] * BS;
            cameraNodePosition[1] = cameraNodePosition[1] + cameraOffset[1] * BS;
            cameraNodePosition[2] = cameraNodePosition[2] + cameraOffset[2] * BS;
            mClouds->Update(cameraNodePosition, mSky->GetCloudColor());
            if (mClouds->IsCameraInsideCloud() && mGameSettings.enableFog)
            {
                // if inside clouds, and fog enabled, use that as sky
                // color(s)
                SColor cloudsDark = mClouds->GetColor().GetInterpolated(SColor(255, 0, 0, 0), 0.9f);
                mSky->OverrideColors(cloudsDark, mClouds->GetColor());
                mSky->SetInClouds(true);
                mRunData.fogRange = std::fmin(mRunData.fogRange * 0.5f, 32.0f * BS);
                // do not draw clouds after all
                mClouds->SetVisible(false);
            }
        }
        else 
        {
            mClouds->SetVisible(false);
        }
    }

    // Update particles
    mParticleMgr->Step(dTime);

    // Fog
    if (mEnvironment->GetVisualMap())
        mEnvironment->GetVisualMap()->GetControl()->fogRange = mRunData.fogRange;

    //Inventory
    if (player->GetWieldIndex() != mRunData.newPlayerItem)
    {
        mEnvironment->GetPlayer()->SetWieldIndex(mRunData.newPlayerItem);
        mUpdateWieldedItem = true;

        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataPlayerItem>(player->GetId(), mRunData.newPlayerItem));
    }

    if (UpdateWieldedItem()) 
    {
        // Update wielded tool
        ItemStack selectedItem, handItem;
        ItemStack& toolItem = player->GetWieldedItem(&selectedItem, &handItem);
        mPlayerCamera->Wield(toolItem);
    }

    /*
        Update block draw list every 200ms or when camera direction has
        changed much
    */
    mRunData.updateDrawListTimer += dTime;

    Vector3<float> cameraDirection = mPlayerCamera->GetDirection();
    if (mCameraOffsetChanged ||
        mRunData.updateDrawListTimer >= 0.2 || 
        Length(mRunData.updateDrawListLastCamDir - cameraDirection) > 0.2)
    {
        mRunData.updateDrawListTimer = 0;
        mEnvironment->GetVisualMap()->UpdateDrawList();
        mRunData.updateDrawListLastCamDir = cameraDirection;
    }

    mGameUI->Update(*stats, mGameUI->mDrawControl, mEnvironment.get(), 
        cam, mRunData.pointedOld, mGameUI->mUIChatConsole, dTime);

    /*
       make sure menu is on top
       1. Delete form menu reference if menu was removed
       2. Else, make sure form menu is on top
    */
    do 
    {
        // breakable. only runs for one iteration
        if (!mGameUI->GetFormUI())
            break;

        if (!mGameUI->GetFormUI()->IsActive())
        {
            mGameUI->DeleteFormUI();
            break;
        }

        std::shared_ptr<UIInventoryForm>& formUI =
            std::static_pointer_cast<UIInventoryForm>(mGameUI->GetFormUI());
        auto &loc = formUI->GetFormLocation();
        if (loc.type == InventoryLocation::NODEMETA) 
        {
            MapNodeMetadata* meta = 
                mEnvironment->GetVisualMap()->GetMapNodeMetadata(loc.nodePosition);
            if (!meta || meta->GetString("formspec").empty()) 
            {
                formUI->QuitForm();
                break;
            }
        }

        if (mGameUI->IsMenuActive())
            mGameUI->GetRootUIElement()->BringToFront(formUI);
    } 
    while (false);

    /*
        Damage flash
    */
    if (mRunData.damageFlash > 0.0f)
    {
        mGameUI->mDamageFlash = mRunData.damageFlash;
        mRunData.damageFlash = 0;
    }

    /*
        Damage camera tilt
    */
    if (player->mHurtTiltTimer > 0.0f) 
    {
        player->mHurtTiltTimer -= dTime * 6.0f;

        if (player->mHurtTiltTimer < 0.0f)
            player->mHurtTiltStrength = 0.0f;
    }

    /*
        Update minimap pos and rotation
    */
    if (mGameUI->mMinimap && mGameUI->mFlags.showHud)
    {
        Vector3<float> position = player->GetPosition();
        Vector3<short> playerPosition;
        playerPosition[0] = (short)((position[0] + (position[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        playerPosition[1] = (short)((position[1] + (position[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        playerPosition[2] = (short)((position[2] + (position[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        mGameUI->mMinimap->SetPosition(playerPosition);
        mGameUI->mMinimap->SetAngle(player->GetYaw());
    }

    Profiling->GraphAdd("Update frame [ms]", (float)ttUpdate.Stop(true));
}

bool MinecraftHumanView::LoadMedia(const std::string& filePath, bool fromMediaPush)
{
    std::string name;

    const char* imageExt[] = {
        ".png", ".jpg", ".bmp", ".tga",
        ".pcx", ".ppm", ".psd", ".wal", ".rgb", NULL};
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

            mTextureSrc->InsertSourceImage(ToString(fileName), texture);
            return true;
        }

        LogInformation("Couldn't load image file \"" + filePath + "\"");
        return false;
    }

    const char* soundExt[] = {
        ".0.ogg", ".1.ogg", ".2.ogg", ".3.ogg", ".4.ogg",
        ".5.ogg", ".6.ogg", ".7.ogg", ".8.ogg", ".9.ogg", ".ogg", NULL};
    name = StringRemoveEnd(filePath, soundExt);
    if (!name.empty()) 
    {
        if (mSoundMgr->LoadSound(name, filePath))
            return true;

        LogInformation("Couldn't load sound file \"" + filePath + "\"");
        return false;
    }

    const char* modelExt[] = {".x", ".b3d", ".md2", ".dae", ".obj", NULL};
    name = StringRemoveEnd(filePath, modelExt);
    if (!name.empty()) 
    {
        std::shared_ptr<ResHandle> resHandle = 
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(filePath)));
        if (resHandle)
        {
            std::shared_ptr<MeshResourceExtraData> resData =
                std::static_pointer_cast<MeshResourceExtraData>(resHandle->GetExtra());
            //mMeshData[filename] = data;
            return true;
        }
        
        LogInformation("Couldn't store model into memory : \"" + filePath + "\"");
        return false;
    }

    const char* translate_ext[] = {".tr", NULL};
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

std::string MinecraftHumanView::GetModStoragePath() const
{
    std::string currentDir = ToString(FileSystem::Get()->GetWorkingDirectory());
    return currentDir + "/mod_storage";
}

void MinecraftHumanView::SetControlledActor(ActorId actorId)
{
    // note: making the camera node a child of the player node
    // would lead to unexpected behaviour, so we don't do that.
    mPlayer = mScene->GetSceneNode(actorId);
    if (!mPlayer)
    {
        LogError("Invalid player");
        return;
    }
    mPlayerHead = mScene->AddEmptyNode(mPlayer);

    HumanView::SetControlledActor(actorId);
}

void MinecraftHumanView::GameUIUpdateDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGameUIUpdate> pCastEventData =
        std::static_pointer_cast<EventDataGameUIUpdate>(pEventData);
    if (!pCastEventData->GetUiString().empty())
        mGameplayText = pCastEventData->GetUiString();
    else
        mGameplayText.clear();
}

void MinecraftHumanView::SetActorControllerDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataSetActorController> pCastEventData =
        std::static_pointer_cast<EventDataSetActorController>(pEventData);
    //SetActorController(pCastEventData->GetActorId());

    HumanView::SetControlledActor(mPlayerCamera->GetCameraNode()->GetId());
}

void MinecraftHumanView::SendHP(ActorId actorId, uint16_t hp)
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player != NULL, "invalid player");

    uint16_t oldHP = player->mHp;
    player->mHp = hp;

    if (hp < oldHP)
    {
        // Add to VisualEvent queue
        VisualEvent* evt = new VisualEvent();
        evt->type = VE_PLAYER_DAMAGE;
        evt->PlayerDamage.amount = oldHP - hp;
        mVisualEventQueue.push(evt);
    }
}

void MinecraftHumanView::HudAddDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudAdd> pCastEventData =
        std::static_pointer_cast<EventDataHudAdd>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_HUDADD;
    evt->hudadd = new VisualEventHudAdd();
    evt->hudadd->id = pCastEventData->GetId();
    evt->hudadd->type = pCastEventData->GetType();
    evt->hudadd->pos = pCastEventData->GetPosition();
    evt->hudadd->name = pCastEventData->GetHudName();
    evt->hudadd->scale = pCastEventData->GetScale();
    evt->hudadd->text = pCastEventData->GetText();
    evt->hudadd->number = pCastEventData->GetNumber();
    evt->hudadd->item = pCastEventData->GetItem();
    evt->hudadd->dir = pCastEventData->GetDirection();
    evt->hudadd->align = pCastEventData->GetAlign();
    evt->hudadd->offset = pCastEventData->GetOffset();
    evt->hudadd->worldPos = pCastEventData->GetWorldPosition();
    evt->hudadd->size = pCastEventData->GetSize();
    evt->hudadd->zIndex = pCastEventData->GetZIndex();
    evt->hudadd->text2 = pCastEventData->GetText2();
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::HudRemoveDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudRemove> pCastEventData =
        std::static_pointer_cast<EventDataHudRemove>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_HUDRM;
    evt->HudRemove.id = pCastEventData->GetId();
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::HudChangeDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudChange> pCastEventData =
        std::static_pointer_cast<EventDataHudChange>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_HUDCHANGE;
    evt->hudChange = new VisualEventHudChange();
    evt->hudChange->id = pCastEventData->GetId();
    evt->hudChange->stat = static_cast<HudElementStat>(pCastEventData->GetStat());

    switch (evt->hudChange->stat)
    {
        case HUD_STAT_POS:
        case HUD_STAT_SCALE:
        case HUD_STAT_ALIGN:
        case HUD_STAT_OFFSET:
            evt->hudChange->v2fData =*(Vector2<float> *)pCastEventData->GetValue();
            break;
        case HUD_STAT_NAME:
        case HUD_STAT_TEXT:
        case HUD_STAT_TEXT2:
            evt->hudChange->sData = *(std::string *)pCastEventData->GetValue();
            break;
        case HUD_STAT_WORLD_POS:
            evt->hudChange->v3fData = *(Vector3<float> *)pCastEventData->GetValue();
            break;
        case HUD_STAT_SIZE:
            evt->hudChange->v2sData = *(Vector2<int> *)pCastEventData->GetValue();
            break;
        case HUD_STAT_NUMBER:
        case HUD_STAT_ITEM:
        case HUD_STAT_DIR:
        default:
            evt->hudChange->data = *(unsigned int *)pCastEventData->GetValue();
            break;
    }

    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::HudSetFlagsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudSetFlags> pCastEventData =
        std::static_pointer_cast<EventDataHudSetFlags>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    bool wasMinimapVisible = player->mHudFlags & HUD_FLAG_MINIMAP_VISIBLE;
    bool wasMinimapRadarVisible = player->mHudFlags & HUD_FLAG_MINIMAP_RADAR_VISIBLE;

    player->mHudFlags &= ~pCastEventData->GetMask();
    player->mHudFlags |= pCastEventData->GetFlags();

    mGameUI->mMinimapDisabled = !(player->mHudFlags & HUD_FLAG_MINIMAP_VISIBLE);
    bool minimapRadarDisabled = !(player->mHudFlags & HUD_FLAG_MINIMAP_RADAR_VISIBLE);

    // Not so satisying code to keep compatibility with old fixed mode system
    // -->

    // Hide minimap if it has been disabled by the logic
    if (mGameUI->mMinimap && mGameUI->mMinimapDisabled && wasMinimapVisible)
        // defers a minimap update, therefore only call it if really
        // needed, by checking that minimap was visible before
        mGameUI->mMinimap->SetModeIndex(0);

    // If radar has been disabled, try to find a non radar mode or fall back to 0
    if (mGameUI->mMinimap && minimapRadarDisabled && wasMinimapRadarVisible)
    {
        while (mGameUI->mMinimap->GetModeIndex() > 0 && mGameUI->mMinimap->GetMode().type == MINIMAP_TYPE_RADAR)
            mGameUI->mMinimap->NextMode();
    }
    // <--
    // End of 'not so satifying code'
}

void MinecraftHumanView::HudSetParamDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudSetParam> pCastEventData =
        std::static_pointer_cast<EventDataHudSetParam>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    if (pCastEventData->GetParam() == HUD_PARAM_HOTBAR_ITEMCOUNT && 
        pCastEventData->GetValue().size() == 4)
    {
        int hotbarItemCount = ReadInt32((unsigned char*)pCastEventData->GetValue().c_str());
        if (hotbarItemCount > 0 && hotbarItemCount <= HUD_HOTBAR_ITEMCOUNT_MAX)
            player->mHudHotbarItemCount = hotbarItemCount;
    }
    else if (pCastEventData->GetParam() == HUD_PARAM_HOTBAR_IMAGE)
    {
        player->mHotbarImage = pCastEventData->GetValue();
    }
    else if (pCastEventData->GetParam() == HUD_PARAM_HOTBAR_SELECTED_IMAGE)
    {
        player->mHotbarSelectedImage = pCastEventData->GetValue();
    }
}

void MinecraftHumanView::HudSetSkyDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudSetSky> pCastEventData =
        std::static_pointer_cast<EventDataHudSetSky>(pEventData);

    SkyboxParams skybox;
    skybox.bgcolor = pCastEventData->GetBGColor();
    skybox.type = pCastEventData->GetType();
    skybox.clouds = pCastEventData->GetClouds();
    skybox.fogSunTint = pCastEventData->GetFogSunTint();
    skybox.fogMoonTint = pCastEventData->GetFogMoonTint();
    skybox.fogTintType = pCastEventData->GetFogTintType();
    skybox.skyColor = pCastEventData->GetSkyColor();
    skybox.textures = pCastEventData->GetTextures();

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_SET_SKY;
    evt->setSky = new SkyboxParams(skybox);
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::HudSetSunDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudSetSun> pCastEventData =
        std::static_pointer_cast<EventDataHudSetSun>(pEventData);

    SunParams sun;
    sun.texture = pCastEventData->GetTexture();
    sun.toneMap = pCastEventData->GetToneMap();
    sun.sunrise = pCastEventData->GetSunrise();
    sun.sunriseVisible = pCastEventData->GetSunriseVisible();
    sun.visible = pCastEventData->GetVisible();
    sun.scale = pCastEventData->GetScale();

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_SET_SUN;
    evt->sunParams = new SunParams(sun);
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::HudSetMoonDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudSetMoon> pCastEventData =
        std::static_pointer_cast<EventDataHudSetMoon>(pEventData);

    MoonParams moon;
    moon.texture = pCastEventData->GetTexture();
    moon.toneMap = pCastEventData->GetToneMap();
    moon.visible = pCastEventData->GetVisible();
    moon.scale = pCastEventData->GetScale();

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_SET_MOON;
    evt->moonParams = new MoonParams(moon);
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::HudSetStarsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHudSetStars> pCastEventData =
        std::static_pointer_cast<EventDataHudSetStars>(pEventData);

    StarParams stars;
    stars.starcolor = pCastEventData->GetColor();
    stars.count = pCastEventData->GetCount();
    stars.visible = pCastEventData->GetVisible();
    stars.scale = pCastEventData->GetScale();

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_SET_STARS;
    evt->starParams = new StarParams(stars);
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::SetCloudsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataSetClouds> pCastEventData =
        std::static_pointer_cast<EventDataSetClouds>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_CLOUD_PARAMS;
    evt->CloudParams.density = pCastEventData->GetDensity();
    // use the underlying unsigned int representation, because we can't
    // use struct members with constructors here, and this way
    // we avoid using new() and delete() for no good reason
    evt->CloudParams.colorBright = pCastEventData->GetBrightColor().mColor;
    evt->CloudParams.colorAmbient = pCastEventData->GetAmbientColor().mColor;
    evt->CloudParams.height = pCastEventData->GetHeight();
    evt->CloudParams.thickness = pCastEventData->GetThickness();
    // same here: deconstruct to skip constructor
    evt->CloudParams.speedX = pCastEventData->GetSpeed()[0];
    evt->CloudParams.speedY = pCastEventData->GetSpeed()[1];
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::OverrideDayNightRatioDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataOverrideDayNightRatio> pCastEventData =
        std::static_pointer_cast<EventDataOverrideDayNightRatio>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_OVERRIDE_DAY_NIGHT_RATIO;
    evt->OverrideDayNightRatio.doOverride = pCastEventData->GetOverride();
    evt->OverrideDayNightRatio.ratio = pCastEventData->GetRatio();
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::SetTimeOfDayDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataTimeOfDay> pCastEventData =
        std::static_pointer_cast<EventDataTimeOfDay>(pEventData);

    unsigned short timeOfDay = pCastEventData->GetTimeDay() % 24000;
    float timeSpeed = pCastEventData->GetTimeSpeed();
    /*
    {
        // Old message; try to approximate speed of time by ourselves
        float timeOfDayFloat = (float)timeOfDay / 24000.0f;
        float todDiff = 0;

        if (timeOfDayFloat < 0.2 && mLastTimeOfDay > 0.8)
            todDiff = timeOfDayFloat - mLastTimeOfDay + 1.0f;
        else
            todDiff = timeOfDayFloat - mLastTimeOfDay;

        mLastTimeOfDay = timeOfDayFloat;
        float timeDiff = mTimeOfDayUpdateTimer;
        mTimeOfDayUpdateTimer = 0;

        if (mTimeOfDaySet)
        {
            timeSpeed = (3600.0f * 24.0f) * todDiff / timeDiff;
            LogInformation(
                "Measured timeOfDay speed (old format): " +
                std::to_string(timeSpeed) + " todDiff=" +
                std::to_string(todDiff) + " timeDiff=" +
                std::to_string(timeDiff));
        }
    }
    */
    // Update environment
    mEnvironment->SetTimeOfDay(timeOfDay);
    mEnvironment->SetTimeOfDaySpeed(timeSpeed);
    mTimeOfDaySet = true;

    //unsigned int dr = m_env.getDayNightRatio();
    //infostream << "time_of_day=" << time_of_day
    //		<< " time_speed=" << time_speed
    //		<< " dr=" << dr << std::endl;

}

void MinecraftHumanView::ActiveObjectRemoveAddDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataActiveObjectRemoveAdd> pCastEventData =
        std::static_pointer_cast<EventDataActiveObjectRemoveAdd>(pEventData);

    /*
        unsigned short count of removed objects
        for all removed objects
        {
            unsigned short id
        }
        unsigned short count of added objects
        for all added objects
        {
            unsigned short id
            unsigned char type
            unsigned int initialization data length
            string initialization data
        }
    */

    try
    {
        std::vector<uint8_t> data(pCastEventData->GetData().c_str(), 
            pCastEventData->GetData().c_str() + pCastEventData->GetData().size() + 1);

        unsigned char type;
        unsigned short removedCount, addedCount, id;
        unsigned int offset = 0;

        // Read removed objects
        removedCount = ReadUInt16(&data[offset]);
        offset += 2;
        for (unsigned short i = 0; i < removedCount; i++)
        {
            id = ReadUInt16(&data[offset]);
            offset += 2;

            mEnvironment->RemoveActiveObject(id);
        }

        // Read added objects
        addedCount = ReadUInt16(&data[offset]);
        offset += 2;
        for (unsigned short i = 0; i < addedCount; i++)
        {
            id = ReadUInt16(&data[offset]);
            offset += 2;

            type = ReadUInt8(&data[offset]);
            offset += 1;

            std::string str;
            if (offset + 4 < data.size())
            {
                unsigned int strLen = ReadUInt32(&data[offset]);
                offset += 4;

                str.reserve(strLen);
                str.append((char*)&data[offset], strLen);
                offset += strLen;
            }
            mEnvironment->AddActiveObject(id, type, str);
        }
    }
    catch (const BaseException &e)
    {
        LogInformation("HandleActiveObjectRemoveAdd: " + std::string(e.what()) +
            ". The packet is unreliable, ignoring");
    }

    // mActiveObjectsReceived is false before the first
    // TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD packet is received
    mActiveObjectsReceived = true;
}


void MinecraftHumanView::ActiveObjectMessagesDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataActiveObjectMessages> pCastEventData =
        std::static_pointer_cast<EventDataActiveObjectMessages>(pEventData);

    /*
        for all objects
        {
            unsigned short id
            unsigned short message length
            string message
        }
    */
    std::istringstream is(pCastEventData->GetData(), std::ios_base::binary);

    try
    {
        while (is.good())
        {

            unsigned short id = ReadUInt16(is);
            if (!is.good())
                break;

            std::string message = DeserializeString16(is);

            // Pass on to the environment
            mEnvironment->ProcessActiveObjectMessage(id, message);
        }
    }
    catch (const BaseException &e)
    {
        LogError("MinecraftHumanView::HandleActiveObjectMessages: "
            "caught SerializationError: " + std::string(e.what()));
    }
}

void MinecraftHumanView::ShowFormDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataShowForm> pCastEventData =
        std::static_pointer_cast<EventDataShowForm>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_SHOW_FORM;
    // pointer is required as event is a struct only!
    // adding a std:string to a struct isn't possible
    evt->ShowForm.form = new std::string(pCastEventData->GetForm());
    evt->ShowForm.formName = new std::string(pCastEventData->GetFormName());
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::DeathScreenDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataDeathScreen> pCastEventData =
        std::static_pointer_cast<EventDataDeathScreen>(pEventData);

    bool setCameraPointTarget = pCastEventData->SetCameraPointTarget();
    Vector3<float> cameraPointTarget = pCastEventData->GetCameraPointTarget();

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_DEATHSCREEN;
    evt->DeathScreen.setCameraPointTarget = setCameraPointTarget;
    evt->DeathScreen.cameraPointTargetX = cameraPointTarget[0];
    evt->DeathScreen.cameraPointTargetY = cameraPointTarget[1];
    evt->DeathScreen.cameraPointTargetZ = cameraPointTarget[2];
    mVisualEventQueue.push(evt);

    ShowDeathForm();

    /* Handle visualization */
    VisualPlayer *player = mEnvironment->GetPlayer();
    mRunData.damageFlash = 0;
    player->mHurtTiltTimer = 0;
    player->mHurtTiltStrength = 0;
}

void MinecraftHumanView::InitChatDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataInitChat> pCastEventData =
        std::static_pointer_cast<EventDataInitChat>(pEventData);

    mGameUI->mUIChatConsole->SetChat(pCastEventData->GetChat());
}

void MinecraftHumanView::UpdateChatDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataUpdateChat> pCastEventData =
        std::static_pointer_cast<EventDataUpdateChat>(pEventData);

    // Display all messages in a static text element
    mGameUI->SetChatText(pCastEventData->GetChat(), pCastEventData->GetLineCount());
}

void MinecraftHumanView::MovementDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataMovement> pCastEventData =
        std::static_pointer_cast<EventDataMovement>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player != NULL, "invalid player");

    player->mMovementAccelerationDefault = pCastEventData->GetAccelDefault() * BS;
    player->mMovementAccelerationAir = pCastEventData->GetAccelAir() * BS;
    player->mMovementAccelerationFast = pCastEventData->GetAccelFast() * BS;
    player->mMovementSpeedWalk = pCastEventData->GetSpeedWalk() * BS;
    player->mMovementSpeedCrouch = pCastEventData->GetSpeedCrouch() * BS;
    player->mMovementSpeedFast = pCastEventData->GetSpeedFast() * BS;
    player->mMovementSpeedClimb = pCastEventData->GetSpeedClimb() * BS;
    player->mMovementSpeedJump = pCastEventData->GetSpeedJump() * BS;
    player->mMovementLiquidFluidity = pCastEventData->GetLiquidFluidity() * BS;
    player->mMovementLiquidFluiditySmooth = pCastEventData->GetLiquidFluiditySmooth() * BS;
    player->mMovementLiquidSink = pCastEventData->GetLiquidSink() * BS;
    player->mMovementGravity = pCastEventData->GetGravity() * BS;
}

void MinecraftHumanView::PlayerSpeedDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerSpeed> pCastEventData =
        std::static_pointer_cast<EventDataPlayerSpeed>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");
    player->AddVelocity(pCastEventData->GetVelocity());
}

void MinecraftHumanView::PlayerHPDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerHP> pCastEventData =
        std::static_pointer_cast<EventDataPlayerHP>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    unsigned short oldhp = player->mHp;

    unsigned short hp = pCastEventData->GetHP();
    player->mHp = hp;

    if (hp < oldhp)
    {
        // Add to VisualEvent queue
        VisualEvent* evt = new VisualEvent();
        evt->type = VE_PLAYER_DAMAGE;
        evt->PlayerDamage.amount = oldhp - hp;
        mVisualEventQueue.push(evt);
    }
}

void MinecraftHumanView::PlayerBreathDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerBreath> pCastEventData =
        std::static_pointer_cast<EventDataPlayerBreath>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    player->SetBreath(pCastEventData->GetBreath());
}

void MinecraftHumanView::PlayerInventoryFormDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerInventoryForm> pCastEventData =
        std::static_pointer_cast<EventDataPlayerInventoryForm>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    // Store form in VisualPlayer
    player->mInventoryForm = pCastEventData->GetInventoryForm();
}

void MinecraftHumanView::PlayerEyeOffsetDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerEyeOffset> pCastEventData =
        std::static_pointer_cast<EventDataPlayerEyeOffset>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    player->mEyeOffsetFirst = pCastEventData->GetFirst();
    player->mEyeOffsetThird = pCastEventData->GetThird();
}

void MinecraftHumanView::PlayerAnimationsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerAnimations> pCastEventData =
        std::static_pointer_cast<EventDataPlayerAnimations>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    unsigned int anim = 0;
    for (auto frame : pCastEventData->GetFrames())
    {
        player->mLocalAnimations[anim] = frame;
        anim++;
    }

    player->mLocalAnimationSpeed = pCastEventData->GetSpeed();
}

void MinecraftHumanView::PlayerMoveDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerMove> pCastEventData =
        std::static_pointer_cast<EventDataPlayerMove>(pEventData);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");

    Vector3<float> pos = pCastEventData->GetPosition();
    EulerAngles<float> rot = pCastEventData->GetRotation();
    float yaw = rot.mAngle[1] * (float)GE_C_RAD_TO_DEG;
    float pitch = rot.mAngle[2] * (float)GE_C_RAD_TO_DEG;
    player->SetPosition(pos);

    LogInformation("Visual pos=(" + std::to_string(pos[0]) +
        "," + std::to_string(pos[1]) + "," + std::to_string(pos[1]) + ")" +
        " pitch=" + std::to_string(pitch) + " yaw=" + std::to_string(yaw));

    /*
        Add to VisualEvent queue.
        This has to be sent to the main program because otherwise
        it would just force the pitch and yaw values to whatever
        the camera points to.
    */
    VisualEvent* evt = new VisualEvent();
    evt->type = VE_PLAYER_FORCE_MOVE;
    evt->PlayerForceMove.pitch = pitch;
    evt->PlayerForceMove.yaw = yaw;
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::PlayerRegainGroundDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerRegainGround> pCastEventData =
        std::static_pointer_cast<EventDataPlayerRegainGround>(pEventData);

    mSoundMaker->PlayPlayerStep();
}

void MinecraftHumanView::PlayerJumpDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerJump> pCastEventData =
        std::static_pointer_cast<EventDataPlayerJump>(pEventData);

    mSoundMaker->PlayPlayerJump();
}

void MinecraftHumanView::PlayerFallingDamageDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlayerFallingDamage> pCastEventData =
        std::static_pointer_cast<EventDataPlayerFallingDamage>(pEventData);

    mSoundMaker->mSound->PlaySoundGlobal(SimpleSound("player_falling_damage", 0.5), false);
}

void MinecraftHumanView::HandlePlaySoundAtDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataPlaySoundAt> pCastEventData =
        std::static_pointer_cast<EventDataPlaySoundAt>(pEventData);

    mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
        pCastEventData->GetGain(), pCastEventData->GetPosition(), pCastEventData->GetPitch());
}

void MinecraftHumanView::HandlePlaySoundDelegate(BaseEventDataPtr pEventData)
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
            VisualActiveObject* vao = mEnvironment->GetActiveObject(pCastEventData->GetObjectId());
            if (vao)
                pos = vao->GetPosition();
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

void MinecraftHumanView::HandleStopSoundDelegate(BaseEventDataPtr pEventData)
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

void MinecraftHumanView::HandleFadeSoundDelegate(BaseEventDataPtr pEventData)
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

void MinecraftHumanView::SpawnParticleDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataSpawnParticle> pCastEventData =
        std::static_pointer_cast<EventDataSpawnParticle>(pEventData);

    std::istringstream is(pCastEventData->GetData(), std::ios_base::binary);

    uint16_t protoVersion = 39;
    ParticleParameters particle;
    particle.Deserialize(is, protoVersion);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_SPAWN_PARTICLE;
    evt->spawnParticle = new ParticleParameters(particle);
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::AddParticleSpawnerDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataAddParticleSpawner> pCastEventData =
        std::static_pointer_cast<EventDataAddParticleSpawner>(pEventData);

    ParticleSpawnerParameters params;
    params.amount = pCastEventData->GetAmount();
    params.time = pCastEventData->GetTime();
    params.minPos = pCastEventData->GetMinPos();
    params.maxPos = pCastEventData->GetMaxPos();
    params.minVel = pCastEventData->GetMinVel();
    params.maxVel = pCastEventData->GetMaxVel();
    params.minAcc = pCastEventData->GetMinAcc();
    params.maxAcc = pCastEventData->GetMaxAcc();
    params.minExpTime = pCastEventData->GetMinExpTime();
    params.maxExpTime = pCastEventData->GetMaxExpTime();
    params.minSize = pCastEventData->GetMinSize();
    params.maxSize = pCastEventData->GetMaxSize();
    params.collisionDetection = pCastEventData->GetCollisionDetection();
    params.texture = pCastEventData->GetTexture();

    params.vertical = pCastEventData->GetVertical();
    params.collisionRemoval = pCastEventData->GetCollisionRemoval();

    std::istringstream is(pCastEventData->GetAnimation(), std::ios_base::binary);
    params.animation.Deserialize(is);
    params.glow = pCastEventData->GetGlow();    
    params.objectCollision = pCastEventData->GetObjectCollision();
    params.node.param0 = pCastEventData->GetNodeParam0();
    params.node.param2 = pCastEventData->GetNodeParam2();
    params.nodeTile = pCastEventData->GetNodeTile();

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_ADD_PARTICLESPAWNER;
    evt->AddParticleSpawner.parameters = new ParticleSpawnerParameters(params);
    evt->AddParticleSpawner.attachedId = pCastEventData->GetAttachedId();
    evt->AddParticleSpawner.id = pCastEventData->GetId();
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::DeleteParticleSpawnerDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataDeleteParticleSpawner> pCastEventData =
        std::static_pointer_cast<EventDataDeleteParticleSpawner>(pEventData);

    VisualEvent* evt = new VisualEvent();
    evt->type = VE_DELETE_PARTICLESPAWNER;
    evt->DeleteParticleSpawner.id = pCastEventData->GetId();
    mVisualEventQueue.push(evt);
}

void MinecraftHumanView::ViewBobbingStepDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataViewBobbingStep> pCastEventData =
        std::static_pointer_cast<EventDataViewBobbingStep>(pEventData);

    mSoundMaker->PlayPlayerStep();
}

void MinecraftHumanView::CameraPunchLeftDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataCameraPunchLeft> pCastEventData =
        std::static_pointer_cast<EventDataCameraPunchLeft>(pEventData);

    mSoundMaker->mSound->PlaySoundGlobal(mSoundMaker->mPlayerLeftPunchSound, false);
}

void MinecraftHumanView::CameraPunchRightDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataCameraPunchRight> pCastEventData =
        std::static_pointer_cast<EventDataCameraPunchRight>(pEventData);

    mSoundMaker->mSound->PlaySoundGlobal(mSoundMaker->mPlayerRightPunchSound, false);
}

void MinecraftHumanView::HandleMapNodeRemoveDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataMapNodeRemove> pCastEventData =
        std::static_pointer_cast<EventDataMapNodeRemove>(pEventData);

    mEnvironment->RemoveNode(pCastEventData->GetPoint());
}

void MinecraftHumanView::HandleMapNodeAddDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataMapNodeAdd> pCastEventData =
        std::static_pointer_cast<EventDataMapNodeAdd>(pEventData);

    mEnvironment->AddNode(pCastEventData->GetPoint(), pCastEventData->GetNode());
}

void MinecraftHumanView::HandleMapNodeDugDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataMapNodeDug> pCastEventData =
        std::static_pointer_cast<EventDataMapNodeDug>(pEventData);

    const ContentFeatures& cFeatures = mSoundMaker->mNodeMgr->Get(pCastEventData->GetNode());
    mSoundMaker->mSound->PlaySoundGlobal(cFeatures.soundDug, false);
}

void MinecraftHumanView::ChangePasswordDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataChangePassword> pCastEventData =
        std::static_pointer_cast<EventDataChangePassword>(pEventData);

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{100, 100};
    rect.mCenter = rect.mExtent / 2;

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    formUI = std::make_shared<UIPasswordChange>(mGameUI.get(), -1, rect);
    formUI->SetParent(mGameUI->GetRootUIElement());
    formUI->OnInit();
}

void MinecraftHumanView::ChangeVolumeDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataChangeVolume> pCastEventData =
        std::static_pointer_cast<EventDataChangeVolume>(pEventData);

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ 100, 100 };
    rect.mCenter = rect.mExtent / 2;

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    formUI = std::make_shared<UIVolumeChange>(mGameUI.get(), -1, rect);
    formUI->SetParent(mGameUI->GetRootUIElement());
    formUI->OnInit();
}

void MinecraftHumanView::HandleBlockDataDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleBlockData> pCastEventData =
        std::static_pointer_cast<EventDataHandleBlockData>(pEventData);

    MapSector* sector;
    MapBlock* block;
    std::istringstream istr(pCastEventData->GetData(), std::ios_base::binary);

    Vector3<short> pos3d = pCastEventData->GetPosition();
    Vector2<short> pos2d{ pos3d[0], pos3d[2] };
    sector = mEnvironment->GetVisualMap()->EmergeSector(pos2d);

    LogAssert(sector->GetPosition() == pos2d, "invalid position");

    uint8_t version = SER_FMT_VER_HIGHEST_READ;
    block = sector->GetBlockNoCreateNoEx(pos3d[1]);
    if (block)
    {
        // Update an existing block
        block->Deserialize(istr, version, false);
        block->DeserializeNetworkSpecific(istr);
    }
    else
    {
        // Create a new block
        block = new MapBlock(mEnvironment->GetMap().get(), mEnvironment.get(), pos3d);
        block->Deserialize(istr, version, false);
        block->DeserializeNetworkSpecific(istr);
        sector->InsertBlock(block);
    }

    EventManager::Get()->TriggerEvent(std::make_shared<EventDataSaveBlockData>(block));

    // Add it to mesh update queue and set it to be acknowledged after update.
   mEnvironment->AddUpdateMeshTaskWithEdge(pos3d, true);
}


void MinecraftHumanView::HandleNodeMetaChangedDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleNodeMetaChanged> pCastEventData =
        std::static_pointer_cast<EventDataHandleNodeMetaChanged>(pEventData);

    std::istringstream is(pCastEventData->GetData(), std::ios::binary);
    std::stringstream sstr;
    DecompressZlib(is, sstr);

    MapNodeMetadataList metaUpdatesList(false);
    metaUpdatesList.Deserialize(sstr, mEnvironment->GetItemManager(), true);

    for (MapNodeMetadataMap::const_iterator i = metaUpdatesList.Begin(); i != metaUpdatesList.End(); ++i)
    {
        Vector3<short> pos = i->first;

        if (mEnvironment->GetMap()->IsValidPosition(pos) &&
            mEnvironment->GetMap()->SetMapNodeMetadata(pos, i->second))
            continue; // Prevent from deleting metadata

        // Meta couldn't be set, unused metadata
        delete i->second;
    }
}

void MinecraftHumanView::HandleNodesDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleNodes> pCastEventData =
        std::static_pointer_cast<EventDataHandleNodes>(pEventData);

    //Node definition

    /*
        unsigned short command
        unsigned int length of the next item
        zlib-compressed serialized NodeDefManager
    */
    std::ostringstream os(std::ios::binary);
    CompressZlib(pCastEventData->GetData(), os);

    // Make data buffer
    //LogInformation("Sending node definitions to id( " + std::to_string(actorId) + " ):");

    LogInformation("Received node definitions:");

    // Mesh update thread must be stopped while
    // updating content definitions
    LogAssert(!mEnvironment->mMeshUpdateThread.IsRunning(), "mesh update thread must be stopped");

    // Deserialize node definitions
    std::istringstream is(os.str());
    std::stringstream sstr;
    DecompressZlib(is, sstr);

    // Deserialize node definitions
    mEnvironment->GetNodeManager()->Deserialize(sstr);

    mNodeReceived = true;
}

void MinecraftHumanView::HandleItemsDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleItems> pCastEventData =
        std::static_pointer_cast<EventDataHandleItems>(pEventData);

    //Item definition
    std::ostringstream os(std::ios::binary);
    CompressZlib(pCastEventData->GetData(), os);

    // Make data buffer
    //LogInformation("Sending item definitions to id(" + actorId +  "): size=" << pkt.getSize());

        //LogInformation("Received item definitions: packet size: " + pkt->getSize());

    // Mesh update thread must be stopped while
    // updating content definitions
    LogAssert(!mEnvironment->mMeshUpdateThread.IsRunning(), "mesh update thread must be stopped");

    // Decompress item definitions
    std::istringstream is(os.str());
    std::stringstream sstr;
    DecompressZlib(is, sstr);

    // Deserialize node definitions
    BaseWritableItemManager* itemMgr =
        static_cast<BaseWritableItemManager*>(mEnvironment->GetItemManager());
    itemMgr->Deserialize(sstr);
    mItemReceived = true;
}

void MinecraftHumanView::HandleInventoryDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleInventory> pCastEventData =
        std::static_pointer_cast<EventDataHandleInventory>(pEventData);

    std::istringstream is(pCastEventData->GetInventory(), std::ios_base::binary);

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player != NULL, "invalid player");

    player->mInventory.Deserialize(is);

    mUpdateWieldedItem = true;

    delete mInventoryFromLogic;
    mInventoryFromLogic = new Inventory(player->mInventory);
    mInventoryFromLogicAge = 0.0;
}

void MinecraftHumanView::HandleDetachedInventoryDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleDetachedInventory> pCastEventData =
        std::static_pointer_cast<EventDataHandleDetachedInventory>(pEventData);

    mEnvironment->HandleDetachedInventory(
        pCastEventData->GetInventoryName(), pCastEventData->GetInventory(), pCastEventData->KeepInventory());
}


void MinecraftHumanView::HandleMediaDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataHandleMedia> pCastEventData =
        std::static_pointer_cast<EventDataHandleMedia>(pEventData);

    // Mesh update thread must be stopped while
    // updating content definitions
    LogAssert(!mEnvironment->mMeshUpdateThread.IsRunning(), "mesh update thread must be stopped");

    // Check media cache
    for (auto media : pCastEventData->GetMedia())
        LoadMedia(ToString(media.second));

    mMediaReceived = true;
}

void MinecraftHumanView::ChangeMenuDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataChangeMenu> pCastEventData =
        std::static_pointer_cast<EventDataChangeMenu>(pEventData);

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ 100, 100 };
    rect.mCenter = rect.mExtent / 2;

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    formUI = std::make_shared<UIKeyChange>(mGameUI.get(), -1, rect);
    formUI->SetParent(mGameUI->GetRootUIElement());
    formUI->OnInit();
}

void MinecraftHumanView::RegisterAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::GameUIUpdateDelegate),
        EventDataGameUIUpdate::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::SetActorControllerDelegate),
        EventDataSetActorController::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::InitChatDelegate),
        EventDataInitChat::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::UpdateChatDelegate),
        EventDataUpdateChat::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudAddDelegate),
        EventDataHudAdd::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudRemoveDelegate),
        EventDataHudRemove::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudChangeDelegate),
        EventDataHudChange::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetFlagsDelegate),
        EventDataHudSetFlags::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetParamDelegate),
        EventDataHudSetParam::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetSkyDelegate),
        EventDataHudSetSky::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetSunDelegate),
        EventDataHudSetSun::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetMoonDelegate),
        EventDataHudSetMoon::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::SetCloudsDelegate),
        EventDataSetClouds::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::SetTimeOfDayDelegate),
        EventDataTimeOfDay::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ActiveObjectRemoveAddDelegate),
        EventDataActiveObjectRemoveAdd::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ActiveObjectMessagesDelegate),
        EventDataActiveObjectMessages::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ShowFormDelegate),
        EventDataShowForm::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::DeathScreenDelegate),
        EventDataDeathScreen::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::MovementDelegate),
        EventDataMovement::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerHPDelegate),
        EventDataPlayerHP::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerBreathDelegate),
        EventDataPlayerBreath::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerInventoryFormDelegate),
        EventDataPlayerInventoryForm::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerAnimationsDelegate),
        EventDataPlayerAnimations::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerRegainGroundDelegate),
        EventDataPlayerRegainGround::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerMoveDelegate),
        EventDataPlayerMove::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerJumpDelegate),
        EventDataPlayerJump::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerFallingDamageDelegate),
        EventDataPlayerFallingDamage::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandlePlaySoundAtDelegate),
        EventDataPlaySoundAt::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandlePlaySoundDelegate),
        EventDataPlaySoundType::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleStopSoundDelegate),
        EventDataStopSound::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleFadeSoundDelegate),
        EventDataFadeSound::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::SpawnParticleDelegate),
        EventDataSpawnParticle::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::AddParticleSpawnerDelegate),
        EventDataAddParticleSpawner::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::DeleteParticleSpawnerDelegate),
        EventDataDeleteParticleSpawner::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ViewBobbingStepDelegate),
        EventDataViewBobbingStep::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::CameraPunchLeftDelegate),
        EventDataCameraPunchLeft::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::CameraPunchRightDelegate),
        EventDataCameraPunchRight::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMapNodeRemoveDelegate),
        EventDataMapNodeRemove::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMapNodeAddDelegate),
        EventDataMapNodeAdd::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMapNodeDugDelegate),
        EventDataMapNodeDug::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ChangePasswordDelegate),
        EventDataChangePassword::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ChangeVolumeDelegate),
        EventDataChangeVolume::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::ChangeMenuDelegate),
        EventDataChangeMenu::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleBlockDataDelegate),
        EventDataHandleBlockData::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleNodeMetaChangedDelegate),
        EventDataHandleNodeMetaChanged::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleNodesDelegate),
        EventDataHandleNodes::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleItemsDelegate),
        EventDataHandleItems::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleInventoryDelegate),
        EventDataHandleInventory::skEventType);
    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleDetachedInventoryDelegate),
        EventDataHandleDetachedInventory::skEventType);

    pGlobalEventManager->AddListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMediaDelegate),
        EventDataHandleMedia::skEventType);
}

void MinecraftHumanView::RemoveAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::GameUIUpdateDelegate),
        EventDataGameUIUpdate::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::SetActorControllerDelegate),
        EventDataSetActorController::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::InitChatDelegate),
        EventDataInitChat::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::UpdateChatDelegate),
        EventDataUpdateChat::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudAddDelegate),
        EventDataHudAdd::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudRemoveDelegate),
        EventDataHudRemove::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudChangeDelegate),
        EventDataHudRemove::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetFlagsDelegate),
        EventDataHudRemove::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetParamDelegate),
        EventDataHudSetParam::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetSkyDelegate),
        EventDataHudSetSky::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetSunDelegate),
        EventDataHudSetSun::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HudSetMoonDelegate),
        EventDataHudSetMoon::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::SetCloudsDelegate),
        EventDataSetClouds::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::SetTimeOfDayDelegate),
        EventDataTimeOfDay::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ActiveObjectRemoveAddDelegate),
        EventDataActiveObjectRemoveAdd::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ActiveObjectMessagesDelegate),
        EventDataActiveObjectMessages::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ShowFormDelegate),
        EventDataShowForm::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::DeathScreenDelegate),
        EventDataDeathScreen::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::MovementDelegate),
        EventDataMovement::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerHPDelegate),
        EventDataPlayerHP::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerBreathDelegate),
        EventDataPlayerBreath::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerInventoryFormDelegate),
        EventDataPlayerInventoryForm::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerAnimationsDelegate),
        EventDataPlayerAnimations::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerRegainGroundDelegate),
        EventDataPlayerRegainGround::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerMoveDelegate),
        EventDataPlayerMove::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerJumpDelegate),
        EventDataPlayerJump::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::PlayerFallingDamageDelegate),
        EventDataPlayerFallingDamage::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandlePlaySoundAtDelegate),
        EventDataPlaySoundAt::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandlePlaySoundDelegate),
        EventDataPlaySoundType::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleStopSoundDelegate),
        EventDataStopSound::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleFadeSoundDelegate),
        EventDataFadeSound::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::SpawnParticleDelegate),
        EventDataSpawnParticle::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::AddParticleSpawnerDelegate),
        EventDataAddParticleSpawner::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::DeleteParticleSpawnerDelegate),
        EventDataDeleteParticleSpawner::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ViewBobbingStepDelegate),
        EventDataViewBobbingStep::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::CameraPunchLeftDelegate),
        EventDataCameraPunchLeft::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::CameraPunchRightDelegate),
        EventDataCameraPunchRight::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMapNodeRemoveDelegate),
        EventDataMapNodeRemove::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMapNodeAddDelegate),
        EventDataMapNodeAdd::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMapNodeDugDelegate),
        EventDataMapNodeDug::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ChangePasswordDelegate),
        EventDataChangePassword::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ChangeVolumeDelegate),
        EventDataChangePassword::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::ChangeMenuDelegate),
        EventDataChangePassword::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleBlockDataDelegate),
        EventDataHandleBlockData::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleNodeMetaChangedDelegate),
        EventDataHandleNodeMetaChanged::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleNodesDelegate),
        EventDataHandleNodes::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleItemsDelegate),
        EventDataHandleItems::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleInventoryDelegate),
        EventDataHandleInventory::skEventType);
    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleDetachedInventoryDelegate),
        EventDataHandleDetachedInventory::skEventType);

    pGlobalEventManager->RemoveListener(
        MakeDelegate(this, &MinecraftHumanView::HandleMediaDelegate),
        EventDataHandleMedia::skEventType);
}


void MinecraftHumanView::DropSelectedItem(bool singleItem)
{
    BaseDropAction* action = new BaseDropAction();
    action->count = singleItem ? 1 : 0;
    action->fromInventory.SetCurrentPlayer();
    action->fromList = "main";
    action->fromItem = mEnvironment->GetPlayer()->GetWieldIndex();

    // Send it to the logic
    std::ostringstream os(std::ios_base::binary);
    action->Serialize(os);

    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHandleInventoryAction>(
        mEnvironment->GetPlayer()->GetId(), os.str()));

    // Predict some local inventory changes
    action->Apply(mEnvironment.get(), mEnvironment.get());

    // Remove it
    delete action;
}

void MinecraftHumanView::OpenInventory()
{
    LogInformation("Game: Launching inventory");

    std::shared_ptr<PlayerInventoryFormSource> formSrc(
        new PlayerInventoryFormSource(mEnvironment->GetPlayer()));

    InventoryLocation inventoryloc;
    inventoryloc.SetCurrentPlayer();
    Inventory* inventory = mEnvironment->GetInventory(inventoryloc);

    if (!BaseGame::Get()->ModsLoaded() || !BaseGame::Get()->OnOpenInventory(inventory))
    {
        RectangleShape<2, int> menuRectangle;
        menuRectangle.mCenter = Vector2<int>{ 50, 50 };
        menuRectangle.mExtent = Vector2<int>{ 100, 100 };

        std::shared_ptr<TextDestination> textDst(new TextDestinationPlayerInventory());
        std::shared_ptr<BaseUIForm>& formUI = mGameUI->UpdateForm("");
        if (formUI)
        {
            formUI->SetFormPrepend(mEnvironment->GetPlayer()->mFormPrepend);
            formUI->SetFormSource(formSrc);
            formUI->SetTextDestination(textDst);
        }
        else
        {
            formUI.reset(new UIInventoryForm(mGameUI.get(), -1,
                menuRectangle, mTextureSrc.get(), formSrc, textDst, "",
                mEnvironment.get(), mEnvironment->GetItemManager(), inventoryloc));
            formUI->SetParent(mGameUI->GetRootUIElement());
            formUI->OnInit();
        }
    }
}

void MinecraftHumanView::OpenConsole(float scale, const wchar_t* line)
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


void MinecraftHumanView::MakeScreenshot()
{

}


void MinecraftHumanView::ToggleFreeMove()
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

void MinecraftHumanView::ToggleFreeMoveAlt()
{
    if (mGameSettings.doubletapJump && mRunData.jumpTimer < 0.2f)
        ToggleFreeMove();

    mRunData.resetJumpTimer = true;
}


void MinecraftHumanView::TogglePitchMove()
{
    bool pitchMove = !Settings::Get()->GetBool("pitch_move");
    Settings::Get()->Set("pitch_move", pitchMove ? "true" : "false");

    if (pitchMove)
        mGameUI->ShowTranslatedStatusText("Pitch move mode enabled");
    else
        mGameUI->ShowTranslatedStatusText("Pitch move mode disabled");
}


void MinecraftHumanView::ToggleFast()
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


void MinecraftHumanView::ToggleNoClip()
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

void MinecraftHumanView::ToggleCinematic()
{
    bool cinematic = !Settings::Get()->GetBool("cinematic");
    Settings::Get()->Set("cinematic", cinematic ? "true" : "false");

    if (cinematic)
        mGameUI->ShowTranslatedStatusText("Cinematic mode enabled");
    else
        mGameUI->ShowTranslatedStatusText("Cinematic mode disabled");
}

// Autoforward by toggling continuous forward.
void MinecraftHumanView::ToggleAutoforward()
{
    bool autorunEnabled = !Settings::Get()->GetBool("continuous_forward");
    Settings::Get()->Set("continuous_forward", autorunEnabled ? "true" : "false");

    if (autorunEnabled)
        mGameUI->ShowTranslatedStatusText("Automatic forward enabled");
    else
        mGameUI->ShowTranslatedStatusText("Automatic forward disabled");
}

void MinecraftHumanView::ToggleMinimap(bool shiftPressed)
{
    if (!mGameUI->mMinimap || !mGameUI->mFlags.showHud || !Settings::Get()->GetBool("enable_minimap"))
        return;

    if (shiftPressed)
        mGameUI->mMinimap->ToggleMinimapShape();
    else
        mGameUI->mMinimap->NextMode();

    // TODO: When legacy minimap is deprecated, keep only HUD minimap stuff here

    // Not so satisying code to keep compatibility with old fixed mode system
    // -->
    unsigned int hudFlags = mEnvironment->GetPlayer()->mHudFlags;

    if (hudFlags & HUD_FLAG_MINIMAP_VISIBLE) 
    {
        // If radar is disabled, try to find a non radar mode or fall back to 0
        if (!(hudFlags & HUD_FLAG_MINIMAP_RADAR_VISIBLE))
        {
            while (mGameUI->mMinimap->GetModeIndex() && mGameUI->mMinimap->GetMode().type == MINIMAP_TYPE_RADAR)
                mGameUI->mMinimap->NextMode();
        }

        mGameUI->mFlags.showMinimap = mGameUI->mMinimap->GetMode().type != MINIMAP_TYPE_OFF;
    }
    else
    {
        mGameUI->mFlags.showMinimap = false;
    }

    // <--
    // End of 'not so satifying code'
    if ((hudFlags & HUD_FLAG_MINIMAP_VISIBLE) ||
        (mGameUI->mHud && mGameUI->mHud->HasElementOfType(HUD_ELEM_MINIMAP)))
    {
        mGameUI->ShowStatusText(ToWideString(mGameUI->mMinimap->GetMode().label));
    }
    else
    {
        mGameUI->ShowTranslatedStatusText("Minimap currently disabled by game or mod");
    }
}

void MinecraftHumanView::ToggleFog()
{
    bool fogEnabled = Settings::Get()->GetBool("enable_fog");
    Settings::Get()->SetBool("enable_fog", !fogEnabled);
    if (fogEnabled)
        mGameUI->ShowTranslatedStatusText("Fog disabled");
    else
        mGameUI->ShowTranslatedStatusText("Fog enabled");
}


void MinecraftHumanView::ToggleDebug()
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
        mGameUI->mDrawControl->showWireframe = false;
        mGameUI->ShowTranslatedStatusText("Debug info shown");
    }
    else if (!mGameUI->mFlags.showProfilerGraph && !
            mGameUI->mDrawControl->showWireframe)
    {
        mGameUI->mFlags.showProfilerGraph = true;
        mGameUI->ShowTranslatedStatusText("Profiler graph shown");
    }
    else if (!mGameUI->mDrawControl->showWireframe && hasDebugPrivs)
    {
        mGameUI->mFlags.showProfilerGraph = false;
        mGameUI->mDrawControl->showWireframe = true;
        mGameUI->ShowTranslatedStatusText("Wireframe shown");
    }
    else 
    {
        mGameUI->mFlags.showDebug = false;
        mGameUI->mFlags.showProfilerGraph = false;
        mGameUI->mDrawControl->showWireframe = false;
        if (hasDebugPrivs)
            mGameUI->ShowTranslatedStatusText("Debug info, profiler graph, and wireframe hidden");
        else 
            mGameUI->ShowTranslatedStatusText("Debug info and profiler graph hidden");
    }
}


void MinecraftHumanView::ToggleUpdatePlayerCamera()
{
    mFlags.disableCameraUpdate = !mFlags.disableCameraUpdate;
    if (mFlags.disableCameraUpdate)
        mGameUI->ShowTranslatedStatusText("PlayerCamera update disabled");
    else
        mGameUI->ShowTranslatedStatusText("PlayerCamera update enabled");
}


void MinecraftHumanView::IncreaseViewRange()
{
    int16_t range = Settings::Get()->GetInt16("viewing_range");
    int16_t rangeNew = range + 10;

    wchar_t buf[255];
    const wchar_t *str;
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


void MinecraftHumanView::DecreaseViewRange()
{
    int16_t range = Settings::Get()->GetInt16("viewing_range");
    int16_t rangeNew = range - 10;

    wchar_t buf[255];
    const wchar_t *str;
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


void MinecraftHumanView::ToggleFullViewRange()
{
    mGameUI->mDrawControl->rangeAll = !mGameUI->mDrawControl->rangeAll;
    if (mGameUI->mDrawControl->rangeAll)
        mGameUI->ShowTranslatedStatusText("Enabled unlimited viewing range");
    else
        mGameUI->ShowTranslatedStatusText("Disabled unlimited viewing range");
}


void MinecraftHumanView::CheckZoomEnabled()
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    if (player->GetZoomFOV() < 0.001f || player->GetFov().fov > 0.0f)
        mGameUI->ShowTranslatedStatusText("Zoom currently disabled by game or mod");
}


void MinecraftHumanView::UpdatePlayerCameraDirection(PlayerCameraOrientation* cam, float dTime)
{
    if (System::Get()->IsWindowActive() && 
        System::Get()->IsWindowFocused() && 
        !mGameUI->IsMenuActive())
    {
        // Mac OSX gets upset if this is set every frame
        if (System::Get()->GetCursorControl()->IsVisible())
            System::Get()->GetCursorControl()->SetVisible(false);

        if (mFirstLoopAfterWindowActivation)
        {
            mFirstLoopAfterWindowActivation = false;

            System::Get()->GetCursorControl()->SetPosition(0.5f, 0.5f);
        }
        else 
        {
            UpdatePlayerCameraOrientation(cam, dTime);
        }

    }
    else 
    {
        // Mac OSX gets upset if this is set every frame
        if (!System::Get()->GetCursorControl()->IsVisible())
            System::Get()->GetCursorControl()->SetVisible(true);

        mFirstLoopAfterWindowActivation = true;
    }
}

// Get the factor to multiply with sensitivity to get the same mouse/joystick
// responsiveness independently of FOV.
float MinecraftHumanView::GetSensitivityScaleFactor() const
{
    float fovY = mPlayerCamera->GetFovY();

    // Multiply by a constant such that it becomes 1.0 at 72 degree FOV and
    // 16:9 aspect ratio to minimize disruption of existing sensitivity
    // settings.
    return tan(fovY / 2.0f) * 1.3763818698f;
}

void MinecraftHumanView::UpdatePlayerCameraOrientation(PlayerCameraOrientation* cam, float dTime)
{
    Vector2<unsigned int> center(Renderer::Get()->GetScreenSize() / (unsigned int)2);
    Vector2<unsigned int> cursorPos = System::Get()->GetCursorControl()->GetPosition();
    Vector2<int> dist = { (int)cursorPos[0] - (int)center[0], (int)cursorPos[1] - (int)center[1] };

    if (mInvertMouse || mPlayerCamera->GetCameraMode() == CAMERA_MODE_THIRD_FRONT)
        dist[1] = -dist[1];

    float sensScale = GetSensitivityScaleFactor();
    cam->cameraYaw += dist[0] * mGameSettings.mouseSensitivity * sensScale;
    cam->cameraPitch += dist[1] * mGameSettings.mouseSensitivity * sensScale;

    if (dist[0] != 0 || dist[1] != 0)
        System::Get()->GetCursorControl()->SetPosition(0.5f, 0.5f);

    cam->cameraPitch = std::clamp(cam->cameraPitch, -89.5f, 89.5f);
}


void MinecraftHumanView::UpdatePlayerControl(const PlayerCameraOrientation& cam)
{
    //TimeTaker tt("update player control", NULL, PRECISION_NANO);

    // DO NOT use the IsKeyDown method for the forward, backward, left, right
    // buttons, as the code that uses the controls needs to be able to
    // distinguish between the two in order to know when to use joysticks.

    PlayerControl control(
        IsKeyDown(KeyType::FORWARD),
        IsKeyDown(KeyType::BACKWARD),
        IsKeyDown(KeyType::LEFT),
        IsKeyDown(KeyType::RIGHT),
        IsKeyDown(KeyType::JUMP),
        IsKeyDown(KeyType::AUX1),
        IsKeyDown(KeyType::SNEAK),
        IsKeyDown(KeyType::ZOOM),
        IsKeyDown(KeyType::DIG),
        IsKeyDown(KeyType::PLACE),
        cam.cameraPitch,
        cam.cameraYaw);

    unsigned int keypressBits = (
        ((unsigned int)(IsKeyDown(KeyType::FORWARD) & 0x1) << 0) |
        ((unsigned int)(IsKeyDown(KeyType::BACKWARD) & 0x1) << 1) |
        ((unsigned int)(IsKeyDown(KeyType::LEFT) & 0x1) << 2) |
        ((unsigned int)(IsKeyDown(KeyType::RIGHT) & 0x1) << 3) |
        ((unsigned int)(IsKeyDown(KeyType::JUMP) & 0x1) << 4) |
        ((unsigned int)(IsKeyDown(KeyType::AUX1) & 0x1) << 5) |
        ((unsigned int)(IsKeyDown(KeyType::SNEAK) & 0x1) << 6) |
        ((unsigned int)(IsKeyDown(KeyType::DIG) & 0x1) << 7) |
        ((unsigned int)(IsKeyDown(KeyType::PLACE) & 0x1) << 8) |
        ((unsigned int)(IsKeyDown(KeyType::ZOOM) & 0x1) << 9));

    VisualPlayer* player = mEnvironment->GetPlayer();

    // autojump if set: simulate "jump" key
    if (player->GetAutoJump())
    {
        control.jump = true;
        keypressBits |= 1U << 4;
    }

    // autoforward if set: simulate "up" key
    if (player->GetPlayerSettings().continuousForward &&
        mActiveObjectsReceived && !player->IsDead()) 
    {
        control.up = true;
        keypressBits |= 1U << 0;
    }

    SetPlayerControl(control);
    player->mKeyPressed = keypressBits;

    //tt.stop();
}

/****************************************************************************/
/****************************************************************************
 Shutdown / cleanup
 ****************************************************************************/
 /****************************************************************************/

void MinecraftHumanView::ExtendedResourceCleanup()
{
    // Extended resource accounting
    LogInformation("Game resources after cleanup:");
    //LogInformation("\tRemaining meshes   : " + RenderingEngine::get_mesh_cache()->getMeshCount());
    //LogInformation("\tRemaining textures : " + driver->getTextureCount());
    /*
    for (unsigned int i = 0; i < driver->GetTextureCount(); i++) 
    {
        Texture2* texture = driver->GetTextureByIndex(i);
        LogInformation("\t\t" << i << ":" << texture->GetName().GetPath());
    }
    */
    ClearTextureNameCache();
    /*LogInformation("\tRemaining materials: " + driver->GetMaterialRendererCount() + 
        " (note: game engine doesn't support removing renderers)");*/
}

void MinecraftHumanView::Shutdown()
{
    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    if (formUI)
        formUI->QuitForm();
    
    bool cloudMenuBackground = Settings::Get()->GetBool("menu_clouds");
    if (cloudMenuBackground)
    {
        mClouds->Update(0);
        mClouds->PreRender(mScene.get());
        mClouds->Render(mScene.get());

        Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
        Renderer::Get()->ClearBuffers();
        mCloudMgr->OnRender();
    }
    else
    {
        Renderer::Get()->SetClearColor(SColor(255, 0, 0, 0));
        Renderer::Get()->ClearBuffers();
    }

    mGameUI->ShowOverlayMessage(L"Shutting down...", mTextureSrc, 0, false);

    Renderer::Get()->DisplayColorBuffer(0);

    /* cleanup menus */
    if (mGameUI->GetFormUI())
        mGameUI->GetFormUI()->SetVisible(false);
    mGameUI->DeleteFormUI();

    if (mEnvironment)
    {
        mEnvironment->Stop();

        while (!mEnvironment->IsShutdown())
        {
            LogAssert(mTextureSrc != NULL, "invalid texture source");
            LogAssert(mShaderSrc != NULL, "invalid shader source");
            mTextureSrc->ProcessQueue();
            mShaderSrc->ProcessQueue();
            Sleep(100);
        }
    }

    ExtendedResourceCleanup();
}

void MinecraftHumanView::ShowDeathForm()
{
    std::string formStr =
        "size[11,5.5]bgcolor[#320000b4;true]"
        "label[4.85,1.35;You died]"
        "button_exit[4,3;3,0.5;btn_respawn;Respawn]";

    /* Create menu */
    /* Note: FormSource and LocalFormHandler  *
     * are deleted by guiFormMenu                     */
    std::shared_ptr<FormSource> formSrc =  std::make_shared<FormSource>(formStr);
    std::shared_ptr<LocalFormHandler> textDst = std::make_shared<LocalFormHandler>(
        mEnvironment->GetPlayer()->GetId(), "bultin:death");

    InventoryLocation inventoryloc;
    inventoryloc.SetCurrentPlayer();

    RectangleShape<2, int> rectangle;
    rectangle.mCenter = Vector2<int>{ 50, 50 };
    rectangle.mExtent = Vector2<int>{ 100, 100 };

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    if (formUI)
    {
        formUI->SetFormPrepend(mEnvironment->GetPlayer()->mFormPrepend);
        formUI->SetFormSource(formSrc);
        formUI->SetTextDestination(textDst);
    }
    else
    {
        formUI.reset(new UIInventoryForm(mGameUI.get(), -1, rectangle,
            mTextureSrc.get(), formSrc, textDst, "", mEnvironment.get(),
            mEnvironment->GetItemManager(), inventoryloc));
        formUI->SetParent(mGameUI->GetRootUIElement());
        formUI->OnInit();
    }

    formUI->SetFocus("btn_respawn");
}


void MinecraftHumanView::ShowPauseMenu()
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

    float yPos = mSimpleSingleplayerMode ? 0.7f : 0.1f;
    std::ostringstream os;

    os << "form_version[1]" << SIZE_TAG
        << "button_exit[4," << (yPos++) << ";3,0.5;btn_continue;"
        << "Continue" << "]";

    if (!mSimpleSingleplayerMode) 
    {
        os << "button_exit[4," << (yPos++) << ";3,0.5;btn_change_password;"
            << "Change Password" << "]";
    }
    else 
    {
        os << "field[4.95,0;5,1.5;;" << "Game paused" << ";]";
    }

    if (Settings::Get()->GetBool("enable_sound")) 
    {
        os << "button_exit[4," << (yPos++) << ";3,0.5;btn_sound;"
            << "Sound Volume" << "]";
    }
    os << "button_exit[4," << (yPos++) << ";3,0.5;btn_key_config;"
        << "Change Keys" << "]";

    os << "button_exit[4," << (yPos++) << ";3,0.5;btn_exit_menu;"
        << "Exit to Menu" << "]";
    os << "button_exit[4," << (yPos++) << ";3,0.5;btn_exit_os;"
        << "Exit to OS" << "]"
        << "textarea[7.5,0.25;3.9,6.25;;" << controlText << ";]"
        << "textarea[0.4,0.25;3.9,6.25;;" << "Minecraft \n"
        << "\n" << "Game info:" << "\n";
    //const std::string& address = mEnvironment->GetAddressName();
    static const std::string mode = "- Mode: ";
    if (!mSimpleSingleplayerMode) 
    {
        /*
        Address serverAddress = client->getServerAddress();
        if (!address.empty()) 
        {
            os << mode << "Remote server" << "\n" << "- Address: " << address;
        }
        else 
        {
            os << mode << "Hosting server";
        }
        os << "\n" << "- Port: " << serverAddress.getPort() << "\n";
        */
    }
    else 
    {
        os << mode << "Singleplayer" << "\n";
    }
    if (mSimpleSingleplayerMode) 
    {
        static const std::string on = "On";
        static const std::string off = "Off";
        const std::string& damage = Settings::Get()->GetBool("enable_damage") ? on : off;
        const std::string& creative = Settings::Get()->GetBool("creative_mode") ? on : off;
        const std::string& announced = Settings::Get()->GetBool("server_announce") ? on : off;
        os << "- Damage: " << damage << "\n" << "- Creative Mode: " << creative << "\n";
        
        if (!mSimpleSingleplayerMode) 
        {
            const std::string& pvp = Settings::Get()->GetBool("enable_pvp") ? on : off;
            //~ PvP = Player versus Player
            os << "- PvP: " << pvp << "\n" << "- Public: " << announced << "\n";
            std::string serverName = Settings::Get()->Get("server_name");
            StringFormEscape(serverName);
            if (announced == on && !serverName.empty())
                os << "- Server Name: " << serverName;
        }
    }
    os << ";]";

    /* Create menu */
    /* Note: FormSource and LocalFormHandler  *
     * are deleted by FormMenu                     */
    std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(os.str());
    std::shared_ptr<LocalFormHandler> textDst = std::make_shared<LocalFormHandler>(
        mEnvironment->GetPlayer()->GetId(), "MT_PAUSE_MENU");

    InventoryLocation inventoryloc;
    inventoryloc.SetCurrentPlayer();

    RectangleShape<2, int> rectangle;
    rectangle.mCenter = Vector2<int>{ 50, 50 };
    rectangle.mExtent = Vector2<int>{ 100, 100 };

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    if (formUI)
    {
        formUI->SetFormPrepend(mEnvironment->GetPlayer()->mFormPrepend);
        formUI->SetFormSource(formSrc);
        formUI->SetTextDestination(textDst);
    }
    else
    {
        formUI.reset(new UIInventoryForm(mGameUI.get(), -1, rectangle,
            mTextureSrc.get(), formSrc, textDst, "", mEnvironment.get(),
            mEnvironment->GetItemManager(), inventoryloc));
        formUI->SetParent(mGameUI->GetRootUIElement());
        formUI->OnInit();
    }

    formUI->SetFocus("btn_continue");

    if (mSimpleSingleplayerMode)
        PauseAnimation();
}


VisualEvent* MinecraftHumanView::GetVisualEvent()
{
    LogAssert(!mVisualEventQueue.empty(), "Cannot GetVisualEvent, queue is empty.");

    VisualEvent* evt = mVisualEventQueue.front();
    mVisualEventQueue.pop();
    return evt;
}

void MinecraftHumanView::ProcessVisualEvents(PlayerCameraOrientation* cam)
{
    while (HasVisualEvents())
    {
        std::unique_ptr<VisualEvent> evt(GetVisualEvent());
        LogAssert(evt->type < VE_MAX, "Invalid visual event type");
        const VisualEventHandler& evtHandler = mVisualEventHandler[evt->type];
        (this->*evtHandler.handler)(evt.get(), cam);
    }
}

const VisualEventHandler MinecraftHumanView::mVisualEventHandler[VE_MAX] = {
    {&MinecraftHumanView::HandleVisualEventNone},
    {&MinecraftHumanView::HandleVisualEventPlayerDamage},
    {&MinecraftHumanView::HandleVisualEventPlayerForceMove},
    {&MinecraftHumanView::HandleVisualEventDeathscreen},
    {&MinecraftHumanView::HandleVisualEventShowForm},
    {&MinecraftHumanView::HandleVisualEventShowLocalForm},
    {&MinecraftHumanView::HandleVisualEventHandleParticleEvent},
    {&MinecraftHumanView::HandleVisualEventHandleParticleEvent},
    {&MinecraftHumanView::HandleVisualEventHandleParticleEvent},
    {&MinecraftHumanView::HandleVisualEventHudAdd},
    {&MinecraftHumanView::HandleVisualEventHudRemove},
    {&MinecraftHumanView::HandleVisualEventHudChange},
    {&MinecraftHumanView::HandleVisualEventSetSky},
    {&MinecraftHumanView::HandleVisualEventSetSun},
    {&MinecraftHumanView::HandleVisualEventSetMoon},
    {&MinecraftHumanView::HandleVisualEventSetStars},
    {&MinecraftHumanView::HandleVisualEventOverrideDayNigthRatio},
    {&MinecraftHumanView::HandleVisualEventCloudParams}};

void MinecraftHumanView::HandleVisualEventNone(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    LogError("VisualEvent type None received");
}

void MinecraftHumanView::HandleVisualEventPlayerDamage(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    if (BaseGame::Get()->ModsLoaded())
        BaseGame::Get()->OnDamageTaken();

    // Damage flash and hurt tilt are not used at death
    if (mEnvironment->GetHP() > 0) 
    {
        VisualPlayer* player = mEnvironment->GetPlayer();

        float hpMax = player->GetVAO() ? 
            (float)player->GetVAO()->GetProperties().hpMax : (float)PLAYER_MAX_HP_DEFAULT;
        float damageRatio = evt->PlayerDamage.amount / hpMax;

        mRunData.damageFlash += 95.0f + 64.f * damageRatio;
        mRunData.damageFlash = std::min(mRunData.damageFlash, 127.0f);

        player->mHurtTiltTimer = 1.5f;
        player->mHurtTiltStrength = std::clamp(damageRatio * 5.0f, 1.0f, 4.0f);
    }

    // Play damage sound
    mSoundMaker->mSound->PlaySoundGlobal(SimpleSound("player_damage", 0.5), false);
}

void MinecraftHumanView::HandleVisualEventPlayerForceMove(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    cam->cameraYaw = evt->PlayerForceMove.yaw;
    cam->cameraPitch = evt->PlayerForceMove.pitch;
}

void MinecraftHumanView::HandleVisualEventDeathscreen(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    // If visual scripting is enabled, deathscreen is handled by CSM code in
    // builtin/client/init.script
    if (BaseGame::Get()->ModsLoaded())
        BaseGame::Get()->OnDeath();
    else
        ShowDeathForm();

    /* Handle visualization */
    VisualPlayer* player = mEnvironment->GetPlayer();
    mRunData.damageFlash = 0;
    player->mHurtTiltTimer = 0;
    player->mHurtTiltStrength = 0;
}

void MinecraftHumanView::HandleVisualEventShowForm(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    if (!evt->ShowForm.form->empty()) 
    {
        std::shared_ptr<FormSource> formSrc(new FormSource(*(evt->ShowForm.form)));
        std::shared_ptr<TextDestinationPlayerInventory> textDst(
            new TextDestinationPlayerInventory(*(evt->ShowForm.formName)));

        InventoryLocation inventoryloc;
        inventoryloc.SetCurrentPlayer();

        RectangleShape<2, int> rectangle;
        rectangle.mCenter = Vector2<int>{ 50, 50 };
        rectangle.mExtent = Vector2<int>{ 100, 100 };

        std::shared_ptr<BaseUIForm>& formUI = mGameUI->UpdateForm(*(evt->ShowForm.formName));
        if (formUI)
        {
            formUI->SetFormPrepend(mEnvironment->GetPlayer()->mFormPrepend);
            formUI->SetFormSource(formSrc);
            formUI->SetTextDestination(textDst);
        }
        else
        {
            formUI.reset(new UIInventoryForm(mGameUI.get(), -1, rectangle,
                mTextureSrc.get(), formSrc, textDst, mEnvironment->GetPlayer()->mFormPrepend,
                mEnvironment.get(), mEnvironment->GetItemManager(), inventoryloc));
            formUI->SetParent(mGameUI->GetRootUIElement());
            formUI->OnInit();
        }
    }
    else
    {
        std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
        if (formUI && (evt->ShowForm.formName->empty() ||
            *(evt->ShowForm.form) == mGameUI->GetFormName()))
            formUI->QuitForm();
    }

    delete evt->ShowForm.form;
    delete evt->ShowForm.formName;
}

void MinecraftHumanView::HandleVisualEventShowLocalForm(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    std::shared_ptr<FormSource> formSrc(new FormSource(*(evt->ShowForm.form)));
    std::shared_ptr<TextDestinationPlayerInventory> textDst(
        new TextDestinationPlayerInventory(*(evt->ShowForm.formName)));

    InventoryLocation inventoryloc;
    inventoryloc.SetCurrentPlayer();

    RectangleShape<2, int> rectangle;
    rectangle.mCenter = Vector2<int>{ 50, 50 };
    rectangle.mExtent = Vector2<int>{ 100, 100 };

    std::shared_ptr<BaseUIForm>& formUI = mGameUI->GetFormUI();
    if (formUI)
    {
        formUI->SetFormPrepend(mEnvironment->GetPlayer()->mFormPrepend);
        formUI->SetFormSource(formSrc);
        formUI->SetTextDestination(textDst);
    }
    else
    {
        formUI.reset(new UIInventoryForm(mGameUI.get(), -1, rectangle,
            mTextureSrc.get(), formSrc, textDst, mEnvironment->GetPlayer()->mFormPrepend,
            mEnvironment.get(), mEnvironment->GetItemManager(), inventoryloc));
        formUI->SetParent(mGameUI->GetRootUIElement());
        formUI->OnInit();
    }

    delete evt->ShowForm.form;
    delete evt->ShowForm.formName;
}

void MinecraftHumanView::HandleVisualEventHandleParticleEvent(VisualEvent *event, PlayerCameraOrientation* cam)
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    mParticleMgr->HandleParticleEvent(event, player);
}

void MinecraftHumanView::HandleVisualEventHudAdd(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    VisualPlayer* player = mEnvironment->GetPlayer();

    unsigned int id = evt->hudadd->id;
    // ignore if we already have a HUD with that ID
    auto i = mHudLogicToVisual.find(id);
    if (i != mHudLogicToVisual.end()) 
    {
        delete evt->hudadd;
        return;
    }

    HudElement* el = new HudElement;
    el->type = static_cast<HudElementType>(evt->hudadd->type);
    el->position = evt->hudadd->pos;
    el->name = evt->hudadd->name;
    el->scale = evt->hudadd->scale;
    el->text = evt->hudadd->text;
    el->number = evt->hudadd->number;
    el->item = evt->hudadd->item;
    el->direction = evt->hudadd->dir;
    el->align = evt->hudadd->align;
    el->offset = evt->hudadd->offset;
    el->worldPosition = evt->hudadd->worldPos;
    el->size = evt->hudadd->size;
    el->zIndex = evt->hudadd->zIndex;
    el->text2 = evt->hudadd->text2;
    mHudLogicToVisual[id] = player->AddHud(el);

    delete evt->hudadd;
}

void MinecraftHumanView::HandleVisualEventHudRemove(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    VisualPlayer* player = mEnvironment->GetPlayer();

    auto it = mHudLogicToVisual.find(evt->HudRemove.id);
    if (it != mHudLogicToVisual.end()) 
    {
        HudElement* el = player->RemoveHud(it->second);
        delete el;
        mHudLogicToVisual.erase(it);
    }

}

void MinecraftHumanView::HandleVisualEventHudChange(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    VisualPlayer* player = mEnvironment->GetPlayer();

    HudElement* el = nullptr;
    auto it = mHudLogicToVisual.find(evt->hudChange->id);
    if (it != mHudLogicToVisual.end()) 
        el = player->GetHud(it->second);

    if (el == nullptr) 
    {
        delete evt->hudChange;
        return;
    }

#define CASE_SET(statval, prop, dataprop) \
	case statval: \
		el->prop = evt->hudChange->dataprop; \
		break

    switch (evt->hudChange->stat) 
    {
        CASE_SET(HUD_STAT_POS, position, v2fData);

        CASE_SET(HUD_STAT_NAME, name, sData);

        CASE_SET(HUD_STAT_SCALE, scale, v2fData);

        CASE_SET(HUD_STAT_TEXT, text, sData);

        CASE_SET(HUD_STAT_NUMBER, number, data);

        CASE_SET(HUD_STAT_ITEM, item, data);

        CASE_SET(HUD_STAT_DIR, direction, data);

        CASE_SET(HUD_STAT_ALIGN, align, v2fData);

        CASE_SET(HUD_STAT_OFFSET, offset, v2fData);

        CASE_SET(HUD_STAT_WORLD_POS, worldPosition, v3fData);

        CASE_SET(HUD_STAT_SIZE, size, v2sData);

        CASE_SET(HUD_STAT_Z_INDEX, zIndex, data);

        CASE_SET(HUD_STAT_TEXT2, text2, sData);
    }

#undef CASE_SET

    delete evt->hudChange;
}

void MinecraftHumanView::HandleVisualEventSetSky(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    mSky->SetVisible(false);
    // Whether clouds are visible in front of a custom skybox.
    mSky->SetCloudsEnabled(evt->setSky->clouds);

    if (mSkybox) 
    {
        mSkybox->DetachParent();
        mSkybox = NULL;
    }
    // Clear the old textures out in case we switch rendering type.
    mSky->ClearSkyboxTextures();
    // Handle according to type
    if (evt->setSky->type == "regular") 
    {
        // Shows the mesh skybox
        mSky->SetVisible(true);
        // Update mesh based skybox colours if applicable.
        mSky->SetSkyColors(evt->setSky->skyColor);
        mSky->SetHorizonTint(
            evt->setSky->fogSunTint,
            evt->setSky->fogMoonTint,
            evt->setSky->fogTintType);
    }
    else if (evt->setSky->type == "skybox" && evt->setSky->textures.size() == 6) 
    {
        // Disable the dyanmic mesh skybox:
        mSky->SetVisible(false);
        // Set fog colors:
        mSky->SetFallbackBGColor(evt->setSky->bgcolor);
        // Set sunrise and sunset fog tinting:
        mSky->SetHorizonTint(
            evt->setSky->fogSunTint,
            evt->setSky->fogMoonTint,
            evt->setSky->fogTintType);
        // Add textures to skybox.
        for (int i = 0; i < 6; i++)
            mSky->AddTextureToSkybox(evt->setSky->textures[i], i, mTextureSrc.get());
    }
    else 
    {
        // Handle everything else as plain color.
        if (evt->setSky->type != "plain")
            LogWarning("Unknown sky type: " + evt->setSky->type);
        mSky->SetVisible(false);
        mSky->SetFallbackBGColor(evt->setSky->bgcolor);
        // Disable directional sun/moon tinting on plain or invalid skyboxes.
        mSky->SetHorizonTint(evt->setSky->bgcolor, evt->setSky->bgcolor, "custom");
    }
    delete evt->setSky;
}

void MinecraftHumanView::HandleVisualEventSetSun(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    mSky->SetSunVisible(evt->sunParams->visible);
    mSky->SetSunTexture(evt->sunParams->texture,
        evt->sunParams->toneMap, mTextureSrc.get());
    mSky->SetSunScale(evt->sunParams->scale);
    mSky->SetSunriseVisible(evt->sunParams->sunriseVisible);
    mSky->SetSunriseTexture(evt->sunParams->sunrise, mTextureSrc.get());
    delete evt->sunParams;
}

void MinecraftHumanView::HandleVisualEventSetMoon(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    mSky->SetMoonVisible(evt->moonParams->visible);
    mSky->SetMoonTexture(evt->moonParams->texture, evt->moonParams->toneMap, mTextureSrc.get());
    mSky->SetMoonScale(evt->moonParams->scale);
    delete evt->moonParams;
}

void MinecraftHumanView::HandleVisualEventSetStars(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    mSky->SetStarsVisible(evt->starParams->visible);
    mSky->SetStarCount(evt->starParams->count, false);
    mSky->SetStarColor(evt->starParams->starcolor);
    mSky->SetStarScale(evt->starParams->scale);
    delete evt->starParams;
}

void MinecraftHumanView::HandleVisualEventOverrideDayNigthRatio(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    mEnvironment->SetDayNightRatioOverride(
        evt->OverrideDayNightRatio.doOverride,
        (unsigned int)(evt->OverrideDayNightRatio.ratio * 1000.0f));
}

void MinecraftHumanView::HandleVisualEventCloudParams(VisualEvent* evt, PlayerCameraOrientation* cam)
{
    if (!mClouds)
        return;

    mClouds->SetDensity(evt->CloudParams.density);
    mClouds->SetColorBright(SColor(evt->CloudParams.colorBright));
    mClouds->SetColorAmbient(SColor(evt->CloudParams.colorAmbient));
    mClouds->SetHeight(evt->CloudParams.height);
    mClouds->SetThickness(evt->CloudParams.thickness);
    mClouds->SetSpeed(Vector2<float>{evt->CloudParams.speedX, evt->CloudParams.speedY});
}

void MinecraftHumanView::UpdatePlayerCamera(unsigned int busyTime, float dTime)
{
    VisualPlayer* player = mEnvironment->GetPlayer();

    /*
        For interaction pumRectanglePoses, get info about the held item
        - What item is it?
        - Is it a usable item?
        - Can it point to liquids?
    */
    ItemStack playeritem;
    {
        ItemStack selected, hand;
        playeritem = player->GetWieldedItem(&selected, &hand);
    }
    ToolCapabilities playerItemToolcap = 
        playeritem.GetToolCapabilities(mEnvironment->GetItemManager());

    Vector3<short> oldCameraOffset = mPlayerCamera->GetOffset();
    if (WasKeyDown(KeyType::CAMERA_MODE)) 
    {
        GenericVisualActiveObject* playerVAO = player->GetVAO();

        // If playerVAO not loaded, don't change camera
        if (!playerVAO)
            return;

        mPlayerCamera->ToggleCameraMode();

        // Make the player visible depending on camera mode.
        playerVAO->UpdateMeshCulling();
        playerVAO->SetChildrenVisible(mPlayerCamera->GetCameraMode() > CAMERA_MODE_FIRST);
    }

    float fullPunchInterval = playerItemToolcap.fullPunchInterval;
    float toolReloadRatio = mRunData.timeFromLastPunch / fullPunchInterval;

    toolReloadRatio = std::min(toolReloadRatio, 1.f);
    mPlayerCamera->Update(player, dTime, busyTime / 1000.0f, toolReloadRatio);
    mPlayerCamera->Step(dTime);

    Vector3<float> cameraPosition = mPlayerCamera->GetPosition();
    Vector3<float> cameraDirection = mPlayerCamera->GetDirection();
    float cameraFov = mPlayerCamera->GetFovMax();
    Vector3<short> cameraOffset = mPlayerCamera->GetOffset();

    mCameraOffsetChanged = (cameraOffset != oldCameraOffset);

    if (!mFlags.disableCameraUpdate) 
    {
        mEnvironment->GetVisualMap()->UpdateCamera(cameraPosition, cameraDirection, cameraFov, cameraOffset);
        if (mCameraOffsetChanged) 
        {
            mEnvironment->UpdateCameraOffset(cameraOffset);
            if (mClouds)
                mClouds->UpdateCameraOffset(cameraOffset);
        }
    }
}

void MinecraftHumanView::UpdateSound(float dTime)
{
    // Update sound listener
    Vector3<float> cameraOffset = Vector3<float>{
        mPlayerCamera->GetOffset()[0] * BS, 
        mPlayerCamera->GetOffset()[1] * BS, 
        mPlayerCamera->GetOffset()[2] * BS };
    mSoundMgr->UpdateListener(
        mPlayerCamera->GetCameraNode()->GetRelativeTransform().GetTranslation() + cameraOffset,
        Vector3<float>::Zero(), mPlayerCamera->GetDirection(), 
        HProject(mPlayerCamera->GetCameraNode()->Get()->GetUVector()));

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

    VisualPlayer* player = mEnvironment->GetPlayer();

    // Tell the sound maker whether to make footstep sounds
    mSoundMaker->mMakesFootstepSound = player->mMakesFootstepSound;

    //	Update sound maker
    if (player->mMakesFootstepSound)
        mSoundMaker->Step(dTime);

    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    MapNode node = map->GetNode(player->GetFootstepNodePosition());
    mSoundMaker->mPlayerStepSound = mEnvironment->GetNodeManager()->Get(node).soundFootstep;
}

void MinecraftHumanView::ProcessPlayerInteraction(float dTime, bool showHud, bool showDebug)
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    const Vector3<float> cameraDirection = mPlayerCamera->GetDirection();
    const Vector3<short> cameraOffset = mPlayerCamera->GetOffset();

    /*
        Calculate what block is the crosshair pointing to
    */
    ItemStack selectedItem, handItem;
    const ItemStack& toolItem = player->GetWieldedItem(&selectedItem, &handItem);

    const Item& selectedDefinition = selectedItem.GetDefinition(mEnvironment->GetItemManager());
    float d = GetToolRange(selectedDefinition, handItem.GetDefinition(mEnvironment->GetItemManager()));

    Line3<float> shootLine;
    switch (mPlayerCamera->GetCameraMode()) 
    {
        case CAMERA_MODE_FIRST:
            // Shoot from camera position, with bobbing
            shootLine.mStart = mPlayerCamera->GetPosition();
            break;
        case CAMERA_MODE_THIRD:
            // Shoot from player head, no bobbing
            shootLine.mStart = mPlayerCamera->GetHeadPosition();
            break;
        case CAMERA_MODE_THIRD_FRONT:
            shootLine.mStart = mPlayerCamera->GetHeadPosition();
            // prevent player pointing anything in front-view
            d = 0;
            break;
    }
    shootLine.mEnd = shootLine.mStart + cameraDirection * BS * d;

    PointedThing pointed = UpdatePointedThing(shootLine,
        selectedDefinition.liquidsPointable,
        !mRunData.btnDownForDig, cameraOffset);

    if (pointed != mRunData.pointedOld) 
    {
        LogInformation("Pointing at " + pointed.Dump());
        mGameUI->mHud->UpdateSelectionMesh(cameraOffset);
    }

    // Allow digging again if button is not pressed
    if (mRunData.diggingBlocked && !IsKeyDown(KeyType::DIG))
        mRunData.diggingBlocked = false;

    /*
        Stop digging when
        - releasing dig button
        - pointing away from node
    */
    if (mRunData.digging) 
    {
        if (WasKeyReleased(KeyType::DIG)) 
        {
            LogInformation("Dig button released (stopped digging)");
            mRunData.digging = false;
        }
        else if (pointed != mRunData.pointedOld)
        {
            if (pointed.type == POINTEDTHING_NODE && 
                mRunData.pointedOld.type == POINTEDTHING_NODE && 
                pointed.nodeUndersurface == mRunData.pointedOld.nodeUndersurface) 
            {
                // Still pointing to the same node, but a different face.
                // Don't reset.
            }
            else 
            {
                LogInformation("Pointing away from node (stopped digging)");
                mRunData.digging = false;
                mGameUI->mHud->UpdateSelectionMesh(cameraOffset);
            }
        }

        if (!mRunData.digging) 
        {
            Interact(INTERACT_STOP_DIGGING, mRunData.pointedOld);
            mEnvironment->SetCrack(-1, Vector3<short>::Zero());
            mRunData.digTime = 0.0;
        }
    }
    else if (mRunData.digInstantly && WasKeyReleased(KeyType::DIG)) 
    {
        // Remove e.g. torches faster when clicking instead of holding dig button
        mRunData.nodigDelayTimer = 0;
        mRunData.digInstantly = false;
    }

    if (!mRunData.digging && mRunData.btnDownForDig && !IsKeyDown(KeyType::DIG))
        mRunData.btnDownForDig = false;

    mRunData.punching = false;

    mSoundMaker->mPlayerLeftPunchSound.name = "";

    // Prepare for repeating, unless we're not supposed to
    if (IsKeyDown(KeyType::PLACE) && !Settings::Get()->GetBool("safe_dig_and_place"))
        mRunData.repeatPlaceTimer += dTime;
    else
        mRunData.repeatPlaceTimer = 0;

    if (selectedDefinition.usable && IsKeyDown(KeyType::DIG)) 
    {
        if (WasKeyPressed(KeyType::DIG) && (!BaseGame::Get()->ModsLoaded() ||
            BaseGame::Get()->OnUseItem(selectedItem, pointed)))
            Interact(INTERACT_USE, pointed);
    }
    else if (pointed.type == POINTEDTHING_NODE) 
    {
        HandlePointingAtNode(pointed, selectedItem, handItem, dTime);
    }
    else if (pointed.type == POINTEDTHING_OBJECT) 
    {
        Vector3<float> playerPosition = player->GetPosition();
        HandlePointingAtObject(pointed, toolItem, playerPosition, showDebug);
    }
    else if (IsKeyDown(KeyType::DIG)) 
    {
        // When button is held down in air, show continuous animation
        mRunData.punching = true;
        // Run callback even though item is not usable
        if (WasKeyPressed(KeyType::DIG) && BaseGame::Get()->ModsLoaded())
            BaseGame::Get()->OnUseItem(selectedItem, pointed);
    }
    else if (WasKeyPressed(KeyType::PLACE))
        HandlePointingAtNothing(selectedItem);

    mRunData.pointedOld = pointed;

    if (mRunData.punching || WasKeyPressed(KeyType::DIG))
        mPlayerCamera->SetDigging(0); // dig animation

    ClearWasKeyPressed();
    ClearWasKeyReleased();
    // Ensure DIG & PLACE are marked as handled
    WasKeyDown(KeyType::DIG);
    WasKeyDown(KeyType::PLACE);
}

PointedThing MinecraftHumanView::UpdatePointedThing(const Line3<float>& shootLine, 
    bool liquidsPointable, bool lookForObject, const Vector3<short>& cameraOffset)
{
    std::vector<BoundingBox<float>>* selectionBoxes = mGameUI->mHud->GetSelectionBoxes();
    selectionBoxes->clear();
    mGameUI->mHud->SetSelectedFaceNormal(Vector3<float>::Zero());
    static thread_local const bool showEntitySelectionBox = 
        Settings::Get()->GetBool("show_entity_selectionbox");

    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();

    mRunData.selectedObject = NULL;
    mGameUI->mHud->mPointingAtObject = false;

    RaycastState raycastState(shootLine, lookForObject, liquidsPointable);
    PointedThing result;
    mEnvironment->ContinueRaycast(&raycastState, &result);
    if (result.type == POINTEDTHING_OBJECT) 
    {
        mGameUI->mHud->mPointingAtObject = true;

        mRunData.selectedObject = mEnvironment->GetActiveObject(result.objectId);
        BoundingBox<float> selectionBox;
        if (showEntitySelectionBox && 
            mRunData.selectedObject->DoShowSelectionBox() &&
            mRunData.selectedObject->GetSelectionBox(&selectionBox)) 
        {
            Vector3<float> pos = mRunData.selectedObject->GetPosition();
            selectionBoxes->push_back(BoundingBox<float>(selectionBox));
            mGameUI->mHud->SetSelectionPosition(pos, cameraOffset);
        }
    }
    else if (result.type == POINTEDTHING_NODE) 
    {
        // Update selection boxes
        MapNode node = map->GetNode(result.nodeUndersurface);
        std::vector<BoundingBox<float>> boxes;
        node.GetSelectionBoxes(nodeMgr, &boxes, node.GetNeighbors(result.nodeUndersurface, map));

        float d = 0.002f * BS;
        for (std::vector<BoundingBox<float>>::const_iterator it = boxes.begin(); it != boxes.end(); ++it) 
        {
            BoundingBox<float> box = *it;
            box.mMinEdge -= Vector3<float>{d, d, d};
            box.mMaxEdge += Vector3<float>{d, d, d};
            selectionBoxes->push_back(box);
        }

        Vector3<float> nodeUndersurface = Vector3<float>{
            result.nodeUndersurface[0] * BS, result.nodeUndersurface[1] * BS, result.nodeUndersurface[2] * BS };
        mGameUI->mHud->SetSelectionPosition(nodeUndersurface, cameraOffset);
        mGameUI->mHud->SetSelectedFaceNormal(Vector3<float>{
            (float)result.intersectionNormal[0],
            (float)result.intersectionNormal[1],
            (float)result.intersectionNormal[2]});
    }

    // Update selection mesh light level and vertex colors
    if (!selectionBoxes->empty()) 
    {
        Vector3<float> pf = mGameUI->mHud->GetSelectionPosition();
        Vector3<short> p;
        p[0] = (short)((pf[0] + (pf[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[1] = (short)((pf[1] + (pf[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[2] = (short)((pf[2] + (pf[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        // Get selection mesh light level
        MapNode node = map->GetNode(p);
        unsigned short nodeLight = GetInteriorLight(node, -1, nodeMgr);
        unsigned short lightLevel = nodeLight;

        for (const Vector3<short>& dir : Face6D) 
        {
            node = map->GetNode(p + dir);
            nodeLight = GetInteriorLight(node, -1, nodeMgr);
            if (nodeLight > lightLevel)
                lightLevel = nodeLight;
        }

        unsigned int dayNightRatio = mEnvironment->GetDayNightRatio();
        SColor color;
        FinalColorBlend(&color, lightLevel, dayNightRatio);

        // Modify final color a bit with time
        unsigned int timer = Timer::GetRealTime() % 5000;
        float timerf = (float)(GE_C_PI * ((timer / 2500.f) - 0.5f));
        float sinR = 0.08f * std::sin(timerf);
        float sinG = 0.08f * std::sin(timerf + (float)GE_C_PI * 0.5f);
        float sinB = 0.08f * std::sin(timerf + (float)GE_C_PI);
        color.SetRed(std::clamp((int)std::round(color.GetRed() * (0.8f + sinR)), 0, 255));
        color.SetGreen(std::clamp((int)std::round(color.GetGreen() * (0.8f + sinG)), 0, 255));
        color.SetBlue(std::clamp((int)std::round(color.GetBlue() * (0.8f + sinB)), 0, 255));

        // Set mesh final color
        mGameUI->mHud->SetSelectionMeshColor(color);
    }
    return result;
}

void MinecraftHumanView::Interact(InteractAction action, const PointedThing& pointed)
{
    if (mGameState != BGS_RUNNING)
    {
        LogError("Canceled : game is not ready");
        return;
    }

    VisualPlayer* player = mEnvironment->GetPlayer();
    if (player == NULL)
        return;

    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    float cameraFov = map->GetCameraFov();
    float wantedRange = map->GetControl()->wantedRange;

    std::ostringstream tmpOs(std::ios::binary);
    pointed.Serialize(tmpOs);

    Vector3<int> playerPosition{
        (int)(player->GetPosition()[0] * 100),
        (int)(player->GetPosition()[1] * 100),
        (int)(player->GetPosition()[2] * 100) };
    Vector3<int> playerSpeed{
        (int)(player->GetSpeed()[0] * 100),
        (int)(player->GetSpeed()[1] * 100),
        (int)(player->GetSpeed()[2] * 100) };

    EventManager::Get()->QueueEvent(std::make_shared<EventDataInteract>(
        player->GetId(), player->mKeyPressed, action, (uint16_t)player->GetWieldIndex(), tmpOs.str(),
        playerPosition, playerSpeed, (short)(player->GetPitch() * 100), (short)(player->GetYaw() * 100),
        (uint8_t)(cameraFov * 80), (uint8_t)std::min(255, (int)(ceil(wantedRange / MAP_BLOCKSIZE)))));
}


void MinecraftHumanView::HandlePointingAtNothing(const ItemStack& playerItem)
{
    LogInformation("Attempted to place item while pointing at nothing");
    PointedThing fauxPointed;
    fauxPointed.type = POINTEDTHING_NOTHING;
    Interact(INTERACT_ACTIVATE, fauxPointed);
}


void MinecraftHumanView::HandlePointingAtNode(const PointedThing& pointed,
    const ItemStack& selectedItem, const ItemStack& handItem, float dTime)
{
    Vector3<short> nodePos = pointed.nodeUndersurface;
    Vector3<short> neighboumRectanglePos = pointed.nodeAbovesurface;

    /*
        Check information text of node
    */

    const bool hasInteractPrivs = true; //mEnvironment->CheckPrivilege("interact");
    if (mRunData.nodigDelayTimer <= 0.0 && IsKeyDown(KeyType::DIG) && 
        !mRunData.diggingBlocked && hasInteractPrivs)
    {
        HandleDigging(pointed, nodePos, selectedItem, handItem, dTime);
    }

    // This should be done after digging handling
    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    MapNodeMetadata* meta = map->GetMapNodeMetadata(nodePos);

    if (meta) 
    {
        mGameUI->SetInfoText(UnescapeTranslate(ToWideString(
            meta->GetString("infotext")), nullptr));
    }
    else 
    {
        MapNode node = map->GetNode(nodePos);

        if (mEnvironment->GetNodeManager()->Get(node).tile[0].name == "unknown_node.png")
        {
            mGameUI->SetInfoText(L"Unknown node: " +
                ToWideString(mEnvironment->GetNodeManager()->Get(node).name));
        }
    }

    if ((WasKeyPressed(KeyType::PLACE) || 
        mRunData.repeatPlaceTimer >= mGameSettings.repeatPlaceTime) && hasInteractPrivs)
    {
        mRunData.repeatPlaceTimer = 0;
        LogInformation("Place button pressed while looking at ground");

        // Placing animation (always shown for feedback)
        mPlayerCamera->SetDigging(1);

        mSoundMaker->mPlayerRightPunchSound = SimpleSound();

        // If the wielded item has node placement prediction,
        // make that happen
        // And also set the sound and send the interact
        // But first check for meta form and rightclickable
        auto& item = selectedItem.GetDefinition(mEnvironment->GetItemManager());
        bool placed = NodePlacement(item, selectedItem, nodePos, neighboumRectanglePos, pointed, meta);

        if (placed && BaseGame::Get()->ModsLoaded())
            BaseGame::Get()->OnPlaceNode(pointed, item);
    }
}

bool MinecraftHumanView::NodePlacement(
    const Item& selectedDefinition, const ItemStack& selectedItem, 
    const Vector3<short>& nodePos, const Vector3<short>& neighbourPos,
    const PointedThing& pointed, const MapNodeMetadata* meta)
{
    const auto& prediction = selectedDefinition.nodePlacementPrediction;

    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    bool isValidPosition;

    MapNode node = map->GetNode(nodePos, &isValidPosition);
    if (!isValidPosition) 
    {
        mSoundMaker->mPlayerRightPunchSound = selectedDefinition.soundPlaceFailed;
        return false;
    }

    // form in meta
    if (meta && !meta->GetString("formspec").empty() && !IsKeyDown(KeyType::SNEAK))
    {
        // on_rightclick callbacks are called anyway
        if (mEnvironment->GetNodeManager()->Get(map->GetNode(nodePos)).rightClickable)
            Interact(INTERACT_PLACE, pointed);

        LogInformation("Launching custom inventory view");

        RectangleShape<2, int> menuRectangle;
        menuRectangle.mCenter = Vector2<int>{ 50, 50 };
        menuRectangle.mExtent = Vector2<int>{ 100, 100 };

        InventoryLocation inventoryloc;
        inventoryloc.SetNodeMeta(nodePos);

        std::shared_ptr<NodeMetadataFormSource> formSrc(
            new NodeMetadataFormSource(mEnvironment->GetVisualMap(), nodePos));
        std::shared_ptr<TextDestination> textDst(new TextDestinationNodeMetadata(nodePos));

        std::shared_ptr<BaseUIForm>& formUI = mGameUI->UpdateForm("");
        if (formUI)
        {
            formUI->SetFormPrepend(mEnvironment->GetPlayer()->mFormPrepend);
            formUI->SetFormSource(formSrc);
            formUI->SetTextDestination(textDst);
        }
        else
        {
            formUI.reset(new UIInventoryForm(mGameUI.get(), -1,
                menuRectangle, mTextureSrc.get(), formSrc, textDst, "",
                mEnvironment.get(), mEnvironment->GetItemManager(), inventoryloc));
            formUI->SetParent(mGameUI->GetRootUIElement());
            formUI->OnInit();
        }

        formUI->SetForm(meta->GetString("formspec"));
        return false;
    }

    // on_rightclick callback
    if (prediction.empty() || 
        (mEnvironment->GetNodeManager()->Get(node).rightClickable && !IsKeyDown(KeyType::SNEAK)))
    {
        // Report to logic
        Interact(INTERACT_PLACE, pointed);
        return false;
    }

    LogInformation("Node placement prediction for " + selectedDefinition.name + " is " + prediction);
    Vector3<short> pos = neighbourPos;

    // Place inside node itself if buildable_to
    MapNode nodeUnder = map->GetNode(nodePos, &isValidPosition);
    if (isValidPosition) 
    {
        if (!mEnvironment->GetNodeManager()->Get(nodeUnder).buildableTo)
        {
            node = map->GetNode(pos, &isValidPosition);
            if (isValidPosition && !mEnvironment->GetNodeManager()->Get(node).buildableTo)
            {
                mSoundMaker->mPlayerRightPunchSound = selectedDefinition.soundPlaceFailed;
                // Report to logic
                Interact(INTERACT_PLACE, pointed);
                return false;
            }
        }
        else
        {
            pos = nodePos;
        }
    }

    // Find id of predicted node
    uint16_t id;
    bool found = mEnvironment->GetNodeManager()->GetId(prediction, id);
    if (!found) 
    {
        LogWarning("Node placement prediction failed for " +selectedDefinition.name + 
            " (places " + prediction + ") - Name not known");
        // Handle this as if prediction was empty
        // Report to logic
        Interact(INTERACT_PLACE, pointed);
        return false;
    }

    const ContentFeatures& predicted = mEnvironment->GetNodeManager()->Get(id);

    // Predict param2 for facedir and wallmounted nodes
    // Compare core.item_place_node() for what the logic does
    unsigned char param2 = 0;

    const unsigned char placeParam2 = selectedDefinition.placeParam2;

    if (placeParam2)
    {
        param2 = placeParam2;
    }
    else if (predicted.paramType2 == CPT2_WALLMOUNTED ||
        predicted.paramType2 == CPT2_COLORED_WALLMOUNTED)
    {
        Vector3<short> dir = nodePos - neighbourPos;

        if (abs(dir[1]) > std::max(abs(dir[0]), abs(dir[2])))
            param2 = dir[1] < 0 ? 1 : 0;
        else if (abs(dir[0]) > abs(dir[2]))
            param2 = dir[0] < 0 ? 3 : 2;
        else
            param2 = dir[2] < 0 ? 5 : 4;
    }
    else if (predicted.paramType2 == CPT2_FACEDIR ||
        predicted.paramType2 == CPT2_COLORED_FACEDIR)
    {
        Vector3<float> pos  = mEnvironment->GetPlayer()->GetPosition();
        Vector3<short> dir;
        dir[0] = (short)((pos[0] + (pos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        dir[1] = (short)((pos[1] + (pos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        dir[2] = (short)((pos[2] + (pos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        if (abs(dir[0]) > abs(dir[2]))
            param2 = dir[0] < 0 ? 3 : 1;
        else
            param2 = dir[2] < 0 ? 2 : 0;
    }

    // Check attachment if node is in group AttachedNode
    if (ItemGroupGet(predicted.groups, "AttachedNode") != 0) 
    {
        const static Vector3<short> wallmountedDirs[8] = {
            Vector3<short>{0, 1, 0},
            Vector3<short>{0, -1, 0},
            Vector3<short>{1, 0, 0},
            Vector3<short>{-1, 0, 0},
            Vector3<short>{0, 0, 1},
            Vector3<short>{0, 0, -1}};
        Vector3<short> pp;

        if (predicted.paramType2 == CPT2_WALLMOUNTED ||
            predicted.paramType2 == CPT2_COLORED_WALLMOUNTED)
            pp = pos + wallmountedDirs[param2];
        else
            pp = pos + Vector3<short>{0, -1, 0};

        if (!mEnvironment->GetNodeManager()->Get(map->GetNode(pp)).walkable)
        {
            mSoundMaker->mPlayerRightPunchSound = selectedDefinition.soundPlaceFailed;
            // Report to logic
            Interact(INTERACT_PLACE, pointed);
            return false;
        }
    }

    // Apply color
    if (!placeParam2 && (predicted.paramType2 == CPT2_COLOR || 
        predicted.paramType2 == CPT2_COLORED_FACEDIR || 
        predicted.paramType2 == CPT2_COLORED_WALLMOUNTED)) 
    {
        const auto& indexstr = selectedItem.metadata.GetString("palette_index", 0);
        if (!indexstr.empty()) 
        {
            int index = atoi(indexstr.c_str());
            if (predicted.paramType2 == CPT2_COLOR) 
            {
                param2 = index;
            }
            else if (predicted.paramType2 == CPT2_COLORED_WALLMOUNTED) 
            {
                // param2 = pure palette index + other
                param2 = (index & 0xf8) | (param2 & 0x07);
            }
            else if (predicted.paramType2 == CPT2_COLORED_FACEDIR) 
            {
                // param2 = pure palette index + other
                param2 = (index & 0xe0) | (param2 & 0x1f);
            }
        }
    }

    // Add node to visual map
    node = MapNode(id, 0, param2);

    try 
    {
        VisualPlayer* player = mEnvironment->GetPlayer();

        const bool hasNoClipPrivs = true; //mEnvironment->CheckPrivilege("noclip");

        // Dont place node when player would be inside new node
        // NOTE: This is to be eventually implemented by a mod as visual-side
        if (!mEnvironment->GetNodeManager()->Get(node).walkable || 
            Settings::Get()->GetBool("enable_build_where_you_stand") ||
            (hasNoClipPrivs && Settings::Get()->GetBool("noclip")) ||
            (mEnvironment->GetNodeManager()->Get(node).walkable &&
                neighbourPos != player->GetStandingNodePosition() + Vector3<short>{0, 1, 0} &&
                neighbourPos != player->GetStandingNodePosition() + Vector3<short>{0, 2, 0})) 
        {
            // This triggers the required mesh update too
            mEnvironment->AddNode(pos, node);
            // Report to logic
            Interact(INTERACT_PLACE, pointed);
            // A node is predicted, also play a sound
            mSoundMaker->mPlayerRightPunchSound = selectedDefinition.soundPlace;
            return true;
        }
        else 
        {
            mSoundMaker->mPlayerRightPunchSound = selectedDefinition.soundPlaceFailed;
            return false;
        }
    }
    catch (const InvalidPositionException&) 
    {
        LogWarning("Node placement prediction failed for " + selectedDefinition.name + 
            " (places " + prediction + ") - Position not loaded");
        mSoundMaker->mPlayerRightPunchSound = selectedDefinition.soundPlaceFailed;
        return false;
    }
}


void MinecraftHumanView::HandlePointingAtObject(const PointedThing& pointed,
    const ItemStack& toolItem, const Vector3<float>& playerPosition, bool showDebug)
{
    std::wstring infoText = UnescapeTranslate(
        ToWideString(mRunData.selectedObject->InfoText()), nullptr);

    if (showDebug) 
    {
        if (!infoText.empty())
            infoText += L"\n";

        infoText += ToWideString(mRunData.selectedObject->DebugInfoText());
    }

    mGameUI->SetInfoText(infoText);

    if (IsKeyDown(KeyType::DIG)) 
    {
        bool doPunch = false;
        bool doPunchDamage = false;

        if (mRunData.objectHitDelayTimer <= 0.0) 
        {
            doPunch = true;
            doPunchDamage = true;
            mRunData.objectHitDelayTimer = ObjectHitDelay;
        }

        if (WasKeyPressed(KeyType::DIG))
            doPunch = true;

        if (doPunch) 
        {
            LogInformation("Punched object");
            mRunData.punching = true;
        }

        if (doPunchDamage) 
        {
            // Report direct punch
            Vector3<float> objpos = mRunData.selectedObject->GetPosition();
            Vector3<float> dir = objpos - playerPosition;
            Normalize(dir);

            bool disableSend = mRunData.selectedObject->DirectReportPunch(
                dir, &toolItem, mRunData.timeFromLastPunch);
            mRunData.timeFromLastPunch = 0;

            if (!disableSend)
                Interact(INTERACT_START_DIGGING, pointed);
        }
    }
    else if (WasKeyDown(KeyType::PLACE)) 
    {
        LogInformation("Pressed place button while pointing at object");
        Interact(INTERACT_PLACE, pointed);  // place
    }
}


void MinecraftHumanView::HandleDigging(
    const PointedThing& pointed, const Vector3<short>& nodePos, 
    const ItemStack& selectedItem, const ItemStack& handItem, float dTime)
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
    MapNode node = mEnvironment->GetVisualMap()->GetNode(nodePos);

    // NOTE: Similar piece of code exists on the logic side for cheat detection.
    // Get digging parameters
    DigParams params = GetDigParams(
        mEnvironment->GetNodeManager()->Get(node).groups,
        &selectedItem.GetToolCapabilities(mEnvironment->GetItemManager()));

    // If can't dig, try hand
    if (!params.diggable) 
    {
        params = GetDigParams(mEnvironment->GetNodeManager()->Get(node).groups,
            &handItem.GetToolCapabilities(mEnvironment->GetItemManager()));
    }

    if (params.diggable) 
    {
        mRunData.digTimeComplete = params.time;

        if (mGameSettings.enableParticles)
        {
            const ContentFeatures& features = mEnvironment->GetNodeManager()->Get(node);
            mParticleMgr->AddNodeParticle(player, nodePos, node, features);
        }
    }
    else
    {
        // I guess nobody will wait for this long
        mRunData.digTimeComplete = 10000000.0;
    }

    if (!mRunData.digging) 
    {
        LogInformation("Started digging");
        mRunData.digInstantly = mRunData.digTimeComplete == 0;
        if (BaseGame::Get()->ModsLoaded() && BaseGame::Get()->OnPunchNode(nodePos, node))
            return;
        Interact(INTERACT_START_DIGGING, pointed);
        mRunData.digging = true;
        mRunData.btnDownForDig = true;
    }

    if (!mRunData.digInstantly) 
    {
        mRunData.digIndex = (unsigned short)
            (mCrackAnimationLength * mRunData.digTime / mRunData.digTimeComplete);
    }
    else 
    {
        // This is for e.g. torches
        mRunData.digIndex = mCrackAnimationLength;
    }

    SimpleSound soundDig = mEnvironment->GetNodeManager()->Get(node).soundDig;
    if (soundDig.Exists() && params.diggable) 
    {
        if (soundDig.name == "__group") 
        {
            if (!params.mainGroup.empty()) 
            {
                mSoundMaker->mPlayerLeftPunchSound.gain = 0.5;
                mSoundMaker->mPlayerLeftPunchSound.name = std::string("default_dig_") + ToLowerString(params.mainGroup);
            }
        }
        else mSoundMaker->mPlayerLeftPunchSound = soundDig;
    }

    // Don't show cracks if not diggable
    if (mRunData.digTimeComplete >= 100000.0) 
    {

    }
    else if (mRunData.digIndex < mCrackAnimationLength)
    {
        //TimeTaker timer("client.setTempMod");
        //infostream<<"digIndex="<<digIndex<<std::endl;
        mEnvironment->SetCrack(mRunData.digIndex, nodePos);
    }
    else 
    {
        LogInformation("Digging completed");
        mEnvironment->SetCrack(-1, Vector3<short>::Zero());

        mRunData.digTime = 0;
        mRunData.digging = false;
        // we successfully dug, now block it from repeating if we want to be safe
        if (Settings::Get()->GetBool("safe_dig_and_place"))
            mRunData.diggingBlocked = true;

        mRunData.nodigDelayTimer =
            mRunData.digTimeComplete / (float)mCrackAnimationLength;

        // We don't want a corresponding delay to very time consuming nodes
        // and nodes without digging time (e.g. torches) get a fixed delay.
        if (mRunData.nodigDelayTimer > 0.3f)
            mRunData.nodigDelayTimer = 0.3f;
        else if (mRunData.digInstantly)
            mRunData.nodigDelayTimer = 0.15f;

        bool isValidPosition;
        MapNode wasNode = map->GetNode(nodePos, &isValidPosition);
        if (isValidPosition) 
        {
            if (BaseGame::Get()->ModsLoaded() && BaseGame::Get()->OnFallNode(nodePos, wasNode))
                return;

            const ContentFeatures& content = mEnvironment->GetNodeManager()->Get(wasNode);
            if (content.nodeDigPrediction == "air") 
            {
                mEnvironment->RemoveNode(nodePos);
            }
            else if (!content.nodeDigPrediction.empty())
            {
                uint16_t id;
                bool found = mEnvironment->GetNodeManager()->GetId(content.nodeDigPrediction, id);
                if (found)
                    mEnvironment->AddNode(nodePos, id, true);
            }
            // implicit else: no prediction
        }

        Interact(INTERACT_DIGGING_COMPLETED, pointed);

        if (mGameSettings.enableParticles)
        {
            const ContentFeatures& features = mEnvironment->GetNodeManager()->Get(wasNode);
            mParticleMgr->AddDiggingParticles(player, nodePos, wasNode, features);
        }

        // Send event to trigger sound
        EventManager::Get()->TriggerEvent(std::make_shared<EventDataMapNodeDug>(nodePos, wasNode));
    }

    if (mRunData.digTimeComplete < 100000.0) 
    {
        mRunData.digTime += dTime;
    }
    else 
    {
        mRunData.digTime = 0;
        mEnvironment->SetCrack(-1, nodePos);
    }

    mPlayerCamera->SetDigging(0);  // Dig animation
}

void MinecraftHumanView::SetPlayerControl(PlayerControl& control)
{
    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");
    player->mControl = control;
}

// Returns true once after the inventory of the visual player
// has been updated from the logic.
bool MinecraftHumanView::UpdateWieldedItem()
{
    if (!mUpdateWieldedItem)
        return false;

    mUpdateWieldedItem = false;

    VisualPlayer* player = mEnvironment->GetPlayer();
    LogAssert(player, "invalid player");
    if (auto* list = player->mInventory.GetList("main"))
        list->SetModified(false);
    if (auto* list = player->mInventory.GetList("hand"))
        list->SetModified(false);

    return true;
}

void PauseNodeAnimation(PausedNodesList& paused, std::shared_ptr<Node> node) 
{
    if (!node)
        return;
    for (auto&& child : node->GetChildren())
        PauseNodeAnimation(paused, child);
    if (node->GetType() != NT_ANIMATED_MESH)
        return;
    auto animatedNode = std::static_pointer_cast<AnimatedObjectMeshNode>(node);
    float speed = animatedNode->GetAnimationSpeed();
    if (!speed)
        return;
    paused.push_back({ animatedNode, speed });
    animatedNode->SetAnimationSpeed(0.0f);
}

void MinecraftHumanView::PauseAnimation()
{
    PauseNodeAnimation(mPausedAnimatedNodes, mScene->GetRootNode());
}

void MinecraftHumanView::ResumeAnimation()
{
    for (auto &&pair : mPausedAnimatedNodes)
        pair.first->SetAnimationSpeed(pair.second);
    mPausedAnimatedNodes.clear();
}

bool MinecraftHumanView::InitSound()
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

    mSoundMaker = std::make_shared<SoundMaker>(mSoundMgr.get(), mEnvironment->GetNodeManager());
    if (!mSoundMaker)
    {
        LogError("Failed to Initialize OpenAL audio");
        return false;
    }

    //mSoundMaker->RegisterReceiver(mEventMgr);

    return true;
}