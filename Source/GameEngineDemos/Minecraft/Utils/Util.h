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

#ifndef UTIL_H
#define UTIL_H

#include "../MinecraftStd.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

#include "Graphic/Scene/Hierarchy/BoundingBox.h"


// This directly sets the range of light.
// Actually this is not the real maximum, and this is not the brightest, the
// brightest is LIGHT_SUN.
// If changed, this constant as defined in builtin/game/constants.script must
// also be changed.
#define LIGHT_MAX 14
// Light is stored as 4 bits, thus 15 is the maximum.
// This brightness is reserved for sunlight
#define LIGHT_SUN 15


#define NOISE_MAGIC_X    1619
#define NOISE_MAGIC_Y    31337
#define NOISE_MAGIC_Z    52591
#define NOISE_MAGIC_SEED 1013


/**
 * \internal
 *
 * \warning DO NOT USE this directly; it is here simply so that DecodeLight()
 * can be inlined.
 *
 * Array size is #LIGHTMAX+1
 *
 * The array is a lookup table to convert the internal representation of light
 * (brightness) to the display brightness.
 *
 */
extern const unsigned char* LightDecodeTable;

// Maximum radius of a block.  The magic number is
// sqrt(3.0) / 2.0 in literal form.
static constexpr const float BLOCK_MAX_RADIUS = 0.866025403784f * MAP_BLOCKSIZE * BS;

// 0 <= light <= LIGHT_SUN
// 0 <= return value <= 255
inline unsigned char DecodeLight(unsigned char light)
{
    // assert(light <= LIGHT_SUN);
    if (light > LIGHT_SUN)
        light = LIGHT_SUN;
    return LightDecodeTable[light];
}

// 0.0 <= light <= 1.0
// 0.0 <= return value <= 1.0
float DecodeLight(float light);

void SetLightTable(float gamma);

// 0 <= daylightFactor <= 1000
// 0 <= lightday, lightnight <= LIGHT_SUN
// 0 <= return value <= LIGHT_SUN
inline unsigned char BlendLight(unsigned int dayLightFactor, unsigned char lightDay, unsigned char lightNight)
{
    unsigned int c = 1000;
    unsigned int l = ((dayLightFactor * lightDay + (c - dayLightFactor) * lightNight)) / c;
    if (l > LIGHT_SUN)
        l = LIGHT_SUN;
    return l;
}

bool IsBlockInsight(Vector3<short> blockPos, 
    Vector3<float> cameraPos, Vector3<float> cameraDir,
    float cameraFov, float range, float* distancePtr=NULL);

short AdjustDistance(short dist, float zoomFov);

/*
    Find out the full path of an image by trying different fileName
    extensions.

    If failed, return "".

    TODO: Should probably be moved out from here, because things needing
          this function do not need anything else from this header
*/
std::string GetImagePath(std::string path);

/*
    Gets the path to a texture by first checking if the texture exists
    in texture_path and if not, using the data path.

    Checks all supported extensions by replacing the original extension.

    If not found, returns "".

    Utilizes a thread-safe cache.
*/
std::string GetTexturePath(const std::string& fileName, bool* isBasePack = nullptr);

void ClearTextureNameCache();

/*
	Returns integer position of node in given floating point position
*/
inline Vector3<short> FloatToInt(Vector3<float> pos, float d)
{
    return Vector3<short>{
       (short)((pos[0] + (pos[0] > 0 ? d / 2 : -d / 2)) / d),
       (short)((pos[1] + (pos[1] > 0 ? d / 2 : -d / 2)) / d),
       (short)((pos[2] + (pos[2] > 0 ? d / 2 : -d / 2)) / d)};
}

/*
	Returns integer position of node in given double precision position
 */
inline Vector3<short> DoubleToInt(Vector3<double> pos, double d)
{
    return Vector3<short>{
        (short)((pos[0] + (pos[0] > 0 ? d / 2 : -d / 2)) / d),
        (short)((pos[1] + (pos[1] > 0 ? d / 2 : -d / 2)) / d),
        (short)((pos[2] + (pos[2] > 0 ? d / 2 : -d / 2)) / d)};
}

/*
	Returns floating point position of node in given integer position
*/
inline Vector3<float> IntToFloat(Vector3<short> pos, float d)
{
    return Vector3<float>{ (float)pos[0] * d, (float)pos[1] * d, (float)pos[2] * d};
}

