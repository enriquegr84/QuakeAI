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

#include "Interlaced.h"

#include "../../Games/Environment/VisualEnvironment.h"

#include "../Shader.h"

DrawingCoreInterlaced::DrawingCoreInterlaced(
    BaseUI* ui, VisualEnvironment* vEnv, Scene* pScene, Hud* hud)
	: DrawingCoreStereo(ui, vEnv, pScene, hud)
{
	InitMaterial();
}

void DrawingCoreInterlaced::InitMaterial()
{
    mBlendState = std::make_shared<BlendState>();
    mDepthStencilState = std::make_shared<DepthStencilState>();
    mRasterizerState = std::make_shared<RasterizerState>();

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
    vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);

    MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));

    // A point light illuminates the target.  Create a semitransparent
    // material for the patch.
    meshBuffer->GetMaterial() = std::make_shared<Material>();
    meshBuffer->GetMaterial()->mDepthBuffer = false;
    meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ZERO;

    unsigned int shader = mVisualEnv->GetShaderSource()->GetShader("InterlacedMerge", TILE_MATERIAL_BASIC);
    meshBuffer->GetMaterial()->mType = mVisualEnv->GetShaderSource()->GetShaderInfo(shader).material;
    meshBuffer->GetMaterial()->mTypeParam2 = shader;
    for (int k = 0; k < 3; ++k)
    {
        meshBuffer->GetMaterial()->mTextureLayer[k].mFilter = SamplerState::MIN_L_MAG_L_MIP_P;
        meshBuffer->GetMaterial()->mTextureLayer[k].mModeU = SamplerState::CLAMP;
        meshBuffer->GetMaterial()->mTextureLayer[k].mModeV = SamplerState::CLAMP;
    }
    mMeshBuffer.reset(meshBuffer);

    // fill vertices
    for (unsigned int i = 0; i < mMeshBuffer->GetVertice()->GetNumElements(); i += 4)
    {
        mMeshBuffer->Position(0 + i) = Vector3<float>{ 1.f, -1.f, 0.f };
        mMeshBuffer->Position(1 + i) = Vector3<float>{ -1.f, 1.f, 0.f };
        mMeshBuffer->Position(2 + i) = Vector3<float>{ 1.f, 1.f, 0.f };
        mMeshBuffer->Position(3 + i) = Vector3<float>{ 1.f, -1.f, 0.f };

        mMeshBuffer->Normal(0 + i) = Vector3<float>{ 0, 0, -1 };
        mMeshBuffer->Normal(1 + i) = Vector3<float>{ 0, 0, -1 };
        mMeshBuffer->Normal(2 + i) = Vector3<float>{ 0, 0, -1 };
        mMeshBuffer->Normal(3 + i) = Vector3<float>{ 0, 0, -1 };

        mMeshBuffer->Color(0, 0 + i) = SColorF(0, 1.f, 1.f, 1.f).ToArray();
        mMeshBuffer->Color(0, 1 + i) = SColorF(1.f, 0, 1.f, 1.f).ToArray();
        mMeshBuffer->Color(0, 2 + i) = SColorF(1.f, 1.f, 0, 1.f).ToArray();
        mMeshBuffer->Color(0, 3 + i) = SColorF(1.f, 1.f, 1.f, 1.f).ToArray();

        mMeshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 1.0f, 0.0f };
        mMeshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 0.0f, 0.0f };
        mMeshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 0.0f, 1.0f };
        mMeshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 1.0f, 1.0f };
    }

    // fill indices
    int vertices = 0;
    for (unsigned int i = 0; i < mMeshBuffer->GetIndice()->GetNumPrimitives(); i += 2, vertices += 4)
    {
        mMeshBuffer->GetIndice()->SetTriangle(i,
            (unsigned int)0 + vertices, (unsigned int)1 + vertices, (unsigned int)2 + vertices);
        mMeshBuffer->GetIndice()->SetTriangle(i + 1,
            (unsigned int)2 + vertices, (unsigned int)3 + vertices, (unsigned int)0 + vertices);
    }
}

