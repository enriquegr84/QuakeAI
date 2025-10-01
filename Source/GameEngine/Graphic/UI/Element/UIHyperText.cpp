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

#include "UIHyperText.h"

#include "Graphic/Renderer/Renderer.h"

#include "Application/System/System.h"

#include "Core/Utility/StringUtil.h"

bool CheckColor(const std::string &str)
{
	SColor color;
	return ParseColorString(str, color, false);
}

bool CheckInteger(const std::string &str)
{
	if (str.empty())
		return false;

	char *endptr = nullptr;
	strtol(str.c_str(), &endptr, 10);

	return *endptr == '\0';
}

// -----------------------------------------------------------------------------
// ParsedText - A text parser

void ParsedText::Element::SetStyle(StyleList &style)
{
	this->underline = IsYes(style["underline"]);

	SColor color;
	if (ParseColorString(style["color"], color, false))
		this->color = color;
	if (ParseColorString(style["hovercolor"], color, false))
		this->hovercolor = color;
    /*
	unsigned int font_size = std::atoi(style["fontsize"].c_str());
	FontMode font_mode = FM_Standard;
	if (style["fontstyle"] == "mono")
		font_mode = FM_Mono;

	FontSpec spec(font_size, font_mode,
		IsYes(style["bold"]), IsYes(style["italic"]));

	// TODO: find a way to check font validity
	// Build a new fontengine ?
	this->font = g_fontengine->getFont(spec);

	if (!this->font)
		printf("No font found ! Size=%d, mode=%d, bold=%s, italic=%s\n",
				font_size, font_mode, style["bold"].c_str(),
				style["italic"].c_str());
    */
}

void ParsedText::Paragraph::SetStyle(StyleList &style)
{
	if (style["halign"] == "center")
		this->halign = HALIGN_CENTER;
	else if (style["halign"] == "right")
		this->halign = HALIGN_RIGHT;
	else if (style["halign"] == "justify")
		this->halign = HALIGN_JUSTIFY;
	else
		this->halign = HALIGN_LEFT;
}

ParsedText::ParsedText(const wchar_t *text)
{
	// Default style
	mRootTag.name = "root";
	mRootTag.style["fontsize"] = "16";
	mRootTag.style["fontstyle"] = "normal";
	mRootTag.style["bold"] = "false";
	mRootTag.style["italic"] = "false";
	mRootTag.style["underline"] = "false";
	mRootTag.style["halign"] = "left";
	mRootTag.style["color"] = "#EEEEEE";
	mRootTag.style["hovercolor"] = "#FF0000";

	mActiveTags.push_front(&mRootTag);
	mStyle = mRootTag.style;

	// Default simple tags definitions
	StyleList style;

	style["color"] = "#0000FF";
	style["underline"] = "true";
	mElementTags["action"] = style;
	style.clear();

	style["bold"] = "true";
	mElementTags["b"] = style;
	style.clear();

	style["italic"] = "true";
	mElementTags["i"] = style;
	style.clear();

	style["underline"] = "true";
	mElementTags["u"] = style;
	style.clear();

	style["fontstyle"] = "mono";
	mElementTags["mono"] = style;
	style.clear();

	style["fontsize"] = mRootTag.style["fontsize"];
	mElementTags["normal"] = style;
	style.clear();

	style["fontsize"] = "24";
	mElementTags["big"] = style;
	style.clear();

	style["fontsize"] = "36";
	mElementTags["bigger"] = style;
	style.clear();

	style["halign"] = "center";
	mParagraphTags["center"] = style;
	style.clear();

	style["halign"] = "justify";
	mParagraphTags["justify"] = style;
	style.clear();

	style["halign"] = "left";
	mParagraphTags["left"] = style;
	style.clear();

	style["halign"] = "right";
	mParagraphTags["right"] = style;
	style.clear();

	mElement = NULL;
	mParagraph = NULL;
	mEndParagraphReason = ER_NONE;

	Parse(text);
}

ParsedText::~ParsedText()
{
	for (auto& tag : mNotRootTags)
		delete tag;
}