// Random helper. Usually d=BS
inline BoundingBox<float> GetNodeBox(Vector3<short> pos, float d)
{
	return BoundingBox<float>(
		(float)pos[0] * d - 0.5f * d,
		(float)pos[1] * d - 0.5f * d,
		(float)pos[2] * d - 0.5f * d,
		(float)pos[0] * d + 0.5f * d,
		(float)pos[1] * d + 0.5f * d,
		(float)pos[2] * d + 0.5f * d
	);
}

inline short GetContainerPosition(short pos, short d)
{
    return (pos >= 0 ? pos : pos - d + 1) / d;
}

inline Vector2<short> GetContainerPosition(Vector2<short> pos, short d)
{
    return Vector2<short>{
        GetContainerPosition(pos[0], d),
        GetContainerPosition(pos[1], d)};
}

inline Vector3<short> GetContainerPosition(Vector3<short> pos, short d)
{
    return Vector3<short>{
        GetContainerPosition(pos[0], d),
        GetContainerPosition(pos[1], d),
        GetContainerPosition(pos[2], d)};
}

inline Vector2<short> GetContainerPosition(Vector2<short> pos, Vector2<short> d)
{
    return Vector2<short>{
        GetContainerPosition(pos[0], d[0]),
        GetContainerPosition(pos[1], d[1])};
}

inline Vector3<short> GetContainerPosition(Vector3<short> pos, Vector3<short> d)
{
    return Vector3<short>{
        GetContainerPosition(pos[0], d[0]),
        GetContainerPosition(pos[1], d[1]),
        GetContainerPosition(pos[2], d[2])};
}

inline void GetContainerPositionWithOffset(short pos, short d, short &container, short &offset)
{
    container = (pos >= 0 ? pos : pos - d + 1) / d;
    offset = pos & (d - 1);
}

inline void GetContainerPositionWithOffset(const Vector2<short>& pos, short d,
    Vector2<short>& container, Vector2<short>& offset)
{
    GetContainerPositionWithOffset(pos[0], d, container[0], offset[0]);
    GetContainerPositionWithOffset(pos[1], d, container[1], offset[1]);
}

inline void GetContainerPositionWithOffset(const Vector3<short>& pos, short d,
    Vector3<short>& container, Vector3<short>& offset)
{
    GetContainerPositionWithOffset(pos[0], d, container[0], offset[0]);
    GetContainerPositionWithOffset(pos[1], d, container[1], offset[1]);
    GetContainerPositionWithOffset(pos[2], d, container[2], offset[2]);
}

inline bool IsInArea(Vector3<short> pos, short d)
{
    return (
        pos[0] >= 0 && pos[0] < d &&
        pos[1] >= 0 && pos[1] < d &&
        pos[2] >= 0 && pos[2] < d);
}

inline bool IsInArea(Vector2<short> pos, short d)
{
    return (
        pos[0] >= 0 && pos[0] < d &&
        pos[1] >= 0 && pos[1] < d);
}

inline bool IsInArea(Vector3<short> pos, Vector3<short> d)
{
    return (
        pos[0] >= 0 && pos[0] < d[0] &&
        pos[1] >= 0 && pos[1] < d[1] &&
        pos[2] >= 0 && pos[2] < d[2]);
}

inline void SortBoxVertices(Vector3<short>& p1, Vector3<short>& p2)
{
    if (p1[0] > p2[0])
        std::swap(p1[0], p2[0]);
    if (p1[1] > p2[1])
        std::swap(p1[1], p2[1]);
    if (p1[2] > p2[2])
        std::swap(p1[2], p2[2]);
}

inline Vector3<short> ComponentWiseMin(const Vector3<short>& a, const Vector3<short>& b)
{
    return Vector3<short>{
        std::min(a[0], b[0]), std::min(a[1], b[1]), std::min(a[2], b[2])};
}

inline Vector3<short> ComponentWiseMax(const Vector3<short>& a, const Vector3<short>& b)
{
    return Vector3<short>{
        std::max(a[0], b[0]), std::max(a[1], b[1]), std::max(a[2], b[2])};
}


/** Returns \p f wrapped to the range [-360, 360]
 *
 *  See test.cpp for example cases.
 *
 *  \note This is also used in cases where degrees wrapped to the range [0, 360]
 *  is innapropriate (e.g. pitch needs negative values)
 *
 *  \internal functionally equivalent -- although precision may vary slightly --
 *  to fmodf((f), 360.0f) however empirical tests indicate that this approach is
 *  faster.
 */
