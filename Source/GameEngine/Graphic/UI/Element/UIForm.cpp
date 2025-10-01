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

#include "UIForm.h"

#include "UICheckBox.h"
#include "UIScrollBar.h"
#include "UIScrollContainer.h"
#include "UIStaticText.h"
#include "UITabControl.h"
#include "UITable.h"
#include "UIFont.h"

#include "Core/OS/OS.h"

#include "Application/System/System.h"
#include "Application/Settings.h"


// clang-format off
BaseUIForm::BaseUIForm(BaseUI* ui, int id, RectangleShape<2, int> rectangle, bool remapDoubleClick) :
    BaseUIElement(UIET_MODAL_FORM, id, rectangle), mUI(ui), mActive(true), mRemapDoubleClick(remapDoubleClick)
{
    mDoubleClickDetect[0].time = 0;
    mDoubleClickDetect[1].time = 0;

    mDoubleClickDetect[0].pos.MakeZero();
    mDoubleClickDetect[1].pos.MakeZero();
}

// clang-format on
BaseUIForm::~BaseUIForm()
{

}

void BaseUIForm::OnInit()
{
    SetVisible(true);
    mUI->SetFocus(shared_from_this());
}

void BaseUIForm::AllowFocusRemoval(bool allow)
{
    mAllowFocusRemoval = allow;
}

bool BaseUIForm::CanTakeFocus(BaseUIElement* el)
{
    return (el && (el == this || IsChild(el))) || mAllowFocusRemoval;
}

void BaseUIForm::Draw()
{
    if (!IsVisible())
        return;

    Renderer* renderer = Renderer::Get();
    Vector2<unsigned int> screenSize(renderer->GetScreenSize());
    if (screenSize != mScreenSizeOld)
    {
        mScreenSizeOld = screenSize;
        RegenerateUI(screenSize);
    }

    DrawForm();
}

/*
    This should be called when the Form wants to quit.

    WARNING: THIS DEALLOCATES THE Form FROM MEMORY. Return
    immediately if you call this from the Form itself.

    (More precisely, this decrements the reference count.)
*/
void BaseUIForm::QuitForm()
{
	mActive = false;
    mAllowFocusRemoval = true;
    // This removes Environment's grab on us
    mUI->RemoveFocus(shared_from_this());
	mUI->RemoveHovered();

	RemoveChildren(true);
    Remove();
}