void ParsedText::Parse(const wchar_t *text)
{
	wchar_t c;
	unsigned int cursor = 0;
	bool escape = false;

	while ((c = text[cursor]) != L'\0') 
    {
		cursor++;

		if (c == L'\r') 
        { // Mac or Windows breaks
			if (text[cursor] == L'\n')
				cursor++;
			// If text has begun, don't skip empty line
			if (mParagraph) 
            {
				EndParagraph(ER_NEWLINE);
				EnterElement(ELEMENT_SEPARATOR);
			}
			escape = false;
			continue;
		}

		if (c == L'\n') 
        { // Unix breaks
			// If text has begun, don't skip empty line
			if (mParagraph) 
            {
				EndParagraph(ER_NEWLINE);
				EnterElement(ELEMENT_SEPARATOR);
			}
			escape = false;
			continue;
		}

		if (escape) 
        {
			escape = false;
			PushChar(c);
			continue;
		}

		if (c == L'\\') 
        {
			escape = true;
			continue;
		}

		// Tag check
		if (c == L'<') 
        {
			unsigned int newcursor = ParseTag(text, cursor);
			if (newcursor > 0) 
            {
				cursor = newcursor;
				continue;
			}
		}

		// Default behavior
		PushChar(c);
	}

	EndParagraph(ER_NONE);
}

void ParsedText::EndElement()
{
	mElement = NULL;
}

void ParsedText::EndParagraph(EndReason reason)
{
	if (!mParagraph)
		return;

	EndReason previous = mEndParagraphReason;
	mEndParagraphReason = reason;
	if (mEmptyParagraph && 
        (reason == ER_TAG || (reason == ER_NEWLINE && previous == ER_TAG))) 
    {
		// Ignore last empty paragraph
		mParagraph = nullptr;
		mParagraphs.pop_back();
		return;
	}
	EndElement();
	mParagraph = NULL;
}

void ParsedText::EnterParagraph()
{
	if (!mParagraph) 
    {
		mParagraphs.emplace_back();
		mParagraph = &mParagraphs.back();
		mParagraph->SetStyle(mStyle);
		mEmptyParagraph = true;
	}
}

void ParsedText::EnterElement(ElementType type)
{
	EnterParagraph();

	if (!mElement || mElement->type != type) 
    {
		mParagraph->elements.emplace_back();
		mElement = &mParagraph->elements.back();
		mElement->type = type;
		mElement->tags = mActiveTags;
		mElement->SetStyle(mStyle);
	}
}

void ParsedText::PushChar(wchar_t c)
{
	// New word if needed
	if (c == L' ' || c == L'\t') 
    {
		if (!mEmptyParagraph)
			EnterElement(ELEMENT_SEPARATOR);
		else
			return;
	} 
    else 
    {
		mEmptyParagraph = false;
		EnterElement(ELEMENT_TEXT);
	}
	mElement->text += c;
}

ParsedText::Tag *ParsedText::NewTag(const std::string &name, const AttrsList &attrs)
{
	EndElement();
	Tag* newTag = new Tag();
    newTag->name = name;
    newTag->attrs = attrs;
	mNotRootTags.push_back(newTag);
	return newTag;
}

ParsedText::Tag *ParsedText::OpenTag(const std::string &name, const AttrsList &attrs)
{
	Tag* newTag = NewTag(name, attrs);
	mActiveTags.push_front(newTag);
	return newTag;
}

bool ParsedText::CloseTag(const std::string &name)
{
	bool found = false;
    for (auto id = mActiveTags.begin(); id != mActiveTags.end(); ++id)
    {
        if ((*id)->name == name) 
        {
            mActiveTags.erase(id);
            found = true;
            break;
        }
    }
	return found;
}

void ParsedText::ParseGenericStyleAttr(
		const std::string &name, const std::string &value, StyleList &style)
{
	// Color styles
	if (name == "color" || name == "hovercolor") 
    {
		if (CheckColor(value))
			style[name] = value;
		// Boolean styles
	} 
    else if (name == "bold" || name == "italic" || name == "underline") 
    {
		style[name] = IsYes(value);
	} 
    else if (name == "size") 
    {
		if (CheckInteger(value))
			style["fontsize"] = value;
	} 
    else if (name == "font") 
    {
		if (value == "mono" || value == "normal")
			style["fontstyle"] = value;
	}
}

void ParsedText::ParseStyles(const AttrsList &attrs, StyleList &style)
{
	for (auto const &attr : attrs)
		ParseGenericStyleAttr(attr.first, attr.second, style);
}

