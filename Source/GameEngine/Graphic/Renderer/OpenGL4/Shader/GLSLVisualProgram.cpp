// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)


#include "GLSLVisualProgram.h"

GLSLVisualProgram::~GLSLVisualProgram()
{
    if (glIsProgram(mProgramHandle))
    {
        if (glIsShader(mVertexShaderHandle))
        {
            glDetachShader(mProgramHandle, mVertexShaderHandle);
            glDeleteShader(mVertexShaderHandle);
        }

        if (glIsShader(mPixelShaderHandle))
        {
            glDetachShader(mProgramHandle, mPixelShaderHandle);
            glDeleteShader(mPixelShaderHandle);
        }

        if (glIsShader(mGeometryShaderHandle))
        {
            glDetachShader(mProgramHandle, mGeometryShaderHandle);
            glDeleteShader(mGeometryShaderHandle);
        }

        glDeleteProgram(mProgramHandle);
    }
}

GLSLVisualProgram::GLSLVisualProgram(GLuint programHandle, GLuint vertexShaderHandle,
    GLuint pixedlShaderHandle, GLuint geometryShaderHandle)
    :
    mProgramHandle(programHandle),
    mVertexShaderHandle(vertexShaderHandle),
    mPixelShaderHandle(pixedlShaderHandle),
    mGeometryShaderHandle(geometryShaderHandle),
    mReflector(programHandle)
{
}
