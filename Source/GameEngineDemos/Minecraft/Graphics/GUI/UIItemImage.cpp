// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIItemImage.h"

#include "../Hud.h"

#include "../../Games/Actors/Inventory.h"
#include "../../Games/Actors/InventoryManager.h"

//! constructor
UIItemImage::UIItemImage(BaseUI* ui, int id, RectangleShape<2, int> rectangle, 
    const std::string& itemName, VisualEnvironment* env, BaseItemManager* itemMgr,
    std::shared_ptr<BaseUIFont> font)
    : BaseUIElement(UIET_ELEMENT, id, rectangle), mUI(ui), mEnvironment(env),
    mItemName(itemName), mItemMgr(itemMgr), mFont(font), mLabel(std::wstring())
{
}

void UIItemImage::Draw( )
{
    if (!IsVisible())
        return;

    ItemStack item;
    item.Deserialize(mItemName, mItemMgr);

    // Viewport rectangle on screen
    RectangleShape<2, int> rect = RectangleShape<2, int>(mAbsoluteRect);
    DrawItemStack(mUI, mEnvironment, item, rect, &mAbsoluteClippingRect, IT_ROT_NONE);
    
    SColor color(255, 255, 255, 255);
    mFont->Draw(mLabel, rect, color, true, true, &mAbsoluteClippingRect);

    BaseUIElement::Draw();
}

