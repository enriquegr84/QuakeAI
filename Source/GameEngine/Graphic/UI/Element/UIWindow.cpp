// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIWindow.h"

#include "Graphic/Renderer/Renderer.h"

//! constructor
UIWindow::UIWindow(BaseUI* ui, int id, RectangleShape<2, int> rectangle)
	: BaseUIWindow(id, rectangle), mUI(ui), mScrollBarV(0), mScrollBarPos(0),
	mDragging(false), mIsDraggableWindow(true), mDrawBackground(true), 
	mDrawTitlebar(true), mIsActive(false)
{
	#ifdef _DEBUG
		//SetDebugName("UIWindow");
	#endif

	std::shared_ptr<BaseUISkin> skin = 0;
	if (mUI)
		skin = mUI->GetSkin();

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

	mEffect = std::make_shared<ColorEffect>(
		ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

	// Create the geometric object for drawing.
	mVisualBackground = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);

	vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
	ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
	vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

	// Create the geometric object for drawing.
	mVisualTitle = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

//! destructor
UIWindow::~UIWindow()
{

}

void UIWindow::RefreshSprites()
{
	if (!mUI)
		return;

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (!skin)
		return;

	const std::shared_ptr<BaseUISpriteBank>& sprites = skin->GetSpriteBank();
	if (!sprites)
		return;

	mCurrentIconColor = skin->GetColor(IsEnabled() ? DC_WINDOW_SYMBOL : DC_GRAY_WINDOW_SYMBOL);

	if (sprites)
	{
		mCloseButton->SetSpriteBank(sprites);
		mCloseButton->SetSprite(BS_BUTTON_UP, DI_WINDOW_CLOSE, mCurrentIconColor);
		mCloseButton->SetSprite(BS_BUTTON_DOWN, DI_WINDOW_CLOSE, mCurrentIconColor);

		mMinButton->SetSpriteBank(sprites);
		mMinButton->SetSprite(BS_BUTTON_UP, DI_WINDOW_MINIMIZE, mCurrentIconColor);
		mMinButton->SetSprite(BS_BUTTON_DOWN, DI_WINDOW_MINIMIZE, mCurrentIconColor);

		mRestoreButton->SetSpriteBank(sprites);
		mRestoreButton->SetSprite(BS_BUTTON_UP, DI_WINDOW_RESTORE, mCurrentIconColor);
		mRestoreButton->SetSprite(BS_BUTTON_DOWN, DI_WINDOW_RESTORE, mCurrentIconColor);

		mCollapseButton->SetSpriteBank(sprites);
		mCollapseButton->SetSprite(BS_BUTTON_UP, DI_WINDOW_COLLAPSE, mCurrentIconColor);
		mCollapseButton->SetSprite(BS_BUTTON_DOWN, DI_WINDOW_COLLAPSE, mCurrentIconColor);
	}
}


//! Initialize Window
void UIWindow::OnInit(bool scrollBarVertical)
{
	std::shared_ptr<BaseUISkin> skin = 0;
	if (mUI)
		skin = mUI->GetSkin();

	mCurrentIconColor = SColor(255, 255, 255, 255);

	int buttonw = 15;
	if (skin)
		buttonw = skin->GetSize(DS_WINDOW_BUTTON_WIDTH);
	
	RectangleShape<2, int> rect;
	rect.mCenter[0] = (int)(buttonw / 2.f);
	rect.mCenter[1] = 2 + (int)(buttonw / 2.f);
	rect.mExtent[0] = buttonw;
	rect.mExtent[1] = buttonw;

	mCollapseButton = mUI->AddButton(rect, shared_from_this(), -1,
		L"", skin ? skin->GetDefaultText(DT_WINDOW_COLLAPSE) : L"Collapse");
	mCollapseButton->SetPushButton(true);
	mCollapseButton->SetSubElement(true);
	mCollapseButton->SetTabStop(false);
	mCollapseButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);

	rect.mCenter[0] = mRelativeRect.mExtent[0] - (int)round(buttonw / 2.f);
	mCloseButton = mUI->AddButton(rect, shared_from_this(), -1,
		L"", skin ? skin->GetDefaultText(DT_WINDOW_CLOSE) : L"Close");
	mCloseButton->SetSubElement(true);
	mCloseButton->SetTabStop(false);
	mCloseButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);
	
	rect.mCenter[0] -= buttonw;
	mRestoreButton = mUI->AddButton(rect, shared_from_this(), -1,
		L"", skin ? skin->GetDefaultText(DT_WINDOW_RESTORE) : L"Restore");
	mRestoreButton->SetVisible(false);
	mRestoreButton->SetSubElement(true);
	mRestoreButton->SetTabStop(false);
	mRestoreButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);
	
	rect.mCenter[0] -= buttonw;
	mMinButton = mUI->AddButton(rect, shared_from_this(), -1,
		L"", skin ? skin->GetDefaultText(DT_WINDOW_MINIMIZE) : L"Minimize");
	mMinButton->SetVisible(false);
	mMinButton->SetSubElement(true);
	mMinButton->SetTabStop(false);
	mMinButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);

	if (scrollBarVertical)
	{
		int scrollBarSize = skin->GetSize(DS_SCROLLBAR_SIZE);
		int barHeight = skin->GetSize(DS_WINDOW_BUTTON_WIDTH) + 2;

		rect.mCenter[0] = mRelativeRect.mExtent[0] - (scrollBarSize/2);
		rect.mCenter[1] = barHeight + ((mRelativeRect.mExtent[1] - barHeight) / 2);
		rect.mExtent[0] = scrollBarSize;
		rect.mExtent[1] = mRelativeRect.mExtent[1] - barHeight - 2;

		mScrollBarV.reset(new UIScrollBar(mUI, 0, rect, false, true));
		mScrollBarV->SetParent(shared_from_this());
		mScrollBarV->OnInit(false);
		mScrollBarV->SetSubElement(true);
		mScrollBarV->SetPosition(0);
	}

	// this element is a tab group
	SetTabGroup(true);
	SetTabStop(true);
	SetTabOrder(-1);

	RefreshSprites();
}

