#include "ObjectEffect.h"

ObjectEffect::ObjectEffect(std::shared_ptr<VisualProgram> const& program, 
	std::shared_ptr<Texture2> const& texture, SamplerState::Filter filter, 
	SamplerState::Mode mode0, SamplerState::Mode mode1)
	: mTexture(texture)
{
	mProgram = program;
	if (mProgram)
	{
		mEmissiveColor = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), true);
		mSkyBgColor = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), true);
		mFogDistance = std::make_shared<ConstantBuffer>(sizeof(float), true);

		mWMatrixConstant = std::make_shared<ConstantBuffer>(sizeof(Matrix4x4<float>), true);
		mVWMatrixConstant = std::make_shared<ConstantBuffer>(sizeof(Matrix4x4<float>), true);
		mPVMatrixConstant = std::make_shared<ConstantBuffer>(sizeof(Matrix4x4<float>), true);

		mSampler = std::make_shared<SamplerState>();
		mSampler->mFilter = filter;
		mSampler->mMode[0] = mode0;
		mSampler->mMode[1] = mode1;

		SetEmissiveColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		SetSkyBgColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		SetFogDistance(0.f);

		mProgram->GetPShader()->Set("EmissiveColor", mEmissiveColor);
		mProgram->GetPShader()->Set("SkyBgColor", mSkyBgColor);
		mProgram->GetPShader()->Set("FogDistance", mFogDistance);

		mProgram->GetVShader()->Set("WMatrix", mWMatrixConstant);
		mProgram->GetVShader()->Set("VWMatrix", mVWMatrixConstant);
		mProgram->GetVShader()->Set("PVMatrix", mPVMatrixConstant);
		//mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
#if defined(_OPENGL_)
		mProgram->GetPShader()->Set("baseSampler", texture);
#else
		mProgram->GetPShader()->Set("baseTexture", texture);
#endif
		mProgram->GetPShader()->Set("baseSampler", mSampler);
	}
}

std::shared_ptr<ConstantBuffer> const& ObjectEffect::GetEmissiveColor() const
{
	return mEmissiveColor;
}
std::shared_ptr<ConstantBuffer> const& ObjectEffect::GetSkyBgColor() const
{
	return mSkyBgColor;
}
std::shared_ptr<ConstantBuffer> const& ObjectEffect::GetFogDistance() const
{
	return mFogDistance;
}

void ObjectEffect::SetWMatrixConstant(std::shared_ptr<ConstantBuffer> const& wMatrix)
{
	mWMatrixConstant = wMatrix;
	mProgram->GetVShader()->Set("WMatrix", mWMatrixConstant);
}
void ObjectEffect::SetVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& vwMatrix)
{
	mVWMatrixConstant = vwMatrix;
	mProgram->GetVShader()->Set("VWMatrix", mVWMatrixConstant);
}
void ObjectEffect::SetPVMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvMatrix)
{
	mPVMatrixConstant = pvMatrix;
	mProgram->GetVShader()->Set("PVMatrix", mPVWMatrixConstant);
}
void ObjectEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
	mPVWMatrixConstant = pvwMatrix;
	mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void ObjectEffect::SetEmissiveColor(Vector4<float> const& emissiveColor)
{
	Vector4<float>* data = mEmissiveColor->Get<Vector4<float>>();
	*data = emissiveColor;
}
void ObjectEffect::SetSkyBgColor(Vector4<float> const& skyBgColor)
{
	Vector4<float>* data = mSkyBgColor->Get<Vector4<float>>();
	*data = skyBgColor;
}
void ObjectEffect::SetFogDistance(float const& fogDistance)
{
	float* data = mFogDistance->Get<float>();
	*data = fogDistance;
}

void ObjectEffect::SetTexture(std::shared_ptr<Texture2> const& texture)
{
	mTexture = texture;
#if defined(_OPENGL_)
	mProgram->GetPShader()->Set("baseSampler", mTexture);
#else
	mProgram->GetPShader()->Set("baseTexture", mTexture);
#endif
}
