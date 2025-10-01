// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef GLSLCOMPUTEPROGRAM_H
#define GLSLCOMPUTEPROGRAM_H

#include "Graphic/Shader/ComputeProgram.h"
#include "GLSLReflection.h"

class GRAPHIC_ITEM GLSLComputeProgram : public ComputeProgram
{
public:
    // Construction and destruction.
    virtual ~GLSLComputeProgram();
    GLSLComputeProgram(GLuint programHandle, GLuint cshaderHandle);

    // Member access.  GLEngine needs the program handle for enabling and
    // disabling the program.  TODO: Do we need the GetCShaderHandle
    // function?
    inline GLuint GetProgramHandle() const
    {
        return mProgramHandle;
    }

    inline GLuint GetComputeShaderHandle() const
    {
        return mComputeShaderHandle;
    }

    inline GLSLReflection const& GetReflector() const
    {
        return mReflector;
    }

private:
    GLuint mProgramHandle;
    GLuint mComputeShaderHandle;
    GLSLReflection mReflector;
};

#endif