// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "TextEffect.h"

#include "Mathematic/Algebra/Vector2.h"

TextEffect::TextEffect(std::shared_ptr<VisualProgram> const& program, std::shared_ptr<Texture2> const& texture)
{
	mProgram = program;
    if (mProgram)
    {
        mTranslate = std::make_shared<ConstantBuffer>(sizeof(Vector2<float>), true);
        mColor = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), true);
        mSamplerState = std::make_shared<SamplerState>();

        SetTranslate(0.0f, 0.0f);
        mProgram->GetVertexShader()->Set("Translate", mTranslate);

        SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        mProgram->GetPixelShader()->Set("TextColor", mColor);
#if defined(_OPENGL_)
        mProgram->GetPixelShader()->Set("baseSampler", texture);
#else
        mProgram->GetPixelShader()->Set("baseTexture", texture);
#endif
        mProgram->GetPixelShader()->Set("baseSampler", mSamplerState);
    }
}

std::shared_ptr<ConstantBuffer> const& TextEffect::GetTranslate() const
{
    return mTranslate;
}

std::shared_ptr<ConstantBuffer> const& TextEffect::GetColor() const
{
    return mColor;
}

void TextEffect::SetTranslate(float x, float  y)
{
    float* data = mTranslate->Get<float>();
    data[0] = x;
    data[1] = y;
}

void TextEffect::SetColor(Vector4<float> const& color)
{
    Vector4<float>* data = mColor->Get<Vector4<float>>();
    *data = color;
}