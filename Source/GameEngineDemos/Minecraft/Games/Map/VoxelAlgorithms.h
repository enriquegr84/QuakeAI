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

#ifndef VOXELALGORITHMS_H
#define VOXELALGORITHMS_H

#include "Voxel.h"

#include "MapNode.h"

class Map;
class LogicMap;
class MapBlock;
class MMVManip;

/*!
 * Updates the lighting on the map.
 * The result will be correct only if
 * no nodes were changed except the given ones.
 * Before calling this procedure make sure that all new nodes on
 * the map have zero light level!
 *
 * \param oldNodes contains the MapNodes that were replaced by the new
 * MapNodes and their positions
 * \param modifiedBlocks output, contains all map blocks that
 * the function modified
 */
void UpdateLightingNodes(Map* map,
	const std::vector<std::pair<Vector3<short>, MapNode>>& oldNodes,
	std::map<Vector3<short>, MapBlock*>& modifiedBlocks);

/*!
 * Updates borders of the given mapblock.
 * Only updates if the block was marked with incomplete
 * lighting and the neighbor is also loaded.
 *
 * \param block the block to update
 * \param modifiedBlocks output, contains all map blocks that
 * the function modified
 */
void UpdateBlockBorderLighting(Map* map, MapBlock* block,
	std::map<Vector3<short>, MapBlock*>& modifiedBlocks);

/*!
 * Copies back nodes from a voxel manipulator
 * to the map and updates lighting.
 * For logic use only.
 *
 * \param modifiedBlocks output, contains all map blocks that
 * the function modified
 */
void BlitBackWithLight(LogicMap* map, MMVManip* vm,
	std::map<Vector3<short>, MapBlock*>* modifiedBlocks);

/*!
 * Corrects the light in a map block.
 * For logic use only.
 *
 * \param block the block to update
 */
void RepairBlockLight(LogicMap* map, MapBlock* block,
	std::map<Vector3<short>, MapBlock*>* modifiedBlocks);

/*!
 * This class iterates trough voxels that intersect with
 * a line. The collision detection does not see nodeboxes,
 * every voxel is a cube and is returned.
 * This iterator steps to all nodes exactly once.
 */
struct VoxelLineIterator
{
public:
	//! Starting position of the line in world coordinates.
	Vector3<float> mStartPosition;
	//! Direction and length of the line in world coordinates.
	Vector3<float> mLineVector;
	/*!
	 * Each component stores the next smallest positive number, by
	 * which multiplying the line's vector gives a vector that ends
	 * on the intersection of two nodes.
	 */
	Vector3<float> mNextIntersectionMulti { 10000.0f, 10000.0f, 10000.0f };
	/*!
	 * Each component stores the smallest positive number, by which
	 * mNextIntersectionMulti's components can be increased.
	 */
	Vector3<float> mIntersectionMultiInc { 10000.0f, 10000.0f, 10000.0f };
	/*!
	 * Direction of the line. Each component can be -1 or 1 (if a
	 * component of the line's vector is 0, then there will be 1).
	 */
	Vector3<short> mStepDirections { 1, 1, 1 };
	//! Position of the current node.
	Vector3<short> mCurrentNodePos;
	//! Index of the current node
	short mCurrentIndex = 0;
	//! Position of the start node.
	Vector3<short> mStartNodePos;
	//! Index of the last node
	short mLastIndex;

	/*!
	 * Creates a voxel line iterator with the given line.
	 * @param startPos starting point of the line
	 * in voxel coordinates
	 * @param lineVector    length and direction of the
	 * line in voxel coordinates. startPos+lineVector
	 * is the end of the line
	 */
	VoxelLineIterator(const Vector3<float>& startPos,const Vector3<float>& lineVector);

	/*!
	 * Steps to the next voxel.
	 * Updates mCurrentNodePos and
	 * m_previous_node_pos.
	 * Note that it works even if HasNext() is false,
	 * continuing the line as a ray.
	 */
	void Next();

	/*!
	 * Returns true if the next voxel intersects the given line.
	 */
	inline bool HasNext() const
	{
		return mCurrentIndex < mLastIndex;
	}

	/*!
	 * Returns how many times next() must be called until
	 * voxel==mCurrentNodePos.
	 * If voxel does not intersect with the line,
	 * the result is unodeMgrined.
	 */
	short GetIndex(Vector3<short> voxel);
};


#endif
