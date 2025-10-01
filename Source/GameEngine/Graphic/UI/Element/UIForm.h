/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef UIFORM_H
#define UIFORM_H

#include "UITable.h"
#include "UIScrollBar.h"

#include "Core/Utility/StringUtil.h"
#include "Application/System/CursorControl.h"

#include "Graphic/UI/Style.h"

#include "Graphic/Scene/Hierarchy/Visual.h"

enum FormFieldType 
{
	FFT_BUTTON,
	FFT_TABLE,
	FFT_TABHEADER,
	FFT_CHECKBOX,
	FFT_DROPDOWN,
	FFT_SCROLLBAR,
	FFT_BOX,
	FFT_ITEMIMAGE,
	FFT_HYPERTEXT,
	FFT_ANIMATEDIMAGE,
	FFT_UNKOWN
};

enum FormQuitMode 
{
	FQM_NO,
	FQM_ACCEPT,
	FQM_CANCEL
};

class BaseUICheckBox;
class UIScrollContainer;
class BaseUIStaticText;
class BaseUIFont;

struct TextDestination
{
	virtual ~TextDestination() = default;

	// This is deprecated I guess? -celeron55
	virtual void GotText(const std::wstring& text) {}
    virtual void GotText(const StringMap& fields) {}

	std::string mFormName;
};

class BaseFormSource
{
public:
	virtual ~BaseFormSource() = default;
	virtual const std::string& GetForm() const = 0;
    virtual void SetForm(const std::string& form) = 0;

	// Fill in variables in field text
	virtual std::string ResolveText(const std::string& str) { return str; }
};


// Remember to drop() the form after creating, so that it can
// remove itself when it wants to.
class BaseUIForm : public BaseUIElement
{
public:
    BaseUIForm(BaseUI* ui, int id, RectangleShape<2, int> rectangle, bool remapDoubleClick = true);
    virtual ~BaseUIForm();

    //virtual void OnInit();
    virtual void Draw();

    virtual void OnInit();

    void AllowFocusRemoval(bool allow);
    bool CanTakeFocus(BaseUIElement* el);
    void QuitForm();

    virtual void SetForm(const std::string& form) { }
    virtual void SetFormSource(std::shared_ptr<BaseFormSource> formSrc) { }
    virtual void SetFormPrepend(const std::string& formPrepend) { }

    virtual void SetTextDestination(std::shared_ptr<TextDestination> textDst) { }

    virtual void SetFocus(const std::string& elementName) { };

    virtual void RegenerateUI(Vector2<unsigned int> screenSize) = 0;
    virtual void DrawForm() = 0;

    virtual bool OnPreEvent(const Event& evt);
    virtual bool OnEvent(const Event& evt) { return false; };

    virtual bool IsActive() { return mActive; }

protected:

    BaseUI* mUI;

    virtual std::wstring GetLabel(int id) = 0;
    virtual std::string GetName(int id) = 0;

    /**
        check if event is part of a double click
        @param event event to evaluate
        @return true/false if a doubleclick was detected
    */
    bool DoubleClickDetection(const Event& evt);

    Vector2<int> mPointer;
    Vector2<int> mOldPointer;  // Mouse position after previous mouse event
    Vector2<unsigned int> mScreenSizeOld;

    bool mActive;

private:
    struct ClickPos
    {
        Vector2<int> pos;
        unsigned int time;
    };
    ClickPos mDoubleClickDetect[2];

    //std::shared_ptr<Visual> mVisual;
    //std::shared_ptr<VisualEffect> mEffect;

    /* If true, remap a double-click (or double-tap) action to ESC. This is so
     * that, for example, Android users can double-tap to close a forms.
    *
     * This value can (currently) only be set by the class constructor
     * and the default value for the setting is true.
     */
    bool mRemapDoubleClick;
    // This might be necessary to expose to the implementation if it
    // wants to launch other Forms
    bool mAllowFocusRemoval = false;
};

class UIForm : public BaseUIForm
{
public:
	UIForm(BaseUI* ui, int id, RectangleShape<2, int> rectangle, 
        std::shared_ptr<BaseFormSource> formSrc, std::shared_ptr<TextDestination> txtDst, 
        const std::string& formPrepend, bool remapDoubleClick = true);

	~UIForm();

	virtual void SetForm(const std::string& formString)
	{
        mFormSource->SetForm(formString);
		mIsFormRegenerated = false;
	}

    // formSrc is deleted by this UIForm
    virtual void SetFormSource(std::shared_ptr<BaseFormSource> formSrc)
    {
        mFormSource = formSrc;
        mIsFormRegenerated = false;
    }

	virtual void SetFormPrepend(const std::string& formPrepend)
	{
		mFormPrepend = formPrepend;
	}

	// textDst is deleted by this UIForm
	virtual void SetTextDestination(std::shared_ptr<TextDestination> textDst)
	{
		mTextDst = textDst;
	}

	void AllowClose(bool value)
	{
		mAllowClose = value;
	}

