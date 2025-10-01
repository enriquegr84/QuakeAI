// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)


#include "Core/Logger/Logger.h"
#include "Core/IO/FileSystem.h"
#include "Core/IO/ResourceCache.h"
#include "Core/Utility/StringUtil.h"

#include "GLSLComputeProgram.h"
#include "GLSLProgramFactory.h"
#include "GLSLVisualProgram.h"
#include "GLSLShader.h"

std::string GLSLProgramFactory::defaultVersion = "#version 430";
std::string GLSLProgramFactory::defaultVSEntry = "main";
std::string GLSLProgramFactory::defaultPSEntry = "main";
std::string GLSLProgramFactory::defaultGSEntry = "main";
std::string GLSLProgramFactory::defaultCSEntry = "main";
unsigned int GLSLProgramFactory::defaultFlags = 0;  // unused in GLSL for now

GLSLProgramFactory::GLSLProgramFactory()
{
    version = defaultVersion;
    vsEntry = defaultVSEntry;
    psEntry = defaultPSEntry;
    gsEntry = defaultGSEntry;
    csEntry = defaultCSEntry;
    flags = defaultFlags;
}

// Create shader program after being compiled
std::shared_ptr<VisualProgram> GLSLProgramFactory::CreateFromProgram(
    std::shared_ptr<VisualProgram> const& program)
{
    const GLuint& vsHandle = std::static_pointer_cast<GLSLVisualProgram>(program)->GetVertexShaderHandle();
    const GLuint& psHandle = std::static_pointer_cast<GLSLVisualProgram>(program)->GetPixelShaderHandle();
    const GLuint& gsHandle = std::static_pointer_cast<GLSLVisualProgram>(program)->GetGeometryShaderHandle();

    GLuint programHandle = glCreateProgram();
    if (programHandle == 0)
    {
        LogError("Program creation failed.");
        return nullptr;
    }

    glAttachShader(programHandle, vsHandle);
    glAttachShader(programHandle, psHandle);
    if (gsHandle)
    {
        glAttachShader(programHandle, gsHandle);
    }

    if (!Link(programHandle))
    {
        glDetachShader(programHandle, vsHandle);
        glDeleteShader(vsHandle);
        glDetachShader(programHandle, psHandle);
        glDeleteShader(psHandle);
        if (gsHandle)
        {
            glDetachShader(programHandle, gsHandle);
            glDeleteShader(gsHandle);
        }
        glDeleteProgram(programHandle);
        return nullptr;
    }

    std::shared_ptr<GLSLVisualProgram> glslProgram = 
        std::make_shared<GLSLVisualProgram>(programHandle, vsHandle, psHandle, gsHandle);

    GLSLReflection const& reflector = glslProgram->GetReflector();
    auto vshader = std::make_shared<GLSLShader>(reflector, GE_VERTEX_SHADER, GLSLReflection::ST_VERTEX);
    auto pshader = std::make_shared<GLSLShader>(reflector, GE_PIXEL_SHADER, GLSLReflection::ST_PIXEL);
    glslProgram->SetVertexShader(vshader);
    glslProgram->SetPixelShader(pshader);
    if (gsHandle > 0)
    {
        auto gshader = std::make_shared<GLSLShader>(reflector, GE_GEOMETRY_SHADER, GLSLReflection::ST_GEOMETRY);
        glslProgram->SetGeometryShader(gshader);
    }
    return glslProgram;
}

