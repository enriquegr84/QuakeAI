// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UIANIMATEDIMAGE_H
#define UIANIMATEDIMAGE_H

#include "UIElement.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

//! BaseUI element displaying an image.
class BaseUIAnimatedImage : public BaseUIElement
{
public:

	//! constructor
    BaseUIAnimatedImage(int id, RectangleShape<2, int> rectangle)
		: BaseUIElement(UIET_ANIMATED_IMAGE, id, rectangle) {}

	//! Sets frame index
	virtual void SetFrameIndex(int frame) = 0;

	//! Gets frame index
	virtual int GetFrameIndex() const = 0;
};

class UIAnimatedImage : public BaseUIAnimatedImage
{
public:

	//! constructor
	UIAnimatedImage(BaseUI* ui, int id, RectangleShape<2, int> rectangle, 
        const std::wstring& textureName, int frameCount, int frameDuration);

	//! destructor
	virtual ~UIAnimatedImage();

    //! Sets frame index
    virtual void SetFrameIndex(int frame);

    //! Gets frame index
    virtual int GetFrameIndex() const;

	//! draws the element and its children
	virtual void Draw();

private:

	BaseUI* mUI;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<VisualEffect> mEffect;
	std::shared_ptr<BlendState> mBlendState;

    std::shared_ptr<Texture2> mTexture;
    unsigned int mGlobalTime;
    unsigned int mFrameDuration;
    unsigned int mFrameTime;
    int mFrameIndex;
    int mFrameCount;

};

#endif
