// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIImage.h"

#include "Core/OS/OS.h"

#include "Graphic/Image/ImageResource.h"
#include "Graphic/Renderer/Renderer.h"

//! constructor
UIImage::UIImage(BaseUI* ui, int id, RectangleShape<2, int> rectangle)
	: BaseUIImage(id, rectangle), mUI(ui), mTexture(0), mColor{ 1.f, 1.f, 1.f, 1.f },
	mBackground(false), mUseAlphaChannel(false), mScaleImage(false)
{
	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		mBlendState = std::make_shared<BlendState>();
        mBlendState->mTarget[0].enable = true;
        mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

		// Create a vertex buffer for a two-triangles square. The PNG is stored
		// in left-handed coordinates. The texture coordinates are chosen to
		// reflect the texture in the y-direction.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		// Create an effect for the vertex and pixel shaders. The texture is
		// bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extraRes =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extraRes->GetProgram())
            extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		mEffect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
	}
}


//! destructor
UIImage::~UIImage()
{

}


//! Sets if the image should be background
void UIImage::SetBackground(bool autoclip, RectangleShape<2, int> middle)
{
    mBackground = true;
    mBGAutoclip = autoclip;
    mBGMiddle = middle;
}


//! sets a texture
void UIImage::SetTexture(const std::shared_ptr<Texture2>& texture)
{
	if (texture == mTexture)
		return;
	mTexture = texture;
}

//! Gets the texture
const std::shared_ptr<Texture2>& UIImage::GetTexture() const
{
	return mTexture;
}

//! sets the color of the image
void UIImage::SetColor(SColorF color)
{
	mColor = color;
}

//! Gets the color of the image
SColorF UIImage::GetColor() const
{
	return mColor;
}

//! draws the element and its children
void UIImage::Draw()
{
	if (!IsVisible())
		return;

    Renderer::Get()->SetBlendState(mBlendState);

    if (mBackground)
    {
        RectangleShape<2, int> rect = mAbsoluteRect;
        if (mBGAutoclip)
        {
            rect.mExtent += mParent->GetAbsolutePosition().mExtent;
            rect.mCenter += mParent->GetAbsolutePosition().mExtent / 2;
        }

        const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
        auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
        effect->SetTexture(mTexture);

        if (mBGMiddle.GetArea())
        {
            RectangleShape<2, int > middle = mBGMiddle;
            // `-x` is interpreted as `w - x`
            if (middle.mExtent[0] < 0)
            {
                middle.mExtent[0] += effect->GetTexture()->GetDimension(0);
                middle.mCenter[0] += effect->GetTexture()->GetDimension(0) / 2;
            }
            if (middle.mExtent[1] < 0)
            {
                middle.mExtent[1] += effect->GetTexture()->GetDimension(1);
                middle.mCenter[1] += effect->GetTexture()->GetDimension(1) / 2;
            }

            skin->Draw2DTexture9Slice(mVisual, rect, middle);
        }
        else
        {
            const SColor color(255, 255, 255, 255);
            const SColor colors[] = { color, color, color, color };

            RectangleShape<2, int> tcoordRect;
            tcoordRect.mExtent = Vector2<int>{
                (int)effect->GetTexture()->GetDimension(0) ,
                (int)effect->GetTexture()->GetDimension(1) };
            tcoordRect.mCenter = tcoordRect.mExtent / 2;

            skin->Draw2DTextureFilterScaled(mVisual, rect, tcoordRect, colors);
        }

    }
    else
    { 
        const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
        if (mTexture)
        {
            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
            effect->SetTexture(mTexture);

            RectangleShape<2, int> tcoordRect;
            tcoordRect.mExtent = Vector2<int>{
                (int)effect->GetTexture()->GetDimension(0) ,
                (int)effect->GetTexture()->GetDimension(1) };
            tcoordRect.mCenter = tcoordRect.mExtent / 2;

            skin->Draw2DTexture(mVisual, mAbsoluteRect, tcoordRect);
        }
    }

    Renderer::Get()->SetDefaultBlendState();

	BaseUIElement::Draw();
}


//! sets if the image should use its alpha channel to draw itself
void UIImage::SetUseAlphaChannel(bool use)
{
	mUseAlphaChannel = use;

	if (mUseAlphaChannel)
	{
        mBlendState->mTarget[0].enable = true;
        mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;
	}
	else mBlendState->mTarget[0] = BlendState::Target();

	Renderer::Get()->Unbind(mBlendState);
}


//! sets if the image should use its alpha channel to draw itself
void UIImage::SetScaleImage(bool scale)
{
	mScaleImage = scale;
}


//! Returns true if the image is scaled to fit, false if not
bool UIImage::IsImageScaled() const
{
	return mScaleImage;
}


//! Returns true if it is a background image, false if not
bool UIImage::IsBackgroundImage() const
{
    return mBackground;
}


//! Returns true if the image is using the alpha channel, false if not
bool UIImage::IsAlphaChannelUsed() const
{
	return mUseAlphaChannel;
}