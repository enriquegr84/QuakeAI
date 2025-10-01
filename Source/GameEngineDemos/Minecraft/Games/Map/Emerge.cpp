/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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

#include "Emerge.h"

#include "Voxel.h"

#include "../../Utils/Util.h"

#include "../Environment/LogicEnvironment.h"

#include "MapGeneratorDecoration.h"
#include "MapGeneratorSchematic.h"
#include "MapGeneratorBiome.h"
#include "MapGeneratorOre.h"
#include "MapBlock.h"

#include "Core/Logger/Logger.h"
#include "Core/Threading/Thread.h"
#include "Core/Threading/ConditionVariable.h"
#include "Core/Utility/Profiler.h"

#include "Application/Settings.h"

class EmergeThread : public Thread 
{
public:
	bool mEnableMapgenDebugInfo;
	int mId;

	EmergeThread(LogicEnvironment* env, int ethreadid);
	~EmergeThread() = default;

	void* Run();
	void Signal();

	// Requires queue mutex held
	bool PushBlock(const Vector3<short>& pos);

	void CancelPendingItems();

	static void RunCompletionCallbacks(const Vector3<short>& pos, 
        EmergeAction action, const EmergeCallbackList& callbacks);

private:
    LogicEnvironment* mEnvironment;

	LogicMap* mMap;
    MapGenerator* mMapgen;

    ConditionVariable mQueueEvent;
	std::queue<Vector3<short>> mBlockQueue;

	bool PopBlockEmerge(Vector3<short>* pos, BlockEmergeData* bedata);

	EmergeAction GetBlockOrStartGen(const Vector3<short>& pos, 
        bool allowGen, MapBlock** block, BlockMakeData* data);
	MapBlock* FinishGen(Vector3<short> pos, BlockMakeData* bmdata,
		std::map<Vector3<short>, MapBlock *>* modifiedBlocks);

	friend class EmergeManager;
};

class MapEditEventAreaIgnorer
{
public:
	MapEditEventAreaIgnorer(VoxelArea* ignorevariable, const VoxelArea& a):
		mIgnoreVariable(ignorevariable)
	{
		if(mIgnoreVariable->GetVolume() == 0)
			*mIgnoreVariable = a;
		else
			mIgnoreVariable = NULL;
	}

	~MapEditEventAreaIgnorer()
	{
		if(mIgnoreVariable)
		{
			LogAssert(mIgnoreVariable->GetVolume() != 0, "invalid volume");
			*mIgnoreVariable = VoxelArea();
		}
	}

private:
	VoxelArea* mIgnoreVariable;
};

EmergeParams::~EmergeParams()
{
	LogInformation("EmergeParams: destroying ");
	// Delete everything that was cloned on creation of EmergeParams
	delete mBiomeGen;
	delete mBiomeMgr;
	delete mOreMgr;
	delete mDecoMgr;
	delete mSchemMgr;
}

EmergeParams::EmergeParams(
    EmergeManager* parent, const BiomeGenerator* biomeGen,
	const BiomeManager* biomeMgr, const OreManager* oreMgr, 
    const DecorationManager* decoMgr, const SchematicManager* schemMgr) :
	mNodeMgr(parent->mNodeMgr), mEnableMapgenDebugInfo(parent->mEnableMapgenDebugInfo),
	mGenNotifyOn(parent->mGenNotifyOn), mGenNotifyOnDecoIds(&parent->mGenNotifyOnDecoIds),
	mBiomeMgr(biomeMgr->Clone()), mOreMgr(oreMgr->Clone()),
	mDecoMgr(decoMgr->Clone()), mSchemMgr(schemMgr->Clone())
{
	this->mBiomeGen = biomeGen->Clone(this->mBiomeMgr);
}

////
//// EmergeManager
////

EmergeManager* EmergeManager::mEmergeMgr = NULL;

EmergeManager* EmergeManager::Get(void)
{
    LogAssert(EmergeManager::mEmergeMgr, "Emerge manager doesn't exist");
    return EmergeManager::mEmergeMgr;
}

