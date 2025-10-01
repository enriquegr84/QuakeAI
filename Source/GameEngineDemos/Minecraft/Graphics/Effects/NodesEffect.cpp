// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/13)

#include "NodesEffect.h"

NodesEffect::NodesEffect(
    std::shared_ptr<VisualProgram> const& program, std::shared_ptr<Texture2Array> const& textures,
    SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1)
{
    mProgram = program;
    if (mProgram)
    {
        mSkyBgColor = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), true);
        mFogDistance = std::make_shared<ConstantBuffer>(sizeof(float), true);
        mDayLight = std::make_shared<ConstantBuffer>(sizeof(Vector3<float>), true);
        mCameraOffset = std::make_shared<ConstantBuffer>(sizeof(Vector3<float>), true);
        mAnimationTimer = std::make_shared<ConstantBuffer>(sizeof(float), true);

        mWMatrixConstant = std::make_shared<ConstantBuffer>(sizeof(Matrix4x4<float>), true);
        mVWMatrixConstant = std::make_shared<ConstantBuffer>(sizeof(Matrix4x4<float>), true);

        mSampler = std::make_shared<SamplerState>();
        mSampler->mFilter = filter;
        mSampler->mMode[0] = mode0;
        mSampler->mMode[1] = mode1;

        mTextures = textures;

        SetSkyBgColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        SetFogDistance(0.f);

        SetDayLight({ 0.0f, 0.0f, 0.0f });
        SetCameraOffset({ 0.0f, 0.0f, 0.0f });
        SetAnimationTimer(0.f);

        mProgram->GetPShader()->Set("SkyBgColor", mSkyBgColor);
        mProgram->GetPShader()->Set("FogDistance", mFogDistance);

        mProgram->GetVShader()->Set("DayLight", mDayLight);
        mProgram->GetVShader()->Set("CameraOffset", mCameraOffset);
        if (mProgram->GetVShader()->Get("AnimationTimer") != -1)
            mProgram->GetVShader()->Set("AnimationTimer", mAnimationTimer);

        mProgram->GetVShader()->Set("WMatrix", mWMatrixConstant);
        mProgram->GetVShader()->Set("VWMatrix", mVWMatrixConstant);
        mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);

#if defined(_OPENGL_)
        mProgram->GetPShader()->Set("baseSampler", mTextures);
        mProgram->GetPShader()->Set("baseSampler", mSampler);
#else
        mProgram->GetPShader()->Set("baseTextureArray", mTextures);
#endif

#if defined(_OPENGL_)
        mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
#else
        mProgram->GetPShader()->Set("baseSampler", mSampler);
        mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
#endif
    }
}

std::shared_ptr<ConstantBuffer> const& NodesEffect::GetDayLight() const
{
    return mDayLight;
}
std::shared_ptr<ConstantBuffer> const& NodesEffect::GetCameraOffset() const
{
    return mCameraOffset;
}
std::shared_ptr<ConstantBuffer> const& NodesEffect::GetAnimationTimer() const
{
    return mAnimationTimer;
}

std::shared_ptr<ConstantBuffer> const& NodesEffect::GetSkyBgColor() const
{
    return mSkyBgColor;
}
std::shared_ptr<ConstantBuffer> const& NodesEffect::GetFogDistance() const
{
    return mFogDistance;
}

void NodesEffect::SetDayLight(Vector3<float> const& dayLight)
{
    Vector3<float>* data = mDayLight->Get<Vector3<float>>();
    *data = dayLight;
}
void NodesEffect::SetCameraOffset(Vector3<float> const& cameraOffset)
{
    Vector3<float>* data = mCameraOffset->Get<Vector3<float>>();
    *data = cameraOffset;
}
void NodesEffect::SetAnimationTimer(float const& animationTimer)
{
    float* data = mAnimationTimer->Get<float>();
    *data = animationTimer;
}

void NodesEffect::SetSkyBgColor(Vector4<float> const& skyBgColor)
{
    Vector4<float>* data = mSkyBgColor->Get<Vector4<float>>();
    *data = skyBgColor;
}
void NodesEffect::SetFogDistance(float const& fogDistance)
{
    float* data = mFogDistance->Get<float>();
    *data = fogDistance;
}

void NodesEffect::SetWMatrixConstant(std::shared_ptr<ConstantBuffer> const& wMatrix)
{
    mWMatrixConstant = wMatrix;
    mProgram->GetVShader()->Set("WMatrix", mWMatrixConstant);
}
void NodesEffect::SetVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& vwMatrix)
{
    mVWMatrixConstant = vwMatrix;
    mProgram->GetVShader()->Set("VWMatrix", mVWMatrixConstant);
}
void NodesEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void NodesEffect::SetTextures(std::shared_ptr<Texture2Array> const& textures)
{
    mTextures = textures;
#if defined(_OPENGL_)
    mProgram->GetPShader()->Set("baseSampler", mTextures);
    mProgram->GetPShader()->Set("baseSampler", mSampler);
#else
    mProgram->GetPShader()->Set("baseTextureArray", mTextures);
#endif

}