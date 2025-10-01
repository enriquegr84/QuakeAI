/*
Minetest
Copyright (C) 2016 MillersMan <millersman@users.noreply.github.com>

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

#include "Reflowscan.h"

#include "Map.h"
#include "MapBlock.h"

#include "../../Graphics/Node.h"

#include "Core/OS/OS.h"


ReflowScan::ReflowScan(Map* map, const NodeManager* nodeMgr) :
	mMap(map), mNodeMgr(nodeMgr)
{

}

void ReflowScan::Scan(MapBlock* block, std::queue<Vector3<short>>* liquidQueue)
{
	mBlockPos = block->GetPosition();
	mRelBlockPos = block->GetRelativePosition();
	mLiquidQueue = liquidQueue;

	// Prepare the lookup which is a 3x3x3 array of the blocks surrounding the
	// scanned block. Blocks are only added to the lookup if they are really
	// needed. The lookup is indexed manually to use the same index in a
	// bit-array (of uint32 type) which stores for unloaded blocks that they
	// were already fetched from Map but were actually nullptr.
	memset(mLookup, 0, sizeof(mLookup));
	int block_idx = 1 + (1 * 9) + (1 * 3);
	mLookup[block_idx] = block;
	mLookupStateBitset = 1 << block_idx;

	// Scan the columns in the block
    for (short z = 0; z < MAP_BLOCKSIZE; z++)
    {
        for (short x = 0; x < MAP_BLOCKSIZE; x++)
        {
            ScanColumn(x, z);
        }
    }

	// Scan neighbouring columns from the nearby blocks as they might contain
	// liquid nodes that weren't allowed to flow to prevent gaps.
	for (short i = 0; i < MAP_BLOCKSIZE; i++) 
    {
		ScanColumn(i, -1);
		ScanColumn(i, MAP_BLOCKSIZE);
		ScanColumn(-1, i);
		ScanColumn(MAP_BLOCKSIZE, i);
	}
}

inline MapBlock *ReflowScan::LookupBlock(int x, int y, int z)
{
	// Gets the block that contains (x,y,z) relativ to the scanned block.
	// This uses a lookup as there might be many lookups into the same
	// neighbouring block which makes fetches from Map costly.
	int bx = (MAP_BLOCKSIZE + x) / MAP_BLOCKSIZE;
	int by = (MAP_BLOCKSIZE + y) / MAP_BLOCKSIZE;
	int bz = (MAP_BLOCKSIZE + z) / MAP_BLOCKSIZE;
	int idx = (bx + (by * 9) + (bz * 3));
	MapBlock *result = mLookup[idx];
	if (!result && (mLookupStateBitset & (1 << idx)) == 0) {
		// The block wasn't requested yet so fetch it from Map and store it
		// in the lookup
        Vector3<short> pos = mBlockPos + Vector3<short>{
            (short)(bx - 1), (short)(by - 1), (short)(bz - 1)};
		mLookup[idx] = result = mMap->GetBlockNoCreateNoEx(pos);
		mLookupStateBitset |= (1 << idx);
	}
	return result;
}

inline bool ReflowScan::IsLiquidFlowableTo(int x, int y, int z)
{
	// Tests whether (x,y,z) is a node to which liquid might flow.
	bool validPosition;
	MapBlock* block = LookupBlock(x, y, z);
	if (block) 
    {
		int dx = (MAP_BLOCKSIZE + x) % MAP_BLOCKSIZE;
		int dy = (MAP_BLOCKSIZE + y) % MAP_BLOCKSIZE;
		int dz = (MAP_BLOCKSIZE + z) % MAP_BLOCKSIZE;

		MapNode node = block->GetNodeNoCheck(dx, dy, dz, &validPosition);
		if (node.GetContent() != CONTENT_IGNORE) 
        {
			const ContentFeatures &f = mNodeMgr->Get(node);
			// NOTE: No need to check for flowing nodes with lower liquid level
			// as they should only occur on top of other columns where they
			// will be added to the queue themselves.
			return f.floodable;
		}
	}
	return false;
}

inline bool ReflowScan::IsLiquidHorizontallyFlowable(int x, int y, int z)
{
	// Check if the (x,y,z) might spread to one of the horizontally
	// neighbouring nodes
	return IsLiquidFlowableTo(x - 1, y, z) ||
		IsLiquidFlowableTo(x + 1, y, z) ||
		IsLiquidFlowableTo(x, y, z - 1) ||
		IsLiquidFlowableTo(x, y, z + 1);
}

void ReflowScan::ScanColumn(int x, int z)
{
	bool valid_position;

	// Is the column inside a loaded block?
	MapBlock* block = LookupBlock(x, 0, z);
	if (!block)
		return;

	MapBlock* above = LookupBlock(x, MAP_BLOCKSIZE, z);
	int dx = (MAP_BLOCKSIZE + x) % MAP_BLOCKSIZE;
	int dz = (MAP_BLOCKSIZE + z) % MAP_BLOCKSIZE;

	// Get the state from the node above the scanned block
	bool wasIgnore, wasLiquid;
	if (above) 
    {
		MapNode node = above->GetNodeNoCheck(dx, 0, dz, &valid_position);
		wasIgnore = node.GetContent() == CONTENT_IGNORE;
		wasLiquid = mNodeMgr->Get(node).IsLiquid();
	} 
    else 
    {
		wasIgnore = true;
		wasLiquid = false;
	}

	bool wasChecked = false;
	bool wasPushed = false;

	// Scan through the whole block
	for (short y = MAP_BLOCKSIZE - 1; y >= 0; y--) 
    {
		MapNode node = block->GetNodeNoCheck(dx, y, dz, &valid_position);
		const ContentFeatures& f = mNodeMgr->Get(node);
		bool isIgnore = node.GetContent() == CONTENT_IGNORE;
		bool isLiquid = f.IsLiquid();

		if (isIgnore || wasIgnore || isLiquid == wasLiquid) 
        {
			// Neither topmost node of liquid column nor topmost node below column
			wasChecked = false;
			wasPushed = false;
		} 
        else if (isLiquid) 
        {
			// This is the topmost node in the column
			bool isPushed = false;
			if (f.liquidType == LIQUID_FLOWING || IsLiquidHorizontallyFlowable(x, y, z)) 
            {
                mLiquidQueue->push(mRelBlockPos + Vector3<short>{(short)x, (short)y,(short)z});
				isPushed = true;
			}
			// Remember waschecked and waspushed to avoid repeated
			// checks/pushes in case the column consists of only this node
			wasChecked = true;
			wasPushed = isPushed;
		} 
        else 
        {
			// This is the topmost node below a liquid column
			if (!wasPushed && (f.floodable || (!wasChecked && IsLiquidHorizontallyFlowable(x, y + 1, z)))) 
            {
				// Activate the lowest node in the column which is one
				// node above this one
                mLiquidQueue->push(mRelBlockPos + Vector3<short>{(short)x, (short)(y + 1), (short)z});
			}
		}

		wasLiquid = isLiquid;
		wasIgnore = isIgnore;
	}

	// Check the node below the current block
	MapBlock* below = LookupBlock(x, -1, z);
	if (below) 
    {
		MapNode node = below->GetNodeNoCheck(dx, MAP_BLOCKSIZE - 1, dz, &valid_position);
		const ContentFeatures& f = mNodeMgr->Get(node);
		bool isIgnore = node.GetContent() == CONTENT_IGNORE;
		bool isLiquid = f.IsLiquid();

		if (isIgnore || wasIgnore || isLiquid == wasLiquid) 
        {
			// Neither topmost node of liquid column nor topmost node below column
		} 
        else if (isLiquid)
        {
			// This is the topmost node in the column and might want to flow away
			if (f.liquidType == LIQUID_FLOWING || IsLiquidHorizontallyFlowable(x, -1, z))
                mLiquidQueue->push(mRelBlockPos + Vector3<short>{(short)x, (short)-1, (short)z});
		} 
        else 
        {
			// This is the topmost node below a liquid column
			if (!wasPushed && (f.floodable || (!wasChecked && IsLiquidHorizontallyFlowable(x, 0, z)))) 
            {
				// Activate the lowest node in the column which is one
				// node above this one
                mLiquidQueue->push(mRelBlockPos + Vector3<short>{(short)x, 0, (short)z});
			}
		}
	}
}