std::shared_ptr<VisualProgram> GLSLProgramFactory::CreateFromNamedFiles(
	std::string const&, std::string const& vsFile,
	std::string const&, std::string const& psFile,
	std::string const&, std::string const& gsFile,
    ProgramDefines const& customDefines)
{
	BaseReadFile* vertexFile = FileSystem::Get()->CreateReadFile(
		ToWideString(FileSystem::Get()->GetPath(vsFile)));
	if (vertexFile == nullptr)
	{
		LogError("A program must have a vertex shader");
		delete vertexFile;
		return nullptr;
	}

	char* vsSource = new char[vertexFile->GetSize() + 1];
	memset(vsSource, 0, vertexFile->GetSize() + 1);
	vertexFile->Read(vsSource, vertexFile->GetSize());
    GLuint vsHandle = Compile(GL_VERTEX_SHADER, vsSource,
        customDefines.Get().empty() ? defines : customDefines);
	delete vertexFile;
	if (!vsHandle)
		return nullptr;

	BaseReadFile* pixelFile = FileSystem::Get()->CreateReadFile(
		ToWideString(FileSystem::Get()->GetPath(psFile)));
	if (pixelFile == nullptr)
	{
		LogError("A program must have a pixel shader.");
		delete pixelFile;
		return nullptr;
	}

	char* psSource = new char[pixelFile->GetSize() + 1];
	memset(psSource, 0, pixelFile->GetSize() + 1);
	pixelFile->Read(psSource, pixelFile->GetSize());
    GLuint psHandle = Compile(GL_FRAGMENT_SHADER, psSource,
        customDefines.Get().empty() ? defines : customDefines);
	delete pixelFile;
	if (!psHandle)
		return nullptr;

    GLuint gsHandle = 0;
	BaseReadFile* geometryFile = FileSystem::Get()->CreateReadFile(
		ToWideString(FileSystem::Get()->GetPath(gsFile)));
	if (geometryFile != nullptr)
	{
		char* gsSource = new char[geometryFile->GetSize() + 1];
		memset(gsSource, 0, geometryFile->GetSize() + 1);
		geometryFile->Read(gsSource, geometryFile->GetSize());
        gsHandle = Compile(GL_GEOMETRY_SHADER, gsSource,
            customDefines.Get().empty() ? defines : customDefines);
		delete geometryFile;
		if (!gsHandle)
			return nullptr;
	}

	GLuint programHandle = glCreateProgram();
	if (programHandle == 0)
	{
		LogError("Program creation failed.");
		return nullptr;
	}

    glAttachShader(programHandle, vsHandle);
    glAttachShader(programHandle, psHandle);
    if (gsHandle > 0)
    {
        glAttachShader(programHandle, gsHandle);
    }

    if (!Link(programHandle))
    {
        glDetachShader(programHandle, vsHandle);
        glDeleteShader(vsHandle);
        glDetachShader(programHandle, psHandle);
        glDeleteShader(psHandle);
        if (gsHandle)
        {
            glDetachShader(programHandle, gsHandle);
            glDeleteShader(gsHandle);
        }
        glDeleteProgram(programHandle);
        return nullptr;
    }

    std::shared_ptr<GLSLVisualProgram> glslProgram =
        std::make_shared<GLSLVisualProgram>(programHandle, vsHandle, psHandle, gsHandle);

    GLSLReflection const& reflector = glslProgram->GetReflector();
    auto vshader = std::make_shared<GLSLShader>(reflector, GE_VERTEX_SHADER, GLSLReflection::ST_VERTEX);
    auto pshader = std::make_shared<GLSLShader>(reflector, GE_PIXEL_SHADER, GLSLReflection::ST_PIXEL);
    glslProgram->SetVertexShader(vshader);
    glslProgram->SetPixelShader(pshader);
    if (gsHandle > 0)
    {
        auto gshader = std::make_shared<GLSLShader>(reflector, GE_GEOMETRY_SHADER, GLSLReflection::ST_GEOMETRY);
        glslProgram->SetGeometryShader(gshader);
    }
    return glslProgram;
}

