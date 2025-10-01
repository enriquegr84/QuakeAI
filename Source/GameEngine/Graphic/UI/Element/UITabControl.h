// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UITABCONTROL_H
#define UITABCONTROL_H

#include "UIElement.h"

#include "Graphic/Resource/Color.h"

#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUIButton;
class BaseUIFont;

//! A tab-page, onto which other gui elements could be added.
/** BaseUITab refers to the page itself, not to the tab in the tabbar of an BaseUITabControl. */
class BaseUITab : public BaseUIElement
{
public:

    //! constructor
    BaseUITab(int id, RectangleShape<2, int> rectangle)
        : BaseUIElement(UIET_TAB, id, rectangle) {}

    //! Returns zero based index of tab if in tabcontrol.
    /** Can be accessed later BaseUITabControl::GetTab() by this number.
        Note that this number can change when other tabs are inserted or removed .
    */
    virtual int GetNumber() const = 0;

    //! sets if the tab should draw its background
    virtual void SetDrawBackground(bool draw = true) = 0;

    //! sets the color of the background, if it should be drawn.
    virtual void SetBackgroundColor(SColor c) = 0;

    //! returns true if the tab is drawing its background, false if not
    virtual bool IsDrawingBackground() const = 0;

    //! returns the color of the background
    virtual SColor GetBackgroundColor() const = 0;

    //! sets the color of the text
    virtual void SetTextColor(SColor c) = 0;

    //! gets the color of the text
    virtual SColor GetTextColor() const = 0;
};

//! A standard tab control
/** \par This element can create the following events of type EGUI_EVENT_TYPE:
\li EGET_TAB_CHANGED
*/
class BaseUITabControl : public BaseUIElement
{
public:

    //! constructor
    BaseUITabControl(int id, RectangleShape<2, int> rectangle)
        : BaseUIElement(UIET_TAB_CONTROL, id, rectangle) {}

    //! Initialize tabcontrol
    virtual void OnInit() = 0;

    //! Adds a tab
    virtual std::shared_ptr<BaseUITab> AddTab(const wchar_t* caption, int id = -1, bool isActive = false) = 0;

    //! Insert the tab at the given index
    /** \return The tab on success or NULL on failure. */
    virtual std::shared_ptr<BaseUITab> InsertTab(int idx, const wchar_t* caption, int id = -1, bool isActive = false) = 0;

    //! Removes a tab from the tabcontrol
    virtual void RemoveTab(int idx) = 0;

    //! Clears the tabcontrol removing all tabs
    virtual void Clear() = 0;

    //! Returns amount of tabs in the tabcontrol
    virtual size_t GetTabCount() const = 0;

    //! Returns a tab based on zero based index
    /** \param idx: zero based index of tab. Is a value betwenn 0 and GetTabCount()-1;
    \return Returns pointer to the Tab. Returns 0 if no tab
    is corresponding to this tab. */
    virtual std::shared_ptr<BaseUITab> GetTab(int idx) const = 0;

    //! Brings a tab to front.
    /** \param idx: number of the tab.
    \return Returns true if successful. */
    virtual bool SetActiveTab(int idx) = 0;

    //! Brings a tab to front.
    /** \param tab: pointer to the tab.
    \return Returns true if successful. */
    virtual bool SetActiveTab(std::shared_ptr<BaseUITab> tab) = 0;

    //! Returns which tab is currently active
    virtual int GetActiveTab() const = 0;

    //! get the the id of the tab at the given absolute coordinates
    /** \return The id of the tab or -1 when no tab is at those coordinates*/
    virtual int GetTabAt(int xpos, int ypos) const = 0;

    //! Set the height of the tabs
    virtual void SetTabHeight(int height) = 0;

    //! Get the height of the tabs
    /** return Returns the height of the tabs */
    virtual int GetTabHeight() const = 0;

    //! set the maximal width of a tab. Per default width is 0 which means "no width restriction".
    virtual void SetTabMaxWidth(int width) = 0;

    //! get the maximal width of a tab
    virtual int GetTabMaxWidth() const = 0;

    //! Set the alignment of the tabs
    /** Use EGUIA_UPPERLEFT or EGUIA_LOWERRIGHT */
    virtual void SetTabVerticalAlignment(UIAlignment alignment) = 0;

    //! Get the alignment of the tabs
    /** return Returns the alignment of the tabs */
    virtual UIAlignment GetTabVerticalAlignment() const = 0;

    //! Set the extra width added to tabs on each side of the text
    virtual void SetTabExtraWidth(int extraWidth) = 0;

    //! Get the extra width added to tabs on each side of the text
    /** return Returns the extra width of the tabs */
    virtual int GetTabExtraWidth() const = 0;
};

// A tab, onto which other gui elements could be added.
class UITab : public BaseUITab
{
public:

    //! constructor
    UITab(BaseUI* ui, int id, const RectangleShape<2, int>& rectangle, int number);

