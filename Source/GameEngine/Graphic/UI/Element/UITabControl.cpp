// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UITabControl.h"

#include "UIButton.h"
#include "UISkin.h"
#include "UIFont.h"

#include "Core/OS/OS.h"

#include "Graphic/Image/ImageResource.h"
#include "Graphic/Renderer/Renderer.h"


// ------------------------------------------------------------------
// Tab
// ------------------------------------------------------------------

//! constructor
UITab::UITab(BaseUI* baseUI, int id, const RectangleShape<2, int>& rectangle, int number)
	: BaseUITab(id, rectangle), mUI(baseUI), mNumber(number), mBackColor(0,0,0,0), 
    mOverrideTextColorEnabled(false), mTextColor(255,0,0,0), mDrawBackground(false)
{
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

    mEffect = std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);

    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (skin)
		mTextColor = skin->GetColor(DC_BUTTON_TEXT);
}


//! Returns number of tab in tabcontrol. Can be accessed
//! later BaseUITabControl::GetTab() by this number.
int UITab::GetNumber() const
{
	return mNumber;
}


//! Sets the number
void UITab::SetNumber(int n)
{
	mNumber = n;
}

void UITab::RefreshSkinColors()
{
	if ( !mOverrideTextColorEnabled )
		mTextColor = mUI->GetSkin()->GetColor(DC_BUTTON_TEXT);
}

//! draws the element and its children
void UITab::Draw()
{
	if (!IsVisible())
		return;

    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (skin && mDrawBackground)
		skin->Draw2DRectangle(mBackColor, mVisual, mAbsoluteRect, &mAbsoluteClippingRect);

	BaseUIElement::Draw();
}


//! sets if the tab should draw its background
void UITab::SetDrawBackground(bool draw)
{
	mDrawBackground = draw;
}


//! sets the color of the background, if it should be drawn.
void UITab::SetBackgroundColor(SColor c)
{
	mBackColor = c;
}


//! sets the color of the text
void UITab::SetTextColor(SColor c)
{
	mOverrideTextColorEnabled = true;
	mTextColor = c;
}


SColor UITab::GetTextColor() const
{
	return mTextColor;
}


//! returns true if the tab is drawing its background, false if not
bool UITab::IsDrawingBackground() const
{
	return mDrawBackground;
}


//! returns the color of the background
SColor UITab::GetBackgroundColor() const
{
	return mBackColor;
}


// ------------------------------------------------------------------
// Tabcontrol
// ------------------------------------------------------------------

//! constructor
UITabControl::UITabControl(BaseUI* ui, const RectangleShape<2, int>& rectangle, 
    bool fillbackground, bool border, int id)
	: BaseUITabControl(id, rectangle), mUI(ui), mActiveTab(-1),
	mBorder(border), mFillBackground(fillbackground), mScrollControl(false), 
    mTabHeight(0), mVerticalAlignment(UIA_UPPERLEFT), mUpButton(0), mDownButton(0), 
    mTabMaxWidth(0), mCurrentScrollTabIndex(0), mTabExtraWidth(20)
{
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

    mEffect = std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

//! destructor
UITabControl::~UITabControl()
{

}

void UITabControl::OnInit()
{
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    std::shared_ptr<BaseUISpriteBank> sprites = 0;

    mTabHeight = 32;

    if (skin)
    {
        sprites = skin->GetSpriteBank();
        mTabHeight = skin->GetSize(DS_BUTTON_HEIGHT) + 2;
    }

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ 10, 10 };
    rect.mCenter = rect.mExtent / 2;
    mUpButton = mUI->AddButton(rect, shared_from_this());
    if (mUpButton)
    {
        mUpButton->SetSpriteBank(sprites);
        mUpButton->SetVisible(false);
        mUpButton->SetSubElement(true);
        mUpButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);
        mUpButton->SetOverrideFont(mUI->GetBuiltInFont());
    }

    mDownButton = mUI->AddButton(rect, shared_from_this());
    if (mDownButton)
    {
        mDownButton->SetSpriteBank(sprites);
        mDownButton->SetVisible(false);
        mDownButton->SetSubElement(true);
        mDownButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);
        mDownButton->SetOverrideFont(mUI->GetBuiltInFont());
    }

    SetTabVerticalAlignment(UIA_UPPERLEFT);
    RefreshSprites();
}

