// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "PointLightTextureEffect.h"

PointLightTextureEffect::PointLightTextureEffect(
    std::shared_ptr<VisualProgram> const& program, BufferUpdater const& updater, 
	std::shared_ptr<Material> const& material, std::shared_ptr<Lighting> const& lighting, 
	std::shared_ptr<LightCameraGeometry> const& geometry, std::shared_ptr<Texture2> const& texture, 
	SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1)
    : LightingEffect(program, updater, material, lighting, geometry), mTexture(texture)
{
    mSampler = std::make_shared<SamplerState>();
    mSampler->mFilter = filter;
    mSampler->mMode[0] = mode0;
    mSampler->mMode[1] = mode1;

    mMaterialConstant = std::make_shared<ConstantBuffer>(sizeof(InternalMaterial), true);
    UpdateMaterialConstant();

    mLightingConstant = std::make_shared<ConstantBuffer>(sizeof(InternalLighting), true);
    UpdateLightingConstant();

    mGeometryConstant = std::make_shared<ConstantBuffer>(sizeof(InternalGeometry), true);
    UpdateGeometryConstant();

    mProgram->GetPixelShader()->Set("Material", mMaterialConstant);
    mProgram->GetPixelShader()->Set("Lighting", mLightingConstant);
    mProgram->GetPixelShader()->Set("LightCameraGeometry", mGeometryConstant);
#if defined(_OPENGL_)
    mProgram->GetPixelShader()->Set("baseSampler", mTexture);
#else
    mProgram->GetPixelShader()->Set("baseTexture", mTexture);
#endif
    mProgram->GetPixelShader()->Set("baseSampler", mSampler);
}

void PointLightTextureEffect::SetTexture(std::shared_ptr<Texture2> const& texture)
{
	mTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPixelShader()->Set("baseSampler", mTexture);
#else
	mProgram->GetPixelShader()->Set("baseTexture", mTexture);
#endif
}

void PointLightTextureEffect::UpdateMaterialConstant()
{
    InternalMaterial* internalMaterial = mMaterialConstant->Get<InternalMaterial>();
    internalMaterial->emissive = mMaterial->mEmissive;
    internalMaterial->ambient = mMaterial->mAmbient;
    internalMaterial->diffuse = mMaterial->mDiffuse;
    internalMaterial->specular = mMaterial->mSpecular;
    LightingEffect::UpdateMaterialConstant();
}

void PointLightTextureEffect::UpdateLightingConstant()
{
    InternalLighting* internalLighting = mLightingConstant->Get<InternalLighting>();
    internalLighting->ambient = mLighting->mAmbient;
    internalLighting->diffuse = mLighting->mDiffuse;
    internalLighting->specular = mLighting->mSpecular;
    internalLighting->attenuation = mLighting->mAttenuation;
    LightingEffect::UpdateLightingConstant();
}

void PointLightTextureEffect::UpdateGeometryConstant()
{
    InternalGeometry* internalGeometry = mGeometryConstant->Get<InternalGeometry>();
    internalGeometry->lightModelPosition = mGeometry->lightModelPosition;
    internalGeometry->cameraModelPosition = mGeometry->cameraModelPosition;
    LightingEffect::UpdateGeometryConstant();
}