/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef PROFILER_H
#define PROFILER_H

#include "Core/OS/OS.h"

#include "Core/Threading/MutexAutolock.h"

enum ScopeProfilerType
{
    SPT_ADD,
    SPT_AVG,
    SPT_GRAPH_ADD
};


/*
	Time profiler
*/
class Profiler
{
public:
	Profiler();

	void Add(const std::string& name, float value);
	void Avg(const std::string& name, float value);
	void Clear();

	float GetValue(const std::string& name) const;
	int GetAvgCount(const std::string& name) const;
	unsigned int GetElapsedTime() const;

	typedef std::map<std::string, float> GraphValues;

	// Returns the line count
	int Print(std::ostream& o, unsigned int page = 1, unsigned int pagecount = 1);
	void GetPage(GraphValues& o, unsigned int page, unsigned int pagecount);

	void GraphAdd(const std::string& id, float value)
	{
		MutexAutoLock lock(mMutex);
		std::map<std::string, float>::iterator i = mGraphValues.find(id);
		if(i == mGraphValues.end())
			mGraphValues[id] = value;
		else
			i->second += value;
	}

	void GraphGet(GraphValues& result)
	{
		MutexAutoLock lock(mMutex);
		result = mGraphValues;
		mGraphValues.clear();
	}

	void Remove(const std::string& name)
	{
		MutexAutoLock lock(mMutex);
		mAvgCounts.erase(name);
		mData.erase(name);
	}

private:

    std::mutex mMutex;
    std::map<std::string, float> mData;
    std::map<std::string, int> mAvgCounts;
    std::map<std::string, float> mGraphValues;
	unsigned int mStartTime;
};

// Global profiler
extern Profiler* Profiling;

class ScopeProfiler
{

public:
	ScopeProfiler(Profiler* profiler, 
        const std::string& name, ScopeProfilerType type = SPT_ADD);
	~ScopeProfiler();

private:
    TimeTaker* mTimer = nullptr;
	Profiler* mProfiler = nullptr;
	std::string mName;
    ScopeProfilerType mType;
};

#endif