std::shared_ptr<VisualProgram> GLSLProgramFactory::CreateFromNamedSources(
    std::string const&, std::string const& vsSource,
    std::string const&, std::string const& psSource,
    std::string const&, std::string const& gsSource,
    ProgramDefines const& customDefines)
{
    if (vsSource == "" || psSource == "")
    {
        LogError("A program must have a vertex shader and a pixel shader.");
        return nullptr;
    }

    GLuint vsHandle = Compile(GL_VERTEX_SHADER, vsSource,
        customDefines.Get().empty() ? defines : customDefines);
    if (vsHandle == 0)
        return nullptr;

    GLuint psHandle = Compile(GL_FRAGMENT_SHADER, psSource,
        customDefines.Get().empty() ? defines : customDefines);
    if (psHandle == 0)
        return nullptr;

    GLuint gsHandle = 0;
    if (gsSource != "")
    {
        gsHandle = Compile(GL_GEOMETRY_SHADER, gsSource,
            customDefines.Get().empty() ? defines : customDefines);
        if (gsHandle == 0)
            return nullptr;
    }

    GLuint programHandle = glCreateProgram();
    if (programHandle == 0)
    {
        LogError("Program creation failed.");
        return nullptr;
    }

    glAttachShader(programHandle, vsHandle);
    glAttachShader(programHandle, psHandle);
    if (gsHandle > 0)
    {
        glAttachShader(programHandle, gsHandle);
    }

    if (!Link(programHandle))
    {
        glDetachShader(programHandle, vsHandle);
        glDeleteShader(vsHandle);
        glDetachShader(programHandle, psHandle);
        glDeleteShader(psHandle);
        if (gsHandle)
        {
            glDetachShader(programHandle, gsHandle);
            glDeleteShader(gsHandle);
        }
        glDeleteProgram(programHandle);
        return nullptr;
    }

    std::shared_ptr<GLSLVisualProgram> glslProgram =
        std::make_shared<GLSLVisualProgram>(programHandle, vsHandle, psHandle, gsHandle);

    GLSLReflection const& reflector = glslProgram->GetReflector();
    auto vshader = std::make_shared<GLSLShader>(reflector, GE_VERTEX_SHADER, GLSLReflection::ST_VERTEX);
    auto pshader = std::make_shared<GLSLShader>(reflector, GE_PIXEL_SHADER, GLSLReflection::ST_PIXEL);
    glslProgram->SetVertexShader(vshader);
    glslProgram->SetPixelShader(pshader);
    if (gsHandle > 0)
    {
        auto gshader = std::make_shared<GLSLShader>(reflector, GE_GEOMETRY_SHADER, GLSLReflection::ST_GEOMETRY);
        glslProgram->SetGeometryShader(gshader);
    }
    return glslProgram;
}

std::shared_ptr<ComputeProgram> GLSLProgramFactory::CreateFromNamedFile(
	std::string const&, std::string const& csFile, ProgramDefines const& customDefines)
{
	BaseReadFile* shaderFile = FileSystem::Get()->CreateReadFile(ToWideString(csFile));
	if (shaderFile == nullptr)
	{
		LogError("A program must have a compute shader.");
		return nullptr;
	}

	char* csSource = new char[shaderFile->GetSize() + 1];
	memset(csSource, 0, shaderFile->GetSize() + 1);
	shaderFile->Read(csSource, shaderFile->GetSize());
	GLuint csHandle = Compile(GL_COMPUTE_SHADER, csSource,
        customDefines.Get().empty() ? defines : customDefines);
	if (csHandle == 0)
		return nullptr;

	GLuint programHandle = glCreateProgram();
	if (programHandle == 0)
	{
		LogError("Program creation failed.");
		return nullptr;
	}

	glAttachShader(programHandle, csHandle);

	if (!Link(programHandle))
	{
		glDetachShader(programHandle, csHandle);
		glDeleteShader(csHandle);
		glDeleteProgram(programHandle);
		return nullptr;
	}

    auto program = std::make_shared<GLSLComputeProgram>(programHandle, csHandle);
    GLSLReflection const& reflector = program->GetReflector();
    auto cshader = std::make_shared<GLSLShader>(reflector, GE_COMPUTE_SHADER, GLSLReflection::ST_COMPUTE);
    program->SetComputeShader(cshader);
    return program;
}

