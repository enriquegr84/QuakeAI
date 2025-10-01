// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/09/12)

#include "Core/Logger/Logger.h"

#include "HLSLComputeProgram.h"
#include "HLSLProgramFactory.h"
#include "HLSLShaderFactory.h"
#include "HLSLVisualProgram.h"
#include "HLSLShader.h"

std::string HLSLProgramFactory::defaultVersion = "5_0";
std::string HLSLProgramFactory::defaultVSEntry = "VSMain";
std::string HLSLProgramFactory::defaultPSEntry = "PSMain";
std::string HLSLProgramFactory::defaultGSEntry = "GSMain";
std::string HLSLProgramFactory::defaultCSEntry = "CSMain";
unsigned int HLSLProgramFactory::defaultFlags = (
	D3DCOMPILE_DEBUG |
	D3DCOMPILE_SKIP_OPTIMIZATION |
    D3DCOMPILE_ENABLE_STRICTNESS |
    D3DCOMPILE_IEEE_STRICTNESS |
    D3DCOMPILE_OPTIMIZATION_LEVEL3);

HLSLProgramFactory::~HLSLProgramFactory()
{
}

HLSLProgramFactory::HLSLProgramFactory()
{
    version = defaultVersion;
    vsEntry = defaultVSEntry;
    psEntry = defaultPSEntry;
    gsEntry = defaultGSEntry;
    csEntry = defaultCSEntry;
    flags = defaultFlags;
}

int HLSLProgramFactory::GetAPI() const
{
    return PF_HLSL;
}

// Create shader program after being compiled
std::shared_ptr<VisualProgram> HLSLProgramFactory::CreateFromProgram(
    std::shared_ptr<VisualProgram> const& program)
{
    std::shared_ptr<HLSLVisualProgram> visualProgram = std::dynamic_pointer_cast<HLSLVisualProgram>(program);
    LogAssert(visualProgram, "incorrect program");

    std::shared_ptr<HLSLShader> vshader;
    std::shared_ptr<HLSLShader> pshader;
    std::shared_ptr<HLSLShader> gshader;

    const HLSLReflection& hlslVShader = visualProgram->GetVertexShaderHandle();
    if (hlslVShader.IsValid())
    {
        vshader = std::make_shared<HLSLShader>(hlslVShader, GE_VERTEX_SHADER);
    }
    else
    {
        return nullptr;
    }

    const HLSLReflection& hlslPShader = visualProgram->GetPixelShaderHandle();
    if (hlslPShader.IsValid())
    {
        pshader = std::make_shared<HLSLShader>(hlslPShader, GE_PIXEL_SHADER);
    }
    else
    {
        return nullptr;
    }

    const HLSLReflection& hlslGShader = visualProgram->GetGeometryShaderHandle();
    if (hlslGShader.IsValid())
    {
        gshader = std::make_shared<HLSLShader>(hlslGShader, GE_GEOMETRY_SHADER);
    }

    std::shared_ptr<HLSLVisualProgram> hlslProgram = 
        std::make_shared<HLSLVisualProgram>(hlslVShader, hlslPShader, hlslGShader);
    hlslProgram->SetVertexShader(vshader);
    hlslProgram->SetPixelShader(pshader);
    hlslProgram->SetGeometryShader(gshader);
    return hlslProgram;
}

