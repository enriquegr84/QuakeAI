/*
Minetest
Copyright (C) 2015 Nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#include "FacePositionCache.h"

#include "Core/Threading/MutexAutolock.h"

std::unordered_map<uint16_t, std::vector<Vector3<short>>> FacePositionCache::mCache;
std::mutex FacePositionCache::mCacheMutex;

// Calculate the borders of a "d-radius" cube
const std::vector<Vector3<short>>& FacePositionCache::GetFacePositions(uint16_t d)
{
	MutexAutoLock lock(mCacheMutex);
	std::unordered_map<uint16_t, std::vector<Vector3<short>>>::const_iterator it = mCache.find(d);
	if (it != mCache.end())
		return it->second;

	return GenerateFacePosition(d);
}

const std::vector<Vector3<short>>& FacePositionCache::GenerateFacePosition(uint16_t d)
{
	mCache[d] = std::vector<Vector3<short>>();
	std::vector<Vector3<short>>& cache = mCache[d];
	if (d == 0) 
    {
		cache.emplace_back(Vector3<short>::Zero());
		return cache;
	}
	if (d == 1) 
    {
		// This is an optimized sequence of coordinates.
        cache.emplace_back(Vector3<short>{ 0, 1, 0 }); // Top
        cache.emplace_back(Vector3<short>{ 0, 0, 1 }); // Back
        cache.emplace_back(Vector3<short>{ -1, 0, 0 }); // Left
        cache.emplace_back(Vector3<short>{ 1, 0, 0 }); // Right
        cache.emplace_back(Vector3<short>{ 0, 0,-1 }); // Front
        cache.emplace_back(Vector3<short>{ 0,-1, 0 }); // Bottom
		// 6
        cache.emplace_back(Vector3<short>{ -1, 0, 1 }); // Back left
        cache.emplace_back(Vector3<short>{ 1, 0, 1 }); // Back right
        cache.emplace_back(Vector3<short>{ -1, 0,-1 }); // Front left
        cache.emplace_back(Vector3<short>{ 1, 0,-1 }); // Front right
        cache.emplace_back(Vector3<short>{ -1,-1, 0 }); // Bottom left
        cache.emplace_back(Vector3<short>{ 1,-1, 0 }); // Bottom right
        cache.emplace_back(Vector3<short>{ 0,-1, 1 }); // Bottom back
        cache.emplace_back(Vector3<short>{ 0,-1,-1 }); // Bottom front
        cache.emplace_back(Vector3<short>{ -1, 1, 0 }); // Top left
        cache.emplace_back(Vector3<short>{ 1, 1, 0 }); // Top right
        cache.emplace_back(Vector3<short>{ 0, 1, 1 }); // Top back
        cache.emplace_back(Vector3<short>{ 0, 1,-1 }); // Top front
		// 18
        cache.emplace_back(Vector3<short>{ -1, 1, 1 }); // Top back-left
        cache.emplace_back(Vector3<short>{ 1, 1, 1 }); // Top back-right
        cache.emplace_back(Vector3<short>{ -1, 1,-1 }); // Top front-left
        cache.emplace_back(Vector3<short>{ 1, 1,-1 }); // Top front-right
        cache.emplace_back(Vector3<short>{ -1,-1, 1 }); // Bottom back-left
        cache.emplace_back(Vector3<short>{ 1,-1, 1 }); // Bottom back-right
        cache.emplace_back(Vector3<short>{ -1,-1,-1 }); // Bottom front-left
        cache.emplace_back(Vector3<short>{ 1,-1,-1 }); // Bottom front-right
		// 26
		return cache;
	}

	// Take blocks in all sides, starting from y=0 and going +-y
	for (int16_t y = 0; y <= d - 1; y++) 
    {
		// Left and right side, including borders
		for (int16_t z =- d; z <= d; z++) 
        {
            cache.emplace_back(Vector3<short>{(short)d, y, z});
            cache.emplace_back(Vector3<short>{(short)-d, y, z});
			if (y != 0) 
            {
                cache.emplace_back(Vector3<short>{(short)d, (short)-y, z});
                cache.emplace_back(Vector3<short>{(short)-d, (short)-y, z});
			}
		}
		// Back and front side, excluding borders
		for (int16_t x = -d + 1; x <= d - 1; x++) 
        {
            cache.emplace_back(Vector3<short>{x, y, (short)d});
            cache.emplace_back(Vector3<short>{x, y, (short)-d});
			if (y != 0) 
            {
                cache.emplace_back(Vector3<short>{x, (short)-y, (short)d});
                cache.emplace_back(Vector3<short>{x, (short)-y, (short)-d});
			}
		}
	}

	// Take the bottom and top face with borders
	// -d < x < d, y = +-d, -d < z < d
    for (int16_t x = -d; x <= d; x++)
    {
        for (int16_t z = -d; z <= d; z++)
        {
            cache.emplace_back(Vector3<short>{x, (short)-d, z});
            cache.emplace_back(Vector3<short>{x, (short)d, z});
        }
    }
	return cache;
}
