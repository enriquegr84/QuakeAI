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

#ifndef THREAD_H
#define THREAD_H

#include "GameEngineStd.h"

#include "MutexAutolock.h"
#include "Semaphore.h"

#include <atomic>
#include <thread>
#include <mutex>

/*
 * On platforms using pthreads, these five priority classes correlate to
 * even divisions between the minimum and maximum reported thread priority.
 */
#if !defined(_WIN32)
	#define THREAD_PRIORITY_LOWEST       0
	#define THREAD_PRIORITY_BELOW_NORMAL 1
	#define THREAD_PRIORITY_NORMAL       2
	#define THREAD_PRIORITY_ABOVE_NORMAL 3
	#define THREAD_PRIORITY_HIGHEST      4
#endif


 // Thread-safe Double-ended queue

template<typename Element>
class MutexedQueue
{
public:
    template<typename Key, typename U, typename Caller, typename CallerData>
    friend class RequestQueue;

    MutexedQueue() = default;

    bool Empty() const
    {
        MutexAutoLock lock(mMutex);
        return mQueue.empty();
    }

    void PushBack(const Element& element)
    {
        MutexAutoLock lock(mMutex);
        mQueue.push_back(element);
        mSignal.Post();
    }

    /* this version of pop_front returns a empty element of T on timeout.
    * Make sure default constructor of T creates a recognizable "empty" element
    */
    Element PopFrontNoEx(uint32_t waitTimeMaxMs)
    {
        if (mSignal.wait(waitTimeMaxMs))
        {
            MutexAutoLock lock(mMutex);

            Element el = std::move(mQueue.front());
            mQueue.pop_front();
            return el;
        }

        return Element();
    }

    Element PopFront(uint32_t waitTimeMaxMs)
    {
        if (mSignal.Wait(waitTimeMaxMs))
        {
            MutexAutoLock lock(mMutex);

            Element el = std::move(mQueue.front());
            mQueue.pop_front();
            return el;
        }

        throw ItemNotFoundException("MutexedQueue: queue is empty");
    }

    Element PopFrontNoEx()
    {
        mSignal.Wait();

        MutexAutoLock lock(mMutex);

        Element el = std::move(mQueue.front());
        mQueue.pop_front();
        return el;
    }

    Element PopBack(uint32_t waitTimeMaxMs = 0)
    {
        if (mSignal.wait(waitTimeMaxMs))
        {
            MutexAutoLock lock(mMutex);

            Element el = std::move(mQueue.back());
            mQueue.pop_back();
            return el;
        }

        throw ItemNotFoundException("MutexedQueue: queue is empty");
    }

    /* this version of pop_back returns a empty element of T on timeout.
    * Make sure default constructor of T creates a recognizable "empty" element
    */
    Element PopBackNoEx(uint32_t waitTimeMaxMs)
    {
        if (mSignal.wait(waitTimeMaxMs))
        {
            MutexAutoLock lock(mMutex);

            Element el = std::move(mQueue.back());
            mQueue.pop_back();
            return el;
        }

        return Element();
    }

    Element PopBackNoEx()
    {
        mSignal.wait();

        MutexAutoLock lock(mMutex);

        Element el = std::move(mQueue.back());
        mQueue.pop_back();
        return el;
    }

protected:
    std::mutex& GetMutex() { return mMutex; }

    std::deque<Element>& GetQueue() { return mQueue; }

    std::deque<Element> mQueue;
    mutable std::mutex mMutex;
    Semaphore mSignal;
};


template<typename Key, typename Value>
class MutexedMap
{
public:
    MutexedMap() = default;

    void Set(const Key& name, const Value& value)
    {
        MutexAutoLock lock(mMutex);
        mValues[name] = value;
    }

    bool Get(const Key& name, Value* result) const
    {
        MutexAutoLock lock(mMutex);
        auto n = mValues.find(name);
        if (n == mValues.end())
            return false;
        if (result)
            *result = n->second;
        return true;
    }

    std::vector<Value> GetValues() const
    {
        MutexAutoLock lock(mMutex);
        std::vector<Value> result;
        result.reserve(mValues.size());
        for (auto it = mValues.begin(); it != mValues.end(); ++it)
            result.push_back(it->second);
        return result;
    }

