//========================================================================
// MinecraftView.h : Game View Class
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

#ifndef MINECRAFTVIEW_H
#define MINECRAFTVIEW_H

#include "Data/CloudParams.h"

#include "Games/Environment/VisualEnvironment.h"

#include "Games/Map/MapNodeMetadata.h"
#include "Games/Forms/Menu/BaseMenu.h"
#include "Games/Forms/Death.h"

#include "Graphics/Shader.h"
#include "Graphics/Node.h"
#include "Graphics/Sky.h"
#include "Graphics/Hud.h"
#include "Graphics/WieldMesh.h"
#include "Graphics/ProfilerGraph.h"
#include "Graphics/Particles.h"
#include "Graphics/VisualEvent.h"

#include "Graphics/Map/Minimap.h"
#include "Graphics/Map/VisualMap.h"
#include "Graphics/Map/MapBlockMesh.h"

#include "Graphics/Drawing/Core.h"

#include "Graphics/GUI/UIVolumeChange.h"
#include "Graphics/GUI/UIPasswordChange.h"
#include "Graphics/GUI/UIKeyChange.h"
#include "Graphics/GUI/UIInventoryForm.h"

#include "Utils/PointedThing.h"

#include "Core/Utility/Chat.h"
#include "Core/Utility/EnrichedString.h"

#include "Core/Event/EventManager.h"

#include "Audio/Sound.h"

#include "Game/Level/Level.h"
#include "Game/View/HumanView.h"

#include "Graphic/UI/Element/UIForm.h"
#include "Graphic/UI/Element/UIChatConsole.h"

#include "MinecraftEvents.h"

class Node;
struct MapDrawControl;

class VisualEnvironment;

class PathingGraph;
class SoundProcess;
class BaseEventManager;

class MinecraftHumanView;


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

struct PlayerCameraOrientation
{
    float cameraYaw;    // "right/left"
    float cameraPitch;  // "up/down"
};

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

class MinecraftMainMenuUI : public BaseUI
{
protected:

    std::shared_ptr<BaseMenu> mMenu;
    std::shared_ptr<UIForm> mFormMenu;
    std::shared_ptr<FormSource> mFormSource;
    std::shared_ptr<TextDestination> mTextDestination;

public:
	MinecraftMainMenuUI();
	virtual ~MinecraftMainMenuUI();

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
    void UpdateMenuUI(std::wstring gameId);
    bool SetMenuUI(std::wstring id, std::wstring gameId);

    void ClearMenuUI(std::wstring id);
    bool SetGenericMenuUI(std::wstring id);
};


class MinecraftMainMenuView : public HumanView
{

public:

	MinecraftMainMenuView(); 
	~MinecraftMainMenuView();

	virtual bool OnMsgProc(const Event& evt);
	virtual void RenderText();	

    virtual void OnRender(double time, float elapsedTime);
    virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs);

    void OpenContentStoreDelegate(BaseEventDataPtr pEventData);
    void DeleteContentStoreDelegate(BaseEventDataPtr pEventData);
    void OpenGameSelectionDelegate(BaseEventDataPtr pEventData);
    void ChangeGameSelectionDelegate(BaseEventDataPtr pEventData);

protected:

    void UpdateCameraRotation(const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const;

    std::shared_ptr<CloudSystemNode> mMenuCloud;
	std::shared_ptr<MinecraftMainMenuUI> mMinecraftMainMenuUI;

private:

    void RegisterAllDelegates(void);
    void RemoveAllDelegates(void);

    MenuSoundFetcher mSoundFetcher;
    std::shared_ptr<BaseSoundManager> mSoundMgr = nullptr;
};


/*
 * This object intend to contain the core UI elements
 * It includes:
 *   - status texts
 *   - debug texts
 *   - chat texts
 *   - hud control and flags
 */

class MinecraftUI : public BaseUI
{
    friend class MinecraftHumanView;

public:
    MinecraftUI();
    virtual ~MinecraftUI();

    // Flags that can, or may, change during main game loop
    struct Flags
    {
        bool showChat = true;
        bool showHud = true;
        bool showMinimap = false;
        bool showDebug = true;
        bool showProfilerGraph = false;
    };

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

    void Update(const RunStats& stats,
        std::shared_ptr<MapDrawControl> drawControl, VisualEnvironment* visualEnv,
        const PlayerCameraOrientation& cam, const PointedThing& pointedOld,
        const std::shared_ptr<UIChatConsole> chatConsole, float dTime);