// clang-format off
bool BaseUIForm::DoubleClickDetection(const Event& evt)
{
    /* The following code is for capturing double-clicks of the mouse button
     * and translating the double-click into an EET_KEY_INPUT_EVENT event
     * -- which closes the form -- under some circumstances.
     *
     * There have been many github issues reporting this as a bug even though it
     * was an intended feature.  For this reason, remapping the double-click as
     * an ESC must be explicitly set when creating this class via the
     * /p remap_dbl_click parameter of the constructor.
     */

    if (!mRemapDoubleClick)
        return false;

    if (evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
    {
        mDoubleClickDetect[0].pos = mDoubleClickDetect[1].pos;
        mDoubleClickDetect[0].time = mDoubleClickDetect[1].time;

        mDoubleClickDetect[1].pos = mPointer;
        mDoubleClickDetect[1].time = Timer::GetTime();
    }
    else if (evt.mMouseInput.mEvent == MIE_LMOUSE_LEFT_UP)
    {
        unsigned int delta = std::abs((int)(mDoubleClickDetect[0].time - Timer::GetTime()));
        if (delta > 400)
            return false;

        int squareDistance = LengthSq(mDoubleClickDetect[0].pos - mDoubleClickDetect[1].pos);
        if (squareDistance > (30 * 30))
            return false;

        Event translated;
        // translate doubleclick to escape
        translated.mEventType = ET_KEY_INPUT_EVENT;
        translated.mKeyInput.mKey = KEY_ESCAPE;
        translated.mKeyInput.mControl = false;
        translated.mKeyInput.mShift = false;
        translated.mKeyInput.mPressedDown = true;
        translated.mKeyInput.mChar = 0;
        OnEvent(translated);

        return true;
    }

    return false;
}

bool BaseUIForm::OnPreEvent(const Event& evt)
{
    if (evt.mEventType == ET_MOUSE_INPUT_EVENT)
    {
        Vector2<int> mousePosition{ evt.mMouseInput.X, evt.mMouseInput.Y };
        std::shared_ptr<BaseUIElement> hovered =
            mUI->GetRootUIElement()->GetElementFromPoint(mousePosition);
        if (!IsChild(hovered.get()))
            if (DoubleClickDetection(evt))
                return true;
    }
    return false;
}


/*
	UIForm
*/
static unsigned int FontLineHeight(std::shared_ptr<BaseUIFont> font)
{
	return font->GetDimension(L"Ay")[1];
}

UIForm::UIForm(BaseUI* ui, int id, RectangleShape<2, int> rectangle, 
    std::shared_ptr<BaseFormSource> formSrc, std::shared_ptr<TextDestination> txtDst, 
    const std::string& formPrepend, bool remapDoubleClick) : 
    BaseUIForm(ui, id, rectangle, remapDoubleClick), 
    mFormPrepend(formPrepend), mFormSource(formSrc), mTextDst(txtDst)
{
	mTooltipShowDelay = Settings::Get()->GetUInt("tooltip_show_delay");

	mPadding = Vector2<int>::Zero();
	mSpacing = Vector2<float>::Zero();
	mImageSize = Vector2<int>::Zero();
	mOffset = Vector2<int>::Zero();
	mPosOffset = Vector2<float>::Zero();

	mCurrentKeysPending.keyDown = false;
	mCurrentKeysPending.keyUp = false;
	mCurrentKeysPending.keyEnter = false;
	mCurrentKeysPending.keyEscape = false;

    mBlendState = std::make_shared<BlendState>();
    mBlendState->mTarget[0].enable = true;
    mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
    mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    // Create a vertex buffer for a single triangle.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

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

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

UIForm::~UIForm()
{
    // delete all children
    RemoveChildren(true);

    if (mTooltipElement)
    {
        mTooltipElement->Remove();
        mTooltipElement = nullptr;
    }
}

void UIForm::SetInitialFocus()
{
	// Set initial focus according to following order of precedence:
	// 1. first empty editbox
	// 2. first editbox
	// 3. first table
	// 4. last button
	// 5. first focusable (not statictext, not tabheader)
	// 6. first child element
	std::list<std::shared_ptr<BaseUIElement>> children = GetChildren();

	// in case "children" contains any NULL elements, remove them
	for (std::list<std::shared_ptr<BaseUIElement>>::iterator it = children.begin(); it != children.end();)
    {
		if (*it)
			++it;
		else
			it = children.erase(it);
	}

	// 1. first empty editbox
	for (std::shared_ptr<BaseUIElement> child : children)
    {
		if (child->GetType() == UIET_EDIT_BOX && child->GetText()[0] == 0) 
        {
			mUI->SetFocus(child);
			return;
		}
	}

	// 2. first editbox
	for (std::shared_ptr<BaseUIElement> child : children) 
    {
		if (child->GetType() == UIET_EDIT_BOX)
        {
            mUI->SetFocus(child);
			return;
		}
	}

	// 3. first table
	for (std::shared_ptr<BaseUIElement> child : children)
    {
		if (child->GetType() == UIET_TABLE)
        {
            mUI->SetFocus(child);
			return;
		}
	}

	// 4. last button
    std::list<std::shared_ptr<BaseUIElement>>::reverse_iterator it;
	for (it = children.rbegin(); it != children.rend(); it++) 
    {
		if ((*it)->GetType() == UIET_BUTTON) 
        {
            mUI->SetFocus(*it);
			return;
		}
	}

	// 5. first focusable (not statictext, not tabheader)
	for (std::shared_ptr<BaseUIElement> child : children) 
    {
		if (child->GetType() != UIET_STATIC_TEXT &&
            child->GetType() != UIET_TAB_CONTROL) 
        {
            mUI->SetFocus(child);
			return;
		}
	}

	// 6. first child element
	if (children.empty())
        mUI->SetFocus(shared_from_this());
	else
        mUI->SetFocus(*(children.begin()));
}

std::shared_ptr<UITable> UIForm::GetTable(const std::string& tablename)
{
	for (auto const& table : mTables)
    {
		if (tablename == table.first.mName)
			return table.second;
	}
	return 0;
}

std::vector<std::string>* UIForm::GetDropDownValues(const std::string& name)
{
	for (auto& dropdown : mDropdowns)
		if (name == dropdown.first.mName)
			return &dropdown.second;

	return NULL;
}

Vector2<int> UIForm::GetElementBasePosition(const std::vector<std::string> *vPos)
{
    Vector2<float> basePos = Vector2<float>{ (float)mPadding[0], (float)mPadding[1] } + mPosOffset * mSpacing;
	if (vPos) 
    {
		basePos[0] += (float)(atof((*vPos)[0].c_str()) * mSpacing[0]);
		basePos[1] += (float)(atof((*vPos)[1].c_str()) * mSpacing[1]);
	}
    return Vector2<int>{(int)basePos[0], (int)basePos[1]};
}

Vector2<int> UIForm::GetRealCoordinateBasePosition(const std::vector<std::string>& vPos)
{
    return Vector2<int>{
        (int)((atof(vPos[0].c_str()) + mPosOffset[0]) * mImageSize[0]),
        (int)((atof(vPos[1].c_str()) + mPosOffset[1]) * mImageSize[1])};
}

Vector2<int> UIForm::GetRealCoordinateGeometry(const std::vector<std::string>& vGeom)
{
    return Vector2<int>{
        (int)(atof(vGeom[0].c_str()) * mImageSize[0]),
        (int)(atof(vGeom[1].c_str()) * mImageSize[1])};
}

void UIForm::ParseSize(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,',');
	if (parts.size() >= 2)
	{
		if (parts[1].find(';') != std::string::npos)
			parts[1] = parts[1].substr(0,parts[1].find(';'));

		data->mInvSize[0] = (float)std::max(0., atof(parts[0].c_str()));
		data->mInvSize[1] = (float)std::max(0., atof(parts[1].c_str()));

		LockSize(false);
		if (parts.size() == 3)
			if (parts[2] == "true")
				LockSize(true, Vector2<unsigned int>{800, 600});
		data->mExplicitSize = true;
		return;
	}
    LogError("Invalid size element (" + 
        std::to_string(parts.size()) + "): '" + element + "'" );
}

void UIForm::ParseContainer(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ',');
	if (parts.size() >= 2) 
    {
		if (parts[1].find(';') != std::string::npos)
			parts[1] = parts[1].substr(0, parts[1].find(';'));

		mContainerStack.push(mPosOffset);
		mPosOffset[0] += (float)(atof(parts[0].c_str()));
		mPosOffset[1] += (float)(atof(parts[1].c_str()));
		return;
	}
    LogError("Invalid container start element (" +
        std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseContainerEnd(ParserData* data)
{
	if (!mContainerStack.empty()) 
    {
        mPosOffset = mContainerStack.top();
        mContainerStack.pop();
    }
    else
        LogError("Invalid container end element, no matching container start element");

}

void UIForm::ParseScrollContainer(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() != 4 || parts.size() != 5) 
    {
        LogError("Invalid scroll_container start element (" + 
            std::to_string(parts.size()) + "): '" + element + "'");
		return;
	}

	std::vector<std::string> vPos  = Split(parts[0], ',');
	std::vector<std::string> vGeom = Split(parts[1], ',');
	std::string scrollbarName = parts[2];
	std::string orientation = parts[3];
	float scrollFactor = 0.1f;
	if (parts.size() >= 5 && !parts[4].empty())
		scrollFactor = (float)atof(parts[4].c_str());

    if (vPos.size() != 2)
        LogError("Invalid pos for scrollcontainer specified: \"" + parts[0] + "\"");
    if (vGeom.size() != 2)
        LogError("Invalid geom for scrollcontainer specified: \"" + parts[1] + "\"");

	Vector2<int> pos = GetRealCoordinateBasePosition(vPos);
	Vector2<int> geom = GetRealCoordinateGeometry(vGeom);

    if (orientation == "vertical")
        scrollFactor *= -mImageSize[1];
    else if (orientation == "horizontal")
        scrollFactor *= -mImageSize[0];
    else
        LogWarning("Invalid scrollcontainer orientation: " + orientation);

	// old parent (at first: this)
	// ^ is parent of clipper
	// ^ is parent of mover
	// ^ is parent of other elements

	// make clipper
    RectangleShape<2, int> rectClipper = RectangleShape<2, int>();
    rectClipper.mExtent = geom;
    rectClipper.mCenter = pos + geom / 2;

    std::shared_ptr<BaseUIElement> clipper = 
        std::make_shared<BaseUIElement>(UIET_ELEMENT, -1, rectClipper);
    clipper->SetParent(data->mCurrentParent);
	clipper->SetSubElement(true);

	// make mover
	Field fieldMover("", L"", L"", 258 + (int)mFields.size());

    RectangleShape<2, int> rectMover = RectangleShape<2, int>();
    rectMover.mExtent = geom;
    rectMover.mCenter = geom / 2;
	std::shared_ptr<UIScrollContainer> mover = std::static_pointer_cast<UIScrollContainer>(
        mUI->AddScrollContainer(orientation, scrollFactor, rectMover, clipper, fieldMover.mId));
	mover->SetSubElement(true);
	data->mCurrentParent = mover;

	mScrollContainers.emplace_back(scrollbarName, mover);

	mFields.push_back(fieldMover);

	// remove interferring mOffset of normal containers
	mContainerStack.push(mPosOffset);
	mPosOffset[0] = 0.0f;
	mPosOffset[1] = 0.0f;
}

void UIForm::ParseScrollContainerEnd(ParserData *data)
{
	if (data->mCurrentParent == shared_from_this() || 
        data->mCurrentParent->GetParent() == shared_from_this() || 
        mContainerStack.empty()) 
    {
        LogError("Invalid scrollcontainer end element, " 
            "no matching scrollcontainer start element");
		return;
	}

	if (LengthSq(mPosOffset) != 0.0f)
    {
		// mPosOffset is only set by containers and scrollcontainers.
		// scroll_containers always set it to 0,0 which means that if it is
		// not 0,0, it is a normal container that was opened last, not a
		// scrollcontainer
        LogError("Invalid scrollcontainer end element, an inner container was left open");
		return;
	}

	data->mCurrentParent = data->mCurrentParent->GetParent()->GetParent();
	mPosOffset = mContainerStack.top();
	mContainerStack.pop();
}

void UIForm::ParseList(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

    if (parts.size() >= 4)
    {
        std::string location = parts[0];
        std::string listName = parts[1];
        std::vector<std::string> vPos = Split(parts[2], ',');
        std::vector<std::string> vGeom = Split(parts[3], ',');
        std::string startIndex;
        if (parts.size() == 5)
            startIndex = parts[4];

        if (vPos.size() != 2)
            LogError("Invalid pos for list specified: \"" + parts[2] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for list specified: \"" + parts[3] + "\"");

        Vector2<int> geom{ atoi(vGeom[0].c_str()), atoi(vGeom[1].c_str()) };

        int startIdx = 0;
        if (!startIndex.empty())
            startIdx = atoi(startIndex.c_str());

        if (geom[0] < 0 || geom[1] < 0 || startIdx < 0)
        {
            LogError("Invalid list element: '" + element + "'");
            return;
        }

        if (!data->mExplicitSize)
            LogWarning("invalid use of list without a Size[] element");

        Field field("", L"", L"", 258 + (int)mFields.size(), 3);
        auto style = GetDefaultStyleForElement("list", field.mName);

        Vector2<float> slotScale = style.GetVector(Style::SIZE, Vector2<float>::Zero());
        Vector2<float> slotSize{
            slotScale[0] <= 0.f ? mImageSize[0] : std::max<float>(slotScale[0] * mImageSize[0], 1),
            slotScale[1] <= 0.f ? mImageSize[1] : std::max<float>(slotScale[1] * mImageSize[1], 1) };

        Vector2<float> slotSpacing = style.GetVector(Style::SPACING, Vector2<float>{-1, -1});
        Vector2<float> defaultSpacing = data->mRealCoordinates ?
            Vector2<float>{mImageSize[0] * 0.25f, mImageSize[1] * 0.25f} :
            Vector2<float>{mSpacing[0] - mImageSize[0], mSpacing[1] - mImageSize[1]};

		slotSpacing[0] = slotSpacing[0] < 0 ? defaultSpacing[0] : mImageSize[0] * slotSpacing[0];
		slotSpacing[1] = slotSpacing[1] < 0 ? defaultSpacing[1] : mImageSize[1] * slotSpacing[1];

		slotSpacing += slotSize;

		Vector2<int> pos = data->mRealCoordinates ? GetRealCoordinateBasePosition(vPos) :
				GetElementBasePosition(&vPos);
        /*
		RectangleShape<2, int> rect = RectangleShape<2, int>(pos[0], pos[1],
				pos[0] + (geom[0] - 1) * slotSpacing[0] + slotSize[0],
				pos[1] + (geom[1] - 1) * slotSpacing[1] + slotSize.Y);
        */
		mFields.push_back(field);
		return;
	}
	LogError("Invalid list element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseCheckbox(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 3)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::string name = parts[1];
		std::string label = parts[2];
		std::string selected;

		if (parts.size() >= 4)
			selected = parts[3];

        if (vPos.size() != 2)
            LogError("Invalid pos for checkbox specified: \"" + parts[0] + "\"");

		bool cbSelected = false;
		if (selected == "true")
            cbSelected = true;

		std::wstring wlabel = ToWideString(UnescapeString(label));
		const Vector2<int> labelSize = mFont->GetDimension(wlabel);
		int cbSize = mUI->GetSkin()->GetSize(DS_CHECK_BOX_WIDTH);
		int yCenter = (std::max(labelSize[1], cbSize) + 1) / 2;

		Vector2<int> pos;
		RectangleShape<2, int> rect;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);

            rect.mExtent[0] = labelSize[0] + cbSize + 7;
            rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
            rect.mExtent[1] = 2 * yCenter;
            rect.mCenter[1] = pos[1];
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);

            rect.mExtent[0] = labelSize[0] + cbSize + 7;
            rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
            rect.mExtent[1] = 2 * yCenter;
            rect.mCenter[1] = pos[1] + mImageSize[1] / 2;
		}

		Field field(name, wlabel, wlabel, 258 + (int)mFields.size() );
		field.mType = FFT_CHECKBOX;

        std::shared_ptr<BaseUICheckBox> checkBox = mUI->AddCheckBox(
            cbSelected, rect, data->mCurrentParent, field.mId, field.mLabel.c_str());
		checkBox->SetSubElement(true);

		auto style = GetDefaultStyleForElement("checkbox", name);
		field.mSound = style.Get(Style::Property::SOUND, "");
        checkBox->SetNotClipped(style.GetBool(Style::NOCLIP, false));

		if (field.mName == mFocusedElement)
			mUI->SetFocus(checkBox);

		mCheckboxes.emplace_back(field, checkBox);
		mFields.push_back(field);
		return;
	}
	LogError("Invalid checkbox element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseScrollBar(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 5) 
    {
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[3];
		std::string value = parts[4];

        if (vPos.size() != 2)
            LogError("Invalid pos for scrollbar specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for scrollbar specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> dim;

		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			dim = GetRealCoordinateGeometry(vGeom);
		} else 
        {
			pos = GetElementBasePosition(&vPos);
			dim[0] = (int)(atof(vGeom[0].c_str()) * mSpacing[0]);
			dim[1] = (int)(atof(vGeom[1].c_str()) * mSpacing[1]);
		}

        RectangleShape<2, int> rect;
        rect.mExtent = dim;
        rect.mCenter = pos - dim / 2;

		Field field(name, L"", L"", 258 + (int)mFields.size());

		bool isHorizontal = true;
		if (parts[2] == "vertical")
			isHorizontal = false;

		field.mType = FFT_SCROLLBAR;
		field.mSend  = true;
        std::shared_ptr<UIScrollBar> scrollbar = std::static_pointer_cast<UIScrollBar>( 
            mUI->AddScrollBar(isHorizontal, true, rect, data->mCurrentParent, field.mId));
		scrollbar->SetSubElement(true);

		auto style = GetDefaultStyleForElement("scrollbar", name);
        scrollbar->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        scrollbar->SetArrowsVisible(data->ScrollbarOptions.arrowVisiblity);

		int max = data->ScrollbarOptions.max;
		int min = data->ScrollbarOptions.min;
        scrollbar->SetMax(max);
        scrollbar->SetMin(min);

        scrollbar->SetPosition(atoi(parts[4].c_str()));
        scrollbar->SetSmallStep(data->ScrollbarOptions.smallStep);
        scrollbar->SetLargeStep(data->ScrollbarOptions.largeStep);

		int scrollbarSize = isHorizontal ? dim[0] : dim[1];
        scrollbar->SetPageSize(scrollbarSize * (max - min + 1) / data->ScrollbarOptions.thumbSize);
		if (field.mName == mFocusedElement)
			mUI->SetFocus(scrollbar);

		mScrollbars.emplace_back(field, scrollbar);
		mFields.push_back(field);
		return;
	}
	LogError("Invalid scrollbar element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseScrollBarOptions(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() == 0) 
    {
		LogWarning("Invalid scrollbaroptions element(" + 
            std::to_string(parts.size()) + "): '" + element + "'");
		return;
	}

	for (const std::string& i : parts) 
    {
		std::vector<std::string> options = Split(i, '=');
		if (options.size() != 2) 
        {
			LogWarning("Invalid scrollbaroptions option syntax: '" + element + "'");
			continue; // Go to next option
		}

		if (options[0] == "max") 
        {
			data->ScrollbarOptions.max = atoi(options[1].c_str());
			continue;
		} 
        else if (options[0] == "min") 
        {
			data->ScrollbarOptions.min = atoi(options[1].c_str());
			continue;
		} 
        else if (options[0] == "smallstep") 
        {
			int value = atoi(options[1].c_str());
			data->ScrollbarOptions.smallStep = value < 0 ? 10 : value;
			continue;
		} 
        else if (options[0] == "largestep")
        {
			int value = atoi(options[1].c_str());
			data->ScrollbarOptions.largeStep = value < 0 ? 100 : value;
			continue;
		} 
        else if (options[0] == "thumbsize") 
        {
			int value = atoi(options[1].c_str());
			data->ScrollbarOptions.thumbSize = value <= 0 ? 1 : value;
			continue;
		} 
        else 
        {
            if (options[0] == "arrows")
            {
                std::string value = Trim(options[1].c_str());
                if (value == "hide")
                    data->ScrollbarOptions.arrowVisiblity = UIScrollBar::HIDE;
                else if (value == "show")
                    data->ScrollbarOptions.arrowVisiblity = UIScrollBar::SHOW;
                else // Auto hide/show
                    data->ScrollbarOptions.arrowVisiblity = UIScrollBar::DEFAULT;
                continue;
            }
		}

		LogWarning("Invalid scrollbaroptions option(" + options[0] + "): '" + element + "'");
	}
}

void UIForm::ParseImage(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 3)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[2];

        if (vPos.size() != 2)
            LogError("Invalid pos for image specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for image specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)(atof(vGeom[0].c_str()) * mImageSize[0]);
			geom[1] = (int)(atof(vGeom[1].c_str()) * mImageSize[1]);
		}

		if (!data->mExplicitSize)
			LogWarning("invalid use of image without a size[] element");

		std::shared_ptr<Texture2> texture;
        std::shared_ptr<ResHandle> resHandle = 
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(name)));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			texture = resData->GetImage();
			texture->SetName(ToWideString(name));
		}

		if (!texture)
		{
			LogError("Unable to load texture: \t" + name);
			return;
		}
		texture->AutogenerateMipmaps();

		Field field(name, L"", L"", 258 + (int)mFields.size(), 1);

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;
		std::shared_ptr<BaseUIImage> image = mUI->AddImage(
            rect, data->mCurrentParent, field.mId, 0, false);
        image->SetSubElement(true);
		image->SetTexture(texture);
        image->SetScaleImage(true);

		auto style = GetDefaultStyleForElement("image", field.mName);
        image->SetNotClipped(style.GetBool(Style::NOCLIP, mFormVersion < 3));
		mFields.push_back(field);

		// images should let events through
		mClickThroughElements.push_back(image);
		return;
	}

	if (parts.size() == 2) 
    {
		std::vector<std::string> vPos = Split(parts[0],',');
		std::string name = UnescapeString(parts[1]);

        if (vPos.size() != 2)
            LogError("Invalid pos for image specified: \"" + parts[0] + "\"");

		Vector2<int> pos = GetElementBasePosition(&vPos);

		if (!data->mExplicitSize)
			LogWarning("invalid use of image without a size[] element");

		std::shared_ptr<Texture2> texture;
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(name)));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			texture = resData->GetImage();
			texture->SetName(ToWideString(name));
		}

		if (!texture)
		{
			LogError("Unable to load texture: \t" + name);
			return;
		}
		texture->AutogenerateMipmaps();

        Field field(name, L"", L"", 258 + (int)mFields.size());

        std::shared_ptr<BaseUIImage> image = mUI->AddImage(
			texture, pos, data->mCurrentParent, field.mId, 0, true);
		image->SetSubElement(true);

        auto style = GetDefaultStyleForElement("image", field.mName);
        image->SetNotClipped(style.GetBool(Style::NOCLIP, mFormVersion < 3));
        mFields.push_back(field);

        // images should let events through
        mClickThroughElements.push_back(image);
        return;
	}
	LogError("Invalid image element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseAnimatedImage(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() < 6) 
    {
		LogError("Invalid animated_image element(" + 
            std::to_string(parts.size()) + "): '" + element + "'");
		return;
	}

	std::vector<std::string> vPos  = Split(parts[0], ',');
	std::vector<std::string> vGeom = Split(parts[1], ',');
	std::string name = parts[2];
	std::wstring textureName = ToWideString(UnescapeString(parts[3]));
	int frameCount = atoi(parts[4].c_str());
	int frameDuration = atoi(parts[5].c_str());

    if (vPos.size() != 2)
        LogError("Invalid pos for animatedimage specified: \"" + parts[0] + "\"");
    if (vGeom.size() != 2)
        LogError("Invalid geom for animatedimage specified: \"" + parts[1] + "\"");

	Vector2<int> pos;
	Vector2<int> geom;
	if (data->mRealCoordinates) 
    {
		pos = GetRealCoordinateBasePosition(vPos);
		geom = GetRealCoordinateGeometry(vGeom);
	} 
    else 
    {
		pos = GetElementBasePosition(&vPos);
		geom[0] = (int)(atof(vGeom[0].c_str()) * mImageSize[0]);
		geom[1] = (int)(atof(vGeom[1].c_str()) * mImageSize[1]);
	}

	if (!data->mExplicitSize)
		LogWarning("Invalid use of animated_image without a size[] element");

	Field field(name, L"", L"", 258 + (int)mFields.size());
	field.mType = FFT_ANIMATEDIMAGE;
	field.mSend = true;

    RectangleShape<2, int> rect;
    rect.mExtent = geom;
    rect.mCenter = pos - geom / 2;
    std::shared_ptr<BaseUIAnimatedImage> animatedImage = mUI->AddAnimatedImage(
        rect, textureName, frameCount, frameDuration, data->mCurrentParent, field.mId);
	animatedImage->SetSubElement(true);
	if (parts.size() >= 7)
        animatedImage->SetFrameIndex(atoi(parts[6].c_str()) - 1);

	auto style = GetDefaultStyleForElement("animatedimage", field.mName, "image");
    animatedImage->SetNotClipped(style.GetBool(Style::NOCLIP, false));

	// Animated images should let events through
	mClickThroughElements.push_back(animatedImage);

	mFields.push_back(field);
}

