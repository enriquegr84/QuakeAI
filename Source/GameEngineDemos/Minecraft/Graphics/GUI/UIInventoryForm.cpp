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

#include "UIInventoryForm.h"

#include "UIScene.h"
#include "UIItemImage.h"

#include "../Hud.h"

#include "../../Games/Games.h"

#include "Core/OS/OS.h"

#include "Graphic/Renderer/Renderer.h"

#include "Application/GameApplication.h"

UIInventoryList::UIInventoryList(BaseUI* ui, int id, 
    const RectangleShape<2, int>& rectangle, const std::string& listname, VisualEnvironment* env, 
    BaseItemManager* itemMgr, const InventoryLocation& inventoryLoc, const Vector2<int>& geom, 
    const int startItemIndex, const Vector2<int>& slotSize, const Vector2<float>& slotSpacing, 
    UIInventoryForm* form, const Options& options, BaseUIFont* font)
    : BaseUIElement(UIET_ELEMENT, id, rectangle), mUI(ui), mListName(listname), mEnvironment(env),
    mItemMgr(itemMgr), mInventoryLoc(inventoryLoc), mGeom(geom), mStartItemIndex(startItemIndex), 
    mSlotSize(slotSize), mSlotSpacing(slotSpacing), mUIInventoryForm(form), 
    mOptions(options), mFont(font), mHoveredIndex(-1), mAlreadyWarned(false)
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

    mEffect = std::make_shared<ColorEffect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

void UIInventoryList::Draw()
{
    if (!IsVisible())
        return;

    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Inventory* inv = mEnvironment->GetInventory(mInventoryLoc);
    if (!inv) 
    {
        if (!mAlreadyWarned) 
        {
            LogWarning("UIInventoryList::Draw(): The inventory location "
                "\"" + mInventoryLoc.Dump() + "\" doesn't exist");
            mAlreadyWarned = true;
        }
        return;
    }
    InventoryList* ilist = inv->GetList(mListName);
    if (!ilist) 
    {
        if (!mAlreadyWarned) 
        {
            LogWarning("UIInventoryList::Draw(): The inventory list \"" +
                mListName + "\" @ \"" + mInventoryLoc.Dump() + "\" doesn't exist");
            mAlreadyWarned = true;
        }
        return;
    }
    mAlreadyWarned = false;

    const Item* selectedItem = mUIInventoryForm->GetSelectedItem();

    RectangleShape<2, int> imgRect;
    imgRect.mExtent = mSlotSize;
    imgRect.mCenter = imgRect.mExtent / 2;
    Vector2<int> basePos = mAbsoluteRect.GetVertice(RVP_UPPERLEFT);

    const int listSize = (int)ilist->GetSize();
    for (int i = 0; i < mGeom[0] * mGeom[1]; i++) 
    {
        int itemIndex = i + mStartItemIndex;
        if (itemIndex >= listSize)
            break;

        Vector2<int> p{ 
            (i % mGeom[0]) * (int)mSlotSpacing[0],
            (i / mGeom[0]) * (int)mSlotSpacing[1] };
        RectangleShape<2, int> rect = imgRect;
        rect.mCenter += basePos + p;
        ItemStack item = ilist->GetItem(itemIndex);

        bool selected = selectedItem && 
            mEnvironment->GetInventory(selectedItem->inventoryloc) == inv && 
            selectedItem->listname == mListName && 
            selectedItem->index == itemIndex;
        bool hovering = mHoveredIndex == itemIndex;
        ItemRotationKind rotationKind = selected ? IT_ROT_SELECTED :
            (hovering ? IT_ROT_HOVERED : IT_ROT_NONE);

        // layer 0
        if (hovering) 
            skin->Draw2DRectangle(mOptions.slotbgHighlighted, mVisual, rect, &mAbsoluteClippingRect);
        else 
            skin->Draw2DRectangle(mOptions.slotbgNormal, mVisual, rect, &mAbsoluteClippingRect);

        // Draw inv slot borders
        if (mOptions.slotborder) 
        {
            int x1 = rect.GetVertice(RVP_UPPERLEFT)[0];
            int y1 = rect.GetVertice(RVP_UPPERLEFT)[1];
            int x2 = rect.GetVertice(RVP_LOWERRIGHT)[0];
            int y2 = rect.GetVertice(RVP_LOWERRIGHT)[1];
            int border = 1;

            RectangleShape<2, int> clippingRect = mParent ? 
                mParent->GetAbsoluteClippingRect() : RectangleShape<2, int>();
            RectangleShape<2, int>* clippingRectPtr = mParent ? &clippingRect : nullptr;
            
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ x2 + border, y1 } - Vector2<int>{ x1 - border, y1 - border };
            rect.mCenter = Vector2<int>{ x1 - border, y1 - border } + rect.mExtent / 2;
            skin->Draw2DRectangle(mOptions.slotbordercolor, mVisual, rect, clippingRectPtr);

            rect.mExtent = Vector2<int>{ x2 + border, y2 + border } - Vector2<int>{ x1 - border, y2 };
            rect.mCenter = Vector2<int>{ x1 - border, y2 } + rect.mExtent / 2;
            skin->Draw2DRectangle(mOptions.slotbordercolor, mVisual, rect, clippingRectPtr);

            rect.mExtent = Vector2<int>{ x1, y2 } - Vector2<int>{ x1 - border, y1 };
            rect.mCenter = Vector2<int>{ x1 - border, y1 } + rect.mExtent / 2;
            skin->Draw2DRectangle(mOptions.slotbordercolor, mVisual, rect, clippingRectPtr);

            rect.mExtent = Vector2<int>{ x2, y1 } - Vector2<int>{ x2 - border, y2 };
            rect.mCenter = Vector2<int>{ x2 - border, y2 } + rect.mExtent / 2;
            skin->Draw2DRectangle(mOptions.slotbordercolor, mVisual, rect, clippingRectPtr);
        }

        // layer 1
        if (selected)
            item.TakeItem(mUIInventoryForm->GetSelectedAmount());

        if (!item.IsEmpty()) 
        {
            // Draw item stack
            DrawItemStack(mUI, mEnvironment, item, rect, &mAbsoluteClippingRect, rotationKind);
            // Add hovering tooltip
            if (hovering && !selectedItem) 
            {
                std::string tooltip = item.GetDescription(mItemMgr);
                if (mUIInventoryForm->DoTooltipAppendItemname())
                    tooltip += "\n[" + item.name + "]";
                mUIInventoryForm->AddHoveredItemTooltip(tooltip);
            }
        }
    }

    BaseUIElement::Draw();
}

