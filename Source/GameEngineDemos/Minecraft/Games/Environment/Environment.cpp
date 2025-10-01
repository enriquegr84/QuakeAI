/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "Application/Settings.h"

#include "Environment.h"

#include "../Map/Map.h"

#include "../../Physics/Collision.h"
#include "../../Physics/Raycast.h"

#include "Core/Threading/MutexAutolock.h"


float TimeToDaynightRatio(float timeOfDay, bool smooth)
{
    float time = timeOfDay;
    if (time < 0.0f)
        time += ((int)(-time) / 24000) * 24000.0f;
    if (time >= 24000.0f)
        time -= ((int)(time) / 24000) * 24000.0f;
    if (time > 12000.0f)
        time = 24000.0f - time;

    const float values[9][2] = {
        {4250.0f + 125.0f, 175.0f},
        {4500.0f + 125.0f, 175.0f},
        {4750.0f + 125.0f, 250.0f},
        {5000.0f + 125.0f, 350.0f},
        {5250.0f + 125.0f, 500.0f},
        {5500.0f + 125.0f, 675.0f},
        {5750.0f + 125.0f, 875.0f},
        {6000.0f + 125.0f, 1000.0f},
        {6250.0f + 125.0f, 1000.0f},
    };

    if (!smooth) 
    {
        float lastTime = values[0][0];
        for (unsigned int i = 1; i < 9; i++) 
        {
            float t0 = values[i][0];
            float switchTime = (t0 + lastTime) / 2.0f;
            lastTime = t0;
            if (switchTime <= time)
                continue;

            return values[i][1];
        }
        return 1000.f;
    }

    if (time <= 4625.0f) // 4500 + 125
        return values[0][1];
    else if (time >= 6125.0f) // 6000 + 125
        return 1000.f;

    for (unsigned int i = 0; i < 9; i++) 
    {
        if (values[i][0] <= time)
            continue;

        float td0 = values[i][0] - values[i - 1][0];
        float f = (time - values[i - 1][0]) / td0;
        return f * values[i][1] + (1.0f - f) * values[i - 1][1];
    }
    return 1000.f;
}


Environment::Environment() : mTimeOfDaySpeed(0.0f), mDayCount(0)
{
	mCacheEnableShaders = Settings::Get()->GetBool("enable_shaders");
	mCacheActiveBlockMgmtInterval = Settings::Get()->GetFloat("active_block_mgmt_interval");
	mCacheAbmInterval = Settings::Get()->GetFloat("abm_interval");
	mCacheNodetimerInterval = Settings::Get()->GetFloat("nodetimer_interval");
	mCacheAbmTimeBudget = Settings::Get()->GetFloat("abm_time_budget");

	mTimeOfDay = Settings::Get()->GetUInt("world_start_time");
	mTimeOfDayFloat = (float)mTimeOfDay / 24000.0f;
}

unsigned int Environment::GetDayNightRatio()
{
    MutexAutoLock lock(this->mTimeLock);
	if (mEnableDayNightRatioOverride)
		return mDayNightRatioOverride;
	return (unsigned int)TimeToDaynightRatio(mTimeOfDayFloat * 24000, mCacheEnableShaders);
}

void Environment::SetTimeOfDaySpeed(float speed)
{
	mTimeOfDaySpeed = speed;
}

void Environment::SetDayNightRatioOverride(bool enable, unsigned int value)
{
    MutexAutoLock lock(this->mTimeLock);
	mEnableDayNightRatioOverride = enable;
	mDayNightRatioOverride = value;
}

void Environment::SetTimeOfDay(unsigned int time)
{
    MutexAutoLock lock(this->mTimeLock);
	if (mTimeOfDay > time)
		++mDayCount;
	mTimeOfDay = time;
	mTimeOfDayFloat = (float)time / 24000.f;
}

unsigned int Environment::GetTimeOfDay()
{
    MutexAutoLock lock(this->mTimeLock);
	return mTimeOfDay;
}

float Environment::GetTimeOfDayFloat()
{
    MutexAutoLock lock(this->mTimeLock);
	return mTimeOfDayFloat;
}

bool Environment::LineOfSight(Vector3<float> pos1, Vector3<float> pos2, Vector3<short>* p)
{
	// Iterate trough nodes on the line
	VoxelLineIterator iterator(pos1 / BS, (pos2 - pos1) / BS);
	do 
    {
		MapNode node = GetMap()->GetNode(iterator.mCurrentNodePos);

		// Return non-air
		if (node.param0 != CONTENT_AIR)
        {
			if (p)
				*p = iterator.mCurrentNodePos;
			return false;
		}
		iterator.Next();
	} 
    while (iterator.mCurrentIndex <= iterator.mLastIndex);
	
    return true;
}

