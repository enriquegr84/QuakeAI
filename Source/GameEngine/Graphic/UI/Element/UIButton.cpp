// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIButton.h"
#include "UIImage.h"

#include "Core/OS/OS.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"

// Multiply with a color to get the default corresponding hovered color
#define COLOR_HOVERED_MOD 1.25f

// Multiply with a color to get the default corresponding pressed color
#define COLOR_PRESSED_MOD 0.85f

//! constructor
UIButton::UIButton(BaseUI* ui, int id, RectangleShape<2, int> rectangle)
:	BaseUIButton(id, rectangle), mSpriteBank(0), mOverrideFont(0), mUI(ui),
    mClickTime(0), mHoverTime(0), mFocusTime(0), mPushButton(false), 
	mPressed(false), mUseAlphaChannel(false), mDrawBorder(true), mScaleImage(false)
{
    //basic visual effect
    {
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

        std::shared_ptr<VisualEffect> effect = 
            std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

        // Create the geometric object for drawing.
        mVisualBackground = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

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

		mEffect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
	}
}


//! destructor
UIButton::~UIButton()
{

}

//! initialize button
void UIButton::OnInit(bool noclip, bool foregroundImage)
{
	SetNotClipped(noclip);

	// Initialize the sprites.
	for (unsigned int i=0; i<BS_COUNT; ++i)
		mButtonSprites[i].Index = -1;
	
	// This element can be tabbed.
	SetTabStop(true);
	SetTabOrder(-1);

    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    for (size_t i = 0; i < 4; i++)
        mColors[i] = skin->GetColor((UIDefaultColor)i);

    RectangleShape<2, int> rect;
    rect.mExtent = mAbsoluteRect.mExtent;
    rect.mCenter = rect.mExtent / 2;

    mStaticText = mUI->AddStaticText(mText.c_str(), rect, false, false, shared_from_this(), mID);
    mStaticText->SetTextAlignment(UIA_CENTER, UIA_CENTER);

    if (foregroundImage)
    {
        mForegroundImage = mUI->AddImage(rect, shared_from_this());
        mForegroundImage->SetScaleImage(IsScalingImage());
        SendToBack(mForegroundImage);
    }
}

//! Sets if the images should be scaled to fit the button
void UIButton::SetScaleImage(bool scaleImage)
{
	mScaleImage = scaleImage;
}


//! Returns whether the button scale the used images
bool UIButton::IsScalingImage() const
{
	return mScaleImage;
}

void UIButton::SetColor(SColor color)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();

    mBGColor = color;

    float d = 0.65f;
    for (size_t i = 0; i < 4; i++) 
    {
        SColor base = skin->GetColor((UIDefaultColor)i);
        mColors[i] = base.GetInterpolated(color, d);
    }
}

//! Sets if the button should use the skin to draw its border
void UIButton::SetDrawBorder(bool border)
{
	mDrawBorder = border;
}


void UIButton::SetSpriteBank(const std::shared_ptr<BaseUISpriteBank>& sprites)
{
	mSpriteBank = sprites;
}

void UIButton::SetSprite(UIButtonState state, int index, SColor color, bool loop, bool scale)
{
	mButtonSprites[(unsigned int)state].Index	= index;
	mButtonSprites[(unsigned int)state].Color	= color;
	mButtonSprites[(unsigned int)state].Loop	= loop;
    mButtonSprites[(unsigned int)state].Scale   = scale;
}


//! Get the sprite-index for the given state or -1 when no sprite is set
int UIButton::GetSpriteIndex(UIButtonState state) const
{
    return mButtonSprites[(unsigned int)state].Index;
}


//! Get the sprite color for the given state. Color is only used when a sprite is set.
SColor UIButton::GetSpriteColor(UIButtonState state) const
{
    return mButtonSprites[(unsigned int)state].Color;
}


//! Returns if the sprite in the given state does loop
bool UIButton::GetSpriteLoop(UIButtonState state) const
{
    return mButtonSprites[(unsigned int)state].Loop;
}


//! Returns if the sprite in the given state is scaled
bool UIButton::GetSpriteScale(UIButtonState state) const
{
    return mButtonSprites[(unsigned int)state].Scale;
}


