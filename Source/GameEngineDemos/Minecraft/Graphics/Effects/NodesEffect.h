#ifndef NODESEFFECT_H
#define NODESEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Effect/VisualEffect.h"


class GRAPHIC_ITEM NodesEffect : public VisualEffect
{
public:
    // Constructionn.
    NodesEffect(
        std::shared_ptr<VisualProgram> const& program, std::shared_ptr<Texture2Array> const& textures,
        SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1);

    std::shared_ptr<ConstantBuffer> const& GetDayLight() const;
    std::shared_ptr<ConstantBuffer> const& GetCameraOffset() const;
    std::shared_ptr<ConstantBuffer> const& GetAnimationTimer() const;

    std::shared_ptr<ConstantBuffer> const& GetSkyBgColor() const;
    std::shared_ptr<ConstantBuffer> const& GetFogDistance() const;

    inline std::shared_ptr<Texture2Array> const& GetTextures() const;
    inline std::shared_ptr<SamplerState> const& GetSampler() const;

    void SetWMatrixConstant(std::shared_ptr<ConstantBuffer> const& wMatrix);
    void SetVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& vwMatrix);
    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);

    void SetDayLight(Vector3<float> const& dayLight);
    void SetCameraOffset(Vector3<float> const& cameraOffset);
    void SetAnimationTimer(float const& animationTimer);

    void SetSkyBgColor(Vector4<float> const& skyBgColor);
    void SetFogDistance(float const& fogDistance);

    void SetTextures(std::shared_ptr<Texture2Array> const& textures);

    inline std::shared_ptr<ConstantBuffer> const& GetVWMatrixConstant() const
    {
        return mVWMatrixConstant;
    }
    inline void SetVWMatrix(Matrix4x4<float> const& vwMatrix)
    {
        *mVWMatrixConstant->Get<Matrix4x4<float>>() = vwMatrix;
    }
    inline Matrix4x4<float> const& GetVWMatrix() const
    {
        return *mVWMatrixConstant->Get<Matrix4x4<float>>();
    }

    inline std::shared_ptr<ConstantBuffer> const& GetWMatrixConstant() const
    {
        return mWMatrixConstant;
    }
    inline void SetWMatrix(Matrix4x4<float> const& wMatrix)
    {
        *mWMatrixConstant->Get<Matrix4x4<float>>() = wMatrix;
    }
    inline Matrix4x4<float> const& GetWMatrix() const
    {
        return *mWMatrixConstant->Get<Matrix4x4<float>>();
    }

private:
    // Pixel shader parameters.
    std::shared_ptr<ConstantBuffer> mDayLight;
    std::shared_ptr<ConstantBuffer> mCameraOffset;
    std::shared_ptr<ConstantBuffer> mAnimationTimer;

    std::shared_ptr<ConstantBuffer> mSkyBgColor;
    std::shared_ptr<ConstantBuffer> mFogDistance;

    std::shared_ptr<Texture2Array> mTextures;
    std::shared_ptr<SamplerState> mSampler;

    // The constant buffer that stores the 4x4 world
    // transformation for the Visual object to which this effect is
    // attached.
    std::shared_ptr<ConstantBuffer> mWMatrixConstant;
    std::shared_ptr<ConstantBuffer> mVWMatrixConstant;

};

inline std::shared_ptr<Texture2Array> const& NodesEffect::GetTextures() const
{
    return mTextures;
}

inline std::shared_ptr<SamplerState> const& NodesEffect::GetSampler() const
{
    return mSampler;
}

#endif