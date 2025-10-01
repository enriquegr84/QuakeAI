//========================================================================
// UIEngine.h : Defines UI elements of the GameEngine application
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
// The source code is managed and Maintained through Google Code: 
//    http://code.google.com/p/gamecode4/
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

#ifndef UIENGINE_H
#define UIENGINE_H

#include "GameEngineStd.h"

#include "Element/UIRoot.h"
#include "Element/UIBox.h"
#include "Element/UIFont.h"
#include "Element/UISkin.h"
#include "Element/UIButton.h"
#include "Element/UIWindow.h"
#include "Element/UIElement.h"
#include "Element/UICheckBox.h"
#include "Element/UIComboBox.h"
#include "Element/UIEditBox.h"
#include "Element/UIHyperText.h"
#include "Element/UIImage.h"
#include "Element/UIAnimatedImage.h"
#include "Element/UIListBox.h"
#include "Element/UIScrollBar.h"
#include "Element/UIScrollContainer.h"
#include "Element/UITabControl.h"
#include "Element/UITable.h"
#include "Element/UIForm.h"
#include "Element/UITabControl.h"
#include "Element/UITreeView.h"
#include "Element/UIStaticText.h"
#include "Element/UISpriteBank.h"
#include "Element/UIHyperText.h"
#include "Element/UIChatConsole.h"

#include "UIElementFactory.h"

#include "Graphic/ScreenElement.h"

enum UITextureLayer
{
    UITL_BACKGROUND = 0,
    UITL_OVERLAY,
    UITL_HEADER,
    UITL_FOOTER,
    UITL_COUNT
};

//
// class BaseUI									- Chapter 10, page 286  
//
// This was factored to create a common class that
// implements some of the BaseScreenElement class common
// to modal/modeless dialogs
//
class BaseUI : public BaseScreenElement
{

public:

	//! constructor
	BaseUI();

	//! destructor
	virtual ~BaseUI();

	virtual bool OnInit();
	virtual bool OnRestore() { return true; }
	virtual bool OnLostDevice() { return true; }

	virtual bool IsVisible() { return mVisible; }
	virtual void SetVisible(bool visible) { mVisible = visible; }

	virtual void OnUpdate(unsigned int timeMs, unsigned long deltaMs) { };

	//! draws all gui elements
	virtual bool OnRender(double time, float elapsedTime);

	virtual bool OnPostRender( unsigned int time );

	//! Posts an input event to the environment.
	/** Usually you do not have to
	use this method, it is used by the engine internally.
	\param event The event to post.
	\return True if succeeded, else false. */
	virtual bool OnMsgProc(const Event& evt);

	// Don't handle any messages
	virtual bool OnEvent( const Event& evt) { return false; }

	//! removes all elements from the environment
	virtual void Clear();

    /**
     * specify text to appear as header string
     * @param text to set
     */
    void SetTitleText(const std::wstring& text);

	//! returns the font
	virtual std::shared_ptr<BaseUIFont> GetFont(const std::wstring& fileName);

	//! add an externally loaded font
	virtual const std::shared_ptr<BaseUIFont>& AddFont(
		const std::wstring& name, const std::shared_ptr<BaseUIFont>& font);

	//! Returns the element with the focus
	virtual const std::shared_ptr<BaseUIElement>& GetFocus() const;

	//! Returns the element last known to be under the mouse
	virtual const std::shared_ptr<BaseUIElement>& GetHovered() const;

	//! Returns the root gui element.
	virtual const std::shared_ptr<BaseUIElement>& GetRootUIElement();

	//! Returns the default element factory which can create all built in elements
	virtual std::shared_ptr<UIElementFactory> GetDefaultUIElementFactory();

	//! Returns a scene node factory by index
	virtual std::shared_ptr<UIElementFactory> GetUIElementFactory(unsigned int index) const;

	//! returns the current gui skin
	virtual const std::shared_ptr<BaseUISkin>& GetSkin() const;

	//! Sets a new UI Skin
	virtual void SetSkin(const std::shared_ptr<BaseUISkin>& skin);

