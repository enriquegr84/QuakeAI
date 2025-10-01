// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UIBOX_H
#define UIBOX_H

#include "GameEngineStd.h"

#include "UIElement.h"

#include "Graphic/Resource/Color.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUIFont;

//! Box Element
class BaseUIBox : public BaseUIElement
{
public:

    //! constructor
    BaseUIBox(int id, RectangleShape<2, int> rectangle) :
        BaseUIElement(UIET_BOX, id, rectangle)
    {

    }

};

//! Box Element
class UIBox : public BaseUIBox
{
public:

	//! constructor
    UIBox(BaseUI* ui, int id,
        RectangleShape<2, int> rectangle,
        const std::array<SColor, 4>& colors,
        const std::array<SColor, 4>& bordercolors,
        const std::array<int, 4>& borderwidths);

    virtual void Draw();

private:

	BaseUI* mUI;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;

    std::array<SColor, 4> mColors;
    std::array<SColor, 4> mBorderColors;
    std::array<INT, 4> mBorderWidths;
};

#endif