/*
	Check if a node is pointable
*/
inline static bool IsPointableNode(const MapNode& n, const NodeManager* nodeMgr , bool liquidsPointable)
{
	const ContentFeatures& features = nodeMgr->Get(n);
	return features.pointable || (liquidsPointable && features.IsLiquid());
}

void Environment::ContinueRaycast(RaycastState* state, PointedThing* result)
{
	const NodeManager* nodeMgr = GetNodeManager();
	if (state->mInitializationNeeded) 
    {
		// Add objects
		if (state->mObjectsPointable) 
        {
			std::vector<PointedThing> found;
			GetSelectedActiveObjects(state->mShootLine, found);
			for (const PointedThing& pointed : found)
				state->mFoundThings.push(pointed);
		}
		// Set search range
		BoundingBox<short> maximalExceed = nodeMgr->GetSelectionBoxIntUnion();
		state->mSearchRange.mMinEdge = -maximalExceed.mMaxEdge;
		state->mSearchRange.mMaxEdge = -maximalExceed.mMinEdge;
		// Setting is done
		state->mInitializationNeeded = false;
	}

	// The index of the first pointed thing that was not returned
	// before. The last index which needs to be tested.
	short lastIndex = state->mIterator.mLastIndex;
	if (!state->mFoundThings.empty()) 
    {
        Vector3<float> intersection = state->mFoundThings.top().intersectionPoint;
        Vector3<short> intersectionPoint;
        intersectionPoint[0] = (short)((intersection[0] + (intersection[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        intersectionPoint[1] = (short)((intersection[1] + (intersection[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        intersectionPoint[2] = (short)((intersection[2] + (intersection[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		lastIndex = state->mIterator.GetIndex(intersectionPoint);
	}

	std::shared_ptr<Map> map = GetMap();
	// If a node is found, this is the center of the
	// first nodebox the shootline meets.
    Vector3<float> foundBoxCenter{ 0, 0, 0 };
	// The untested nodes are in this range.
    BoundingBox<short> newNodes;
	while (state->mIterator.mCurrentIndex <= lastIndex) 
    {
		// Test the nodes around the current node in search_range.
		newNodes = state->mSearchRange;
		newNodes.mMinEdge += state->mIterator.mCurrentNodePos;
		newNodes.mMaxEdge += state->mIterator.mCurrentNodePos;

		// Only check new nodes
		Vector3<short> delta = state->mIterator.mCurrentNodePos - state->mPreviousNode;
		if (delta[0] > 0)
			newNodes.mMinEdge[0] = newNodes.mMaxEdge[0];
        else if (delta[0] < 0)
			newNodes.mMaxEdge[0] = newNodes.mMinEdge[0];
        else if (delta[1] > 0)
			newNodes.mMinEdge[1] = newNodes.mMaxEdge[1];
        else if (delta[1] < 0)
			newNodes.mMaxEdge[1] = newNodes.mMinEdge[1];
        else if (delta[2] > 0)
			newNodes.mMinEdge[2] = newNodes.mMaxEdge[2];
        else if (delta[2] < 0)
			newNodes.mMaxEdge[2] = newNodes.mMinEdge[2];

		// For each untested node
        for (short x = newNodes.mMinEdge[0]; x <= newNodes.mMaxEdge[0]; x++)
        {
            for (short y = newNodes.mMinEdge[1]; y <= newNodes.mMaxEdge[1]; y++)
            {
                for (short z = newNodes.mMinEdge[2]; z <= newNodes.mMaxEdge[2]; z++)
                {
                    MapNode node;
                    Vector3<short> nodePos{ x, y, z };
                    bool isValidPosition;
                    node = map->GetNode(nodePos, &isValidPosition);
                    if (!(isValidPosition && IsPointableNode(node, nodeMgr, state->mLiquidsPointable)))
                        continue;

                    PointedThing result;
                    std::vector<BoundingBox<float>> boxes;
                    node.GetSelectionBoxes(nodeMgr, &boxes, node.GetNeighbors(nodePos, map));

                    // Is there a collision with a selection box?
                    bool isColliding = false;
                    // Minimal distance of all collisions
                    float minDistanceSq = 10000000;
                    // ID of the current box (loop counter)
                    unsigned short id = 0;

                    Vector3<float> nPos = 
                        Vector3<float>{ (float)nodePos[0], (float)nodePos[1], (float)nodePos[2] } * BS;
                    // This loop translates the boxes to their in-world place.
                    for (BoundingBox<float>& box : boxes) 
                    {
                        box.mMinEdge += nPos;
                        box.mMaxEdge += nPos;

                        Vector3<float> intersectionPoint;
                        Vector3<short> intersectionNormal;
                        if (!BoxLineCollision(box, state->mShootLine.mStart, 
                            state->mShootLine.GetVector(), &intersectionPoint, &intersectionNormal)) 
                        {
                            ++id;
                            continue;
                        }

                        float distanceSq = LengthSq(intersectionPoint - state->mShootLine.mStart);
                        // If this is the nearest collision, save it
                        if (minDistanceSq > distanceSq) 
                        {
                            minDistanceSq = distanceSq;
                            result.intersectionPoint = intersectionPoint;
                            result.intersectionNormal = intersectionNormal;
                            result.boxId = id;
                            foundBoxCenter = box.GetCenter();
                            isColliding = true;
                        }
                        ++id;
                    }
                    // If there wasn't a collision, stop
                    if (!isColliding)
                        continue;

                    result.type = POINTEDTHING_NODE;
                    result.nodeUndersurface = nodePos;
                    result.distanceSq = minDistanceSq;
                    // Set undersurface and abovesurface nodes
                    float d = 0.002f * BS;
                    Vector3<float> fakeIntersection = result.intersectionPoint;
                    // Move intersection towards its source block.
                    if (fakeIntersection[0] < foundBoxCenter[0])
                        fakeIntersection[0] += d;
                    else
                        fakeIntersection[0] -= d;

                    if (fakeIntersection[1] < foundBoxCenter[1])
                        fakeIntersection[1] += d;
                    else
                        fakeIntersection[1] -= d;

                    if (fakeIntersection[2] < foundBoxCenter[2])
                        fakeIntersection[2] += d;
                    else
                        fakeIntersection[2] -= d;

                    result.nodeRealUndersurface[0] = 
                        (short)((fakeIntersection[0] + (fakeIntersection[0] > 0 ? BS / 2 : -BS / 2)) / BS);
                    result.nodeRealUndersurface[1] = 
                        (short)((fakeIntersection[1] + (fakeIntersection[1] > 0 ? BS / 2 : -BS / 2)) / BS);
                    result.nodeRealUndersurface[2] = 
                        (short)((fakeIntersection[2] + (fakeIntersection[2] > 0 ? BS / 2 : -BS / 2)) / BS);

                    result.nodeAbovesurface = result.nodeRealUndersurface + result.intersectionNormal;
                    // Push found PointedThing
                    state->mFoundThings.push(result);
                    // If this is nearer than the old nearest object,
                    // the search can be shorter
                    short newIndex = state->mIterator.GetIndex(result.nodeRealUndersurface);
                    if (newIndex < lastIndex)
                        lastIndex = newIndex;
                }
            }
        }
		// Next node
		state->mPreviousNode = state->mIterator.mCurrentNodePos;
		state->mIterator.Next();
	}
	// Return empty PointedThing if nothing left on the ray
	if (state->mFoundThings.empty()) 
    {
		result->type = POINTEDTHING_NOTHING;
	} 
    else 
    {
		*result = state->mFoundThings.top();
		state->mFoundThings.pop();
	}
}

void Environment::StepTimeOfDay(float dTime)
{
    MutexAutoLock lock(this->mTimeLock);

	// Cached in order to prevent the two reads we do to give
	// different results (can be written by code not under the lock)
	float cachedTimeOfDaySpeed = mTimeOfDaySpeed;

	float speed = cachedTimeOfDaySpeed * 24000.f / (24.f * 3600);
	mTimeConversionSkew += dTime;
	unsigned int units = (unsigned int)(mTimeConversionSkew * speed);
	bool syncFloat = false;
	if (units > 0) 
    {
		// Sync at overflow
		if (mTimeOfDay + units >= 24000) 
        {
			syncFloat = true;
			++mDayCount;
		}
		mTimeOfDay = (mTimeOfDay + units) % 24000;
		if (syncFloat)
			mTimeOfDayFloat = (float)mTimeOfDay / 24000.f;
	}
	if (speed > 0)
		mTimeConversionSkew -= (float)units / speed;

	if (!syncFloat) 
    {
		mTimeOfDayFloat += cachedTimeOfDaySpeed / 24 / 3600 * dTime;
		if (mTimeOfDayFloat > 1.0)
			mTimeOfDayFloat -= 1.0;
		if (mTimeOfDayFloat < 0.0)
			mTimeOfDayFloat += 1.0;
	}
}

unsigned int Environment::GetDayCount()
{
	// Atomic<unsigned int> counter
	return mDayCount;
}
