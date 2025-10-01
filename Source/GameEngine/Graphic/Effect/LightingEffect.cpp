// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/13)

#include "LightingEffect.h"

#include "Mathematic/Algebra/Matrix4x4.h"

LightingEffect::LightingEffect(
    std::shared_ptr<VisualProgram> const& program, BufferUpdater const& updater,
	std::shared_ptr<Material> const& material, std::shared_ptr<Lighting> const& lighting,
	std::shared_ptr<LightCameraGeometry> const& geometry) :
    mMaterial(material), mLighting(lighting), mGeometry(geometry)
{
	mProgram = program;
    if (mProgram)
    {
        mBufferUpdater = updater;
        mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
    }
}

void LightingEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void LightingEffect::UpdateMaterialConstant()
{
    if (mMaterialConstant)
    {
        mBufferUpdater(mMaterialConstant);
    }
}

void LightingEffect::UpdateLightingConstant()
{
    if (mLightingConstant)
    {
        mBufferUpdater(mLightingConstant);
    }
}

void LightingEffect::UpdateGeometryConstant()
{
    if (mGeometryConstant)
    {
        mBufferUpdater(mGeometryConstant);
    }
}