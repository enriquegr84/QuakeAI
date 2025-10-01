// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/09/12)

#ifndef HLSLSHADER_H
#define HLSLSHADER_H

#include "Graphic/GraphicStd.h"

#include "Graphic/Shader/Shader.h"

#include "HLSLReflection.h"

class GRAPHIC_ITEM HLSLShader : public Shader
{
public:
    HLSLShader(HLSLReflection const& reflector, GraphicObjectType type);

    virtual void Set(std::string const& textureName,
        std::shared_ptr<TextureSingle> const& texture,
        std::string const& samplerName,
        std::shared_ptr<SamplerState> const& state) override;

    virtual void Set(std::string const& textureName,
        std::shared_ptr<TextureArray> const& texture,
        std::string const& samplerName,
        std::shared_ptr<SamplerState> const& state) override;

    virtual bool IsValid(Data const& goal, ConstantBuffer* resource) const override;
    virtual bool IsValid(Data const& goal, TextureBuffer* resource) const override;
    virtual bool IsValid(Data const& goal, StructuredBuffer* resource) const override;
    virtual bool IsValid(Data const& goal, RawBuffer* resource) const override;
    virtual bool IsValid(Data const& goal, TextureSingle* resource) const override;
    virtual bool IsValid(Data const& goal, TextureArray* resource) const override;
    virtual bool IsValid(Data const& goal, SamplerState* state) const override;
};

#endif