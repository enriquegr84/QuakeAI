// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/13)

#include "Texture2ArrayEffect.h"

Texture2ArrayEffect::Texture2ArrayEffect(
	std::shared_ptr<VisualProgram> const& program, std::shared_ptr<Texture2Array> const& textures,
	SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1)
{
	mProgram = program;
    if (mProgram)
    {
		mSampler = std::make_shared<SamplerState>();
		mSampler->mFilter = filter;
		mSampler->mMode[0] = mode0;
		mSampler->mMode[1] = mode1;

		mTextures = textures;
#if defined(_OPENGL_)
		mProgram->GetPixelShader()->Set("baseSampler", mTextures);
		mProgram->GetPixelShader()->Set("baseSampler", mSampler);
#else
		mProgram->GetPixelShader()->Set("baseTextureArray", mTextures);
#endif

#if defined(_OPENGL_)
		mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
#else
		mProgram->GetPixelShader()->Set("baseSampler", mSampler);
		mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
#endif
    }
}

void Texture2ArrayEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void Texture2ArrayEffect::SetTextures(std::shared_ptr<Texture2Array> const& textures)
{
	mTextures = textures;
#if defined(_OPENGL_)
	mProgram->GetPixelShader()->Set("baseSampler", mTextures);
	mProgram->GetPixelShader()->Set("baseSampler", mSampler);
#else
	mProgram->GetPixelShader()->Set("baseTextureArray", mTextures);
#endif

}
