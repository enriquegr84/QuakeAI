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

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

/*
	This class is the game's environment.
	It contains:
	- The map
	- Players
	- Other objects
	- The current time in the game
	- etc.
*/

#include "GameEngineStd.h"

#include "Mathematic/Geometric/Line3.h"

#include <mutex>

class Map;
struct PointedThing;
class RaycastState;
class NodeManager;
class BaseItemManager;
class BaseCraftManager;

class Environment
{
public:
	// Environment will delete the map passed to the constructor
	Environment();
	virtual ~Environment() = default;

	/*
		Step everything in environment.
		- Move players
		- Step mobs
		- Run timers of map
	*/
	virtual void Step(float dTime) = 0;

	virtual std::shared_ptr<Map> GetMap() = 0;

	unsigned int GetDayNightRatio();

	// 0-23999
	virtual void SetTimeOfDay(unsigned int time);
	unsigned int GetTimeOfDay();
	float GetTimeOfDayFloat();

	void StepTimeOfDay(float dTime);
	void SetTimeOfDaySpeed(float speed);

	void SetDayNightRatioOverride(bool enable, unsigned int value);

	unsigned int GetDayCount();

	/*!
	 * Returns false if the given line intersects with a
	 * non-air node, true otherwise.
	 * \param pos1 start of the line
	 * \param pos2 end of the line
	 * \param p output, position of the first non-air node
	 * the line intersects
	 */
	bool LineOfSight(Vector3<float> pos1, Vector3<float> pos2, Vector3<short>* p = nullptr);

	/*!
	 * Gets the objects pointed by the shootline as
	 * pointed things.
	 * If this is a visual environment, the local player
	 * won't be returned.
	 * @param[in]  shootlineOnMap the shootline for
	 * the test in world coordinates
	 *
	 * @param[out] objects          found objects
	 */
	virtual void GetSelectedActiveObjects(
        const Line3<float>& shootlineOnMap, std::vector<PointedThing>& objects) = 0;

	/*!
	 * Returns the next node or object the shootline meets.
	 * @param state current state of the raycast
	 * @result output, will contain the next pointed thing
	 */
	void ContinueRaycast(RaycastState* state, PointedThing* result);

	// Node manager
	virtual NodeManager* GetNodeManager() = 0;

	// Item manager
	virtual BaseItemManager* GetItemManager() = 0;

	// Craft manager
	virtual BaseCraftManager* GetCraftManager() = 0;

	// counter used internally when triggering ABMs
	unsigned int mAddedObjects;

protected:
    
    float mTimeOfDaySpeed;

	/*
	 * Below: values managed by mTimeLock
	 */
	// Time of day in milli-hours (0-23999), determines day and night
	unsigned int mTimeOfDay;
	// Time of day in 0...1
	float mTimeOfDayFloat;
	// Stores the skew created by the float -> unsigned int conversion
	// to be applied at next conversion, so that there is no real skew.
	float mTimeConversionSkew = 0.0f;
	// Overriding the day-night ratio is useful for custom sky visuals
	bool mEnableDayNightRatioOverride = false;
	unsigned int mDayNightRatioOverride = 0;
	// Days from the logic start, accounts for time shift
	// in game (e.g. /time or bed usage)
	unsigned int mDayCount;
	/*
	 * Above: values managed by mTimeLock
	 */

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change logic settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool mCacheEnableShaders;
	float mCacheActiveBlockMgmtInterval;
	float mCacheAbmInterval;
	float mCacheNodetimerInterval;
	float mCacheAbmTimeBudget;

private:
	std::mutex mTimeLock;
};

#endif