void UIForm::ParseButton(ParserData* data, const std::string& element, const std::string& type)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 4)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[2];
		std::string label = parts[3];

        if (vPos.size() != 2)
            LogError("Invalid pos for button specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for button specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		RectangleShape<2, int> rect;

		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);

            rect.mExtent = geom;
            rect.mCenter = pos + geom / 2;
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)((atof(vGeom[0].c_str()) * mSpacing[0]) - (mSpacing[0] - mImageSize[0]));
			pos[1] += (int)((atof(vGeom[1].c_str()) * (float)mImageSize[1]) / 2);

            rect.mExtent[0] = geom[0];
            rect.mCenter[0] = pos[0] + geom[0] / 2;
            rect.mExtent[1] = 2 * mButtonHeight;
            rect.mCenter[1] = pos[1];
		}

		if(!data->mExplicitSize)
			LogWarning("invalid use of button without a size[] element");

		std::wstring wlabel = ToWideString(UnescapeString(label));

		Field field(name, wlabel, L"", 258 + (int)mFields.size());
		field.mType = FFT_BUTTON;
		if(type == "button_exit")
            field.mIsExit = true;

        std::shared_ptr<BaseUIButton> button = mUI->AddButton(
            rect, data->mCurrentParent, field.mId, field.mLabel.c_str());
		button->SetSubElement(true);

		auto style = GetStyleForElement(type, name, (type != "button") ? "button" : "");
		field.mSound = style[Style::STATE_DEFAULT].Get(Style::Property::SOUND, "");
        button->SetStyles(style);
		if (field.mName == mFocusedElement)
			mUI->SetFocus(button);

		mFields.push_back(field);
		return;
	}
	LogError("Invalid button element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseBackground(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 3) 
    {
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = UnescapeString(parts[2]);

        if (vPos.size() != 2)
            LogError("Invalid pos for background specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for background specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			pos[0] -= (int)((mSpacing[0] - mImageSize[0]) / 2.f);
			pos[1] -= (int)((mSpacing[1] - mImageSize[1]) / 2.f);

			geom[0] = (int)(atof(vGeom[0].c_str()) * mSpacing[0]);
			geom[1] = (int)(atof(vGeom[1].c_str()) * mSpacing[1]);
		}

		bool clip = false;
		if (parts.size() >= 4 && IsYes(parts[3])) 
        {
			if (data->mRealCoordinates) 
            {
				pos = GetRealCoordinateBasePosition(vPos) * -1;
				geom.MakeZero();
			} 
            else 
            {
				pos[0] = atoi(vPos[0].c_str()); //acts as mOffset
				pos[1] = atoi(vPos[1].c_str());
			}
			clip = true;
		}

		RectangleShape<2, int> middle;
		if (parts.size() >= 5) 
        {
			std::vector<std::string> vMiddle = Split(parts[4], ',');
			if (vMiddle.size() == 1)
            {
				int x = atoi(vMiddle[0].c_str());
                middle.mExtent = Vector2<int>{ 2 * x, 2 * x };
			} 
            else if (vMiddle.size() == 2) 
            {
				int x = atoi(vMiddle[0].c_str());
				int y =	atoi(vMiddle[1].c_str());
                middle.mExtent = Vector2<int>{ 2 * x, 2 * y };
				// `-x` is interpreted as `w - x`
			} 
            else if (vMiddle.size() == 4) 
            {
                Vector2<int> upperLeftCorner = Vector2<int>{ 
                    atoi(vMiddle[0].c_str()),  atoi(vMiddle[1].c_str()) };
                Vector2<int> lowerRightCorner = Vector2<int>{
                    atoi(vMiddle[2].c_str()),  atoi(vMiddle[3].c_str()) };
                middle.mExtent = lowerRightCorner - upperLeftCorner;
                middle.mCenter = upperLeftCorner + middle.mExtent / 2;
			} 
            else LogWarning("Invalid rectangle given to middle param of background[] element");
		}

		if (!data->mExplicitSize && !clip)
            LogWarning("invalid use of unclipped background without a size[] element");

		Field field(name, L"", L"", 258 + (int)mFields.size());

		RectangleShape<2, int> rect;
		if (!clip) 
        {
			// no auto_clip => position like normal image
            rect.mExtent = geom;
            rect.mCenter = pos + geom / 2;
		} 
        else 
        {
			// it will be auto-clipped when drawing
            rect.mExtent = 2 * pos;
		}

		std::shared_ptr<Texture2> texture;
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(name)));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			texture = resData->GetImage();
			texture->SetName(ToWideString(name));
		}

		if (!texture)
		{
			LogError("Unable to load texture: \t" + name);
			return;
		}
		texture->AutogenerateMipmaps();

        std::shared_ptr<BaseUIImage> image = mUI->AddImage(
            rect, shared_from_this(), field.mId, field.mLabel.c_str());
        LogAssert(image, "Failed to create background form element");
		image->SetSubElement(true);
        image->SetBackground(clip, middle);
        image->SetTexture(texture);
        image->SetNotClipped(true);
        image->SetVisible(false); // the element is drawn manually before all others

		mBackgrounds.push_back(image);
		mFields.push_back(field);
		return;
	}
	LogError("Invalid background element(" + 
        std::to_string(parts.size()) + "): '" + element + "'" );
}

void UIForm::ParseTableOptions(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	data->mTableOptions.clear();
	for (const std::string& part : parts) 
    {
		// Parse table option
		std::string opt = UnescapeString(part);
		data->mTableOptions.push_back(UITable::SplitOption(opt));
	}
}

void UIForm::ParseTableColumns(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	data->mTableColumns.clear();
	for (const std::string& part : parts) 
    {
		std::vector<std::string> colParts = Split(part,',');
		UITable::TableColumn column;
		// Parse column type
		if (!colParts.empty())
			column.mType = colParts[0];
		// Parse column options
		for (size_t j = 1; j < colParts.size(); ++j) 
        {
			std::string opt = UnescapeString(colParts[j]);
			column.mSettings.push_back(UITable::SplitOption(opt));
		}
		data->mTableColumns.push_back(column);
	}
}

void UIForm::ParseTable(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 4)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[2];
		std::vector<std::string> items = Split(parts[3],',');
		std::string strInitialSelection;
		std::string strTransparent = "false";

		if (parts.size() >= 5)
			strInitialSelection = parts[4];

        if (vPos.size() != 2)
            LogError("Invalid pos for table specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for table specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)(atof(vGeom[0].c_str()) * mSpacing[0]);
			geom[1] = (int)(atof(vGeom[1].c_str()) * mSpacing[1]);
		}

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

		Field field(name, L"", L"", 258 + (int)mFields.size());
		field.mType = FFT_TABLE;
		for (std::string& item : items) 
			item = UnescapeString(item);

		//now really show table
        std::shared_ptr<UITable> table = std::static_pointer_cast<UITable>(
            mUI->AddTable(mScaling, rect, data->mCurrentParent, field.mId));
		table->SetSubElement(true);
		if (field.mName == mFocusedElement)
            mUI->SetFocus(table);

        table->SetTable(data->mTableOptions, data->mTableColumns, items);
		if (data->mTableDynamicData.find(name) != data->mTableDynamicData.end())
            table->SetDynamicData(data->mTableDynamicData[name]);

		if (!strInitialSelection.empty() && strInitialSelection != "0")
            table->SetSelected(atoi(strInitialSelection.c_str()));

		auto style = GetDefaultStyleForElement("table", name);
        table->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        //table->SetOverrideFont(style.GetFont());

		mTables.emplace_back(field, table);
		mFields.push_back(field);
		return;
	}
	LogError("Invalid table element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseTextList(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 4)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[2];
		std::vector<std::string> items = Split(parts[3],',');
		std::string strInitialSelection;
		std::string strTransparent = "false";

		if (parts.size() >= 5)
			strInitialSelection = parts[4];

		if (parts.size() >= 6)
			strTransparent = parts[5];

        if (vPos.size() != 2)
            LogError("Invalid pos for textlist specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for textlist specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;

		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)(atof(vGeom[0].c_str()) * mSpacing[0]);
			geom[1] = (int)(atof(vGeom[1].c_str()) * mSpacing[1]);
		}

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

		Field field(name, L"", L"", 258 + (int)mFields.size());
        field.mType = FFT_TABLE;

		for (std::string& item : items)
			item = UnescapeString(item);

        //now really show list
        std::shared_ptr<UITable> table = std::static_pointer_cast<UITable>( 
            mUI->AddTable(mScaling, rect, data->mCurrentParent, field.mId));
		table->SetSubElement(true);
        if (field.mName == mFocusedElement)
            mUI->SetFocus(table);

        table->SetTextList(items, IsYes(strTransparent));
        if (data->mTableDynamicData.find(name) != data->mTableDynamicData.end())
            table->SetDynamicData(data->mTableDynamicData[name]);
        if (!strInitialSelection.empty() && strInitialSelection != "0")
            table->SetSelected(atoi(strInitialSelection.c_str()));

        auto style = GetDefaultStyleForElement("textlist", name);
        table->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        //table->SetOverrideFont(style.GetFont());

        mTables.emplace_back(field, table);
        mFields.push_back(field);
		return;
	}
    LogError("Invalid textlist element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseDropDown(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() >= 5)
	{
		std::vector<std::string> vPos = Split(parts[0], ',');
		std::string name = parts[2];
		std::vector<std::string> items = Split(parts[3], ',');
		std::string strInitialSelection = parts[4];

		if (parts.size() >= 6 && IsYes(parts[5]))
			mDropdownIndexEvent[name] = true;

        if (vPos.size() != 2)
            LogError("Invalid pos for dropdown specified: \"" + parts[0] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		RectangleShape<2, int> rect;
		if (data->mRealCoordinates) 
        {
			std::vector<std::string> vGeom = Split(parts[1],',');

			if (vGeom.size() == 1)
				vGeom.emplace_back("1");

            if (vGeom.size() != 2)
                LogError("Invalid geom for dropdown specified: \"" + parts[1] + "\"");

			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
            rect.mExtent = geom;
            rect.mCenter = pos + geom / 2;
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);

			int width = (int)(atof(parts[1].c_str()) * mSpacing[1]);

            rect.mExtent[0] = width;
            rect.mExtent[1] = mButtonHeight * 2;
            rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
            rect.mCenter[1] = pos[1] + rect.mExtent[1] / 2;
		}

		Field field(name, L"", L"", 258 + (int)mFields.size());
        field.mType = FFT_DROPDOWN;
		field.mSend = true;

		//now really show list
        std::shared_ptr<BaseUIComboBox> comboBox = 
			mUI->AddComboBox(rect, data->mCurrentParent, field.mId);
		comboBox->SetSubElement(true);
		if (field.mName == mFocusedElement) 
			mUI->SetFocus(comboBox);

		for (const std::string& item : items) 
			comboBox->AddItem(ToWideString(UnescapeString(item)).c_str());

		if (!strInitialSelection.empty())
			comboBox->SetSelected(atoi(strInitialSelection.c_str())-1);

		auto style = GetDefaultStyleForElement("dropdown", name);
		field.mSound = style.Get(Style::Property::SOUND, "");
		comboBox->SetNotClipped(style.GetBool(Style::NOCLIP, false));

		mFields.push_back(field);

		mDropdowns.emplace_back(field, std::vector<std::string>());
		std::vector<std::string>& values = mDropdowns.back().second;
		for (const std::string& item : items)
			values.push_back(UnescapeString(item));

		return;
	}
	LogError("Invalid dropdown element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseFieldCloseOnEnter(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');
	if (parts.size() >= 2)
		mFieldCloseOnEnter[parts[0]] = IsYes(parts[1]);
}

void UIForm::ParsePwdField(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 4)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[2];
		std::string label = parts[3];

        if (vPos.size() != 2)
            LogError("Invalid pos for pwdfield specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for pwdfield specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			pos -= mPadding;

			geom[0] = (int)((atof(vGeom[0].c_str()) * mSpacing[0]) - (mSpacing[0] - mImageSize[0]));

			pos[1] += (int)((atof(vGeom[1].c_str()) * (float)mImageSize[1]) / 2);
			pos[1] -= mButtonHeight;
			geom[1] = mButtonHeight*2;
		}

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

		std::wstring wlabel = ToWideString(UnescapeString(label));

		Field field(name, wlabel, L"", 258 + (int)mFields.size(), 0, CI_IBEAM);
		field.mSend = true;

        std::shared_ptr<BaseUIEditBox> editBox = mUI->AddEditBox(
            L"", rect, true, true, data->mCurrentParent, field.mId);
		editBox->SetSubElement(true);
		if (field.mName == mFocusedElement)
			mUI->SetFocus(editBox);

		if (label.length() >= 1) 
        {
            int fontHeight = Renderer::Get()->GetFont()->GetDimension( L" " )[1];
            rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1] - fontHeight / 2;
            rect.mExtent[1] = fontHeight;
			std::shared_ptr<BaseUIStaticText> staticText = mUI->AddStaticText(
				field.mLabel.c_str(), rect, false, true, data->mCurrentParent, 0);
			staticText->SetSubElement(true);
		}
        editBox->SetPasswordBox(true,L'*');

		auto style = GetDefaultStyleForElement("pwdfield", name, "field");
        editBox->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        editBox->SetDrawBorder(style.GetBool(Style::BORDER, true));
        editBox->SetOverrideColor(style.GetColor(Style::TEXTCOLOR, SColor(0xFFFFFFFF)));
        //editBox->SetOverrideFont(style.GetFont());

		Event evt;
		evt.mEventType             = ET_KEY_INPUT_EVENT;
		evt.mKeyInput.mKey         = KEY_END;
		evt.mKeyInput.mChar        = 0;
		evt.mKeyInput.mControl     = false;
		evt.mKeyInput.mShift       = false;
		evt.mKeyInput.mPressedDown = true;
        editBox->OnEvent(evt);

		// Note: Before 5.2.0 "parts.size() >= 5" resulted in a
		// warning referring to mFieldCloseOnEnter[]!

		mFields.push_back(field);
		return;
	}
	LogError("Invalid pwdfield element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::CreateTextField(ParserData *data, Field &field, RectangleShape<2, int> &rect, bool isMultiline)
{
	bool isEditable = !field.mName.empty();
	if (!isEditable && !isMultiline) 
    {
		int fontHeight = Renderer::Get()->GetFont()->GetDimension(L" ")[1];
		rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1] + fontHeight / 2;
		rect.mExtent[1] = fontHeight;

		//  field id to 0, this stops submit searching for a value that isn't there
		std::shared_ptr<BaseUIStaticText> staticText = mUI->AddStaticText(
			field.mLabel.c_str(), rect, false, true, data->mCurrentParent, 0);
		staticText->SetSubElement(true);
		return;
	}

	if (isEditable) 
    {
		field.mSend = true;
	} 
    else if (isMultiline && field.mDefault.empty() && !field.mLabel.empty()) 
    {
		// Multiline textareas: swap default and label for backwards compat
		field.mLabel.swap(field.mDefault);
	}

    std::shared_ptr<BaseUIEditBox> editBox;
	if (isMultiline) 
    {
        editBox = mUI->AddEditBox(field.mDefault.c_str(), rect, true, isEditable,
            data->mCurrentParent, field.mId);
		editBox->SetSubElement(true);
        editBox->SetMultiLine(true);
	} 
    else if (isEditable) 
    {
        editBox = mUI->AddEditBox(field.mDefault.c_str(), rect, true, true,
            data->mCurrentParent, field.mId);
		editBox->SetSubElement(true);
	}

	auto style = GetDefaultStyleForElement(isMultiline ? "textarea" : "field", field.mName);

	if (editBox) 
    {
		if (isEditable && field.mName == mFocusedElement)
			mUI->SetFocus(editBox);

		if (isMultiline) 
        {
            editBox->SetMultiLine(true);
            editBox->SetWordWrap(true);
            editBox->SetTextAlignment(UIA_UPPERLEFT, UIA_UPPERLEFT);
		} 
        else 
        {
			Event evt;
			evt.mEventType             = ET_KEY_INPUT_EVENT;
			evt.mKeyInput.mKey         = KEY_END;
			evt.mKeyInput.mChar        = 0;
			evt.mKeyInput.mControl     = 0;
			evt.mKeyInput.mShift       = 0;
			evt.mKeyInput.mPressedDown = true;
			editBox->OnEvent(evt);
		}

        editBox->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        editBox->SetDrawBorder(style.GetBool(Style::BORDER, true));
        editBox->SetOverrideColor(style.GetColor(Style::TEXTCOLOR, SColor(0xFFFFFFFF)));
		if (style.Get(Style::BGCOLOR, "") == "transparent") 
            editBox->SetDrawBackground(false);
        //editBox->SetOverrideFont(style.GetFont());
	}

	if (!field.mLabel.empty()) 
    {
        int fontHeight = Renderer::Get()->GetFont()->GetDimension(L" ")[1];
        rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1] - fontHeight / 2;
        rect.mExtent[1] = fontHeight;
        std::shared_ptr<BaseUIStaticText> text = mUI->AddStaticText(
            field.mLabel.c_str(), rect, false, true, data->mCurrentParent, 0);
		text->SetSubElement(true);
		if (text)
            text->SetNotClipped(style.GetBool(Style::NOCLIP, false));
	}
}