inline float Modulo360(float f)
{
    int sign;
    int whole;
    float fraction;

    if (f < 0)
    {
        f = -f;
        sign = -1;
    }
    else
    {
        sign = 1;
    }

    whole = (int)f;

    fraction = f - whole;
    whole %= 360;

    return sign * (whole + fraction);
}


/** Returns \p f wrapped to the range [0, 360]
  */
inline float WrapDegrees360(float f)
{
    float value = Modulo360(f);
    return value < 0 ? value + 360 : value;
}


/** Returns \p Vector3<float> wrapped to the range [0, 360]
  */
inline Vector3<float> WrapDegrees360(Vector3<float> v)
{
    Vector3<float> value{ Modulo360(v[0]), Modulo360(v[1]), Modulo360(v[2]) };

    // Now that values are wrapped, use to get values for certain ranges
    value[0] = value[0] < 0 ? value[0] + 360 : value[0];
    value[1] = value[1] < 0 ? value[1] + 360 : value[1];
    value[2] = value[2] < 0 ? value[2] + 360 : value[2];
    return value;
}


/** Returns \p f wrapped to the range [-180, 180]
  */
inline float WrapDegrees180(float f)
{
    float value = Modulo360(f + 180);
    if (value < 0)
        value += 360;
    return value - 180;
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
inline void Paging(unsigned int length, unsigned int page, unsigned int pagecount, 
    unsigned int& minindex, unsigned int& maxindex)
{
	if (length < 1 || pagecount < 1 || page < 1 || page > pagecount) 
    {
		// Special cases or invalid parameters
		minindex = maxindex = 0;
	} 
    else if(pagecount <= length) 
    {
		// Less pages than entries in the list:
		// Each page contains at least one entry
		minindex = (length * (page-1) + (pagecount-1)) / pagecount;
		maxindex = (length * page + (pagecount-1)) / pagecount;
	} 
    else 
    {
		// More pages than entries in the list:
		// Make sure the empty pages are at the end
		if (page < length) 
        {
			minindex = page-1;
			maxindex = page;
		} 
        else 
        {
			minindex = 0;
			maxindex = 0;
		}
	}
}

inline float CycleShift(float value, float by = 0, float max = 1)
{
    if (value + by < 0)   return value + by + max;
    if (value + by > max) return value + by - max;
    return value + by;
}


// Gradual steps towards the target value in a wrapped (circular) system
// using the shorter of both ways
template<typename T>
inline void WrappedApproachShortest(T& current, const T target, const T stepsize, const T maximum)
{
	T delta = target - current;
	if (delta < 0)
		delta += maximum;

	if (delta > stepsize && maximum - delta > stepsize) 
    {
		current += (delta < maximum / 2) ? stepsize : -stepsize;
		if (current >= maximum)
			current -= maximum;
	} 
    else current = target;
}

template<typename K, typename V>
class LRUCache
{
public:
    LRUCache(size_t limit, void (*cacheMiss)(void* data, const K& key, V* dest), void* data)
    {
        mLimit = limit;
        mCacheMiss = cacheMiss;
        mCacheMissData = data;
    }

    void SetLimit(size_t limit)
    {
        mLimit = limit;
        Invalidate();
    }

    void Invalidate()
    {
        mMap.clear();
        mQueue.clear();
    }

    const V* LookupCache(K key)
    {
        typename CacheType::iterator it = mMap.find(key);
        V* ret;
        if (it != mMap.end()) 
        {
            // found!
            CacheEntryType& entry = it->second;

            ret = &entry.second;

            // update the usage information
            mQueue.erase(entry.first);
            mQueue.push_front(key);
            entry.first = mQueue.begin();
        }
        else 
        {
            // cache miss -- enter into cache
            CacheEntryType& entry = mMap[key];
            ret = &entry.second;
            mCacheMiss(mCacheMissData, key, &entry.second);

            // delete old entries
            if (mQueue.size() == mLimit) 
            {
                const K& id = mQueue.back();
                mMap.erase(id);
                mQueue.pop_back();
            }

            mQueue.push_front(key);
            entry.first = mQueue.begin();
        }
        return ret;
    }
private:
    void (*mCacheMiss)(void* data, const K& key, V* dest);
    void* mCacheMissData;
    size_t mLimit;
    typedef typename std::template pair<typename std::template list<K>::iterator, V> CacheEntryType;
    typedef std::template map<K, CacheEntryType> CacheType;
    CacheType mMap;
    // we can't use std::deque here, because its iterators get invalidated
    std::list<K> mQueue;
};


#endif