EmergeManager::EmergeManager(LogicEnvironment* env)
{
	mNodeMgr = env->GetNodeManager();
	mBiomeMgr = new BiomeManager(env);
	mOreMgr = new OreManager(env);
	mDecoMgr = new DecorationManager(env);
	mSchemMgr = new SchematicManager(env);

	// initialized later
	mMapgenParams = nullptr;
	mBiomeGen = nullptr;

	// Note that accesses to this variable are not synchronized.
	// This is because the *only* thread ever starting or stopping
	mEnableMapgenDebugInfo = Settings::Get()->GetBool("enable_mapgen_debug_info");

	short nthreads = 1;

    nthreads = Settings::Get()->GetInt16("num_emerge_threads");
	// If automatic, leave a proc for the main thread and one for
	// some other misc thread
	if (nthreads == 0)
		nthreads = Thread::GetNumberOfProcessors() - 2;
	if (nthreads < 1)
		nthreads = 1;

	mQLimitTotal = Settings::Get()->GetUInt16("emergequeue_limit_total");
	// FIXME: these fallback values are probably not good
	mQLimitDiskonly = Settings::Get()->GetUInt16("emergequeue_limit_diskonly");
	// mQLimitDiskonly = nthreads * 5 + 1; fallback
	mQLimitGenerate = Settings::Get()->GetUInt16("emergequeue_limit_generate");
	// mQLimitGenerate = nthreads + 1; fallback

	// don't trust user input for something very important like this
	if (mQLimitTotal < 1)
		mQLimitTotal = 1;
	if (mQLimitDiskonly < 1)
		mQLimitDiskonly = 1;
	if (mQLimitGenerate < 1)
		mQLimitGenerate = 1;

	for (short i = 0; i < nthreads; i++)
		mThreads.push_back(new EmergeThread(env, i));

    if (EmergeManager::mEmergeMgr)
    {
        LogError("Attempting to create two global emerge manager! \
					The old one will be destroyed and overwritten with this one.");
        delete EmergeManager::mEmergeMgr;
    }

    EmergeManager::mEmergeMgr = this;

	LogInformation("EmergeManager: using " + std::to_string(nthreads) + " threads");
}


EmergeManager::~EmergeManager()
{
	for (unsigned int i = 0; i != mThreads.size(); i++) 
    {
		EmergeThread *thread = mThreads[i];
		if (mThreadsActive) 
        {
			thread->Stop();
			thread->Signal();
			thread->Wait();
		}
		delete thread;

		// Mapgen init might not be finished if there is an error during Startup.
		if (mMapgens.size() > i)
			delete mMapgens[i];
	}

	delete mBiomeMgr;
	delete mOreMgr;
	delete mDecoMgr;
	delete mSchemMgr;


    if (EmergeManager::mEmergeMgr == this)
        EmergeManager::mEmergeMgr = nullptr;
}


BiomeManager* EmergeManager::GetWritableBiomeManager()
{
	LogAssert(mMapgens.empty(),
		"Writable managers can only be returned before mapgen init");
	return mBiomeMgr;
}

OreManager* EmergeManager::GetWritableOreManager()
{
	LogAssert(mMapgens.empty(),
		"Writable managers can only be returned before mapgen init");
	return mOreMgr;
}

DecorationManager* EmergeManager::GetWritableDecorationManager()
{
    LogAssert(mMapgens.empty(),
		"Writable managers can only be returned before mapgen init");
	return mDecoMgr;
}

SchematicManager* EmergeManager::GetWritableSchematicManager()
{
    LogAssert(mMapgens.empty(),
		"Writable managers can only be returned before mapgen init");
	return mSchemMgr;
}


void EmergeManager::InitMapGenerators(MapGeneratorParams* params)
{
    LogAssert(mMapgens.empty(), "Mapgen already initialized");

    mMapgenParams = params;

    Vector3<short> cSize = Vector3<short>{ 1, 1, 1 } * (short)(params->chunkSize * MAP_BLOCKSIZE);
    mBiomeGen = mBiomeMgr->CreateBiomeGenerator(BIOMEGEN_ORIGINAL, params->bparams, cSize);
	for (unsigned int i = 0; i != mThreads.size(); i++) 
    {
		EmergeParams* p = new EmergeParams(this, mBiomeGen, mBiomeMgr, mOreMgr, mDecoMgr, mSchemMgr);
		LogInformation("EmergeManager: Created params for thread " + std::to_string(i));
		mMapgens.push_back(MapGenerator::CreateMapGenerator(params->mgtype, params, p));
	}
}


MapGenerator* EmergeManager::GetCurrentMapGenerator()
{
	if (!mThreadsActive)
		return nullptr;

	for (unsigned int i = 0; i != mThreads.size(); i++) 
    {
		EmergeThread *t = mThreads[i];
		if (t->IsRunning() && t->IsCurrentThread())
			return t->mMapgen;
	}

	return nullptr;
}


void EmergeManager::StartThreads()
{
	if (mThreadsActive)
		return;

	for (unsigned int i = 0; i != mThreads.size(); i++)
		mThreads[i]->Start();

	mThreadsActive = true;
}