void UIForm::ParseSimpleField(ParserData *data, std::vector<std::string>& parts)
{
	std::string name = parts[0];
	std::string label = parts[1];
	std::string defaultVal = parts[2];

	if (data->mExplicitSize)
		LogWarning("invalid use of unpositioned \"field\"");

	Vector2<int> pos = GetElementBasePosition(nullptr);
	pos[1] = (data->mSimpleFieldCount + 2) * 60;

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ 300, mButtonHeight * 2 };
    rect.mCenter = Vector2<int>{ mDesiredRect.mExtent[0] / 2 - 150, pos[1] };

	if (mFormSource)
		defaultVal = mFormSource->ResolveText(defaultVal);

	std::wstring wlabel = ToWideString(UnescapeString(label));
	Field field(name, wlabel, ToWideString(UnescapeString(defaultVal)), 258 + (int)mFields.size(), 0, CI_IBEAM);
	CreateTextField(data, field, rect, false);
	mFields.push_back(field);

	data->mSimpleFieldCount++;
}

void UIForm::ParseTextArea(ParserData* data, std::vector<std::string>& parts, const std::string& type)
{
	std::vector<std::string> vPos = Split(parts[0],',');
	std::vector<std::string> vGeom = Split(parts[1],',');
	std::string name = parts[2];
	std::string label = parts[3];
	std::string defaultVal = parts[4];

    if (vPos.size() != 2)
        LogError("Invalid pos for " + type + " specified: \"" + parts[0] + "\"");
    if (vGeom.size() != 2)
        LogError("Invalid geom for " + type + " specified: \"" + parts[1] + "\"");

	Vector2<int> pos;
	Vector2<int> geom;
	if (data->mRealCoordinates) 
    {
		pos = GetRealCoordinateBasePosition(vPos);
		geom = GetRealCoordinateGeometry(vGeom);
	} 
    else
    {
		pos = GetElementBasePosition(&vPos);
		pos -= mPadding;

		geom[0] = (int)((atof(vGeom[0].c_str()) * mSpacing[0]) - (mSpacing[0] - mImageSize[0]));

		if (type == "textarea")
		{
			geom[1] = (int)((atof(vGeom[1].c_str()) * (float)mImageSize[1]) - (mSpacing[1]-mImageSize[1]));
			pos[1] += mButtonHeight;
		}
		else
		{
			pos[1] += (int)((atof(vGeom[1].c_str()) * (float)mImageSize[1]) / 2);
			pos[1] -= mButtonHeight;
			geom[1] = mButtonHeight*2;
		}
	}

    RectangleShape<2, int> rect;
    rect.mExtent = geom;
    rect.mCenter = pos + geom / 2;

    if (!data->mExplicitSize)
        LogWarning("invalid use of positioned " + type + " without a size[] element");

	if(mFormSource)
		defaultVal = mFormSource->ResolveText(defaultVal);

	std::wstring wlabel = ToWideString(UnescapeString(label));

	Field field(name, wlabel, ToWideString(UnescapeString(defaultVal)), 258 + (int)mFields.size(), 0, CI_IBEAM);
	CreateTextField(data, field, rect, type == "textarea");

	// Note: Before 5.2.0 "parts.size() >= 6" resulted in a
	// warning referring to mFieldCloseOnEnter[]!

	mFields.push_back(field);
}

void UIForm::ParseField(ParserData* data, const std::string& element, const std::string& type)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() == 3 || parts.size() == 4) 
    {
		ParseSimpleField(data,parts);
		return;
	}

	if (parts.size() >= 5)
	{
		ParseTextArea(data,parts,type);
		return;
	}
	LogError("Invalid field element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseHyperText(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');
	std::vector<std::string> vPos = Split(parts[0], ',');
	std::vector<std::string> vGeom = Split(parts[1], ',');
	std::string name = parts[2];
	std::string text = parts[3];

    if (vPos.size() != 2)
        LogError("Invalid pos for hypertext specified: \"" + parts[0] + "\"");
    if (vGeom.size() != 2)
        LogError("Invalid geom for hypertext specified: \"" + parts[1] + "\"");

	Vector2<int> pos;
	Vector2<int> geom;
	if (data->mRealCoordinates) 
    {
		pos = GetRealCoordinateBasePosition(vPos);
		geom = GetRealCoordinateGeometry(vGeom);
	} 
    else 
    {
		pos = GetElementBasePosition(&vPos);
		pos -= mPadding;

		geom[0] = (int)((atof(vGeom[0].c_str()) * mSpacing[0]) - (mSpacing[0] - mImageSize[0]));
		geom[1] = (int)((atof(vGeom[1].c_str()) * (float)mImageSize[1]) - (mSpacing[1] - mImageSize[1]));
		pos[1] += mButtonHeight;
	}

    RectangleShape<2, int> rect;
    rect.mExtent = geom;
    rect.mCenter = pos + geom / 2;
	if(mFormSource)
		text = mFormSource->ResolveText(text);

	Field field(name, ToWideString(UnescapeString(text)), L"", 258 + (int)mFields.size());
	field.mType = FFT_HYPERTEXT;

	auto style = GetDefaultStyleForElement("hypertext", field.mName);
	field.mSound = style.Get(Style::Property::SOUND, "");

	std::shared_ptr<BaseUIHyperText> hypertext = mUI->AddHypertext(
        rect, data->mCurrentParent, field.mId, field.mLabel.c_str());
	hypertext->SetSubElement(true);

	mFields.push_back(field);
}

