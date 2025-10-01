// Geometric Tools, LLC
// Copyright (c) 1998-2014
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//
// File Version: 5.0.2 (2011/08/13)

#include "OS.h"

#include "Core/Logger/Logger.h"

#if defined(_WINDOWS_API_) && defined(_MSC_VER) && (_MSC_VER > 1298)
	#include <stdlib.h>
	#define bswap_16(X) _byteswap_ushort(X)
	#define bswap_32(X) _byteswap_ulong(X)
#endif

unsigned int ByteSwap::Byteswap(unsigned int num) {return bswap_32(num);}
signed int ByteSwap::Byteswap(signed int num) {return bswap_32(num);}
//float ByteSwap::ByteSwap(float num) {unsigned int tmp=IR(num); tmp=bswap_32(tmp); return (FR(tmp));}
// prevent accidental byte swapping of chars
char  ByteSwap::Byteswap(char num)  {return num;}


#if defined(_WINDOWS_API_)
// ----------------------------------------------------------------
// Windows specific functions
// ----------------------------------------------------------------
#include <time.h>

static LARGE_INTEGER HighPerformanceFreq;
static BOOL HighPerformanceTimerSupport = FALSE;
static BOOL MultiCore = FALSE;


TimeTaker::TimeTaker(const std::string& name, uint64_t* result, TimePrecision prec)
{
    mName = name;
    mResult = result;
    mPrecision = prec;
    mTime = GetTime(prec);
}

uint64_t TimeTaker::Stop(bool quiet)
{
    if (mRunning)
    {
        uint64_t dTime = GetTime(mPrecision) - mTime;
        if (mResult == nullptr)
        {
            if (!quiet)
            {
                static const char* const units[] = {
                    "s"  /* PRECISION_SECONDS */,
                    "ms" /* PRECISION_MILLI */,
                    "us" /* PRECISION_MICRO */,
                    "ns" /* PRECISION_NANO */,
                };
                LogInformation(
                    mName + " took " + std::to_string(dTime) + units[mPrecision]);
            }
        }
        else
        {
            (*mResult) += dTime;
        }
        mRunning = false;
        return dTime;
    }
    return 0;
}

uint64_t TimeTaker::GetTimeElapsed()
{
    return GetTime(mPrecision) - mTime;
}


uint64_t TimeTaker::GetTime(TimePrecision prec)
{
    switch (prec)
    {
        case PRECISION_SECONDS:
            return Timer::GetTime() / 1000;
        case PRECISION_MILLI:
            return Timer::GetTime();
        case PRECISION_MICRO:
            return Timer::GetTime() * 1000;
        case PRECISION_NANO:
            return Timer::GetTime() * 1000 * 1000;
    }
    LogError("Called GetTime with invalid time precision");
    return 0;
}

void Timer::InitTimer(bool usePerformanceTimer)
{
	// workaround for hires timer on multiple core systems, bios bugs result in bad hires timers.
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	MultiCore = (sysinfo.dwNumberOfProcessors > 1);

	//performance timer determines the frequency through a call to QueryPerformanceFrequency(),
	if (usePerformanceTimer)
		HighPerformanceTimerSupport = QueryPerformanceFrequency(&HighPerformanceFreq);
	else
		HighPerformanceTimerSupport = FALSE;
	InitVirtualTimer();
}


unsigned int Timer::GetRealTime()
{
	if (HighPerformanceTimerSupport)
	{
		// Avoid potential timing inaccuracies across multiple cores by
		// temporarily setting the affinity of this process to one core.
		DWORD_PTR affinityMask=0;
		if(MultiCore)
			affinityMask = SetThreadAffinityMask(GetCurrentThread(), 1);

		// To compute the elpased time of an iteration of the game loop, it is called the
		// QueryPerformanceCounter
		LARGE_INTEGER nTime;
		BOOL queriedOK = QueryPerformanceCounter(&nTime);

		// Restore the true affinity.
		if(MultiCore)
			(void)SetThreadAffinityMask(GetCurrentThread(), affinityMask);

		if(queriedOK)
			return unsigned int((nTime.QuadPart) * 1000 / HighPerformanceFreq.QuadPart);

	}

	return GetTickCount();
}

//! Get real time and date in calendar form
Timer::RealTimeDate Timer::GetRealTimeAndDate()
{
	time_t rawtime;
	time(&rawtime);

	struct tm * timeinfo;
	timeinfo = localtime(&rawtime);

	// init with all 0 to indicate error
	Timer::RealTimeDate date={0};
	// at least Windows returns NULL on some illegal dates
	if (timeinfo)
	{
		// set useful values if succeeded
		date.Hour=(unsigned int)timeinfo->tm_hour;
		date.Minute=(unsigned int)timeinfo->tm_min;
		date.Second=(unsigned int)timeinfo->tm_sec;
		date.Day=(unsigned int)timeinfo->tm_mday;
		date.Month=(unsigned int)timeinfo->tm_mon+1;
		date.Year=(unsigned int)timeinfo->tm_year+1900;
		date.Weekday=(Timer::EWeekday)timeinfo->tm_wday;
		date.Yearday=(unsigned int)timeinfo->tm_yday+1;
		date.IsDST=timeinfo->tm_isdst != 0;
	}
	return date;
}

#endif // windows

// ------------------------------------------------------------------------
/** Returns the number of seconds since 1.1.1970. This function is used
    *  to compare access times of files, e.g. news, addons data etc.
*/
long long Timer::GetTimeSinceEpoch()
{
#ifdef WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    long long t = ft.dwHighDateTime;
    t <<= 32;
    t /= 10;
    // The Unix epoch starts on Jan 1 1970.  Need to subtract
    // the difference in seconds from Jan 1 1601.
#       if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#           define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#       else
#           define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#       endif
    t -= DELTA_EPOCH_IN_MICROSECS;

    t |= ft.dwLowDateTime;
    // Convert to seconds since epoch
    t /= 1000000UL;
    return t;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
#endif
};   // getTimeSinceEpoch