void EmergeManager::StopThreads()
{
	if (!mThreadsActive)
		return;

	// Request thread stop in parallel
	for (unsigned int i = 0; i != mThreads.size(); i++) 
    {
		mThreads[i]->Stop();
		mThreads[i]->Signal();
	}

	// Then do the waiting for each
	for (unsigned int i = 0; i != mThreads.size(); i++)
		mThreads[i]->Wait();

	mThreadsActive = false;
}


bool EmergeManager::IsRunning()
{
	return mThreadsActive;
}


bool EmergeManager::EnqueueBlockEmerge(ActorId actorId,
    Vector3<short> blockpos, bool allowGenerate, bool ignoreQueueLimits)
{
	uint16_t flags = 0;
	if (allowGenerate)
		flags |= BLOCK_EMERGE_ALLOW_GEN;
	if (ignoreQueueLimits)
		flags |= BLOCK_EMERGE_FORCE_QUEUE;

	return EnqueueBlockEmergeEx(blockpos, actorId, flags, NULL, NULL);
}


bool EmergeManager::EnqueueBlockEmergeEx(Vector3<short> blockpos, 
    ActorId actorId, uint16_t flags, EmergeCompletionCallback callback, void* callbackParam)
{
	EmergeThread *thread = NULL;
	bool entryAlreadyExists = false;

	{
		MutexAutoLock queuelock(mQueueMutex);

		if (!PushBlockEmergeData(blockpos, actorId, flags,
				callback, callbackParam, &entryAlreadyExists))
			return false;

		if (entryAlreadyExists)
			return true;

		thread = GetOptimalThread();
		thread->PushBlock(blockpos);
	}

	thread->Signal();

	return true;
}


//
// Mapgen-related helper functions
//


// TODO(hmmmm): Move this to LogicMap
Vector3<short> EmergeManager::GetContainingChunk(Vector3<short> blockpos)
{
	return GetContainingChunk(blockpos, mMapgenParams->chunkSize);
}

// TODO(hmmmm): Move this to LogicMap
Vector3<short> EmergeManager::GetContainingChunk(Vector3<short> blockpos, short chunksize)
{
	short coff = -chunksize / 2;
    Vector3<short> chunkOffset{ coff, coff, coff };

	return GetContainerPosition(blockpos - chunkOffset, chunksize) * chunksize + chunkOffset;
}


int EmergeManager::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	if (mMapgens.empty() || !mMapgens[0]) 
    {
		LogError("EmergeManager: GetSpawnLevelAtPoint() called before mapgen init");
		return 0;
	}

	return mMapgens[0]->GetSpawnLevelAtPoint(pos);
}


int EmergeManager::GetGroundLevelAtPoint(Vector2<short> pos)
{
	if (mMapgens.empty() || !mMapgens[0]) 
    {
		LogError("EmergeManager: GetGroundLevelAtPoint() called before mapgen init");
		return 0;
	}

	return mMapgens[0]->GetGroundLevelAtPoint(pos);
}

// TODO(hmmmm): Move this to LogicMap
bool EmergeManager::IsBlockUnderground(Vector3<short> blockpos)
{
	// Use a simple heuristic
	return blockpos[1] * (MAP_BLOCKSIZE + 1) <= mMapgenParams->waterLevel;
}

bool EmergeManager::PushBlockEmergeData(Vector3<short> pos, ActorId actorRequested, uint16_t flags,
	EmergeCompletionCallback callback, void* callbackParam, bool* entryAlreadyExists)
{
	uint16_t& countPeer = mActorQueueCount[actorRequested];

	if ((flags & BLOCK_EMERGE_FORCE_QUEUE) == 0) 
    {
		if (mBlocksEnqueued.size() >= mQLimitTotal)
			return false;

		if (actorRequested != INVALID_ACTOR_ID)
        {
			uint16_t qlimitPeer = (flags & BLOCK_EMERGE_ALLOW_GEN) ? mQLimitGenerate : mQLimitDiskonly;
			if (countPeer >= qlimitPeer)
				return false;
		} 
        else 
        {
			// limit block enqueue requests for active blocks to 1/2 of total
			if (countPeer * 2 >= mQLimitTotal)
				return false;
		}
	}

	std::pair<std::map<Vector3<short>, BlockEmergeData>::iterator, bool> findres;
	findres = mBlocksEnqueued.insert(std::make_pair(pos, BlockEmergeData()));

	BlockEmergeData &bedata = findres.first->second;
	*entryAlreadyExists   = !findres.second;

	if (callback)
		bedata.callbacks.emplace_back(callback, callbackParam);

	if (*entryAlreadyExists) 
    {
		bedata.flags |= flags;
	} 
    else 
    {
		bedata.flags = flags;
		bedata.actorRequested = actorRequested;

		countPeer++;
	}

	return true;
}