void UIForm::ParseLabel(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 2)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::string text = parts[1];

        if (vPos.size() != 2)
            LogError("Invalid pos for label specified: \"" + parts[0] + "\"");

		if(!data->mExplicitSize)
			LogWarning("invalid use of label without a Size[] element");

		std::vector<std::string> lines = Split(text, '\n');

		auto style = GetDefaultStyleForElement("label", "");
        std::shared_ptr<BaseUIFont> font;// = style.GetFont();
		if (!font)
			font = mFont;

		for (unsigned int i = 0; i != lines.size(); i++) 
        {
			std::wstring labelColors = ToWideString(UnescapeString(lines[i]));
			// Without color escapes to get the font dimensions
			std::wstring labelPlain = UnescapeEnriched(labelColors);

			RectangleShape<2, int> rect;

			if (data->mRealCoordinates) 
            {
				// Lines are spaced at the distance of 1/2 image size.
				// This alows lines that line up with the new elements
				// easily without sacrificing good line distance.  If
				// it was one whole image size, it would have too much
				// spacing.
				Vector2<int> pos = GetRealCoordinateBasePosition(vPos);

				// Labels are positioned by their center, not their top.
				pos[1] += ( mImageSize[1] / -2) + (mImageSize[1] * i / 2);

                rect.mExtent[0] = font->GetDimension(labelPlain.c_str())[0];
                rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
                rect.mExtent[1] = mImageSize[1];
                rect.mCenter[1] = pos[1] + rect.mExtent[1] / 2;
			} 
            else 
            {
				// Lines are spaced at the nominal distance of
				// 2/5 slot, even if the font doesn't
				// quite match that.  This provides consistent
				// form layout, at the expense of sometimes
				// having sub-optimal spacing for the font.
				// We multiply by 2 and then divide by 5, rather
				// than multiply by 0.4, to get exact results
				// in the integer cases: 0.4 is not exactly
				// representable in binary floating point.

				Vector2<int> pos = GetElementBasePosition(nullptr);
				pos[0] += (int)(atof(vPos[0].c_str()) * mSpacing[0]);
				pos[1] += (int)((atof(vPos[1].c_str()) + 7.0f / 30.0f) * mSpacing[1]);

				pos[1] += (int)(i * mSpacing[1] * 2.0f / 5.0f);

                rect.mExtent[0] = font->GetDimension(labelPlain)[0];
                rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
                rect.mExtent[1] = 2 * mButtonHeight;
                rect.mCenter[1] = pos[1];
			}

			Field field("", labelColors, L"", 258 + (int)mFields.size(), 4);
			std::shared_ptr<BaseUIStaticText> staticText = mUI->AddStaticText(
                field.mLabel.c_str(), rect, false, false, data->mCurrentParent, field.mId);
			staticText->SetSubElement(true);
            staticText->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);
            staticText->SetNotClipped(style.GetBool(Style::NOCLIP, false));
            staticText->SetOverrideColor(style.GetColor(Style::TEXTCOLOR, SColor(0xFFFFFFFF)));
            staticText->SetOverrideFont(font);

			mFields.push_back(field);

			// labels should let events through
			mClickThroughElements.push_back(staticText);
		}

		return;
	}
	LogError("Invalid label element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseVertLabel(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 2)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::wstring text = ToWideString(UnescapeString(parts[1]));

        if (vPos.size() != 2)
            LogError("Invalid pos for vertlabel specified: \"" + parts[1] + "\"");

		auto style = GetDefaultStyleForElement("vertlabel", "", "label");
        std::shared_ptr<BaseUIFont> font;// = style.GetFont();
		if (!font)
			font = mFont;

		Vector2<int> pos;
		RectangleShape<2, int> rect;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);

			// Vertlabels are positioned by center, not left.
			pos[0] -= mImageSize[0] / 2;

			// We use text.length + 1 because without it, the rect
			// isn't quite tall enough and cuts off the text.
            rect.mExtent[0] = mImageSize[0];
            rect.mExtent[1] = FontLineHeight(font) * ((unsigned int)text.length() + 1);
            rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
            rect.mCenter[1] = pos[1] + rect.mExtent[1] / 2;
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);

			// As above, the length must be one longer. The width of
			// the rect (15 pixels) seems rather arbitrary, but
			// changing it might break something.
            rect.mExtent[0] = 15;
            rect.mCenter[0] = pos[0] + rect.mExtent[0] / 2;
            rect.mExtent[1] = FontLineHeight(font) * ((unsigned int)text.length() + 1);
            rect.mCenter[1] = pos[1]  + ((mImageSize[1] / 2) - mButtonHeight) + rect.mExtent[1] / 2;
		}

		if(!data->mExplicitSize)
			LogWarning("invalid use of label without a size[] element");

		std::wstring label;

		for (wchar_t i : text) 
        {
			label += i;
			label += L"\n";
		}

		Field field("", label, L"", 258 + (int)mFields.size());
        std::shared_ptr<BaseUIStaticText> staticText = mUI->AddStaticText(
            field.mLabel.c_str(), rect, false, false, data->mCurrentParent, field.mId);
		staticText->SetSubElement(true);
        staticText->SetTextAlignment(UIA_CENTER, UIA_CENTER);
        staticText->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        staticText->SetOverrideColor(style.GetColor(Style::TEXTCOLOR, SColor(0xFFFFFFFF)));
        staticText->SetOverrideFont(font);

		mFields.push_back(field);

		// vertlabels should let events through
		mClickThroughElements.push_back(staticText);
		return;
	}
	LogError( "Invalid vertlabel element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseImageButton(ParserData* data, const std::string& element, const std::string& type)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 5)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string imageName = parts[2];
		std::string name = parts[3];
		std::string label = parts[4];

        if (vPos.size() != 2)
            LogError("Invalid pos for imagebutton specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for imagebutton specified: \"" + parts[1] + "\"");

		std::string pressedImageName;

		if (parts.size() >= 8)
			pressedImageName = parts[7];

		Vector2<int> pos;
		Vector2<int> geom;

		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)((atof(vGeom[0].c_str()) * mSpacing[0]) - (mSpacing[0] - mImageSize[0]));
			geom[1] = (int)((atof(vGeom[1].c_str()) * mSpacing[1]) - (mSpacing[1] - mImageSize[1]));
		}

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

		if (!data->mExplicitSize)
			LogWarning("invalid use of image_button without a size[] element");

		std::wstring wlabel = ToWideString(UnescapeString(label));
		Field field(name, wlabel, ToWideString(UnescapeString(imageName)), 258 + (int)mFields.size());
		field.mType = FFT_BUTTON;
		if (type == "image_button_exit")
            field.mIsExit = true;

        std::shared_ptr<BaseUIButton> buttonImage = mUI->AddButton(
            rect, data->mCurrentParent, field.mId, field.mLabel.c_str(), 0, false, true);
		buttonImage->SetSubElement(true);
		if (field.mName == mFocusedElement)
			mUI->SetFocus(buttonImage);

		auto style = GetStyleForElement("image_button", field.mName);
		field.mSound = style[Style::STATE_DEFAULT].Get(Style::Property::SOUND, "");

		// Override style properties with values ified directly in the element
		if (!imageName.empty())
			style[Style::STATE_DEFAULT].Set(Style::FGIMG, imageName);

		if (!pressedImageName.empty())
			style[Style::STATE_PRESSED].Set(Style::FGIMG, pressedImageName);

		if (parts.size() >= 7) 
        {
			style[Style::STATE_DEFAULT].Set(Style::NOCLIP, parts[5]);
			style[Style::STATE_DEFAULT].Set(Style::BORDER, parts[6]);
		}
        buttonImage->SetStyles(style);

		mFields.push_back(field);
		return;
	}
	LogError("Invalid imagebutton element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseTabHeader(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() >= 4)
	{
		std::vector<std::string> vPos = Split(parts[0],',');

		// If we're using real coordinates, add an extra field for height.
		// Width is not here because tabs are the width of the text, and
		// there's no reason to change that.
		unsigned int i = 0;
		std::vector<std::string> vGeom = {"1", "1"}; // Dummy width and height
		bool autoWidth = true;
		if (parts.size() == 7) 
        {
			i++;

			vGeom = Split(parts[1], ',');
			if (vGeom.size() == 1)
				vGeom.insert(vGeom.begin(), "1"); // Dummy value
			else
				autoWidth = false;
		}

		std::string name = parts[i+1];
		std::vector<std::string> buttons = Split(parts[i+2], ',');
		std::string strIndex = parts[i+3];
		bool showBackground = true;
		bool showBorder = true;
		int tabIndex = atoi(strIndex.c_str()) - 1;

        if (vPos.size() != 2)
            LogError("Invalid pos for tabheader specified: \"" + parts[0] + "\"");

		if (parts.size() == 6 + i) 
        {
			if (parts[4+i].compare("true") == 0)
				showBackground = false;
			if (parts[5+i].compare("false") == 0)
				showBorder = false;
		}

		Field field(name, L"", L"", 258 + (int)mFields.size());
        field.mType = FFT_TABHEADER;

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);

			geom = GetRealCoordinateGeometry(vGeom);
			// Set default height
			if (parts.size() <= 6)
				geom[1] = mButtonHeight * 2;
			pos[1] -= geom[1]; // TabHeader base pos is the bottom, not the top.
            if (autoWidth)
                geom[0] = mDesiredRect.mExtent[0]; // Set automatic width

            if (vGeom.size() != 2)
                LogError("Invalid geom for tabheader specified: \"" + parts[1] + "\"");
		} 
        else 
        {
			Vector2<float> basePos = mPosOffset * mSpacing;
			basePos[0] += (int)(atof(vPos[0].c_str()) * mSpacing[0]);
			basePos[1] += (int)(atof(vPos[1].c_str()) * mSpacing[1] - mButtonHeight * 2);
            pos = Vector2<int>{ (int)basePos[0], (int)basePos[1] };

			geom[1] = mButtonHeight * 2;
			geom[0] = mDesiredRect.mExtent[0];
		}

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

        std::shared_ptr<BaseUITabControl> tabControl = 
            mUI->AddTabControl(rect, data->mCurrentParent, showBackground, showBorder, field.mId);
		tabControl->SetSubElement(true);
        tabControl->SetAlignment(UIA_UPPERLEFT, UIA_UPPERLEFT, UIA_UPPERLEFT, UIA_LOWERRIGHT);
        tabControl->SetTabHeight(geom[1]);

		auto style = GetDefaultStyleForElement("tabheader", name);
		field.mSound = style.Get(Style::Property::SOUND, "");
        tabControl->SetNotClipped(style.GetBool(Style::NOCLIP, true));

        int tabIdx = 0;
		for (const std::string& button : buttons) 
        {
			auto tab = tabControl->AddTab(
                ToWideString(UnescapeString(button)).c_str(), tabIdx++, tabIdx==tabIndex);
			if (style.IsNotDefault(Style::BGCOLOR))
				tab->SetBackgroundColor(style.GetColor(Style::BGCOLOR));

			tab->SetTextColor(style.GetColor(Style::TEXTCOLOR, SColor(0xFFFFFFFF)));
		}

		mFields.push_back(field);
		return;
	}
	LogError("Invalid TabHeader element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseBox(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() >= 3)
	{
		std::vector<std::string> vPos = Split(parts[0], ',');
		std::vector<std::string> vGeom = Split(parts[1], ',');

        if (vPos.size() != 2)
            LogError("Invalid pos for box specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for box specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)(atof(vGeom[0].c_str()) * mSpacing[0]);
			geom[1] = (int)(atof(vGeom[1].c_str()) * mSpacing[1]);
		}

		Field field("", L"", L"", 258 + (int)mFields.size(), -2);
		field.mType = FFT_BOX;

		auto style = GetDefaultStyleForElement("box", field.mName);

		SColor tmpColor;
		std::array<SColor, 4> colors;
		std::array<SColor, 4> borderColors = {0x0, 0x0, 0x0, 0x0};
		std::array<int, 4> borderWidths = {0, 0, 0, 0};

		if (ParseColorString(parts[2], tmpColor, true, 0x8C)) 
        {
			colors = { tmpColor, tmpColor, tmpColor, tmpColor };
		} 
        else 
        {
			colors = style.GetColorArray(Style::COLORS, {0x0, 0x0, 0x0, 0x0});
			borderColors = style.GetColorArray(Style::BORDERCOLORS,
				{0x0, 0x0, 0x0, 0x0});
			borderWidths = style.GetIntArray(Style::BORDERWIDTHS, {0, 0, 0, 0});
		}

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

        std::shared_ptr<BaseUIBox> box = mUI->AddBox(
            rect, colors, borderColors, borderWidths, data->mCurrentParent, field.mId);
		box->SetSubElement(true);
        box->SetNotClipped(style.GetBool(Style::NOCLIP, mFormVersion < 3));

		mFields.push_back(field);
		return;
	}
	LogError("Invalid Box element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIForm::ParseBackgroundColor(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');
	const unsigned int parameterCount = (unsigned int)parts.size();

	if (parameterCount > 2) 
    {
		LogError("Invalid bgcolor element(" + std::to_string(parameterCount) + "): '" + element + "'");
		return;
	}

	// bgcolor
	if (parameterCount >= 1 && parts[0] != "")
		ParseColorString(parts[0], mBGColor, false);

	// fullscreen
	if (parameterCount >= 2) 
    {
		if (parts[1] == "both") 
        {
			mBGNonFullscreen = true;
			mBGFullscreen = true;
		} 
        else if (parts[1] == "neither") 
        {
			mBGNonFullscreen = false;
			mBGFullscreen = false;
		} 
        else if (parts[1] != "")
        {
			mBGFullscreen = IsYes(parts[1]);
			mBGNonFullscreen = !mBGFullscreen;
		}
	}

	// fbgcolor
	if (parameterCount >= 3 && parts[2] != "")
		ParseColorString(parts[2], mFullscreenBGColor, false);
}

void UIForm::ParseTooltip(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');
	if (parts.size() < 2) 
    {
		LogError("Invalid tooltip element(" + std::to_string(parts.size()) + "): '" + element + "'");
		return;
	}

	// Get mode and check size
	bool rectMode = parts[0].find(',') != std::string::npos;
	size_t baseSize = rectMode ? 3 : 2;
	if (parts.size() != baseSize && parts.size() != baseSize + 2) 
    {
		LogError("Invalid tooltip element(" + std::to_string(parts.size()) + "): '" + element + "'");
		return;
	}

	// Read colors
	SColor bgcolor = mDefaultTooltipBGColor;
	SColor color   = mDefaultTooltipColor;
	if (parts.size() == baseSize + 2 &&
        (!ParseColorString(parts[baseSize], bgcolor, false) ||
        !ParseColorString(parts[baseSize + 1], color, false))) 
    {
		LogError("Invalid color in tooltip element(" + std::to_string(parts.size()) + "): '" + element + "'");
		return;
	}

	// Make tooltip 
	std::string text = UnescapeString(parts[rectMode ? 2 : 1]);
	Tooltip toolTip(ToWideString(text), bgcolor, color);

	// Add tooltip
	if (rectMode) 
    {
		std::vector<std::string> vPos  = Split(parts[0], ',');
		std::vector<std::string> vGeom = Split(parts[1], ',');

        if (vPos.size() != 2)
            LogError("Invalid pos for tooltip specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for tooltip specified: \"" + parts[1] + "\"");

		Vector2<int> pos;
		Vector2<int> geom;
		if (data->mRealCoordinates) 
        {
			pos = GetRealCoordinateBasePosition(vPos);
			geom = GetRealCoordinateGeometry(vGeom);
		} 
        else 
        {
			pos = GetElementBasePosition(&vPos);
			geom[0] = (int)(atof(vGeom[0].c_str()) * mSpacing[0]);
			geom[1] = (int)(atof(vGeom[1].c_str()) * mSpacing[1]);
		}

		Field field("", L"", L"", 258 + (int)mFields.size() );

        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + geom / 2;

		std::shared_ptr<BaseUIElement> element = 
            std::make_shared<BaseUIElement>(UIET_ELEMENT, field.mId, rect);
        element->SetParent(data->mCurrentParent);
		element->SetSubElement(true);

		// the element the rect tooltip is bound to should not block mouse-clicks
        element->SetVisible(false);

		mFields.push_back(field);
		mTooltipRects.emplace_back(element, toolTip);

	} else mTooltips[parts[0]] = toolTip;
}

bool UIForm::ParseVersionDirect(const std::string& data)
{
	//some prechecks
	if (data.empty())
		return false;

	std::vector<std::string> parts = Split(data,'[');
	if (parts.size() < 2)
		return false;

	if (Trim(parts[0]) != "form_version") {
		return false;
	}

	if (IsNumber(parts[1])) 
    {
		mFormVersion = atoi(parts[1].c_str());
		return true;
	}

	return false;
}

bool UIForm::ParseSizeDirect(ParserData* data, const std::string& element)
{
	if (element.empty())
		return false;

	std::vector<std::string> parts = Split(element,'[');

	if (parts.size() < 2)
		return false;

	std::string type = Trim(parts[0]);
	std::string description = Trim(parts[1]);

	if (type != "size" && type != "invsize")
		return false;

	if (type == "invsize")
		LogWarning("Deprecated form element \"invsize\" is used");

	ParseSize(data, description);
	return true;
}

bool UIForm::ParsePositionDirect(ParserData *data, const std::string& element)
{
	if (element.empty())
		return false;

	std::vector<std::string> parts = Split(element, '[');

	if (parts.size() != 2)
		return false;

	std::string type = Trim(parts[0]);
	std::string description = Trim(parts[1]);

	if (type != "position")
		return false;

	ParsePosition(data, description);
	return true;
}

void UIForm::ParsePosition(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ',');

	if (parts.size() == 2) 
    {
		data->mOffset[0] = (float)(atof(parts[0].c_str()));
		data->mOffset[1] = (float)(atof(parts[1].c_str()));
		return;
	}

	LogError("Invalid position element (" + std::to_string(parts.size()) + "): '" + element + "'");
}

bool UIForm::ParseAnchorDirect(ParserData *data, const std::string& element)
{
	if (element.empty())
		return false;

	std::vector<std::string> parts = Split(element, '[');

	if (parts.size() != 2)
		return false;

	std::string type = Trim(parts[0]);
	std::string description = Trim(parts[1]);

	if (type != "anchor")
		return false;

	ParseAnchor(data, description);

	return true;
}

void UIForm::ParseAnchor(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ',');

	if (parts.size() == 2) 
    {
		data->mAnchor[0] = (float)(atof(parts[0].c_str()));
		data->mAnchor[1] = (float)(atof(parts[1].c_str()));
		return;
	}

	LogError("Invalid mAnchor element (" + std::to_string(parts.size()) + "): '" + element + "'");
}

bool UIForm::ParseStyle(ParserData *data, const std::string& element, bool styleType)
{
	std::vector<std::string> parts = Split(element, ';');

	if (parts.size() < 2) 
    {
		LogError("Invalid style element (" + std::to_string(parts.size()) + "): '" + element + "'");
		return false;
	}

	Style style;

	// Parse properties
	for (size_t i = 1; i < parts.size(); i++) 
    {
		size_t equalPos = parts[i].find('=');
		if (equalPos == std::string::npos) 
        {
			LogError("Invalid style element (Property missing value): '" + element + "'");
			return false;
		}

		std::string propname = Trim(parts[i].substr(0, equalPos));
		std::string value    = Trim(UnescapeString(parts[i].substr(equalPos + 1)));

		std::transform(propname.begin(), propname.end(), propname.begin(), ::tolower);

		Style::Property prop = Style::GetPropertyByName(propname);
		if (prop == Style::NONE) 
        {
			if (mPropertyWarned.find(propname) != mPropertyWarned.end()) 
            {
				LogWarning("Invalid style element (Unknown property " + propname + "): '" + element + "'");
				mPropertyWarned.insert(propname);
			}
			continue;
		}

        style.Set(prop, value);
	}

	std::vector<std::string> selectors = Split(parts[0], ',');
	for (size_t sel = 0; sel < selectors.size(); sel++) 
    {
		std::string selector = Trim(selectors[sel]);

		// Copy the style properties to a new Style
		// This allows a separate state mask per-selector
		Style selectorStyle = style;

		// Parse state information, if it exists
		bool stateValid = true;
		size_t statePos = selector.find(':');
		if (statePos != std::string::npos) 
        {
			std::string stateStr = selector.substr(statePos + 1);
			selector = selector.substr(0, statePos);

			if (stateStr.empty()) 
            {
				LogError("Invalid style element (Invalid state): '" + element + "'");
				stateValid = false;
			} 
            else 
            {
				std::vector<std::string> states = Split(stateStr, '+');
				for (std::string& state : states) 
                {
					Style::State converted = Style::GetStateByName(state);
					if (converted == Style::STATE_INVALID) 
                    {
						LogInformation("Unknown style state " + state + " in element '" + element + "'");
						stateValid = false;
						break;
					}

					selectorStyle.AddState(converted);
				}
			}
		}

		if (!stateValid) 
        {
			// Skip this selector
			continue;
		}

		if (styleType) 
			mThemeByType[selector].push_back(selectorStyle);
        else 
			mThemeByName[selector].push_back(selectorStyle);

		// Backwards-compatibility for existing _hovered/_pressed properties
		if (selectorStyle.HasProperty(Style::BGCOLOR_HOVERED) ||
            selectorStyle.HasProperty(Style::BGIMG_HOVERED) || 
            selectorStyle.HasProperty(Style::FGIMG_HOVERED)) 
        {
			Style hoverStyle;
            hoverStyle.AddState(Style::STATE_HOVERED);

			if (selectorStyle.HasProperty(Style::BGCOLOR_HOVERED)) 
                hoverStyle.Set(Style::BGCOLOR, selectorStyle.Get(Style::BGCOLOR_HOVERED, ""));
			if (selectorStyle.HasProperty(Style::BGIMG_HOVERED)) 
                hoverStyle.Set(Style::BGIMG, selectorStyle.Get(Style::BGIMG_HOVERED, ""));
			if (selectorStyle.HasProperty(Style::FGIMG_HOVERED)) 
                hoverStyle.Set(Style::FGIMG, selectorStyle.Get(Style::FGIMG_HOVERED, ""));

			if (styleType)
				mThemeByType[selector].push_back(hoverStyle);
            else
				mThemeByName[selector].push_back(hoverStyle);
		}
		if (selectorStyle.HasProperty(Style::BGCOLOR_PRESSED) || 
            selectorStyle.HasProperty(Style::BGIMG_PRESSED) ||
            selectorStyle.HasProperty(Style::FGIMG_PRESSED)) 
        {
			Style pressStyle;
            pressStyle.AddState(Style::STATE_PRESSED);

			if (selectorStyle.HasProperty(Style::BGCOLOR_PRESSED))
                pressStyle.Set(Style::BGCOLOR, selectorStyle.Get(Style::BGCOLOR_PRESSED, ""));
			if (selectorStyle.HasProperty(Style::BGIMG_PRESSED))
                pressStyle.Set(Style::BGIMG, selectorStyle.Get(Style::BGIMG_PRESSED, ""));
			if (selectorStyle.HasProperty(Style::FGIMG_PRESSED))
                pressStyle.Set(Style::FGIMG, selectorStyle.Get(Style::FGIMG_PRESSED, ""));

			if (styleType)
				mThemeByType[selector].push_back(pressStyle);
            else
				mThemeByName[selector].push_back(pressStyle);
		}
	}

	return true;
}