	//! Creates a new UI Skin based on a template.
	/** \return Returns a pointer to the created skin.
	If you no longer need the skin, you should call UISkin::drop().
	See IReferenceCounted::drop() for more information. */
	virtual std::shared_ptr<BaseUISkin> CreateSkin(UISkinThemeType type);

	//! returns default font
	virtual std::shared_ptr<BaseUIFont> GetBuiltInFont();

	//! returns the sprite bank
	virtual std::shared_ptr<BaseUISpriteBank> GetSpriteBank(const std::wstring& filename);

	//! returns the sprite bank
	virtual std::shared_ptr<BaseUISpriteBank> AddEmptySpriteBank(const std::wstring& filename);

	//! remove loaded font
	virtual void RemoveFont(const std::shared_ptr<BaseUIFont>& font);

	//! removes the hovered element
	virtual bool RemoveHovered();

	//! sets the focus to an element
	virtual bool SetFocus(std::shared_ptr<BaseUIElement> element);

	//! removes the focus from an element
	virtual bool RemoveFocus(const std::shared_ptr<BaseUIElement>& element = 0);

	//! Returns if the element has focus
	virtual bool HasFocus(
		const std::shared_ptr<BaseUIElement>& element, bool checkSubElements = false) const;

	//! Adds an element factory to the gui environment.
	/** Use this to extend the gui environment with new element types which it should be
	able to create automaticly, for example when loading data from xml files. */
	virtual void RegisterUIElementFactory(const std::shared_ptr<UIElementFactory>& factoryToAdd);

	//! Returns amount of registered scene node factories.
	virtual size_t GetRegisteredUIElementFactoryCount() const;

	//! Adds a UI Element by its name
	virtual std::shared_ptr<BaseUIElement> AddUIElement(
		UIElementType elementType, const std::shared_ptr<BaseUIElement>& parent = 0);