	void LockSize(bool lock,Vector2<unsigned int> baseScreenSize=Vector2<unsigned int>::Zero())
	{
		mLock = lock;
		mLockScreenSize = baseScreenSize;
	}

	void SetInitialFocus();

	virtual void SetFocus(const std::string& elementname)
	{
		mFocusedElement = elementname;
	}

    // Remove and re-add (or reposition) stuff
	virtual void RegenerateUI(Vector2<unsigned int> ScreenSize);

	virtual void DrawForm();

	void AcceptInput(FormQuitMode quitmode=FQM_NO);
	virtual bool OnPreEvent(const Event& evt);
	virtual bool OnEvent(const Event& evt);

    void SetActive(bool active) { mActive = active; }

    int GetField(const std::string& name);
    std::wstring GetLabel(int id);
    std::string GetName(int id);

	std::shared_ptr<UITable> GetTable(const std::string& tablename);
	std::vector<std::string>* GetDropDownValues(const std::string& name);

protected:

    struct Field
    {
        Field() = default;

        Field(const std::string& name, const std::wstring& label,
            const std::wstring& default_text, int id, int mPriority = 0,
            CursorIcon cursorIcon = CI_NORMAL) :
            mName(name), mLabel(label), mDefault(UnescapeEnriched(default_text)),
            mId(id), mSend(false), mType(FFT_UNKOWN), mIsExit(false),
            mPriority(mPriority), mCursorIcon(cursorIcon)
        {
        }

        std::string mName;
        std::wstring mLabel;
        std::wstring mDefault;
        int mId;
        bool mSend;
        FormFieldType mType;
        bool mIsExit;
        // Draw mPriority for form version < 3
        int mPriority;
        RectangleShape<2, int> mRect;
        CursorIcon mCursorIcon;
        std::string mSound;
    };

    struct Tooltip
    {
        Tooltip() = default;
        Tooltip(const std::wstring& aTooltip, SColor aBGcolor, SColor aColor) :
            mTooltip(aTooltip), mBGColor(aBGcolor), mColor(aColor)
        {
        }

        std::wstring mTooltip;
        SColor mBGColor;
        SColor mColor;
    };

    class ParserData
    {
    public:
        bool mExplicitSize;
        bool mRealCoordinates;
        uint8_t mSimpleFieldCount;
        Vector2<float> mInvSize;
        Vector2<int> mSize;
        Vector2<float> mOffset;
        Vector2<float> mAnchor;
        RectangleShape<2, int> mRect;
        Vector2<int> mBasePos;
        Vector2<unsigned int> mScreenSize;
        UITable::TableOptions mTableOptions;
        UITable::TableColumns mTableColumns;
        std::shared_ptr<BaseUIElement> mCurrentParent = nullptr;

        struct
        {
            int max = 1000;
            int min = 0;
            int smallStep = 10;
            int largeStep = 100;
            int thumbSize = 1;
            BaseUIScrollBar::ArrowVisibility arrowVisiblity = BaseUIScrollBar::DEFAULT;
        } ScrollbarOptions;

        // used to restore table selection/scroll/treeview state
        std::unordered_map<std::string, UITable::DynamicData> mTableDynamicData;
    };

    struct KeyPending
    {
        bool keyUp;
        bool keyDown;
        bool keyEnter;
        bool keyEscape;
    };

    const Field* GetField(int id);

	Vector2<int> GetBasePosition() const
	{
        return mPadding + mOffset + mAbsoluteRect.GetVertice(RVP_UPPERLEFT);
	}

	Vector2<int> GetElementBasePosition(const std::vector<std::string>* vPos);
	Vector2<int> GetRealCoordinateBasePosition(const std::vector<std::string>& vPos);
	Vector2<int> GetRealCoordinateGeometry(const std::vector<std::string>& vGeom);

	Style GetDefaultStyleForElement(const std::string& type,
        const std::string& name="", const std::string& parentType="");
	std::array<Style, Style::NUM_STATES> GetStyleForElement(
        const std::string& type, const std::string& name="", const std::string& parentType="");

    bool mShowDebug = false;

    std::unordered_map<std::string, std::vector<Style>> mThemeByType;
    std::unordered_map<std::string, std::vector<Style>> mThemeByName;
    std::unordered_set<std::string> mPropertyWarned;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;

    std::string mLastFormName;
    uint16_t mFormVersion = 1;
    std::shared_ptr<BaseFormSource> mFormSource;
    std::shared_ptr<TextDestination> mTextDst;

    std::string mFormString;
    std::string mFormPrepend;

    // Default true because we can't control regeneration on resizing, but
    // we can control cases when the form is shown intentionally.
    bool mIsFormRegenerated = true;
    
    int mButtonHeight;

	Vector2<int> mPadding;
	Vector2<float> mSpacing;
	Vector2<int> mImageSize;
	Vector2<int> mOffset;
	Vector2<float> mPosOffset;
	std::stack<Vector2<float>> mContainerStack;

    std::vector<Field> mFields;
    std::map<std::string, Tooltip> mTooltips;
    std::vector<std::pair<std::shared_ptr<BaseUIElement>, Tooltip>> mTooltipRects;

