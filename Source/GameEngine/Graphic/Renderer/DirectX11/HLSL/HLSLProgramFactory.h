// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef HLSLPROGRAMFACTORY_H
#define HLSLPROGRAMFACTORY_H

#include "Graphic/Shader/ProgramFactory.h"

class GRAPHIC_ITEM HLSLProgramFactory : public ProgramFactory
{
public:
    // The 'defaultVersion' can be set once on application initialization if
    // you want an HLSL version different from our default when constructing a
    // program factory.
    static std::string defaultVersion;  // "5_0" (Shader Model 5)
    static std::string defaultVSEntry;  // "VSMain"
    static std::string defaultPSEntry;  // "PSMain"
    static std::string defaultGSEntry;  // "GSMain"
    static std::string defaultCSEntry;  // "CSMain"
    static unsigned int defaultFlags;
        // enable strictness, ieee strictness, optimization level 3

    // Construction.  The 'version' member is set to 'defaultVersion'.  The
    // 'defines' are empty.
    virtual ~HLSLProgramFactory();
    HLSLProgramFactory();

    // The returned value is used as a lookup index into arrays of strings
    // corresponding to shader programs.
    virtual int GetAPI() const;

    // Create new shader program after being compiled
    virtual std::shared_ptr<VisualProgram> CreateFromProgram(
        std::shared_ptr<VisualProgram> const& program) override;

    // Create a program for GPU display.
	std::shared_ptr<VisualProgram> CreateFromByteCode(
		std::vector<unsigned char> const& vsBytecode,
		std::vector<unsigned char> const& psBytecode,
		std::vector<unsigned char> const& gsBytecode);

    // Create a program for GPU computing.
	std::shared_ptr<ComputeProgram> CreateFromByteCode(
		std::vector<unsigned char> const& csBytecode);

    // HLSLVisualProgram and HLSLComputeProgram objects are responsible
    // for destroying the shaders and program.  The factory wraps the
    // program objects as shared pointers to allow automatic clean-up.

private:

	// Create a program for GPU display. The files are
	// passed as parameters in case the shader compiler needs
	// this for #include path searches.
	virtual std::shared_ptr<VisualProgram> CreateFromNamedFiles(
		std::string const& vsName, std::string const& vsFile,
		std::string const& psName, std::string const& psFile,
		std::string const& gsName, std::string const& gsFile, 
        ProgramDefines const& customDefines);

    // Create a program for GPU display.  The files are loaded, converted to
    // strings, and passed to CreateFromNamedSources.  The filenames are
    // passed as the 'xsName' parameters in case the shader compiler needs
    // this for #include path searches.
    virtual std::shared_ptr<VisualProgram> CreateFromNamedSources(
		std::string const& vsName, std::string const& vsSource,
		std::string const& psName, std::string const& psSource,
		std::string const& gsName, std::string const& gsSource,
        ProgramDefines const& customDefines);

    // Create a program for GPU computing. The filename is passed as parameters 
	// in case the shader compiler needs this for #include path searches.
    virtual std::shared_ptr<ComputeProgram> CreateFromNamedFile(
		std::string const& csName, std::string const& csFile,
        ProgramDefines const& customDefines);

	// Create a program for GPU computing. The file is loaded, converted to
	// a string, and passed to CreateFromNamedSource. The filename is passed
	// as the 'csName' parameters in case the shader compiler needs this for
	// #include path searches.
	virtual std::shared_ptr<ComputeProgram> CreateFromNamedSource(
		std::string const& csName, std::string const& csSource,
        ProgramDefines const& customDefines);
};

#endif