void UITabControl::RefreshSprites()
{
	SColor color(255,255,255,255);
	std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	if (skin)
		color = skin->GetColor(IsEnabled() ? DC_WINDOW_SYMBOL : DC_GRAY_WINDOW_SYMBOL);

	if (mUpButton)
	{
		mUpButton->SetSprite(BS_BUTTON_UP, DI_CURSOR_LEFT, color);
		mUpButton->SetSprite(BS_BUTTON_DOWN, DI_CURSOR_LEFT, color);
	}

	if (mDownButton)
	{
		mDownButton->SetSprite(BS_BUTTON_UP, DI_CURSOR_RIGHT, color);
		mDownButton->SetSprite(BS_BUTTON_DOWN, DI_CURSOR_RIGHT, color);
	}
}

//! Adds a tab
std::shared_ptr<BaseUITab> UITabControl::AddTab(const wchar_t* caption, int id, bool isActive)
{
	std::shared_ptr<UITab> tab = std::make_shared<UITab>(mUI, (int)mTabs.size(), CalcTabPosition(), id);
    tab->SetParent(shared_from_this());
    tab->SetAlignment(UIA_UPPERLEFT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_LOWERRIGHT);
	tab->SetText(caption);
	tab->SetVisible(false);
	mTabs.push_back(tab);

	if (isActive)
	{
		mActiveTab = tab->GetNumber();
		tab->SetVisible(true);
	}

	RecalculateScrollBar();

	return tab;
}


//! adds a tab which has been created elsewhere
void UITabControl::AddTab(std::shared_ptr<UITab> tab)
{
	if (!tab)
		return;

	// check if its already added
	for (unsigned int i=0; i < mTabs.size(); ++i)
	{
		if (mTabs[i] == tab)
			return;
	}

	if (tab->GetNumber() == -1)
		tab->SetNumber((int)mTabs.size());

	while (tab->GetNumber() >= (int)mTabs.size())
		mTabs.push_back(0);

	if (mTabs[tab->GetNumber()])
	{
		mTabs.push_back(mTabs[tab->GetNumber()]);
		mTabs[mTabs.size()-1]->SetNumber((int)mTabs.size());
	}
	mTabs[tab->GetNumber()] = tab;

	if (mActiveTab == -1)
		mActiveTab = tab->GetNumber();

	if (tab->GetNumber() == mActiveTab)
		SetActiveTab(mActiveTab);
}

//! Insert the tab at the given index
std::shared_ptr<BaseUITab> UITabControl::InsertTab(int idx, const wchar_t* caption, int id, bool isActive)
{
	if ( idx < 0 || idx > (int)mTabs.size() )	// idx == Tabs.size() is indeed ok here as core::array can handle that
		return NULL;

    std::shared_ptr<UITab> tab = std::make_shared<UITab>(mUI, idx, CalcTabPosition(), id);
	tab->SetText(caption);
	tab->SetAlignment(UIA_UPPERLEFT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_LOWERRIGHT);
	tab->SetVisible(false);
	mTabs.insert(mTabs.begin() + idx, tab);

    if (isActive)
    {
        mActiveTab = tab->GetNumber();
        tab->SetVisible(true);
    }

	for (unsigned int i=(unsigned int)idx+1; i < mTabs.size(); ++i )
		mTabs[i]->SetNumber(i);

	RecalculateScrollBar();

	return tab;
}

//! Removes a tab from the tabcontrol
void UITabControl::RemoveTab(int idx)
{
	if ( idx < 0 || idx >= (int)mTabs.size() )
		return;

	mTabs.erase(mTabs.begin() + idx);
	for (unsigned int i=(unsigned int)idx; i < mTabs.size(); ++i )
		mTabs[i]->SetNumber(i);
}

//! Clears the tabcontrol removing all tabs
void UITabControl::Clear()
{
	mTabs.clear();
}

//! Returns amount of tabs in the tabcontrol
size_t UITabControl::GetTabCount() const
{
	return mTabs.size();
}


//! Returns a tab based on zero based index
std::shared_ptr<BaseUITab> UITabControl::GetTab(int idx) const
{
	if ((unsigned int)idx >= mTabs.size())
		return 0;

	return mTabs[idx];
}