    //! adds an button. The returned pointer must not be dropped.
    virtual std::shared_ptr<BaseUIBox> AddBox(
        const RectangleShape<2, int>& rectangle, 
        const std::array<SColor, 4>& colors, 
        const std::array<SColor, 4>& bordercolors,
        const std::array<int, 4>& borderwidths,
        const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

	//! adds an button. The returned pointer must not be dropped.
	virtual std::shared_ptr<BaseUIButton> AddButton(const RectangleShape<2, int>& rectangle,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, const wchar_t* text = 0, 
		const wchar_t* tooltiptext = 0, bool noClip = false, bool foreGroundImage = false);

	//! adds a window. The returned pointer must not be dropped.
	virtual std::shared_ptr<BaseUIWindow> AddWindow(const RectangleShape<2, int>& rectangle, bool modal = false,
		const wchar_t* text = 0, const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

	//! adds a static text. The returned pointer must not be dropped.
	virtual std::shared_ptr<BaseUIStaticText> AddStaticText(const wchar_t* text, 
		const RectangleShape<2, int>& rectangle, bool border = false, bool wordWrap = true, 
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, bool drawBackground = false);

	//! Adds an edit box. The returned pointer must not be dropped.
    virtual std::shared_ptr<BaseUIEditBox> AddEditBox(const wchar_t* text,
        const RectangleShape<2, int>& rectangle, bool border = false, bool isEditable = true,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

	//! Adds an image element.
	virtual std::shared_ptr<BaseUIImage> AddImage(std::shared_ptr<Texture2> texture, 
        Vector2<int> pos, const std::shared_ptr<BaseUIElement>& parent = 0, 
        int id = -1, const wchar_t* text = 0, bool useAlphaChannel = true);

	//! adds an image. The returned pointer must not be dropped.
	virtual std::shared_ptr<BaseUIImage> AddImage(const RectangleShape<2, int>& rectangle,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, 
        const wchar_t* text = 0, bool useAlphaChannel = true);

    //! adds an animated image. The returned pointer must not be dropped.
    virtual std::shared_ptr<BaseUIAnimatedImage> AddAnimatedImage(const RectangleShape<2, int>& rectangle,
        const std::wstring &textureName, int frameCount, int frameDuration,
        const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

	//! adds a scrollbar. The returned pointer must not be dropped.
	virtual std::shared_ptr<BaseUIScrollBar> AddScrollBar(bool horizontal, bool autoScale, 
        const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

    //! adds a scrollcontainer. The returned pointer must not be dropped.
    virtual std::shared_ptr<BaseUIScrollContainer> AddScrollContainer(const std::string &orientation, float scrollfactor,
        const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

    //! adds a form. The returned pointer must not be dropped.
    virtual std::shared_ptr<BaseUIForm> AddForm(
        std::shared_ptr<BaseFormSource> formSrc, std::shared_ptr<TextDestination> txtDest, const std::string& formPrepend,
        const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1 );

    //! Adds a tab control to the environment.
    virtual std::shared_ptr<BaseUITabControl> AddTabControl(const RectangleShape<2, int>& rectangle,
        const std::shared_ptr<BaseUIElement>& parent = 0, bool fillbackground = false, bool border = true, int id = -1);

    //! adds a hypertext. The returned pointer must not be dropped.
    virtual std::shared_ptr<BaseUIHyperText> AddHypertext(const RectangleShape<2, int>& rectangle,
        const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, const wchar_t* text = 0);

    //! Adds a table to the environment
    virtual std::shared_ptr<BaseUITable> BaseUI::AddTable(float scaling, 
        const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent, int id);

	//! adds a checkbox
	virtual std::shared_ptr<BaseUICheckBox> AddCheckBox(bool checked, const RectangleShape<2, int>& rectangle,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, const wchar_t* text = 0);

	//! adds a list box
	virtual std::shared_ptr<BaseUIListBox> AddListBox(const RectangleShape<2, int>& rectangle,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, bool drawBackground = false);

	//! adds a tree view
	virtual std::shared_ptr<BaseUITreeView> AddTreeView(const RectangleShape<2, int>& rectangle,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1, bool drawBackground = false,
		bool scrollBarVertical = true, bool scrollBarHorizontal = false);

	//! Adds a combo box to the environment.
	virtual std::shared_ptr<BaseUIComboBox> AddComboBox(const RectangleShape<2, int>& rectangle,
		const std::shared_ptr<BaseUIElement>& parent = 0, int id = -1);

protected:

    bool DrawBackground();
    bool DrawOverlay();
    bool DrawHeader();
    bool DrawFooter();

    /**
     * load a texture for a specified layer
     * @param layer draw layer to specify texture
     * @param texturepath full path of texture to load
     */
    bool SetTexture(const std::wstring& identifier,
        const std::wstring& texturePath, bool tileTexture, unsigned int minSize);

    /** update size of header text element */
    void UpdateTitleTextSize();

    /** pointer to gui element shown at topleft corner */
    std::shared_ptr<BaseUIStaticText> mTitleText = nullptr;

    /** array containing pointers to current specified texture layers */
    struct UITexture
    {
        bool mTile;
        unsigned int mMinSize;
        std::shared_ptr<Texture2> mTexture = nullptr;
    } mTextureLayer[UITL_COUNT];


	std::shared_ptr<BaseUIElement> mRoot;
	bool mVisible;

private:

	std::shared_ptr<BaseUIElement> GetNextElement(bool reverse = false, bool group = false);
	void UpdateHoveredElement(Vector2<int> mousePos);

	struct ToolTip
	{
		std::shared_ptr<BaseUIStaticText> mElement;
		unsigned int mLastTime;
		unsigned int mEnterTime;
		unsigned int mLaunchTime;
		unsigned int mRelaunchTime;
	};

	ToolTip mToolTip;

	std::vector<std::shared_ptr<UIElementFactory>> UIElementFactoryList;

	std::map<std::wstring, std::shared_ptr<BaseUISpriteBank>> mBanks;
	std::map<std::wstring, std::shared_ptr<BaseUIFont>> mFonts;
	std::shared_ptr<BaseUIElement> mHovered;
    // subelements replaced by their parent, so you only have 'real' elements here
	std::shared_ptr<BaseUIElement> mHoveredNoSubelement;	
	std::shared_ptr<BaseUIElement> mFocus;
    unsigned int mFocusFlags;
	Vector2<int> mLastHoveredMousePos;
	std::shared_ptr<BaseUISkin> mCurrentSkin;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<Visual> mVisualLayout;
    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;
};

#endif