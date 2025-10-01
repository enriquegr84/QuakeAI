// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UIITEMIMAGE_H
#define UIITEMIMAGE_H

#include "GameEngineStd.h"

#include "../../Games/Actors/Item.h"

#include "Graphic/UI/Element/UIElement.h"
#include "Graphic/UI/Element/UIFont.h"

#include "Graphic/Resource/Color.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

//! ItemImage Element
class UIItemImage : public BaseUIElement
{
public:

	//! constructor
    UIItemImage(BaseUI* ui, int id, RectangleShape<2, int> rectangle, const std::string& itemName, 
        VisualEnvironment* env, BaseItemManager* itemMgr, std::shared_ptr<BaseUIFont> font);

    virtual void Draw();

    virtual void SetText(const wchar_t *text) override
    {
        mLabel = text;
    }

private:

	BaseUI* mUI;

    VisualEnvironment* mEnvironment;

    BaseItemManager* mItemMgr;

    std::wstring mLabel;
    std::string mItemName;
    std::shared_ptr<BaseUIFont> mFont;
};

#endif