bool EmergeManager::PopBlockEmergeData(Vector3<short> pos, BlockEmergeData* bedata)
{
	std::map<Vector3<short>, BlockEmergeData>::iterator it;
    std::unordered_map<uint16_t, uint16_t>::iterator it2;

	it = mBlocksEnqueued.find(pos);
	if (it == mBlocksEnqueued.end())
		return false;

	*bedata = it->second;

	it2 = mActorQueueCount.find(bedata->actorRequested);
	if (it2 == mActorQueueCount.end())
		return false;

	uint16_t &countPeer = it2->second;
	LogAssert(countPeer != 0, "invalid count");
	countPeer--;

	mBlocksEnqueued.erase(it);

	return true;
}


EmergeThread* EmergeManager::GetOptimalThread()
{
	size_t nthreads = mThreads.size();

	LogAssert(nthreads, "No emerge threads!");

	size_t index = 0;
	size_t nItemsLowest = mThreads[0]->mBlockQueue.size();
	for (size_t i = 1; i < nthreads; i++) 
    {
		size_t nitems = mThreads[i]->mBlockQueue.size();
		if (nitems < nItemsLowest) 
        {
			index = i;
			nItemsLowest = nitems;
		}
	}

	return mThreads[index];
}


////
//// EmergeThread
////

EmergeThread::EmergeThread(LogicEnvironment* env, int ethreadid) : 
    mEnableMapgenDebugInfo(false), mId(ethreadid), mEnvironment(env),
	mMap(NULL), mMapgen(NULL)
{
	mName = "Emerge-" + std::to_string(ethreadid);
}


void EmergeThread::Signal()
{
	mQueueEvent.Signal();
}


bool EmergeThread::PushBlock(const Vector3<short>& pos)
{
	mBlockQueue.push(pos);
	return true;
}


void EmergeThread::CancelPendingItems()
{
	MutexAutoLock queuelock(EmergeManager::Get()->mQueueMutex);

	while (!mBlockQueue.empty()) 
    {
		BlockEmergeData bedata;
		Vector3<short> pos;

		pos = mBlockQueue.front();
		mBlockQueue.pop();

        EmergeManager::Get()->PopBlockEmergeData(pos, &bedata);

		RunCompletionCallbacks(pos, EMERGE_CANCELLED, bedata.callbacks);
	}
}


void EmergeThread::RunCompletionCallbacks(const Vector3<short>& pos, 
    EmergeAction action, const EmergeCallbackList& callbacks)
{
	for (size_t i = 0; i != callbacks.size(); i++) 
    {
		EmergeCompletionCallback callback;
		void *param;

		callback = callbacks[i].first;
		param = callbacks[i].second;

		callback(pos, action, param);
	}
}


bool EmergeThread::PopBlockEmerge(Vector3<short>* pos, BlockEmergeData* bedata)
{
	MutexAutoLock queuelock(EmergeManager::Get()->mQueueMutex);

	if (mBlockQueue.empty())
		return false;

	*pos = mBlockQueue.front();
	mBlockQueue.pop();

    EmergeManager::Get()->PopBlockEmergeData(*pos, bedata);
	return true;
}


EmergeAction EmergeThread::GetBlockOrStartGen(const Vector3<short>& pos, 
    bool allowGen, MapBlock** block, BlockMakeData* bmdata)
{
	MutexAutoLock envlock(mEnvironment->mEnvMutex);

	// 1). Attempt to fetch block from memory
	*block = mMap->GetBlockNoCreateNoEx(pos);

	if (*block && !(*block)->IsDummy()) 
    {
		if ((*block)->IsGenerated())
			return EMERGE_FROM_MEMORY;
	} 
    else 
    {
		// 2). Attempt to load block from disk if it was not in the memory
		*block = mMap->LoadBlock(pos);
		if (*block && (*block)->IsGenerated())
			return EMERGE_FROM_DISK;
	}

	// 3). Attempt to start generation
	if (allowGen && mMap->InitBlockMake(pos, bmdata))
		return EMERGE_GENERATED;

	// All attempts failed; cancel this block emerge
	return EMERGE_CANCELLED;
}


