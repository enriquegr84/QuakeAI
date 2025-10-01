// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/13)

#include "ConstantColorEffect.h"


ConstantColorEffect::ConstantColorEffect(
	std::shared_ptr<VisualProgram> const& program, Vector4<float> const& color)
{
	mProgram = program;
	if (mProgram)
	{
		mColorConstant = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), true);
		*mColorConstant->Get<Vector4<float>>() = color;

		mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
		mProgram->GetVertexShader()->Set("ConstantColor", mColorConstant);
	}
}

void ConstantColorEffect::SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix)
{
    mPVWMatrixConstant = pvwMatrix;
    mProgram->GetVertexShader()->Set("PVWMatrix", mPVWMatrixConstant);
}