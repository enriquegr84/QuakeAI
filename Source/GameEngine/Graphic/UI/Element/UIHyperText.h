/*
Minetest
Copyright (C) 2019 EvicenceBKidscode / Pierre-Yves Rollo <dev@pyrollo.com>

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


#ifndef UIHYPERTEXT_H
#define UIHYPERTEXT_H

#include "UIElement.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUIFont;
class BaseUIScrollBar;

class ParsedText
{
public:
	ParsedText(const wchar_t *text);
	~ParsedText();

    enum ElementType
    {
        ELEMENT_TEXT,
        ELEMENT_SEPARATOR,
        ELEMENT_IMAGE,
        ELEMENT_ITEM
    };

    enum BackgroundType
    {
        BACKGROUND_NONE,
        BACKGROUND_COLOR
    };

    enum FloatType
    {
        FLOAT_NONE,
        FLOAT_RIGHT,
        FLOAT_LEFT
    };

    enum HalignType
    {
        HALIGN_CENTER,
        HALIGN_LEFT,
        HALIGN_RIGHT,
        HALIGN_JUSTIFY
    };

    enum VAlignType
    {
        VALIGN_MIDDLE,
        VALIGN_TOP,
        VALIGN_BOTTOM
    };

	typedef std::unordered_map<std::string, std::string> StyleList;
	typedef std::unordered_map<std::string, std::string> AttrsList;

	struct Tag
	{
		std::string name;
		AttrsList attrs;
		StyleList style;
	};

	struct Element
	{
		std::list<Tag*> tags;
		ElementType type;
		std::wstring text = L"";

		Vector2<int> dim;
		Vector2<int> pos;
		int drawwidth;

		FloatType floating = FLOAT_NONE;

        VAlignType valign;

		std::shared_ptr<BaseUIFont> font;

		SColor color;
		SColor hovercolor;
		bool underline;

		int baseline = 0;

		// img & item specific attributes
		std::string name;
		Vector3<int> angle{0, 0, 0};
		Vector3<int> rotation{0, 0, 0};

		int margin = 10;

		void SetStyle(StyleList &style);
	};

	struct Paragraph
	{
		std::vector<Element> elements;
		HalignType halign;
		int margin = 10;

		void SetStyle(StyleList &style);
	};

	std::vector<Paragraph> mParagraphs;

	// Element style
	int mMargin = 3;
	VAlignType mVAlign = VALIGN_TOP;
	BackgroundType mBackgroundType = BACKGROUND_NONE;
	SColor mBackgroundColor;

	Tag mRootTag;

protected:
	typedef enum { ER_NONE, ER_TAG, ER_NEWLINE } EndReason;

	// Parser functions
	void EnterElement(ElementType type);
	void EndElement();
	void EnterParagraph();
	void EndParagraph(EndReason reason);
	void PushChar(wchar_t c);
	ParsedText::Tag* NewTag(const std::string &name, const AttrsList &attrs);
	ParsedText::Tag* OpenTag(const std::string &name, const AttrsList &attrs);
	bool CloseTag(const std::string &name);
	void ParseGenericStyleAttr(const std::string &name, const std::string &value,
			StyleList &style);
	void ParseStyles(const AttrsList &attrs, StyleList &style);
	void GlobalTag(const ParsedText::AttrsList &attrs);
	unsigned int ParseTag(const wchar_t *text, unsigned int cursor);
	void Parse(const wchar_t *text);

	std::unordered_map<std::string, StyleList> mElementTags;
	std::unordered_map<std::string, StyleList> mParagraphTags;

	std::vector<Tag*> mNotRootTags;
	std::list<Tag*> mActiveTags;

	// Current values
	StyleList mStyle;
	Element* mElement;
	Paragraph* mParagraph;
	bool mEmptyParagraph;
	EndReason mEndParagraphReason;
};

class TextDrawer
{
public:
	TextDrawer(BaseUI* ui, const wchar_t *text);

	void Place(const RectangleShape<2, int>& destRect);
	inline int GetHeight() { return mHeight; };
	void Draw(const RectangleShape<2, int> &clipRect, const Vector2<int> &destOffset);
	ParsedText::Element* GetElementAt(Vector2<int> pos);
	ParsedText::Tag* mHovertag;

protected:
	struct RectangleMargin
	{
		RectangleShape<2, int> rect;
		int margin;
	};

    BaseUI* mUI;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;

	ParsedText mText;
	int mHeight;
	int mVOffset;
	std::vector<RectangleMargin> mFloating;
};


class BaseUIHyperText : public BaseUIElement
{
public:
    //! constructor
    BaseUIHyperText(int id, RectangleShape<2, int> rectangle) :
        BaseUIElement(UIET_HYPERTEXT, id, rectangle)
    {
    }
};


class UIHyperText : public BaseUIHyperText
{
public:
	//! constructor
	UIHyperText(BaseUI* ui, int id, RectangleShape<2, int> rectangle, const wchar_t *text);

	//! destructor
	virtual ~UIHyperText();

	//! draws the element and its children
	virtual void Draw();

	virtual bool OnEvent(const Event& evt);

protected:

    BaseUI* mUI;

	std::shared_ptr<BaseUIScrollBar> mVScrollbar;
	TextDrawer mDrawer;

	// Positioning
	unsigned int mScrollbarWidth;
	RectangleShape<2, int> mDisplayTextRect;
	Vector2<int> mTextScrollpos;

	ParsedText::Element* GetElementAt(int x, int y);
	void CheckHover(int x, int y);
};

#endif