void ParsedText::GlobalTag(const AttrsList &attrs)
{
	for (const auto &attr : attrs) 
    {
		// Only page level style
		if (attr.first == "margin") 
        {
			if (CheckInteger(attr.second))
				mMargin = atoi(attr.second.c_str());
		} 
        else if (attr.first == "valign") 
        {
			if (attr.second == "top")
				mVAlign = ParsedText::VALIGN_TOP;
			else if (attr.second == "bottom")
				mVAlign = ParsedText::VALIGN_BOTTOM;
			else if (attr.second == "middle")
				mVAlign = ParsedText::VALIGN_MIDDLE;
		} 
        else if (attr.first == "background") 
        {
			SColor color;
			if (attr.second == "none") 
            {
				mBackgroundType = BACKGROUND_NONE;
			} 
            else if (ParseColorString(attr.second, color, false)) 
            {
				mBackgroundType = BACKGROUND_COLOR;
				mBackgroundColor = color;
			}

			// Inheriting styles
		} 
        else if (attr.first == "halign") 
        {
			if (attr.second == "left" || attr.second == "center" ||
                attr.second == "right" ||attr.second == "justify")
				mRootTag.style["halign"] = attr.second;

			// Generic default styles
		} 
        else 
        {
			ParseGenericStyleAttr(attr.first, attr.second, mRootTag.style);
		}
	}
}

unsigned int ParsedText::ParseTag(const wchar_t *text, unsigned int cursor)
{
	// Tag name
	bool end = false;
	std::string name = "";
	wchar_t c = text[cursor];

	if (c == L'/') 
    {
		end = true;
		c = text[++cursor];
		if (c == L'\0')
			return 0;
	}

	while (c != ' ' && c != '>') 
    {
		name += (char)c;
		c = text[++cursor];
		if (c == L'\0')
			return 0;
	}

	// Tag attributes
	AttrsList attrs;
	while (c != L'>') 
    {
		std::string attrName = "";
		std::wstring attrVal = L"";

		while (c == ' ') 
        {
			c = text[++cursor];
			if (c == L'\0' || c == L'=')
				return 0;
		}

		while (c != L' ' && c != L'=') 
        {
			attrName += (char)c;
			c = text[++cursor];
			if (c == L'\0' || c == L'>')
				return 0;
		}

		while (c == L' ') 
        {
			c = text[++cursor];
			if (c == L'\0' || c == L'>')
				return 0;
		}

		if (c != L'=')
			return 0;

		c = text[++cursor];

		if (c == L'\0')
			return 0;

		while (c != L'>' && c != L' ') {
			attrVal += c;
			c = text[++cursor];
			if (c == L'\0')
				return 0;
		}

		attrs[attrName] = ToString(attrVal);
	}

	++cursor; // Last ">"

	// Tag specific processing
	StyleList style;

	if (name == "global") 
    {
		if (end)
			return 0;
		GlobalTag(attrs);

	} 
    else if (name == "style") 
    {
		if (end) 
        {
			CloseTag(name);
		} 
        else 
        {
			ParseStyles(attrs, style);
			OpenTag(name, attrs)->style = style;
		}
		EndElement();
	} 
    else if (name == "img" || name == "item") 
    {
		if (end)
			return 0;

		// Name is a required attribute
		if (!attrs.count("name"))
			return 0;

		// Rotate attribute is only for <item>
		if (attrs.count("rotate") && name != "item")
			return 0;

		// Angle attribute is only for <item>
		if (attrs.count("angle") && name != "item")
			return 0;

		// Ok, element can be created
		NewTag(name, attrs);

		if (name == "img")
			EnterElement(ELEMENT_IMAGE);
		else
			EnterElement(ELEMENT_ITEM);

		mElement->text = ToWideString(attrs["name"]);

		if (attrs.count("float")) 
        {
			if (attrs["float"] == "left")
				mElement->floating = FLOAT_LEFT;
			if (attrs["float"] == "right")
				mElement->floating = FLOAT_RIGHT;
		}

		if (attrs.count("width"))
        {
			int width = atoi(attrs["width"].c_str());
			if (width > 0)
				mElement->dim[0] = width;
		}

		if (attrs.count("height")) 
        {
			int height = atoi(attrs["height"].c_str());
			if (height > 0)
				mElement->dim[1] = height;
		}

		if (attrs.count("angle")) 
        {
			std::string str = attrs["angle"];
			std::vector<std::string> parts = Split(str, ',');
			if (parts.size() == 3) 
            {
                mElement->angle = Vector3<int>{
                    std::clamp(atoi(parts[0].c_str()), -180, 180),
                    std::clamp(atoi(parts[1].c_str()), -180, 180),
                    std::clamp(atoi(parts[2].c_str()), -180, 180) };
				mElement->rotation.MakeZero();
			}
		}

		if (attrs.count("rotate")) 
        {
			if (attrs["rotate"] == "yes") 
            {
                mElement->rotation = Vector3<int>{ 0, 100, 0 };
			} 
            else 
            {
				std::string str = attrs["rotate"];
				std::vector<std::string> parts = Split(str, ',');
				if (parts.size() == 3) 
                {
                    mElement->rotation = Vector3<int>{
                        std::clamp(atoi(parts[0].c_str()), -1000, 1000),
                        std::clamp(atoi(parts[1].c_str()), -1000, 1000),
                        std::clamp(atoi(parts[2].c_str()), -1000, 1000) };
				}
			}
		}

		EndElement();

	} else if (name == "tag") 
    {
		// Required attributes
		if (!attrs.count("name"))
			return 0;

		StyleList tagstyle;
		ParseStyles(attrs, tagstyle);

		if (IsYes(attrs["paragraph"]))
			mParagraphTags[attrs["name"]] = tagstyle;
		else
			mElementTags[attrs["name"]] = tagstyle;

	} 
    else if (name == "action") 
{
		if (end) 
        {
			CloseTag(name);
		} 
        else 
        {
			if (!attrs.count("name"))
				return 0;
			OpenTag(name, attrs)->style = mElementTags["action"];
		}

	} 
    else if (mElementTags.count(name))
    {
		if (end) 
        {
			CloseTag(name);
		} 
        else 
        {
			OpenTag(name, attrs)->style = mElementTags[name];
		}
		EndElement();

	} else if (mParagraphTags.count(name)) 
    {
		if (end) 
        {
			CloseTag(name);
		} 
        else 
        {
			OpenTag(name, attrs)->style = mParagraphTags[name];
		}
		EndParagraph(ER_TAG);

	} else return 0; // Unknown tag

	// Update styles accordingly
	mStyle.clear();
	for (auto tag = mActiveTags.crbegin(); tag != mActiveTags.crend(); ++tag)
		for (const auto &prop : (*tag)->style)
			mStyle[prop.first] = prop.second;

	return cursor;
}

