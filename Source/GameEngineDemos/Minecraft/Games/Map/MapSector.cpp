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

#include "MapSector.h"
#include "MapBlock.h"

MapSector::MapSector(Map* parent, Vector2<short> pos) :
    mParent(parent), mPosition(pos)
{
}

MapSector::~MapSector()
{
	DeleteBlocks();
}

void MapSector::DeleteBlocks()
{
	// Clear cache
	mBlockCache = nullptr;

	// Delete all
	for (auto &block : mBlocks)
		delete block.second;

	// Clear container
	mBlocks.clear();
}

MapBlock* MapSector::GetBlockBuffered(short y)
{
	MapBlock* block;

	if (mBlockCache && y == mBlockCacheY)
		return mBlockCache;

	// If block doesn't exist, return NULL
	std::unordered_map<short, MapBlock*>::const_iterator n = mBlocks.find(y);
	block = (n != mBlocks.end() ? n->second : nullptr);

	// Cache the last result
	mBlockCacheY = y;
	mBlockCache = block;

	return block;
}

MapBlock* MapSector::GetBlockNoCreateNoEx(short y)
{
	return GetBlockBuffered(y);
}

MapBlock* MapSector::CreateBlankBlockNoInsert(short y, Environment* env)
{
	LogAssert(GetBlockBuffered(y) == NULL, "invalid block");	// Pre-condition

    Vector3<short> blockPosMap{ mPosition[0], y, mPosition[1] };
	MapBlock* block = new MapBlock(mParent, env, blockPosMap);
	return block;
}

MapBlock* MapSector::CreateBlankBlock(short y, Environment* env)
{
	MapBlock* block = CreateBlankBlockNoInsert(y, env);

	mBlocks[y] = block;
	return block;
}

void MapSector::InsertBlock(MapBlock* block)
{
	short blockY = block->GetPosition()[1];

	MapBlock* block2 = GetBlockBuffered(blockY);
	if (block2) 
    {
		throw AlreadyExistsException("Block already exists");
	}

    Vector2<short> p2d{ block->GetPosition()[0], block->GetPosition()[2] };
	LogAssert(p2d == mPosition, "invalid map position");

	// Insert into container
	mBlocks[blockY] = block;
}

void MapSector::DeleteBlock(MapBlock* block)
{
	short blockY = block->GetPosition()[1];

	// Clear from cache
	mBlockCache = nullptr;

	// Remove from container
	mBlocks.erase(blockY);

	// Delete
	delete block;
}

void MapSector::GetBlocks(MapBlockVec& dest)
{
	for (auto& block : mBlocks)
		dest.push_back(block.second);
}
