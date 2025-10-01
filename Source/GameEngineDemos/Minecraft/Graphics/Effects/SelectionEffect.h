#ifndef SELECTIONEFFECT_H
#define SELECTIONEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Effect/VisualEffect.h"


class GRAPHIC_ITEM SelectionEffect : public VisualEffect
{
public:
    // Constructionn.
    SelectionEffect(std::shared_ptr<VisualProgram> const& program, 
        std::shared_ptr<Texture2> const& texture, SamplerState::Filter filter, 
        SamplerState::Mode mode0, SamplerState::Mode mode1);

    inline std::shared_ptr<Texture2> const& GetTexture() const;
    inline std::shared_ptr<SamplerState> const& GetSampler() const;

    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);
    void SetTexture(std::shared_ptr<Texture2> const& texture);

private:

    // Pixel shader parameters.
    std::shared_ptr<Texture2> mTexture;
    std::shared_ptr<SamplerState> mSampler;

};

inline std::shared_ptr<Texture2> const& SelectionEffect::GetTexture() const
{
    return mTexture;
}

inline std::shared_ptr<SamplerState> const& SelectionEffect::GetSampler() const
{
    return mSampler;
}

#endif