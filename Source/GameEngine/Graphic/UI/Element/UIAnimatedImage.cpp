// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIAnimatedImage.h"

#include "Core/OS/OS.h"

#include "Graphic/Image/ImageResource.h"
#include "Graphic/Renderer/Renderer.h"

//! constructor
UIAnimatedImage::UIAnimatedImage(BaseUI* ui, int id, RectangleShape<2, int> rectangle, 
    const std::wstring &textureName, int frameCount, int frameDuration)
	: BaseUIAnimatedImage(id, rectangle), mUI(ui), mGlobalTime(0), mFrameDuration(1), 
    mFrameTime(0), mFrameIndex(0), mFrameCount(1)
{
    mFrameCount = std::max(frameCount, 1);
    mFrameDuration = std::max(frameDuration, 0);

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(textureName));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        mTexture = extra->GetImage();
        mTexture->AutogenerateMipmaps();
        if (extra->GetImage()->GetDimension(1) < mFrameCount)
            mFrameCount = extra->GetImage()->GetDimension(1);

		mBlendState = std::make_shared<BlendState>();

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
    else
    {
        // No need to step an animation if we have nothing to draw
        mFrameCount = 1;
    }
}


//! destructor
UIAnimatedImage::~UIAnimatedImage()
{
}

//! draws the element and its children
void UIAnimatedImage::Draw()
{
	if (!IsVisible())
		return;

    // Render the current frame
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    if (mTexture) 
    {
        const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();

        const SColor color(255, 255, 255, 255);
        const SColor colors[] = { color, color, color, color };

        Vector2<unsigned int> size{ mTexture->GetWidth(), mTexture->GetHeight() };
        size[1] /= mFrameCount;

        RectangleShape<2, int> rect;
        rect.mExtent[0] = size[0];
        rect.mExtent[1] = size[1];
        rect.mCenter[0] = rect.mExtent[0] / 2;
        rect.mCenter[1] = (size[1] * mFrameIndex) + rect.mExtent[1] / 2;

        skin->Draw2DTextureFilterScaled(mVisual, mAbsoluteRect, rect, colors);
    }

    // Step the animation
    if (mFrameCount > 1 && mFrameDuration > 0) 
    {
        // Determine the delta time to step
        unsigned int newGlobalTime = Timer::GetTime();
        if (newGlobalTime > 0)
            mFrameTime += newGlobalTime - mGlobalTime;

        mGlobalTime = newGlobalTime;

        // Advance by the number of elapsed frames, looping if necessary
        mFrameIndex += mFrameTime / mFrameDuration;
        mFrameIndex %= mFrameCount;

        // If 1 or more frames have elapsed, reset the frame time counter with
        // the remainder
        mFrameTime %= mFrameDuration;
    }
}

//! sets frame index
void UIAnimatedImage::SetFrameIndex(int frame)
{
    int idx = std::max(frame, 0);
    if (idx > 0 && idx < mFrameCount)
        mFrameIndex = idx;
}

//! Gets the image texture
int UIAnimatedImage::GetFrameIndex() const
{
    return mFrameIndex;
}