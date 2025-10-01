// Geometric Tools, LLC
// Copyright (c) 1998-2014
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//
// File Version: 5.0.10 (2012/11/29)

#ifndef CORESTD_H
#define CORESTD_H

//----------------------------------------------------------------------------
// Platform-specific information.  The defines to control which platform is
// included are listed below.  Add others as needed.
//
// _Windows/32 or Windows/64    :  Microsoft Windows
// __APPLE__                :  Macintosh OS X
// __linux__ or __LINUX__   :  Linux
//----------------------------------------------------------------------------
// Microsoft Windows platform
//----------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)  || defined(WIN64)

#define _WINDOWS_API_

#define WIN32_LEAN_AND_MEAN

#undef NOMINMAX
#define NOMINMAX

#ifndef __MSXML_LIBRARY_DEFINED__
#define __MSXML_LIBRARY_DEFINED__
#endif

#ifndef __XMLDOCUMENT_FWD_DEFINED__
#define __XMLDOCUMENT_FWD_DEFINED__
#endif

#include <windows.h>
#include <windowsx.h>

#define LITTLE_ENDIAN

#pragma warning(disable : 4091)

// Disable the Microsoft warnings about not using the secure functions.
#pragma warning(disable : 4996)

// The use of <libname>_ITEM to export an entire class generates warnings
// when member data and functions involving templates or inlines occur. To
// avoid the warning, <libname>_ITEM can be applied only to those items
// that really need to be exported.
#pragma warning(disable : 4251) 

// Support for standard integer types. This is only a small part of what
// stdint.h provides on Unix platforms.
#include <climits>

#ifdef _MSC_VER
    #include <eh.h>
#endif
#define NORETURN __declspec(noreturn)
#define FUNCTION_NAME __FUNCTION__

#endif
//----------------------------------------------------------------------------
// Macintosh OS X platform
//----------------------------------------------------------------------------
#if defined(__APPLE__)

#if defined(__BIG_ENDIAN__)
#define BIG_ENDIAN
#else
#define LITTLE_ENDIAN
#endif

#endif
//----------------------------------------------------------------------------
// PC Linux platform
//----------------------------------------------------------------------------
#if !defined(__LINUX__) && defined(__linux__)
// Apparently, many PC Linux flavors define __linux__, but we have used
// __LINUX__.  To avoid breaking code by replacing __LINUX__ by __linux__,
// we will just define __LINUX__.
#define __LINUX__
#endif
#if defined(__LINUX__)

// Support for standard integer types.
#include <inttypes.h>

#define LITTLE_ENDIAN

#endif
//----------------------------------------------------------------------------

// Begin Microsoft Windows DLL support.
#if defined(CORE_DLL_EXPORT)
    // For the DLL library.
    #define CORE_ITEM __declspec(dllexport)
#elif defined(CORE_DLL_IMPORT)
    // For a client of the DLL library.
    #define CORE_ITEM __declspec(dllimport)
#else
    // For the static library and for Apple/Linux.
    #define CORE_ITEM
#endif
// End Microsoft Windows DLL support.


// User-defined keywords for syntax highlighting of special class sections.
#define public_internal public
#define protected_internal protected
#define private_internal private

// Avoid warnings about unused variables.  This is designed for variables
// that are exposed in debug configurations but are hidden in release
// configurations.
#define GE_UNUSED(variable) (void)variable

// Macros to enable or disable various Assert subsystems.  TODO:  Currently,
// the extended assert system is implemented only for Microsoft Visual Studio.
#ifdef _DEBUG

    #if defined(_WINDOWS_) && defined(_MSC_VER)

		#include <crtdbg.h>

        #define USE_ASSERT
        #ifdef USE_ASSERT
            #define USE_ASSERT_WRITE_TO_OUTPUT_WINDOW
            #define USE_ASSERT_WRITE_TO_MESSAGE_BOX
        #endif
    #endif
    //#define USE_ASSERT_LOG_TO_FILE
#else
	#include <assert.h> 
#endif

#define GE_INT8_MIN  (-0x7F - 1)
#define GE_INT16_MIN (-0x7FFF - 1)
#define GE_INT32_MIN (-0x7FFFFFFF - 1)
#define GE_INT64_MIN (-0x7FFFFFFFFFFFFFFF - 1)