    void Clear() { mValues.clear(); }

private:
    std::map<Key, Value> mValues;
    mutable std::mutex mMutex;
};


template<typename T>
class MutexedVariable
{
public:
    MutexedVariable(const T& value) : mValue(value) { }

    T Get()
    {
        MutexAutoLock lock(mMutex);
        return mValue;
    }

    void Set(const T& value)
    {
        MutexAutoLock lock(mMutex);
        mValue = value;
    }

    // You pretty surely want to grab the lock when accessing this
    T mValue;

private:
    std::mutex mMutex;
};

/*
    A single worker thread - multiple client threads queue framework.
*/
template<typename Key, typename T, typename Caller, typename CallerData>
class GetResult 
{
public:
    Key mKey;
    T mItem;
    std::pair<Caller, CallerData> mCaller;
};

template<typename Key, typename T, typename Caller, typename CallerData>
class ResultQueue : public MutexedQueue<GetResult<Key, T, Caller, CallerData> > 
{
};

template<typename Caller, typename Data, typename Key, typename T>
class CallerInfo 
{
public:
    Caller mCaller;
    Data mData;
    ResultQueue<Key, T, Caller, Data>* mDest;
};

template<typename Key, typename T, typename Caller, typename CallerData>
class GetRequest {
public:
    GetRequest() = default;
    ~GetRequest() = default;

    GetRequest(const Key& key) : mKey(key)
    {
    }

    Key mKey;
    std::list<CallerInfo<Caller, CallerData, Key, T> > mCallers;
};

/**
 * Notes for RequestQueue usage
 * @param Key unique key to identify a request for a specific resource
 * @param T ?
 * @param Caller unique id of calling thread
 * @param CallerData data passed back to caller
 */
template<typename Key, typename T, typename Caller, typename CallerData>
class RequestQueue {
public:
    bool Empty()
    {
        return mQueue.Empty();
    }

    void Add(const Key &key, Caller caller, CallerData callerdata,
        ResultQueue<Key, T, Caller, CallerData> *dest)
    {
        typename std::deque<GetRequest<Key, T, Caller, CallerData> >::iterator i;
        typename std::list<CallerInfo<Caller, CallerData, Key, T> >::iterator j;

        {
            MutexAutoLock lock(mQueue.GetMutex());

            /*
                If the caller is already on the list, only update CallerData
            */
            for (i = mQueue.GetQueue().begin(); i != mQueue.GetQueue().end(); ++i) 
            {
                GetRequest<Key, T, Caller, CallerData>& request = *i;
                if (request.mKey != key)
                    continue;

                for (j = request.mCallers.begin(); j != request.mCallers.end(); ++j) 
                {
                    CallerInfo<Caller, CallerData, Key, T>& ca = *j;
                    if (ca.mCaller == caller) 
                    {
                        ca.mData = callerdata;
                        return;
                    }
                }

                CallerInfo<Caller, CallerData, Key, T> ca;
                ca.mCaller = caller;
                ca.mData = callerdata;
                ca.mDest = dest;
                request.mCallers.push_back(ca);
                return;
            }
        }

        /*
            Else add a new request to the queue
        */

        GetRequest<Key, T, Caller, CallerData> request;
        request.mKey = key;
        CallerInfo<Caller, CallerData, Key, T> ca;
        ca.mCaller = caller;
        ca.mData = callerdata;
        ca.mDest = dest;
        request.mCallers.push_back(ca);

        mQueue.PushBack(request);
    }

    GetRequest<Key, T, Caller, CallerData> Pop(unsigned int timeoutMs)
    {
        return mQueue.PopFront(timeoutMs);
    }

    GetRequest<Key, T, Caller, CallerData> Pop()
    {
        return mQueue.PopFrontNoEx();
    }

    void PushResult(GetRequest<Key, T, Caller, CallerData> req, T res)
    {
        for (typename std::list<CallerInfo<Caller, CallerData, Key, T> >::iterator
            it = req.mCallers.begin(); it != req.mCallers.end(); ++it)
        {
            CallerInfo<Caller, CallerData, Key, T>& ca = *it;

            GetResult<Key, T, Caller, CallerData> result;
            result.mKey = req.mKey;
            result.mItem = res;
            result.mCaller.first = ca.mCaller;
            result.mCaller.second = ca.mData;

            ca.mDest->PushBack(result);
        }
    }

private:
    MutexedQueue<GetRequest<Key, T, Caller, CallerData> > mQueue;
};

