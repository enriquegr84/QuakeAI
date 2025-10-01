// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/09/12)

#ifndef HLSLSHADERTYPE_H
#define HLSLSHADERTYPE_H

#include "HLSLResource.h"

#include <vector>
#include <fstream>

class GRAPHIC_ITEM HLSLShaderType
{
public:
    struct Description
    {
        Description()
            :
            varClass(D3D_SVC_SCALAR),
            varType(D3D_SVT_VOID),
            numRows(0),
            numColumns(0),
            numElements(0),
            numChildren(0),
            offset(0),
            typeName("")
        {
        }

        D3D_SHADER_VARIABLE_CLASS varClass;
        D3D_SHADER_VARIABLE_TYPE varType;
        unsigned int numRows;
        unsigned int numColumns;
        unsigned int numElements;
        unsigned int numChildren;
        unsigned int offset;
        std::string typeName;
    };

    // Construction.
    HLSLShaderType();

    // Deferred construction for shader reflection.  These functions are
    // intended to be write-once.
    void SetDescription(D3D_SHADER_TYPE_DESC const& desc);
    inline void SetName(std::string const& name)
    {
        mName = name;
    }

    // This is non-const and is intended to be used as part of the Set*
    // write-once system.  HLSLShaderFactory::{GetVariables,GetTypes} are
    // the clients and they ensure that i is a valid index.
    HLSLShaderType& GetChild(unsigned int i);

    // For use in construction of lookup tables for name-offset pairs.
    HLSLShaderType const& GetChild(unsigned int i) const;

    // Member access.
    inline std::string const& GetName() const
    {
        return mName;
    }

    inline D3D_SHADER_VARIABLE_CLASS GetClass() const
    {
        return mDesc.varClass;
    }

    inline D3D_SHADER_VARIABLE_TYPE GetType() const
    {
        return mDesc.varType;
    }

    inline unsigned int GetNumRows() const
    {
        return mDesc.numRows;
    }

    inline unsigned int GetNumColumns() const
    {
        return mDesc.numColumns;
    }

    inline unsigned int GetNumElements() const
    {
        return mDesc.numElements;
    }

    inline unsigned int GetNumChildren() const
    {
        return mDesc.numChildren;
    }

    inline unsigned int GetOffset() const
    {
        return mDesc.offset;
    }

    inline std::string const& GetTypeName() const
    {
        return mDesc.typeName;
    }

    inline std::vector<HLSLShaderType> const& GetChildren() const
    {
        return mChildren;
    }

    // Print to a text file for human readability.
    void Print(std::ofstream& output, int indent) const;

private:
    Description mDesc;
	std::string mName;
	std::vector<HLSLShaderType> mChildren;

    // Support for Print.
    static std::string const msVarClass[];
    static std::string const msVarType[];
};

#endif