    void ShowMinimap(bool show);

    inline void SetInfoText(const std::wstring& str) { mInfoText = str; }
    inline void ClearInfoText() { mInfoText.clear(); }

    inline void ShowStatusText(const std::wstring& str)
    {
        mStatusText = str;
        mStatusTextTime = 0.0f;
    }
    void ShowTranslatedStatusText(const char *str);
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
        std::shared_ptr<BaseTextureSource> texSrc, 
        float dTime, int percent, bool drawClouds = true);

    std::shared_ptr<BaseUIForm>& UpdateForm(const std::string& formName)
    {
        mFormName = formName;
        return mForm;
    }

    const std::string& GetFormName() { return mFormName; }
    std::shared_ptr<BaseUIForm>& GetFormUI() { return mForm; }
    void DeleteFormUI();

private:

    bool IsMenuActive();
    void SetMenuActive(bool active);

    Flags mFlags;

    std::shared_ptr<DrawingCore> mDrawingCore;

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

    int mResetHWBufferCounter = 0;

    // Default: "". If other than "": Empty ShowForm packets will only
    // close the form when the formName matches
    std::string mFormName;
    std::shared_ptr<BaseUIForm> mForm;

    std::shared_ptr<UIChatConsole> mUIChatConsole = nullptr;
    std::shared_ptr<MapDrawControl> mDrawControl = nullptr;
    std::shared_ptr<Hud> mHud = nullptr;

    Minimap* mMinimap = nullptr;
    bool mMinimapDisabled = false;

    float mDamageFlash = 0;

    bool mIsMenuActive = false;
};



class PlayerInventoryFormSource : public BaseFormSource
{
public:
    PlayerInventoryFormSource(VisualPlayer* player) : mPlayer(player)
    {
    }

    void SetForm(const std::string& form)
    {

    }

    const std::string& GetForm() const
    {
        return mPlayer->mInventoryForm;
    }

protected:

    VisualPlayer* mPlayer;
};


struct TextDestinationPlayerInventory : public TextDestination
{
    TextDestinationPlayerInventory()
    {
        mFormName = "";
    }

    TextDestinationPlayerInventory(const std::string& formName)
    {
        mFormName = formName;
    }

    void GotText(const StringMap& fields)
    {
        size_t fieldsSize = fields.size();
        LogAssert(fieldsSize <= 0xFFFF, "Unsupported number of inventory fields");

        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataHandleInventoryFields>(mFormName, fields));
    }
};

/*
    Text input system
*/
struct TextDestinationNodeMetadata : public TextDestination
{
    TextDestinationNodeMetadata(Vector3<short> pos)
    {
        mPosition = pos;
    }
    // This is deprecated I guess? -celeron55
    void GotText(const std::wstring& text)
    {
        std::string ntext = ToString(text);
        LogInformation("Submitting 'text' field of node at (" + std::to_string(mPosition[0]) +
            "," + std::to_string(mPosition[1]) + "," + std::to_string(mPosition[2]) + "): " + ntext);
        StringMap fields;
        fields["text"] = ntext;

        size_t fieldsSize = fields.size();
        LogAssert(fieldsSize < 0xFFFF, "Unsupported number of nodemeta fields");

        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataHandleNodeMetaFields>(mPosition, "", fields));

    }
    void GotText(const StringMap& fields)
    {
        size_t fieldsSize = fields.size();
        LogAssert(fieldsSize < 0xFFFF, "Unsupported number of nodemeta fields");

        EventManager::Get()->QueueEvent(
            std::make_shared<EventDataHandleNodeMetaFields>(mPosition, "", fields));
    }

    Vector3<short> mPosition;
};


struct LocalFormHandler : public TextDestination
{
    ActorId mActorId;

    LocalFormHandler(ActorId id, const std::string& formName)
    {
        mActorId = id;
        mFormName = formName;
    }