std::shared_ptr<VisualProgram> HLSLProgramFactory::CreateFromByteCode(
	std::vector<unsigned char> const& vsBytecode,
	std::vector<unsigned char> const& psBytecode,
	std::vector<unsigned char> const& gsBytecode)
{
    if (vsBytecode.size() == 0 || psBytecode.size() == 0)
    {
        LogError("A program must have a vertex shader and a pixel shader.");
        return nullptr;
    }

	std::shared_ptr<HLSLShader> vshader;
	std::shared_ptr<HLSLShader> pshader;
	std::shared_ptr<HLSLShader> gshader;

    HLSLReflection hlslVShader = HLSLShaderFactory::CreateFromByteCode("vs",
        vsEntry, std::string("vs_") + version, vsBytecode.size(), vsBytecode.data());
    if (hlslVShader.IsValid())
    {
        vshader = std::make_shared<HLSLShader>(hlslVShader, GE_VERTEX_SHADER);
    }
    else
    {
        return nullptr;
    }

    HLSLReflection hlslPShader = HLSLShaderFactory::CreateFromByteCode("ps",
        psEntry, std::string("ps_") + version, psBytecode.size(), psBytecode.data());
    if (hlslPShader.IsValid())
    {
        pshader = std::make_shared<HLSLShader>(hlslPShader, GE_PIXEL_SHADER);
    }
    else
    {
        return nullptr;
    }

    HLSLReflection hlslGShader;
    if (gsBytecode.size() > 0)
    {
        hlslGShader = HLSLShaderFactory::CreateFromByteCode("gs",
            gsEntry, std::string("gs_") + version, gsBytecode.size(), gsBytecode.data());
        if (hlslGShader.IsValid())
        {
            gshader = std::make_shared<HLSLShader>(hlslGShader, GE_GEOMETRY_SHADER);
        }
        else
        {
            return nullptr;
        }
    }

    std::shared_ptr<HLSLVisualProgram> hlslProgram =
        std::make_shared<HLSLVisualProgram>(hlslVShader, hlslPShader, hlslGShader);
    hlslProgram->SetVertexShader(vshader);
    hlslProgram->SetPixelShader(pshader);
    hlslProgram->SetGeometryShader(gshader);
    return hlslProgram;
}

std::shared_ptr<ComputeProgram> HLSLProgramFactory::CreateFromNamedFile(
	std::string const& csName, std::string const& csFile, ProgramDefines const& customDefines)
{
	if (csFile == "")
	{
		LogError("A program must have a compute shader.");
		return nullptr;
	}

    HLSLReflection hlslCShader = HLSLShaderFactory::CreateFromFile(
        csName, csFile, csEntry, std::string("cs_") + version, 
        customDefines.Get().empty() ? defines : customDefines, flags);
	if (hlslCShader.IsValid())
	{
        auto cshader = std::make_shared<HLSLShader>(hlslCShader, GE_COMPUTE_SHADER);
        auto program = std::make_shared<HLSLComputeProgram>();
        program->SetComputeShader(cshader);
		return program;
	}
	else
	{
		return nullptr;
	}
}

std::shared_ptr<ComputeProgram> HLSLProgramFactory::CreateFromNamedSource(
	std::string const& csName, std::string const& csSource, ProgramDefines const& customDefines)
{
    if (csSource == "")
    {
        LogError("A program must have a compute shader.");
        return nullptr;
    }

    HLSLReflection hlslCShader = HLSLShaderFactory::CreateFromString(
        csName, csSource, csEntry, std::string("cs_") + version, 
        customDefines.Get().empty() ? defines : customDefines, flags);
    if (hlslCShader.IsValid())
    {
        auto cshader = std::make_shared<HLSLShader>(hlslCShader, GE_COMPUTE_SHADER);
        auto program = std::make_shared<HLSLComputeProgram>();
        program->SetComputeShader(cshader);
        return program;
    }
    else
    {
        return nullptr;
    }
}

std::shared_ptr<VisualProgram> HLSLProgramFactory::CreateFromNamedFiles(
	std::string const& vsName, std::string const& vsFile,
	std::string const& psName, std::string const& psFile,
	std::string const& gsName, std::string const& gsFile,
    ProgramDefines const& customDefines)
{
	if (vsFile == "" || psFile == "")
	{
		LogError("A program must have a vertex shader and a pixel shader.");
		return nullptr;
	}

    std::shared_ptr<HLSLShader> vshader;
    std::shared_ptr<HLSLShader> pshader;
    std::shared_ptr<HLSLShader> gshader;

    HLSLReflection hlslVShader = HLSLShaderFactory::CreateFromFile(
        vsName, vsFile, vsEntry, std::string("vs_") + version,
        customDefines.Get().empty() ? defines : customDefines, flags);
    if (hlslVShader.IsValid())
    {
        vshader = std::make_shared<HLSLShader>(hlslVShader, GE_VERTEX_SHADER);
    }
    else
    {
        return nullptr;
    }

    HLSLReflection hlslPShader = HLSLShaderFactory::CreateFromFile(
        psName, psFile, psEntry, std::string("ps_") + version,
        customDefines.Get().empty() ? defines : customDefines, flags);
    if (hlslPShader.IsValid())
    {
        pshader = std::make_shared<HLSLShader>(hlslPShader, GE_PIXEL_SHADER);
    }
    else
    {
        return nullptr;
    }

    HLSLReflection hlslGShader;
    if (gsFile != "")
    {
        hlslGShader = HLSLShaderFactory::CreateFromFile(
            gsName, gsFile, gsEntry, std::string("gs_") + version,
            customDefines.Get().empty() ? defines : customDefines, flags);
        if (hlslGShader.IsValid())
        {
            gshader = std::make_shared<HLSLShader>(hlslGShader, GE_GEOMETRY_SHADER);
        }
        else
        {
            return nullptr;
        }
    }

    std::shared_ptr<HLSLVisualProgram> hlslProgram =
        std::make_shared<HLSLVisualProgram>(hlslVShader, hlslPShader, hlslGShader);
    hlslProgram->SetVertexShader(vshader);
    hlslProgram->SetPixelShader(pshader);
    hlslProgram->SetGeometryShader(gshader);
    return hlslProgram;
}

