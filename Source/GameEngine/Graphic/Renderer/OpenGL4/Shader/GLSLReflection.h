// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef GLSLREFLECTION_H
#define GLSLREFLECTION_H

#include "GameEngineStd.h"

#include "Graphic/Renderer/OpenGL4/OpenGL.h"

// Query a program object for all information relevant to manipulating the
// program at run time.

class GRAPHIC_ITEM GLSLReflection
{
public:
    // Construction.  The input is the handle to a program that was
    // successfully created for the active context.
    GLSLReflection(GLuint handle);

    enum  // Named indices for the 'referencedBy' arrays.
    {
        ST_VERTEX,
        ST_GEOMETRY,
        ST_PIXEL,
        ST_COMPUTE,
        ST_TESSCONTROL,
        ST_TESSEVALUATION
    };

    struct Input
    {
        Input()
            :
            name(""),
            type(0),
            location(0),
            arraySize(0),
            referencedBy{ 0, 0, 0, 0, 0, 0 },
            isPerPatch(0),
            locationComponent(0)
        {
        }

        std::string name;
        GLint type;
        GLint location;
        GLint arraySize;
        GLint referencedBy[6];
        GLint isPerPatch;
        GLint locationComponent;
    };

    struct Output
    {
        Output()
            :
            name(""),
            type(0),
            location(0),
            arraySize(0),
            referencedBy{ 0, 0, 0, 0, 0, 0 },
            isPerPatch(0),
            locationComponent(0),
            locationIndex(0)
        {
        }

        std::string name;
        GLint type;
        GLint location;
        GLint arraySize;
        GLint referencedBy[6];
        GLint isPerPatch;
        GLint locationComponent;
        GLint locationIndex;
    };

    struct Uniform
    {
        Uniform()
            :
            fullName(""),
            name(""),
            type(0),
            location(0),
            arraySize(0),
            offset(0),
            blockIndex(0),
            arrayStride(0),
            matrixStride(0),
            isRowMajor(0),
            atomicCounterBufferIndex(0),
            referencedBy{ 0, 0, 0, 0, 0, 0 }
        {
        }

        std::string fullName;
        std::string name;
        GLint type;
        GLint location;
        GLint arraySize;
        GLint offset;
        GLint blockIndex;
        GLint arrayStride;
        GLint matrixStride;
        GLint isRowMajor;
        GLint atomicCounterBufferIndex;
        GLint referencedBy[6];
    };

    struct DataBlock
    {
        DataBlock()
            :
            name(""),
            bufferBinding(0),
            bufferDataSize(0),
            referencedBy{ 0, 0, 0, 0, 0, 0 },
            activeVariables{}
        {
        }

        std::string name;
        GLint bufferBinding;
        GLint bufferDataSize;
        GLint referencedBy[6];
        std::vector<GLint> activeVariables;
    };

    struct AtomicCounterBuffer
    {
        AtomicCounterBuffer()
            :
            bufferBinding(0),
            bufferDataSize(0),
            referencedBy{ 0, 0, 0, 0, 0, 0 },
            activeVariables{}
        {
        }

        GLint bufferBinding;
        GLint bufferDataSize;
        GLint referencedBy[6];
        std::vector<GLint> activeVariables;
    };

    struct SubroutineUniform
    {
        SubroutineUniform()
            :
            name(""),
            location(0),
            arraySize(0),
            compatibleSubroutines{}
        {
        }

        std::string name;
        GLint location;
        GLint arraySize;
        std::vector<GLint> compatibleSubroutines;
    };

    struct BufferVariable
    {
        BufferVariable()
            :
            fullName(""),
            name(""),
            type(0),
            arraySize(0),
            offset(0),
            blockIndex(0),
            arrayStride(0),
            matrixStride(0),
            isRowMajor(0),
            topLevelArraySize(0),
            topLevelArrayStride(0),
            referencedBy{ 0, 0, 0, 0, 0, 0 }
        {
        }

        std::string fullName;
        std::string name;
        GLint type;
        GLint arraySize;
        GLint offset;
        GLint blockIndex;
        GLint arrayStride;
        GLint matrixStride;
        GLint isRowMajor;
        GLint topLevelArraySize;
        GLint topLevelArrayStride;
        GLint referencedBy[6];
    };

    struct TransformFeedbackVarying
    {
        TransformFeedbackVarying()
            :
            name(""),
            type(0),
            arraySize(0),
            offset(0),
            transformFeedbackBufferIndex(0)
        {
        }

        std::string name;
        GLint type;
        GLint arraySize;
        GLint offset;
        GLint transformFeedbackBufferIndex;
    };

    struct TransformFeedbackBuffer
    {
        TransformFeedbackBuffer()
            :
            bufferBinding(0),
            transformFeedbackBufferStride(0),
            activeVariables{}
        {
        }

