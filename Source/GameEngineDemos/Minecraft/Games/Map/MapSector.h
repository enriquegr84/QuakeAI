/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef MAPSECTOR_H
#define MAPSECTOR_H

#include "GameEngineStd.h"

#include "MapBlock.h"

#include "Mathematic/Algebra/Vector3.h"

class Map;
class Environment;

/*
	This is an Y-wise stack of MapBlocks.
*/

#define MAPSECTOR_LOGIC 0
#define MAPSECTOR_VISUAL 1

class MapSector
{
public:

	MapSector(Map* parent, Vector2<short> pos);
	virtual ~MapSector();

	void DeleteBlocks();

	Vector2<short> GetPosition()
	{
		return mPosition;
	}

	MapBlock* GetBlockNoCreateNoEx(short y);
	MapBlock* CreateBlankBlockNoInsert(short y, Environment* env);
	MapBlock* CreateBlankBlock(short y, Environment* env);

	void InsertBlock(MapBlock* block);

	void DeleteBlock(MapBlock* block);

	void GetBlocks(MapBlockVec& dest);

	bool Empty() const { return mBlocks.empty(); }

	unsigned int Size() const { return (unsigned int)mBlocks.size(); }

protected:
    /*
    Private methods
    */
    MapBlock* GetBlockBuffered(short y);

	// The pile of MapBlocks
	std::unordered_map<short, MapBlock*> mBlocks;

	Map* mParent;
	// Position on parent (in MapBlock widths)
	Vector2<short> mPosition;

	// Last-used block is cached here for quicker access.
	// Be sure to set this to nullptr when the cached block is deleted
	MapBlock* mBlockCache = nullptr;
	short mBlockCacheY;

};


#endif