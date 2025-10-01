// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIFont.h"

#include "Core/OS/OS.h"
#include "Core/IO/ReadFile.h"
#include "Core/IO/XmlResource.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"

//! constructor
UIFont::UIFont(BaseUI* ui, std::wstring fileName, const std::shared_ptr<Font> font)
:	mUI(ui), mFont(font)
{
	if (mUI)
	{
		mSpriteBank = mUI->GetSpriteBank(fileName);
		if (!mSpriteBank)	// could be default-font which has no file
			mSpriteBank = mUI->AddEmptySpriteBank(fileName);
	}
}

//! constructor
UIFont::UIFont(BaseUI* ui, std::wstring fileName)
	: mUI(ui), mFont(0)
{
	if (mUI)
	{
		mSpriteBank = mUI->GetSpriteBank(fileName);
		if (!mSpriteBank)	// could be default-font which has no file
			mSpriteBank = mUI->AddEmptySpriteBank(fileName);
	}
}


//! destructor
UIFont::~UIFont()
{
	// TODO: spritebank still exists in gui-environment and should be Removed here when it's
	// reference-count is 1. Just can't do that from here at the moment.
	// But spritebank would not be able to drop textures anyway because those are in texture-cache
	// where they can't be Removed unless materials start reference-couting 'em.
}


// Font widht and height info
Vector2<int> UIFont::GetDimension(std::wstring const& message) const
{
	return mFont->GetDimension(message);
}

// Font widht and height info
Vector2<int> UIFont::GetDimension(std::wstring const& message, unsigned int threshold) const
{
    return mFont->GetDimension(message, threshold);
}

// Font text length
unsigned int UIFont::GetLength(std::wstring const& message, unsigned int threshold) const
{
    return mFont->GetLength(message, threshold);
}

std::wstring UIFont::GetText(
    std::wstring const& message, unsigned int offset, unsigned int dimension) const
{
    return mFont->GetText(message, offset, dimension);
}

//! loads a font file, native file needed, for texture parsing
bool UIFont::Load(const std::wstring& filename)
{
	BaseResource resource(filename);
	std::shared_ptr<ResHandle> fontResource = ResCache::Get()->GetHandle(&resource);
	if (fontResource->GetExtra()->ToString() == L"ImageResourceExtraData")
	{
		std::shared_ptr<ImageResourceExtraData> extra = 
			std::static_pointer_cast<ImageResourceExtraData>(fontResource->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		return true;
	}
	else if (fontResource->GetExtra()->ToString() == L"XmlResourceExtraData")
	{
		std::shared_ptr<XmlResourceExtraData> extra = 
			std::static_pointer_cast<XmlResourceExtraData>(fontResource->GetExtra());
		return true;
	}
	return false;
}


//! draws some text and clips it to the specified rectangle if wanted
void UIFont::Draw(const std::wstring& text, const RectangleShape<2, int>& position,
    SColorF const& color, bool hcenter, bool vcenter, const RectangleShape<2, int>* clip)
{
	if (!Renderer::Get())
		return;

    Vector2<int> sourcePos = position.GetVertice(RVP_LOWERLEFT);
    Vector2<int> sourceSize(GetDimension(text));
    Vector2<int> sourceOffset = Vector2<int>::Zero();

    // Determine offset positions.
    if (hcenter || vcenter)
    {
        if (hcenter)
            sourcePos[0] += ((position.mExtent[0] - sourceSize[0]) >> 1);

        if (vcenter)
            sourcePos[1] -= ((position.mExtent[1] - sourceSize[1]) >> 1);
    }

    if (clip)
    {
        if (sourcePos[0] < clip->GetVertice(RVP_UPPERLEFT)[0])
        {
            sourceSize[0] += sourcePos[0] - clip->GetVertice(RVP_UPPERLEFT)[0];
            if (sourceSize[0] < 0)
                return;

            sourceOffset[0] += mFont->GetDimension(
                text, clip->GetVertice(RVP_UPPERLEFT)[0] - sourcePos[0])[0];
            sourcePos[0] = clip->GetVertice(RVP_UPPERLEFT)[0];
        }

        if (sourcePos[0] + sourceSize[0] > clip->GetVertice(RVP_LOWERRIGHT)[0])
        {
            sourceSize[0] -= (sourcePos[0] + sourceSize[0]) - clip->GetVertice(RVP_LOWERRIGHT)[0];
            if (sourceSize[0] < 0)
                return;
        }

        if (sourcePos[1] - sourceSize[1] < clip->GetVertice(RVP_UPPERLEFT)[1])
        {
            return;
            /*
            sourceSize[1] += (sourcePos[1] - sourceSize[1]) - clip->GetVertice(RVP_UPPERLEFT)[1];
            if (sourceSize[1] < 0)
                return;

            sourcePos[1] = clip->GetVertice(RVP_UPPERLEFT)[1];
            */
        }

        if (sourcePos[1] + sourceSize[1] > clip->GetVertice(RVP_LOWERRIGHT)[1])
        {
            sourceSize[1] -= (sourcePos[1] + sourceSize[1]) - clip->GetVertice(RVP_LOWERRIGHT)[1];
            if (sourceSize[1] < 0)
                return;
        }
    }

    // clip these coordinates
    if (sourcePos[0] < 0)
    {
        sourceSize[0] += sourcePos[0];
        if (sourceSize[0] < 0)
            return;

        sourcePos[0] = 0;
        sourceOffset[0] = 0;
    }

    Vector2<unsigned int> renderScreenSize = Renderer::Get()->GetScreenSize();
    if (sourcePos[0] + sourceSize[0] > (int)renderScreenSize[0])
    {
        sourceSize[0] -= (sourcePos[0] + sourceSize[0]) - renderScreenSize[0];
        if (sourceSize[0] < 0)
            return;
    }

    if (sourcePos[1] - sourceSize[1] < 0)
    {
        sourceSize[1] += (sourcePos[1] - sourceSize[1]);
        if (sourceSize[1] < 0)
            return;

        sourcePos[1] = 0;
    }

    if (sourcePos[1] + sourceSize[1] > (int)renderScreenSize[1])
    {
        sourceSize[1] -= (sourcePos[1] + sourceSize[1]) - renderScreenSize[1];
        if (sourceSize[1] < 0)
            return;
    }
    
	Renderer::Get()->Draw( sourcePos[0], sourcePos[1], color, 
        mFont->GetText( text, sourceOffset[0], sourceSize[0]));
}