//! called if an event happened.
bool UIButton::OnEvent(const Event& ev)
{
	if (!IsEnabled())
		return BaseUIElement::OnEvent(ev);

	switch(ev.mEventType)
	{
		case ET_KEY_INPUT_EVENT:
			if (ev.mKeyInput.mPressedDown &&
				(ev.mKeyInput.mKey == KEY_RETURN || ev.mKeyInput.mKey == KEY_SPACE))
			{
				if (!mPushButton)
					SetPressed(true);
				else
					SetPressed(!mPressed);

				return true;
			}
			if (mPressed && !mPushButton && 
				ev.mKeyInput.mPressedDown && ev.mKeyInput.mKey == KEY_ESCAPE)
			{
				SetPressed(false);
				return true;
			}
			else
			if (!ev.mKeyInput.mPressedDown && mPressed &&
				(ev.mKeyInput.mKey == KEY_RETURN || ev.mKeyInput.mKey == KEY_SPACE))
			{
				if (!mPushButton)
					SetPressed(false);

				if (mParent)
				{
                    mClickShiftState = ev.mKeyInput.mShift;
                    mClickControlState = ev.mKeyInput.mControl;

					Event newEvent;
					newEvent.mEventType = ET_UI_EVENT;
					newEvent.mUIEvent.mCaller = this;
					newEvent.mUIEvent.mElement = 0;
					newEvent.mUIEvent.mEventType = UIEVT_BUTTON_CLICKED;
					mParent->OnEvent(newEvent);
				}
				return true;
			}
			break;
		case ET_UI_EVENT:
			if (ev.mUIEvent.mCaller == this)
			{
				if (ev.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST)
				{
					if (!mPushButton)
						SetPressed(false);
					mFocusTime = Timer::GetTime();
				}
				else if (ev.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUSED)
				{
					mFocusTime = Timer::GetTime();
				}
				else if (ev.mUIEvent.mEventType == UIEVT_ELEMENT_HOVERED || 
						ev.mUIEvent.mEventType == UIEVT_ELEMENT_LEFT)
				{
					mHoverTime = Timer::GetTime();
				}
			}
			break;
		case ET_MOUSE_INPUT_EVENT:
			if (ev.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
			{
				if (mUI->HasFocus(shared_from_this()) &&
					!mAbsoluteRect.IsPointInside(Vector2<int>{ev.mMouseInput.X, ev.mMouseInput.Y}))
				{
					mUI->RemoveFocus(shared_from_this());
					return false;
				}

				if (!mPushButton)
					SetPressed(true);

				mUI->SetFocus(shared_from_this());
				return true;
			}
			else if (ev.mMouseInput.mEvent == MIE_LMOUSE_LEFT_UP)
			{
				bool wasPressed = mPressed;

				if (!mAbsoluteRect.IsPointInside(Vector2<int>{ev.mMouseInput.X, ev.mMouseInput.Y }))
				{
					if (!mPushButton)
						SetPressed(false);
					return true;
				}

				if (!mPushButton)
					SetPressed(false);
				else
					SetPressed(!mPressed);

				if ((!mPushButton && wasPressed && mParent) ||
					(mPushButton && wasPressed != mPressed))
				{
                    mClickShiftState = ev.mMouseInput.mShift;
                    mClickControlState = ev.mMouseInput.mControl;

					Event newEvent;
					newEvent.mEventType = ET_UI_EVENT;
					newEvent.mUIEvent.mCaller = this;
					newEvent.mUIEvent.mElement = 0;
					newEvent.mUIEvent.mEventType = UIEVT_BUTTON_CLICKED;
					mParent->OnEvent(newEvent);
				}

				return true;
			}
			break;
		default:
			break;
	}

	return mParent ? mParent->OnEvent(ev) : false;
}


//! draws the element and its children
void UIButton::Draw( )
{
	if (!mVisible)
		return;

    Renderer::Get()->SetBlendState(mBlendState);

    // Track hovered state, if it has changed then we need to update the style.
    bool hovered = IsHovered();
    if (hovered != mWasHovered) 
    {
        mWasHovered = hovered;
        SetFromState();
    }

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();

    if (mDrawBorder)
    {
        if (!mPressed)
        {
            skin->Draw3DButtonPaneStandard(
                mVisualBackground, mAbsoluteRect, &mAbsoluteClippingRect, mColors);
        }
        else
        {
            skin->Draw3DButtonPanePressed(
                mVisualBackground, mAbsoluteRect, &mAbsoluteClippingRect, mColors);
        }
    }

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);

    // The image changes based on the state, so we use the default every time.
    UIButtonImageState imageState = BIS_IMAGE_UP;
    if (mButtonImages[(unsigned int)imageState].Texture)
    {
        Vector2<int> pos = mAbsoluteRect.mCenter;
        RectangleShape<2, int> sourceRect(mButtonImages[(unsigned int)imageState].SourceRect);
        if (sourceRect.mCenter == Vector2<int>::Zero())
        {
            sourceRect = RectangleShape<2, int>();
            sourceRect.mExtent[0] = mButtonImages[(unsigned int)imageState].Texture->GetWidth();
            sourceRect.mExtent[1] = mButtonImages[(unsigned int)imageState].Texture->GetHeight();
        }
        
        pos[0] -= sourceRect.mExtent[0] / 2;
        pos[1] -= sourceRect.mExtent[1] / 2;

        if (mPressed)
        {
            // Create a pressed-down effect by moving the image when it looks identical to the unpressed state image
            UIButtonImageState unpressedState = GetImageState(false);
            if (unpressedState == imageState || 
                mButtonImages[(unsigned int)imageState] == mButtonImages[(unsigned int)unpressedState])
            {
                pos[0] += skin->GetSize(DS_BUTTON_PRESSED_IMAGE_OFFSET_X);
                pos[1] += skin->GetSize(DS_BUTTON_PRESSED_IMAGE_OFFSET_Y);
            }
        }

        effect->SetTexture(mButtonImages[(unsigned int)imageState].Texture);

        SColor imageColors[] = { mBGColor, mBGColor, mBGColor, mBGColor };
        if (mBGMiddle.GetArea()) 
        {
            RectangleShape<2, int > middle = mBGMiddle;
            // `-x` is interpreted as `w - x`
            if (middle.mExtent[0] < 0)
            {
                middle.mExtent[0] += effect->GetTexture()->GetDimension(0);
                middle.mCenter[0] += effect->GetTexture()->GetDimension(0) / 2;
            }
            if (middle.mExtent[1] < 0)
            {
                middle.mExtent[1] += effect->GetTexture()->GetDimension(1);
                middle.mCenter[1] += effect->GetTexture()->GetDimension(1) / 2;
            }

            skin->Draw2DTexture9Slice(mVisual, mScaleImage ? mAbsoluteRect : 
                RectangleShape<2, int>(pos, sourceRect.mAxis, sourceRect.mExtent), middle, imageColors);
        }
        else
        {
            skin->Draw2DTexture(mVisual, mScaleImage ? mAbsoluteRect : 
                RectangleShape<2, int>(pos, sourceRect.mAxis, sourceRect.mExtent), sourceRect, imageColors);
        }
    }


    if (mSpriteBank)
    {
        if (IsEnabled())
        {
            // pressed / unpressed animation
            UIButtonState state = mPressed ? BS_BUTTON_DOWN : BS_BUTTON_UP;
            if (mButtonSprites[state].Index != -1)
            {
                if (mButtonSprites[state].Scale)
                {
                    RectangleShape<2, int> spriteRect(mAbsoluteRect);
                    spriteRect.mCenter -= spriteRect.mExtent / 2;

                    mSpriteBank->Draw2DSprite(mButtonSprites[state].Index, mVisual, 
                        spriteRect, &mAbsoluteClippingRect, mButtonSprites[state].Color,
                        Timer::GetTime() - mClickTime, mButtonSprites[state].Loop);
                }
                else
                {
                    mSpriteBank->Draw2DSprite(mButtonSprites[state].Index, mVisual, mAbsoluteRect,
                        &mAbsoluteClippingRect, mButtonSprites[state].Color, mClickTime, Timer::GetTime(),
                        mButtonSprites[state].Loop, true);
                }
            }


            // focused / unfocused animation
            state = mUI->HasFocus(shared_from_this()) ? BS_BUTTON_FOCUSED : BS_BUTTON_NOT_FOCUSED;
            if (mButtonSprites[state].Index != -1)
            {
                if (mButtonSprites[state].Scale)
                {
                    RectangleShape<2, int> spriteRect(mAbsoluteRect);
                    spriteRect.mCenter -= spriteRect.mExtent / 2;

                    mSpriteBank->Draw2DSprite(mButtonSprites[state].Index, mVisual,
                        spriteRect, &mAbsoluteClippingRect, mButtonSprites[state].Color,
                        Timer::GetTime() - mFocusTime, mButtonSprites[state].Loop);
                }
                else
                {
                    mSpriteBank->Draw2DSprite(mButtonSprites[state].Index, mVisual, mAbsoluteRect,
                        &mAbsoluteClippingRect, mButtonSprites[state].Color, mFocusTime, Timer::GetTime(),
                        mButtonSprites[state].Loop, true);
                }
            }

            // mouse over / off animation
            state = IsHovered() ? BS_BUTTON_MOUSE_OVER : BS_BUTTON_MOUSE_OFF;
            if (mButtonSprites[state].Index != -1)
            {
                if (mButtonSprites[state].Scale)
                {
                    RectangleShape<2, int> spriteRect(mAbsoluteRect);
                    spriteRect.mCenter -= spriteRect.mExtent / 2;

                    mSpriteBank->Draw2DSprite(mButtonSprites[state].Index, mVisual,
                        spriteRect, &mAbsoluteClippingRect, mButtonSprites[state].Color,
                        Timer::GetTime() - mHoverTime, mButtonSprites[state].Loop);
                }
                else
                {
                    mSpriteBank->Draw2DSprite(mButtonSprites[state].Index, mVisual, mAbsoluteRect,
                        &mAbsoluteClippingRect, mButtonSprites[state].Color, mHoverTime, Timer::GetTime(),
                        mButtonSprites[state].Loop, true);
                }
            }
        }
        else
        {
            // draw disabled
//			drawSprite(EGBS_BUTTON_DISABLED, 0, pos);
        }
    }

    Renderer::Get()->SetDefaultBlendState();

	BaseUIElement::Draw();
}

