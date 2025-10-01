/*
Minetest
Copyright (C) 2013-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2015-2018 paramat

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

#ifndef MAPGENERATORSINGLENODE_H
#define MAPGENERATORSINGLENODE_H

#include "MapGenerator.h"

struct MapGeneratorSinglenodeParams : public MapGeneratorParams
{
	MapGeneratorSinglenodeParams() = default;
	~MapGeneratorSinglenodeParams() = default;

	void ReadParams(const Settings* settings) {}
	void WriteParams(Settings* settings) const {}
};

class MapGeneratorSinglenode : public MapGenerator
{
public:
	uint16_t mContentNode;
	unsigned char mSetLight;

	MapGeneratorSinglenode(MapGeneratorParams* params, EmergeParams* emerge);
	~MapGeneratorSinglenode() = default;

	virtual MapGeneratorType GetType() const { return MAPGEN_SINGLENODE; }

	void MakeChunk(BlockMakeData* data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);
};

#endif