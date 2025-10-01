// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/13)

#ifndef TEXTURE2ARRAYEFFECT_H
#define TEXTURE2ARRAYEFFECT_H

#include "Mathematic/Algebra/Matrix4x4.h"

#include "Graphic/Resource/Texture/Texture2Array.h"
#include "Graphic/Effect/VisualEffect.h"


class GRAPHIC_ITEM Texture2ArrayEffect : public VisualEffect
{
public:
    // Constructionn.
	Texture2ArrayEffect(
        std::shared_ptr<VisualProgram> const& program, std::shared_ptr<Texture2Array> const& textures, 
        SamplerState::Filter filter, SamplerState::Mode mode0, SamplerState::Mode mode1);

    inline std::shared_ptr<Texture2Array> const& GetTextures() const;
    inline std::shared_ptr<SamplerState> const& GetSampler() const;

    void SetPVWMatrixConstant(std::shared_ptr<ConstantBuffer> const& pvwMatrix);
	void SetTextures(std::shared_ptr<Texture2Array> const& textures);

private:

    // Pixel shader parameters.
	std::shared_ptr<Texture2Array> mTextures;
	std::shared_ptr<SamplerState> mSampler;
};

inline std::shared_ptr<Texture2Array> const& Texture2ArrayEffect::GetTextures() const
{
    return mTextures;
}

inline std::shared_ptr<SamplerState> const& Texture2ArrayEffect::GetSampler() const
{
    return mSampler;
}

#endif
