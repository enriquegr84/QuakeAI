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

#include "MapGeneratorSingleNode.h"
#include "MapBlock.h"
#include "MapNode.h"
#include "Map.h"

#include "Voxel.h"
#include "VoxelAlgorithms.h"
#include "Emerge.h"

#include "../../Graphics/Node.h"


MapGeneratorSinglenode::MapGeneratorSinglenode(MapGeneratorParams* params, EmergeParams* emerge)
	: MapGenerator(MAPGEN_SINGLENODE, params, emerge)
{
	const NodeManager* nodeMgr = emerge->mNodeMgr;

	mContentNode = nodeMgr->GetId("mapgen_singlenode");
	if (mContentNode == CONTENT_IGNORE)
		mContentNode = CONTENT_AIR;

	MapNode node(mContentNode);
	mSetLight = (nodeMgr->Get(node).sunlightPropagates) ? LIGHT_SUN : 0x00;
}


//////////////////////// Map generator

void MapGeneratorSinglenode::MakeChunk(BlockMakeData *data)
{
	// Pre-conditions
	LogAssert(data->vmanip, "invalid vmanip");
	LogAssert(data->nodeMgr, "invalid node manager");

	this->mGenerating = true;
	this->mMMVManip = data->vmanip;
	this->mNodeMgr = data->nodeMgr;

	Vector3<short> blockPosMin = data->blockPosMin;
	Vector3<short> blockPosMax = data->blockPosMax;

	// Area of central chunk
	Vector3<short> mNodeMin = blockPosMin * (short)MAP_BLOCKSIZE;
    Vector3<short> mNodeMax = (blockPosMax + Vector3<short>{1, 1, 1}) * 
        (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1};

	mBlockSeed = GetBlockSeed2(mNodeMin, (int)data->seed);

	MapNode node(mContentNode);
    for (int16_t z = mNodeMin[2]; z <= mNodeMax[2]; z++)
    {
        for (int16_t y = mNodeMin[1]; y <= mNodeMax[1]; y++) 
        {
            unsigned int i = mMMVManip->mArea.Index(mNodeMin[0], y, z);
            for (int16_t x = mNodeMin[0]; x <= mNodeMax[0]; x++)
            {
                if (mMMVManip->mData[i].GetContent() == CONTENT_IGNORE)
                    mMMVManip->mData[i] = node;
                i++;
            }
        }
    }

	// Add top and bottom side of water to transforming_liquid queue
	UpdateLiquid(&data->transformingLiquid, mNodeMin, mNodeMax);

	// Set lighting
	if ((mFlags & MG_LIGHT) && mSetLight == LIGHT_SUN)
		SetLighting(LIGHT_SUN, mNodeMin, mNodeMax);

	this->mGenerating = false;
}


int MapGeneratorSinglenode::GetSpawnLevelAtPoint(Vector2<short> pos)
{
	return 0;
}