bool UIInventoryList::OnEvent(const Event& evt)
{
    if (evt.mEventType != ET_MOUSE_INPUT_EVENT)
    {
        if (evt.mEventType == ET_UI_EVENT &&
            evt.mUIEvent.mEventType == UIEVT_ELEMENT_LEFT)
        {
            // element is no longer hovered
            mHoveredIndex = -1;
        }
        return BaseUIElement::OnEvent(evt);
    }

    mHoveredIndex = GetItemIndexAtPosition(
        Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y});

    if (mHoveredIndex != -1)
        return BaseUIElement::OnEvent(evt);

    // no item slot at pos of mouse event => allow clicking through
    // find the element that would be hovered if this inventorylist was invisible
    bool wasVisible = IsVisible();
    SetVisible(false);
    std::shared_ptr<BaseUIElement> hovered = 
        mUI->GetRootUIElement()->GetElementFromPoint(
        Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y});

    // if the player clicks outside of the form window, hovered is not
    // mUIInventoryForm, but some other weird element (with ID -1). we do however need
    // hovered to be mUIInventoryForm as item dropping when clicking outside of the
    // form window is handled in its OnEvent callback
    bool ret;
    if (!hovered || hovered->GetID() == -1)
        ret = mUIInventoryForm->OnEvent(evt);
    else
        ret = hovered->OnEvent(evt);

    SetVisible(wasVisible);
    return ret;
}

int UIInventoryList::GetItemIndexAtPosition(Vector2<int> p) const
{
    // no item if no gui element at pointer
    if (!IsVisible() || mAbsoluteClippingRect.GetArea() <= 0 ||
        !mAbsoluteClippingRect.IsPointInside(p))
        return -1;

    // there can not be an item if the inventory or the inventorylist does not exist
    Inventory* inv = mEnvironment->GetInventory(mInventoryLoc);
    if (!inv)
        return -1;
    InventoryList* ilist = inv->GetList(mListName);
    if (!ilist)
        return -1;

    RectangleShape<2, int> imgRect;
    imgRect.mExtent = mSlotSize;
    imgRect.mCenter = mSlotSize / 2;
    Vector2<int> basePos = mAbsoluteRect.GetVertice(RVP_UPPERLEFT);

    // instead of looping through each slot, we look where p would be in the grid
    int idx = (p[0] - basePos[0]) / (int)mSlotSpacing[0] + 
        mGeom[0] * ((p[1] - basePos[1]) / (int)mSlotSpacing[1]);

    Vector2<int> p0{ 
        (int)((idx % mGeom[0]) * mSlotSpacing[0]),
        (int)((idx / mGeom[0]) * mSlotSpacing[1]) };

    RectangleShape<2, int> rect = imgRect;
    rect.mCenter += basePos + p0;

    rect.ClipAgainst(mAbsoluteClippingRect);
    if (rect.GetArea() > 0 && rect.IsPointInside(p) &&
        idx + mStartItemIndex < (int)ilist->GetSize())
        return idx + mStartItemIndex;

    return -1;
}


/*
	UIForm
*/
static unsigned int FontLineHeight(std::shared_ptr<BaseUIFont> font)
{
	return font->GetDimension(L"Ay")[1];
}

UIInventoryForm::UIInventoryForm(BaseUI* ui, int id, 
    RectangleShape<2, int> rectangle, BaseSimpleTextureSource* textureSrc,
    std::shared_ptr<BaseFormSource> formSrc, std::shared_ptr<TextDestination> txtDst, 
    const std::string& formPrepend, VisualEnvironment* env, BaseItemManager* itemMgr, 
    const InventoryLocation& inventoryLoc, bool remapDoubleClick) :
    UIForm(ui, id, rectangle, formSrc, txtDst, formPrepend, remapDoubleClick), 
    mTextureSrc(textureSrc), mEnvironment(env), mItemMgr(itemMgr), mInventoryLocation(inventoryLoc)
{
    mTooltipAppendItemName = Settings::Get()->GetBool("tooltip_append_itemname");

    mBlendState = std::make_shared<BlendState>();
    mBlendState->mTarget[0].enable = true;
    mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
    mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;
}

UIInventoryForm::~UIInventoryForm()
{
    delete mSelectedItem;
}


void UIInventoryForm::ParseList(ParserData *data, const std::string& element)
{
    std::vector<std::string> parts = Split(element, ';');

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

        InventoryLocation loc;
        if (location == "context" || location == "current_name")
            loc = mInventoryLocation;
        else
            loc.Deserialize(location);

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
            Vector2<float>{ mSpacing[0] - mImageSize[0], mSpacing[1] - mImageSize[1] };

        slotSpacing[0] = slotSpacing[0] < 0 ? defaultSpacing[0] : mImageSize[0] * slotSpacing[0];
        slotSpacing[1] = slotSpacing[1] < 0 ? defaultSpacing[1] : mImageSize[1] * slotSpacing[1];

        slotSpacing += slotSize;

        Vector2<int> pos = data->mRealCoordinates ? 
            GetRealCoordinateBasePosition(vPos) : GetElementBasePosition(&vPos);
        
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{
            (int)((geom[0] - 1) * slotSpacing[0] + slotSize[0]),
            (int)((geom[1] - 1) * slotSpacing[1] + slotSize[1])};
        rect.mCenter = pos + rect.mExtent / 2;

        std::shared_ptr<UIInventoryList> list(new UIInventoryList(
            mUI, field.mId, rect, listName, mEnvironment, mItemMgr, loc, geom, startIdx, 
            Vector2<int>{(int)slotSize[0], (int)slotSize[1]}, slotSpacing, this, 
            ((InventoryParserData*)data)->mInventorylistOptions, mFont.get()));
        list->SetParent(data->mCurrentParent ? data->mCurrentParent : mUI->GetRootUIElement());
        list->SetNotClipped(style.GetBool(Style::NOCLIP, false));
        list->SetSubElement(true);

        mInventorylists.push_back(list);
        mFields.push_back(field);
        return;
    }
    LogError("Invalid list element(" + std::to_string(parts.size()) + "): '" + element + "'");
}