//! called if an event happened.
bool UITabControl::OnEvent(const Event& evt)
{
	if (IsEnabled())
	{
		switch(evt.mEventType)
		{
		    case ET_UI_EVENT:
			    switch(evt.mUIEvent.mEventType)
			    {
			        case UIEVT_BUTTON_CLICKED:
				        if (evt.mUIEvent.mCaller == mUpButton.get())
				        {
					        ScrollLeft();
					        return true;
				        }
				        else if (evt.mUIEvent.mCaller == mDownButton.get())
				        {
					        ScrollRight();
					        return true;
				        }
			            break;

			        default:
			            break;
			    }
			    break;
		    case ET_MOUSE_INPUT_EVENT:
			    switch(evt.mMouseInput.mEvent)
			    {
			        case MIE_LMOUSE_PRESSED_DOWN:
				        // todo: dragging tabs around
				        return true;
			        case MIE_LMOUSE_LEFT_UP:
			        {
				        int idx = GetTabAt(evt.mMouseInput.X, evt.mMouseInput.Y);
				        if ( idx >= 0 )
				        {
					        SetActiveTab(idx);
					        return true;
				        }
				        break;
			        }
			        default:
				        break;
			    }
			    break;
		    default:
			    break;
        }
	}

	return BaseUIElement::OnEvent(evt);
}


void UITabControl::ScrollLeft()
{
	if (mCurrentScrollTabIndex > 0 )
		--mCurrentScrollTabIndex;

	RecalculateScrollBar();
}


void UITabControl::ScrollRight()
{
	if (mCurrentScrollTabIndex < (int)(mTabs.size()) - 1)
		if (NeedScrollControl(mCurrentScrollTabIndex, true))
			++mCurrentScrollTabIndex;

	RecalculateScrollBar();
}

int UITabControl::CalcTabWidth(int pos, std::shared_ptr<BaseUIFont> font, 
    const wchar_t* text, bool withScrollControl) const
{
	if ( !font )
		return 0;

	int len = font->GetDimension(text)[0] + mTabExtraWidth;
	if (mTabMaxWidth > 0 && len > mTabMaxWidth)
		len = mTabMaxWidth;

	// check if we miss the place to draw the tab-button
	if ( withScrollControl && mScrollControl && 
        pos+len > mUpButton->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - 2)
	{
		int tabMinWidth = font->GetDimension(L"A")[0];
		if (mTabExtraWidth > 0 && mTabExtraWidth > tabMinWidth )
			tabMinWidth = mTabExtraWidth;

		if ( mScrollControl && 
            pos+tabMinWidth <= mUpButton->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - 2 )
		{
			len = mUpButton->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - 2 - pos;
		}
	}
	return len;
}

bool UITabControl::NeedScrollControl(int startIndex, bool withScrollControl)
{
	if ( startIndex >= (int)mTabs.size() )
		startIndex -= 1;

	if ( startIndex < 0 )
		startIndex = 0;

    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	if (!skin)
		return false;

	std::shared_ptr<BaseUIFont> font = skin->GetFont();

	RectangleShape<2, int> frameRect(mAbsoluteRect);

	if (mTabs.empty())
		return false;

	if (!font)
		return false;

	int pos = frameRect.GetVertice(RVP_UPPERLEFT)[0] + 2;
	for (int i=startIndex; i<(int)mTabs.size(); ++i)
	{
		// get Text
		const wchar_t* text = 0;
		if (mTabs[i])
			text = mTabs[i]->GetText();

		// get text length
        // always without withScrollControl here or len would be shortened
		int len = CalcTabWidth(pos, font, text, false);

        frameRect.mExtent[0] = len;
        frameRect.mCenter[0] = pos + len / 2;

		pos += len;
		if ( withScrollControl && pos > mUpButton->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - 2)
			return true;

		if ( !withScrollControl && pos > mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[0] )
			return true;
	}

	return false;
}


RectangleShape<2, int> UITabControl::CalcTabPosition()
{
	RectangleShape<2, int> r;
    r.mExtent[0] = mAbsoluteRect.mExtent[0];
    r.mCenter[0] = r.mExtent[0] / 2;

    if (mBorder)
        r.mExtent[0] -= 2;

	if (mVerticalAlignment == UIA_UPPERLEFT )
	{
        r.mExtent[1] = mAbsoluteRect.mExtent[1] - mTabHeight - 3;
        r.mCenter[1] = mTabHeight + 2 + r.mExtent[1] / 2;
	}
	else
	{
        r.mExtent[1] = mAbsoluteRect.mExtent[1] - mTabHeight - 2;
        r.mCenter[1] = r.mExtent[1] / 2;
	}

	return r;
}


