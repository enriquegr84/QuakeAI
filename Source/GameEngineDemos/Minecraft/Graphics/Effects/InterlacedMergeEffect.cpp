
#include "InterlacedMergeEffect.h"

InterlacedMergeEffect::InterlacedMergeEffect(std::shared_ptr<VisualProgram> const& program, 
	std::shared_ptr<Texture2> const& base, std::shared_ptr<Texture2> const& normal, 
	std::shared_ptr<Texture2> const& flag, SamplerState::Filter filter, 
	SamplerState::Mode mode0, SamplerState::Mode mode1)
	: mBaseTexture(base), mNormalTexture(normal), mFlagTexture(flag)
{
	mProgram = program;
	if (mProgram)
	{
		mBaseSampler = std::make_shared<SamplerState>();
		mBaseSampler->mFilter = filter;
		mBaseSampler->mMode[0] = mode0;
		mBaseSampler->mMode[1] = mode1;

		mNormalSampler = std::make_shared<SamplerState>();
		mNormalSampler->mFilter = filter;
		mNormalSampler->mMode[0] = mode0;
		mNormalSampler->mMode[1] = mode1;

		mFlagSampler = std::make_shared<SamplerState>();
		mFlagSampler->mFilter = filter;
		mFlagSampler->mMode[0] = mode0;
		mFlagSampler->mMode[1] = mode1;

		mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
#if defined(_OPENGL_)
		mProgram->GetPShader()->Set("baseSampler", base);
		mProgram->GetPShader()->Set("normalSampler", normal);
		mProgram->GetPShader()->Set("flagSampler", flag);
#else
		mProgram->GetPShader()->Set("baseTexture", base);
		mProgram->GetPShader()->Set("normalTexture", normal);
		mProgram->GetPShader()->Set("flagTexture", flag);
#endif
		mProgram->GetPShader()->Set("baseSampler", mBaseSampler);
		mProgram->GetPShader()->Set("normalSampler", mNormalSampler);
		mProgram->GetPShader()->Set("flagSampler", mFlagSampler);
	}
}

void InterlacedMergeEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
	mPVWMatrixConstant = pvwMatrix;
	mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void InterlacedMergeEffect::SetBaseTexture(std::shared_ptr<Texture2> const& texture)
{
	mBaseTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPShader()->Set("baseSampler", mBaseTexture);
#else
	mProgram->GetPShader()->Set("baseTexture", mBaseTexture);
#endif
}

void InterlacedMergeEffect::SetNormalTexture(std::shared_ptr<Texture2> const& texture)
{
	mNormalTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPShader()->Set("normalSampler", mNormalTexture);
#else
	mProgram->GetPShader()->Set("normalTexture", mNormalTexture);
#endif
}

void InterlacedMergeEffect::SetFlagTexture(std::shared_ptr<Texture2> const& texture)
{
	mFlagTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPShader()->Set("flagSampler", mFlagTexture);
#else
	mProgram->GetPShader()->Set("flagTexture", mFlagTexture);
#endif
}