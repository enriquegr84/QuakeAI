// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIScrollBar.h"

#include "Core/OS/OS.h"

#include "Graphic/Image/ImageResource.h"
#include "Graphic/Renderer/Renderer.h"

//! constructor
UIScrollBar::UIScrollBar(BaseUI* ui, int id, RectangleShape<2, int> rectangle, bool horizontal, bool autoScale)
	: BaseUIScrollBar(id, rectangle), mUI(ui), mUpButton(0), mDownButton(0), mDragging(false), mHorizontal(horizontal),
	mDraggedBySlider(false), mTrayClicked(false), mAutoScaling(autoScale), mScrollPosition(0), mDrawCenter(0), mThumbSize(0), 
    mMinPosition(0), mMaxPosition(100), mSmallStep(10), mLargeStep(50), mDragOffset(0), mBorderSize(0), mPageSize(100)
{
    // basic color effect
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

        std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

    // slider color effect
    {
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

        mEffectSlider = std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
        mVisualSlider = std::make_shared<Visual>(vbuffer, ibuffer, mEffectSlider);
    }
}


//! destructor
UIScrollBar::~UIScrollBar()
{

}


//! initialize checkbox
void UIScrollBar::OnInit(bool noclip)
{
	RefreshControls();

	SetNotClipped(noclip);

	// this element can be tabbed to
	SetTabStop(true);
	SetTabOrder(-1);

	SetPosition(0);
}