void UIForm::ParseSetFocus(const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

	if (mIsFormRegenerated)
		return; // Never focus on resizing

	bool forceFocus = parts.size() >= 2 && IsYes(parts[1]);
	if (forceFocus || mTextDst->mFormName != mLastFormName)
		SetFocus(parts[0]);
}

void UIForm::ParseElement(ParserData* data, const std::string& element)
{
	//some prechecks
	if (element.empty())
		return;

	if (ParseVersionDirect(element))
		return;

	size_t pos = element.find('[');
	if (pos == std::string::npos)
		return;

	std::string type = Trim(element.substr(0, pos));
	std::string description = element.substr(pos+1);

	if (type == "container") 
    {
		ParseContainer(data, description);
		return;
	}

	if (type == "container_end") 
    {
		ParseContainerEnd(data);
		return;
	}

	if (type == "list") 
    {
		ParseList(data, description);
		return;
	}

	if (type == "checkbox") 
    {
		ParseCheckbox(data, description);
		return;
	}

	if (type == "image") 
    {
		ParseImage(data, description);
		return;
	}

	if (type == "animated_image") 
    {
		ParseAnimatedImage(data, description);
		return;
	}

	if (type == "button" || type == "button_exit") 
    {
		ParseButton(data, description, type);
		return;
	}

	if (type == "background" || type == "background9") 
    {
		ParseBackground(data, description);
		return;
	}

	if (type == "tableoptions")
    {
		ParseTableOptions(data, description);
		return;
	}

	if (type == "tablecolumns")
    {
		ParseTableColumns(data, description);
		return;
	}

	if (type == "table")
    {
		ParseTable(data, description);
		return;
	}

	if (type == "textlist")
    {
		ParseTextList(data, description);
		return;
	}

	if (type == "dropdown")
    {
		ParseDropDown(data, description);
		return;
	}

	if (type == "field_close_on_enter") 
    {
		ParseFieldCloseOnEnter(data, description);
		return;
	}

	if (type == "pwdfield") 
    {
		ParsePwdField(data, description);
		return;
	}

	if ((type == "field") || (type == "textarea"))
    {
		ParseField(data, description, type);
		return;
	}

	if (type == "hypertext") 
    {
		ParseHyperText(data, description);
		return;
	}

	if (type == "label") 
    {
		ParseLabel(data, description);
		return;
	}

	if (type == "vertlabel") 
    {
		ParseVertLabel(data, description);
		return;
	}

	if ((type == "image_button") || (type == "image_button_exit")) 
    {
		ParseImageButton(data, description, type);
		return;
	}

	if (type == "tabheader") 
    {
		ParseTabHeader(data, description);
		return;
	}

	if (type == "box") 
    {
		ParseBox(data, description);
		return;
	}

	if (type == "bgcolor") 
    {
		ParseBackgroundColor(data, description);
		return;
	}

	if (type == "tooltip") 
    {
		ParseTooltip(data, description);
		return;
	}

	if (type == "scrollbar") 
    {
		ParseScrollBar(data, description);
		return;
	}

	if (type == "real_coordinates") 
    {
		data->mRealCoordinates = IsYes(description);
		return;
	}

	if (type == "style") 
    {
		ParseStyle(data, description, false);
		return;
	}

	if (type == "style_type") 
    {
		ParseStyle(data, description, true);
		return;
	}

	if (type == "scrollbaroptions") 
    {
		ParseScrollBarOptions(data, description);
		return;
	}

	if (type == "scroll_container") 
    {
		ParseScrollContainer(data, description);
		return;
	}

	if (type == "scroll_container_end") 
    {
		ParseScrollContainerEnd(data);
		return;
	}

	if (type == "set_focus") 
    {
		ParseSetFocus(description);
		return;
	}

	// Ignore others
	LogInformation("Unknown Draw: type=" + type + ", data=\"" + description + "\"");
}

void UIForm::RegenerateUI(Vector2<unsigned int> screenSize)
{
	// Useless to regenerate without a screenSize
	if ((screenSize[0] <= 0) || (screenSize[1] <= 0)) {
		return;
	}

	ParserData data;

	// Preserve stuff only on same form, not on a new form.
	if (mTextDst->mFormName == mLastFormName) 
    {
		// Preserve tables/textlists
		for (auto const& table : mTables)
        {
			std::string tableName = table.first.mName;
			std::shared_ptr<UITable> currentTable = table.second;
			data.mTableDynamicData[tableName] = currentTable->GetDynamicData();
		}

		// Preserve focus
		std::shared_ptr<BaseUIElement> focusedElement = mUI->GetFocus();
		if (focusedElement && focusedElement->GetParent() == shared_from_this())
        {
			int focusedId = focusedElement->GetID();
			if (focusedId > 257)
            {
				for (const UIForm::Field &field : mFields) 
                {
					if (field.mId == focusedId)
                    {
						mFocusedElement = field.mName;
						break;
					}
				}
			}
		}
	} 
    else 
    {
		// Don't keep old focus value
		mFocusedElement = "";
	}

	// Remove children
    RemoveChildren(true);

    if (mTooltipElement)
    {
        mTooltipElement->Remove();
        mTooltipElement = nullptr;
    }

    data.mSize = Vector2<int>{ 100, 100 };
	data.mScreenSize = screenSize;
    data.mOffset = Vector2<float>{ 0.5f, 0.5f };
    data.mAnchor = Vector2<float>{ 0.5f, 0.5f };
	data.mSimpleFieldCount = 0;

	// Base position of contents of form
	data.mBasePos = GetBasePosition();

	// the parent for the parsed elements
	data.mCurrentParent = shared_from_this();

	mBackgrounds.clear();
	mTables.clear();
	mCheckboxes.clear();
	mScrollbars.clear();
	mFields.clear();
	mTooltips.clear();
	mTooltipRects.clear();
	mDropdowns.clear();
	mScrollContainers.clear();
	mThemeByName.clear();
	mThemeByType.clear();
	mClickThroughElements.clear();
	mFieldCloseOnEnter.clear();
	mDropdownIndexEvent.clear();

	mBGNonFullscreen = true;
	mBGFullscreen = false;

	mFormVersion = 1;

	{
		Vector4<short> formBGColor = Settings::Get()->GetVector4("form_default_bg_color");
		mBGColor = SColor(
			(std::min(std::max((int)round(formBGColor[0]), 0), 255)),
			(std::min(std::max((int)round(formBGColor[1]), 0), 255)),
			(std::min(std::max((int)round(formBGColor[2]), 0), 255)),
			(std::min(std::max((int)round(formBGColor[3]), 0), 255)));
	}

	{
		Vector4<short> formBGColor = Settings::Get()->GetVector4("form_fullscreen_bg_color");
		mFullscreenBGColor = SColor(
			(std::min(std::max((int)round(formBGColor[0]), 0), 255)),
			(std::min(std::max((int)round(formBGColor[1]), 0), 255)),
			(std::min(std::max((int)round(formBGColor[2]), 0), 255)),
			(std::min(std::max((int)round(formBGColor[3]), 0), 255)));
	}

	mDefaultTooltipBGColor = SColor(255,110,130,60);
	mDefaultTooltipColor = SColor(255,255,255,255);

	// Add tooltip
	{
		LogAssert(!mTooltipElement, "invalid tooltip element");
		// Note: parent != this so that the tooltip isn't clipped by the  rectangle
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{110, 18};
        rect.mCenter = rect.mExtent / 2;
		mTooltipElement = mUI->AddStaticText(L"", rect);
		mTooltipElement->EnableOverrideColor(true);
		mTooltipElement->SetBackgroundColor(mDefaultTooltipBGColor);
		mTooltipElement->SetDrawBackground(true);
		mTooltipElement->SetDrawBorder(true);
		mTooltipElement->SetOverrideColor(mDefaultTooltipColor);
		//mTooltipElement->SetTextAlignment(UIA_CENTER, UIA_CENTER);
		mTooltipElement->SetWordWrap(true);
	}
	std::vector<std::string> elements = Split(mFormString,']');

	/* try to read version from first element only */
    unsigned int i = 0;
	if (!elements.empty())
		if (ParseVersionDirect(elements[0]))
			i++;

	/* we need Size first in order to calculate image scale */
	data.mExplicitSize = false;
	for (; i< elements.size(); i++)
		if (!ParseSizeDirect(&data, elements[i]))
			break;

	/* "position" element is always after "size" element if it used */
	for (; i< elements.size(); i++)
		if (!ParsePositionDirect(&data, elements[i]))
			break;

	/* "mAnchor" element is always after "position" (or  "size" element) if it used */
	for (; i< elements.size(); i++)
		if (!ParseAnchorDirect(&data, elements[i]))
			break;

	/* "no_prepend" element is always after "position" (or  "size" element) if it used */
	bool enablePrepends = true;
	for (; i < elements.size(); i++)
    {
		if (elements[i].empty())
			break;

		std::vector<std::string> parts = Split(elements[i], '[');
		if (Trim(parts[0]) == "no_prepend")
            enablePrepends = false;
		else
			break;
	}

	/* Copy of the "real_coordinates" element for after the form size. */
	data.mRealCoordinates = mFormVersion >= 2;
	for (; i < elements.size(); i++) 
    {
		std::vector<std::string> parts = Split(elements[i], '[');
		std::string name = Trim(parts[0]);
		if (name != "real_coordinates" || parts.size() != 2)
			break; // Invalid format

		data.mRealCoordinates = IsYes(Trim(parts[1]));
	}

	if (data.mExplicitSize) 
    {
		// compute scaling for ified form Size
		if (mLock) 
        {
            Renderer* renderer = Renderer::Get();
            Vector2<unsigned int> screenSize(renderer->GetScreenSize());
			Vector2<unsigned int> delta = screenSize - mLockScreenSize;

			if (screenSize[1] > mLockScreenSize[1])
				delta[1] /= 2;
			else
				delta[1] = 0;

			if (screenSize[0] > mLockScreenSize[0])
				delta[0] /= 2;
			else
				delta[0] = 0;

            mOffset = Vector2<int>{ (int)delta[0], (int)delta[1] };

            data.mScreenSize = mLockScreenSize;
		} 
        else mOffset.MakeZero();

        double scaling = Settings::Get()->GetFloat("gui_scaling");
		double screenDPI = System::Get()->GetDisplayDensity() * 96;

		double useImageSize;
		if (mLock) 
        {
			// In fixed-Size mode, image Size
			// is 0.53 inch multiplied by the scaling
			// config parameter.  This magic Size is chosen
			// to make the main  (15.5 images
			// wide, including border) just fit into the
			// default window (800 pixels wide) at 96 DPI
			// and default scaling (1.00).
			useImageSize = 0.5555 * screenDPI * scaling;
		} 
        else 
        {
			// Variables for the maximum image size that can fit in the screen.
			double fitxImageSize;
			double fityImageSize;

			// Pad the ScreenSize with 5% of the ScreenSize on all sides to ensure
			// that even the largest forms don't touch the screen borders.
            Vector2<float> paddedScreenSize{
                data.mScreenSize[0] * 0.9f, data.mScreenSize[1] * 0.9f };

			if (data.mRealCoordinates) 
            {
				fitxImageSize = paddedScreenSize[0] / data.mInvSize[0];
				fityImageSize = paddedScreenSize[1] / data.mInvSize[1];
			} 
            else
            {
				// The maximum image size in the old coordinate system also needs to
				// factor in mPadding and mSpacing along with 0.1 slot spare
				// and help text space, hence the magic numbers.
				fitxImageSize = paddedScreenSize[0] / ((5.0 / 4.0) * (0.5 + data.mInvSize[0]));
				fityImageSize = paddedScreenSize[1] / ((15.0 / 13.0) * (0.85 + data.mInvSize[1]));
			}

			// Desktop computers have more space, so try to fit 15 coordinates.
			double preferImageSize = paddedScreenSize[1] / 15 * scaling;

			// Try to use the preferred image size, but if that's bigger than the maximum
			// Size, use the maximum Size.
			useImageSize = std::min(preferImageSize, std::min(fitxImageSize, fityImageSize));
		}

		// Everything else is scaled in proportion to the image size. The slot spacing
		// is 5/4 image Size horizontally and 15/13 image size vertically.The padding around the form 
        // (incorporating the border of the outer slots) is 3/8 image size. Font height 
        // (baseline to baseline) is 2/5 vertical slot spacing, and button
		// half-height is 7/8 of font height.
        mImageSize = Vector2<int>{(int)useImageSize, (int)useImageSize};
        mSpacing = Vector2<float>{(float)(useImageSize*5.0 / 4), (float)(useImageSize*15.0 / 13)};
        mPadding = Vector2<int>{(int)(useImageSize*3.0 / 8), (int)(useImageSize*3.0 / 8)};
		mButtonHeight = (int)(useImageSize*15.0/13 * 0.35);
        
		mFont = mUI->GetSkin()->GetFont();
		if (data.mRealCoordinates) 
        {
            data.mSize = Vector2<int>{
                (int)(data.mInvSize[0]*mImageSize[0]),
                (int)(data.mInvSize[1]*mImageSize[1])};
		} 
        else 
        {
            data.mSize = Vector2<int>{
                (int)(mPadding[0] * 2 + mSpacing[0]*(data.mInvSize[0] - 1.0) + mImageSize[0]),
                (int)(mPadding[1] * 2 + mSpacing[1]*(data.mInvSize[1] - 1.0) + mImageSize[1] + mButtonHeight * 2.0 / 3.0)};
		}

        data.mRect = RectangleShape<2, int>();
        data.mRect.mExtent[0] = (int)((1.0 - data.mAnchor[0]) * (float)data.mSize[0]) + 
            (int)(data.mAnchor[0] * (float)data.mSize[0]);
        data.mRect.mCenter[0] = (int)((float)data.mScreenSize[0] * data.mOffset[0] - 
            (int)(data.mAnchor[0] * (float)data.mSize[0]) + mOffset[0]) + data.mRect.mExtent[0] / 2;
        data.mRect.mExtent[1] = (int)((1.0 - data.mAnchor[1]) * (float)data.mSize[1]) + 
            (int)(data.mAnchor[1] * (float)data.mSize[1]);
        data.mRect.mCenter[1] = (int)((float)data.mScreenSize[1] * data.mOffset[1] - 
            (int)(data.mAnchor[1] * (float)data.mSize[1]) + mOffset[1]) + data.mRect.mExtent[1] / 2;
        mDesiredRect = data.mRect;
	} 
    else 
    {
		// Non-Size[] form must consist only of text fields and
		// implicit "Proceed" button.  Use default font, and
		// temporary form Size which will be recalculated below.
		mFont = mUI->GetSkin()->GetFont();
		mButtonHeight = (int)(FontLineHeight(mFont) * 0.875);
        mDesiredRect = RectangleShape<2, int>();
        mDesiredRect.mExtent[0] = (int)((1.0 - data.mAnchor[0]) * 580.0) + (int)(data.mAnchor[0] * 580.0);
        mDesiredRect.mCenter[0] = (int)((float)data.mScreenSize[0] * data.mOffset[0]) - 
            (int)(data.mAnchor[0] * 580.0) + mDesiredRect.mExtent[0] / 2;
        mDesiredRect.mExtent[1] = (int)((1.0 - data.mAnchor[1]) * 300.0) + (int)(data.mAnchor[1] * 300.0);
        mDesiredRect.mCenter[1] = (int)((float)data.mScreenSize[1] * data.mOffset[1]) -
            (int)(data.mAnchor[1] * 300.0) + mDesiredRect.mExtent[1] / 2;
	}
	RecalculateAbsolutePosition(false);
	data.mBasePos = GetBasePosition();
	mTooltipElement->SetOverrideFont(mFont);

	std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	std::shared_ptr<BaseUIFont> oldFont = skin->GetFont();
	skin->SetFont(mFont);

	mPosOffset.MakeZero();

	// used for form versions < 3
    std::list<std::shared_ptr<BaseUIElement>>::iterator legacySortStart = mChildren.end();

	if (enablePrepends) 
    {
		// Backup the coordinates so that prepends can use the coordinates of choice.
		bool rcBackup = data.mRealCoordinates;
		uint16_t versionBackup = mFormVersion;
		data.mRealCoordinates = false; // Old coordinates by default.

		std::vector<std::string> prependElements = Split(mFormPrepend, ']');
		for (const auto &element : prependElements)
			ParseElement(&data, element);

		// legacy sorting for form versions < 3
		if (mFormVersion >= 3)
			// prepends do not need to be reordered
			legacySortStart = mChildren.end();
		else if (versionBackup >= 3)
			// only prepends elements have to be reordered
			LegacySortElements(legacySortStart);

		mFormVersion = versionBackup;
		data.mRealCoordinates = rcBackup; // Restore coordinates
	}

	for (; i< elements.size(); i++)
		ParseElement(&data, elements[i]);

	if (data.mCurrentParent != shared_from_this())
		LogError("Invalid form string: scroll_container was never closed!");
    else if (!mContainerStack.empty())
		LogError("Invalid form string: container was never closed!");

	// get the scrollbar elements for scroll_containers
	for (const std::pair<std::string, std::shared_ptr<UIScrollContainer>> &scrollContainer : mScrollContainers) 
    {
		for (const std::pair<Field, std::shared_ptr<UIScrollBar>> &scrollBar : mScrollbars) 
        {
			if (scrollContainer.first == scrollBar.first.mName)
            {
                scrollContainer.second->SetScrollBar(scrollBar.second);
				break;
			}
		}
	}

	// If there are fields without explicit Size[], add a "Proceed"
	// button and adjust Size to fit all the fields.
	if (data.mSimpleFieldCount > 0 && !data.mExplicitSize) 
    {
        data.mRect = RectangleShape<2, int>();
        data.mRect.mExtent[0] = 580;
        data.mRect.mCenter[0] = data.mScreenSize[0] / 2;
        data.mRect.mExtent[1] = 240 / 2 + data.mSimpleFieldCount * 60 + 300 / 2;
        data.mRect.mCenter[1] = data.mScreenSize[1] / 2 - 300 / 2 + data.mRect.mExtent[1] / 2;
		mDesiredRect = data.mRect;
		RecalculateAbsolutePosition(false);
		data.mBasePos = GetBasePosition();

		{
			Vector2<int> pos = data.mBasePos;
			pos[1] = (data.mSimpleFieldCount + 2) * 60;

            Vector2<int> size = mDesiredRect.mExtent;
            data.mRect = RectangleShape<2, int>();
            data.mRect.mExtent[0] = 140;
            data.mRect.mCenter[0] = size[0] / 2;
            data.mRect.mExtent[1] = mButtonHeight * 2;
            data.mRect.mCenter[1] = pos[1] + mButtonHeight;

			const wchar_t *text = L"Proceed";
			mUI->AddButton(data.mRect, shared_from_this(), 257, text);
		}
	}

	// Set initial focus if parser didn't set it
	std::shared_ptr<BaseUIElement> focusedElement = mUI->GetFocus();
    if (!focusedElement || !IsChild(focusedElement.get()) ||
        focusedElement->GetType() == UIET_TAB_CONTROL)
    {
        SetInitialFocus();
    }

	skin->SetFont(oldFont);

	// legacy sorting
	if (mFormVersion < 3)
		LegacySortElements(legacySortStart);

	// Formname and regeneration setting
	if (!mIsFormRegenerated) 
    {
		// Only set previous form name if we purposefully showed a new form
		mLastFormName = mTextDst->mFormName;
		mIsFormRegenerated = true;
	}
}

