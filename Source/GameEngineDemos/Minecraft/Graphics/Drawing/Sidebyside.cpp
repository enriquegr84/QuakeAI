/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

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

#include "Sidebyside.h"

#include "../Hud.h"

DrawingCoreSideBySide::DrawingCoreSideBySide(BaseUI* ui, 
    VisualEnvironment* vEnv, Scene* pScene, Hud* hud, bool horizontal, bool flipped)
	: DrawingCoreStereo(ui, vEnv, pScene, hud), mHorizontal(horizontal), mFlipped(flipped)
{
}

void DrawingCoreSideBySide::InitTextures()
{
	if (mHorizontal) 
    {
		mImageSize = {mScreenSize[0], mScreenSize[1] / 2};
        mRectanglePos.mExtent = { (int)mScreenSize[0], (int)mScreenSize[1]};
        mRectanglePos.mCenter = Vector2<int>{ 0, (int)(mScreenSize[1] / 2) };
	} 
    else 
    {
		mImageSize = {mScreenSize[0] / 2, mScreenSize[1]};
        mRectanglePos.mExtent = { (int)mScreenSize[0], (int)mScreenSize[1] };
        mRectanglePos.mCenter = Vector2<int>{ (int)(mScreenSize[0] / 2, 0) };
	}
	mVirtualSize = mImageSize;

    mTexture = std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, mImageSize[0], mImageSize[1], false);
    mDrawTarget = std::make_shared<DrawTarget>(
        1, mTexture->GetFormat(), mTexture->GetDimension(0), mTexture->GetDimension(1), true);
    mDrawTarget->AutogenerateRTMipmaps();
    mDrawTarget->GetRTTexture(0)->SetCopyType(Resource::COPY_STAGING_TO_CPU);

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
    std::shared_ptr<ResHandle> resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

    const std::shared_ptr<ShaderResourceExtraData>& extra =
        std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
    if (!extra->GetProgram())
        extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    mEffect = std::make_shared<Texture2Effect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), mTexture,
        SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

void DrawingCoreSideBySide::ClearTextures()
{

}

void DrawingCoreSideBySide::OnRender()
{
    // Draw the images. The resulting image is stored in the draw target texture.
	Renderer::Get()->Resize(mImageSize[0], mImageSize[1]); // HACK to make GUI smaller
    Renderer::Get()->Enable(mDrawTarget);
    RenderImages();
    Renderer::Get()->Disable(mDrawTarget);
    Renderer::Get()->Resize(mScreenSize[0], mScreenSize[1]);

    // Copy the draw target texture to texture2d.
    Renderer::Get()->CopyGpuToCpu(mDrawTarget->GetRTTexture(0));
    Renderer::Get()->CopyCpuToGpu(mTexture);

    const SColor colors[] = { mSkyColor };
    mEffect->SetTexture(mTexture);

    RectangleShape<2, int> tcoordRect;
    tcoordRect.mExtent = { (int)mImageSize[0], (int)mImageSize[1] };
    tcoordRect.mCenter = tcoordRect.mExtent / 2;

    RectangleShape<2, int> rect = mRectanglePos;
    rect.mCenter = Vector2<int>{ (int)mScreenSize[0] / 2, (int)mScreenSize[1] / 2 };

    mUI->GetSkin()->Draw2DTexture(mVisual, rect, tcoordRect, colors);
    mUI->GetSkin()->Draw2DTexture(mVisual, mRectanglePos, tcoordRect, colors);
}

void DrawingCoreSideBySide::UseEye(bool _right)
{
	//driver->setRenderTarget(_right ? right : left, true, true, skycolor);
	DrawingCoreStereo::UseEye(_right ^ mFlipped);
}

void DrawingCoreSideBySide::ResetEye()
{
	mHud->ResizeHotbar();
	RenderHUD();
	//driver->setRenderTarget(nullptr, false, false, skycolor);
	DrawingCoreStereo::ResetEye();
}