        GLint bufferBinding;
        GLint transformFeedbackBufferStride;
        std::vector<GLint> activeVariables;
    };

    // Member access.
    inline GLuint GetProgramHandle() const
    {
        return mHandle;
    }

    inline std::vector<GLSLReflection::Input> const& GetInputs() const
    {
        return mInputs;
    }

    inline std::vector<GLSLReflection::Output> const& GetOutputs() const
    {
        return mOutputs;
    }

    inline std::vector<GLSLReflection::Uniform> const& GetUniforms() const
    {
        return mUniforms;
    }

    inline std::vector<GLSLReflection::DataBlock> const& GetUniformBlocks() const
    {
        return mUniformBlocks;
    }

    inline std::vector<GLSLReflection::BufferVariable> const& GetBufferVariables() const
    {
        return mBufferVariables;
    }

    inline std::vector<GLSLReflection::DataBlock> const& GetBufferBlocks() const
    {
        return mShaderStorageBlocks;
    }

    inline std::vector<GLSLReflection::AtomicCounterBuffer> const& GetAtomicCounterBuffers() const
    {
        return mAtomicCounterBuffers;
    }

    // This will not work on an instance based on a visual program.
    // This instance must correspond to a compute shader only program.
    void GetComputeShaderWorkGroupSize(GLint &numXThreads, GLint& numYThreads, GLint& numZThreads) const;

    // Print to a text file for human readability.
    void Print(std::ofstream& output) const;

private:
    void ReflectProgramInputs();
    void ReflectProgramOutputs();
    void ReflectUniforms();
    void ReflectDataBlocks(GLenum programInterface, std::vector<DataBlock>& blocks);
    void ReflectAtomicCounterBuffers();
    void ReflectSubroutines(GLenum programInterface, std::vector<std::string>& subroutines);
    void ReflectSubroutineUniforms(GLenum programInterface, std::vector<SubroutineUniform>& subUniforms);
    void ReflectBufferVariables();
    void ReflectTransformFeedbackVaryings();
    void ReflectTransformFeedbackBuffers();

    GLuint mHandle;
    std::vector<Input> mInputs;
    std::vector<Output> mOutputs;
    std::vector<Uniform> mUniforms;
    std::vector<DataBlock> mUniformBlocks;
    std::vector<DataBlock> mShaderStorageBlocks;
    std::vector<AtomicCounterBuffer> mAtomicCounterBuffers;
    std::vector<std::string> mVertexSubroutines;
    std::vector<std::string> mGeometrySubroutines;
    std::vector<std::string> mPixelSubroutines;
    std::vector<std::string> mComputeSubroutines;
    std::vector<std::string> mTessControlSubroutines;
    std::vector<std::string> mTessEvaluationSubroutines;
    std::vector<SubroutineUniform> mVertexSubroutineUniforms;
    std::vector<SubroutineUniform> mGeometrySubroutineUniforms;
    std::vector<SubroutineUniform> mPixelSubroutineUniforms;
    std::vector<SubroutineUniform> mComputeSubroutineUniforms;
    std::vector<SubroutineUniform> mTessControlSubroutineUniforms;
    std::vector<SubroutineUniform> mTessEvaluationSubroutineUniforms;
    std::vector<BufferVariable> mBufferVariables;
    std::vector<TransformFeedbackVarying> mTransformFeedbackVaryings;
    std::vector<TransformFeedbackBuffer> mTransformFeedbackBuffers;

    // Used by Print() method to mape enums to strings.
    struct EnumMap
    {
        EnumMap(GLenum inValue, std::string const& inName,
            std::string const& inShaderName, uint32_t inRows,
            uint32_t inCols, uint32_t inSize)
            :
            value(inValue),
            name(inName),
            shaderName(inShaderName),
            rows(inRows),
            cols(inCols),
            size(inSize)
        {
        }

        GLenum value;
        std::string name;
        std::string shaderName;
        unsigned rows; // use actual dim for straight vectors
        unsigned cols; // only use for cols in matrices
        unsigned size; // use 0 for opaques
    };
    static const EnumMap msEnumMap[];
    static unsigned GetEnumSize(GLenum value, GLint arraySize, GLint arrayStride, GLint matrixStride, GLint isRowMajor);
    static std::string GetEnumName(GLenum value);
    static std::string GetEnumShaderName(GLenum value);
    static std::string GetReferencedByShaderList(GLint const referencedBy[6]);

private:
    // TODO: This is a workaround for an apparent bug in the Intel
    // HD 4600 OpenGL 4.3.0 (build 10.18.15.4281 and previous).
    // Sometimes a buffer object in a compute shader is reported as
    // unreferenced when in fact it is referenced.  Remove this once
    // the bug is fixed.
    void IntelWorkaround(std::string const& name, GLint results[]);
    bool mVendorIsIntel;
    std::map<GLenum, int> mShaderTypeMap;
};

#endif