// -----------------------------------------------------------------------------
// Text Drawer

TextDrawer::TextDrawer(BaseUI* ui, const wchar_t *text) :
		mUI(ui), mText(text)
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

    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);

	// Size all elements
	for (auto& paragraph : mText.mParagraphs) 
    {
		for (auto& element : paragraph.elements) 
        {
			switch (element.type) 
            {
			    case ParsedText::ELEMENT_SEPARATOR:
			    case ParsedText::ELEMENT_TEXT:
				    if (element.font) 
                    {
					    element.dim[0] = element.font->GetDimension(element.text)[0];
					    element.dim[1] = element.font->GetDimension(L"Yy")[1];
				    } 
                    else 
                    {
					    element.dim = {0, 0};
				    }
				    break;

			    case ParsedText::ELEMENT_IMAGE:
			    case ParsedText::ELEMENT_ITEM:
				    // Resize only non sized items
				    if (element.dim[0] != 0 && element.dim[1] != 0)
					    break;

				    // Default image and item size
                    Vector2<int> dim{ 80, 80 };

				    if (element.type == ParsedText::ELEMENT_IMAGE) 
                    {
                        std::shared_ptr<ResHandle> resHandle =
                            ResCache::Get()->GetHandle(&BaseResource(element.text));
                        std::shared_ptr<ImageResourceExtraData> resData =
                            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
                        std::shared_ptr<Texture2> texture = resData->GetImage();
                        if (texture)
                        {
                            dim[0] = texture->GetDimension(0);
                            dim[1] = texture->GetDimension(1);
                        }
				    }

                    if (element.dim[1] == 0)
                    {
                        if (element.dim[0] == 0)
                            element.dim = dim;
                        else
                            element.dim[1] = dim[1] * element.dim[0] / dim[0];
                    }
				    else
					    element.dim[0] = dim[0] * element.dim[1] / dim[1];
				    break;
			}
		}
	}
}

// Get element at given coordinates. Coordinates are inner coordinates (starting
// at 0,0).
ParsedText::Element *TextDrawer::GetElementAt(Vector2<int> pos)
{
	pos[1] -= mVOffset;
	for (auto& paragraph : mText.mParagraphs) 
    {
		for (auto& element : paragraph.elements) 
        {
            RectangleShape<2, int> rect;
            rect.mExtent = element.dim;
            rect.mCenter = element.pos + element.dim / 2;
			if (rect.IsPointInside(pos))
				return &element;
		}
	}
	return 0;
}