//! called if an event happened.
bool UIScrollBar::OnEvent(const Event& evt)
{
	if (IsEnabled())
	{
		switch(evt.mEventType)
		{
			case ET_KEY_INPUT_EVENT:
				if (evt.mKeyInput.mPressedDown)
				{
					const int oldPos = mScrollPosition;
					bool absorb = true;
					switch (evt.mKeyInput.mKey)
					{
						case KEY_LEFT:
						case KEY_UP:
							SetPosition(mScrollPosition - mSmallStep);
							break;
						case KEY_RIGHT:
						case KEY_DOWN:
							SetPosition(mScrollPosition + mSmallStep);
							break;
						case KEY_HOME:
							SetPosition(mMinPosition);
							break;
						case KEY_PRIOR:
							SetPosition(mScrollPosition - mLargeStep);
							break;
						case KEY_END:
							SetPosition(mMaxPosition);
							break;
						case KEY_NEXT:
							SetPosition(mScrollPosition + mLargeStep);
							break;
						default:
							absorb = false;
					}

					if (mScrollPosition != oldPos)
					{
						Event newEvent;
						newEvent.mEventType = ET_UI_EVENT;
						newEvent.mUIEvent.mCaller = this;
						newEvent.mUIEvent.mElement = 0;
						newEvent.mUIEvent.mEventType = UIEVT_SCROLL_BAR_CHANGED;
						mParent->OnEvent(newEvent);
					}
					if (absorb)
						return true;
				}
				break;
			case ET_UI_EVENT:
				if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
				{
					if (evt.mUIEvent.mCaller == mUpButton.get())
						SetPosition(mScrollPosition-mSmallStep);
					else
					if (evt.mUIEvent.mCaller == mDownButton.get())
						SetPosition(mScrollPosition+mSmallStep);

					Event newEvent;
					newEvent.mEventType = ET_UI_EVENT;
					newEvent.mUIEvent.mCaller = this;
					newEvent.mUIEvent.mElement = 0;
					newEvent.mUIEvent.mEventType = UIEVT_SCROLL_BAR_CHANGED;
					mParent->OnEvent(newEvent);

					return true;
				}
				else
				if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST)
				{
					if (evt.mUIEvent.mCaller == this)
						mDragging = false;
				}
				break;
			case ET_MOUSE_INPUT_EVENT:
				{
					Vector2<int> p{ evt.mMouseInput.X, evt.mMouseInput.Y };

					bool isInside = IsPointInside ( p );
					switch(evt.mMouseInput.mEvent)
					{
						case MIE_MOUSE_WHEEL:
							if (mUI->HasFocus(shared_from_this()))
							{
                                int8_t d = evt.mMouseInput.mWheel < 0 ? -1 : 1;
                                int8_t h = mHorizontal ? 1 : -1;
								SetPosition(	GetPosition() + ( d * mSmallStep * h ));

								Event newEvent;
								newEvent.mEventType = ET_UI_EVENT;
								newEvent.mUIEvent.mCaller = this;
								newEvent.mUIEvent.mElement = 0;
								newEvent.mUIEvent.mEventType = UIEVT_SCROLL_BAR_CHANGED;
								mParent->OnEvent(newEvent);
								return true;
							}
							break;
						case MIE_LMOUSE_PRESSED_DOWN:
						{
							if (isInside)
							{
								mDragging = true;
								mDraggedBySlider = mSliderRect.IsPointInside(p);
                                Vector2<int> corner = mSliderRect.GetVertice(RVP_UPPERLEFT);
                                mDragOffset = mHorizontal ? p[0] - corner[0] : p[1] - corner[1];
								mTrayClicked = !mDraggedBySlider;
                                if (mTrayClicked)
                                {
                                    const int newPos = GetPosFromMousePosition(p);
                                    const int oldPos = mScrollPosition;
                                    SetPosition(newPos);
                                    //drag in the middle
                                    mDragOffset = mThumbSize / 2;
                                    //report the scroll event
                                    if (mScrollPosition != oldPos && mParent)
                                    {
                                        Event newEvent;
                                        newEvent.mEventType = ET_UI_EVENT;
                                        newEvent.mUIEvent.mCaller = this;
                                        newEvent.mUIEvent.mElement = 0;
                                        newEvent.mUIEvent.mEventType = UIEVT_SCROLL_BAR_CHANGED;
                                        mParent->OnEvent(newEvent);
                                    }
                                }
								mUI->SetFocus ( shared_from_this() );
								return true;
							}
							break;
						}
						case MIE_LMOUSE_LEFT_UP:
						case MIE_MOUSE_MOVED:
						{
							if ( !evt.mMouseInput.IsLeftPressed () )
								mDragging = false;

							if ( !mDragging )
							{
								if (evt.mMouseInput.mEvent == MIE_MOUSE_MOVED )
									break;
								return isInside;
							}

							if (evt.mMouseInput.mEvent == MIE_LMOUSE_LEFT_UP )
								mDragging = false;

                            // clang-format off
							if (!mDraggedBySlider)
							{
								if ( isInside )
								{
									mDraggedBySlider = mSliderRect.IsPointInside(p);
									mTrayClicked = !mDraggedBySlider;
								}

								if (!mDraggedBySlider)
								{
									mTrayClicked = false;
									if (evt.mMouseInput.mEvent == MIE_MOUSE_MOVED)
										return isInside;
								}
							}
                            // clang-format on

                            const int newPos = GetPosFromMousePosition(p);
                            const int oldPos = mScrollPosition;
                            SetPosition(newPos);

							if (mScrollPosition != oldPos && mParent)
							{
								Event newEvent;
								newEvent.mEventType = ET_UI_EVENT;
								newEvent.mUIEvent.mCaller = this;
								newEvent.mUIEvent.mElement = 0;
								newEvent.mUIEvent.mEventType = UIEVT_SCROLL_BAR_CHANGED;
								mParent->OnEvent(newEvent);
							}
							return isInside;
						} break;

						default:
							break;
					}
				} 
				break;

			default:
				break;
		}
	}

	return BaseUIElement::OnEvent(evt);
}

