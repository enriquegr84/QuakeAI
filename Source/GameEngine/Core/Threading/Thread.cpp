/*
This file is a part of the JThread package, which contains some object-
oriented thread wrappers for different thread implementations.

Copyright (c) 2000-2006  Jori Liesenborgs (jori.liesenborgs@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "Thread.h"
#include "MutexAutolock.h"

#include "Core/Logger/Logger.h"

// for setName
#if defined(__linux__)
	#include <sys/prctl.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
	#include <pthread_np.h>
#elif defined(__NetBSD__)
	#include <sched.h>
#elif defined(_MSC_VER)
	struct THREADNAME_INFO {
		DWORD dwType;     // Must be 0x1000
		LPCSTR szName;    // Pointer to name (in user addr space)
		DWORD dwThreadID; // Thread ID (-1=caller thread)
		DWORD dwFlags;    // Reserved for future use, must be zero
	};
#endif

// for bindToProcessor
#if __FreeBSD_version >= 702106
	typedef cpuset_t cpu_set_t;
#elif defined(__sun) || defined(sun)
	#include <sys/types.h>
	#include <sys/processor.h>
	#include <sys/procset.h>
#elif defined(_AIX)
	#include <sys/processor.h>
	#include <sys/thread.h>
#elif defined(__APPLE__)
	#include <mach/mach_init.h>
	#include <mach/thread_act.h>
#endif


Thread::Thread(const std::string& name) :
	mName(name), mRequestStop(false), mRunning(false)
{

}


Thread::~Thread()
{
	// kill the thread if running
	if (mRunning) 
    {
        mRunning = false;

#if defined(_WIN32)
		// See https://msdn.microsoft.com/en-us/library/hh920601.aspx#thread__native_handle_method
		TerminateThread((HANDLE) mThreadObj->native_handle(), 0);
		CloseHandle((HANDLE)mThreadObj->native_handle());
#else
		// We need to pthread_kill instead on Android since NDKv5's pthread
		// implementation is incomplete.
# ifdef __ANDROID__
		pthread_kill(getThreadHandle(), SIGKILL);
# else
		pthread_cancel(getThreadHandle());
# endif
		wait();
#endif
	}
    else Wait();

	// Make sure start finished mutex is unlocked before it's destroyed
	if (mStartFinishedMutex.try_lock())
        mStartFinishedMutex.unlock();

}


bool Thread::Start()
{
	MutexAutoLock lock(mMutex);

	if (mRunning)
		return false;

	mRequestStop = false;

	// The mutex may already be locked if the thread is being restarted
    mStartFinishedMutex.try_lock();

	try 
    {
		mThreadObj = new std::thread(ThreadProc, this);
	} 
    catch (const std::system_error& ) 
    {
		return false;
	}

	// Allow spawned thread to continue
	mStartFinishedMutex.unlock();

	while (!mRunning)
		Sleep(1);

	mJoinable = true;

	return true;
}


bool Thread::Stop()
{
	mRequestStop = true;
	return true;
}


bool Thread::Wait()
{
	MutexAutoLock lock(mMutex);

	if (!mJoinable)
		return false;


	mThreadObj->join();

	delete mThreadObj;
	mThreadObj = nullptr;

	LogAssert(mRunning == false, "thread running");
	mJoinable = false;
	return true;
}



bool Thread::GetReturnValue(void** ret)
{
	if (mRunning)
		return false;

	*ret = mRetval;
	return true;
}


void Thread::ThreadProc(Thread* thr)
{
	thr->SetName(thr->mName);

	//g_logger.registerThread(thr->mName);
	thr->mRunning = true;

	// Wait for the thread that started this one to finish initializing the
	// thread handle so that getThreadId/getThreadHandle will work.
	thr->mStartFinishedMutex.lock();

	thr->mRetval = thr->Run();

	thr->mRunning = false;
	// Unlock m_start_finished_mutex to prevent data race condition on Windows.
	// On Windows with VS2017 build TerminateThread is called and this mutex is not
	// released. We try to unlock it from caller thread and it's refused by system.
	thr->mStartFinishedMutex.unlock();
	//g_logger.deregisterThread();
}


void Thread::SetName(const std::string& name)
{
#if defined(__linux__)

	// It would be cleaner to do this with pthread_setname_np,
	// which was added to glibc in version 2.12, but some major
	// distributions are still runing 2.11 and previous versions.
	prctl(PR_SET_NAME, name.c_str());

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

	pthread_set_name_np(pthread_self(), name.c_str());

#elif defined(__NetBSD__)

	pthread_setname_np(pthread_self(), "%s", const_cast<char*>(name.c_str()));

#elif defined(__APPLE__)

	pthread_setname_np(name.c_str());

#elif defined(__HAIKU__)

	rename_thread(find_thread(NULL), name.c_str());

#elif defined(_MSC_VER)

	// Windows itself doesn't support thread names,
	// but the MSVC debugger does...
	THREADNAME_INFO info;

	info.dwType = 0x1000;
	info.szName = name.c_str();
	info.dwThreadID = -1;
	info.dwFlags = 0;

	__try {
		RaiseException(0x406D1388, 0,
			sizeof(info) / sizeof(DWORD), (ULONG_PTR *)&info);
	} __except (EXCEPTION_CONTINUE_EXECUTION) {
	}

#elif defined(_WIN32) || defined(__GNU__)

	// These platforms are known to not support thread names.
	// Silently ignore the request.

#else
	#warning "Unrecognized platform, thread names will not be available."
#endif
}


unsigned int Thread::GetNumberOfProcessors()
{
	return std::thread::hardware_concurrency();
}


bool Thread::BindToProcessor(unsigned int procNumber)
{
#if defined(__ANDROID__)

	return false;

#elif _MSC_VER

	return SetThreadAffinityMask(GetThreadHandle(), 1 << procNumber);

#elif __MINGW32__

	return SetThreadAffinityMask(pthread_gethandle(getThreadHandle()), 1 << procNumber);

#elif __FreeBSD_version >= 702106 || defined(__linux__) || defined(__DragonFly__)

	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(procNumber, &cpuset);

	return pthread_setaffinity_np(getThreadHandle(), sizeof(cpuset), &cpuset) == 0;
#elif defined(__NetBSD__)

	cpuset_t *cpuset = cpuset_create();
	if (cpuset == NULL)
		return false;
	int r = pthread_setaffinity_np(getThreadHandle(), cpuset_size(cpuset), cpuset);
	cpuset_destroy(cpuset);
	return r == 0;
#elif defined(__sun) || defined(sun)

	return processor_bind(P_LWPID, P_MYID, procNumber, NULL) == 0

#elif defined(_AIX)

	return bindprocessor(BINDTHREAD, m_kernel_thread_id, procNumber) == 0;

#elif defined(__hpux) || defined(hpux)

	pthread_spu_t answer;

	return pthread_processor_bind_np(PTHREAD_BIND_ADVISORY_NP,
			&answer, procNumber, getThreadHandle()) == 0;

#elif defined(__APPLE__)

	struct thread_affinity_policy tapol;

	thread_port_t threadport = pthread_mach_thread_np(getThreadHandle());
	tapol.affinity_tag = procNumber + 1;
	return thread_policy_set(threadport, THREAD_AFFINITY_POLICY,
			(thread_policy_t)&tapol,
			THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;

#else

	return false;

#endif
}


bool Thread::SetPriority(int prio)
{
#ifdef _MSC_VER

	return SetThreadPriority(GetThreadHandle(), prio);

#elif __MINGW32__

	return SetThreadPriority(pthread_gethandle(GetThreadHandle()), prio);

#else

	struct sched_param sparam;
	int policy;

	if (pthread_getschedparam(GetThreadHandle(), &policy, &sparam) != 0)
		return false;

	int min = sched_get_priority_min(policy);
	int max = sched_get_priority_max(policy);

	sparam.sched_priority = min + prio * (max - min) / THREAD_PRIORITY_HIGHEST;
	return pthread_setschedparam(GetThreadHandle(), policy, &sparam) == 0;

#endif
}

