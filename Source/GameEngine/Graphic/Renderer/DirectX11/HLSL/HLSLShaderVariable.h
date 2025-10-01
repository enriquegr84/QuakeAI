// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/09/12)

#ifndef HLSLSHADERVARIABLE_H
#define HLSLSHADERVARIABLE_H

#include "HLSLResource.h"

#include <vector>

#include <fstream>

class GRAPHIC_ITEM HLSLShaderVariable
{
public:
    struct Description
    {
        Description()
            :
            name(""),
            offset(0),
            numBytes(0),
            flags(0),
            textureStart(0),
            textureNumSlots(0),
            samplerStart(0),
            samplerNumSlots(0),
            defaultValue{}
        {
        }

        std::string name;
        unsigned int offset;
        unsigned int numBytes;
        unsigned int flags;
        unsigned int textureStart;
        unsigned int textureNumSlots;
        unsigned int samplerStart;
        unsigned int samplerNumSlots;
		std::vector<unsigned char> defaultValue;
    };

    // Construction.  Shader variables are reported for constant buffers,
    // texture buffers, and structs defined in the shaders (resource
    // binding information).
    HLSLShaderVariable();

    // Deferred construction for shader reflection.  This function is
    // intended to be write-once.
    void SetDescription(D3D_SHADER_VARIABLE_DESC const& desc);

    // Member access.
    inline std::string const& HLSLShaderVariable::GetName() const
    {
        return mDesc.name;
    }

    inline unsigned int HLSLShaderVariable::GetOffset() const
    {
        return mDesc.offset;
    }

    inline unsigned int HLSLShaderVariable::GetNumBytes() const
    {
        return mDesc.numBytes;
    }

    inline unsigned int HLSLShaderVariable::GetFlags() const
    {
        return mDesc.flags;
    }

    inline unsigned int HLSLShaderVariable::GetTextureStart() const
    {
        return mDesc.textureStart;
    }

    inline unsigned int HLSLShaderVariable::GetTextureNumSlots() const
    {
        return mDesc.textureNumSlots;
    }

    inline unsigned int HLSLShaderVariable::GetSamplerStart() const
    {
        return mDesc.samplerStart;
    }

    inline unsigned int HLSLShaderVariable::GetSamplerNumSlots() const
    {
        return mDesc.samplerNumSlots;
    }

    inline std::vector<unsigned char> const& HLSLShaderVariable::GetDefaultValue() const
    {
        return mDesc.defaultValue;
    }

    // Print to a text file for human readability.
    void Print(std::ofstream& output) const;

private:
    Description mDesc;
};

#endif