UIButtonImageState UIButton::GetImageState(bool pressed)
{
    return GetImageState(pressed, mButtonImages);
}

UIButtonImageState UIButton::GetImageState(bool pressed, const ButtonImage* images)
{
    // figure state we should have
    UIButtonImageState state = BIS_IMAGE_DISABLED;
    bool focused = mUI->HasFocus(shared_from_this());
    bool mouseOver = IsHovered();
    if (IsEnabled())
    {
        if (pressed)
        {
            if (focused && mouseOver)
                state = BIS_IMAGE_DOWN_FOCUSED_MOUSEOVER;
            else if (focused)
                state = BIS_IMAGE_DOWN_FOCUSED;
            else if (mouseOver)
                state = BIS_IMAGE_DOWN_MOUSEOVER;
            else
                state = BIS_IMAGE_DOWN;
        }
        else // !pressed
        {
            if (focused && mouseOver)
                state = BIS_IMAGE_UP_FOCUSED_MOUSEOVER;
            else if (focused)
                state = BIS_IMAGE_UP_FOCUSED;
            else if (mouseOver)
                state = BIS_IMAGE_UP_MOUSEOVER;
            else
                state = BIS_IMAGE_UP;
        }
    }

    // find a compatible state that has images
    while (state != BIS_IMAGE_UP && !images[(unsigned int)state].Texture)
    {
        switch (state)
        {
        case BIS_IMAGE_UP_FOCUSED:
            state = BIS_IMAGE_UP;
            break;
        case BIS_IMAGE_UP_FOCUSED_MOUSEOVER:
            state = BIS_IMAGE_UP_FOCUSED;
            break;
        case BIS_IMAGE_DOWN_MOUSEOVER:
            state = BIS_IMAGE_DOWN;
            break;
        case BIS_IMAGE_DOWN_FOCUSED:
            state = BIS_IMAGE_DOWN;
            break;
        case BIS_IMAGE_DOWN_FOCUSED_MOUSEOVER:
            state = BIS_IMAGE_DOWN_FOCUSED;
            break;
        case BIS_IMAGE_DISABLED:
            if (pressed)
                state = BIS_IMAGE_DOWN;
            else
                state = BIS_IMAGE_UP;
            break;
        default:
            state = BIS_IMAGE_UP;
        }
    }

    return state;
}