/*
   This function places all elements according to given width. Elements have
   been previously sized by constructor and will be later drawed by draw.
   It may be called each time width changes and resulting height can be
   retrieved using getHeight. See UIHyperText constructor, it uses it once to
   test if text fits in window and eventually another time if width is reduced
   mFloating because of scrollbar added.
*/
void TextDrawer::Place(const RectangleShape<2, int> &destRect)
{
	mFloating.clear();
	int y = 0;
	int yMargin = mText.mMargin;

	// Iterator used :
	// paragraph - Current paragraph, walked only once
	// element - Current element, walked only once
	// element and floating - local element and floating operators
	for (auto& paragraph : mText.mParagraphs) 
    {
		// Find and place floating stuff in paragraph
		for (auto element = paragraph.elements.begin(); element != paragraph.elements.end(); ++element) 
        {
			if (element->floating != ParsedText::FLOAT_NONE) 
            {
				if (y)
					element->pos[1] = y + std::max(yMargin, element->margin);
				else
					element->pos[1] = yMargin;

				if (element->floating == ParsedText::FLOAT_LEFT)
					element->pos[0] = mText.mMargin;
				if (element->floating == ParsedText::FLOAT_RIGHT)
					element->pos[0] = destRect.mExtent[0] - element->dim[0] - mText.mMargin;

				RectangleMargin floating;
                floating.rect.mExtent = element->dim;
                floating.rect.mCenter = element->pos + element->dim / 2;
				floating.margin = element->margin;

				mFloating.push_back(floating);
			}
		}

		if (y)
			y = y + std::max(yMargin, paragraph.margin);

		yMargin = paragraph.margin;

		// Place non floating stuff
		std::vector<ParsedText::Element>::iterator el = paragraph.elements.begin();

		while (el != paragraph.elements.end())
        {
			// Determine line width and y pos
			int left, right;
			int nexty = y;
			do 
            {
				y = nexty;
				nexty = 0;

				// Inner left & right
				left = mText.mMargin;
				right = destRect.mExtent[0] - mText.mMargin;

				for (const auto &floating : mFloating) 
                {
					// Does floating rect intersect paragraph y line?
					if (floating.rect.GetVertice(RVP_UPPERLEFT)[1] - floating.margin <= y &&
                        floating.rect.GetVertice(RVP_LOWERRIGHT)[1] + floating.margin >= y) 
                    {
						// Next Y to try if no room left
						if (!nexty || floating.rect.GetVertice(RVP_LOWERRIGHT)[1] +
								std::max(floating.margin, paragraph.margin) < nexty) 
                        {
							nexty = floating.rect.GetVertice(RVP_LOWERRIGHT)[1] +
									std::max(floating.margin, paragraph.margin) + 1;
						}

						if (floating.rect.GetVertice(RVP_LOWERRIGHT)[0] - floating.margin <= left &&
                            floating.rect.GetVertice(RVP_LOWERRIGHT)[0] + floating.margin < right)
                        {
							// float on left
							if (floating.rect.GetVertice(RVP_LOWERRIGHT)[0] +
                                std::max(floating.margin, paragraph.margin) > left) 
                            {
								left = floating.rect.GetVertice(RVP_LOWERRIGHT)[0] +
										std::max(floating.margin, paragraph.margin);
							}
						} 
                        else if (floating.rect.GetVertice(RVP_LOWERRIGHT)[0] + floating.margin >= right &&
                                floating.rect.GetVertice(RVP_UPPERLEFT)[0] - floating.margin > left)
                        {
							// float on right
                            if (floating.rect.GetVertice(RVP_UPPERLEFT)[0] -
                                std::max(floating.margin, paragraph.margin) < right)
                            {
                                right = floating.rect.GetVertice(RVP_UPPERLEFT)[0] -
                                    std::max(floating.margin, paragraph.margin);
                            }
						} 
                        else if (floating.rect.GetVertice(RVP_UPPERLEFT)[0] - floating.margin <= left &&
							 	floating.rect.GetVertice(RVP_LOWERRIGHT)[0] + floating.margin >= right)
                        {
							// float taking all space
							left = right;
						}
						else
						{ 
                            // float in the middle -- should not occure yet, see that later
						}
					}
				}
			} while (nexty && right <= left);

			unsigned int lineWidth = right - left;
			float x = (float)left;

			int charsHeight = 0;
			int charsWidth = 0;
			int wordCount = 0;

			// Skip begining of line separators but include them in height
			// computation.
			while (el != paragraph.elements.end() &&
					el->type == ParsedText::ELEMENT_SEPARATOR) 
            {
				if (el->floating == ParsedText::FLOAT_NONE)
                {
					el->drawwidth = 0;
					if (charsHeight < el->dim[1])
						charsHeight = el->dim[1];
				}
				el++;
			}

			std::vector<ParsedText::Element>::iterator lineStart = el;
			std::vector<ParsedText::Element>::iterator lineEnd = paragraph.elements.end();

			// First pass, find elements fitting into line
			// (or at least one element)
			while (el != paragraph.elements.end() && 
                (charsWidth == 0 || charsWidth + el->dim[0] <= lineWidth)) 
            {
				if (el->floating == ParsedText::FLOAT_NONE) 
                {
					if (el->type != ParsedText::ELEMENT_SEPARATOR) 
                    {
						lineEnd = el;
						wordCount++;
					}
					charsWidth += el->dim[0];
					if (charsHeight < el->dim[1])
						charsHeight = el->dim[1];
				}
				el++;
			}

			// Empty line, nothing to place only go down line height
			if (lineEnd == paragraph.elements.end())
            {
				y += charsHeight;
				continue;
			}

			// Point to the first position outside line (may be end())
			lineEnd++;

			// Second pass, compute printable line width and adjustments
			charsWidth = 0;
			int top = 0;
			int bottom = 0;
			for (auto line = lineStart; line != lineEnd; ++line) 
            {
				if (line->floating == ParsedText::FLOAT_NONE) 
                {
					charsWidth += line->dim[0];
					if (top < (int)line->dim[1] - line->baseline)
						top = line->dim[1] - line->baseline;
					if (bottom < line->baseline)
						bottom = line->baseline;
				}
			}

			float extraspace = 0.f;

			switch (paragraph.halign) 
            {
			    case ParsedText::HALIGN_CENTER:
				    x += (lineWidth - charsWidth) / 2.f;
				    break;
			    case ParsedText::HALIGN_JUSTIFY:
				    if (wordCount > 1 && // Justification only if at least two words
					    !(lineEnd == paragraph.elements.end())) // Don't justify last line
					    extraspace = ((float)(lineWidth - charsWidth)) / (wordCount - 1);
				    break;
			    case ParsedText::HALIGN_RIGHT:
				    x += lineWidth - charsWidth;
				    break;
			    case ParsedText::HALIGN_LEFT:
				    break;
			}

			// Third pass, actually place everything
			for (auto line = lineStart; line != lineEnd; ++line) 
            {
				if (line->floating != ParsedText::FLOAT_NONE)
					continue;

				line->pos[0] = (int)x;
				line->pos[1] = (int)y;
				switch (line->type) 
                {
				    case ParsedText::ELEMENT_TEXT:
				    case ParsedText::ELEMENT_SEPARATOR:
					    line->pos[0] = (int)x;

					    // Align char baselines
					    line->pos[1] = (int)(y + top + line->baseline - line->dim[1]);

					    x += line->dim[0];
					    if (line->type == ParsedText::ELEMENT_SEPARATOR)
						    x += extraspace;
					    break;

				    case ParsedText::ELEMENT_IMAGE:
				    case ParsedText::ELEMENT_ITEM:
					    x += line->dim[0];
					    break;
				}

				// Draw width for separator can be different than element
				// width. This will be important for char effects like
				// underline.
				line->drawwidth = (int)(x - line->pos[0]);
			}
			y += charsHeight;
		} // Elements (actually lines)
	} // Paragraph

	// Check if float goes under paragraph
	for (const auto &f : mFloating) 
		if (f.rect.GetVertice(RVP_LOWERRIGHT)[1] >= y)
			y = f.rect.GetVertice(RVP_LOWERRIGHT)[1];

	mHeight = y + mText.mMargin;
	// Compute vertical offset according to vertical alignment
    if (mHeight < destRect.mExtent[1])
    {
        switch (mText.mVAlign)
        {
            case ParsedText::VALIGN_BOTTOM:
                mVOffset = destRect.mExtent[1] - mHeight;
                break;
            case ParsedText::VALIGN_MIDDLE:
                mVOffset = (destRect.mExtent[1] - mHeight) / 2;
                break;
            case ParsedText::VALIGN_TOP:
            default:
                mVOffset = 0;
        }
    }
	else mVOffset = 0;
}