//! called if an event happened.
bool UIWindow::OnEvent(const Event& evt)
{
	if (IsEnabled())
	{
		Vector2<int> p{ evt.mMouseInput.X, evt.mMouseInput.Y };

		switch (evt.mEventType)
		{
			case ET_UI_EVENT:
				if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
				{
					if (evt.mUIEvent.mCaller == mScrollBarV.get())
					{
						UpdateClientRect();
						return true;
					}
				}
				else if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST)
				{
					mDragging = false;
					mIsActive = false;
				}
				else if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUSED)
				{
					if (mParent && ((evt.mUIEvent.mCaller == this) || IsChild(evt.mUIEvent.mCaller)))
					{
						mParent->BringToFront(shared_from_this());
						mIsActive = true;
					}
					else
					{
						mIsActive = false;
					}
				}
				else if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
				{
					if (evt.mUIEvent.mCaller == mCloseButton.get())
					{
						if (mParent)
						{
							// send close event to parent
							Event e;
							e.mEventType = ET_UI_EVENT;
							e.mUIEvent.mCaller = this;
							e.mUIEvent.mElement = 0;
							e.mUIEvent.mEventType = UIEVT_ELEMENT_CLOSED;

							// if the event was not absorbed
							if (!mParent->OnEvent(e))
								Remove();

							return true;
						}
						else
						{
							Remove();
							return true;
						}
					}
					else if (evt.mUIEvent.mCaller == mCollapseButton.get())
					{
						if (mCollapseButton->IsPressed())
						{
							mCollapseButton->SetSprite(BS_BUTTON_UP, DI_WINDOW_EXPAND, mCurrentIconColor);
							mCollapseButton->SetSprite(BS_BUTTON_DOWN, DI_WINDOW_EXPAND, mCurrentIconColor);
						
							bool minState = mMinButton->IsVisible();
							bool closeState = mCloseButton->IsVisible();
							bool restoreState = mRestoreButton->IsVisible();
							bool collapseState = mCollapseButton->IsVisible();

							std::list<std::shared_ptr<BaseUIElement>>::iterator it = mChildren.begin();
							for (; it != mChildren.end(); ++it)
								(*it)->UpdateVisibility(false);

							mMinButton->UpdateVisibility(minState);
							mCloseButton->UpdateVisibility(closeState);
							mRestoreButton->UpdateVisibility(restoreState);
							mCollapseButton->UpdateVisibility(collapseState);
						}
						else
						{
							mCollapseButton->SetSprite(BS_BUTTON_UP, DI_WINDOW_COLLAPSE, mCurrentIconColor);
							mCollapseButton->SetSprite(BS_BUTTON_DOWN, DI_WINDOW_COLLAPSE, mCurrentIconColor);

							UpdateClientRect();
						}
						return true;
					}
				}
			break;
			case ET_MOUSE_INPUT_EVENT:
				switch (evt.mMouseInput.mEvent)
				{
					case MIE_MOUSE_WHEEL:
						if (mScrollBarV)
						{
							if (!mCollapseButton->IsPressed())
							{
								mScrollBarV->SetPosition(mScrollBarV->GetPosition() + (evt.mMouseInput.mWheel < 0 ? -1 : 1) * -10);
								UpdateClientRect();
							}
						}
						return true;
						break;

					case MIE_LMOUSE_PRESSED_DOWN:
						mDragStart[0] = evt.mMouseInput.X;
						mDragStart[1] = evt.mMouseInput.Y;
						mDragging = mIsDraggableWindow;

						if (mParent)
							mParent->BringToFront(shared_from_this());
						return true;
					case MIE_LMOUSE_LEFT_UP:
						mDragging = false;
						return true;
					case MIE_MOUSE_MOVED:
						if (!evt.mMouseInput.IsLeftPressed())
							mDragging = false;

						if (mDragging)
						{
							// gui window should not be dragged outside its parent
							if (mParent)
							{
								if (evt.mMouseInput.X < mParent->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] + 1 ||
									evt.mMouseInput.Y < mParent->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[1] + 1 ||
									evt.mMouseInput.X > mParent->GetAbsolutePosition().GetVertice(RVP_LOWERRIGHT)[0] - 1 ||
									evt.mMouseInput.Y > mParent->GetAbsolutePosition().GetVertice(RVP_LOWERRIGHT)[1] - 1)
									return true;
							}

							Move(Vector2<int>{evt.mMouseInput.X - mDragStart[0], evt.mMouseInput.Y - mDragStart[1]});
							mDragStart[0] = evt.mMouseInput.X;
							mDragStart[1] = evt.mMouseInput.Y;
							return true;
						}
						break;
					default:
						break;
					}
				default:
					break;
		}
	}

	return BaseUIElement::OnEvent(evt);
}