//! sets another skin independent font. if this is set to zero, the button uses the font of the skin.
void UIButton::SetOverrideFont(const std::shared_ptr<BaseUIFont>& font)
{
	if (mOverrideFont == font)
		return;

	mOverrideFont = font;

    mStaticText->SetOverrideFont(font);
}

//! Gets the override font (if any)
const std::shared_ptr<BaseUIFont>& UIButton::GetOverrideFont() const
{
	return mOverrideFont;
}

//! Get the font which is used right now for drawing
std::shared_ptr<BaseUIFont> UIButton::GetActiveFont() const
{
	if ( mOverrideFont )
		return mOverrideFont;
	if (mUI->GetSkin())
		return mUI->GetSkin()->GetFont(DFFT_BUTTON);
	return std::shared_ptr<BaseUIFont>();
}


//! Sets another color for the text.
void UIButton::SetOverrideColor(SColor color)
{
    mOverrideColor = color;
    mOverrideColorEnabled = true;

    mStaticText->SetOverrideColor(color);
}

SColor UIButton::GetOverrideColor() const
{
    return mOverrideColor;
}

void UIButton::EnableOverrideColor(bool enable)
{
    mOverrideColorEnabled = enable;
}

bool UIButton::IsOverrideColorEnabled() const
{
    return mOverrideColorEnabled;
}

