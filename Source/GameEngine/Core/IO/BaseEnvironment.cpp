// Geometric Tools, LLC
// Copyright (c) 1998-2014
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//
// File Version: 5.0.0 (2010/01/01)

#include "Core/Core.h"
#include "BaseEnvironment.h"

//----------------------------------------------------------------------------
const std::wstring BaseEnvironment::GetWorkingDirectory()
{
	std::wstring wdir;
	#if defined(_WINDOWS_API_)
		wchar_t tmp[_MAX_PATH];
		_wgetcwd(tmp, _MAX_PATH);
		wdir = tmp;
		std::replace(wdir.begin(), wdir.end(), L'\\', L'/');
	#endif

	return wdir;
}

//----------------------------------------------------------------------------
std::string BaseEnvironment::GetAbsolutePath(const std::string& filename)
{
#if defined(_WINDOWS_API_)
	char tmp[_MAX_PATH];
	GetModuleFileNameA(NULL, tmp, _MAX_PATH);
	std::string dir = tmp;
	std::replace(dir.begin(), dir.end(), '\\', '/');
	dir = dir.substr(0, dir.find_last_of('/'));
	dir += filename;

	return dir;
#else
	return path(filename);
#endif
}