std::shared_ptr<ComputeProgram> GLSLProgramFactory::CreateFromNamedSource(
    std::string const&, std::string const& csSource, ProgramDefines const& customDefines)
{
    if (csSource == "")
    {
        LogError("A program must have a compute shader.");
        return nullptr;
    }

    GLuint csHandle = Compile(GL_COMPUTE_SHADER, csSource,
        customDefines.Get().empty() ? defines : customDefines);
    if (csHandle == 0)
        return nullptr;

    GLuint programHandle = glCreateProgram();
    if (programHandle == 0)
    {
        LogError("Program creation failed.");
        return nullptr;
    }

    glAttachShader(programHandle, csHandle);

    if (!Link(programHandle))
    {
        glDetachShader(programHandle, csHandle);
        glDeleteShader(csHandle);
        glDeleteProgram(programHandle);
        return nullptr;
    }

    auto program = std::make_shared<GLSLComputeProgram>(programHandle, csHandle);
    GLSLReflection const& reflector = program->GetReflector();
    auto cshader = std::make_shared<GLSLShader>(reflector, GE_COMPUTE_SHADER, GLSLReflection::ST_COMPUTE);
    program->SetComputeShader(cshader);
    return program;
}

GLuint GLSLProgramFactory::Compile(GLenum shaderType, 
    std::string const& source, ProgramDefines const& customDefines)
{
    GLuint handle = glCreateShader(shaderType);
    if (handle > 0)
    {
        // Prepend to the definitions
        // 1. The version of the GLSL program; for example, "#version 400".
        // 2. A define for the matrix-vector multiplication convention if
        //    it is selected as GE_USE_MAT_VEC: "define GE_USE_MAT_VEC 1"
        //    else "define GE_USE_MAT_VEC 0".
        // 3. "layout(std140, *_major) uniform;" for either row_major or column_major
        //    to select default for all uniform matrices and select std140 layout.
        // 4. "layout(std430, *_major) buffer;" for either row_major or column_major
        //    to select default for all buffer matrices and select std430 layout.
        // Append to the definitions the source-code string.
        auto const& definitions = customDefines.Get();
        std::vector<std::string> glslDefines;
        glslDefines.reserve(definitions.size() + 5);
        glslDefines.push_back(version + "\n");
#if defined(GE_USE_MAT_VEC)
        glslDefines.push_back("#define GE_USE_MAT_VEC 1\n");
#else
        glslDefines.push_back("#define GE_USE_MAT_VEC 0\n");
#endif
#if defined(GE_USE_ROW_MAJOR)
        glslDefines.push_back("layout(std140, row_major) uniform;\n");
        glslDefines.push_back("layout(std430, row_major) buffer;\n");
#else
        glslDefines.push_back("layout(std140, column_major) uniform;\n");
        glslDefines.push_back("layout(std430, column_major) buffer;\n");
#endif
        for (auto const& def : definitions)
        {
            glslDefines.push_back("#define " + def.first + " " + def.second + "\n");
        }
        glslDefines.push_back(source);

        // Repackage the definitions for glShaderSource.
        std::vector<GLchar const*> code;
        code.reserve(glslDefines.size());
        for (auto const& def : glslDefines)
        {
            code.push_back(def.c_str());
        }

        glShaderSource(handle, static_cast<GLsizei>(code.size()), &code[0], nullptr);
        glCompileShader(handle);
        GLint status;
        glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE)
        {
            GLint logLength;
            glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                std::vector<GLchar> log(logLength);
                GLsizei numWritten;
                glGetShaderInfoLog(handle, static_cast<GLsizei>(logLength), &numWritten, log.data());
                LogError("Compile failed:\n" + std::string(log.data()));
            }
            else
            {
                LogError("Invalid info log length.");
            }
            glDeleteShader(handle);
            handle = 0;
        }
    }
    else
    {
        LogError("Cannot create shader.");
    }
    return handle;
}

bool GLSLProgramFactory::Link(GLuint programHandle)
{
    glLinkProgram(programHandle);
    int status;
    glGetProgramiv(programHandle, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        int logLength;
        glGetProgramiv(programHandle, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0)
        {
            std::vector<GLchar> log(logLength);
            int numWritten;
            glGetProgramInfoLog(programHandle, logLength, &numWritten, log.data());
            LogError("Link failed:\n" + std::string(log.data()));
        }
        else
        {
            LogError("Invalid info log length.");
        }
        return false;
    }
    else
    {
        return true;
    }
}