// our Randomizer is not really os specific, so we
// code one for all, which should work on every platform the same,
// which is desireable.

signed int Randomizer::seed = (int)time(0);

//! generates a pseudo random number
signed int Randomizer::Rand()
{
	// (a*seed)%m with Schrage's method
	seed = a * (seed%q) - r* (seed/q);
	if (seed<0)
		seed += m;

	return seed;
}

//! generates a pseudo random number
float Randomizer::FRand()
{
	return Rand()*(1.f/rMax);
}

signed int Randomizer::RandMax()
{
	return rMax;
}

//! resets the randomizer
void Randomizer::Reset(signed int value)
{
	seed = value;
}

PcgRandom::PcgRandom(uint64_t state, uint64_t seq)
{
    Seed(state, seq);
}

void PcgRandom::Seed(uint64_t state, uint64_t seq)
{
    mState = 0U;
    mInc = (seq << 1u) | 1u;
    Next();
    mState += state;
    Next();
}


unsigned int PcgRandom::Next()
{
    uint64_t oldstate = mState;
    mState = oldstate * 6364136223846793005ULL + mInc;

    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (unsigned int)((xorshifted >> rot) | (xorshifted << ((-rot) & 31)));
}


unsigned int PcgRandom::Range(unsigned int bound)
{
    // If the bound is 0, we cover the whole RNG's range
    if (bound == 0)
        return Next();

    /*
        This is an optimization of the expression:
          0x100000000ull % bound
        since 64-bit modulo operations typically much slower than 32.
    */
    unsigned int threshold = -bound % bound;
    unsigned int r;

    /*
        If the bound is not a multiple of the RNG's range, it may cause bias,
        e.g. a RNG has a range from 0 to 3 and we take want a number 0 to 2.
        Using rand() % 3, the number 0 would be twice as likely to appear.
        With a very large RNG range, the effect becomes less prevalent but
        still present.

        This can be solved by modifying the range of the RNG to become a
        multiple of bound by dropping values above the a threshold.

        In our example, threshold == 4 % 3 == 1, so reject values < 1
        (that is, 0), thus making the range == 3 with no bias.

        This loop may look dangerous, but will always terminate due to the
        RNG's property of uniformity.
    */
    while ((r = Next()) < threshold);

    return r % bound;
}


int PcgRandom::Range(int min, int max)
{
    if (max < min)
        throw RNGException("Invalid range (max < min)");

    // We have to cast to s64 because otherwise this could overflow,
    // and signed overflow is unodeMgrined behavior.
    unsigned int bound = (int64_t)max - (int64_t)min + 1;
    return Range(bound) + min;
}


void PcgRandom::Bytes(void *out, size_t len)
{
    unsigned char *outb = (unsigned char *)out;
    int bytesLeft = 0;
    unsigned int r;

    while (len--)
    {
        if (bytesLeft == 0)
        {
            bytesLeft = sizeof(unsigned int);
            r = Next();
        }

        *outb = r & 0xFF;
        outb++;
        bytesLeft--;
        r >>= CHAR_BIT;
    }
}


int PcgRandom::RandNormalDist(int min, int max, int numTrials)
{
    int accum = 0;
    for (int i = 0; i != numTrials; i++)
        accum += Range(min, max);
    return (int)std::round((float)accum / numTrials);
}



// ------------------------------------------------------
/*
	Timer implementation, the mStartRealTime member stores the counter value for computer 
	the total elapsed time. the member mStaticTime is used for storing subsequent calls to 
	QueryPerformanceCounter() to compute the elpased time per frame.
*/

float Timer::mVirtualTimerSpeed = 1.0f;
signed int Timer::mVirtualTimerStopCounter = 0;
unsigned int Timer::mLastVirtualTime = 0;
unsigned int Timer::mStartRealTime = 0;
unsigned int Timer::mStaticTime = 0;

//! returns current virtual time
unsigned int Timer::GetTime()
{
	if (IsStopped())
		return mLastVirtualTime;

	return mLastVirtualTime + (unsigned int)((mStaticTime - mStartRealTime) * mVirtualTimerSpeed);
}

//! ticks, advances the virtual timer, encapsulates the elpased-time calculations
void Timer::Tick()
{
	mStaticTime = GetRealTime();
}

//! sets the current virtual time
void Timer::SetTime(unsigned int time)
{
	mStaticTime = GetRealTime();
	mLastVirtualTime = time;
	mStartRealTime = mStaticTime;
}

//! stops the virtual timer
void Timer::StopTimer()
{
	if (!IsStopped())
	{
		// stop the virtual timer
		mLastVirtualTime = GetTime();
	}

	--mVirtualTimerStopCounter;
}

//! starts the virtual timer
void Timer::StartTimer()
{
	++mVirtualTimerStopCounter;

	if (!IsStopped())
	{
		// restart virtual timer
		SetTime(mLastVirtualTime);
	}
}

//! sets the speed of the virtual timer
void Timer::SetSpeed(float speed)
{
	SetTime(GetTime());

	mVirtualTimerSpeed = speed;
	if (mVirtualTimerSpeed < 0.0f)
		mVirtualTimerSpeed = 0.0f;
}

//! gets the speed of the virtual timer
float Timer::GetSpeed()
{
	return mVirtualTimerSpeed;
}

//! returns if the timer currently is stopped
bool Timer::IsStopped()
{
	return mVirtualTimerStopCounter < 0;
}

void Timer::InitVirtualTimer()
{
	mStaticTime = GetRealTime();
	mStartRealTime = mStaticTime;
}