	std::vector<std::shared_ptr<BaseUIElement>> mBackgrounds;
	std::unordered_map<std::string, bool> mFieldCloseOnEnter;
	std::unordered_map<std::string, bool> mDropdownIndexEvent;

	std::vector<std::pair<Field, std::shared_ptr<UITable>>> mTables;
	std::vector<std::pair<Field, std::shared_ptr<BaseUICheckBox>>> mCheckboxes;
	std::vector<std::pair<Field, std::vector<std::string>>> mDropdowns;
	std::vector<std::shared_ptr<BaseUIElement>> mClickThroughElements;
    std::vector<std::pair<Field, std::shared_ptr<UIScrollBar>>> mScrollbars;
    std::vector<std::pair<std::string, std::shared_ptr<UIScrollContainer>>> mScrollContainers;

	std::shared_ptr<BaseUIStaticText> mTooltipElement = nullptr;
	unsigned int mTooltipShowDelay;
	unsigned int mHoveredTime = 0;
	int mOldTooltipId = -1;

	bool mAllowClose = true;
	bool mLock = false;
	Vector2<unsigned int> mLockScreenSize;

	bool mBGNonFullscreen;
	bool mBGFullscreen;
	SColor mBGColor;
	SColor mFullscreenBGColor;
	SColor mDefaultTooltipBGColor;
	SColor mDefaultTooltipColor;

    std::shared_ptr<BaseUIFont> mFont = nullptr;
    std::string mFocusedElement = "";

    KeyPending mCurrentKeysPending;
    std::string mCurrentFieldEnterPending = "";

    virtual void ParseElement(ParserData* data, const std::string& element);
    virtual void ParseList(ParserData* data, const std::string& element);
    virtual void ParseImage(ParserData* data, const std::string& element);
    virtual void ParseBackground(ParserData* data, const std::string& element);

    void ParseSize(ParserData* data, const std::string& element);
    void ParseContainer(ParserData* data, const std::string& element);
    void ParseContainerEnd(ParserData* data);
    void ParseScrollContainer(ParserData *data, const std::string& element);
    void ParseScrollContainerEnd(ParserData *data);
    void ParseCheckbox(ParserData* data, const std::string& element);
    void ParseAnimatedImage(ParserData *data, const std::string& element);
    void ParseButton(ParserData* data, const std::string& element, const std::string& typ);
    void ParseTableOptions(ParserData* data, const std::string& element);
    void ParseTableColumns(ParserData* data, const std::string& element);
    void ParseTable(ParserData* data, const std::string& element);
    void ParseTextList(ParserData* data, const std::string& element);
    void ParseDropDown(ParserData* data, const std::string& element);
    void ParseFieldCloseOnEnter(ParserData *data, const std::string& element);
    void ParsePwdField(ParserData* data, const std::string& element);
    void ParseField(ParserData* data, const std::string& element, const std::string& type);
    void CreateTextField(ParserData *data, Field& field, RectangleShape<2, int> &rect, bool isMultiline);
    void ParseSimpleField(ParserData* data, std::vector<std::string>& parts);
    void ParseTextArea(ParserData* data, std::vector<std::string>& parts, const std::string& type);
    void ParseHyperText(ParserData *data, const std::string& element);
    void ParseLabel(ParserData* data, const std::string& element);
    void ParseVertLabel(ParserData* data, const std::string& element);
    void ParseImageButton(ParserData* data, const std::string& element, const std::string& type);
    void ParseTabHeader(ParserData* data, const std::string& element);
    void ParseBox(ParserData* data, const std::string& element);
    void ParseBackgroundColor(ParserData* data, const std::string& element);
    void ParseTooltip(ParserData* data, const std::string& element);
    bool ParseVersionDirect(const std::string& data);
    bool ParseSizeDirect(ParserData* data, const std::string& element);
    void ParseScrollBar(ParserData* data, const std::string& element);
    void ParseScrollBarOptions(ParserData *data, const std::string& element);
    bool ParsePositionDirect(ParserData *data, const std::string& element);
    void ParsePosition(ParserData *data, const std::string& element);
    bool ParseAnchorDirect(ParserData *data, const std::string& element);
    void ParseAnchor(ParserData *data, const std::string& element);
    bool ParseStyle(ParserData *data, const std::string& element, bool style_type);
    void ParseSetFocus(const std::string& element);
    
    /**
    * In form version < 2 the elements were not ordered properly. Some element
    * types were drawn before others.
    * This function sorts the elements in the old order for backwards compatibility.
    */
    void LegacySortElements(std::list<std::shared_ptr<BaseUIElement>>::iterator from);

    void ShowTooltip(const std::wstring& text, const SColor &color, const SColor &bgcolor);

    void TryClose();
};

class FormSource: public BaseFormSource
{
public:
    FormSource(const std::string& form): mForm(form)
	{
	}

	~FormSource() = default;

	void SetForm(const std::string& form)
	{
		mForm = form;
	}

	const std::string& GetForm() const
	{
		return mForm;
	}

	std::string mForm;
};

#endif