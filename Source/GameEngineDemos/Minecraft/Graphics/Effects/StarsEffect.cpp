#include "StarsEffect.h"

StarsEffect::StarsEffect(std::shared_ptr<VisualProgram> const& program)
{
    mProgram = program;
    if (mProgram)
    {
        mStarColor = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), true);

        mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
        mProgram->GetVertexShader()->Set("StarColor", mStarColor);
    }
}

void StarsEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

std::shared_ptr<ConstantBuffer> const& StarsEffect::GetStarColor() const
{
    return mStarColor;
}

void StarsEffect::SetStarColor(Vector4<float> const& starColor)
{
    Vector4<float>* data = mStarColor->Get<Vector4<float>>();
    *data = starColor;
}