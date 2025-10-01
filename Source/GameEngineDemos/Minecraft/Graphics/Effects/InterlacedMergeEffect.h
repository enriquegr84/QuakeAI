#ifndef INTERLACEDMERGEEFFECT_H
#define INTERLACEDMERGEEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Effect/VisualEffect.h"

class GRAPHIC_ITEM InterlacedMergeEffect : public VisualEffect
{
public:
    // Construction.
    InterlacedMergeEffect(std::shared_ptr<VisualProgram> const& program, 
        std::shared_ptr<Texture2> const& base, std::shared_ptr<Texture2> const& normal, 
        std::shared_ptr<Texture2> const& flag, SamplerState::Filter filter, 
        SamplerState::Mode mode0, SamplerState::Mode mode1);

    inline std::shared_ptr<Texture2> const& GetBaseTexture() const;
    inline std::shared_ptr<SamplerState> const& GetBaseSampler() const;
    inline std::shared_ptr<Texture2> const& GetNormalTexture() const;
    inline std::shared_ptr<SamplerState> const& GetNormalSampler() const;
    inline std::shared_ptr<Texture2> const& GetFlagTexture() const;
    inline std::shared_ptr<SamplerState> const& GetFlagSampler() const;

    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);

    void SetBaseTexture(std::shared_ptr<Texture2> const& texture);
    void SetNormalTexture(std::shared_ptr<Texture2> const& texture);
    void SetFlagTexture(std::shared_ptr<Texture2> const& texture);

private:

    // Pixel shader parameters.
    std::shared_ptr<Texture2> mBaseTexture;
    std::shared_ptr<SamplerState> mBaseSampler;

    std::shared_ptr<Texture2> mNormalTexture;
    std::shared_ptr<SamplerState> mNormalSampler;

    std::shared_ptr<Texture2> mFlagTexture;
    std::shared_ptr<SamplerState> mFlagSampler;
};

inline std::shared_ptr<Texture2> const& InterlacedMergeEffect::GetBaseTexture() const
{
    return mBaseTexture;
}

inline std::shared_ptr<SamplerState> const& InterlacedMergeEffect::GetBaseSampler() const
{
    return mBaseSampler;
}

inline std::shared_ptr<Texture2> const& InterlacedMergeEffect::GetNormalTexture() const
{
    return mNormalTexture;
}

inline std::shared_ptr<SamplerState> const& InterlacedMergeEffect::GetNormalSampler() const
{
    return mNormalSampler;
}

inline std::shared_ptr<Texture2> const& InterlacedMergeEffect::GetFlagTexture() const
{
    return mFlagTexture;
}

inline std::shared_ptr<SamplerState> const& InterlacedMergeEffect::GetFlagSampler() const
{
    return mFlagSampler;
}

#endif