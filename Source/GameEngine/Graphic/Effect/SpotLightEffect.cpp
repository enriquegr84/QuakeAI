// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "SpotLightEffect.h"

SpotLightEffect::SpotLightEffect(std::shared_ptr<VisualProgram> const& program, 
    BufferUpdater const& updater, int select, std::shared_ptr<Material> const& material, 
    std::shared_ptr<Lighting> const& lighting, std::shared_ptr<LightCameraGeometry> const& geometry)
    : LightingEffect(program, updater, material, lighting, geometry)
{
    mMaterialConstant = std::make_shared<ConstantBuffer>(sizeof(InternalMaterial), true);
    UpdateMaterialConstant();

    mLightingConstant = std::make_shared<ConstantBuffer>(sizeof(InternalLighting), true);
    UpdateLightingConstant();

    mGeometryConstant = std::make_shared<ConstantBuffer>(sizeof(InternalGeometry), true);
    UpdateGeometryConstant();

    if ((select & 1) == 0)
    {
        mProgram->GetVertexShader()->Set("Material", mMaterialConstant);
        mProgram->GetVertexShader()->Set("Lighting", mLightingConstant);
        mProgram->GetVertexShader()->Set("LightCameraGeometry", mGeometryConstant);
    }
    else
    {
        mProgram->GetPixelShader()->Set("Material", mMaterialConstant);
        mProgram->GetPixelShader()->Set("Lighting", mLightingConstant);
        mProgram->GetPixelShader()->Set("LightCameraGeometry", mGeometryConstant);
    }
}

void SpotLightEffect::UpdateMaterialConstant()
{
    InternalMaterial* internalMaterial = mMaterialConstant->Get<InternalMaterial>();
    internalMaterial->emissive = mMaterial->mEmissive;
    internalMaterial->ambient = mMaterial->mAmbient;
    internalMaterial->diffuse = mMaterial->mDiffuse;
    internalMaterial->specular = mMaterial->mSpecular;
    LightingEffect::UpdateMaterialConstant();
}

void SpotLightEffect::UpdateLightingConstant()
{
    InternalLighting* internalLighting = mLightingConstant->Get<InternalLighting>();
    internalLighting->ambient = mLighting->mAmbient;
    internalLighting->diffuse = mLighting->mDiffuse;
    internalLighting->specular = mLighting->mSpecular;
    internalLighting->spotCutoff = mLighting->mSpotCutoff;
    internalLighting->attenuation = mLighting->mAttenuation;
    LightingEffect::UpdateLightingConstant();
}

void SpotLightEffect::UpdateGeometryConstant()
{
    InternalGeometry* internalGeometry = mGeometryConstant->Get<InternalGeometry>();
    internalGeometry->lightModelPosition = mGeometry->lightModelPosition;
    internalGeometry->lightModelDirection = mGeometry->lightModelDirection;
    internalGeometry->cameraModelPosition = mGeometry->cameraModelPosition;
    LightingEffect::UpdateGeometryConstant();
}