class Thread
{
public:
    Thread(const std::string& name = "");
    virtual ~Thread();

    /*
     * Begins execution of a new thread at the pure virtual method Thread::run().
     * Execution of the thread is guaranteed to have started after this function
     * returns.
     */
    bool Start();

    /*
     * Requests that the thread exit gracefully.
     * Returns immediately; thread execution is guaranteed to be complete after
     * a subsequent call to Thread::wait.
     */
    bool Stop();

    /*
     * Waits for thread to finish.
     * Note:  This does not stop a thread, you have to do this on your own.
     * Returns false immediately if the thread is not started or has been waited
     * on before.
     */
    bool Wait();

    /*
     * Returns true if the calling thread is this Thread object.
     */
    bool IsCurrentThread() { return std::this_thread::get_id() == GetThreadId(); }

    bool IsRunning() { return mRunning; }
    bool StopRequested() { return mRequestStop; }

    std::thread::id GetThreadId() { return mThreadObj->get_id(); }

    /*
     * Gets the thread return value.
     * Returns true if the thread has exited and the return value was available,
     * or false if the thread has yet to finish.
     */
    bool GetReturnValue(void** ret);

    /*
     * Binds (if possible, otherwise sets the affinity of) the thread to the
     * specific processor specified by proc_number.
     */
    bool BindToProcessor(unsigned int procNumber);

    /*
     * Sets the thread priority to the specified priority.
     *
     * prio can be one of: THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL,
     * THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST.
     * On Windows, any of the other priorites as defined by SetThreadPriority
     * are supported as well.
     *
     * Note that it may be necessary to first set the threading policy or
     * scheduling algorithm to one that supports thread priorities if not
     * supported by default, otherwise this call will have no effect.
     */
    bool SetPriority(int prio);

    /*
     * Sets the currently executing thread's name to where supported; useful
     * for debugging.
     */
    static void SetName(const std::string& name);

    /*
     * Returns the number of processors/cores configured and active on this machine.
     */
    static unsigned int GetNumberOfProcessors();

protected:
    std::string mName;

    virtual void* Run() = 0;

private:
    std::thread::native_handle_type GetThreadHandle() { return mThreadObj->native_handle(); }

    static void ThreadProc(Thread* thread);

    void* mRetval = nullptr;
    bool mJoinable = false;
    std::atomic<bool> mRequestStop;
    std::atomic<bool> mRunning;
    std::mutex mMutex;
    std::mutex mStartFinishedMutex;

    std::thread* mThreadObj = nullptr;
};


class UpdateThread : public Thread
{
public:
    UpdateThread(const std::string& name) : Thread(name + "Update") {}
    ~UpdateThread() = default;

    void DeferUpdate() { mUpdateSemaphore.Post(); }

    void Stop()
    {
        Thread::Stop();

        // give us a nudge
        mUpdateSemaphore.Post();
    }

    void* Run()
    {
        while (!StopRequested()) 
        {
            mUpdateSemaphore.Wait();
            // Set semaphore to 0
            while (mUpdateSemaphore.Wait(0));

            if (StopRequested()) break;

            DoUpdate();
        }

        return NULL;
    }

protected:
    virtual void DoUpdate() = 0;

private:
    Semaphore mUpdateSemaphore;
};



// The root class of the object system. It provides some basic services for
// an object-oriented library. This class is abstract.
template <typename Key, typename Value>
class ThreadSafeMap
{
public:
    // Construction and destruction.
    virtual ~ThreadSafeMap();
    ThreadSafeMap();

    // All the operations are thread-safe.
    bool HasElements() const;
    bool Exists(Key key) const;
    void Insert(Key key, Value value);
    bool Remove(Key key, Value& value);
    void RemoveAll();
    bool Get(Key key, Value& value) const;
    void GatherAll(std::vector<Value>& values) const;

protected:
    std::map<Key, Value> mMap;
    mutable std::mutex mMutex;
};

