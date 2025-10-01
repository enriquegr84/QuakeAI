#include "SelectionEffect.h"

SelectionEffect::SelectionEffect(std::shared_ptr<VisualProgram> const& program, 
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

        mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
#if defined(_OPENGL_)
        mProgram->GetPShader()->Set("baseSampler", texture);
#else
        mProgram->GetPShader()->Set("baseTexture", texture);
#endif
        mProgram->GetPShader()->Set("baseSampler", mSampler);
    }
}

void SelectionEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void SelectionEffect::SetTexture(std::shared_ptr<Texture2> const& texture)
{
    mTexture = texture;
#if defined(_OPENGL_)
    mProgram->GetPShader()->Set("baseSampler", mTexture);
#else
    mProgram->GetPShader()->Set("baseTexture", mTexture);
#endif
}