std::shared_ptr<VisualProgram> HLSLProgramFactory::CreateFromNamedSources(
	std::string const& vsName, std::string const& vsSource,
	std::string const& psName, std::string const& psSource,
	std::string const& gsName, std::string const& gsSource,
    ProgramDefines const& customDefines)
{
    if (vsSource == "" || psSource == "")
    {
        LogError("A program must have a vertex shader and a pixel shader.");
        return nullptr;
    }

    std::shared_ptr<HLSLShader> vshader;
    std::shared_ptr<HLSLShader> pshader;
    std::shared_ptr<HLSLShader> gshader;

    HLSLReflection hlslVShader = HLSLShaderFactory::CreateFromString(
        vsName, vsSource, vsEntry, std::string("vs_") +  version, 
        customDefines.Get().empty() ? defines : customDefines, flags);
    if (hlslVShader.IsValid())
    {
        vshader = std::make_shared<HLSLShader>(hlslVShader, GE_VERTEX_SHADER);
    }
    else
    {
        return nullptr;
    }

    HLSLReflection hlslPShader = HLSLShaderFactory::CreateFromString(
        psName, psSource, psEntry, std::string("ps_") + version, 
        customDefines.Get().empty() ? defines : customDefines, flags);
    if (hlslPShader.IsValid())
    {
        pshader = std::make_shared<HLSLShader>(hlslPShader, GE_PIXEL_SHADER);
    }
    else
    {
        return nullptr;
    }

    HLSLReflection hlslGShader;
    if (gsSource != "")
    {
        hlslGShader = HLSLShaderFactory::CreateFromString(
            gsName, gsSource, gsEntry, std::string("gs_") + version, 
            customDefines.Get().empty() ? defines : customDefines, flags);
        if (hlslGShader.IsValid())
        {
            gshader = std::make_shared<HLSLShader>(hlslGShader, GE_GEOMETRY_SHADER);
        }
        else
        {
            return nullptr;
        }
    }

    std::shared_ptr<HLSLVisualProgram> hlslProgram =
        std::make_shared<HLSLVisualProgram>(hlslVShader, hlslPShader, hlslGShader);
    hlslProgram->SetVertexShader(vshader);
    hlslProgram->SetPixelShader(pshader);
    hlslProgram->SetGeometryShader(gshader);
    return hlslProgram;
}

std::shared_ptr<ComputeProgram> HLSLProgramFactory::CreateFromByteCode(
	std::vector<unsigned char> const& csBytecode)
{
    if (csBytecode.size() == 0)
    {
        LogError("A program must have a compute shader.");
        return nullptr;
    }

    HLSLReflection hlslCShader = HLSLShaderFactory::CreateFromByteCode("cs",
        csEntry, std::string("cs_") + version, csBytecode.size(), csBytecode.data());
    if (hlslCShader.IsValid())
    {
        auto cshader = std::make_shared<HLSLShader>(hlslCShader, GE_COMPUTE_SHADER);
        auto program = std::make_shared<HLSLComputeProgram>();
        program->SetComputeShader(cshader);
        return program;
    }
    else
    {
        return nullptr;
    }
}
