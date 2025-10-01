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

#ifndef FACEPOSITIONCACHE_H
#define FACEPOSITIONCACHE_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector3.h"

#include <mutex>


/*
 * This class permits caching getFacePosition call results.
 * This reduces CPU usage and vector calls.
 */
class FacePositionCache 
{
public:
	static const std::vector<Vector3<short>>& GetFacePositions(uint16_t d);

private:
	static const std::vector<Vector3<short>>& GenerateFacePosition(uint16_t d);
	static std::unordered_map<uint16_t, std::vector<Vector3<short>>> mCache;
	static std::mutex mCacheMutex;
};

#endif