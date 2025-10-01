// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UICheckBox.h"

#include "Core/OS/OS.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"

//! constructor
UICheckBox::UICheckBox(BaseUI* ui, int id, RectangleShape<2, int> rectangle, bool checked)
: BaseUICheckBox(ui, id, rectangle), mUI(ui), mCheckTime(0), mPressed(false), mChecked(checked)
{
	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

        mBlendState = std::make_shared<BlendState>();
        mBlendState->mTarget[0].enable = true;
        mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

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

		mEffect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
	}
}

//! initialize checkbox
void UICheckBox::OnInit()
{
	// this element can be tabbed into
	SetTabStop(true);
	SetTabOrder(-1);
}

//! called if an event happened.
bool UICheckBox::OnEvent(const Event& evt)
{
	if (IsEnabled())
	{
		switch(evt.mEventType)
		{
			case ET_KEY_INPUT_EVENT:
				if (evt.mKeyInput.mPressedDown &&
					(evt.mKeyInput.mKey == KEY_RETURN || evt.mKeyInput.mKey == KEY_SPACE))
				{
					mPressed = true;
					return true;
				}
				else
				if (mPressed && evt.mKeyInput.mPressedDown && evt.mKeyInput.mKey == KEY_ESCAPE)
				{
					mPressed = false;
					return true;
				}
				else
				if (!evt.mKeyInput.mPressedDown && mPressed &&
					(evt.mKeyInput.mKey == KEY_RETURN || evt.mKeyInput.mKey == KEY_SPACE))
				{
					mPressed = false;
					if (mParent)
					{
						Event newEvent;
						newEvent.mEventType = ET_UI_EVENT;
						newEvent.mUIEvent.mCaller = this;
						newEvent.mUIEvent.mElement = 0;
						mChecked = !mChecked;
						newEvent.mUIEvent.mEventType = UIEVT_CHECKBOX_CHANGED;
						mParent->OnEvent(newEvent);
					}
					return true;
				}
				break;
			case ET_UI_EVENT:
				if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST)
				{
					if (evt.mUIEvent.mCaller == this)
						mPressed = false;
				}
				break;
			case ET_MOUSE_INPUT_EVENT:
				if (evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
				{
					mPressed = true;
					mCheckTime = Timer::GetTime();
					mUI->SetFocus(shared_from_this());
					return true;
				}
				else
				if (evt.mMouseInput.mEvent == MIE_LMOUSE_LEFT_UP)
				{
					bool wasPressed = mPressed;
					mUI->RemoveFocus(shared_from_this());
					mPressed = false;

					if (wasPressed && mParent)
					{
						if (!mAbsoluteRect.IsPointInside(Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}))
						{
							mPressed = false;
							return true;
						}

						Event newEvent;
						newEvent.mEventType = ET_UI_EVENT;
						newEvent.mUIEvent.mCaller = this;
						newEvent.mUIEvent.mElement = 0;
						mChecked = !mChecked;
						newEvent.mUIEvent.mEventType = UIEVT_CHECKBOX_CHANGED;
						mParent->OnEvent(newEvent);
					}

					return true;
				}
				break;
			default:
				break;
		}
	}

	return BaseUIElement::OnEvent(evt);
}


//! draws the element and its children
void UICheckBox::Draw()
{
	if (!IsVisible())
		return;

	std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	if (skin)
	{
		const int height = skin->GetSize(DS_CHECK_BOX_WIDTH);

		RectangleShape<2, int> checkRect(mAbsoluteRect);
		checkRect.mCenter[0] -= (checkRect.mExtent[0] / 2);
		checkRect.mCenter[0] += (height / 2);
		checkRect.mExtent[0] = height;
		checkRect.mExtent[1] = height;

		UIDefaultColor col = DC_GRAY_EDITABLE;
		if (IsEnabled())
			col = mPressed ? DC_FOCUSED_EDITABLE : DC_EDITABLE;

        Renderer* renderer = Renderer::Get();
        renderer->SetBlendState(mBlendState);

		if (mChecked)
		{
			skin->DrawIcon(shared_from_this(), DI_CHECKBOX_CHECKED, mVisual, checkRect,
				&mAbsoluteClippingRect, mCheckTime, Timer::GetTime(), false);
		}
		else
		{
			skin->DrawIcon(shared_from_this(), DI_CHECKBOX_UNCHECKED, mVisual, checkRect,
				&mAbsoluteClippingRect, mCheckTime, Timer::GetTime(), false);
		}
		if (mText.size())
		{
			checkRect = mAbsoluteRect;
			checkRect.mExtent[0] -= height + 5;
			checkRect.mCenter[0] += (int)round((height + 5) / 2.f);

			std::shared_ptr<BaseUIFont> font = skin->GetFont();
			if (font)
			{
				font->Draw(mText, checkRect,
					skin->GetColor(IsEnabled() ? DC_BUTTON_TEXT : DC_GRAY_TEXT), 
					false, true, &mAbsoluteRect);
			}
		}

        renderer->SetDefaultBlendState();
	}
	BaseUIElement::Draw();
}


//! set if box is checked
void UICheckBox::SetChecked(bool checked)
{
	mChecked = checked;
}


//! returns if box is checked
bool UICheckBox::IsChecked() const
{
	return mChecked;
}