// Draw text in a rectangle with a given offset. Items are actually placed in
// relative (to upper left corner) coordinates.
void TextDrawer::Draw(const RectangleShape<2, int> &clipRect, const Vector2<int> &destOffset)
{
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();

	Vector2<int> offset = destOffset;
	offset[1] += mVOffset;

    if (mText.mBackgroundType == ParsedText::BACKGROUND_COLOR)
    {
        Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

        RectangleShape<2, int> clip;
        clip.mExtent = Vector2<int>{ (int)screenSize[0], (int)screenSize[1] };
        clip.mCenter = clip.mExtent / 2;
        skin->Draw2DRectangle(mText.mBackgroundColor, mVisual, clipRect, &clip);
    }

	for (auto &paragraph : mText.mParagraphs) 
    {
		for (auto &element : paragraph.elements) 
        {
            RectangleShape<2, int> rect;
            rect.mExtent = element.dim;
            rect.mCenter = element.pos + offset + element.dim / 2;
			if (!rect.IsColliding(clipRect))
				continue;

			switch (element.type) 
            {
			    case ParsedText::ELEMENT_SEPARATOR:
			    case ParsedText::ELEMENT_TEXT: 
                {
				    SColor color = element.color;

				    for (auto tag : element.tags)
					    if (&(*tag) == mHovertag)
						    color = element.hovercolor;

				    if (!element.font)
					    break;

				    if (element.type == ParsedText::ELEMENT_TEXT)
					    element.font->Draw(element.text, rect, color, false, true, &clipRect);

				    if (element.underline &&  element.drawwidth) 
                    {
					    int linepos = element.pos[1] + offset[1] +
                            element.dim[1] - (element.baseline >> 1);

                        RectangleShape<2, int> linerect;
                        linerect.mExtent[0] = element.drawwidth;
                        linerect.mCenter[0] = element.pos[0] + offset[0] + element.drawwidth / 2;
                        linerect.mExtent[1] = 2 * (linepos + (element.baseline >> 3));
                        linerect.mCenter[1] = 0;
					    skin->Draw2DRectangle(color, mVisual, linerect, &clipRect);
				    }
			    } break;

			    case ParsedText::ELEMENT_IMAGE: 
                {
                    std::shared_ptr<ResHandle>& resHandle =
                        ResCache::Get()->GetHandle(&BaseResource(element.text));
                    if (resHandle)
                    {
                        const std::shared_ptr<ImageResourceExtraData>& extra =
                            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
                        if (extra->GetImage())
                        {
                            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
                            effect->SetTexture(extra->GetImage());

                            RectangleShape<2, int> tcoordRect;
                            tcoordRect.mExtent = Vector2<int>{
                                (int)effect->GetTexture()->GetDimension(0) ,
                                (int)effect->GetTexture()->GetDimension(1) };
                            tcoordRect.mCenter = tcoordRect.mExtent / 2;

                            skin->Draw2DTexture(mVisual, rect, tcoordRect, 0);
                        }
                    }
			    } break;

			    case ParsedText::ELEMENT_ITEM: 
                {
                    /*
				    IItemDefManager *idef = m_client->idef();
				    ItemStack item;
				    item.Deserialize(stringw_to_utf8(element.text), idef);

				    DrawItemStack(
						    m_environment->getVideoDriver(),
						    g_fontengine->getFont(), item, rect, &clipRect,
						    m_client, IT_ROT_OTHER, element.angle, element.rotation
				    );
                    */
			    } break;
			}
		}
	}
}