void UIForm::LegacySortElements(std::list<std::shared_ptr<BaseUIElement>>::iterator from)
{
	/*
		Draw order for form_version <= 2:
		-3  bgcolor
		-2  background
		-1  box
		0   All other elements
		1   image
		2   item_image, item_image_button
		3   list
		4   label
	*/
	if (from == mChildren.end())
		from = mChildren.begin();
	else
		from++;

	std::list<std::shared_ptr<BaseUIElement>>::iterator to = mChildren.end();
	// 1: Copy into a sortable container
	std::vector<std::shared_ptr<BaseUIElement>> elements;
	for (auto it = from; it != to; ++it)
		elements.emplace_back(*it);

	// 2: Sort the container
	std::stable_sort(elements.begin(), elements.end(),
			[this] (const std::shared_ptr<BaseUIElement> a, const std::shared_ptr<BaseUIElement> b) -> bool {
		const Field *fieldA = GetField(a->GetID());
		const Field *fieldB = GetField(b->GetID());
		return fieldA && fieldB && fieldA->mPriority < fieldB->mPriority;
	});

	// 3: Re-assign the pointers
	for (auto element : elements) 
    {
		*from = element;
		from++;
	}
}

void UIForm::DrawForm()
{
	if (mFormSource) 
	{
		const std::string& newform = mFormSource->GetForm();
		if (newform != mFormString) 
		{
			mFormString = newform;
			mIsFormRegenerated = false;
			RegenerateUI(mScreenSizeOld);
		}
	}

	std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	std::shared_ptr<BaseUIFont> oldFont = skin->GetFont();
	skin->SetFont(mFont);

	/*
		Draw background color
	*/
    Renderer* renderer = Renderer::Get();
    Vector2<unsigned int> screenSize(renderer->GetScreenSize());
    RectangleShape<2, int> allBackground;
    allBackground.mExtent[0] = screenSize[0];
    allBackground.mExtent[1] = screenSize[1];
    allBackground.mCenter[0] = screenSize[0] / 2;
    allBackground.mCenter[1] = screenSize[1] / 2;

    renderer->SetBlendState(mBlendState);

	if (mBGFullscreen)
		skin->Draw2DRectangle(mFullscreenBGColor, mVisual, allBackground, &allBackground);
	if (mBGNonFullscreen)
        skin->Draw2DRectangle(mBGColor, mVisual, mAbsoluteRect, &mAbsoluteClippingRect);

	/*
		Draw rect_mode tooltip
	*/
	mTooltipElement->SetVisible(false);
	for (const auto &pair : mTooltipRects) 
    {
		const RectangleShape<2, int> &rect = pair.first->GetAbsoluteClippingRect();
		if (rect.GetArea() > 0 && rect.IsPointInside(mPointer)) 
        {
			const std::wstring& text = pair.second.mTooltip;
			if (!text.empty()) 
            {
				ShowTooltip(text, pair.second.mColor, pair.second.mBGColor);
				break;
			}
		}
	}

	/*
		Draw backgrounds
	*/
	for (std::shared_ptr<BaseUIElement> background : mBackgrounds) 
    {
        background->SetVisible(true);
        background->Draw();
        background->SetVisible(false);
	}

    renderer->SetDefaultBlendState();

	// Some elements are only visible while being drawn
	for (std::shared_ptr<BaseUIElement> element : mClickThroughElements)
        element->SetVisible(true);

	/*
		This is where all the drawing happens.
	*/
	std::list<std::shared_ptr<BaseUIElement>>::iterator it = mChildren.begin();
	for (; it != mChildren.end(); ++it)
		if ((*it)->IsNotClipped() || mAbsoluteClippingRect.IsColliding((*it)->GetAbsolutePosition()))
			(*it)->Draw();

	for (std::shared_ptr<BaseUIElement> element : mClickThroughElements)
		element->SetVisible(false);


    mPointer[0] = System::Get()->GetCursorControl()->GetPosition()[0];
    mPointer[1] = System::Get()->GetCursorControl()->GetPosition()[1];
    CursorIcon currentCursorIcon = System::Get()->GetCursorControl()->GetActiveIcon();

	/*
		Draw fields/buttons tooltips and update the mouse cursor
	*/
	std::shared_ptr<BaseUIElement> hovered =
			mUI->GetRootUIElement()->GetElementFromPoint(mPointer);
	bool hoveredElementFound = false;

	if (hovered != NULL) 
    {
		if (mShowDebug) 
        {
			RectangleShape<2, int> rect = hovered->GetAbsoluteClippingRect();
			skin->Draw2DRectangle(SColor(0x22FFFF00), mVisual, rect, &rect);
		}

		int id = hovered->GetID();
		uint64_t delta = 0;
		if (id == -1) 
        {
			mOldTooltipId = id;
		} 
        else 
        {
			if (id == mOldTooltipId) 
            {
				delta = std::abs((int)(mHoveredTime - Timer::GetTime()));
			} 
            else 
            {
				mHoveredTime = Timer::GetTime();
				mOldTooltipId = id;
			}
		}

		// Find and update the current tooltip and cursor icon
		if (id != -1) 
        {
			for (const Field &field : mFields) 
            {
				if (field.mId != id)
					continue;

				if (delta >= mTooltipShowDelay) 
                {
					const std::wstring& text = mTooltips[field.mName].mTooltip;
					if (!text.empty())
						ShowTooltip(text, mTooltips[field.mName].mColor, mTooltips[field.mName].mBGColor);
				}

                if (field.mType != FFT_HYPERTEXT && currentCursorIcon != field.mCursorIcon)
                    System::Get()->GetCursorControl()->SetActiveIcon(field.mCursorIcon);

				hoveredElementFound = true;

				break;
			}
		}
	}

    if (!hoveredElementFound)
        if (currentCursorIcon != CI_NORMAL)
            System::Get()->GetCursorControl()->SetActiveIcon(CI_NORMAL);

	mTooltipElement->Draw();

	skin->SetFont(oldFont);
}


void UIForm::ShowTooltip(const std::wstring& text, const SColor &color, const SColor &bgcolor)
{
    mTooltipElement->SetBackgroundColor(bgcolor);
    mTooltipElement->SetOverrideColor(color);
	mTooltipElement->SetText(text.c_str());

	// Tooltip Size and mOffset
	int tooltipWidth = mTooltipElement->GetTextWidth() + mButtonHeight;
	int tooltipHeight = mTooltipElement->GetTextHeight() + 5;

    Renderer* renderer = Renderer::Get();
    Vector2<unsigned int> screenSize(renderer->GetScreenSize());
	int tooltipOffsetX = mButtonHeight;
	int tooltipOffsetY = mButtonHeight;

	// Calculate and set the tooltip position
	int tooltipX = mPointer[0] + tooltipOffsetX;
	int tooltipY = mPointer[1] + tooltipOffsetY;
	if (tooltipX + tooltipWidth > (int)screenSize[0])
		tooltipX = (int)screenSize[0] - tooltipWidth  - mButtonHeight;
	if (tooltipY + tooltipHeight > (int)screenSize[1])
		tooltipY = (int)screenSize[1] - tooltipHeight - mButtonHeight;

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ tooltipWidth, tooltipHeight };
    rect.mCenter = Vector2<int>{ tooltipX, tooltipY } + rect.mExtent / 2;
	mTooltipElement->SetRelativePosition(rect);

	// Display the tooltip
	mTooltipElement->SetVisible(true);
	BringToFront(mTooltipElement);
}

