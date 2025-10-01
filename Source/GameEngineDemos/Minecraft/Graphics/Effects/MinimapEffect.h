#ifndef MINIMAPEFFECT_H
#define MINIMAPEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Effect/VisualEffect.h"


class GRAPHIC_ITEM MinimapEffect : public VisualEffect
{
public:
    // Construction.
    MinimapEffect(std::shared_ptr<VisualProgram> const& program,
        std::shared_ptr<Texture2> const& base, std::shared_ptr<Texture2> const& normal, 
        SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1);

    std::shared_ptr<ConstantBuffer> const& GetYawVec() const;

    inline std::shared_ptr<Texture2> const& GetBaseTexture() const;
    inline std::shared_ptr<SamplerState> const& GetBaseSampler() const;
    inline std::shared_ptr<Texture2> const& GetNormalTexture() const;
    inline std::shared_ptr<SamplerState> const& GetNormalSampler() const;

    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);

    void SetYawVec(Vector3<float> const& yawVec);

    void SetBaseTexture(std::shared_ptr<Texture2> const& texture);
    void SetNormalTexture(std::shared_ptr<Texture2> const& texture);

private:
    // Pixel shader parameters.
    std::shared_ptr<ConstantBuffer> mYawVec;

    std::shared_ptr<Texture2> mBaseTexture;
    std::shared_ptr<SamplerState> mBaseSampler;

    std::shared_ptr<Texture2> mNormalTexture;
    std::shared_ptr<SamplerState> mNormalSampler;
};


inline std::shared_ptr<Texture2> const& MinimapEffect::GetBaseTexture() const
{
    return mBaseTexture;
}

inline std::shared_ptr<SamplerState> const& MinimapEffect::GetBaseSampler() const
{
    return mBaseSampler;
}

inline std::shared_ptr<Texture2> const& MinimapEffect::GetNormalTexture() const
{
    return mNormalTexture;
}

inline std::shared_ptr<SamplerState> const& MinimapEffect::GetNormalSampler() const
{
    return mNormalSampler;
}

#endif