void UIInventoryForm::ParseBackground(ParserData* data, const std::string& element)
{
    std::vector<std::string> parts = Split(element, ';');

    if (parts.size() >= 3)
    {
        std::vector<std::string> vPos = Split(parts[0], ',');
        std::vector<std::string> vGeom = Split(parts[1], ',');
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
                int y = atoi(vMiddle[1].c_str());
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

        std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(name);
        if (!texture)
        {
            LogError("Unable to load texture: \t" + name);
            return;
        }

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
        std::to_string(parts.size()) + "): '" + element + "'");
}


void UIInventoryForm::ParseImage(ParserData* data, const std::string& element)
{
    std::vector<std::string> parts = Split(element, ';');

    if (parts.size() >= 3)
    {
        std::vector<std::string> vPos = Split(parts[0], ',');
        std::vector<std::string> vGeom = Split(parts[1], ',');
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

        std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(name);
        if (!texture)
        {
            LogError("Unable to load texture: \t" + name);
            return;
        }

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
        std::vector<std::string> vPos = Split(parts[0], ',');
        std::string name = UnescapeString(parts[1]);

        if (vPos.size() != 2)
            LogError("Invalid pos for image specified: \"" + parts[0] + "\"");

        Vector2<int> pos = GetElementBasePosition(&vPos);

        if (!data->mExplicitSize)
            LogWarning("invalid use of image without a size[] element");

        std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(name);
        if (!texture)
        {
            LogError("Unable to load texture: \t" + name);
            return;
        }

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


void UIInventoryForm::ParseListRing(ParserData *data, const std::string& element)
{
	std::vector<std::string> parts = Split(element, ';');

    if (parts.size() == 2) 
    {
        std::string location = parts[0];
        std::string listname = parts[1];

        InventoryLocation loc;
        if (location == "context" || location == "current_name")
            loc = mInventoryLocation;
        else
            loc.Deserialize(location);

        mInventoryRings.emplace_back(loc, listname);
        return;
    }

    if (element.empty() && mInventorylists.size() > 1) 
    {
        size_t siz = mInventorylists.size();
        // insert the last two inv list elements into the list ring
        const std::shared_ptr<UIInventoryList> spa = mInventorylists[siz - 2];
        const std::shared_ptr<UIInventoryList> spb = mInventorylists[siz - 1];
        mInventoryRings.emplace_back(spa->GetInventoryLoc(), spa->GetListname());
        mInventoryRings.emplace_back(spb->GetInventoryLoc(), spb->GetListname());
        return;
    }

    LogError("Invalid list ring element(" + std::to_string(parts.size()) +
        ", " + std::to_string(mInventorylists.size()) + "): '" + element + "'");
}

void UIInventoryForm::ParseItemImage(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 3)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string name = parts[2];

        if (vPos.size() != 2)
            LogError("Invalid pos for itemimage specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for itemimage specified: \"" + parts[1] + "\"");

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

		if(!data->mExplicitSize)
			LogWarning("invalid use of item_image without a size[] element");

		Field field("", L"", L"", 258 + (int)mFields.size(), 2);
		field.mType = FFT_ITEMIMAGE;


        RectangleShape<2, int> rect;
        rect.mExtent = geom;
        rect.mCenter = pos + rect.mExtent / 2;

		std::shared_ptr<UIItemImage> el(
            new UIItemImage(mUI, field.mId, rect, name, mEnvironment, mItemMgr, mFont));
        el->SetParent(data->mCurrentParent ? data->mCurrentParent : mUI->GetRootUIElement());
        el->SetSubElement(true);

		auto style = GetDefaultStyleForElement("item_image", field.mName);
		el->SetNotClipped(style.GetBool(Style::NOCLIP, false));

		// item images should let events through
		mClickThroughElements.push_back(el);

		mFields.push_back(field);
		return;
	}
	LogError("Invalid ItemImage element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIInventoryForm::ParseItemImageButton(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 5)
	{
		std::vector<std::string> vPos = Split(parts[0],',');
		std::vector<std::string> vGeom = Split(parts[1],',');
		std::string itemName = parts[2];
		std::string name = parts[3];
		std::string label = parts[4];

		label = UnescapeString(label);
		itemName = UnescapeString(itemName);

        if (vPos.size() != 2)
            LogError("Invalid pos for itemimagebutton specified: \"" + parts[0] + "\"");
        if (vGeom.size() != 2)
            LogError("Invalid geom for itemimagebutton specified: \"" + parts[1] + "\"");

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
		if(!data->mExplicitSize)
			LogWarning("invalid use of item_image_button without a size[] element");
        
		ItemStack item;
		item.Deserialize(itemName, mItemMgr);

		mTooltips[name] = Tooltip(
            ToWideString(item.GetDefinition(mItemMgr).description),
            mDefaultTooltipBGColor, mDefaultTooltipColor);

		// the field for the button
		Field field(name, ToWideString(label), 
            ToWideString(itemName), 258 + (int)mFields.size(), 2);

        std::shared_ptr<BaseUIButton> button = mUI->AddButton(
            rect, data->mCurrentParent, field.mId, field.mLabel.c_str());
        button->SetSubElement(true);

        rect.mCenter = rect.mExtent / 2;
        std::shared_ptr<UIItemImage> image(
            new UIItemImage(mUI, field.mId, rect, name, mEnvironment, mItemMgr, mFont));
        image->SetParent(button);
        image->SetSubElement(true);
        button->SendToBack(image);

		auto style = GetStyleForElement("item_image_button", field.mName, "image_button");
		field.mSound = style[Style::STATE_DEFAULT].Get(Style::Property::SOUND, "");
        button->SetStyles(style);

		if (field.mName == mFocusedElement)
			mUI->SetFocus(button);

		field.mType = FFT_BUTTON;
		field.mRect.mExtent = geom;
        field.mRect.mCenter = pos + geom / 2;
        field.mRect.mCenter += data->mBasePos - mPadding;
		mFields.push_back(field);
		return;
	}
	LogError("Invalid ItemImagebutton element(" + std::to_string(parts.size()) + "): '" + element + "'");
}

void UIInventoryForm::ParseListColors(ParserData* data, const std::string& element)
{
	std::vector<std::string> parts = Split(element,';');

	if (parts.size() >= 2)
	{
		ParseColorString(parts[0], ((InventoryParserData*)data)->mInventorylistOptions.slotbgNormal, false);
		ParseColorString(parts[1], ((InventoryParserData*)data)->mInventorylistOptions.slotbgHighlighted, false);

		if (parts.size() >= 3) 
        {
			if (ParseColorString(parts[2], ((InventoryParserData*)data)->mInventorylistOptions.slotbordercolor, false))
                ((InventoryParserData*)data)->mInventorylistOptions.slotborder = true;
		}
		if (parts.size() == 5) 
        {
			SColor tmpColor;

			if (ParseColorString(parts[3], tmpColor, false))
				mDefaultTooltipBGColor = tmpColor;
			if (ParseColorString(parts[4], tmpColor, false))
				mDefaultTooltipColor = tmpColor;
		}

		// update all already parsed inventorylists
		for (std::shared_ptr<UIInventoryList> el : mInventorylists) 
        {
            el->SetSlotBGColors(
                ((InventoryParserData*)data)->mInventorylistOptions.slotbgNormal,
                ((InventoryParserData*)data)->mInventorylistOptions.slotbgHighlighted);
            el->SetSlotBorders(
                ((InventoryParserData*)data)->mInventorylistOptions.slotborder,
                ((InventoryParserData*)data)->mInventorylistOptions.slotbordercolor);
		}
		return;
	}
	LogError("Invalid listcolors element(" + std::to_string(parts.size()) + "): '" + element + "'" );
}

void UIInventoryForm::ParseModel(ParserData *data, const std::string& element)
{
    std::vector<std::string> parts = Split(element, ';');

    if (parts.size() < 5 || parts.size() > 10)
    {
        LogError("Invalid model element (" + std::to_string(parts.size()) + "): '" + element + "'");
        return;
    }

    // Avoid length checks by resizing
    if (parts.size() < 10) parts.resize(10);

    std::vector<std::string> vPos = Split(parts[0], ',');
    std::vector<std::string> vGeom = Split(parts[1], ',');
    std::string name = UnescapeString(parts[2]);
    std::string meshstr = UnescapeString(parts[3]);
    std::vector<std::string> textures = Split(parts[4], ',');
    std::vector<std::string> vecRot = Split(parts[5], ',');
    bool infRotation = IsYes(parts[6]);
    bool mousectrl = IsYes(parts[7]) || parts[7].empty(); // default true
    std::vector<std::string> frameLoop = Split(parts[8], ',');
    std::string speed = UnescapeString(parts[9]);

    if (vPos.size() != 2)
        LogError("Invalid pos for model specified: \"" + parts[0] + "\"");
    if (vGeom.size() != 2)
        LogError("Invalid geom for model specified: \"" + parts[1] + "\"");

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
        LogWarning("invalid use of model without a size[] element");

    std::shared_ptr<AnimatedMesh> animMesh;
    std::shared_ptr<ResHandle>& resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(meshstr)));
    if (resHandle)
    {
        const std::shared_ptr<MeshResourceExtraData>& extra =
            std::static_pointer_cast<MeshResourceExtraData>(resHandle->GetExtra());

        if (extra->GetMesh())
            animMesh = std::dynamic_pointer_cast<AnimatedMesh>(extra->GetMesh());
    }

    if (!animMesh)
    {
        LogError("Invalid model element: Unable to load mesh: \t" + meshstr);
        return;
    }

    Field field(name, L"", L"", 258 + (int)mFields.size());
    RectangleShape<2, int> rect;
    rect.mExtent = geom;
    rect.mCenter = pos + geom / 2;

    std::shared_ptr<UIScene> el(new UIScene(
        GameApplication::Get()->GetHumanView()->mScene.get(), mUI, field.mId, rect));
    el->SetParent(data->mCurrentParent ? data->mCurrentParent : mUI->GetRootUIElement());
    el->SetSubElement(true);

    auto meshNode = el->SetMesh(animMesh);
    for (unsigned int i = 0; i < textures.size() && i < meshNode->GetMaterialCount(); ++i)
        el->SetTexture(i, mTextureSrc->GetTexture(UnescapeString(textures[i])));
    if (vecRot.size() >= 2)
        el->SetRotation(Vector2<float>{(float)atof(vecRot[0].c_str()), (float)atof(vecRot[1].c_str())});

    el->EnableContinuousRotation(infRotation);
    el->EnableMouseControl(mousectrl);

    int frameLoopBegin = 0;
    int frameLoopEnd = 0x7FFFFFFF;
    if (frameLoop.size() == 2) 
    {
        frameLoopBegin = atoi(frameLoop[0].c_str());
        frameLoopEnd = atoi(frameLoop[1].c_str());
    }

    el->SetFrameLoop(frameLoopBegin, frameLoopEnd);
    el->SetAnimationSpeed((float)atof(speed.c_str()));

    auto style = GetStyleForElement("model", field.mName);
    el->SetStyles(style);

    mFields.push_back(field);
}

