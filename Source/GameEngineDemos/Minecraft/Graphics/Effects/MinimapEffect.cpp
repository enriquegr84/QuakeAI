#include "MinimapEffect.h"

MinimapEffect::MinimapEffect(std::shared_ptr<VisualProgram> const& program,
    std::shared_ptr<Texture2> const& base, std::shared_ptr<Texture2> const& normal,
    SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1)
    : mBaseTexture(base), mNormalTexture(normal)
{
	mProgram = program;
    if (mProgram)
    {
		mYawVec = std::make_shared<ConstantBuffer>(sizeof(Vector3<float>), true);

		mBaseSampler = std::make_shared<SamplerState>();
		mBaseSampler->mFilter = filter;
		mBaseSampler->mMode[0] = mode0;
		mBaseSampler->mMode[1] = mode1;

		mNormalSampler = std::make_shared<SamplerState>();
		mNormalSampler->mFilter = filter;
		mNormalSampler->mMode[0] = mode0;
		mNormalSampler->mMode[1] = mode1;

		SetYawVec({ 0.0f, 0.0f, 0.0f });
		mProgram->GetPixelShader()->Set("YawVec", mYawVec);
		mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
#if defined(_OPENGL_)
		mProgram->GetPixelShader()->Set("baseSampler", base);
		mProgram->GetPixelShader()->Set("normalSampler", normal);
#else
		mProgram->GetPixelShader()->Set("baseTexture", base);
		mProgram->GetPixelShader()->Set("normalTexture", normal);
#endif
		mProgram->GetPixelShader()->Set("baseSampler", mBaseSampler);
		mProgram->GetPixelShader()->Set("normalSampler", mNormalSampler);
    }
}

std::shared_ptr<ConstantBuffer> const& MinimapEffect::GetYawVec() const
{
	return mYawVec;
}

void MinimapEffect::SetYawVec(Vector3<float> const& yawVec)
{
	Vector3<float>* data = mYawVec->Get<Vector3<float>>();
	*data = yawVec;
}

void MinimapEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void MinimapEffect::SetBaseTexture(std::shared_ptr<Texture2> const& texture)
{
	mBaseTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPixelShader()->Set("baseSampler", mBaseTexture);
#else
	mProgram->GetPixelShader()->Set("baseTexture", mBaseTexture);
#endif
}

void MinimapEffect::SetNormalTexture(std::shared_ptr<Texture2> const& texture)
{
	mNormalTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPixelShader()->Set("normalSampler", mNormalTexture);
#else
	mProgram->GetPixelShader()->Set("normalTexture", mNormalTexture);
#endif
}