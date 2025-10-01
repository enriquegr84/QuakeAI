// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)


#include "ProgramFactory.h"
#include <fstream>

std::shared_ptr<ProgramFactory> ProgramFactory::mProgramFactory = NULL;

std::shared_ptr<ProgramFactory> ProgramFactory::Get(void)
{
	LogAssert(ProgramFactory::mProgramFactory, "ProgramFactory doesn't exist");
	return ProgramFactory::mProgramFactory;
}

ProgramFactory::ProgramFactory()
    :
    version(""),
    vsEntry(""),
    psEntry(""),
    gsEntry(""),
    csEntry(""),
    defines(),
    flags(0)
{
	if (ProgramFactory::mProgramFactory)
	{
		LogError("Attempting to create two global program factory! \
					The old one will be destroyed and overwritten with this one.");
	}

	ProgramFactory::mProgramFactory.reset(this);
}

ProgramFactory::~ProgramFactory()
{
	if (ProgramFactory::mProgramFactory.get() == this)
		ProgramFactory::mProgramFactory = nullptr;
}

std::shared_ptr<VisualProgram> ProgramFactory::CreateFromFiles(
	std::string const& vsFile, std::string const& psFile, std::string const& gsFile, ProgramDefines const& defs)
{
    return CreateFromNamedFiles("vs", vsFile, "ps", psFile, "gs", gsFile, defs);
}

std::shared_ptr<VisualProgram> ProgramFactory::CreateFromSources(
	std::string const& vsSource, std::string const& psSource, std::string const& gsSource, ProgramDefines const& defs)
{
    return CreateFromNamedSources("vs", vsSource, "ps", psSource, "gs", gsSource, defs);
}

std::shared_ptr<ComputeProgram> ProgramFactory::CreateFromFile(std::string const& csFile, ProgramDefines const& defs)
{
    return CreateFromNamedFile("cs", csFile, defs);
}

std::shared_ptr<ComputeProgram> ProgramFactory::CreateFromSource(std::string const& csSource, ProgramDefines const& defs)
{
    return CreateFromNamedSource("cs", csSource, defs);
}

void ProgramFactory::PushDefines()
{
    mDefinesStack.push(defines);
    defines.Clear();
}

void ProgramFactory::PopDefines()
{
    if (mDefinesStack.size() > 0)
    {
        defines = mDefinesStack.top();
        mDefinesStack.pop();
    }
}

void ProgramFactory::PushFlags()
{
    mFlagsStack.push(flags);
    flags = 0;
}

void ProgramFactory::PopFlags()
{
    if (mFlagsStack.size() > 0)
    {
        flags = mFlagsStack.top();
        mFlagsStack.pop();
    }
}