MapBlock *EmergeThread::FinishGen(Vector3<short> pos, BlockMakeData* bmdata,
	std::map<Vector3<short>, MapBlock*>* modifiedBlocks)
{
	MutexAutoLock envlock(mEnvironment->mEnvMutex);
	
    ScopeProfiler sp(Profiling, "EmergeThread: after Mapgen::makeChunk", SPT_AVG);

	/*
		Perform post-processing on blocks (invalidate lighting, queue liquid
		transforms, etc.) to finish block make
	*/
	mMap->FinishBlockMake(bmdata, modifiedBlocks);

	MapBlock* block = mMap->GetBlockNoCreateNoEx(pos);
	if (!block) 
    {
		LogWarning("EmergeThread::FinishGen: Couldn't grab block we "
			"just generated: (" + std::to_string(pos[0]) + "," +
            std::to_string(pos[1]) + "," + std::to_string(pos[2]) + ")");
		return NULL;
	}

	Vector3<short> minp = bmdata->blockPosMin * (short)MAP_BLOCKSIZE;
	Vector3<short> maxp = bmdata->blockPosMax * (short)MAP_BLOCKSIZE +
        Vector3<short>{1, 1, 1} * (short)(MAP_BLOCKSIZE - 1);

	// Ignore map edit events, they will not need to be sent
	// to anybody because the block hasn't been sent to anybody
	MapEditEventAreaIgnorer ign(
		&mEnvironment->mIgnoreMapEditEventsArea, VoxelArea(minp, maxp));

	/*
		Run on_generated callbacks
	*/
	try
    {
        BaseGame::Get()->OnGenerateEnvironment(minp, maxp, mMapgen->mBlockSeed);
	} 
    catch (BaseException &e)
    {
        LogError("FinishGen" + std::string(e.what()));
	}

	/*
		Clear generate notifier events
	*/
	mMapgen->mGenNotify.ClearEvents();

	LogInformation("ended up with: " + AnalyzeBlock(block));

	/*
		Activate the block
	*/
    mEnvironment->ActivateBlock(block, 0);

	return block;
}


void* EmergeThread::Run()
{
	Vector3<short> pos;

	mMap = mEnvironment->GetLogicMap().get();
	mMapgen = EmergeManager::Get()->mMapgens[mId];
	mEnableMapgenDebugInfo = EmergeManager::Get()->mEnableMapgenDebugInfo;

	try
    {
	    while (!StopRequested()) 
        {
		    std::map<Vector3<short>, MapBlock*> modifiedBlocks;
		    BlockEmergeData bedata;
		    BlockMakeData bmdata;
		    EmergeAction action;
		    MapBlock* block = nullptr;

		    if (!PopBlockEmerge(&pos, &bedata)) 
            {
				mQueueEvent.Wait();
			    continue;
		    }

		    if (BlockPositionOverMaxLimit(pos))
			    continue;

		    bool allowGen = bedata.flags & BLOCK_EMERGE_ALLOW_GEN;
			/*
		    LogInformation("pos=(" + std::to_string(pos[0]) + "," +
                std::to_string(pos[1]) + "," + std::to_string(pos[2]) + 
                ") allowGen=" + std::to_string(allowGen));
			*/
			action = GetBlockOrStartGen(pos, allowGen, &block, &bmdata);

		    if (action == EMERGE_GENERATED) 
            {
				ScopeProfiler sp(Profiling, "EmergeThread: Mapgen::makeChunk", SPT_AVG);
				mMapgen->MakeChunk(&bmdata);

			    block = FinishGen(pos, &bmdata, &modifiedBlocks);
		    }

		    RunCompletionCallbacks(pos, action, bedata.callbacks);

		    if (block)
			    modifiedBlocks[pos] = block;

			if (!modifiedBlocks.empty())
				mEnvironment->SetBlocksNotSent(modifiedBlocks);
	    }
	} 
    catch (VersionMismatchException &e) 
    {
		std::ostringstream err;
		err << "World data version mismatch in MapBlock (" 
            << pos[0] << ", " << pos[1] << ", " << pos[2] << ")" << std::endl
			<< "----" << std::endl
			<< "\"" << e.what() << "\"" << std::endl
			<< "See debug.txt." << std::endl
			<< "World probably saved by a newer version of minetest."
			<< std::endl;
		LogError(err.str());
	} 
    catch (SerializationError &e) 
    {
		std::ostringstream err;
		err << "Invalid data in MapBlock "
            << pos[0] << ", " << pos[1] << ", " << pos[2] << ")" << std::endl
			<< "----" << std::endl
			<< "\"" << e.what() << "\"" << std::endl
			<< "See debug.txt." << std::endl
			<< "You can ignore this using [ignore_world_load_errors = true]."
			<< std::endl;
        LogError(err.str());
	}

	return NULL;
}
