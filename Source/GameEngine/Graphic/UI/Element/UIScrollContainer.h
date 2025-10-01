/*
Minetest
Copyright (C) 2020 DS

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

#ifndef UISCROLLCONTAINER_H
#define UISCROLLCONTAINER_H

#include "UIScrollBar.h"


class BaseUIScrollContainer : public BaseUIElement
{
public:
    BaseUIScrollContainer(int id, RectangleShape<2, int> rectangle) :
        BaseUIElement(UIET_SCROLL_CONTAINER, id, rectangle)
    {

    }
};


class UIScrollContainer : public BaseUIScrollContainer
{
public:
    UIScrollContainer(
        BaseUI* ui, int id, RectangleShape<2, int> rectangle,
        const std::string &orientation, float scrollfactor);

	virtual bool OnEvent(const Event& evt);

	virtual void Draw();

	inline void OnScrollEvent(BaseUIElement* caller)
	{
		if (caller == mScrollbar.get())
			UpdateScrolling();
	}

	inline void SetScrollBar(std::shared_ptr<UIScrollBar> scrollbar)
	{
		mScrollbar = scrollbar;
		UpdateScrolling();
	}

private:
	enum OrientationEnum
	{
		VERTICAL,
		HORIZONTAL,
		UNDEFINED
	};

    BaseUI* mUI;

	std::shared_ptr<UIScrollBar> mScrollbar;
	OrientationEnum mOrientation;
	float mScrollFactor;

	void UpdateScrolling();
};

#endif