    void GotText(const StringMap& fields)
    {
        if (mFormName == "MT_PAUSE_MENU")
        {
            if (fields.find("btn_sound") != fields.end())
            {
                std::shared_ptr<EventDataChangeVolume> pEvent(
                    new EventDataChangeVolume());
                BaseEventManager::Get()->TriggerEvent(pEvent);
                return;
            }

            if (fields.find("btn_key_config") != fields.end())
            {
                std::shared_ptr<EventDataChangeMenu> pEvent(
                    new EventDataChangeMenu());
                BaseEventManager::Get()->TriggerEvent(pEvent);
                return;
            }

            if (fields.find("btn_exit_menu") != fields.end())
            {
                //Disconnect();
                return;
            }

            if (fields.find("btn_exit_os") != fields.end())
            {
                //ExitToOS();
                return;
            }

            if (fields.find("btn_change_password") != fields.end())
            {
                BaseEventManager::Get()->TriggerEvent(
                    std::make_shared<EventDataChangePassword>());
                return;
            }

            return;
        }

        if (mFormName == "MT_DEATH_SCREEN")
        {
            EventManager::Get()->TriggerEvent(
                std::make_shared<EventDataPlayerRespawn>(mActorId));
            return;
        }

        if (mFormName == "bultin:death")
        {
            EventManager::Get()->TriggerEvent(
                std::make_shared<EventDataPlayerRespawn>(mActorId, "You died."));
            return;
        }
    }
};


/* Form update callback */

class NodeMetadataFormSource : public BaseFormSource
{
public:
    NodeMetadataFormSource(std::shared_ptr<VisualMap> map, Vector3<short> pos) :
        mMap(map), mPosition(pos)
    {

    }

    void SetForm(const std::string& form)
    {

    }

    const std::string& GetForm() const
    {
        static const std::string emptyString = "";
        MapNodeMetadata* meta = mMap->GetMapNodeMetadata(mPosition);

        if (!meta)
            return emptyString;

        return meta->GetString("formspec");
    }

    virtual std::string ResolveText(const std::string& str)
    {
        MapNodeMetadata* meta = mMap->GetMapNodeMetadata(mPosition);
        if (!meta)
            return str;

        return meta->ResolveString(str);
    }

    std::shared_ptr<VisualMap> mMap;
    Vector3<short> mPosition;
};


class SoundMaker
{
public:

    BaseSoundManager* mSound;
    const NodeManager* mNodeMgr;

    bool mMakesFootstepSound;
    float mPlayerStepTimer;
    float mPlayerJumpTimer;

    SimpleSound mPlayerStepSound;
    SimpleSound mPlayerLeftPunchSound;
    SimpleSound mPlayerRightPunchSound;

    SoundMaker(BaseSoundManager* sound, const NodeManager* nodeMgr) :
        mSound(sound), mNodeMgr(nodeMgr), mMakesFootstepSound(true),
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


#define SIZE_TAG "size[11,5.5,true]" // Fixed size on desktop

/****************************************************************************
 ****************************************************************************/

const float ObjectHitDelay = 0.2f;

struct VisualEventHandler
{
    void (MinecraftHumanView::*handler)(VisualEvent*, PlayerCameraOrientation*);
};

/****************************************************************************
 THE GAME
 ****************************************************************************/

using PausedNodesList = std::vector<std::pair<std::shared_ptr<AnimatedObjectMeshNode>, float>>;


/* 
    The reason the following structs are not anonymous structs within the
    class is that they are not used by the majority of member functions and
    many functions that do require objects of thse types do not modify them
    (so they can be passed as a const qualified parameter)
 */

struct GameRunData
{
    unsigned short digIndex;
    unsigned short newPlayerItem;
    PointedThing pointedOld;
    bool digging;
    bool punching;
    bool btnDownForDig;
    bool digInstantly;
    bool diggingBlocked;
    bool resetJumpTimer;
    float nodigDelayTimer;
    float digTime;
    float digTimeComplete;
    float repeatPlaceTimer;
    float objectHitDelayTimer;
    float timeFromLastPunch;
    VisualActiveObject* selectedObject;

    float jumpTimer;
    float damageFlash;
    float updateDrawListTimer;

    float fogRange;

    Vector3<float> updateDrawListLastCamDir;

    float timeOfDaySmooth;
};

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


class MinecraftHumanView : public HumanView
{

public:
    MinecraftHumanView();
    virtual ~MinecraftHumanView();

    virtual bool OnMsgProc(const Event& evt);
    virtual void RenderText();

    virtual void OnRender(double time, float elapsedTime);
    virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs);
    virtual void OnAttach(GameViewId vid, ActorId aid);