//! Sets an image which should be displayed on the button when it is in normal state.
void UIButton::SetImage(const std::shared_ptr<Texture2>& image)
{
    SetImage(BIS_IMAGE_UP, image);
}

//! Sets the image which should be displayed on the button when it is in its normal state.
void UIButton::SetImage(const std::shared_ptr<Texture2>& image, const RectangleShape<2, int>& sourceRect)
{
    SetImage(BIS_IMAGE_UP, image, sourceRect);
}

//! Sets the image which should be displayed on the button when it is in its normal state.
void UIButton::SetImage(UIButtonImageState state, 
    const std::shared_ptr<Texture2>& image, const RectangleShape<2, int>& sourceRect)
{
    mButtonImages[state].Texture = image;
    mButtonImages[state].SourceRect = sourceRect;
}

//! Sets an image which should be displayed on the button when it is in pressed state.
void UIButton::SetPressedImage(const std::shared_ptr<Texture2>& image)
{
    SetImage(BIS_IMAGE_DOWN, image);
}

//! Sets the image which should be displayed on the button when it is in its pressed state.
void UIButton::SetPressedImage(const std::shared_ptr<Texture2>& image, const RectangleShape<2, int>& sourceRect)
{
    SetImage(BIS_IMAGE_DOWN, image, sourceRect);
}

//! Sets a foreground image which should be displayed on the button.
void UIButton::SetForegroundImage(const std::shared_ptr<Texture2>& image)
{
    if (mForegroundImage)
        mForegroundImage->SetTexture(image);
}

//! Sets the text displayed by the button
void UIButton::SetText(const wchar_t* text)
{
    mStaticText->SetText(text);

    BaseUIButton::SetText(text);
}

//! Sets if the button should behave like a push button. Which means it
//! can be in two states: Normal or Pressed. With a click on the button,
//! the user can change the state of the button.
void UIButton::SetPushButton(bool pushButton)
{
	mPushButton = pushButton;
}

//! Returns if the button is currently pressed
bool UIButton::IsPressed() const
{
	return mPressed;
}

//! Returns if this element (or one of its direct children) is hovered
bool UIButton::IsHovered() const
{
    std::shared_ptr<BaseUIElement> hovered = mUI->GetHovered();
    return  hovered == shared_from_this() || 
        (hovered != nullptr && hovered->GetParent() == shared_from_this());
}

//! Sets the pressed state of the button if this is a pushbutton
void UIButton::SetPressed(bool pressed)
{
	if (mPressed != pressed)
	{
		mClickTime = Timer::GetTime();
		mPressed = pressed;
	}
}


