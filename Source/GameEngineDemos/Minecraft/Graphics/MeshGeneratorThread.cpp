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

#include "MeshGeneratorThread.h"

#include "../Games/Environment/VisualEnvironment.h"

#include "../Games/Map/MapBlock.h"
#include "../Games/Map/Map.h"

#include "Application/Settings.h"

#include "Core/Utility/Profiler.h"


/*
	CachedMapBlockData
*/

CachedMapBlockData::~CachedMapBlockData()
{
	LogAssert(refcountFromQueue == 0, "invalid ref count");

	delete[] data;
}

/*
	QueuedMeshUpdate
*/

QueuedMeshUpdate::~QueuedMeshUpdate()
{
	delete data;
}

/*
	MeshUpdateQueue
*/

MeshUpdateQueue::MeshUpdateQueue(VisualEnvironment* env) : mEnvironment(env)
{
	mCacheEnableShaders = Settings::Get()->GetBool("enable_shaders");
	mCacheSmoothLighting = Settings::Get()->GetBool("smooth_lighting");
	mMeshGeneratorBlockCacheSize = Settings::Get()->GetInt("meshgen_block_cache_size");
}

MeshUpdateQueue::~MeshUpdateQueue()
{
	MutexAutoLock lock(mMutex);

	for (auto& c : mCache)
		delete c.second;

	for (QueuedMeshUpdate* q : mQueue)
		delete q;

}

void MeshUpdateQueue::AddBlock(Map *map, Vector3<short> pos, bool ackBlockToLogic, bool urgent)
{
	MutexAutoLock lock(mMutex);

	CleanupCache();

	/*
		Cache the block data (force-update the center block, don't update the
		neighbors but get them if they aren't already cached)
	*/
	std::vector<CachedMapBlockData*> cachedBlocks;
	size_t cacheHitCounter = 0;
	cachedBlocks.reserve(3*3*3);
	Vector3<short> dp;
    for (dp[0] = -1; dp[0] <= 1; dp[0]++)
    {
        for (dp[1] = -1; dp[1] <= 1; dp[1]++)
        {
            for (dp[2] = -1; dp[2] <= 1; dp[2]++)
            {
                Vector3<short> pos1 = pos + dp;
                CachedMapBlockData* cachedBlock;
                if (dp == Vector3<short>::Zero())
                    cachedBlock = CacheBlock(map, pos1, FORCE_UPDATE);
                else
                    cachedBlock = CacheBlock(map, pos1, SKIP_UPDATE_IF_ALREADY_CACHED, &cacheHitCounter);
                cachedBlocks.push_back(cachedBlock);
            }
        }
    }
	Profiling->Avg("MeshUpdateQueue: MapBlocks from cache [%]", 
        100.0f * cacheHitCounter / cachedBlocks.size());

	/*
		Mark the block as urgent if requested
	*/
	if (urgent)
		mUrgents.insert(pos);

	/*
		Find if block is already in queue.
		If it is, update the data and quit.
	*/
	for (QueuedMeshUpdate* q : mQueue) 
    {
		if (q->position == pos) 
        {
			// NOTE: We are not adding a new position to the queue, thus
			//       refcountFromQueue stays the same.
			if(ackBlockToLogic)
				q->ackBlockToLogic = true;
			q->crackLevel = mEnvironment->GetCrackLevel();
			q->crackPosition = mEnvironment->GetCrackPosition();
			return;
		}
	}

	/*
		Add the block
	*/
	QueuedMeshUpdate* q = new QueuedMeshUpdate;
	q->position = pos;
	q->ackBlockToLogic = ackBlockToLogic;
	q->crackLevel = mEnvironment->GetCrackLevel();
	q->crackPosition = mEnvironment->GetCrackPosition();
	mQueue.push_back(q);

	// This queue entry is a new reference to the cached blocks
	for (CachedMapBlockData* cachedBlock : cachedBlocks)
		cachedBlock->refcountFromQueue++;
}

// Returned pointer must be deleted
// Returns NULL if queue is empty
QueuedMeshUpdate* MeshUpdateQueue::Pop()
{
	MutexAutoLock lock(mMutex);

	bool mustBeUrgent = !mUrgents.empty();
	for (std::vector<QueuedMeshUpdate*>::iterator i = mQueue.begin(); i != mQueue.end(); ++i) 
    {
		QueuedMeshUpdate* q = *i;
		if(mustBeUrgent && mUrgents.count(q->position) == 0)
			continue;
		mQueue.erase(i);
		mUrgents.erase(q->position);
		FillDataFromMapBlockCache(q);
		return q;
	}
	return NULL;
}