    //! destructor
    //virtual ~UITab();

    //! Returns number of this tab in tabcontrol. Can be accessed
    //! later BaseUITabControl::GetTab() by this number.
    virtual int GetNumber() const;

    //! Sets the number
    virtual void SetNumber(int n);

    //! draws the element and its children
    virtual void Draw();

    //! sets if the tab should draw its background
    virtual void SetDrawBackground(bool draw = true);

    //! sets the color of the background, if it should be drawn.
    virtual void SetBackgroundColor(SColor c);

    //! sets the color of the text
    virtual void SetTextColor(SColor c);

    //! returns true if the tab is drawing its background, false if not
    virtual bool IsDrawingBackground() const;

    //! returns the color of the background
    virtual SColor GetBackgroundColor() const;

    virtual SColor GetTextColor() const;

    //! only for internal use by UITabControl
    void RefreshSkinColors();

private:

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;

    BaseUI* mUI;

    int mNumber;
    SColor mBackColor;
    bool mOverrideTextColorEnabled;
    SColor mTextColor;
    bool mDrawBackground;
};


//! A standard tab control
class UITabControl : public BaseUITabControl
{
public:

    //! destructor
    UITabControl(BaseUI* ui, const RectangleShape<2, int>& rectangle,
        bool fillbackground = true, bool border = true, int id = -1);

    //! destructor
    virtual ~UITabControl();

    //! Initialize tabcontrol
    virtual void OnInit();

    //! Adds a tab
    virtual std::shared_ptr<BaseUITab> AddTab(const wchar_t* caption, int id = -1, bool isActive = false);

    //! Adds a tab that has already been created
    virtual void AddTab(std::shared_ptr<UITab> tab);

    //! Insert the tab at the given index
    virtual std::shared_ptr<BaseUITab> InsertTab(int idx, const wchar_t* caption, int id = -1, bool isActive = false);

    //! Removes a tab from the tabcontrol
    virtual void RemoveTab(int idx);

    //! Clears the tabcontrol removing all tabs
    virtual void Clear();

    //! Returns amount of tabs in the tabcontrol
    virtual size_t GetTabCount() const;

    //! Returns a tab based on zero based index
    virtual std::shared_ptr<BaseUITab> GetTab(int idx) const;

    //! Brings a tab to front.
    virtual bool SetActiveTab(int idx);

    //! Brings a tab to front.
    virtual bool SetActiveTab(std::shared_ptr<BaseUITab>tab);

    //! Returns which tab is currently active
    virtual int GetActiveTab() const;

    //! get the the id of the tab at the given absolute coordinates
    virtual int GetTabAt(int xpos, int ypos) const;

    //! called if an event happened.
    virtual bool OnEvent(const Event& evt);

    //! draws the element and its children
    virtual void Draw();

    //! Removes a child.
    virtual void RemoveChild(const std::shared_ptr<BaseUIElement>& child);

    //! Set the height of the tabs
    virtual void SetTabHeight(int height);

    //! Get the height of the tabs
    virtual int GetTabHeight() const;

    //! set the maximal width of a tab. Per default width is 0 which means "no width restriction".
    virtual void SetTabMaxWidth(int width);

    //! get the maximal width of a tab
    virtual int GetTabMaxWidth() const;

    //! Set the alignment of the tabs
    //! note: EGUIA_CENTER is not an option
    virtual void SetTabVerticalAlignment(UIAlignment alignment);

    //! Get the alignment of the tabs
    virtual UIAlignment GetTabVerticalAlignment() const;

    //! Set the extra width added to tabs on each side of the text
    virtual void SetTabExtraWidth(int extraWidth);

    //! Get the extra width added to tabs on each side of the text
    virtual int GetTabExtraWidth() const;

    //! Update the position of the element, decides scroll button status
    virtual void UpdateAbsolutePosition();

private:

    void ScrollLeft();
    void ScrollRight();
    bool NeedScrollControl(int startIndex = 0, bool withScrollControl = false);
    int CalcTabWidth(int pos, std::shared_ptr<BaseUIFont> font, const wchar_t* text, bool withScrollControl) const;
    RectangleShape<2, int> CalcTabPosition();

    void RecalculateScrollButtonPlacement();
    void RecalculateScrollBar();
    void RefreshSprites();

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;

    BaseUI* mUI;

    std::vector<std::shared_ptr<UITab>> mTabs;	// std::shared_ptr<UITab> because we need SetNumber (which is certainly not nice)
    int mActiveTab;
    bool mBorder;
    bool mFillBackground;
    bool mScrollControl;
    int mTabHeight;
    UIAlignment mVerticalAlignment;
    std::shared_ptr<BaseUIButton> mUpButton;
    std::shared_ptr<BaseUIButton> mDownButton;
    int mTabMaxWidth;
    int mCurrentScrollTabIndex;
    int mTabExtraWidth;
};

#endif