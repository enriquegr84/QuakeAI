/*
 Minetest
 Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
 Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>
 Copyright (C) 2013 teddydestodes <derkomtur@schattengang.net>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 2.1 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "UIKeyChange.h"

#include "Graphic/UI/UIEngine.h"

#include "Core/Utility/Serialize.h"

#include "Application/Settings.h"
#include "Application/System/KeyEvent.h"

#define KMaxButtonPerColumns 12

enum
{
	GUI_ID_BACK_BUTTON = 101, 
    GUI_ID_ABORT_BUTTON, 
    GUI_ID_SCROLL_BAR,
	// buttons
	GUI_ID_KEY_FORWARD_BUTTON,
	GUI_ID_KEY_BACKWARD_BUTTON,
	GUI_ID_KEY_LEFT_BUTTON,
	GUI_ID_KEY_RIGHT_BUTTON,
	GUI_ID_KEY_AUX1_BUTTON,
	GUI_ID_KEY_FLY_BUTTON,
	GUI_ID_KEY_FAST_BUTTON,
	GUI_ID_KEY_JUMP_BUTTON,
	GUI_ID_KEY_NOCLIP_BUTTON,
	GUI_ID_KEY_PITCH_MOVE,
	GUI_ID_KEY_CHAT_BUTTON,
	GUI_ID_KEY_CMD_BUTTON,
	GUI_ID_KEY_CMD_LOCAL_BUTTON,
	GUI_ID_KEY_CONSOLE_BUTTON,
	GUI_ID_KEY_SNEAK_BUTTON,
	GUI_ID_KEY_DROP_BUTTON,
	GUI_ID_KEY_INVENTORY_BUTTON,
	GUI_ID_KEY_HOTBAR_PREV_BUTTON,
	GUI_ID_KEY_HOTBAR_NEXT_BUTTON,
	GUI_ID_KEY_MUTE_BUTTON,
	GUI_ID_KEY_DEC_VOLUME_BUTTON,
	GUI_ID_KEY_INC_VOLUME_BUTTON,
	GUI_ID_KEY_RANGE_BUTTON,
	GUI_ID_KEY_ZOOM_BUTTON,
	GUI_ID_KEY_CAMERA_BUTTON,
	GUI_ID_KEY_MINIMAP_BUTTON,
	GUI_ID_KEY_SCREENSHOT_BUTTON,
	GUI_ID_KEY_CHATLOG_BUTTON,
	GUI_ID_KEY_HUD_BUTTON,
	GUI_ID_KEY_FOG_BUTTON,
	GUI_ID_KEY_DEC_RANGE_BUTTON,
	GUI_ID_KEY_INC_RANGE_BUTTON,
	GUI_ID_KEY_AUTOFWD_BUTTON,
	// other
	GUI_ID_CB_AUX1_DESCENDS,
	GUI_ID_CB_DOUBLETAP_JUMP,
	GUI_ID_CB_AUTOJUMP,
};

UIKeyChange::UIKeyChange(BaseUI* ui, int id, RectangleShape<2, int> rectangle) :
    BaseUIForm(ui, id, rectangle)
{
	InitKeys();

	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    // Create a vertex buffer for a single triangle.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

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

    mEffect = std::make_shared<ColorEffect>(
		ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

UIKeyChange::~UIKeyChange()
{
	RemoveChildren(true);

	for (KeySetting* ks : mKeySettings)
		delete ks;

	mKeySettings.clear();
}

void UIKeyChange::RemoveChildren(bool cascade)
{
    BaseUIElement::RemoveChildren(cascade);

	mKeyUsedText = nullptr;
}

void UIKeyChange::RegenerateUI(Vector2<unsigned int> screenSize)
{
	RemoveChildren(true);

	const float s = Settings::Get()->GetFloat("gui_scaling");
    mDesiredRect = RectangleShape<2, int>();
    mDesiredRect.mExtent = Vector2<int>{ (int)(835 * s), (int)(430 * s) };
    mDesiredRect.mCenter = Vector2<int>{ (int)(screenSize[0] / 2), (int)(screenSize[1] / 2) };
    RecalculateAbsolutePosition(false);

    Vector2<int> topLeft{ 0, 0 };

	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(600 * s), (int)(40 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += topLeft + Vector2<int>{(int)(25 * s), (int)(3 * s)};

		const std::wstring text  = L"Keybindings. (If this menu screws up, remove stuff from minetest.conf)";
		mUI->AddStaticText(text.c_str(), rect, false, true, shared_from_this(), -1);
		//t->SetTextAlignment(UIA_CENTER, UIA_UPPERLEFT);
	}

	// Build buttons

    Vector2<int> offset{ (int)(25 * s), (int)(60 * s) };
	for(size_t i = 0; i < mKeySettings.size(); i++)
	{
		KeySetting *k = mKeySettings.at(i);
		{
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ (int)(150 * s), (int)(20 * s) };
            rect.mCenter = rect.mExtent / 2;
            rect.mCenter += topLeft + offset;

            mUI->AddStaticText(k->buttonName.c_str(), rect, false, true, shared_from_this(), -1);
		}

		{
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ (int)(100 * s), (int)(30 * s) };
            rect.mCenter = rect.mExtent / 2;
            rect.mCenter += topLeft + Vector2<int>{(int)(offset[0] + 150 * s), (int)(offset[1] - 5 * s)};

            const std::wstring text = ToWideString(k->key.Name());
            k->button = mUI->AddButton(rect, shared_from_this(), k->id, text.c_str());
		}
		if ((i + 1) % KMaxButtonPerColumns == 0) 
        {
			offset[0] += (int)(260 * s);
			offset[1] = (int)(60 * s);
		} 
        else offset += Vector2<int>{0, (int)(25 * s)};
	}

	{
		int optionX = offset[0];
        int optionY = offset[1] + (int)(5 * s);
		int optionW = (int)(180 * s);
		{
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ optionW, (int)(30 * s) };
            rect.mCenter = rect.mExtent / 2;
            rect.mCenter += topLeft + Vector2<int>{optionX, optionY};

            std::wstring text = L"\"Aux1\" = climb down";
            mUI->AddCheckBox(Settings::Get()->GetBool("aux1_descends"),
                rect, shared_from_this(), GUI_ID_CB_AUX1_DESCENDS, text.c_str());
		}
        offset += Vector2<int>{0, (int)(25 * s)};
	}

	{
        int optionX = offset[0];
        int optionY = offset[1] + (int)(5 * s);
        int optionW = (int)(280 * s);
        {
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ optionW, (int)(30 * s) };
            rect.mCenter = rect.mExtent / 2;
            rect.mCenter += topLeft + Vector2<int>{optionX, optionY};

            std::wstring text = L"Double tap \"jump\" to toggle fly";
            mUI->AddCheckBox(Settings::Get()->GetBool("doubletap_jump"),
                rect, shared_from_this(), GUI_ID_CB_DOUBLETAP_JUMP, text.c_str());
        }
        offset += Vector2<int>{0, (int)(25 * s)};
	}

	{
        int optionX = offset[0];
        int optionY = offset[1] + (int)(5 * s);
        int optionW = 280;
        {
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ optionW, (int)(30 * s) };
            rect.mCenter = rect.mExtent / 2;
            rect.mCenter += topLeft + Vector2<int>{optionX, optionY};

            std::wstring text = L"Automatic jumping";
            mUI->AddCheckBox(Settings::Get()->GetBool("autojump"),
                rect, shared_from_this(), GUI_ID_CB_AUTOJUMP, text.c_str());
        }
        offset += Vector2<int>{0, (int)(25 * s)};
	}

	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(100 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += topLeft + Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 2 - 105 * s),
            (int)(mDesiredRect.mExtent[1] - 40 * s)};

        std::wstring text = L"Save";
        mUI->AddButton(rect, shared_from_this(), GUI_ID_BACK_BUTTON, text.c_str());
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(100 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mCenter += topLeft + Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 2 + 5 * s),
            (int)(mDesiredRect.mExtent[1] - 40 * s)};

        std::wstring text = L"Cancel";
        mUI->AddButton(rect, shared_from_this(), GUI_ID_ABORT_BUTTON, text.c_str());
	}
}

void UIKeyChange::DrawForm()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	Renderer::Get()->SetBlendState(mBlendState);

    SColor bgColor(140, 0, 0, 0);
    skin->Draw2DRectangle(bgColor, mVisual, mAbsoluteRect, &mAbsoluteClippingRect);

	Renderer::Get()->SetDefaultBlendState();

    BaseUIElement::Draw();
}

bool UIKeyChange::AcceptInput()
{
	for (KeySetting* keySetting : mKeySettings) 
    {
        std::string defaultKey;
        try 
        {
            defaultKey = Settings::GetLayer(SL_DEFAULTS)->Get(keySetting->settingName);
        }
        catch (SettingNotFoundException&)
        {

        }

		if (keySetting->key.Sym() != defaultKey)
            Settings::Get()->Set(keySetting->settingName, keySetting->key.Sym());
		else
            Settings::Get()->Remove(keySetting->settingName);
	}

	{
        std::shared_ptr<BaseUIElement> el = GetElementFromId(GUI_ID_CB_AUX1_DESCENDS);
		if(el && el->GetType() == UIET_CHECK_BOX)
            Settings::Get()->SetBool("aux1_descends", 
				(std::static_pointer_cast<BaseUICheckBox>(el))->IsChecked());
	}
	{
        std::shared_ptr<BaseUIElement> el = GetElementFromId(GUI_ID_CB_DOUBLETAP_JUMP);
		if(el && el->GetType() == UIET_CHECK_BOX)
            Settings::Get()->SetBool("doubletap_jump", 
				(std::static_pointer_cast<BaseUICheckBox>(el))->IsChecked());
	}
	{
        std::shared_ptr<BaseUIElement> el = GetElementFromId(GUI_ID_CB_AUTOJUMP);
		if(el && el->GetType() == UIET_CHECK_BOX)
            Settings::Get()->SetBool("autojump", 
				(std::static_pointer_cast<BaseUICheckBox>(el))->IsChecked());
	}

	ClearKeyCache();

	//g_gamecallback->signalKeyConfigChange();

	return true;
}

bool UIKeyChange::ResetForm()
{
	if (mActiveKey) 
    {
		mActiveKey->button->SetText(ToWideString(mActiveKey->key.Name()).c_str());
		mActiveKey = nullptr;
	}
	return true;
}

bool UIKeyChange::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_KEY_INPUT_EVENT && mActiveKey && evt.mKeyInput.mPressedDown)
    {
		bool preferCharacter = mShiftDown;
		KeyAction ka(evt.mKeyInput, preferCharacter);

		if (evt.mKeyInput.mKey == KEY_DELETE)
			ka = KeyAction(""); // To erase key settings
		else if (evt.mKeyInput.mKey == KEY_ESCAPE)
			ka = mActiveKey->key; // Cancel

		bool shiftWentDown = false;
        if (!mShiftDown &&
            (evt.mKeyInput.mKey == KEY_SHIFT ||
			evt.mKeyInput.mKey == KEY_LSHIFT ||
			evt.mKeyInput.mKey == KEY_RSHIFT))
        {
            shiftWentDown = true;
        }

		// Display Key already in use message
		bool keyInUse = false;
		if (strcmp(ka.Sym(), "") != 0)
        {
			for (KeySetting* ks : mKeySettings) 
            {
				if (ks != mActiveKey && ks->key == ka) 
                {
					keyInUse = true;
					break;
				}
			}
		}

		if (keyInUse && !this->mKeyUsedText) 
        {
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ 600, 40 };
            rect.mCenter = rect.mExtent / 2;
            rect.mExtent += Vector2<int>{ 25, 30};

            const std::wstring text = L"Key already in use";
            this->mKeyUsedText = mUI->AddStaticText(text.c_str(), rect, false, true, shared_from_this(), -1);
		} 
        else if (!keyInUse && this->mKeyUsedText) 
        {
			this->mKeyUsedText->Remove();
			this->mKeyUsedText = nullptr;
		}

		// But go on
		{
			mActiveKey->key = ka;
			mActiveKey->button->SetText(ToWideString(ka.Name()).c_str());

			// Allow characters made with shift
			if (shiftWentDown)
            {
				mShiftDown = true;
				return false;
			}

			mActiveKey = nullptr;
			return true;
		}
	} 
    else if (evt.mEventType == ET_KEY_INPUT_EVENT && !mActiveKey &&
		evt.mKeyInput.mPressedDown && evt.mKeyInput.mKey == KEY_ESCAPE)
    {
		QuitForm();
		return true;
	} 
    else if (evt.mEventType == ET_UI_EVENT)
    {
		if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST && IsVisible())
		{
			if (!CanTakeFocus(evt.mUIEvent.mElement))
			{
				LogInformation("Not allowing focus change.");
				// Returning true disables focus change
				return true;
			}
		}
		if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
		{
			switch (evt.mUIEvent.mCaller->GetID())
			{
				case GUI_ID_BACK_BUTTON: //back
					AcceptInput();
					QuitForm();
					return true;
				case GUI_ID_ABORT_BUTTON: //abort
					QuitForm();
					return true;
				default:
					ResetForm();
					for (KeySetting* ks : mKeySettings) 
                    {
						if (ks->id == evt.mUIEvent.mCaller->GetID())
                        {
							mActiveKey = ks;
							break;
						}
					}
					LogAssert(!mActiveKey, "Key setting not found");

					mShiftDown = false;
					mActiveKey->button->SetText(L"press key");
					break;
			}
            mUI->SetFocus(shared_from_this());
		}
	}
	return mParent ? mParent->OnEvent(evt) : false;
}

void UIKeyChange::AddKey(int id, const wchar_t* buttonName, const std::string& settingName)
{
	KeySetting* k = new KeySetting;
	k->id = id;
	k->buttonName = buttonName;
	k->settingName = settingName;
	k->key = GetKeySetting(k->settingName.c_str());
	mKeySettings.push_back(k);
}

void UIKeyChange::InitKeys()
{
    AddKey(GUI_ID_KEY_FORWARD_BUTTON,   L"Forward",          "keymap_forward");
    AddKey(GUI_ID_KEY_BACKWARD_BUTTON,  L"Backward",         "keymap_backward");
    AddKey(GUI_ID_KEY_LEFT_BUTTON,      L"Left",             "keymap_left");
    AddKey(GUI_ID_KEY_RIGHT_BUTTON,     L"Right",            "keymap_right");
    AddKey(GUI_ID_KEY_AUX1_BUTTON,      L"Aux1",             "keymap_aux1");
    AddKey(GUI_ID_KEY_JUMP_BUTTON,      L"Jump",             "keymap_jump");
    AddKey(GUI_ID_KEY_SNEAK_BUTTON,     L"Sneak",            "keymap_sneak");
    AddKey(GUI_ID_KEY_DROP_BUTTON,      L"Drop",             "keymap_drop");
    AddKey(GUI_ID_KEY_INVENTORY_BUTTON, L"Inventory",        "keymap_inventory");
    AddKey(GUI_ID_KEY_HOTBAR_PREV_BUTTON,L"Prev. item",      "keymap_hotbar_previous");
    AddKey(GUI_ID_KEY_HOTBAR_NEXT_BUTTON,L"Next item",       "keymap_hotbar_next");
    AddKey(GUI_ID_KEY_ZOOM_BUTTON,      L"Zoom",             "keymap_zoom");
    AddKey(GUI_ID_KEY_CAMERA_BUTTON,    L"Change camera",    "keymap_camera_mode");
    AddKey(GUI_ID_KEY_MINIMAP_BUTTON,   L"Toggle minimap",   "keymap_minimap");
    AddKey(GUI_ID_KEY_FLY_BUTTON,       L"Toggle fly",       "keymap_freemove");
    AddKey(GUI_ID_KEY_PITCH_MOVE,       L"Toggle pitchmove", "keymap_pitchmove");
    AddKey(GUI_ID_KEY_FAST_BUTTON,      L"Toggle fast",      "keymap_fastmove");
    AddKey(GUI_ID_KEY_NOCLIP_BUTTON,    L"Toggle noclip",    "keymap_noclip");
    AddKey(GUI_ID_KEY_MUTE_BUTTON,      L"Mute",             "keymap_mute");
    AddKey(GUI_ID_KEY_DEC_VOLUME_BUTTON,L"Dec. volume",      "keymap_decrease_volume");
    AddKey(GUI_ID_KEY_INC_VOLUME_BUTTON,L"Inc. volume",      "keymap_increase_volume");
    AddKey(GUI_ID_KEY_AUTOFWD_BUTTON,   L"Autoforward",      "keymap_autoforward");
    AddKey(GUI_ID_KEY_CHAT_BUTTON,      L"Chat",             "keymap_chat");
    AddKey(GUI_ID_KEY_SCREENSHOT_BUTTON,L"Screenshot",       "keymap_screenshot");
    AddKey(GUI_ID_KEY_RANGE_BUTTON,     L"Range select",     "keymap_rangeselect");
    AddKey(GUI_ID_KEY_DEC_RANGE_BUTTON, L"Dec. range",       "keymap_decrease_viewing_range_min");
    AddKey(GUI_ID_KEY_INC_RANGE_BUTTON, L"Inc. range",       "keymap_increase_viewing_range_min");
    AddKey(GUI_ID_KEY_CONSOLE_BUTTON,   L"Console",          "keymap_console");
    AddKey(GUI_ID_KEY_CMD_BUTTON,       L"Command",          "keymap_cmd");
    AddKey(GUI_ID_KEY_CMD_LOCAL_BUTTON, L"Local command",    "keymap_cmd_local");
    AddKey(GUI_ID_KEY_HUD_BUTTON,       L"Toggle HUD",       "keymap_toggle_hud");
    AddKey(GUI_ID_KEY_CHATLOG_BUTTON,   L"Toggle chat log",  "keymap_toggle_chat");
    AddKey(GUI_ID_KEY_FOG_BUTTON,       L"Toggle fog",       "keymap_toggle_fog");
}