//! draws the element and its children
void UITabControl::Draw()
{
	if (!IsVisible())
		return;

    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	std::shared_ptr<BaseUIFont> font = skin->GetFont();

	RectangleShape<2, int> frameRect(mAbsoluteRect);
	if (mTabs.empty())
		skin->Draw2DRectangle(skin->GetColor(DC_3D_HIGH_LIGHT), mVisual, frameRect, &mAbsoluteClippingRect);

	if (!font)
		return;

	if ( mVerticalAlignment == UIA_UPPERLEFT )
	{
        frameRect.mCenter[1] = frameRect.GetVertice(RVP_UPPERLEFT)[1] + 2 + mTabHeight / 2;
        frameRect.mExtent[1] = mTabHeight;
	}
	else
	{
        frameRect.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1] - (int)round(mTabHeight / 2.f);
        frameRect.mExtent[1] = mTabHeight;
	}

	RectangleShape<2, int> tr;
	int pos = frameRect.GetVertice(RVP_UPPERLEFT)[0] + 2;

	bool needLeftScroll = mCurrentScrollTabIndex > 0;
	bool needRightScroll = false;

	// left and right pos of the active tab
	int left = 0;
	int right = 0;

	//const wchar_t* activetext = 0;
	std::shared_ptr<UITab> activeTab = 0;
	for (unsigned int i=mCurrentScrollTabIndex; i<mTabs.size(); ++i)
	{
		// get Text
		const wchar_t* text = 0;
		if (mTabs[i])
			text = mTabs[i]->GetText();

		// get text length
		int len = CalcTabWidth(pos, font, text, true);
		if ( mScrollControl && pos+len > mUpButton->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - 2 )
		{
			needRightScroll = true;
			break;
		}

        frameRect.mCenter[0] = pos + len / 2;
        frameRect.mExtent[0] = len;

		pos += len;

		if ((int)i == mActiveTab)
		{
			left = frameRect.GetVertice(RVP_UPPERLEFT)[0];
            right = frameRect.GetVertice(RVP_LOWERRIGHT)[0];
			//activetext = text;
			activeTab = mTabs[i];
		}
		else
		{
			skin->Draw3DTabButton(false, mVisual, frameRect, &mAbsoluteClippingRect, mVerticalAlignment);

			// draw text
            // TODO: exact size depends on borders in draw3DTabButton which we don't get with current interface
			font->Draw(text, frameRect, mTabs[i]->GetTextColor(), true, true, &frameRect);
		}
	}

    // Draw active tab button
    // Drawn later than other buttons because it draw over the buttons before/after it.
	if (left != 0 && right != 0 && activeTab != 0)
	{
		// draw upper highlight frame
		if (mVerticalAlignment == UIA_UPPERLEFT)
		{
            frameRect.mExtent[0] = right - left + 3;
            frameRect.mCenter[0] = left - 2 + frameRect.mExtent[0] / 2;
            frameRect.mExtent[1] += 2;
            frameRect.mCenter[1] -= 1;
			skin->Draw3DTabButton(true, mVisual, frameRect, &mAbsoluteClippingRect, mVerticalAlignment);

			// draw text
            // TODO: exact size depends on borders in draw3DTabButton which we don't get with current interface
			font->Draw(activeTab->GetText(), frameRect, activeTab->GetTextColor(), true, true, &frameRect);
            /*
            tr.mExtent[0] = left - 1 - mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0];
            tr.mCenter[0] = mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] + tr.mExtent[0] / 2;
            tr.mExtent[1] = 1;
            tr.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1];
			skin->Draw2DRectangle(skin->GetColor(DC_3D_HIGH_LIGHT), mVisual, tr, &mAbsoluteClippingRect);

            tr.mExtent[0] = mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[0] - right;
            tr.mCenter[0] = right + tr.mExtent[0] / 2;
			skin->Draw2DRectangle(skin->GetColor(DC_3D_HIGH_LIGHT), mVisual, tr, &mAbsoluteClippingRect);
            */
		}
		else
		{
            frameRect.mExtent[0] = right - left + 3;
            frameRect.mCenter[0] = left - 2 + frameRect.mExtent[0] / 2;
            frameRect.mExtent[1] -= 2;
            frameRect.mCenter[1] += 1;
			skin->Draw3DTabButton(true, mVisual, frameRect, &mAbsoluteClippingRect, mVerticalAlignment);

			// draw text
			font->Draw(activeTab->GetText(), frameRect, activeTab->GetTextColor(), true, true, &frameRect);
            /*
            tr.mExtent[0] = left - 1 - mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0];
            tr.mCenter[0] = mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] + tr.mExtent[0] / 2;
            tr.mExtent[1] = 1;
            tr.mCenter[1] = frameRect.GetVertice(RVP_UPPERLEFT)[1];
            skin->Draw2DRectangle(skin->GetColor(DC_3D_DARK_SHADOW), mVisual, tr, &mAbsoluteClippingRect);

            tr.mExtent[0] = mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[0] - right;
            tr.mCenter[0] = right + tr.mExtent[0] / 2;
            skin->Draw2DRectangle(skin->GetColor(DC_3D_DARK_SHADOW), mVisual, tr, &mAbsoluteClippingRect);
            */
		}
	}
	else
	{
        tr = mAbsoluteRect;
        tr.mExtent[1] = 1;
        tr.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1];

		if (mVerticalAlignment == UIA_UPPERLEFT)
		{
            skin->Draw2DRectangle(skin->GetColor(DC_3D_HIGH_LIGHT), mVisual, tr, &mAbsoluteClippingRect);
		}
		else
		{
            tr.mExtent[1] = 1;
            tr.mCenter[1] = frameRect.GetVertice(RVP_UPPERLEFT)[1];
            skin->Draw2DRectangle(skin->GetColor(DC_3D_DARK_SHADOW), mVisual, tr, &mAbsoluteClippingRect);
		}
	}

	skin->Draw3DTabBody(mBorder, mFillBackground, mVisual, 
        mAbsoluteRect, &mAbsoluteClippingRect, mTabHeight, mVerticalAlignment);

	// enable scrollcontrols on need
	if ( mUpButton )
		mUpButton->SetEnabled(needLeftScroll);
	if ( mDownButton )
		mDownButton->SetEnabled(needRightScroll);
	RefreshSprites();

	BaseUIElement::Draw();
}


