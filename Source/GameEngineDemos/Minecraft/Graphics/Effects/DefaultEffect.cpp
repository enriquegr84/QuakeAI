
#include "DefaultEffect.h"

DefaultEffect::DefaultEffect(std::shared_ptr<VisualProgram> const& program)
{
	mProgram = program;
	if (mProgram)
		mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
}

void DefaultEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVShader()->Set("PVWMatrix", mPVWMatrixConstant);
}