/*
Minetest
Copyright (C) 2013, 2017 celeron55, Perttu Ahola <celeron55@gmail.com>

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


#ifndef MESHGENERATORTHREAD_H
#define MESHGENERATORTHREAD_H

#include "GameEngineStd.h"

#include "Map/MapBlockMesh.h"

#include "Core/Threading/MutexAutolock.h"
#include "Core/Threading/Thread.h"

struct CachedMapBlockData
{
    Vector3<short> position = Vector3<short>{ -1337, -1337, -1337 };
	MapNode* data = nullptr; // A copy of the MapBlock's data member
	int refcountFromQueue = 0;
	time_t lastUsedTimestamp = time(0);

	CachedMapBlockData() = default;
	~CachedMapBlockData();
};

struct QueuedMeshUpdate
{
    Vector3<short> position = Vector3<short>{ -1337, -1337, -1337 };
	bool ackBlockToLogic = false;
	int crackLevel = -1;
	Vector3<short> crackPosition = Vector3<short>::Zero();
	MeshMakeData* data = nullptr; // This is generated in MeshUpdateQueue::Pop()

	QueuedMeshUpdate() = default;
	~QueuedMeshUpdate();
};

/*
	A thread-safe queue of mesh update tasks and a cache of MapBlock data
*/
class MeshUpdateQueue
{
	enum UpdateMode
	{
		FORCE_UPDATE,
		SKIP_UPDATE_IF_ALREADY_CACHED,
	};

public:
	MeshUpdateQueue(VisualEnvironment* env);

	~MeshUpdateQueue();

	// Caches the block at p and its neighbors (if needed) and queues a mesh
	// update for the block at p
	void AddBlock(Map* map, Vector3<short> pos, bool ackBlockToLogic, bool urgent);

	// Returned pointer must be deleted
	// Returns NULL if queue is empty
	QueuedMeshUpdate* Pop();

	unsigned int Size()
	{
		MutexAutoLock lock(mMutex);
		return (unsigned int)mQueue.size();
	}

private:

    VisualEnvironment* mEnvironment;

	std::vector<QueuedMeshUpdate*> mQueue;
    std::set<Vector3<short>> mUrgents;
    std::map<Vector3<short>, CachedMapBlockData*> mCache;
	std::mutex mMutex;

	// TODO: Add callback to update these when g_settings changes
	bool mCacheEnableShaders;
	bool mCacheSmoothLighting;
	int mMeshGeneratorBlockCacheSize;

	CachedMapBlockData* CacheBlock(Map* map, Vector3<short> pos, 
        UpdateMode mode, size_t* cacheHitCounter = NULL);
	CachedMapBlockData* GetCachedBlock(const Vector3<short>& p);
	void FillDataFromMapBlockCache(QueuedMeshUpdate* q);
	void CleanupCache();
};

struct MeshUpdateResult
{
    Vector3<short> position = Vector3<short>{ -1338, -1338, -1338 };
    std::shared_ptr<MapBlockMesh> mesh = nullptr;
	bool ackBlockToLogic = false;

	MeshUpdateResult() = default;
};

class MeshUpdateThread : public UpdateThread
{
public:
	MeshUpdateThread(VisualEnvironment* env);

	// Caches the block at p and its neighbors (if needed) and queues a mesh
	// update for the block at p
	void UpdateBlock(Map* map, Vector3<short> pos, bool ackBlockToLogic, bool urgent);

	Vector3<short> mCameraOffset = Vector3<short>::Zero();
	MutexedQueue<MeshUpdateResult> mQueueOut;

private:

	MeshUpdateQueue mQueueIn;

	// TODO: Add callback to update these when g_settings changes
	int mGenerationInterval;

protected:
	virtual void DoUpdate();
};

#endif