    virtual void SetControlledActor(ActorId actorId);
    virtual bool LoadGameDelegate(tinyxml2::XMLElement* pLevelData) override;

    // event delegates
    void GameUIUpdateDelegate(BaseEventDataPtr pEventData);
    void SetActorControllerDelegate(BaseEventDataPtr pEventData);

    void HudAddDelegate(BaseEventDataPtr pEventData);
    void HudRemoveDelegate(BaseEventDataPtr pEventData);
    void HudChangeDelegate(BaseEventDataPtr pEventData);
    void HudSetFlagsDelegate(BaseEventDataPtr pEventData);
    void HudSetParamDelegate(BaseEventDataPtr pEventData);
    void HudSetSkyDelegate(BaseEventDataPtr pEventData);
    void HudSetSunDelegate(BaseEventDataPtr pEventData);
    void HudSetMoonDelegate(BaseEventDataPtr pEventData);
    void HudSetStarsDelegate(BaseEventDataPtr pEventData);

    void SetCloudsDelegate(BaseEventDataPtr pEventData);
    void SetTimeOfDayDelegate(BaseEventDataPtr pEventData);
    void OverrideDayNightRatioDelegate(BaseEventDataPtr pEventData);

    void ActiveObjectRemoveAddDelegate(BaseEventDataPtr pEventData);
    void ActiveObjectMessagesDelegate(BaseEventDataPtr pEventData);

    void InitChatDelegate(BaseEventDataPtr pEventData);
    void UpdateChatDelegate(BaseEventDataPtr pEventData);

    void ShowFormDelegate(BaseEventDataPtr pEventData);
    void DeathScreenDelegate(BaseEventDataPtr pEventData);

    void MovementDelegate(BaseEventDataPtr pEventData);

    void PlayerHPDelegate(BaseEventDataPtr pEventData);
    void PlayerSpeedDelegate(BaseEventDataPtr pEventData);
    void PlayerBreathDelegate(BaseEventDataPtr pEventData);
    void PlayerInventoryFormDelegate(BaseEventDataPtr pEventData);
    void PlayerEyeOffsetDelegate(BaseEventDataPtr pEventData);
    void PlayerAnimationsDelegate(BaseEventDataPtr pEventData);
    void PlayerRegainGroundDelegate(BaseEventDataPtr pEventData);
    void PlayerMoveDelegate(BaseEventDataPtr pEventData);
    void PlayerJumpDelegate(BaseEventDataPtr pEventData);
    void PlayerFallingDamageDelegate(BaseEventDataPtr pEventData);
    
    void HandlePlaySoundAtDelegate(BaseEventDataPtr pEventData);
    void HandlePlaySoundDelegate(BaseEventDataPtr pEventData);
    void HandleStopSoundDelegate(BaseEventDataPtr pEventData);
    void HandleFadeSoundDelegate(BaseEventDataPtr pEventData);

    void SpawnParticleDelegate(BaseEventDataPtr pEventData);
    void AddParticleSpawnerDelegate(BaseEventDataPtr pEventData);
    void DeleteParticleSpawnerDelegate(BaseEventDataPtr pEventData);

    void ViewBobbingStepDelegate(BaseEventDataPtr pEventData);
    void CameraPunchLeftDelegate(BaseEventDataPtr pEventData);
    void CameraPunchRightDelegate(BaseEventDataPtr pEventData);

    void HandleMapNodeRemoveDelegate(BaseEventDataPtr pEventData);
    void HandleMapNodeAddDelegate(BaseEventDataPtr pEventData);
    void HandleMapNodeDugDelegate(BaseEventDataPtr pEventData);

    void ChangePasswordDelegate(BaseEventDataPtr pEventData);
    void ChangeVolumeDelegate(BaseEventDataPtr pEventData);
    void ChangeMenuDelegate(BaseEventDataPtr pEventData);

    void HandleBlockDataDelegate(BaseEventDataPtr pEventData);
    void HandleNodeMetaChangedDelegate(BaseEventDataPtr pEventData);
    void HandleNodesDelegate(BaseEventDataPtr pEventData);

    void HandleItemsDelegate(BaseEventDataPtr pEventData);
    void HandleInventoryDelegate(BaseEventDataPtr pEventData);
    void HandleDetachedInventoryDelegate(BaseEventDataPtr pEventData);

    void HandleMediaDelegate(BaseEventDataPtr pEventData);

protected:

    bool InitSound();

    void MakeScreenshot();

    void DropSelectedItem(bool singleItem = false);
    void OpenInventory();
    void OpenConsole(float scale, const wchar_t* line=NULL);
    void ToggleFreeMove();
    void ToggleFreeMoveAlt();
    void TogglePitchMove();
    void ToggleFast();
    void ToggleNoClip();
    void ToggleCinematic();
    void ToggleAutoforward();

    void ToggleMinimap(bool shiftPressed);
    void ToggleFog();
    void ToggleDebug();
    void ToggleUpdatePlayerCamera();

    void IncreaseViewRange();
    void DecreaseViewRange();
    void ToggleFullViewRange();
    void CheckZoomEnabled();

    float GetSensitivityScaleFactor() const;

    void UpdatePlayerCameraDirection(PlayerCameraOrientation* cam, float dTime);
    void UpdatePlayerCameraOrientation(PlayerCameraOrientation* cam, float dTime);
    void UpdatePlayerControl(const PlayerCameraOrientation& cam);

    // This is run by Thread and does the actual processing
    void Step(float dTime);

    void Shutdown();

    void ExtendedResourceCleanup();

    void Interact(InteractAction action, const PointedThing& pointed);

    void ProcessVisualEvents(PlayerCameraOrientation* cam);
    bool HasVisualEvents() const { return !mVisualEventQueue.empty(); }
    // Get event from queue. If queue is empty, it triggers an assertion failure.
    VisualEvent* GetVisualEvent();

    void ProcessPlayerInteraction(float dTime, bool showHud, bool showDebug);

    void UpdatePlayerCamera(unsigned int busyTime, float dTime);
    void UpdateSound(float dTime);

    PointedThing UpdatePointedThing(
        const Line3<float>& shootLine, bool liquidsPointable,
        bool lookForObject, const Vector3<short>& cameraOffset);

    bool NodePlacement(const Item& selectedDefinition, const ItemStack& selectedItem,
        const Vector3<short>& nodePos, const Vector3<short>& neighbourPos, 
        const PointedThing& pointed, const MapNodeMetadata* meta);

    void SendHP(ActorId actorId, uint16_t hp);
    void SendPlayerPosition();

    void HandlePointingAtNothing(const ItemStack& playerItem);
    void HandlePointingAtNode(const PointedThing& pointed,
        const ItemStack& selectedItem, const ItemStack& handItem, float dTime);
    void HandlePointingAtObject(const PointedThing& pointed,
        const ItemStack& toolItem, const Vector3<float>& playerPosition, bool showDebug);
    void HandleDigging(const PointedThing& pointed, const Vector3<short>& nodePos,
        const ItemStack& selectedItem, const ItemStack& handItem, float dTime);

    void SetPlayerControl(PlayerControl& control);

    static void SettingsChangedCallback(const std::string& settingName, void* data);

    // Returns true if the inventory of the visual player has been
    // updated from the logic. If it is true, it is set to false.
    bool UpdateWieldedItem();

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

    bool mShowUI;					// If true, it renders the UI control text
    DebugMode mDebugMode;
    std::string mGameplayText;

    std::shared_ptr<BaseWritableTextureSource> mTextureSrc = nullptr;
    std::shared_ptr<BaseWritableShaderSource> mShaderSrc = nullptr;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<BlendState> mBlendState;

    std::shared_ptr<MinecraftUI> mGameUI;

    std::shared_ptr<Node> mPlayer;
    std::shared_ptr<Node> mPlayerHead;
    std::shared_ptr<PlayerCamera> mPlayerCamera = nullptr;

    std::shared_ptr<CloudSystemNode> mClouds;
    std::shared_ptr<Scene> mCloudMgr;
    std::shared_ptr<Sky> mSky;

private:

