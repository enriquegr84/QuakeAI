// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef IMAGE_H
#define IMAGE_H

#include "Graphic/GraphicStd.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Resource/Texture/Texture2.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Geometric/Rectangle.h"

class GRAPHIC_ITEM Image
{
public:
    
    //! copies this surface into another at given position
    static void CopyTo(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source,
        const Vector2<int>& pos = Vector2<int>::Zero());

    //! copies this surface into another at given position
    static void CopyTo(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source,
        const Vector2<int>& pos, const RectangleShape<2, int>& sourceRect, const RectangleShape<2, int>* clipRect = 0);

	//! copies this surface into another, scaling it to fit.
    static void CopyToScaling(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source);

    //! copies this surface into another, using the alpha mask, an cliprect and a color to add with
    static void CopyToWithAlpha(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source, 
        const Vector2<int>& pos, const RectangleShape<2, int>& sourceRect, const SColor& color,
        const RectangleShape<2, int>* clipRect = 0, bool combineAlpha = false);
};

#endif

