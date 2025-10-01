/*
Minetest
Copyright (C) 2015 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "Profiler.h"

#include "Core/Logger/Logger.h"


static Profiler MainProfiler;

// The main profiler used in the app
Profiler* Profiling = &MainProfiler;

Profiler::Profiler()
{
    mStartTime = Timer::GetRealTime();
}

void Profiler::Add(const std::string& name, float value)
{
    MutexAutoLock lock(mMutex);
    {
        /* No average shall have been used; mark add used as -2 */
        std::map<std::string, int>::iterator n = mAvgCounts.find(name);
        if (n != mAvgCounts.end())
        {
            if (n->second == -1)
                n->second = -2;
            LogAssert(n->second == -2, "invalid count");
        }
        else
        {
            mAvgCounts[name] = -2;
        }
    }

    {
        std::map<std::string, float>::iterator n = mData.find(name);
        if (n == mData.end())
            mData[name] = value;
        else
            n->second += value;
    }
}

void Profiler::Avg(const std::string& name, float value)
{
    MutexAutoLock lock(mMutex);
    int &count = mAvgCounts[name];

    LogAssert(count != -2, "invalid count");
    count = std::max(count, 0) + 1;
    mData[name] += value;
}

void Profiler::Clear()
{
    MutexAutoLock lock(mMutex);
    for (auto& data : mData)
        data.second = 0;

    mAvgCounts.clear();
    mStartTime = Timer::GetRealTime();
}

float Profiler::GetValue(const std::string& name) const
{
    auto numerator = mData.find(name);
    if (numerator == mData.end())
        return 0.f;

    auto denominator = mAvgCounts.find(name);
    if (denominator != mAvgCounts.end())
        if (denominator->second >= 1)
            return numerator->second / denominator->second;

    return numerator->second;
}

int Profiler::GetAvgCount(const std::string& name) const
{
    auto n = mAvgCounts.find(name);
    if (n != mAvgCounts.end() && n->second >= 1)
        return n->second;

    return 1;
}

unsigned int Profiler::GetElapsedTime() const
{
    return Timer::GetRealTime() - mStartTime;
}

int Profiler::Print(std::ostream& o, unsigned int page, unsigned int pagecount)
{
    GraphValues values;
    GetPage(values, page, pagecount);
    char numBuf[50];

    for (auto const& value : values)
    {
        o << "  " << value.first << " ";
        if (value.second == 0)
        {
            o << std::endl;
            continue;
        }

        int space = 44 - (unsigned int)value.first.size();
        for (int j = 0; j < space; j++)
        {
            if ((j & 1) && j < space - 1)
                o << ".";
            else
                o << " ";
        }
        snprintf(numBuf, sizeof(numBuf), "% 4ix % 3g",
            GetAvgCount(value.first), value.second);
        o << numBuf << std::endl;
    }
    return (int)values.size();
}


/*
    Splits a list into "pages". For example, the list [1,2,3,4,5] split
    into two pages would be [1,2,3],[4,5]. This function computes the
    minimum and maximum indices of a single page.

    length: Length of the list that should be split
    page: Page number, 1 <= page <= pagecount
    pagecount: The number of pages, >= 1
    minindex: Receives the minimum index (inclusive).
    maxindex: Receives the maximum index (exclusive).

    Ensures 0 <= minindex <= maxindex <= length.
*/
void Paging(unsigned int length,
    unsigned int page, unsigned int pagecount,
    unsigned int& minindex, unsigned int& maxindex)
{
    if (length < 1 || pagecount < 1 || page < 1 || page > pagecount)
    {
        // Special cases or invalid parameters
        minindex = maxindex = 0;
    }
    else if (pagecount <= length)
    {
        // Less pages than entries in the list:
        // Each page contains at least one entry
        minindex = (length * (page - 1) + (pagecount - 1)) / pagecount;
        maxindex = (length * page + (pagecount - 1)) / pagecount;
    }
    else
    {
        // More pages than entries in the list:
        // Make sure the empty pages are at the end
        if (page < length)
        {
            minindex = page - 1;
            maxindex = page;
        }
        else
        {
            minindex = 0;
            maxindex = 0;
        }
    }
}

void Profiler::GetPage(GraphValues& graph, unsigned int page, unsigned int pagecount)
{
    MutexAutoLock lock(mMutex);

    unsigned int minindex, maxindex;
    Paging((unsigned int)mData.size(), page, pagecount, minindex, maxindex);

    for (auto const& data : mData)
    {
        if (maxindex == 0)
            break;
        maxindex--;

        if (minindex != 0)
        {
            minindex--;
            continue;
        }

        graph[data.first] = data.second / GetAvgCount(data.first);
    }
}


ScopeProfiler::ScopeProfiler(
    Profiler* profiler, const std::string& name, ScopeProfilerType type) :
    mProfiler(profiler), mName(name), mType(type)
{
	mName.append(" [ms]");
	if (mProfiler)
		mTimer = new TimeTaker(mName, nullptr, PRECISION_MILLI);
}

ScopeProfiler::~ScopeProfiler()
{
	if (!mTimer)
		return;

	float durationMs = (float)mTimer->Stop(true);
	float duration = durationMs;
	if (mProfiler) 
    {
		switch (mType) 
        {
		    case SPT_ADD:
			    mProfiler->Add(mName, duration);
			    break;
		    case SPT_AVG:
			    mProfiler->Avg(mName, duration);
			    break;
		    case SPT_GRAPH_ADD:
			    mProfiler->GraphAdd(mName, duration);
			    break;
		}
	}
	delete mTimer;
}
