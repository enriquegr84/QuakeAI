// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef VISUALPROGRAM_H
#define VISUALPROGRAM_H

#include "Shader.h"

/*
	Main interface for any graphic device. HLSL uses the class as is. GLSL 
	derives from the class to store the shader and program handles.
*/
class GRAPHIC_ITEM VisualProgram
{
public:
    virtual ~VisualProgram();
    VisualProgram();

    // Member access.
    inline std::shared_ptr<Shader> const& GetVertexShader() const
    {
        return mVertexShader;
    }

    inline std::shared_ptr<Shader> const& GetPixelShader() const
    {
        return mPixelShader;
    }

    inline std::shared_ptr<Shader> const& GetGeometryShader() const
    {
        return mGeometryShader;
    }

    inline void SetVertexShader(std::shared_ptr<Shader> const& shader)
    {
        if (shader)
        {
            LogAssert(shader->GetType() == GE_VERTEX_SHADER, "The input must be a vertex shader.");
        }
        mVertexShader = shader;
    }

    inline void SetPixelShader(std::shared_ptr<Shader> const& shader)
    {
        if (shader)
        {
            LogAssert(shader->GetType() == GE_PIXEL_SHADER, "The input must be a pixel shader.");
        }
        mPixelShader = shader;
    }

    inline void SetGeometryShader(std::shared_ptr<Shader> const& shader)
    {
        if (shader)
        {
            LogAssert(shader->GetType() == GE_GEOMETRY_SHADER, "The input must be a geometry shader.");
        }
        mGeometryShader = shader;
    }

private:
	std::shared_ptr<Shader> mVertexShader;
	std::shared_ptr<Shader> mPixelShader;
	std::shared_ptr<Shader> mGeometryShader;
};

#endif