#define GE_INT8_MAX  0x7F
#define GE_INT16_MAX 0x7FFF
#define GE_INT32_MAX 0x7FFFFFFF
#define GE_INT64_MAX 0x7FFFFFFFFFFFFFFF

#define GE_UINT8_MAX  0xFF
#define GE_UINT16_MAX 0xFFFF
#define GE_UINT32_MAX 0xFFFFFFFF
#define GE_UINT64_MAX 0xFFFFFFFFFFFFFFFF

// Common standard library headers.
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <fstream>
#include <memory>

#include <algorithm>
#include <functional>
#include <stack>
#include <list>
#include <vector>
#include <queue>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include <array>
#include <string>

#include <optional>

#include "json.hpp"

// for convenience
using json = nlohmann::json;

// fast delegate
#include "FastDelegate.h"
using fastdelegate::MakeDelegate;

#include "tinyxml2.h"

#ifdef _WINDOWS_

#include <direct.h>
#include <io.h>
#include <tchar.h>

// Create a string with last error message
inline std::string GetErrorMessage(HRESULT hr)
{
	LPWSTR lpMsgBuf;
	DWORD bufLen = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&lpMsgBuf,
		0, NULL);
	if (bufLen)
	{
		std::wstring errorStr(lpMsgBuf);
        std::string error;

		std::transform(errorStr.begin(), errorStr.end(), std::back_inserter(error), [](wchar_t c) {
			return (char)c;});

		LocalFree(lpMsgBuf);
		return error;
	}
	else
	{
		char buf[256];
		sprintf(buf, "error message failed with %d", GetLastError());
		return std::string(buf);
	}
}

#endif

class BaseException : public std::exception
{
public:
    BaseException(const std::string& s) throw() : mString(s) {}
    ~BaseException() throw() = default;

    virtual const char* what() const throw()
    { return mString.c_str(); }
protected:
    std::string mString;
};

class AlreadyExistsException : public BaseException {
public:
    AlreadyExistsException(const std::string& s) : BaseException(s) {}
};

class VersionMismatchException : public BaseException {
public:
    VersionMismatchException(const std::string& s) : BaseException(s) {}
};

class UnknownKeycode : public BaseException
{
public:
    UnknownKeycode(const char* s) : BaseException(s) {};
};

class SettingNotFoundException : public BaseException
{
public:
    SettingNotFoundException(const std::string& s) : BaseException(s) {}
};

class InvalidPositionException : public BaseException
{
public:
    InvalidPositionException() :
        BaseException("Somebody tried to get/set something in a nonexistent position.")
    {}
    InvalidPositionException(const std::string& s) :
        BaseException(s)
    {}
};

class ItemNotFoundException : public BaseException {
public:
    ItemNotFoundException(const std::string& s) : BaseException(s) {}
};

class SerializationError : public BaseException {
public:
    SerializationError(const std::string& s) : BaseException(s) {}
};

class RNGException : public BaseException {
public:
    RNGException(const std::string& s) : BaseException(s) {}
};

class DatabaseException : public BaseException {
public:
	DatabaseException(const std::string &s): BaseException(s) {}
};


template <class BaseType, class SubType>
BaseType* GenericObjectCreationFunction(void) { return new SubType; }

template <class BaseClass, class IdType>
class GenericObjectFactory
{
	typedef BaseClass* (*ObjectCreationFunction)(void);
	std::map<IdType, ObjectCreationFunction> mCreationFunctions;

public:
	template <class SubClass>
	bool Register(IdType id)
	{
		auto findIt = mCreationFunctions.find(id);
		if (findIt == mCreationFunctions.end())
		{
			mCreationFunctions[id] =
				&GenericObjectCreationFunction<BaseClass, SubClass>;
			return true;
		}

		return false;
	}

	BaseClass* Create(IdType id)
	{
		auto findIt = mCreationFunctions.find(id);
		if (findIt != mCreationFunctions.end())
		{
			ObjectCreationFunction func = findIt->second;
			return func();
		}

		return NULL;
	}
};

#endif