CachedMapBlockData* MeshUpdateQueue::CacheBlock(Map* map, 
    Vector3<short> pos, UpdateMode mode, size_t* cacheHitCounter)
{
	CachedMapBlockData* cachedBlock = nullptr;
	std::map<Vector3<short>, CachedMapBlockData*>::iterator it = mCache.find(pos);

	if (it != mCache.end()) 
    {
		cachedBlock = it->second;

		if (mode == SKIP_UPDATE_IF_ALREADY_CACHED) 
        {
			if (cacheHitCounter)
				(*cacheHitCounter)++;
			return cachedBlock;
		}
	}

	if (!cachedBlock) 
    {
		// Not yet in cache
		cachedBlock = new CachedMapBlockData();
		mCache[pos] = cachedBlock;
	}

	MapBlock* block = map->GetBlockNoCreateNoEx(pos);
	if (block)
    {
		if (!cachedBlock->data)
			cachedBlock->data = new MapNode[MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE];
		memcpy(cachedBlock->data, block->GetData(),
            MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE * sizeof(MapNode));
	} 
    else 
    {
		delete[] cachedBlock->data;
		cachedBlock->data = nullptr;
	}
	return cachedBlock;
}

CachedMapBlockData* MeshUpdateQueue::GetCachedBlock(const Vector3<short>& pos)
{
	std::map<Vector3<short>, CachedMapBlockData*>::iterator it = mCache.find(pos);
	if (it != mCache.end())
		return it->second;

	return NULL;
}

void MeshUpdateQueue::FillDataFromMapBlockCache(QueuedMeshUpdate* q)
{
	MeshMakeData* data = new MeshMakeData(mEnvironment, mCacheEnableShaders);
	q->data = data;

	data->FillBlockDataBegin(q->position);

	time_t tNow = time(0);

	// Collect data for 3*3*3 blocks from cache
	Vector3<short> dp;
    for (dp[0] = -1; dp[0] <= 1; dp[0]++)
    {
        for (dp[1] = -1; dp[1] <= 1; dp[1]++)
        {
            for (dp[2] = -1; dp[2] <= 1; dp[2]++) 
            {
                Vector3<short> p = q->position + dp;
                CachedMapBlockData* cachedBlock = GetCachedBlock(p);
                if (cachedBlock) 
                {
                    cachedBlock->refcountFromQueue--;
                    cachedBlock->lastUsedTimestamp = tNow;
                    if (cachedBlock->data)
                        data->FillBlockData(dp, cachedBlock->data);
                }
            }
        }
    }

	data->SetCrack(q->crackLevel, q->crackPosition);
	data->SetSmoothLighting(mCacheSmoothLighting);
}

void MeshUpdateQueue::CleanupCache()
{
	const int mapBlockKB = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE * sizeof(MapNode) / 1000;
	Profiling->Avg("MeshUpdateQueue MapBlock cache size kB", (float)(mapBlockKB * mCache.size()));

	// The cache size is kept roughly below cacheSoftMaxSize, not letting
	// anything get older than cacheSecondsMax or deleted before 2 seconds.
	const int cacheSecondsMax = 10;
	const int cacheSoftMaxSize = mMeshGeneratorBlockCacheSize * 1000 / mapBlockKB;
	int cacheSeconds = std::max(2, cacheSecondsMax -
			(int)mCache.size() / (cacheSoftMaxSize / cacheSecondsMax));

	int tNow = (int)time(0);
    std::map<Vector3<short>, CachedMapBlockData*>::iterator it;
	for (it = mCache.begin(); it != mCache.end(); ) 
    {
		CachedMapBlockData *cachedBlock = it->second;
		if (cachedBlock->refcountFromQueue == 0 &&
            cachedBlock->lastUsedTimestamp < tNow - cacheSeconds)
        {
			mCache.erase(it++);
			delete cachedBlock;
		}
        else ++it;
	}
}

/*
	MeshUpdateThread
*/

MeshUpdateThread::MeshUpdateThread(VisualEnvironment* env) : UpdateThread("Mesh"), mQueueIn(env)
{
	mGenerationInterval = Settings::Get()->GetUInt16("mesh_generation_interval");
	mGenerationInterval = std::clamp(mGenerationInterval, 0, 50);
}

void MeshUpdateThread::UpdateBlock(Map* map, Vector3<short> pos, bool ackBlockToLogic, bool urgent)
{
	// Allow the MeshUpdateQueue to do whatever it wants
	mQueueIn.AddBlock(map, pos, ackBlockToLogic, urgent);
	DeferUpdate();
}

void MeshUpdateThread::DoUpdate()
{
	QueuedMeshUpdate* q;
	while ((q = mQueueIn.Pop())) 
    {
		if (mGenerationInterval)
			Sleep(mGenerationInterval);
		ScopeProfiler sp(Profiling, "Mesh making (sum)");

		std::shared_ptr<MapBlockMesh> meshNew = 
            std::make_shared<MapBlockMesh>(q->data, mCameraOffset);

		MeshUpdateResult r;
		r.position = q->position;
		r.mesh = meshNew;
		r.ackBlockToLogic = q->ackBlockToLogic;

		mQueueOut.PushBack(r);

		delete q;
	}
}