void UIScrollBar::OnPostDraw(unsigned int timeMs)
{
    /*
	if (mDragging && !mDraggedBySlider && mTrayClicked && timeMs > mLastChange + 200)
	{
		mLastChange = timeMs;

		const int oldPos = mScrollPosition;

		if (mDesiredPos >= mScrollPosition + mLargeStep)
			SetPosition(mScrollPosition + mLargeStep);
		else if (mDesiredPos <= mScrollPosition - mLargeStep)
			SetPosition(mScrollPosition - mLargeStep);
		else if (mDesiredPos >= mScrollPosition - mLargeStep && mDesiredPos <= mScrollPosition + mLargeStep)
			SetPosition(mDesiredPos);

		if (mScrollPosition != oldPos && mParent)
		{
			Event newEvent;
			newEvent.mEventType = ET_UI_EVENT;
			newEvent.mUIEvent.mCaller = this;
			newEvent.mUIEvent.mElement = 0;
			newEvent.mUIEvent.mEventType = UIEVT_SCROLL_BAR_CHANGED;
			mParent->OnEvent(newEvent);
		}
	}
    */
}

//! draws the element and its children
void UIScrollBar::Draw()
{
	if (!IsVisible())
		return;

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (!skin)
		return;

    SColor iconColor = skin->GetColor(IsEnabled() ? DC_WINDOW_SYMBOL : DC_GRAY_WINDOW_SYMBOL);
	if ( iconColor != mCurrentIconColor )
		RefreshControls();

	mSliderRect = mAbsoluteRect;

    Renderer::Get()->SetBlendState(mBlendState);

	// draws the background
	skin->Draw2DRectangle(skin->GetColor(DC_SCROLLBAR), mVisual, mSliderRect, &mAbsoluteClippingRect);

	if (Function<float>::IsNotZero(Range()))
	{
		// recalculate slider rectangle
		if (mHorizontal)
		{
			mSliderRect.mCenter[0] = mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] + mDrawCenter;
			mSliderRect.mExtent[0] = mThumbSize;
		}
		else
		{
            mSliderRect.mCenter[1] = mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[1] + mDrawCenter;
            mSliderRect.mExtent[1] = mThumbSize;
		}
		skin->Draw3DButtonPaneStandard(mVisualSlider, mSliderRect, &mAbsoluteClippingRect);
	}

    Renderer::Get()->SetDefaultBlendState();

	// draw buttons
	BaseUIElement::Draw();
}


void UIScrollBar::UpdateAbsolutePosition()
{
	BaseUIElement::UpdateAbsolutePosition();

	// todo: properly resize
	RefreshControls();
	SetPosition ( mScrollPosition );
}

//!
int UIScrollBar::GetPosFromMousePosition(const Vector2<int> &pos) const
{
    int w, p;
    int offset = mDraggedBySlider ? mDragOffset : mThumbSize / 2;

    if (mHorizontal)
    {
        w = mRelativeRect.mExtent[0] - mBorderSize * 2 - mThumbSize;
        p = pos[0] - mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] - mBorderSize - offset;
    }
    else 
    {
        w = mRelativeRect.mExtent[1] - mBorderSize * 2 - mThumbSize;
        p = pos[1] - mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[1] - mBorderSize - offset;
    }
    return Function<float>::IsNotZero(Range()) ? 
		int(float(p) / float(w) * Range() + 0.5f) + mMinPosition : 0;
}


//! sets the position of the scrollbar
void UIScrollBar::SetPosition(int pos)
{
    int thumbArea = 0, thumbMin = 0;

    if (mHorizontal)
    {
        thumbMin = mRelativeRect.mExtent[1];
        thumbArea = mRelativeRect.mExtent[0] - mBorderSize * 2;
    }
    else 
    {
        thumbMin = mRelativeRect.mExtent[0];
        thumbArea = mRelativeRect.mExtent[1] - mBorderSize * 2;
    }

    if (mAutoScaling)
        mThumbSize = int(thumbArea / (float(mPageSize) / float(thumbArea + mBorderSize * 2)));

    if (thumbMin < thumbArea)
        mThumbSize = std::clamp(mThumbSize, thumbMin, thumbArea);
    else
        mThumbSize = std::clamp(mThumbSize, thumbArea, thumbMin);
    mScrollPosition = std::clamp(pos, mMinPosition, mMaxPosition);

    float f = Function<float>::IsNotZero(Range()) ? 
        (float(thumbArea) - float(mThumbSize)) / Range() : 1.0f;
    mDrawCenter = int((float(mScrollPosition - mMinPosition) * f) + (float(mThumbSize) * 0.5f)) + mBorderSize;
}