//! Updates the absolute position.
void UIWindow::UpdateAbsoluteTransformation()
{
	BaseUIElement::UpdateAbsolutePosition();
	UpdateScrollBarHeight();
	UpdateClientRect();
}


//! draws the element and its children
void UIWindow::Draw()
{
	if (mVisible)
	{
		const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
		if (mCurrentIconColor != skin->GetColor(IsEnabled() ? DC_WINDOW_SYMBOL : DC_GRAY_WINDOW_SYMBOL))
			RefreshSprites();

		RectangleShape<2, int> rect = mAbsoluteRect;

		// draw body fast
		if (mDrawBackground)
		{
			if (mCollapseButton->IsPressed())
			{
				rect = skin->Draw3DWindowBackground(nullptr,
					mVisualTitle, mDrawTitlebar, skin->GetColor(mIsActive ? DC_ACTIVE_BORDER : DC_INACTIVE_BORDER),
					mAbsoluteRect, &mAbsoluteClippingRect);
			}
			else
			{
				rect = skin->Draw3DWindowBackground(mVisualBackground,
					mVisualTitle, mDrawTitlebar, skin->GetColor(mIsActive ? DC_ACTIVE_BORDER : DC_INACTIVE_BORDER),
					mAbsoluteRect, &mAbsoluteClippingRect);
			}

			if (mDrawTitlebar && mText.size())
			{
				rect.mCenter[0] += (int)round(skin->GetSize(DS_TITLEBARTEXT_DISTANCE_X) / 2);
				rect.mCenter[0] += (skin->GetSize(DS_WINDOW_BUTTON_WIDTH) + 5 )/ 2;
				rect.mCenter[1] += (int)round(skin->GetSize(DS_TITLEBARTEXT_DISTANCE_Y) / 2);

				rect.mExtent[0] -= skin->GetSize(DS_TITLEBARTEXT_DISTANCE_X);
				rect.mExtent[0] -= skin->GetSize(DS_WINDOW_BUTTON_WIDTH) + 5;
				rect.mExtent[1] -= skin->GetSize(DS_TITLEBARTEXT_DISTANCE_Y);

				std::shared_ptr<BaseUIFont> font = skin->GetFont(DF_WINDOW);
				if (font)
				{
					font->Draw(mText.c_str(), rect,
						skin->GetColor(mIsActive ? DC_ACTIVE_CAPTION : DC_INACTIVE_CAPTION),
						false, true, &mAbsoluteRect);
				}
			}
		}
	}

	BaseUIElement::Draw();
}

//! Returns true if a point is within this element.
/** Elements with a shape other than a rectangle should override this method */
bool UIWindow::IsPointInside(const Vector2<int>& point) const
{
	if (mCollapseButton->IsPressed())
	{
		const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
		int barHeight = skin->GetSize(DS_WINDOW_BUTTON_WIDTH) + 2;

        return (
            mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[0] >= point[0] &&
            mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] <= point[0] &&
            mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[1] <= point[1] &&
            mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[1] + barHeight >= point[1]);
	}
	else
	{
        return (
            mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[0] >= point[0] &&
            mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] <= point[0] &&
            mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[1] <= point[1] &&
            mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[1] >= point[1]);
	}
}

