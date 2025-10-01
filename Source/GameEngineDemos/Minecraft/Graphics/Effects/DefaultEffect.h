#ifndef DEFAULTEFFECT_H
#define DEFAULTEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Effect/VisualEffect.h"


class GRAPHIC_ITEM DefaultEffect : public VisualEffect
{
public:
    // Construction.
    DefaultEffect(std::shared_ptr<VisualProgram> const& program);

    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);
};

#endif