    void UpdateCameraRotation(const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const;

    void ShowDeathForm();
    void ShowPauseMenu();

    void PauseAnimation();
    void ResumeAnimation();

    bool GetGameContent();
    void AfterContentReceived();
    
    void UpdateProfilers(const RunStats& stats, const FpsControl& updateTimes, float dTime);
    void UpdateStats(RunStats* stats, const FpsControl& updateTimes, float dTime);
    void UpdateInteractTimers(float dTime);

    void ProcessQueues();

    void ProcessItemSelection(unsigned short* newPlayerItem);
    void ProcessUserInput(float dTime);
    void ProcessKeyInput();

    void UpdateFrame(RunStats* stats, float dTime, const PlayerCameraOrientation& cam);

    // Insert a media file appropriately into the appropriate manager
    bool LoadMedia(const std::string& filePath, bool fromMediaPush = false);

    std::string GetModStoragePath() const;

    inline bool IsSinglePlayer() { return mSimpleSingleplayerMode; }

    void RegisterAllDelegates(void);
    void RemoveAllDelegates(void);

    // VisualEvent handlers
    void HandleVisualEventNone(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventPlayerDamage(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventPlayerForceMove(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventDeathscreen(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventShowForm(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventShowLocalForm(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventHandleParticleEvent(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventHudAdd(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventHudRemove(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventHudChange(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventSetSky(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventSetSun(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventSetMoon(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventSetStars(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventOverrideDayNigthRatio(VisualEvent* evt, PlayerCameraOrientation* cam);
    void HandleVisualEventCloudParams(VisualEvent* evt, PlayerCameraOrientation* cam);

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

    struct Flags
    {
        bool forceFogOff = false;
        bool disableCameraUpdate = false;
    };

    // Some timers
    float mAvgRTTTimer = 0.0f;
    float mPlayerPositionSendTimer = 0.0f;
    IntervalLimiter mMapTimerAndUnloadInterval;

    // Visual Environment
    std::unique_ptr<VisualEnvironment> mEnvironment;

    // Sounds
    GameSoundFetcher mSoundFetcher;
    std::shared_ptr<BaseSoundManager> mSoundMgr;
    std::shared_ptr<SoundMaker> mSoundMaker;

    ParticleManager* mParticleMgr = nullptr;

    bool mRegistrationConfirmationShown = false;

    bool mUpdateWieldedItem = false;
    Inventory* mInventoryFromLogic = nullptr;
    float mInventoryFromLogicAge = 0.0f;

    // The authentication methods we can use to enter sudo mode (=change password)
    unsigned int mSudoAuthMethods;

    // The seed returned by the logic is stored here
    uint64_t mMapSeed = 0;

    GameRunData mRunData;
    Flags mFlags;

    // Map logic hud ids to visual hud ids
    std::unordered_map<unsigned int, unsigned int> mHudLogicToVisual;

    /* 'cache'
       This class does take ownership/responsibily for cleaning up etc of any of
       these items (e.g. device)
    */
    bool* mKill;
    std::string* mErrorMessage;
    bool* mReconnectRequested;
    std::shared_ptr<Node> mSkybox;
    PausedNodesList mPausedAnimatedNodes;

    bool mSimpleSingleplayerMode;
    /* End 'cache' */

    // Pre-calculated values
    int mCrackAnimationLength;

    IntervalLimiter mProfilerInterval;

    RunStats mStats;
    PlayerCameraOrientation mCamViewTarget;
    PlayerCameraOrientation mCamView;
    FpsControl mUpdateTimes;
    float mDeltaTime; // in seconds

    Vector2<unsigned int> mScreenSize;

    GameSettings mGameSettings;

    bool mInvertMouse = false;
    bool mFirstLoopAfterWindowActivation = false;
    bool mCameraOffsetChanged = false;

    std::queue<VisualEvent*> mVisualEventQueue;
    static const VisualEventHandler mVisualEventHandler[VE_MAX];

    bool mItemReceived = false;
    bool mNodeReceived = false;
    bool mMediaReceived = false;

    bool mActiveObjectsReceived = false;

    // time_of_day speed approximation for old protocol
    bool mTimeOfDaySet = false;
    float mLastTimeOfDay = -1.0f;
    float mTimeOfDayUpdateTimer = 0.0f;

    // An interval for generally sending object positions and stuff
    float mRecommendedSendInterval = 0.1f;

    // Sounds
    float mRemoveSoundsCheckTimer = 0.0f;
    // Mapping from logic sound ids to our sound ids
    std::unordered_map<int, int> mSoundsLogicToVisual;
    // And the other way!
    std::unordered_map<int, int> mSoundsVisualToLogic;
    // Relation of visual id to object id
    std::unordered_map<int, uint16_t> mSoundsToObjects;

    // Visual modding
    std::vector<Mod> mMods;
    StringMap mModVfs;
};

#endif