//! Returns pointer to the close button
const std::shared_ptr<BaseUIButton>& UIWindow::GetCloseButton() const
{
	return mCloseButton;
}


//! Returns pointer to the minimize button
const std::shared_ptr<BaseUIButton>& UIWindow::GetMinimizeButton() const
{
	return mMinButton;
}


//! Returns pointer to the maximize button
const std::shared_ptr<BaseUIButton>& UIWindow::GetMaximizeButton() const
{
	return mRestoreButton;
}

//! Returns pointer to the collapse button
const std::shared_ptr<BaseUIButton>& UIWindow::GetCollapseButton() const
{
	return mCollapseButton;
}

//! Returns true if the window is draggable, false if not
bool UIWindow::IsDraggable() const
{
	return mIsDraggableWindow;
}


//! Sets whether the window is draggable
void UIWindow::SetDraggable(bool draggable)
{
	mIsDraggableWindow = draggable;

	if (mDragging && !mIsDraggableWindow)
		mDragging = false;
}


//! Set if the window background will be drawn
void UIWindow::SetDrawBackground(bool draw)
{
	mDrawBackground = draw;
}


//! Get if the window background will be drawn
bool UIWindow::GetDrawBackground() const
{
	return mDrawBackground;
}


//! Set if the window titlebar will be drawn
void UIWindow::SetDrawTitlebar(bool draw)
{
	mDrawTitlebar = draw;
}


//! Get if the window titlebar will be drawn
bool UIWindow::GetDrawTitlebar() const
{
	return mDrawTitlebar;
}


void UIWindow::UpdateClientRect()
{
	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	mClientRect = mAbsoluteRect;
	mClientRect.mCenter[1] += (skin->GetSize(DS_WINDOW_BUTTON_WIDTH) + 2) / 2;
	mClientRect.mExtent[1] -= skin->GetSize(DS_WINDOW_BUTTON_WIDTH) + 2;

	if (mScrollBarV)
	{
		mClientRect.mCenter[0] -= skin->GetSize(DS_SCROLLBAR_SIZE) / 2;
		mClientRect.mExtent[0] -= skin->GetSize(DS_SCROLLBAR_SIZE);
	}

	std::list<std::shared_ptr<BaseUIElement>>::iterator it = mChildren.begin();
	for (; it != mChildren.end(); ++it)
	{
		if ((*it) != mMinButton &&
			(*it) != mScrollBarV &&
			(*it) != mCloseButton && 
			(*it) != mRestoreButton &&
			(*it) != mCollapseButton )
		{
			RectangleShape<2, int> rectangle = (*it)->GetRelativePosition();
			rectangle.mCenter[1] += (mScrollBarPos - mScrollBarV->GetPosition());
			(*it)->SetRelativePosition(rectangle);

			if (mClientRect.IsPointInside((*it)->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)) &&
				mClientRect.IsPointInside((*it)->GetAbsolutePosition().GetVertice(RVP_UPPERRIGHT)) &&
				mClientRect.IsPointInside((*it)->GetAbsolutePosition().GetVertice(RVP_LOWERLEFT)) &&
				mClientRect.IsPointInside((*it)->GetAbsolutePosition().GetVertice(RVP_LOWERRIGHT)))
			{
				(*it)->SetVisible(true);
			}
			else
			{
				(*it)->SetVisible(false);
			}
		}
	}

	mScrollBarPos = mScrollBarV->GetPosition();
}

void UIWindow::UpdateScrollBarHeight()
{
	RectangleShape<2, int> rectangle = GetAbsolutePosition();
    int parentHeight = rectangle.GetVertice(RVP_LOWERRIGHT)[1];
	int diffHeight = 0;

	std::list<std::shared_ptr<BaseUIElement>>::iterator it = mChildren.begin();
	for (; it != mChildren.end(); ++it)
	{
		rectangle = (*it)->GetAbsolutePosition();
        int childHeight = rectangle.GetVertice(RVP_LOWERRIGHT)[1];
		if (childHeight - parentHeight > diffHeight)
			diffHeight = childHeight - parentHeight;
	}

	if (mScrollBarV)
	{
		diffHeight = diffHeight ? diffHeight + 2 : diffHeight;
		mScrollBarV->SetMax(diffHeight);
	}
}

//! Returns the rectangle of the drawable area (without border, without titlebar and without scrollbars)
RectangleShape<2, int> UIWindow::GetClientRect() const
{
	return mClientRect;
}