void DrawingCoreInterlaced::InitTextures()
{
    mMeshBuffer->GetMaterial()->mTextureLayer[0].mTexture = 
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, mScreenSize[0], mScreenSize[1], false);
    InitMask();

    Vector2<unsigned int> imageSize{ mScreenSize[0], mScreenSize[1] / 2 };
    mMeshBuffer->GetMaterial()->mTextureLayer[1].mTexture = 
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, imageSize[0], imageSize[1], false);
    mMeshBuffer->GetMaterial()->mTextureLayer[2].mTexture = 
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, imageSize[0], imageSize[1], false);

    mDrawTarget = std::make_shared<DrawTarget>(
        1, DF_R8G8B8A8_UNORM, imageSize[0], imageSize[1], true);
    mDrawTarget->GetRTTexture(0)->SetCopyType(Resource::COPY_STAGING_TO_CPU);

    // Create an effect for the vertex and pixel shaders. The texture is
    // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
    ShaderInfo shader = mVisualEnv->GetShaderSource()->GetShaderInfo(
        mMeshBuffer->GetMaterial()->mTypeParam2);
    std::shared_ptr<InterlacedMergeEffect> effect = std::make_shared<InterlacedMergeEffect>(
        ProgramFactory::Get()->CreateFromProgram(shader.visualProgram),
        mMeshBuffer->GetMaterial()->mTextureLayer[1].mTexture,
        mMeshBuffer->GetMaterial()->mTextureLayer[0].mTexture,
        mMeshBuffer->GetMaterial()->mTextureLayer[2].mTexture,
        mMeshBuffer->GetMaterial()->mTextureLayer[1].mFilter,
        mMeshBuffer->GetMaterial()->mTextureLayer[1].mModeU,
        mMeshBuffer->GetMaterial()->mTextureLayer[1].mModeV);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(
        mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), effect);
}

void DrawingCoreInterlaced::ClearTextures()
{

}

void DrawingCoreInterlaced::InitMask()
{
    BYTE* data = mMeshBuffer->GetMaterial()->mTextureLayer[0].mTexture->Get<BYTE>();
    for (unsigned int j = 0; j < mScreenSize[1]; j++)
    {
        uint8_t val = j % 2 ? 0xff : 0x00;
        memset(data, val, 4 * mScreenSize[0]);
        data += 4 * mScreenSize[0];
    }

}

void DrawingCoreInterlaced::OnRender()
{
    Renderer::Get()->Enable(mDrawTarget);

	RenderImages();
	Merge();
	RenderHUD();
}

void DrawingCoreInterlaced::Merge()
{
    std::shared_ptr<ObjectEffect> effect =
        std::dynamic_pointer_cast<ObjectEffect>(mVisual->GetEffect());
    if (!effect)
        return;

    Renderer::Get()->Disable(mDrawTarget);

    // Copy the draw target texture to texture2d.
    Renderer::Get()->CopyGpuToCpu(mDrawTarget->GetRTTexture(0));
    Renderer::Get()->CopyCpuToGpu(mMeshBuffer->GetMaterial()->mTextureLayer[1].mTexture);

    const SColor colors[] = { mSkyColor };

    effect->SetTexture(mMeshBuffer->GetMaterial()->mTextureLayer[1].mTexture);

    if (mMeshBuffer->GetMaterial()->Update(mBlendState))
        Renderer::Get()->Unbind(mBlendState);
    if (mMeshBuffer->GetMaterial()->Update(mDepthStencilState))
        Renderer::Get()->Unbind(mDepthStencilState);
    if (mMeshBuffer->GetMaterial()->Update(mRasterizerState))
        Renderer::Get()->Unbind(mRasterizerState);

    Renderer::Get()->SetBlendState(mBlendState);
    Renderer::Get()->SetDepthStencilState(mDepthStencilState);
    Renderer::Get()->SetRasterizerState(mRasterizerState);

    Renderer* renderer = Renderer::Get();
    renderer->Update(mVisual->GetVertexBuffer());
    renderer->Draw(mVisual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();

}

void DrawingCoreInterlaced::UseEye(bool _right)
{
	//driver->setRenderTarget(_right ? right : left, true, true, skycolor);
	DrawingCoreStereo::UseEye(_right);
}

void DrawingCoreInterlaced::ResetEye()
{
	//driver->setRenderTarget(nullptr, false, false, skycolor);
	DrawingCoreStereo::ResetEye();
}