//! Set the height of the tabs
void UITabControl::SetTabHeight( int height )
{
	if ( height < 0 )
		height = 0;

	mTabHeight = height;

	RecalculateScrollButtonPlacement();
	RecalculateScrollBar();
}


//! Get the height of the tabs
int UITabControl::GetTabHeight() const
{
	return mTabHeight;
}

//! set the maximal width of a tab. Per default width is 0 which means "no width restriction".
void UITabControl::SetTabMaxWidth(int width )
{
	mTabMaxWidth = width;
}

//! get the maximal width of a tab
int UITabControl::GetTabMaxWidth() const
{
	return mTabMaxWidth;
}


//! Set the extra width added to tabs on each side of the text
void UITabControl::SetTabExtraWidth( int extraWidth )
{
	if ( extraWidth < 0 )
		extraWidth = 0;

	mTabExtraWidth = extraWidth;

	RecalculateScrollBar();
}


//! Get the extra width added to tabs on each side of the text
int UITabControl::GetTabExtraWidth() const
{
	return mTabExtraWidth;
}


void UITabControl::RecalculateScrollBar()
{
	if (!mUpButton || !mDownButton)
		return;

	mScrollControl = NeedScrollControl() || mCurrentScrollTabIndex > 0;
	if (mScrollControl)
	{
		mUpButton->SetVisible( true );
		mDownButton->SetVisible( true );
	}
	else
	{
		mUpButton->SetVisible( false );
		mDownButton->SetVisible( false );
	}

	BringToFront( mUpButton );
	BringToFront( mDownButton );
}

//! Set the alignment of the tabs
void UITabControl::SetTabVerticalAlignment(UIAlignment alignment)
{
	mVerticalAlignment = alignment;

	RecalculateScrollButtonPlacement();
	RecalculateScrollBar();

	RectangleShape<2, int> r(CalcTabPosition());
	for (unsigned int i=0; i<mTabs.size(); ++i )
		mTabs[i]->SetRelativePosition(r);
}

