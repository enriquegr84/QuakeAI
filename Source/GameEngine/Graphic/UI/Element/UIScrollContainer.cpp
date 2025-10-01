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

#include "UIScrollContainer.h"

#include "Graphic/UI/UIEngine.h"

UIScrollContainer::UIScrollContainer(
    BaseUI* ui, int id, RectangleShape<2, int> rectangle, 
    const std::string &orientation, float scrollFactor) :
    BaseUIScrollContainer(id, rectangle),
    mUI(ui), mScrollbar(nullptr), mScrollFactor(scrollFactor)
{
	if (orientation == "vertical")
        mOrientation = VERTICAL;
	else if (orientation == "horizontal")
        mOrientation = HORIZONTAL;
	else
        mOrientation = UNDEFINED;
}

bool UIScrollContainer::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_MOUSE_INPUT_EVENT &&
        evt.mMouseInput.mEvent == MIE_MOUSE_WHEEL &&
        !evt.mMouseInput.IsLeftPressed() && mScrollbar)
    {
		mUI->SetFocus(mScrollbar);
		bool retval = mScrollbar->OnEvent(evt);

		// a hacky fix for updating the hovering and co.
		std::shared_ptr<BaseUIElement> hoveredElem = 
            GetElementFromPoint(Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y});
		Event movEvent = evt;
        movEvent.mMouseInput.mEvent = MIE_MOUSE_MOVED;
        mUI->OnMsgProc(movEvent);
		if (hoveredElem)
            hoveredElem->OnEvent(movEvent);

		return retval;
	}

	return BaseUIElement::OnEvent(evt);
}

void UIScrollContainer::Draw()
{
	if (IsVisible()) 
    {
        for (std::shared_ptr<BaseUIElement> child : mChildren)
        {
            if (child->IsNotClipped() ||
                mAbsoluteClippingRect.IsColliding(child->GetAbsolutePosition()))
            {
                child->Draw();
            }
        }
	}
}

void UIScrollContainer::UpdateScrolling()
{
	int pos = mScrollbar->GetPosition();

    RectangleShape<2, int> rect = GetRelativePosition();
    if (mOrientation == VERTICAL)
    {
        Vector2<int> lowerRightCorner = rect.GetVertice(RVP_LOWERRIGHT);
        rect.mExtent[1] = lowerRightCorner[1] - (int)(pos * mScrollFactor);
        rect.mCenter[1] = lowerRightCorner[1] - (int)round(rect.mExtent[1] / 2.f);
    }
    else if (mOrientation == HORIZONTAL)
    {
        Vector2<int> lowerRightCorner = rect.GetVertice(RVP_LOWERRIGHT);
        rect.mExtent[0] = lowerRightCorner[0] - (int)(pos * mScrollFactor);
        rect.mCenter[0] = lowerRightCorner[0] - (int)round(rect.mExtent[0] / 2.f);
    }

	SetRelativePosition(rect);
}
