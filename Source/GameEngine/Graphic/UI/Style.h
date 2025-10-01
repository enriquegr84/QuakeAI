/*
Minetest
Copyright (C) 2019 rubenwardy

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

#ifndef STYLE_H
#define STYLE_H

#include "Core/Utility/StringUtil.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Geometric/Rectangle.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Image/ImageResource.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class Style
{
public:
	enum Property
	{
        TEXTCOLOR,
        BGCOLOR,
        BGCOLOR_HOVERED,
        BGCOLOR_PRESSED,
        NOCLIP,
        BORDER,
        BGIMG,
        BGIMG_HOVERED,
        BGIMG_MIDDLE,
        BGIMG_PRESSED,
        FGIMG,
        FGIMG_HOVERED,
        FGIMG_PRESSED,
        ALPHA,
        CONTENT_OFFSET,
        PADDING,
        FONT,
        FONT_SIZE,
        COLORS,
        BORDERCOLORS,
        BORDERWIDTHS,
        SOUND,
        SPACING,
        SIZE,
        NUM_PROPERTIES,
        NONE
	};
	enum State
	{
		STATE_DEFAULT = 0,
		STATE_HOVERED = 1 << 0,
		STATE_PRESSED = 1 << 1,
		NUM_STATES = 1 << 2,
		STATE_INVALID = 1 << 3,
	};

private:
	std::array<bool, NUM_PROPERTIES> mPropertySet{};
	std::array<std::string, NUM_PROPERTIES> mProperties;
	State mStateMap = STATE_DEFAULT;

public:
	static Property GetPropertyByName(const std::string &name)
	{
		if (name == "textcolor")
			return TEXTCOLOR;
        else if (name == "bgcolor")
			return BGCOLOR;
        else if (name == "bgcolor_hovered")
            return BGCOLOR_HOVERED;
        else if (name == "bgcolor_pressed")
            return BGCOLOR_PRESSED;
        else if (name == "noclip")
			return NOCLIP;
        else if (name == "border")
			return BORDER;
        else if (name == "bgimg")
			return BGIMG;
        else if (name == "bgimg_hovered")
            return BGIMG_HOVERED;
        else if (name == "bgimg_middle")
            return BGIMG_MIDDLE;
        else if (name == "bgimg_pressed")
            return BGIMG_PRESSED;
        else if (name == "fgimg")
			return FGIMG;
        else if (name == "fgimg_hovered")
            return FGIMG_HOVERED;
        else if (name == "fgimg_pressed") 
            return FGIMG_PRESSED;
        else if (name == "alpha")
			return ALPHA;
        else if (name == "content_offset")
			return CONTENT_OFFSET;
        else if (name == "padding")
			return PADDING;
        else if (name == "font")
			return FONT;
        else if (name == "font_size")
			return FONT_SIZE;
        else if (name == "colors")
			return COLORS;
        else if (name == "bordercolors")
			return BORDERCOLORS;
        else if (name == "borderwidths")
			return BORDERWIDTHS;
        else if (name == "sound")
			return SOUND;
        else if (name == "spacing")
			return SPACING;
        else if (name == "size")
			return SIZE;
        else
			return NONE;
	}

	std::string Get(Property prop, std::string def) const
	{
		const auto &val = mProperties[prop];
		return val.empty() ? def : val;
	}

	void Set(Property prop, const std::string& value)
	{
		mProperties[prop] = value;
		mPropertySet[prop] = true;
	}

	//! Parses a name and returns the corresponding state enum
	static State GetStateByName(const std::string& name)
	{
		if (name == "default")
			return STATE_DEFAULT;
        else if (name == "hovered")
			return STATE_HOVERED;
        else if (name == "pressed")
			return STATE_PRESSED;
        else
			return STATE_INVALID;
	}

	//! Gets the state that this style is intended for
	State GetState() const
	{
		return mStateMap;
	}

	//! Set the given state on this style
	void AddState(State state)
	{
        LogAssert(state >= NUM_STATES, "Out-of-bounds state received");

		mStateMap = static_cast<State>(mStateMap | state);
	}

	//! Using a list of styles mapped to state values, calculate the final
	//  combined style for a state by propagating values in its component states
	static Style GetStyleFromStatePropagation(const std::array<Style, NUM_STATES> &styles, State state)
	{
		Style temp = styles[Style::STATE_DEFAULT];
		temp.mStateMap = state;
		for (int i = Style::STATE_DEFAULT + 1; i <= state; i++)
			if ((state & i) != 0)
				temp = temp | styles[i];

		return temp;
	}

	SColor GetColor(Property prop, SColor def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		ParseColorString(val, def, false, 0xFF);
		return def;
	}

	SColor GetColor(Property prop) const
	{
		const auto &val = mProperties[prop];
        LogAssert(!val.empty(), "Unexpected missing property");

		SColor color;
		ParseColorString(val, color, false, 0xFF);
		return color;
	}

	std::array<SColor, 4> GetColorArray(Property prop, std::array<SColor, 4> def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		std::vector<std::string> strs;
		if (!ParseArray(val, strs))
			return def;

		for (size_t i = 0; i <= 3; i++) 
        {
			SColor color;
			if (ParseColorString(strs[i], color, false, 0xff))
				def[i] = color;
		}

		return def;
	}

	std::array<int, 4> GetIntArray(Property prop, std::array<int, 4> def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		std::vector<std::string> strs;
		if (!ParseArray(val, strs))
			return def;

		for (size_t i = 0; i <= 3; i++)
			def[i] = atoi(strs[i].c_str());

		return def;
	}

	RectangleShape<2, int> GetRect(Property prop, RectangleShape<2, int> def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		RectangleShape<2, int> rect;
		if (!ParseRect(val, &rect))
			return def;

		return rect;
	}

	RectangleShape<2, int> GetRect(Property prop) const
	{
		const auto &val = mProperties[prop];
		LogAssert(!val.empty(), "Unexpected missing property");

		RectangleShape<2, int> rect;
		ParseRect(val, &rect);
		return rect;
	}

	Vector2<float> GetVector(Property prop, Vector2<float> def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		Vector2<float> vec;
		if (!ParseVector(val, &vec))
			return def;

		return vec;
	}

	Vector2<int> GetVector(Property prop, Vector2<int> def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		Vector2<float> vec;
		if (!ParseVector(val, &vec))
			return def;

        return Vector2<int>{(int)vec[0], (int)vec[1]};
	}

	Vector2<int> GetVector(Property prop) const
	{
		const auto &val = mProperties[prop];
        LogAssert(!val.empty(), "Unexpected missing property");

		Vector2<float> vec;
		ParseVector(val, &vec);
        return Vector2<int>{(int)vec[0], (int)vec[1]};
	}

    std::shared_ptr<Texture2> GetTexture(Property prop) const
    {
        const auto &val = mProperties[prop];
        LogAssert(!val.empty(), "Unexpected missing property");

        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(val)));
        std::shared_ptr<ImageResourceExtraData> resData =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        resData->GetImage()->AutogenerateMipmaps();
        return resData->GetImage();
    }

	bool GetBool(Property prop, bool def) const
	{
		const auto &val = mProperties[prop];
		if (val.empty())
			return def;

		return IsYes(val);
	}

	inline bool IsNotDefault(Property prop) const
	{
		return !mProperties[prop].empty();
	}

	inline bool HasProperty(Property prop) const { return mPropertySet[prop]; }

	Style &operator|=(const Style &other)
	{
		for (size_t i = 0; i < NUM_PROPERTIES; i++) 
        {
			auto prop = (Property)i;
			if (other.HasProperty(prop))
				Set(prop, other.Get(prop, ""));
		}

		return *this;
	}

	Style operator|(const Style &other) const
	{
		Style newStyle = *this;
        newStyle |= other;
		return newStyle;
	}

private:

	bool ParseArray(const std::string &value, std::vector<std::string>& arr) const
	{
		std::vector<std::string> strs = Split(value, ',');

        if (strs.size() == 1)
        {
            arr = { strs[0], strs[0], strs[0], strs[0] };
        }
        else if (strs.size() == 2)
        {
            arr = { strs[0], strs[1], strs[0], strs[1] };
        }
        else if (strs.size() == 4)
        {
            arr = strs;
        }
        else 
        {
            LogWarning("Invalid array size " + std::to_string(strs.size()) + " arguments: " + value);
			return false;
		}
		return true;
	}

	bool ParseRect(const std::string &value, RectangleShape<2, int> *parsedRect) const
	{
		RectangleShape<2, int> rect;
		std::vector<std::string> vRect = Split(value, ',');

		if (vRect.size() == 1) 
        {
			int x = atoi(vRect[0].c_str());
            rect.mExtent = Vector<2, int>{ 2 * x, 2 * x };
		} 
        else if (vRect.size() == 2)
        {
			int x = atoi(vRect[0].c_str());
			int y =	atoi(vRect[1].c_str());
            rect.mExtent = Vector<2, int>{ 2 * x, 2 * y };
			// `-x` is interpreted as `w - x`
		} 
        else if (vRect.size() == 4)
        {
            rect.mExtent = Vector<2, int>{ 
                atoi(vRect[2].c_str()) - atoi(vRect[0].c_str()),
                atoi(vRect[3].c_str()) - atoi(vRect[1].c_str()) };

            rect.mCenter = Vector<2, int>{ 
                atoi(vRect[2].c_str()) + rect.mExtent[0] / 2,
                atoi(vRect[3].c_str()) + rect.mExtent[1] / 2 };
		} 
        else 
        {
            LogWarning("Invalid rectangle string format: \"" + value + "\"");
			return false;
		}

		*parsedRect = rect;

		return true;
	}

	bool ParseVector(const std::string &value, Vector2<float>* parsedVec) const
	{
		Vector2<float> vec;
		std::vector<std::string> vVector = Split(value, ',');

		if (vVector.size() == 1) 
        {
			float x = (float)atof(vVector[0].c_str());
			vec[0] = x;
			vec[1] = x;
		} 
        else if (vVector.size() == 2) 
        {
			vec[0] = (float)atof(vVector[0].c_str());
			vec[1] = (float)atof(vVector[1].c_str());
		} 
        else 
        {
            LogWarning("Invalid 2d vector string format: \"" + value + "\"");
			return false;
		}

		*parsedVec = vec;

		return true;
	}
};


#endif