//! Returns whether the button is a push button
bool UIButton::IsPushButton() const
{
	return mPushButton;
}


//! Sets if the alpha channel should be used for drawing images on the button (default is false)
void UIButton::SetUseAlphaChannel(bool useAlphaChannel)
{
	mUseAlphaChannel = useAlphaChannel;

	if (mUseAlphaChannel)
	{
        mBlendState->mTarget[0].enable = true;
        mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;
	}
	else mBlendState->mTarget[0] = BlendState::Target();

	Renderer::Get()->Unbind(mBlendState);
}


//! Returns if the alpha channel should be used for drawing images on the button
bool UIButton::IsAlphaChannelUsed() const
{
	return mUseAlphaChannel;
}


bool UIButton::IsDrawingBorder() const
{
	return mDrawBorder;
}


//! Set element properties from a style corresponding to the button state
void UIButton::SetFromState()
{
    Style::State state = Style::STATE_DEFAULT;

    if (IsPressed())
        state = static_cast<Style::State>(state | Style::STATE_PRESSED);

    if (IsHovered())
        state = static_cast<Style::State>(state | Style::STATE_HOVERED);

    SetFromStyle(Style::GetStyleFromStatePropagation(mStyles, state));
}

//! Set element properties from a style
void UIButton::SetFromStyle(const Style& style)
{
    bool hovered = (style.GetState() & Style::STATE_HOVERED) != 0;
    bool pressed = (style.GetState() & Style::STATE_PRESSED) != 0;

    if (style.IsNotDefault(Style::BGCOLOR)) 
    {
        SetColor(style.GetColor(Style::BGCOLOR));

        // If we have a propagated hover/press color, we need to automatically
        // lighten/darken it
        if (!mStyles[style.GetState()].IsNotDefault(Style::BGCOLOR)) 
        {
            if (pressed) 
            {
                mBGColor = SColor(mBGColor.GetAlpha(),
                    std::clamp<unsigned int>((uint32_t)(mBGColor.GetRed() * COLOR_PRESSED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(mBGColor.GetGreen() * COLOR_PRESSED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(mBGColor.GetBlue() * COLOR_PRESSED_MOD), 0, 255));

                for (size_t i = 0; i < 4; i++)
                {
                    mColors[i] = SColor(mBGColor.GetAlpha(),
                        std::clamp<unsigned int>((uint32_t)(mColors[i].GetRed() * COLOR_PRESSED_MOD), 0, 255),
                        std::clamp<unsigned int>((uint32_t)(mColors[i].GetGreen() * COLOR_PRESSED_MOD), 0, 255),
                        std::clamp<unsigned int>((uint32_t)(mColors[i].GetBlue() * COLOR_PRESSED_MOD), 0, 255));
                }
            }
            else if (hovered) 
            {
                mBGColor = SColor(mBGColor.GetAlpha(),
                    std::clamp<unsigned int>((uint32_t)(mBGColor.GetRed() * COLOR_HOVERED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(mBGColor.GetGreen() * COLOR_HOVERED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(mBGColor.GetBlue() * COLOR_HOVERED_MOD), 0, 255));

                for (size_t i = 0; i < 4; i++)
                {
                    mColors[i] = SColor(mBGColor.GetAlpha(),
                        std::clamp<unsigned int>((uint32_t)(mColors[i].GetRed() * COLOR_HOVERED_MOD), 0, 255),
                        std::clamp<unsigned int>((uint32_t)(mColors[i].GetGreen() * COLOR_HOVERED_MOD), 0, 255),
                        std::clamp<unsigned int>((uint32_t)(mColors[i].GetBlue() * COLOR_HOVERED_MOD), 0, 255));
                }
            }
        }
    }
    else 
    {
        mBGColor = SColor(255, 255, 255, 255);
        for (size_t i = 0; i < 4; i++) 
        {
            SColor base = mUI->GetSkin()->GetColor((UIDefaultColor)i);
            if (pressed) 
            {
                mColors[i] = SColor(base.GetAlpha(),
                    std::clamp<unsigned int>((uint32_t)(base.GetRed() * COLOR_PRESSED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(base.GetGreen() * COLOR_PRESSED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(base.GetBlue() * COLOR_PRESSED_MOD), 0, 255));
            }
            else if (hovered) 
            {
                mColors[i] = SColor(base.GetAlpha(),
                    std::clamp<unsigned int>((uint32_t)(base.GetRed() * COLOR_HOVERED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(base.GetGreen() * COLOR_HOVERED_MOD), 0, 255),
                    std::clamp<unsigned int>((uint32_t)(base.GetBlue() * COLOR_HOVERED_MOD), 0, 255));
            }
            else mColors[i] = base;
        }
    }

    if (style.IsNotDefault(Style::TEXTCOLOR))
    {
        SetOverrideColor(style.GetColor(Style::TEXTCOLOR));
    }
    else 
    {
        SetOverrideColor(SColor(255, 255, 255, 255));
        mOverrideColorEnabled = false;
    }
    SetNotClipped(style.GetBool(Style::NOCLIP, false));
    SetDrawBorder(style.GetBool(Style::BORDER, true));
    SetUseAlphaChannel(style.GetBool(Style::ALPHA, true));
    //SetOverrideFont(style.GetFont());

    if (style.IsNotDefault(Style::BGIMG)) 
    {
        /*
        SetTexture(UIScalingImageButton(
            Environment->getVideoDriver(), style.GetTexture(Style::BGIMG),
            AbsoluteRect.getWidth(), AbsoluteRect.getHeight()));
        */
        SetScaleImage(true);
    }
    else 
    {
        SetImage(nullptr);
    }

    mBGMiddle = style.GetRect(Style::BGIMG_MIDDLE, mBGMiddle);

    // Child padding and offset
    mPadding = style.GetRect(Style::PADDING, RectangleShape<2, int>());
    Vector2<int> upperLeftCorner =
        mPadding.GetVertice(RVP_UPPERLEFT) + mBGMiddle.GetVertice(RVP_UPPERLEFT);
    Vector2<int> lowerRightCorner =
        mPadding.GetVertice(RVP_LOWERRIGHT) + mBGMiddle.GetVertice(RVP_LOWERRIGHT);
    mPadding.mExtent = lowerRightCorner - upperLeftCorner;
    mPadding.mCenter = upperLeftCorner + mPadding.mExtent / 2;

    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    Vector2<int> defaultPressOffset{
        skin->GetSize(DS_BUTTON_PRESSED_IMAGE_OFFSET_X),
        skin->GetSize(DS_BUTTON_PRESSED_IMAGE_OFFSET_Y) };
    mContentOffset = style.GetVector(
        Style::CONTENT_OFFSET, IsPressed() ? defaultPressOffset : Vector2<int>::Zero());

    RectangleShape<2, int> childBounds;
    upperLeftCorner = mPadding.GetVertice(RVP_UPPERLEFT) + mContentOffset;
    lowerRightCorner = mPadding.GetVertice(RVP_LOWERRIGHT) + mAbsoluteRect.mExtent + mContentOffset;
    childBounds.mExtent = lowerRightCorner - upperLeftCorner;
    childBounds.mCenter = upperLeftCorner + childBounds.mExtent / 2;
    for (std::shared_ptr<BaseUIElement> child : GetChildren())
        child->SetRelativePosition(childBounds);

    if (mForegroundImage)
    {
        if (style.IsNotDefault(Style::FGIMG))
        {
            std::shared_ptr<Texture2> texture = style.GetTexture(Style::FGIMG);

            RectangleShape<2, int> tcoordRect;
            tcoordRect.mExtent[0] = texture->GetDimension(0);
            tcoordRect.mExtent[1] = texture->GetDimension(1);
            tcoordRect.mCenter = tcoordRect.mExtent / 2;

            mForegroundImage->SetTexture(
                mScaleImage ? skin->AddScaledTexture(texture, mAbsoluteRect, tcoordRect) : texture);
            SetScaleImage(true);
        }
        else mForegroundImage->SetTexture(nullptr);
    }
}

//! Set the styles used for each state
void UIButton::SetStyles(const std::array<Style, Style::NUM_STATES>& styles)
{
    mStyles = styles;
    SetFromState();
}
// END PATCH