void UITabControl::RecalculateScrollButtonPlacement()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	int buttonSize = 16;
	int buttonHeight = mTabHeight - 2;
	if ( buttonHeight < 0 )
		buttonHeight = mTabHeight;
	if (skin)
	{
		buttonSize = skin->GetSize(DS_WINDOW_BUTTON_WIDTH);
		if (buttonSize > mTabHeight)
			buttonSize = mTabHeight;
	}

	int buttonX = mRelativeRect.mExtent[0] - (int)(2.5f*(float)buttonSize) - 1;
	int buttonY = 0;

	if (mVerticalAlignment == UIA_UPPERLEFT)
	{
		buttonY = 2 + (mTabHeight / 2) - (buttonHeight / 2);
		mUpButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);
		mDownButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_UPPERLEFT);
	}
	else
	{
		buttonY = mRelativeRect.mExtent[1] - (mTabHeight / 2) - (buttonHeight / 2) - 2;
		mUpButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_LOWERRIGHT);
		mDownButton->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_LOWERRIGHT);
	}

    RectangleShape<2, int> rectangle;
    rectangle.mExtent = Vector2<int>{buttonSize, buttonHeight};
    rectangle.mCenter = Vector2<int>{ buttonX + buttonSize / 2,  buttonY + buttonHeight / 2 };
	mUpButton->SetRelativePosition(rectangle);

	buttonX += buttonSize + 1;
    rectangle.mExtent = Vector2<int>{ buttonSize, buttonHeight };
    rectangle.mCenter = Vector2<int>{ buttonX + buttonSize / 2,  buttonY + buttonHeight / 2 };
	mDownButton->SetRelativePosition(rectangle);
}

//! Get the alignment of the tabs
UIAlignment UITabControl::GetTabVerticalAlignment() const
{
	return mVerticalAlignment;
}


int UITabControl::GetTabAt(int xpos, int ypos) const
{
	RectangleShape<2, int> frameRect(mAbsoluteRect);
	if (mVerticalAlignment == UIA_UPPERLEFT)
	{
        frameRect.mCenter[1] = frameRect.GetVertice(RVP_UPPERLEFT)[1] + 2 + mTabHeight / 2;
        frameRect.mExtent[1] = mTabHeight;
	}
	else
	{
        frameRect.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1] - mTabHeight / 2;
        frameRect.mExtent[1] = mTabHeight;
	}

	int pos = frameRect.GetVertice(RVP_UPPERLEFT)[0] + 2;

    Vector2<int> p{ xpos, ypos };
	if (!frameRect.IsPointInside(p))
		return -1;

    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return - 1;

    std::shared_ptr<BaseUIFont> font = skin->GetFont();

	for (int i=mCurrentScrollTabIndex; i<(int)mTabs.size(); ++i)
	{
		// get Text
		const wchar_t* text = 0;
		if (mTabs[i])
			text = mTabs[i]->GetText();

		// get text length
		int len = CalcTabWidth(pos, font, text, true);
		if (mScrollControl && pos+len > mUpButton->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - 2)
			return -1;

        frameRect.mExtent[0] = len;
        frameRect.mCenter[0] = pos + len / 2;

		pos += len;
		if (frameRect.IsPointInside(p))
			return i;
	}
	return -1;
}

//! Returns which tab is currently active
int UITabControl::GetActiveTab() const
{
	return mActiveTab;
}


//! Brings a tab to front.
bool UITabControl::SetActiveTab(int idx)
{
	if ((unsigned int)idx >= mTabs.size())
		return false;

	bool changed = (mActiveTab != idx);

	mActiveTab = idx;

	for (int i=0; i<(int)mTabs.size(); ++i)
		if (mTabs[i])
			mTabs[i]->SetVisible( i == mActiveTab );

	if (changed)
	{
		Event event;
		event.mEventType = ET_UI_EVENT;
		event.mUIEvent.mCaller = this;
		event.mUIEvent.mElement = 0;
		event.mUIEvent.mEventType = UIEVT_TAB_CHANGED;
		mParent->OnEvent(event);
	}

	return true;
}


bool UITabControl::SetActiveTab(std::shared_ptr<BaseUITab>tab)
{
	for (int i=0; i<(int)mTabs.size(); ++i)
		if (mTabs[i] == tab)
			return SetActiveTab(i);
	return false;
}


//! Removes a child.
void UITabControl::RemoveChild(const std::shared_ptr<BaseUIElement>& child)
{
	bool isTab = false;

	unsigned int i=0;
	// check if it is a tab
	while (i<mTabs.size())
	{
		if (mTabs[i] == child)
		{
			mTabs.erase(mTabs.begin() + i);
			isTab = true;
		}
		else ++i;
	}

	// reassign numbers
	if (isTab)
		for (i=0; i<mTabs.size(); ++i)
			if (mTabs[i])
				mTabs[i]->SetNumber(i);

	// remove real element
	BaseUIElement::RemoveChild(child);

	RecalculateScrollBar();
}


//! Update the position of the element, decides scroll button status
void UITabControl::UpdateAbsolutePosition()
{
	BaseUIElement::UpdateAbsolutePosition();
	RecalculateScrollBar();
}