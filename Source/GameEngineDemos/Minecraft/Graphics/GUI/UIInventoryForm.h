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

#ifndef UIINVENTORYFORM_H
#define UIINVENTORYFORM_H

#include "../../Games/Environment/VisualEnvironment.h"

#include "../../Games/Actors/Item.h"

#include "../Shader.h"

#include "Graphic/UI/Element/UIForm.h"

#include "Graphic/Scene/Hierarchy/Visual.h"

class UIInventoryForm;

class UIInventoryList : public BaseUIElement
{
public:
    struct Item
    {
        Item() = default;

        Item(const InventoryLocation& aInventoryloc,
            const std::string& aListname, int aIndex) :
            inventoryloc(aInventoryloc), listname(aListname), index(aIndex)
        {

        }

        bool IsValid() const { return index != -1; }

        InventoryLocation inventoryloc;
        std::string listname;
        int index = -1;
    };

    // options for inventorylists that are setable with the api
    struct Options 
    {
        // whether a one-pixel border for the slots should be drawn and its color
        bool slotborder = false;
        SColor slotbordercolor = SColor(200, 0, 0, 0);
        // colors for normal and highlighted slot background
        SColor slotbgNormal = SColor(255, 128, 128, 128);
        SColor slotbgHighlighted = SColor(255, 192, 192, 192);
    };

    UIInventoryList(BaseUI* ui, int id, const RectangleShape<2, int>& rectangle, 
        const std::string& listname, VisualEnvironment* env, BaseItemManager* itemMgr, 
        const InventoryLocation& inventoryLoc, const Vector2<int>& geom, const int startItemIndex, 
        const Vector2<int>& slotSize, const Vector2<float>& slotSpacing, 
        UIInventoryForm* form, const Options& options, BaseUIFont* font);

    virtual void Draw() override;

    virtual bool OnEvent(const Event& evt);

    const InventoryLocation& GetInventoryLoc() const
    {
        return mInventoryLoc;
    }

    const std::string& GetListname() const
    {
        return mListName;
    }

    void SetSlotBGColors(const SColor& slotbgNormal, const SColor& slotbgHighlighted)
    {
        mOptions.slotbgNormal = slotbgNormal;
        mOptions.slotbgHighlighted = slotbgHighlighted;
    }

    void SetSlotBorders(bool slotborder, const SColor& slotbordercolor)
    {
        mOptions.slotborder = slotborder;
        mOptions.slotbordercolor = slotbordercolor;
    }

    // returns -1 if not item is at pos p
    int GetItemIndexAtPosition(Vector2<int> p) const;

private:

    BaseUI* mUI;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;

    VisualEnvironment* mEnvironment;

    BaseItemManager* mItemMgr;
    const InventoryLocation mInventoryLoc;

    const std::string mListName;

    // the specified width and height of the shown inventorylist in itemslots
    const Vector2<int> mGeom;
    // the first item's index in inventory
    const int mStartItemIndex;

    // specifies how large the slot rects are
    const Vector2<int> mSlotSize;
    // specifies how large the space between slots is (space between is spacing-size)
    const Vector2<float> mSlotSpacing;

    // the UIInventoryForm can have an item selected and co.
    UIInventoryForm* mUIInventoryForm;

    Options mOptions;

    // the font
    BaseUIFont* mFont;

    // the index of the hovered item; -1 if no item is hovered
    int mHoveredIndex;

    // we do not want to write a warning on every draw
    bool mAlreadyWarned;
};


class UIInventoryForm : public UIForm
{
    struct ListRing
    {
        ListRing() = default;

        ListRing(const InventoryLocation& aInventoryloc, const std::string& aListname) :
            inventoryloc(aInventoryloc), listname(aListname)
        {

        }

        InventoryLocation inventoryloc;
        std::string listname;
    };

public:
	UIInventoryForm(BaseUI* ui, int id, 
        RectangleShape<2, int> rectangle, BaseSimpleTextureSource* textureSrc,
        std::shared_ptr<BaseFormSource> formSrc, std::shared_ptr<TextDestination> txtDst, 
        const std::string& formPrepend, VisualEnvironment* env, BaseItemManager* ItemMgr, 
        const InventoryLocation& inventoryLoc, bool remapDoubleClick = true);

    ~UIInventoryForm();

    void SetForm(const std::string& formString, const InventoryLocation& inventoryLocation)
    {
        mInventoryLocation = inventoryLocation;
        UIForm::SetForm(formString);
    }

    const InventoryLocation& GetFormLocation()
    {
        return mInventoryLocation;
    }

    const UIInventoryList::Item* GetSelectedItem() const
    {
        return mSelectedItem;
    }

	const uint16_t GetSelectedAmount() const
	{
		return mSelectedAmount;
	}

	bool DoTooltipAppendItemname() const
	{
		return mTooltipAppendItemName;
	}

	void AddHoveredItemTooltip(const std::string& name)
	{
		mHoveredItemTooltips.emplace_back(name);
	}

    UIInventoryList::Item GetItemAtPosition(Vector2<int> p) const;
    void DrawSelectedItem();
    void UpdateSelectedItem();
    ItemStack VerifySelectedItem();

    // Remove and re-add (or reposition) stuff
    virtual void RegenerateUI(Vector2<unsigned int> screenSize);

    virtual void DrawForm();

    virtual bool OnPreEvent(const Event& evt);
    virtual bool OnEvent(const Event& evt);

protected:

    class InventoryParserData : public ParserData
    {
    public:
        UIInventoryList::Options mInventorylistOptions;
    };

    BaseItemManager* mItemMgr;
    VisualEnvironment* mEnvironment;

    InventoryLocation mInventoryLocation;
    std::vector<std::shared_ptr<UIInventoryList>> mInventorylists;
    std::vector<ListRing> mInventoryRings;

    UIInventoryList::Item* mSelectedItem = nullptr;
	unsigned short mSelectedAmount = 0;
	bool mSelectedDragging = false;
	ItemStack mSelectedSwap;

    bool mAutoPlace = false;

	bool mTooltipAppendItemName;

    virtual void ParseElement(ParserData* data, const std::string& element);
    virtual void ParseList(ParserData* data, const std::string& element);
    virtual void ParseImage(ParserData* data, const std::string& element);
    virtual void ParseBackground(ParserData* data, const std::string& element);

    void ParseListColors(ParserData* data, const std::string& element);
    void ParseListRing(ParserData* data, const std::string& element);
    void ParseItemImage(ParserData* data, const std::string& element);
    void ParseItemImageButton(ParserData* data, const std::string& element);
    void ParseModel(ParserData *data, const std::string& element);

private:

    BaseSimpleTextureSource* mTextureSrc;

    std::vector<std::string> mHoveredItemTooltips;
};


#endif