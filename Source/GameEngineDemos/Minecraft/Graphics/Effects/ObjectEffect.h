#ifndef OBJECTEFFECT_H
#define OBJECTEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Effect/VisualEffect.h"

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2Array.h"
#include "Graphic/Effect/VisualEffect.h"

class GRAPHIC_ITEM ObjectEffect : public VisualEffect
{
public:
    // Constructionn.
    ObjectEffect(std::shared_ptr<VisualProgram> const& program, 
        std::shared_ptr<Texture2> const& texture, SamplerState::Filter filter, 
        SamplerState::Mode mode0, SamplerState::Mode mode1);

    std::shared_ptr<ConstantBuffer> const& GetEmissiveColor() const;
    std::shared_ptr<ConstantBuffer> const& GetSkyBgColor() const;
    std::shared_ptr<ConstantBuffer> const& GetFogDistance() const;

    inline std::shared_ptr<Texture2> const& GetTexture() const;
    inline std::shared_ptr<SamplerState> const& GetSampler() const;

    void SetWMatrixConstant(std::shared_ptr<ConstantBuffer> const& wMatrix);
    void SetVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& vwMatrix);
    void SetPVMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvMatrix);
    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);

    void SetEmissiveColor(Vector4<float> const& emissiveColor);
    void SetSkyBgColor(Vector4<float> const& skyBgColor);
    void SetFogDistance(float const& fogDistance);

    void SetTexture(std::shared_ptr<Texture2> const& texture);

    inline std::shared_ptr<ConstantBuffer> const& GetVWMatrixConstant() const
    {
        return mVWMatrixConstant;
    }
    inline void SetVWMatrix(Matrix4x4<float> const& vwMatrix)
    {
        *mVWMatrixConstant->Get<Matrix4x4<float>>() = vwMatrix;
    }
    inline std::shared_ptr<ConstantBuffer> const& GetPVMatrixConstant() const
    {
        return mPVMatrixConstant;
    }
    inline void SetPVMatrix(Matrix4x4<float> const& pvMatrix)
    {
        *mPVMatrixConstant->Get<Matrix4x4<float>>() = pvMatrix;
    }
    inline Matrix4x4<float> const& GetPVMatrix() const
    {
        return *mPVMatrixConstant->Get<Matrix4x4<float>>();
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
    std::shared_ptr<ConstantBuffer> mEmissiveColor;
    std::shared_ptr<ConstantBuffer> mSkyBgColor;
    std::shared_ptr<ConstantBuffer> mFogDistance;

    std::shared_ptr<Texture2> mTexture;
    std::shared_ptr<SamplerState> mSampler;

    // The constant buffer that stores the 4x4 world
    // transformation for the Visual object to which this effect is
    // attached.
    std::shared_ptr<ConstantBuffer> mWMatrixConstant;
    std::shared_ptr<ConstantBuffer> mVWMatrixConstant;
    std::shared_ptr<ConstantBuffer> mPVMatrixConstant;
};

inline std::shared_ptr<Texture2> const& ObjectEffect::GetTexture() const
{
    return mTexture;
}

inline std::shared_ptr<SamplerState> const& ObjectEffect::GetSampler() const
{
    return mSampler;
}

#endif