//! gets the small step value
int UIScrollBar::GetSmallStep() const
{
	return mSmallStep;
}


//! sets the small step value
void UIScrollBar::SetSmallStep(int step)
{
    mSmallStep = step > 0 ? step : 10;
}


//! gets the large step value
int UIScrollBar::GetLargeStep() const
{
	return mLargeStep;
}


//! sets the small step value
void UIScrollBar::SetLargeStep(int step)
{
    mLargeStep = step > 0 ? step : 50;
}


//! gets the maximum value of the scrollbar.
int UIScrollBar::GetMax() const
{
	return mMaxPosition;
}

//! check wether the scrollbar is being dragged
bool UIScrollBar::IsDragging() const
{
	return mDragging;
}

//! sets the maximum value of the scrollbar.
void UIScrollBar::SetMax(int max)
{
	mMaxPosition = max;
	if ( mMinPosition > mMaxPosition )
		mMinPosition = mMaxPosition;

	bool enable = Function<float>::IsNotZero(Range());
	mUpButton->SetEnabled(enable);
	mDownButton->SetEnabled(enable);
	SetPosition(mScrollPosition);
}

//! gets the minimum value of the scrollbar.
int UIScrollBar::GetMin() const
{
	return mMinPosition;
}


//! sets the minimum value of the scrollbar.
void UIScrollBar::SetMin(int min)
{
	mMinPosition = min;
	if ( mMaxPosition < mMinPosition )
		mMaxPosition = mMinPosition;

	bool enable = Function<float>::IsNotZero(Range());
	mUpButton->SetEnabled(enable);
	mDownButton->SetEnabled(enable);
	SetPosition(mScrollPosition);
}


//! gets the current position of the scrollbar
int UIScrollBar::GetPosition() const
{
	return mScrollPosition;
}


void UIScrollBar::SetPageSize(const int& size)
{
    mPageSize = size;
    SetPosition(mScrollPosition);
}


void UIScrollBar::SetArrowsVisible(ArrowVisibility visible)
{
    mArrowVisibility = visible;
    RefreshControls();
}