void UIInventoryForm::ParseElement(ParserData* data, const std::string& element)
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

    if (type == "background" || type == "background9")
    {
        ParseBackground(data, description);
        return;
    }

    if (type == "image")
    {
        ParseImage(data, description);
        return;
    }

    if (type == "item_image")
    {
        ParseItemImage(data, description);
        return;
    }

    if (type == "item_image_button")
    {
        ParseItemImageButton(data, description);
        return;
    }

    if (type == "listring")
    {
        ParseListRing(data, description);
        return;
    }

    if (type == "listcolors")
    {
        ParseListColors(data, description);
        return;
    }

    if (type == "model")
    {
        ParseModel(data, description);
        return;
    }

    UIForm::ParseElement(data, element);
}

void UIInventoryForm::RegenerateUI(Vector2<unsigned int> screenSize)
{
	// Useless to regenerate without a screenSize
	if ((screenSize[0] <= 0) || (screenSize[1] <= 0)) {
		return;
	}

	InventoryParserData data;

	// Preserve stuff only on same form, not on a new form.
	if (mTextDst->mFormName == mLastFormName) 
    {
		// Preserve tables/textlists
		for (auto &table : mTables) 
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
				for (const UIForm::Field& field : mFields) 
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
    mInventorylists.clear();
    mInventoryRings.clear();
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
		// Note: parent != this so that the tooltip isn't clipped by the rectangle
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{110, 18};
        rect.mCenter = rect.mExtent / 2;
		mTooltipElement = mUI->AddStaticText(L"", rect);
		mTooltipElement->EnableOverrideColor(true);
		mTooltipElement->SetBackgroundColor(mDefaultTooltipBGColor);
		mTooltipElement->SetDrawBackground(true);
		mTooltipElement->SetDrawBorder(true);
		mTooltipElement->SetOverrideColor(mDefaultTooltipColor);
		mTooltipElement->SetTextAlignment(UIA_CENTER, UIA_CENTER);
		mTooltipElement->SetWordWrap(false);
	}
    std::vector<std::string> elements = Split(mFormString, ']');

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

	/* Copy of the "mRealCoordinates" element for after the form size. */
	data.mRealCoordinates = mFormVersion >= 2;
	for (; i < elements.size(); i++) 
    {
		std::vector<std::string> parts = Split(elements[i], '[');
		std::string name = Trim(parts[0]);
		if (name != "mRealCoordinates" || parts.size() != 2)
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
			// is 0.53 inch multiplied by the uiScaling
			// config parameter.  This magic Size is chosen
			// to make the main menu (15.5 images
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

UIInventoryList::Item UIInventoryForm::GetItemAtPosition(Vector2<int> p) const
{
	for (const std::shared_ptr<UIInventoryList> el : mInventorylists) 
    {
		int itemIndexndex = el->GetItemIndexAtPosition(p);
		if (itemIndexndex != -1)
			return UIInventoryList::Item(el->GetInventoryLoc(), el->GetListname(), itemIndexndex);
	}

	return UIInventoryList::Item(InventoryLocation(), "", -1);
}

void UIInventoryForm::DrawSelectedItem()
{
	if (!mSelectedItem) 
    {
		// reset rotation time
        RectangleShape<2, int> rect;

		DrawItemStack(mUI, mEnvironment, ItemStack(), rect, NULL, IT_ROT_DRAGGED);
		return;
	}

    int viewX, viewY, viewW, viewH;
    Renderer::Get()->GetViewport(viewX, viewY, viewW, viewH);
    RectangleShape<2, int> viewRect;
    viewRect.mExtent = Vector2<int>{viewW, viewH};
    viewRect.mCenter = Vector2<int>{viewX + viewW / 2, viewY + viewH / 2};

    RectangleShape<2, int> rect;
    rect.mExtent = mImageSize;
    rect.mCenter = mImageSize / 2;
    rect.mCenter += (mPointer - rect.mCenter);

	Inventory* inv = mEnvironment->GetInventory(mSelectedItem->inventoryloc);
	LogAssert(inv, "invalid inventory");
	InventoryList* list = inv->GetList(mSelectedItem->listname);
	LogAssert(list, "invalid inventory list");
	ItemStack stack = list->GetItem(mSelectedItem->index);
	stack.count = mSelectedAmount;

	rect.ConstrainTo(viewRect);

    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
    RectangleShape<2, int> clipRect;
    clipRect.mExtent = {(int)screenSize[0], (int)screenSize[1]};
    clipRect.mCenter = clipRect.mExtent / 2;

	DrawItemStack(mUI, mEnvironment, stack, rect, &clipRect, IT_ROT_DRAGGED);
}

void UIInventoryForm::DrawForm()
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

	mHoveredItemTooltips.clear();

    UpdateSelectedItem();

    //Draw background color
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

    //Draw rect_mode tooltip
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

    //Draw backgrounds
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

    //This is where all the drawing happens.
	std::list<std::shared_ptr<BaseUIElement>>::iterator it = mChildren.begin();
	for (; it != mChildren.end(); ++it)
		if ((*it)->IsNotClipped() || mAbsoluteClippingRect.IsColliding((*it)->GetAbsolutePosition()))
			(*it)->Draw();

	for (std::shared_ptr<BaseUIElement> element : mClickThroughElements)
		element->SetVisible(false);

	// Draw hovered item tooltips
	for (const std::string& tooltip : mHoveredItemTooltips) 
		ShowTooltip(ToWideString(tooltip), mDefaultTooltipColor, mDefaultTooltipBGColor);

	if (mHoveredItemTooltips.empty()) 
    {
		// reset rotation time
        RectangleShape<2, int> rect;
		DrawItemStack(mUI, mEnvironment, ItemStack(), rect, NULL, IT_ROT_HOVERED);
	}

    mPointer[0] = System::Get()->GetCursorControl()->GetPosition()[0];
    mPointer[1] = System::Get()->GetCursorControl()->GetPosition()[1];
    CursorIcon currentCursorIcon = System::Get()->GetCursorControl()->GetActiveIcon();

    //Draw fields/buttons tooltips and update the mouse cursor
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

    //Draw dragged item stack
	DrawSelectedItem();

	skin->SetFont(oldFont);
}

void UIInventoryForm::UpdateSelectedItem()
{
    VerifySelectedItem();

    // If craftresult is nonempty and nothing else is selected, select it now.
    if (!mSelectedItem) 
    {
        for (const std::shared_ptr<UIInventoryList> el : mInventorylists) 
        {
            if (el->GetListname() != "craftpreview")
                continue;

            Inventory* inv = mEnvironment->GetInventory(el->GetInventoryLoc());
            if (!inv)
                continue;

            InventoryList* list = inv->GetList("craftresult");

            if (!list || list->GetSize() == 0)
                continue;

            const ItemStack &item = list->GetItem(0);
            if (item.IsEmpty())
                continue;

            // Grab selected item from the crafting result list
            mSelectedItem = new UIInventoryList::Item;
            mSelectedItem->inventoryloc = el->GetInventoryLoc();
            mSelectedItem->listname = "craftresult";
            mSelectedItem->index = 0;
            mSelectedAmount = item.count;
            mSelectedDragging = false;
            break;
        }
    }

    // If craftresult is selected, keep the whole stack selected
    if (mSelectedItem && mSelectedItem->listname == "craftresult")
        mSelectedAmount = VerifySelectedItem().count;
}

ItemStack UIInventoryForm::VerifySelectedItem()
{
    // If the selected stack has become empty for some reason, deselect it.
    // If the selected stack has become inaccessible, deselect it.
    // If the selected stack has become smaller, adjust mSelectedAmount.
    // Return the selected stack.
    if (mSelectedItem) 
    {
        if (mSelectedItem->IsValid()) 
        {
            Inventory* inv = mEnvironment->GetInventory(mSelectedItem->inventoryloc);
            if (inv) 
            {
                InventoryList *list = inv->GetList(mSelectedItem->listname);
                if (list && (unsigned int)mSelectedItem->index < list->GetSize()) 
                {
                    ItemStack stack = list->GetItem(mSelectedItem->index);
                    if (!mSelectedSwap.IsEmpty()) 
                    {
                        if (mSelectedSwap.name == stack.name && mSelectedSwap.count == stack.count)
                            mSelectedSwap.Clear();
                    } 
                    else mSelectedAmount = std::min(mSelectedAmount, stack.count);

                    if (!stack.IsEmpty())
                        return stack;
                }
            }
        }

        // selection was not valid
        delete mSelectedItem;
        mSelectedItem = nullptr;
        mSelectedAmount = 0;
        mSelectedDragging = false;
    }
    return ItemStack();
}

bool UIInventoryForm::OnPreEvent(const Event& evt)
{
    return UIForm::OnPreEvent(evt);
}

enum ButtonEventType : uint8_t
{
    BET_LEFT,
    BET_RIGHT,
    BET_MIDDLE,
    BET_WHEEL_UP,
    BET_WHEEL_DOWN,
    BET_UP,
    BET_DOWN,
    BET_MOVE,
    BET_OTHER
};

bool UIInventoryForm::OnEvent(const Event& evt)
{
    if (evt.mEventType == ET_KEY_INPUT_EVENT)
    {
        KeyAction key(evt.mKeyInput);
        if (evt.mKeyInput.mPressedDown &&
            (key == EscapeKey || key == CancelKey || key == GetKeySetting("keymap_inventory")))
        {
            TryClose();
            return true;
        }
        /*
        if (evt.mKeyInput.mPressedDown && kp == GetKeySetting("keymap_screenshot"))
            MakeScreenshot();
        */

        if (evt.mKeyInput.mPressedDown && key == GetKeySetting("keymap_toggle_debug"))
            mShowDebug = !mShowDebug;

        if (evt.mKeyInput.mPressedDown &&
            (evt.mKeyInput.mKey == KEY_RETURN || evt.mKeyInput.mKey == KEY_UP || evt.mKeyInput.mKey == KEY_DOWN))
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

    /* Mouse event other than movement, or crossing the border of inventory
      field while holding right mouse button
     */
    if (evt.mEventType == ET_MOUSE_INPUT_EVENT &&
        (evt.mMouseInput.mEvent != MIE_MOUSE_MOVED ||
        (evt.mMouseInput.mEvent == MIE_MOUSE_MOVED && 
        evt.mMouseInput.IsRightPressed() &&
        GetItemAtPosition(mPointer).index != GetItemAtPosition(mOldPointer).index)))
    {
        // Get selected item and hovered/clicked item (s)

        mOldTooltipId = -1;
        UpdateSelectedItem();
        UIInventoryList::Item item = GetItemAtPosition(mPointer);

        Inventory* invSelected = NULL;
        Inventory* invItem = NULL;
        InventoryList* listItem = NULL;

        if (mSelectedItem)
        {
            invSelected = mEnvironment->GetInventory(mSelectedItem->inventoryloc);
            LogAssert(invSelected, "invalid selected inventory");
            LogAssert(invSelected->GetList(mSelectedItem->listname), "invalid list selected inventory");
        }

        unsigned int itemCount = 0;
        if (item.IsValid())
        {
            do
            { // breakable
                invItem = mEnvironment->GetInventory(item.inventoryloc);

                if (!invItem)
                {
                    LogWarning("UIInventoryForm: The selected inventory location "
                        "\"" + item.inventoryloc.Dump() + "\" doesn't exist");
                    item.index = -1;  // make it invalid again
                    break;
                }

                listItem = invItem->GetList(item.listname);
                if (listItem == NULL)
                {
                    LogWarning("UIInventoryForm: The selected inventory list \"" +
                        item.listname + "\" does not exist");
                    item.index = -1;  // make it invalid again
                    break;
                }

                if ((unsigned int)item.index >= listItem->GetSize())
                {
                    LogInformation("UIInventoryForm: The selected inventory list \"" +
                        item.listname + "\" is too small (index=" + std::to_string(item.index) +
                        ", size=" + std::to_string(listItem->GetSize()) + ")");
                    item.index = -1;  // make it invalid again
                    break;
                }

                itemCount = listItem->GetItem(item.index).count;
            } while (0);

            bool identical = mSelectedItem && item.IsValid() && (invSelected == invItem) &&
                (mSelectedItem->listname == item.listname) && (mSelectedItem->index == item.index);

            ButtonEventType button = BET_LEFT;
            ButtonEventType updown = BET_OTHER;
            switch (evt.mMouseInput.mEvent)
            {
                case MIE_LMOUSE_PRESSED_DOWN:
                    button = BET_LEFT; updown = BET_DOWN;
                    break;
                case MIE_RMOUSE_PRESSED_DOWN:
                    button = BET_RIGHT; updown = BET_DOWN;
                    break;
                case MIE_MMOUSE_PRESSED_DOWN:
                    button = BET_MIDDLE; updown = BET_DOWN;
                    break;
                case MIE_MOUSE_WHEEL:
                    button = (evt.mMouseInput.mWheel > 0) ?
                        BET_WHEEL_UP : BET_WHEEL_DOWN;
                    updown = BET_DOWN;
                    break;
                case MIE_LMOUSE_LEFT_UP:
                    button = BET_LEFT; updown = BET_UP;
                    break;
                case MIE_RMOUSE_LEFT_UP:
                    button = BET_RIGHT; updown = BET_UP;
                    break;
                case MIE_MMOUSE_LEFT_UP:
                    button = BET_MIDDLE; updown = BET_UP;
                    break;
                case MIE_MOUSE_MOVED:
                    updown = BET_MOVE;
                    break;
                default:
                    break;
            }

            // Set this number to a positive value to generate a move action
            // from mSelectedItem to s.
            unsigned int moveAmount = 0;

            // Set this number to a positive value to generate a move action
            // from s to the next inventory ring.
            unsigned int shiftMoveAmount = 0;

            // Set this number to a positive value to generate a drop action
            // from mSelectedItem.
            unsigned int dropAmount = 0;

            // Set this number to a positive value to generate a craft action at s.
            unsigned int craftAmount = 0;

            switch (updown)
            {
                case BET_DOWN:
                    // Some mouse button has been pressed

                    //infostream << "Mouse button " << button << " pressed at p=("
                    //	<< evt.MouseInput.X << "," << evt.MouseInput.Y << ")"
                    //	<< std::endl;

                    mSelectedDragging = false;

                    if (item.IsValid() && item.listname == "craftpreview")
                    {
                        // Craft preview has been clicked: craft
                        craftAmount = (button == BET_MIDDLE ? 10 : 1);
                    }
                    else if (!mSelectedItem)
                    {
                        if (itemCount && button != BET_WHEEL_UP)
                        {
                            // Non-empty stack has been clicked: select or shift-move it
                            mSelectedItem = new UIInventoryList::Item(item);

                            unsigned int count;
                            if (button == BET_RIGHT)
                                count = (itemCount + 1) / 2;
                            else if (button == BET_MIDDLE)
                                count = std::min((int)itemCount, 10);
                            else if (button == BET_WHEEL_DOWN)
                                count = 1;
                            else  // left
                                count = itemCount;

                            if (!evt.mMouseInput.mShift)
                            {
                                // no shift: select item
                                mSelectedAmount = count;
                                mSelectedDragging = button != BET_WHEEL_DOWN;
                                mAutoPlace = false;
                            }
                            else
                            {
                                // shift pressed: move item, right click moves 1
                                shiftMoveAmount = button == BET_RIGHT ? 1 : count;
                            }
                        }
                    }
                    else
                    {
                        // mSelectedItem != NULL
                        LogAssert(mSelectedAmount >= 1, "invalid selected amount");

                        if (item.IsValid())
                        {
                            // Clicked a slot: move
                            if (button == BET_RIGHT || button == BET_WHEEL_UP)
                                moveAmount = 1;
                            else if (button == BET_MIDDLE)
                                moveAmount = std::min((int)mSelectedAmount, 10);
                            else if (button == BET_LEFT)
                                moveAmount = mSelectedAmount;
                            // else wheeldown

                            if (identical)
                            {
                                if (button == BET_WHEEL_DOWN)
                                {
                                    if (mSelectedAmount < itemCount)
                                        ++mSelectedAmount;
                                }
                                else
                                {
                                    if (moveAmount >= mSelectedAmount)
                                        mSelectedAmount = 0;
                                    else
                                        mSelectedAmount -= moveAmount;
                                    moveAmount = 0;
                                }
                            }
                        }
                        else if (!GetAbsoluteClippingRect().IsPointInside(mPointer) && button != BET_WHEEL_DOWN)
                        {
                            // Clicked outside of the window: drop
                            if (button == BET_RIGHT || button == BET_WHEEL_UP)
                                dropAmount = 1;
                            else if (button == BET_MIDDLE)
                                dropAmount = std::min((int)mSelectedAmount, 10);
                            else  // left
                                dropAmount = mSelectedAmount;
                        }
                    }
                    break;
                case BET_UP:
                    // Some mouse button has been released

                    //infostream<<"Mouse button "<<button<<" released at p=("
                    //	<<p.X<<","<<p.Y<<")"<<std::endl;
                    if (mSelectedDragging && mSelectedItem)
                    {
                        if (item.IsValid())
                        {
                            if (!identical)
                            {
                                // Dragged to different slot: move all selected
                                moveAmount = mSelectedAmount;
                            }
                        }
                        else if (!GetAbsoluteClippingRect().IsPointInside(mPointer))
                        {
                            // Dragged outside of window: drop all selected
                            dropAmount = mSelectedAmount;
                        }
                    }

                    mSelectedDragging = false;
                    // Keep track of whether the mouse button be released
                    // One click is drag without dropping. Click + release
                    // + click changes to drop item when moved mode
                    if (mSelectedItem)
                        mAutoPlace = true;
                    break;
                case BET_MOVE:
                    // Mouse has been moved and rmb is down and mouse pointer just
                    // entered a new inventory field (checked in the entry-if, this
                    // is the only action here that is generated by mouse movement)
                    if (mSelectedItem && item.IsValid() && item.listname != "craftpreview")
                    {
                        // Move 1 item
                        // TODO: middle mouse to move 10 items might be handy
                        if (mAutoPlace)
                        {
                            // Only move an item if the destination slot is empty
                            // or contains the same item type as what is going to be
                            // moved
                            InventoryList* listFrom = invSelected->GetList(mSelectedItem->listname);
                            InventoryList* listTo = listItem;
                            LogAssert(listFrom && listTo, "invalid list from/to");
                            ItemStack stackFrom = listFrom->GetItem(mSelectedItem->index);
                            ItemStack stackTo = listTo->GetItem(item.index);
                            if (stackTo.IsEmpty() || stackTo.name == stackFrom.name)
                                moveAmount = 1;
                        }
                    }
                    break;
                default:
                    break;
            }

            // Possibly send inventory action to logic
            if (moveAmount > 0)
            {
                // Send BaseAction::Move
                LogAssert(mSelectedItem && mSelectedItem->IsValid(), "invalid selected item");
                LogAssert(item.IsValid(), "invalid item");

                LogAssert(invSelected && invItem, "invalid item");
                InventoryList* listFrom = invSelected->GetList(mSelectedItem->listname);
                InventoryList* listTo = listItem;
                LogAssert(listFrom && listTo, "invalid list from/to");
                ItemStack stackFrom = listFrom->GetItem(mSelectedItem->index);
                ItemStack stackTo = listTo->GetItem(item.index);

                // Check how many items can be moved
                moveAmount = stackFrom.count = std::min(moveAmount, (unsigned int)stackFrom.count);
                ItemStack leftover = stackTo.AddItem(stackFrom, mItemMgr);
                bool move = true;
                // If source stack cannot be added to destination stack at all,
                // they are swapped
                if (leftover.count == stackFrom.count &&
                    leftover.name == stackFrom.name)
                {
                    if (mSelectedSwap.IsEmpty())
                    {
                        mSelectedAmount = stackTo.count;
                        mSelectedDragging = false;

                        // WARNING: BLACK MAGIC, BUT IN A REDUCED SET
                        // Skip next validation checks due async inventory calls
                        mSelectedSwap = stackTo;
                    }
                    else
                    {
                        move = false;
                    }
                }

                else if (leftover.IsEmpty())
                {
                    // Source stack goes fully into destination stack
                    mSelectedAmount -= moveAmount;
                }
                else
                {
                    // Source stack goes partly into destination stack
                    moveAmount -= leftover.count;
                    mSelectedAmount -= moveAmount;
                }

                if (move)
                {
                    LogInformation("Handing BaseAction::Move to manager");
                    BaseMoveAction* action = new BaseMoveAction();
                    action->count = moveAmount;
                    action->fromInventory = mSelectedItem->inventoryloc;
                    action->fromList = mSelectedItem->listname;
                    action->fromItem = mSelectedItem->index;
                    action->toInventory = item.inventoryloc;
                    action->toList = item.listname;
                    action->toItem = item.index;
                    mEnvironment->DoInventoryAction(action);
                }
            }
            else if (shiftMoveAmount > 0)
            {
                unsigned int mis = (unsigned int)mInventoryRings.size();
                unsigned int index = 0;
                for (; index < mis; index++)
                {
                    const ListRing& listRing = mInventoryRings[index];
                    if (listRing.inventoryloc == item.inventoryloc &&
                        listRing.listname == item.listname)
                        break;
                }
                do
                {
                    if (index >= mis) // if not found
                        break;

                    unsigned int toInventoryIndex = (index + 1) % mis;
                    const ListRing& toInventoryListRing = mInventoryRings[toInventoryIndex];
                    InventoryList* listFrom = listItem;
                    if (!item.IsValid())
                        break;
                    Inventory* invTo = mEnvironment->GetInventory(toInventoryListRing.inventoryloc);
                    if (!invTo)
                        break;
                    InventoryList* listTo = invTo->GetList(toInventoryListRing.listname);
                    if (!listTo)
                        break;
                    ItemStack stackFrom = listFrom->GetItem(item.index);
                    LogAssert(shiftMoveAmount <= stackFrom.count, "invalid shift move amount");

                    LogInformation("Handing BaseAction::Move to manager");
                    BaseMoveAction* action = new BaseMoveAction();
                    action->count = shiftMoveAmount;
                    action->fromInventory = item.inventoryloc;
                    action->fromList = item.listname;
                    action->fromItem = item.index;
                    action->toInventory = toInventoryListRing.inventoryloc;
                    action->toList = toInventoryListRing.listname;
                    action->moveSomewhere = true;
                    mEnvironment->DoInventoryAction(action);
                } while (0);
            }
            else if (dropAmount > 0)
            {
                // Send BaseAction::Drop

                LogAssert(mSelectedItem && mSelectedItem->IsValid(), "invalid selected item");
                LogAssert(invSelected, "invalid selected inventory");
                InventoryList* listFrom = invSelected->GetList(mSelectedItem->listname);
                LogAssert(listFrom, "invalid list");
                ItemStack stackFrom = listFrom->GetItem(mSelectedItem->index);

                // Check how many items can be dropped
                dropAmount = stackFrom.count = std::min(dropAmount, (unsigned int)stackFrom.count);
                LogAssert(dropAmount > 0 && dropAmount <= mSelectedAmount, "invalid drop amount");
                mSelectedAmount -= dropAmount;

                LogInformation("Handing BaseAction::Drop to manager");
                BaseDropAction* action = new BaseDropAction();
                action->count = dropAmount;
                action->fromInventory = mSelectedItem->inventoryloc;
                action->fromList = mSelectedItem->listname;
                action->fromItem = mSelectedItem->index;
                mEnvironment->DoInventoryAction(action);
            }
            else if (craftAmount > 0)
            {
                LogAssert(item.IsValid(), "invalid item");

                // if there are no items selected or the selected item
                // belongs to craftresult list, proceed with crafting
                if (!mSelectedItem || !mSelectedItem->IsValid() || mSelectedItem->listname == "craftresult")
                {
                    LogAssert(invItem, "invalid inventory item");

                    // Send IACTION_CRAFT
                    LogInformation("Handing BASEACTION_CRAFT to manager");
                    BaseCraftAction* action = new BaseCraftAction();
                    action->count = craftAmount;
                    action->craftInventory = item.inventoryloc;
                    mEnvironment->DoInventoryAction(action);
                }
            }

            // If mSelectedAmount has been decreased to zero, deselect
            if (mSelectedAmount == 0)
            {
                mSelectedSwap.Clear();
                delete mSelectedItem;
                mSelectedItem = nullptr;
                mSelectedAmount = 0;
                mSelectedDragging = false;
            }
            mOldPointer = mPointer;
        }
    }

    if (evt.mEventType == ET_UI_EVENT)
    {
        if (evt.mUIEvent.mEventType == UIEVT_TAB_CHANGED && IsVisible())
        {
            // find the element that was clicked
            for (UIForm::Field& field : mFields)
            {
                if ((field.mType == FFT_TABHEADER) &&
                    (field.mId == evt.mUIEvent.mCaller->GetID()))
                {
                    if (!field.mSound.empty() && mEnvironment->GetSoundManager())
                        mEnvironment->GetSoundManager()->PlaySoundGlobal(field.mSound, false, 1.0f);

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
        if ((evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED) ||
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
                // quitform deallocates form
                return true;
            }

            // find the element that was clicked
            for (UIForm::Field& field : mFields)
            {
                // if its a button, set the send field so
                // it can be known which button was pressed
                if (callerId != field.mId)
                    continue;

                if (field.mType == FFT_BUTTON || field.mType == FFT_CHECKBOX)
                {
                    if (!field.mSound.empty() && mEnvironment->GetSoundManager())
                        mEnvironment->GetSoundManager()->PlaySoundGlobal(field.mSound, false, 1.0f);

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
                    for (UIForm::Field& field2 : mFields)
                        if (field2.mType == FFT_DROPDOWN)
                            field2.mSend = false;

                    if (!field.mSound.empty() && mEnvironment->GetSoundManager())
                        mEnvironment->GetSoundManager()->PlaySoundGlobal(field.mSound, false, 1.0f);

                    field.mSend = true;
                    AcceptInput(FQM_NO);

                    // revert configuration to make sure dropdowns are sent on
                    // regular button click
                    for (UIForm::Field& field2 : mFields)
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
                    if (!field.mSound.empty() && mEnvironment->GetSoundManager())
                        mEnvironment->GetSoundManager()->PlaySoundGlobal(field.mSound, false, 1.0f);

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
                for (UIForm::Field& field : mFields)
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
                // quitform deallocates form
                return true;
            }
        }

        if (evt.mUIEvent.mEventType == UIEVT_TABLE_CHANGED)
        {
            int currentId = evt.mUIEvent.mCaller->GetID();
            if (currentId > 257)
            {
                // find the element that was clicked
                for (UIForm::Field& field : mFields)
                {
                    // if it's a table, set the send field
                    // so it can be known which table was changed
                    if (field.mType == FFT_TABLE && field.mId == currentId)
                    {
                        field.mSend = true;
                        AcceptInput();
                        field.mSend = false;
                    }
                }
            }
        }
    }

    return mParent ? mParent->OnEvent(evt) : false;
}