void UIForm::AcceptInput(FormQuitMode quitmode)
{
	if(mTextDst)
	{
		StringMap fields;

		if (quitmode == FQM_ACCEPT) 
        {
			fields["quit"] = "true";
		}

		if (quitmode == FQM_CANCEL) 
        {
			fields["quit"] = "true";
			mTextDst->GotText(fields);
			return;
		}

		if (mCurrentKeysPending.keyDown) 
        {
			fields["key_down"] = "true";
			mCurrentKeysPending.keyDown = false;
		}

		if (mCurrentKeysPending.keyUp) 
        {
			fields["key_up"] = "true";
			mCurrentKeysPending.keyUp = false;
		}

		if (mCurrentKeysPending.keyEnter) 
        {
			fields["key_enter"] = "true";
			mCurrentKeysPending.keyEnter = false;
		}

		if (!mCurrentFieldEnterPending.empty()) 
        {
			fields["key_enter_field"] = mCurrentFieldEnterPending;
			mCurrentFieldEnterPending = "";
		}

		if (mCurrentKeysPending.keyEscape) 
        {
			fields["key_escape"] = "true";
			mCurrentKeysPending.keyEscape = false;
		}

		for (const UIForm::Field& field : mFields) 
        {
			if (field.mSend) 
            {
				std::string name = field.mName;
				if (field.mType == FFT_BUTTON) 
                {
					fields[name] = ToString(field.mLabel);
				} 
                else if (field.mType == FFT_TABLE) 
                {
					std::shared_ptr<UITable> table = GetTable(field.mName);
					if (table) fields[name] = table->CheckEvent();
				} 
                else if (field.mType == FFT_DROPDOWN) 
                {
					// No dynamic cast possible due to some distributions shipped
					// without rtti support in the engine
					std::shared_ptr<BaseUIElement> element = GetElementFromId(field.mId, true);
					std::shared_ptr<BaseUIComboBox> comboBox = nullptr;
					if (element && element->GetType() == UIET_COMBO_BOX) 
                    {
                        comboBox = std::static_pointer_cast<BaseUIComboBox>(element);
					} 
                    else 
                    {
						LogWarning("UIForm::acceptInput: dropdown field without dropdown element");
						continue;
					}
					int selected = comboBox->GetSelected();
					if (selected >= 0) 
                    {
						if (mDropdownIndexEvent.find(field.mName) != mDropdownIndexEvent.end()) 
                        {
							fields[name] = std::to_string(selected + 1);
						} 
                        else 
                        {
							std::vector<std::string> *dropdownValues = GetDropDownValues(field.mName);
							if (dropdownValues && selected < (int)dropdownValues->size())
								fields[name] = std::to_string(selected + 1);
						}
					}
				} 
                else if (field.mType == FFT_TABHEADER) 
                {
					// No dynamic cast possible due to some distributions shipped
					// without rtti support in the engine
                    std::shared_ptr<BaseUIElement> element = GetElementFromId(field.mId, true);
                    std::shared_ptr<BaseUITabControl> tabControl = nullptr;
                    if (element && element->GetType() == UIET_TAB_CONTROL)
                        tabControl = std::static_pointer_cast<BaseUITabControl>(element);

					if (tabControl)
                    {
						std::stringstream ss;
						ss << (tabControl->GetActiveTab() +1);
						fields[name] = ss.str();
					}
				} 
                else if (field.mType == FFT_CHECKBOX) 
                {
					// No dynamic cast possible due to some distributions shipped
					// without rtti support in the engine
                    std::shared_ptr<BaseUIElement> element = GetElementFromId(field.mId, true);
                    std::shared_ptr<BaseUICheckBox> checkBox = nullptr;
                    if (element && element->GetType() == UIET_CHECK_BOX)
                        checkBox = std::static_pointer_cast<BaseUICheckBox>(element);

					if (checkBox)
                    {
						if (checkBox->IsChecked())
							fields[name] = "true";
						else
							fields[name] = "false";
					}
				} 
                else if (field.mType == FFT_SCROLLBAR)
                {
					// No dynamic cast possible due to some distributions shipped
					// without rtti support in the engine
                    std::shared_ptr<BaseUIElement> element = GetElementFromId(field.mId, true);
                    std::shared_ptr<BaseUIScrollBar> scrollBar = nullptr;
                    if (element && element->GetType() == UIET_SCROLL_BAR)
                        scrollBar = std::static_pointer_cast<BaseUIScrollBar>(element);

					if (scrollBar)
                    {
						std::stringstream os;
						os << scrollBar->GetPosition();
						if (field.mDefault == L"Changed")
							fields[name] = "CHG:" + os.str();
						else
							fields[name] = "VAL:" + os.str();
 					}
				} 
                else if (field.mType == FFT_ANIMATEDIMAGE) 
                {
					// No dynamic cast possible due to some distributions shipped
					// without rtti support in the engine
                    std::shared_ptr<BaseUIElement> element = GetElementFromId(field.mId, true);
                    std::shared_ptr<BaseUIAnimatedImage> animatedImage = nullptr;
                    if (element && element->GetType() == UIET_ANIMATED_IMAGE)
                        animatedImage = std::static_pointer_cast<BaseUIAnimatedImage>(element);

					if (animatedImage)
						fields[name] = std::to_string(animatedImage->GetFrameIndex() + 1);
				} 
                else
                {
                    std::shared_ptr<BaseUIElement> element = GetElementFromId(field.mId, true);
					if (element)
						fields[name] = ToString(element->GetText());
				}
			}
		}

		mTextDst->GotText(fields);
	}
}

bool UIForm::OnPreEvent(const Event& evt)
{
	// The BaseUITabControl renders visually using the skin's selected
	// font, which we override for the duration of form drawing,
	// but computes tab hotspots based on how it would have rendered
	// using the font that is selected at the time of button release.
	// To make these two consistent, temporarily override the skin's
	// font while the IGUITabControl is processing the event.
	if (evt.mEventType == ET_MOUSE_INPUT_EVENT &&
		evt.mMouseInput.mEvent == MIE_LMOUSE_LEFT_UP)
    {
		int x = evt.mMouseInput.X;
		int y = evt.mMouseInput.Y;
		std::shared_ptr<BaseUIElement> hovered =
            mUI->GetRootUIElement()->GetElementFromPoint(Vector2<int>{x, y});
		if (hovered && IsChild(hovered.get()) && hovered->GetType() == UIET_TAB_CONTROL) 
        {
			std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
			std::shared_ptr<BaseUIFont> oldFont = skin->GetFont();
			skin->SetFont(mFont);
			bool retval = hovered->OnEvent(evt);
			skin->SetFont(oldFont);
			return retval;
		}
	}

	// Fix Esc/Return key being eaten by checkboxen and tables
	if (evt.mEventType == ET_KEY_INPUT_EVENT)
    {
        KeyAction key(evt.mKeyInput);
		if (key == EscapeKey || key == CancelKey || evt.mKeyInput.mKey == KEY_RETURN)
        {
			std::shared_ptr<BaseUIElement> focused = mUI->GetFocus();
			if (focused && IsChild(focused.get()) &&
                (focused->GetType() == UIET_LIST_BOX || 
                focused->GetType() == UIET_CHECK_BOX) &&
                (focused->GetParent()->GetType() != UIET_COMBO_BOX ||
				evt.mKeyInput.mKey != KEY_RETURN))
            {
				OnEvent(evt);
				return true;
			}
		}
	}
	// Mouse wheel and move events: send to hovered element instead of focused
	if (evt.mEventType == ET_MOUSE_INPUT_EVENT &&
        (evt.mMouseInput.mEvent == MIE_MOUSE_WHEEL ||
        (evt.mMouseInput.mEvent == MIE_MOUSE_MOVED &&
		evt.mMouseInput.mButtonStates == 0)))
    {
        Vector2<int> point = Vector2<int>{ evt.mMouseInput.X, evt.mMouseInput.Y };
		std::shared_ptr<BaseUIElement> hovered = mUI->GetRootUIElement()->GetElementFromPoint(point);
		if (hovered && IsChild(hovered.get())) 
        {
			hovered->OnEvent(evt);
			return evt.mMouseInput.mEvent == MIE_MOUSE_WHEEL;
		}
	}

	return BaseUIForm::OnPreEvent(evt);
}

void UIForm::TryClose()
{
	if (mAllowClose) 
    {
		//DoPause = false;
		AcceptInput(FQM_CANCEL);
		QuitForm();
	} 
    else 
    {
		mTextDst->GotText(L"Quit");
	}
}

bool UIForm::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_KEY_INPUT_EVENT)
    {
		KeyAction key(evt.mKeyInput);
		if (evt.mKeyInput.mPressedDown && (key == EscapeKey || key == CancelKey ))
        {
			TryClose();
			return true;
		}
        /*
		if (event.mKeyInput.mPressedDown && kp == GetKeySetting("keymap_screenshot"))
			m_client->makeScreenshot();
        */

		if (evt.mKeyInput.mPressedDown && key == GetKeySetting("keymap_toggle_debug"))
			mShowDebug = !mShowDebug;

		if (evt.mKeyInput.mPressedDown &&
			(evt.mKeyInput.mKey == KEY_RETURN ||
			evt.mKeyInput.mKey == KEY_UP ||
			evt.mKeyInput.mKey == KEY_DOWN))
        {
			switch (evt.mKeyInput.mKey)
            {
				case KEY_RETURN:
					mCurrentKeysPending.keyEnter = true;
					break;
				case KEY_UP:
					mCurrentKeysPending.keyUp = true;
					break;
				case KEY_DOWN:
					mCurrentKeysPending.keyDown = true;
					break;
				default:
					//can't happen at all!
					LogError("Reached a source line that can't ever been reached");
					break;
			}
			if (mCurrentKeysPending.keyEnter && mAllowClose) 
            {
				AcceptInput(FQM_ACCEPT);
				QuitForm();
			} 
            else 
            {
				AcceptInput();
			}
			return true;
		}

	}

	if (evt.mEventType == ET_UI_EVENT)
    {
		if (evt.mUIEvent.mEventType == UIEVT_TAB_CHANGED && IsVisible())
        {
			// find the element that was clicked
			for (UIForm::Field &field : mFields) 
            {
				if ((field.mType == FFT_TABHEADER) &&
                    (field.mId == evt.mUIEvent.mCaller->GetID()))
                {
                    field.mSend = true;
					AcceptInput();
                    field.mSend = false;
                    break;
				}
			}
		}
		if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST && IsVisible())
        {
			if (!CanTakeFocus(evt.mUIEvent.mElement))
            {
				LogInformation("UIForm: Not allowing focus change.");
				// Returning true disables focus change
				return true;
			}
		}
		if ((evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED) |
            (evt.mUIEvent.mEventType == UIEVT_CHECKBOX_CHANGED) ||
            (evt.mUIEvent.mEventType == UIEVT_COMBO_BOX_CHANGED) ||
            (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED))
        {
			int callerId = evt.mUIEvent.mCaller->GetID();

			if (callerId == 257)
            {
				if (mAllowClose) 
                {
					AcceptInput(FQM_ACCEPT);
					QuitForm();
				} 
                else 
                {
					AcceptInput();
					mTextDst->GotText(L"ExitButton");
				}
				// quit deallocates 
				return true;
			}

			// find the element that was clicked
			for (UIForm::Field &field : mFields) 
            {
				// if its a button, set the send field so
				// lua knows which button was pressed
				if (callerId != field.mId)
					continue;

				if (field.mType == FFT_BUTTON || field.mType == FFT_CHECKBOX) 
                {
					field.mSend = true;
					if (field.mIsExit) 
                    {
						if (mAllowClose) 
                        {
							AcceptInput(FQM_ACCEPT);
							QuitForm();
						} 
                        else 
                        {
							mTextDst->GotText(L"ExitButton");
						}
						return true;
					}

					AcceptInput(FQM_NO);
					field.mSend = false;
				} 
                else if (field.mType == FFT_DROPDOWN) 
                {
					// only send the changed dropdown
					for (UIForm::Field &field2 : mFields)
						if (field2.mType == FFT_DROPDOWN)
                            field2.mSend = false;

					field.mSend = true;
					AcceptInput(FQM_NO);

					// revert configuration to make sure dropdowns are sent on
					// regular button click
					for (UIForm::Field &field2 : mFields)
						if (field2.mType == FFT_DROPDOWN)
                            field2.mSend = true;

					return true;
				} 
                else if (field.mType == FFT_SCROLLBAR) 
                {
                    field.mDefault = L"Changed";
					AcceptInput(FQM_NO);
					field.mDefault = L"";
				} 
                else if (field.mType == FFT_UNKOWN || field.mType == FFT_HYPERTEXT) 
                {
					field.mSend = true;
					AcceptInput();
					field.mSend = false;
				}
			}
		}

		if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
        {
			// move scroll_containers
			for (const std::pair<std::string, std::shared_ptr<UIScrollContainer>>& scrollContainer : mScrollContainers)
                scrollContainer.second->OnScrollEvent(evt.mUIEvent.mCaller);
		}

		if (evt.mUIEvent.mEventType == UIEVT_EDITBOX_ENTER)
        {
			if (evt.mUIEvent.mCaller->GetID() > 257)
            {
				bool closeOnEnter = true;
				for (UIForm::Field &field : mFields) 
                {
					if (field.mType == FFT_UNKOWN && field.mId == evt.mUIEvent.mCaller->GetID())
                    {
						mCurrentFieldEnterPending = field.mName;
						std::unordered_map<std::string, bool>::const_iterator it = mFieldCloseOnEnter.find(field.mName);
						if (it != mFieldCloseOnEnter.end())
                            closeOnEnter = (*it).second;

						break;
					}
				}

				if (mAllowClose && closeOnEnter)
                {
					mCurrentKeysPending.keyEnter = true;
					AcceptInput(FQM_ACCEPT);
					QuitForm();
				} 
                else 
                {
					mCurrentKeysPending.keyEnter = true;
					AcceptInput();
				}
				// quit deallocates 
				return true;
			}
		}

		if (evt.mUIEvent.mEventType == UIEVT_TABLE_CHANGED)
        {
			int currentId = evt.mUIEvent.mCaller->GetID();
			if (currentId > 257) 
            {
				// find the element that was clicked
				for (UIForm::Field &field : mFields) 
                {
					// if it's a table, set the send field
					// so lua knows which table was changed
					if (field.mType == FFT_TABLE && field.mId == currentId)
                    {
                        field.mSend = true;
						AcceptInput();
                        field.mSend=false;
					}
				}
			}
		}
	}

	return mParent ? mParent->OnEvent(evt) : false;
}

/**
 * get name of element by element id
 * @param id of element
 * @return name string or empty string
 */
std::string UIForm::GetName(int id)
{
	for (Field& field : mFields)
		if (field.mId == id)
			return field.mName;

	return "";
}

int UIForm::GetField(const std::string& name)
{
	for (Field& field : mFields)
		if (field.mName == name)
			return field.mId;

	return -1;
}

const UIForm::Field *UIForm::GetField(int id)
{
	for (Field& field : mFields)
		if (field.mId == id)
			return &field;

	return nullptr;
}

/**
 * get label of element by id
 * @param id of element
 * @return label string or empty string
 */
std::wstring UIForm::GetLabel(int id)
{
	for (Field& field : mFields) 
		if (field.mId == id)
			return field.mLabel;

	return L"";
}

Style UIForm::GetDefaultStyleForElement(const std::string& type,
		const std::string& name, const std::string& parentType) 
{
	return GetStyleForElement(type, name, parentType)[Style::STATE_DEFAULT];
}

std::array<Style, Style::NUM_STATES> UIForm::GetStyleForElement(
	const std::string& type, const std::string& name, const std::string& parentType)
{
	std::array<Style, Style::NUM_STATES> ret;

	auto it = mThemeByType.find("*");
	if (it != mThemeByType.end()) 
		for (const Style& style : it->second)
			ret[(unsigned int)style.GetState()] |= style;

	it = mThemeByName.find("*");
	if (it != mThemeByName.end()) 
		for (const Style& style : it->second)
			ret[(unsigned int)style.GetState()] |= style;

	if (!parentType.empty()) 
    {
		it = mThemeByType.find(parentType);
		if (it != mThemeByType.end()) 
        {
			for (const Style& style : it->second)
				ret[(unsigned int)style.GetState()] |= style;
		}
	}

	it = mThemeByType.find(type);
	if (it != mThemeByType.end())
		for (const Style& style : it->second)
			ret[(unsigned int)style.GetState()] |= style;

	it = mThemeByName.find(name);
	if (it != mThemeByName.end()) 
		for (const Style& style : it->second)
			ret[(unsigned int)style.GetState()] |= style;

	return ret;
}