//! refreshes the position and text on child buttons
void UIScrollBar::RefreshControls()
{
	mCurrentIconColor = SColor(255, 255, 255, 255);

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	std::shared_ptr<BaseUISpriteBank> sprites = 0;

	if (skin)
	{
		sprites = skin->GetSpriteBank();
		mCurrentIconColor = skin->GetColor(IsEnabled() ? DC_WINDOW_SYMBOL : DC_GRAY_WINDOW_SYMBOL);
	}

	if (mHorizontal)
	{
		int h = mRelativeRect.mExtent[1];
        mBorderSize = mRelativeRect.mExtent[0] < h * 4 ? 0 : h;

		RectangleShape<2, int> rectangle;
		rectangle.mCenter[0] = h / 2;
		rectangle.mCenter[1] = h / 2;
		rectangle.mExtent[0] = h;
		rectangle.mExtent[1] = h;
		if (!mUpButton)
		{
			mUpButton.reset(new UIButton(mUI, -1, rectangle));
			mUpButton->SetParent(shared_from_this());
			mUpButton->OnInit();
			mUpButton->SetSubElement(true);
			mUpButton->SetTabStop(false);
		}
		if (sprites)
		{
			mUpButton->SetSpriteBank(sprites);
			mUpButton->SetSprite(BS_BUTTON_UP, DI_CURSOR_LEFT, mCurrentIconColor);
			mUpButton->SetSprite(BS_BUTTON_DOWN, DI_CURSOR_LEFT, mCurrentIconColor);
		}
		mUpButton->SetRelativePosition(rectangle);
		mUpButton->SetAlignment(UIA_UPPERLEFT, UIA_UPPERLEFT, UIA_UPPERLEFT, UIA_LOWERRIGHT);

		rectangle.mCenter[0] = mRelativeRect.mExtent[0] - ( h / 2 );
		rectangle.mCenter[1] = h / 2;
		rectangle.mExtent[0] = h;
		rectangle.mExtent[1] = h;
		if (!mDownButton)
		{
			mDownButton.reset(new UIButton(mUI, -1, rectangle));
			mDownButton->SetParent(shared_from_this());
			mDownButton->OnInit();
			mDownButton->SetSubElement(true);
			mDownButton->SetTabStop(false);
		}
		if (sprites)
		{
			mDownButton->SetSpriteBank(sprites);
			mDownButton->SetSprite(BS_BUTTON_UP, DI_CURSOR_RIGHT, mCurrentIconColor);
			mDownButton->SetSprite(BS_BUTTON_DOWN, DI_CURSOR_RIGHT, mCurrentIconColor);
		}
		mDownButton->SetRelativePosition(rectangle);
		mDownButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_LOWERRIGHT);
	}
	else
	{
		int w = mRelativeRect.mExtent[0];
        mBorderSize = mRelativeRect.mExtent[1] < w * 4 ? 0 : w;

		RectangleShape<2, int> rectangle;
		rectangle.mCenter[0] = w / 2;
		rectangle.mCenter[1] = w / 2;
		rectangle.mExtent[0] = w;
		rectangle.mExtent[1] = w;

		if (!mUpButton)
		{
			mUpButton.reset(new UIButton(mUI, -1, rectangle));
			mUpButton->SetParent(shared_from_this());
			mUpButton->OnInit();
			mUpButton->SetSubElement(true);
			mUpButton->SetTabStop(false);
		}
		if (sprites)
		{
			mUpButton->SetSpriteBank(sprites);
			mUpButton->SetSprite(BS_BUTTON_UP, DI_CURSOR_UP, mCurrentIconColor);
			mUpButton->SetSprite(BS_BUTTON_DOWN, DI_CURSOR_UP, mCurrentIconColor);
		}
		mUpButton->SetRelativePosition(rectangle);
		mUpButton->SetAlignment(UIA_UPPERLEFT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);

		rectangle.mCenter[0] = w / 2;
		rectangle.mCenter[1] = mRelativeRect.mExtent[1] - ( w / 2 );
		rectangle.mExtent[0] = w;
		rectangle.mExtent[1] = w;
		if (!mDownButton)
		{
			mDownButton.reset(new UIButton(mUI, -1, rectangle));
			mDownButton->SetParent(shared_from_this());
			mDownButton->OnInit();
			mDownButton->SetSubElement(true);
			mDownButton->SetTabStop(false);
		}
		if (sprites)
		{
			mDownButton->SetSpriteBank(sprites);
			mDownButton->SetSprite(BS_BUTTON_UP, DI_CURSOR_DOWN, mCurrentIconColor);
			mDownButton->SetSprite(BS_BUTTON_DOWN, DI_CURSOR_DOWN, mCurrentIconColor);
		}
		mDownButton->SetRelativePosition(rectangle);
		mDownButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_LOWERRIGHT);
	}

    bool visible = true;
    switch (mArrowVisibility)
    {
        case SHOW:
            visible = true;
            mBorderSize = mHorizontal ? mRelativeRect.mExtent[1] : mRelativeRect.mExtent[0];
            break;
        case HIDE:
            visible = false;
            mBorderSize = 0;
            break;
        case DEFAULT:
            visible = mBorderSize != 0;
            break;
    }

    mUpButton->SetVisible(visible);
    mDownButton->SetVisible(visible);
}