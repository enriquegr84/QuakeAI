// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/13)

#include "Texture2Effect.h"

Texture2Effect::Texture2Effect(std::shared_ptr<VisualProgram> const& program,
    std::shared_ptr<Texture2> const& texture, SamplerState::Filter filter, 
    SamplerState::Mode mode0, SamplerState::Mode mode1)
    : mTexture(texture)
{
    mProgram = program;
    if (mProgram)
    {
        mSampler = std::make_shared<SamplerState>();
        mSampler->mFilter = filter;
        mSampler->mMode[0] = mode0;
        mSampler->mMode[1] = mode1;

        mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
#if defined(_OPENGL_)
        mProgram->GetPixelShader()->Set("baseSampler", texture);
#else
        mProgram->GetPixelShader()->Set("baseTexture", texture);
#endif
        mProgram->GetPixelShader()->Set("baseSampler", mSampler);
    }
}

void Texture2Effect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void Texture2Effect::SetTexture(std::shared_ptr<Texture2> const& texture)
{
	mTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPixelShader()->Set("baseSampler", mTexture);
#else
	mProgram->GetPixelShader()->Set("baseTexture", mTexture);
#endif
}