// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UISCROLLBAR_H
#define UISCROLLBAR_H

#include "UIElement.h"

#include "Graphic/Resource/Color.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUIButton;

//! Default scroll bar GUI element.
/** \par This element can create the following events of type EGUI_EVENT_TYPE:
\li EGET_SCROLL_BAR_CHANGED
*/
class BaseUIScrollBar : public BaseUIElement
{
public:

    enum ArrowVisibility
    {
        HIDE,
        SHOW,
        DEFAULT
    };

	//! constructor
	BaseUIScrollBar(int id, RectangleShape<2, int> rectangle)
		: BaseUIElement(UIET_SCROLL_BAR, id, rectangle) {}

	//! Initialize scrollbar
	virtual void OnInit(bool noclip = false) = 0;

	//! check wether the scrollbar is being dragged
	virtual bool IsDragging() const = 0;

	//! sets the maximum value of the scrollbar.
	virtual void SetMax(int max) = 0;

	//! gets the maximum value of the scrollbar.
	virtual int GetMax() const = 0;

	//! sets the minimum value of the scrollbar.
	virtual void SetMin(int min) = 0;
	//! gets the minimum value of the scrollbar.
	virtual int GetMin() const = 0;

	//! gets the small step value
	virtual int GetSmallStep() const = 0;

	//! Sets the small step
	/** That is the amount that the value changes by when clicking
	on the buttons or using the cursor keys. */
	virtual void SetSmallStep(int step) = 0;

	//! gets the large step value
	virtual int GetLargeStep() const = 0;

	//! Sets the large step
	/** That is the amount that the value changes by when clicking
	in the tray, or using the page up and page down keys. */
	virtual void SetLargeStep(int step) = 0;

	//! gets the current position of the scrollbar
	virtual int GetPosition() const = 0;

	//! sets the current position of the scrollbar
	virtual void SetPosition(int pos) = 0;

    //! sets the page size of the scrollbar
    virtual void SetPageSize(const int &size) = 0;

    virtual void SetArrowsVisible(ArrowVisibility visible) = 0;
};

class UIScrollBar : public BaseUIScrollBar
{
public:

	//! constructor
	UIScrollBar(BaseUI* ui, int id, RectangleShape<2, int> rectangle, bool horizontal, bool autoScale);

	//! destructor
	virtual ~UIScrollBar();

	//! initialize scrollbar
	virtual void OnInit(bool noclip = false);

	//! called if an event happened.
	virtual bool OnEvent(const Event& evt);

	//! draws the element and its children
	virtual void Draw();

	virtual void OnPostDraw(unsigned int timeMs);

	//! check wether the scrollbar is being dragged
	virtual bool IsDragging() const;

	//! gets the maximum value of the scrollbar.
	virtual int GetMax() const;

	//! sets the maximum value of the scrollbar.
	virtual void SetMax(int max);

	//! gets the minimum value of the scrollbar.
	virtual int GetMin() const;

	//! sets the minimum value of the scrollbar.
	virtual void SetMin(int min);

	//! gets the small step value
	virtual int GetSmallStep() const;

	//! sets the small step value
	virtual void SetSmallStep(int step);

	//! gets the large step value
	virtual int GetLargeStep() const;

	//! sets the large step value
	virtual void SetLargeStep(int step);

	//! gets the current position of the scrollbar
	virtual int GetPosition() const;

	//! sets the position of the scrollbar
	virtual void SetPosition(int pos);

    //! sets the page size of the scrollbar
    virtual void SetPageSize(const int &size);

    virtual void SetArrowsVisible(ArrowVisibility visible);

	//! updates the rectangle
	virtual void UpdateAbsolutePosition();

private:

	void RefreshControls();
	int GetPosFromMousePosition(const Vector2<int> &p) const;
    float Range() const { return (float)(mMaxPosition - mMinPosition); }

	std::shared_ptr<BaseUIButton> mUpButton;
	std::shared_ptr<BaseUIButton> mDownButton;

	RectangleShape<2, int> mSliderRect;

	BaseUI* mUI;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;

	std::shared_ptr<Visual> mVisualSlider;
	std::shared_ptr<VisualEffect> mEffectSlider;

    ArrowVisibility mArrowVisibility = DEFAULT;
	bool mDragging;
	bool mHorizontal;
    bool mAutoScaling;
	bool mDraggedBySlider;
	bool mTrayClicked;
	int mScrollPosition;
    int mDrawCenter;
    int mThumbSize;
	int mMinPosition;
	int mMaxPosition;
	int mSmallStep;
	int mLargeStep;
	int mDragOffset;
    int mPageSize;
    int mBorderSize;

    SColor mCurrentIconColor;
};

#endif

