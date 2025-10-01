// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "Logger.h"
#include "Core/Utility/StringUtil.h"

std::string Logger::mMessage;
std::mutex Logger::msMutex;
std::set<Logger::Listener*> Logger::msListeners;

Logger::Logger(char const* file, char const* function, int line, std::string const& message)
{
	msMutex.lock();
	mMessage =
		"File: " + std::string(file) + "\n" +
		"Func: " + std::string(function) + "\n" +
		"Line: " + std::to_string(line) + "\n" +
		message + "\n\n";
	msMutex.unlock();
}

Logger::Logger(char const* file, char const* function, int line, std::wstring const& message)
{
	Logger(file, function, line, ToString(message));
}

void Logger::Assertion()
{
	msMutex.lock();
	for (auto const& listener : msListeners)
	{
		if (listener->GetFlags() & Listener::LISTEN_FOR_ASSERTION)
		{
			listener->Assertion(mMessage);
		}
	}
	msMutex.unlock();
}

void Logger::Error()
{
	msMutex.lock();
	for (auto const& listener : msListeners)
	{
		if (listener->GetFlags() & Listener::LISTEN_FOR_ERROR)
		{
			listener->Error(mMessage);
		}
	}
	msMutex.unlock();
}

void Logger::Warning()
{
	msMutex.lock();
	for (auto const& listener : msListeners)
	{
		if (listener->GetFlags() & Listener::LISTEN_FOR_WARNING)
		{
			listener->Warning(mMessage);
		}
	}
	msMutex.unlock();
}

void Logger::Information()
{
	msMutex.lock();
	for (auto const& listener : msListeners)
	{
		if (listener->GetFlags() & Listener::LISTEN_FOR_INFORMATION)
		{
			listener->Information(mMessage);
		}
	}
	msMutex.unlock();
}

void Logger::Subscribe(Listener* listener)
{
	msMutex.lock();
	msListeners.insert(listener);
	msMutex.unlock();
}

void Logger::Unsubscribe(Listener* listener)
{
	msMutex.lock();
	msListeners.erase(listener);
	msMutex.unlock();
}


// Logger::Listener
Logger::Listener::~Listener()
{
}

Logger::Listener::Listener(int flags)
	:
	mFlags(flags)
{
}

int Logger::Listener::GetFlags() const
{
	return mFlags;
}

void Logger::Listener::Assertion(std::string const& message)
{
	Report("\nGE ASSERTION:\n" + message);
}

void Logger::Listener::Error(std::string const& message)
{
	Report("\nGE ERROR:\n" + message);
}

void Logger::Listener::Warning(std::string const& message)
{
	Report("\nGE WARNING:\n" + message);
}

void Logger::Listener::Information(std::string const& message)
{
	Report("\nGE INFORMATION:\n" + message);
}

void Logger::Listener::Report(std::string const&)
{
	// Stub for derived classes.
}