template <typename Key, typename Value>
ThreadSafeMap<Key, Value>::~ThreadSafeMap()
{
}

template <typename Key, typename Value>
ThreadSafeMap<Key, Value>::ThreadSafeMap()
{
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::HasElements() const
{
    bool hasElements;
    mMutex.lock();
    {
        hasElements = (mMap.size() > 0);
    }
    mMutex.unlock();
    return hasElements;
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::Exists(Key key) const
{
    bool exists;
    mMutex.lock();
    {
        exists = (mMap.find(key) != mMap.end());
    }
    mMutex.unlock();
    return exists;
}

template <typename Key, typename Value>
void ThreadSafeMap<Key, Value>::Insert(Key key, Value value)
{
    mMutex.lock();
    {
        mMap[key] = value;
    }
    mMutex.unlock();
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::Remove(Key key, Value& value)
{
    bool exists;
    mMutex.lock();
    {
        auto iter = mMap.find(key);
        if (iter != mMap.end())
        {
            value = iter->second;
            mMap.erase(iter);
            exists = true;
        }
        else
        {
            exists = false;
        }
    }
    mMutex.unlock();
    return exists;
}

template <typename Key, typename Value>
void ThreadSafeMap<Key, Value>::RemoveAll()
{
    mMutex.lock();
    {
        mMap.clear();
    }
    mMutex.unlock();
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::Get(Key key, Value& value) const
{
    bool exists;
    mMutex.lock();
    {
        auto iter = mMap.find(key);
        if (iter != mMap.end())
        {
            value = iter->second;
            exists = true;
        }
        else
        {
            exists = false;
        }
    }
    mMutex.unlock();
    return exists;
}

template <typename Key, typename Value>
void ThreadSafeMap<Key, Value>::GatherAll(std::vector<Value>& values) const
{
    mMutex.lock();
    {
        if (mMap.size() > 0)
        {
            values.resize(mMap.size());
            auto viter = values.begin();
            for (auto const& m : mMap)
            {
                *viter++ = m.second;
            }
        }
        else
        {
            values.clear();
        }
    }
    mMutex.unlock();
}


template <typename Element>
class ThreadSafeQueue
{
public:
    // Construction and destruction.
    virtual ~ThreadSafeQueue();
    ThreadSafeQueue(size_t maxNumElements = 0);

    // All the operations are thread-safe.
    size_t GetMaxNumElements() const;
    size_t GetNumElements() const;
    bool Push(Element const& element);
    bool Pop(Element& element);

protected:
    size_t mMaxNumElements;
    std::queue<Element> mQueue;
    mutable std::mutex mMutex;
};

template <typename Element>
ThreadSafeQueue<Element>::~ThreadSafeQueue()
{
}

template <typename Element>
ThreadSafeQueue<Element>::ThreadSafeQueue(size_t maxNumElements)
    :
    mMaxNumElements(maxNumElements)
{
}

template <typename Element>
size_t ThreadSafeQueue<Element>::GetMaxNumElements() const
{
    size_t maxNumElements;
    mMutex.lock();
    {
        maxNumElements = mMaxNumElements;
    }
    mMutex.unlock();
    return maxNumElements;
}

template <typename Element>
size_t ThreadSafeQueue<Element>::GetNumElements() const
{
    size_t numElements;
    mMutex.lock();
    {
        numElements = mQueue.size();
    }
    mMutex.unlock();
    return numElements;
}

template <typename Element>
bool ThreadSafeQueue<Element>::Push(Element const& element)
{
    bool pushed;
    mMutex.lock();
    {
        if (mQueue.size() < mMaxNumElements)
        {
            mQueue.push(element);
            pushed = true;
        }
        else
        {
            pushed = false;
        }
    }
    mMutex.unlock();
    return pushed;
}

template <typename Element>
bool ThreadSafeQueue<Element>::Pop(Element& element)
{
    bool popped;
    mMutex.lock();
    {
        if (mQueue.size() > 0)
        {
            element = mQueue.front();
            mQueue.pop();
            popped = true;
        }
        else
        {
            popped = false;
        }
    }
    mMutex.unlock();
    return popped;
}

#endif