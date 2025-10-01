#ifndef STARSEFFECT_H
#define STARSEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Effect/VisualEffect.h"


class GRAPHIC_ITEM StarsEffect : public VisualEffect
{
public:
    // Constructionn.
    StarsEffect(std::shared_ptr<VisualProgram> const& program);

    std::shared_ptr<ConstantBuffer> const& GetStarColor() const;

    void SetStarColor(Vector4<float> const& starColor);

    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);

private:
    // Pixel shader parameters.
    std::shared_ptr<ConstantBuffer> mStarColor;
};

#endif