// -----------------------------------------------------------------------------
// UIHyperText - The formated text area form item

//! constructor
UIHyperText::UIHyperText(BaseUI* ui, int id, RectangleShape<2, int> rectangle, const wchar_t *text) :
    BaseUIHyperText(id, rectangle), mUI(ui), mVScrollbar(nullptr), mDrawer(ui, text)
{
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	mScrollbarWidth = skin ? skin->GetSize(DS_SCROLLBAR_SIZE) : 16;

    RectangleShape<2, int> rect;
    rect.mExtent[0] = mScrollbarWidth;
    rect.mCenter[0] = mRelativeRect.mExtent[0] - mScrollbarWidth / 2;
    rect.mExtent[1] = mRelativeRect.mExtent[1];
    rect.mCenter[1] = mRelativeRect.mExtent[1] / 2;

    mVScrollbar = mUI->AddScrollBar(false, true, rect);
	mVScrollbar->SetVisible(false);
}

//! destructor
UIHyperText::~UIHyperText()
{
	mVScrollbar->Remove();
}

ParsedText::Element *UIHyperText::GetElementAt(int x, int y)
{
	Vector2<int> pos{x, y};
    pos -= mDisplayTextRect.GetVertice(RVP_UPPERLEFT);
	pos -= mTextScrollpos;
	return mDrawer.GetElementAt(pos);
}

void UIHyperText::CheckHover(int x, int y)
{
	mDrawer.mHovertag = nullptr;

    if (mAbsoluteRect.IsPointInside(Vector2<int>{x, y}))
    {
		ParsedText::Element *element = GetElementAt(x, y);

		if (element) 
        {
			for (auto& tag : element->tags) 
            {
				if (tag->name == "action") 
                {
					mDrawer.mHovertag = tag;
					break;
				}
			}
		}
	}
}

bool UIHyperText::OnEvent(const Event& evt)
{
	// Scroll bar
	if (evt.mEventType == ET_UI_EVENT &&
		evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED &&
		evt.mUIEvent.mCaller == mVScrollbar.get())
    {
		mTextScrollpos[1] = -mVScrollbar->GetPosition();
	}

	// Reset hover if element left
	if (evt.mEventType == ET_UI_EVENT &&
		evt.mUIEvent.mEventType == UIEVT_ELEMENT_LEFT)
    {
		mDrawer.mHovertag = nullptr;

        System* system = System::Get();
		if (system->GetCursorControl()->IsVisible())
            system->GetCursorControl()->SetActiveIcon(CI_NORMAL);
	}

	if (evt.mEventType == ET_MOUSE_INPUT_EVENT)
    {
		if (evt.mMouseInput.mEvent == MIE_MOUSE_MOVED)
			CheckHover(evt.mMouseInput.X, evt.mMouseInput.Y);

		if (evt.mMouseInput.mEvent == MIE_MOUSE_WHEEL && mVScrollbar->IsVisible())
        {
			mVScrollbar->SetPosition(
                (int)(mVScrollbar->GetPosition() - evt.mMouseInput.mWheel * mVScrollbar->GetSmallStep()));
			mTextScrollpos[1] = -mVScrollbar->GetPosition();
			mDrawer.Draw(mDisplayTextRect, mTextScrollpos);
			CheckHover(evt.mMouseInput.X, evt.mMouseInput.Y);
			return true;

		} 
        else if (evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
        {
			ParsedText::Element *element = GetElementAt(evt.mMouseInput.X, evt.mMouseInput.Y);
			if (element) 
            {
				for (auto& tag : element->tags) 
                {
					if (tag->name == "action") 
                    {
						mText = std::wstring(L"action:") + ToWideString(tag->attrs["name"]);
						if (mParent) 
                        {
							Event newEvent;
							newEvent.mEventType = ET_UI_EVENT;
							newEvent.mUIEvent.mCaller = this;
							newEvent.mUIEvent.mElement = 0;
							newEvent.mUIEvent.mEventType = UIEVT_BUTTON_CLICKED;
							mParent->OnEvent(newEvent);
						}
						break;
					}
				}
			}
		}
	}

	return BaseUIElement::OnEvent(evt);
}

//! draws the element and its children
void UIHyperText::Draw()
{
	if (!IsVisible())
		return;

	// Text
	mDisplayTextRect = mAbsoluteRect;
	mDrawer.Place(mDisplayTextRect);

	// Show scrollbar if text overflow
	if (mDrawer.GetHeight() > mDisplayTextRect.mExtent[1]) 
    {
		mVScrollbar->SetSmallStep((int)(mDisplayTextRect.mExtent[1] * 0.1f));
		mVScrollbar->SetLargeStep((int)(mDisplayTextRect.mExtent[1] * 0.5f));
		mVScrollbar->SetMax(mDrawer.GetHeight() - mDisplayTextRect.mExtent[1]);

		mVScrollbar->SetVisible(true);

		mVScrollbar->SetPageSize(int(mDrawer.GetHeight()));

		RectangleShape<2, int> smallerRect = mDisplayTextRect;
        smallerRect.mExtent[0] -= mScrollbarWidth;
        smallerRect.mCenter[0] -= mScrollbarWidth / 2;
		mDrawer.Place(smallerRect);
	} 
    else 
    {
		mVScrollbar->SetMax(0);
		mVScrollbar->SetPosition(0);
		mVScrollbar->SetVisible(false);
	}
	mDrawer.Draw(mAbsoluteClippingRect, 
        mDisplayTextRect.GetVertice(RVP_UPPERLEFT) + mTextScrollpos);

	// draw children
	BaseUIElement::Draw();
}
