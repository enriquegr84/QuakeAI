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

#include "VoxelAlgorithms.h"
#include "MapBlock.h"
#include "Map.h"

#include "../../Graphics/Node.h"

/*!
 * A direction.
 * 0=X+
 * 1=Y+
 * 2=Z+
 * 3=Z-
 * 4=Y-
 * 5=X-
 * 6=no direction
 * Two directions are opposite only if their sum is 5.
 */
typedef uint8_t Direction;
/*!
 * Relative node position.
 * This represents a node's position in its map block.
 * All coordinates must be between 0 and 15.
 */
typedef Vector3<short> RelativeV3;
/*!
 * Position of a map block (block coordinates).
 * One blockPos unit is as long as 16 node position units.
 */
typedef Vector3<short> MapBlockV3;

//! Contains information about a node whose light is about to change.
struct ChangingLight 
{
	//! Relative position of the node in its map block.
	RelativeV3 relPosition;
	//! Position of the node's block.
	MapBlockV3 blockPosition;
	//! Pointer to the node's block.
	MapBlock* block = NULL;
	/*!
	 * Direction from the node that caused this node's changing
	 * to this node.
	 */
	Direction sourceDirection = 6;

	ChangingLight() = default;

	ChangingLight(RelativeV3 relPos, MapBlockV3 blockPos, MapBlock* block, Direction sourceDir) :
		relPosition(relPos), blockPosition(blockPos), block(block), sourceDirection(sourceDir)
	{
    
    }
};

/*!
 * A fast, priority queue-like container to contain ChangingLights.
 * The ChangingLights are ordered by the given light levels.
 * The brightest ChangingLight is returned first.
 */
struct LightQueue 
{
	//! For each light level there is a vector.
	std::vector<ChangingLight> lights[LIGHT_SUN + 1];
	//! Light of the brightest ChangingLight in the queue.
	uint8_t maxLight;

	/*!
	 * Creates a LightQueue.
	 * \param reserve for each light level that many slots are reserved.
	 */
	LightQueue(size_t reserve)
	{
		maxLight = LIGHT_SUN;
		for (uint8_t i = 0; i <= LIGHT_SUN; i++)
			lights[i].reserve(reserve);
	}

	/*!
	 * Returns the next brightest ChangingLight and
	 * removes it from the queue.
	 * If there were no elements in the queue, the given parameters
	 * remain unmodified.
	 * \param light light level of the popped ChangingLight
	 * \param data the ChangingLight that was popped
	 * \returns true if there was a ChangingLight in the queue.
	 */
	bool Next(uint8_t& light, ChangingLight& data)
	{
		while (lights[maxLight].empty()) 
        {
			if (maxLight == 0)
				return false;

			maxLight--;
		}
		light = maxLight;
		data = lights[maxLight].back();
		lights[maxLight].pop_back();
		return true;
	}

	/*!
	 * Adds an element to the queue.
	 * The parameters are the same as in ChangingLight's constructor.
	 * \param light light level of the ChangingLight
	 */
	inline void Push(uint8_t light, RelativeV3 relPos, MapBlockV3 blockPos, MapBlock *block, Direction sourceDir)
	{
		LogAssert(light <= LIGHT_SUN, "invalid light");
		lights[light].emplace_back(relPos, blockPos, block, sourceDir);
	}
};

/*!
 * This type of light queue is for unlighting.
 * A node can be pushed in it only if its raw light is zero.
 * This prevents pushing nodes twice into this queue.
 * The light of the pushed ChangingLight must be the
 * light of the node before unlighting it.
 */
typedef LightQueue UnlightQueue;
/*!
 * This type of light queue is for spreading lights.
 * While spreading lights, all the nodes in it must
 * have the same light as the light level the ChangingLights
 * were pushed into this queue with. This prevents unnecessary
 * re-pushing of the nodes into the queue.
 * If a node doesn't let light trough but emits light, it can be added
 * too.
 */
typedef LightQueue ReLightQueue;

/*!
 * NeighborDirs[i] points towards
 * the direction i.
 * See the definition of the type "direction"
 */
const static Vector3<short> NeighborDirs[6] = {
    Vector3<short>{1, 0, 0}, // right
    Vector3<short>{0, 1, 0}, // top
    Vector3<short>{0, 0, 1}, // back
    Vector3<short>{0, 0, -1}, // front
    Vector3<short>{0, -1, 0}, // bottom
    Vector3<short>{-1, 0, 0} // left
};

/*!
 * Transforms the given map block offset by one node towards
 * the specified direction.
 * \param dir the direction of the transformation
 * \param relPos the node's relative position in its map block
 * \param blockPos position of the node's block
 */
bool StepRelativeBlockPosition(Direction dir, RelativeV3& relPos, MapBlockV3& blockPos)
{
	switch (dir) 
    {
	    case 0:
		    if (relPos[0] < MAP_BLOCKSIZE - 1) 
            {
			    relPos[0]++;
		    } 
            else
            {
			    relPos[0] = 0;
			    blockPos[0]++;
			    return true;
		    }
		    break;
	    case 1:
		    if (relPos[1] < MAP_BLOCKSIZE - 1) 
            {
			    relPos[1]++;
		    } 
            else 
            {
			    relPos[1] = 0;
			    blockPos[1]++;
			    return true;
		    }
		    break;
	    case 2:
		    if (relPos[2] < MAP_BLOCKSIZE - 1) 
            {
			    relPos[2]++;
		    } 
            else 
            {
			    relPos[2] = 0;
			    blockPos[2]++;
			    return true;
		    }
		    break;
	    case 3:
		    if (relPos[2] > 0) 
            {
			    relPos[2]--;
		    } 
            else 
            {
			    relPos[2] = MAP_BLOCKSIZE - 1;
			    blockPos[2]--;
			    return true;
		    }
		    break;
	    case 4:
		    if (relPos[1] > 0)
            {
			    relPos[1]--;
		    }
            else
            {
			    relPos[1] = MAP_BLOCKSIZE - 1;
			    blockPos[1]--;
			    return true;
		    }
		    break;
	    case 5:
		    if (relPos[0] > 0) 
            {
			    relPos[0]--;
		    } 
            else 
            {
			    relPos[0] = MAP_BLOCKSIZE - 1;
			    blockPos[0]--;
			    return true;
		    }
		    break;
	}
	return false;
}

/*
 * Removes all light that is potentially emitted by the specified
 * light sources. These nodes will have zero light.
 * Returns all nodes whose light became zero but should be re-lighted.
 *
 * \param bank the light bank in which the procedure operates
 * \param fromNodes nodes whose light is removed
 * \param lightSources nodes that should be re-lighted
 * \param modifiedBlocks output, all modified map blocks are added to this
 */
void UnspreadLight(Map* map, const NodeManager* nodeMgr, LightBank bank,
	UnlightQueue& fromNodes, ReLightQueue& lightSources,
	std::map<Vector3<short>, MapBlock*>& modifiedBlocks)
{
	// Stores data popped from fromNodes
	uint8_t currentLight;
	ChangingLight current;
	// Data of the current neighbor
	MapBlockV3 neighborBlockPos;
	RelativeV3 neighborRelPos;
	// A dummy boolean
	bool isValidPosition;
	// Direction of the brightest neighbor of the node
	Direction sourceDir;
	while (fromNodes.Next(currentLight, current)) 
    {
		// For all nodes that need unlighting

		// There is no brightest neighbor
		sourceDir = 6;
		// The current node
		const MapNode& node = current.block->GetNodeNoCheck(
			current.relPosition, &isValidPosition);
		const ContentFeatures& cFeatures = nodeMgr->Get(node);
		// If the node emits light, it behaves like it had a
		// brighter neighbor.
		uint8_t brightestNeighborLight = cFeatures.lightSource + 1;
		for (Direction i = 0; i < 6; i++) 
        {
			//For each neighbor

			// The node that changed this node has already zero light
			// and it can't give light to this node
			if (current.sourceDirection + i == 5)
				continue;

			// Get the neighbor's position and block
			neighborRelPos = current.relPosition;
			neighborBlockPos = current.blockPosition;
			MapBlock* neighborBlock;
			if (StepRelativeBlockPosition(i, neighborRelPos, neighborBlockPos)) 
            {
				neighborBlock = map->GetBlockNoCreateNoEx(neighborBlockPos);
				if (neighborBlock == NULL) 
                {
					current.block->SetLightingComplete(bank, i, false);
					continue;
				}
			} 
            else 
            {
				neighborBlock = current.block;
			}
			// Get the neighbor itself
			MapNode neighbor = neighborBlock->GetNodeNoCheck(neighborRelPos, &isValidPosition);
			const ContentFeatures& neighborFeatures = nodeMgr->Get(neighbor.GetContent());
			uint8_t neighborLight = neighbor.GetLightRaw(bank, neighborFeatures);
			// If the neighbor has at least as much light as this node, then
			// it won't lose its light, since it should have been added to
			// fromNodes earlier, so its light would be zero.
			if (neighborFeatures.lightPropagates && neighborLight < currentLight) 
            {
				// Unlight, but only if the node has light.
				if (neighborLight > 0) 
                {
					neighbor.SetLight(bank, 0, neighborFeatures);
					neighborBlock->SetNodeNoCheck(neighborRelPos, neighbor);
					fromNodes.Push(neighborLight, neighborRelPos, neighborBlockPos, neighborBlock, i);
					// The current node was modified earlier, so its block
					// is in modifiedBlocks.
					if (current.block != neighborBlock)
						modifiedBlocks[neighborBlockPos] = neighborBlock;
				}
			} 
            else 
            {
				// The neighbor can light up this node.
				if (neighborLight < neighborFeatures.lightSource)
					neighborLight = neighborFeatures.lightSource;

				if (brightestNeighborLight < neighborLight)
                {
					brightestNeighborLight = neighborLight;
					sourceDir = i;
				}
			}
		}
		// If the brightest neighbor is able to light up this node,
		// then add this node to the output nodes.
		if (brightestNeighborLight > 1 && cFeatures.lightPropagates) 
        {
			brightestNeighborLight--;
			lightSources.Push(brightestNeighborLight, current.relPosition,
				current.blockPosition, current.block,
				(sourceDir == 6) ? 6 : 5 - sourceDir
				/* with opposite direction*/);
		}
	}
}

/*
 * Spreads light from the specified starting nodes.
 *
 * Before calling this procedure, make sure that all ChangingLights
 * in lightSources have as much light on the map as they have in
 * lightSources (if the queue contains a node multiple times, the brightest
 * occurrence counts).
 *
 * \param bank the light bank in which the procedure operates
 * \param lightSources starting nodes
 * \param modifiedBlocks output, all modified map blocks are added to this
 */
void SpreadLight(Map* map, const NodeManager* nodeMgr, LightBank bank,
	LightQueue& lightSources, std::map<Vector3<short>, MapBlock*>& modifiedBlocks)
{
	// The light the current node can provide to its neighbors.
	uint8_t spreadingLight;
	// The ChangingLight for the current node.
	ChangingLight current;
	// Position of the current neighbor.
	MapBlockV3 neighborBlockPos;
	RelativeV3 neighborRelPos;
	// A dummy boolean.
	bool isValidPosition;
	while (lightSources.Next(spreadingLight, current)) 
    {
		spreadingLight--;
		for (Direction i = 0; i < 6; i++) 
        {
			// This node can't light up its light source
			if (current.sourceDirection + i == 5)
				continue;

			// Get the neighbor's position and block
			neighborRelPos = current.relPosition;
			neighborBlockPos = current.blockPosition;
			MapBlock* neighborBlock;
			if (StepRelativeBlockPosition(i, neighborRelPos, neighborBlockPos)) 
            {
				neighborBlock = map->GetBlockNoCreateNoEx(neighborBlockPos);
				if (neighborBlock == NULL) 
                {
					current.block->SetLightingComplete(bank, i, false);
					continue;
				}
			} 
            else neighborBlock = current.block;

			// Get the neighbor itself
			MapNode neighbor = neighborBlock->GetNodeNoCheck(neighborRelPos, &isValidPosition);
			const ContentFeatures& cFeatures = nodeMgr->Get(neighbor.GetContent());
			if (cFeatures.lightPropagates) 
            {
				// Light up the neighbor, if it has less light than it should.
				uint8_t neighborLight = neighbor.GetLightRaw(bank, cFeatures);
				if (neighborLight < spreadingLight) 
                {
					neighbor.SetLight(bank, spreadingLight, cFeatures);
					neighborBlock->SetNodeNoCheck(neighborRelPos, neighbor);
					lightSources.Push(spreadingLight, neighborRelPos, neighborBlockPos, neighborBlock, i);
					// The current node was modified earlier, so its block
					// is in modifiedBlocks.
					if (current.block != neighborBlock)
						modifiedBlocks[neighborBlockPos] = neighborBlock;
				}
			}
		}
	}
}

struct SunlightPropagationUnit
{
    Vector2<short> relativePos;
    bool isSunlit;

	SunlightPropagationUnit(Vector2<short> relpos, bool sunlit) :
        relativePos(relpos), isSunlit(sunlit)
	{}
};

struct SunlightPropagationData
{
	std::vector<SunlightPropagationUnit> data;
	Vector3<short> targetBlock;
};

/*!
 * Returns true if the node gets sunlight from the
 * node above it.
 *
 * \param pos position of the node.
 */
bool IsSunlightAbove(Map* map, Vector3<short> pos, const NodeManager* nodeMgr)
{
	bool sunlight = true;
	MapBlockV3 sourceBlockPos;
	RelativeV3 sourceRelPos;
    GetNodeBlockPositionWithOffset(pos + Vector3<short>{0, 1, 0}, sourceBlockPos, sourceRelPos);
	// If the node above has sunlight, this node also can get it.
	MapBlock* sourceBlock = map->GetBlockNoCreateNoEx(sourceBlockPos);
	if (sourceBlock == NULL) 
    {
		// But if there is no node above, then use heuristics
		MapBlock* nodeBlock = map->GetBlockNoCreateNoEx(GetNodeBlockPosition(pos));
		if (nodeBlock == NULL) 
			sunlight = false;
        else 
			sunlight = !nodeBlock->IsUnderground();
	} 
    else 
    {
		bool isValidPosition;
		MapNode above = sourceBlock->GetNodeNoCheck(sourceRelPos, &isValidPosition);
		if (isValidPosition) 
        {
			if (above.GetContent() == CONTENT_IGNORE) 
            {
				// Trust heuristics
				if (sourceBlock->IsUnderground())
					sunlight = false;

			}
            else if (above.GetLight(LIGHTBANK_DAY, nodeMgr) != LIGHT_SUN) 
            {
				// If the node above doesn't have sunlight, this
				// node is in shadow.
				sunlight = false;
			}
		}
	}
	return sunlight;
}

static const LightBank Banks[] = { LIGHTBANK_DAY, LIGHTBANK_NIGHT };

void UpdateLightingNodes(Map* map,
	const std::vector<std::pair<Vector3<short>, MapNode>>& oldNodes,
	std::map<Vector3<short>, MapBlock*>& modifiedBlocks)
{
	const NodeManager* nodeMgr = map->GetNodeManager();
	// For node getter functions
	bool isValidPosition;

	// Process each light bank separately
	for (LightBank bank : Banks) 
    {
		UnlightQueue disappearingLights(256);
		ReLightQueue lightSources(256);
		// Nodes that are brighter than the brightest modified node was
		// won't change, since they didn't get their light from a
		// modified node.
		uint8_t minSafeLight = 0;
		for (auto it = oldNodes.cbegin(); it < oldNodes.cend(); ++it) 
        {
			uint8_t oldLight = it->second.GetLight(bank, nodeMgr);
			if (oldLight > minSafeLight)
				minSafeLight = oldLight;
		}
		// If only one node changed, even nodes with the same brightness
		// didn't get their light from the changed node.
		if (oldNodes.size() > 1)
			minSafeLight++;

		// For each changed node process sunlight and initialize
		for (auto it = oldNodes.cbegin(); it < oldNodes.cend(); ++it) 
        {
			// Get position and block of the changed node
			Vector3<short> p = it->first;
			RelativeV3 relPos;
			MapBlockV3 blockPos;
			GetNodeBlockPositionWithOffset(p, blockPos, relPos);
			MapBlock *block = map->GetBlockNoCreateNoEx(blockPos);
			if (block == NULL || block->IsDummy())
				continue;

			// Get the new node
			MapNode node = block->GetNodeNoCheck(relPos, &isValidPosition);
			if (!isValidPosition)
				break;

			// Light of the old node
			uint8_t oldLight = it->second.GetLight(bank, nodeMgr);

			// Add the block of the added node to modifiedBlocks
			modifiedBlocks[blockPos] = block;

			// Get new light level of the node
			uint8_t newLight = 0;
			if (nodeMgr->Get(node).lightPropagates)
            {
				if (bank == LIGHTBANK_DAY && 
                    nodeMgr->Get(node).sunlightPropagates &&
                    IsSunlightAbove(map, p, nodeMgr)) 
                {
					newLight = LIGHT_SUN;
				} 
                else 
                {
					newLight = nodeMgr->Get(node).lightSource;
					for (const Vector3<short>& neighborDir : NeighborDirs) 
                    {
						Vector3<short> p2 = p + neighborDir;
						bool isValid;
						MapNode n2 = map->GetNode(p2, &isValid);
						if (isValid) 
                        {
							uint8_t spread = n2.GetLight(bank, nodeMgr);
							// If it is sure that the neighbor won't be
							// unlighted, its light can spread to this node.
							if (spread > newLight && spread >= minSafeLight)
								newLight = spread - 1;
						}
					}
				}
			} 
            else 
            {
				// If this is an opaque node, it still can emit light.
				newLight = nodeMgr->Get(node).lightSource;
			}

			if (newLight > 0)
				lightSources.Push(newLight, relPos, blockPos, block, 6);

			if (newLight < oldLight) 
            {
				// The node became opaque or doesn't provide as much
				// light as the previous one, so it must be unlighted.

				// Add to unlight queue
                node.SetLight(bank, 0, nodeMgr);
				block->SetNodeNoCheck(relPos, node);
				disappearingLights.Push(oldLight, relPos, blockPos, block, 6);

				// Remove sunlight, if there was any
				if (bank == LIGHTBANK_DAY && oldLight == LIGHT_SUN) 
                {
					for (short y = p[1] - 1;; y--) 
                    {
                        Vector3<short> n2pos{ p[0], y, p[2] };

						MapNode n2 = map->GetNode(n2pos, &isValidPosition);
						if (!isValidPosition)
							break;

						// If this node doesn't have sunlight, the nodes below
						// it don't have too.
						if (n2.GetLight(LIGHTBANK_DAY, nodeMgr) != LIGHT_SUN)
							break;

						// Remove sunlight and add to unlight queue.
						n2.SetLight(LIGHTBANK_DAY, 0, nodeMgr);
						map->SetNode(n2pos, n2);
						RelativeV3 relPos2;
						MapBlockV3 blockPos2;
						GetNodeBlockPositionWithOffset(n2pos, blockPos2, relPos2);

						MapBlock *block2 = map->GetBlockNoCreateNoEx(blockPos2);
						disappearingLights.Push(LIGHT_SUN, relPos2,
							blockPos2, block2, 4 /* The node above caused the change */);
					}
				}
			} 
            else if (newLight > oldLight) 
            {
				// It is sure that the node provides more light than the previous
				// one, unlighting is not necessary.
				// Propagate sunlight
				if (bank == LIGHTBANK_DAY && newLight == LIGHT_SUN) 
                {
					for (short y = p[1] - 1;; y--) 
                    {
                        Vector3<short> n2pos{ p[0], y, p[2] };

						MapNode n2 = map->GetNode(n2pos, &isValidPosition);
						if (!isValidPosition)
							break;

						// This should not happen, but if the node has sunlight
						// then the iteration should stop.
						if (n2.GetLight(LIGHTBANK_DAY, nodeMgr) == LIGHT_SUN)
							break;

						// If the node terminates sunlight, stop.
						if (!nodeMgr->Get(n2).sunlightPropagates)
							break;

						RelativeV3 relPos2;
						MapBlockV3 blockPos2;
						GetNodeBlockPositionWithOffset(n2pos, blockPos2, relPos2);
						MapBlock *block2 = map->GetBlockNoCreateNoEx(blockPos2);

						// Mark node for lighting.
						lightSources.Push(LIGHT_SUN, relPos2, blockPos2, block2, 4);
					}
				}
			}

		}
		// Remove lights
		UnspreadLight(map, nodeMgr, bank, disappearingLights, lightSources, modifiedBlocks);
		// Initialize light values for light spreading.
		for (uint8_t i = 0; i <= LIGHT_SUN; i++) 
        {
			const std::vector<ChangingLight> &lights = lightSources.lights[i];
			for (std::vector<ChangingLight>::const_iterator it = lights.begin(); it < lights.end(); ++it) 
            {
				MapNode n = it->block->GetNodeNoCheck(it->relPosition, &isValidPosition);
				n.SetLight(bank, i, nodeMgr);
				it->block->SetNodeNoCheck(it->relPosition, n);
			}
		}
		// Spread lights.
		SpreadLight(map, nodeMgr, bank, lightSources, modifiedBlocks);
	}
}

/*!
 * Borders of a map block in relative node coordinates.
 * Compatible with type 'direction'.
 */
const VoxelArea BlockBorders[] = {
    VoxelArea(Vector3<short>{15, 0, 0}, Vector3<short>{15, 15, 15}), //X+
    VoxelArea(Vector3<short>{0, 15, 0}, Vector3<short>{15, 15, 15}), //Y+
    VoxelArea(Vector3<short>{0, 0, 15}, Vector3<short>{15, 15, 15}), //Z+
    VoxelArea(Vector3<short>{0, 0, 0}, Vector3<short>{15, 15, 0}),   //Z-
    VoxelArea(Vector3<short>{0, 0, 0}, Vector3<short>{15, 0, 15}),   //Y-
    VoxelArea(Vector3<short>{0, 0, 0}, Vector3<short>{0, 15, 15})    //X-
};

/*!
 * Returns true if:
 * -the node has unloaded neighbors
 * -the node doesn't have light
 * -the node's light is the same as the maximum of
 * its light source and its brightest neighbor minus one.
 * .
 */
bool IsLightLocallyCorrect(Map* map, const NodeManager* nodeMgr,
	LightBank bank, Vector3<short> pos)
{
	bool isValidPosition;
	MapNode n = map->GetNode(pos, &isValidPosition);
	const ContentFeatures& cFeatures = nodeMgr->Get(n);
	if (cFeatures.paramType != CPT_LIGHT)
		return true;

	uint8_t light = n.GetLightNoChecks(bank, &cFeatures);
	LogAssert(cFeatures.lightSource <= LIGHT_MAX, "invalid light");
	uint8_t brightestNeighbor = cFeatures.lightSource + 1;
	for (const Vector3<short>& neighborDir : NeighborDirs) 
    {
		MapNode n2 = map->GetNode(pos + neighborDir, &isValidPosition);
		uint8_t light2 = n2.GetLight(bank, nodeMgr);
		if (brightestNeighbor < light2)
			brightestNeighbor = light2;
	}
	LogAssert(light <= LIGHT_SUN, "invalid light");
	return brightestNeighbor == light + 1;
}

void UpdateBlockBorderLighting(Map* map, MapBlock* block,
	std::map<Vector3<short>, MapBlock*>& modifiedBlocks)
{
	const NodeManager* nodeMgr = map->GetNodeManager();
	bool isValidPosition;
	for (LightBank bank : Banks) 
    {
		// Since invalid light is not common, do not allocate
		// memory if not needed.
		UnlightQueue disappearingLights(0);
		ReLightQueue lightSources(0);
		// Get incorrect lights
		for (Direction d = 0; d < 6; d++) 
        {
			// For each direction
			// Get neighbor block
			Vector3<short> otherpos = block->GetPosition() + NeighborDirs[d];
			MapBlock* other = map->GetBlockNoCreateNoEx(otherpos);
			if (other == NULL)
				continue;

			// Only update if lighting was not completed.
			if (block->IsLightingComplete(bank, d) &&
                other->IsLightingComplete(bank, 5 - d))
				continue;
			// Reset flags
			block->SetLightingComplete(bank, d, true);
			other->SetLightingComplete(bank, 5 - d, true);
			// The two blocks and their connecting surfaces
			MapBlock* blocks[] = {block, other};
			VoxelArea areas[] = {BlockBorders[d], BlockBorders[5 - d]};
			// For both blocks
			for (uint8_t blocknum = 0; blocknum < 2; blocknum++) 
            {
				MapBlock* b = blocks[blocknum];
				VoxelArea a = areas[blocknum];
				// For all nodes
                for (int x = a.mMinEdge[0]; x <= a.mMaxEdge[0]; x++)
                {
                    for (int z = a.mMinEdge[2]; z <= a.mMaxEdge[2]; z++)
                    {
                        for (int y = a.mMinEdge[1]; y <= a.mMaxEdge[1]; y++)
                        {
                            MapNode node = b->GetNodeNoCheck(x, y, z, &isValidPosition);
                            uint8_t light = node.GetLight(bank, nodeMgr);
                            // Sunlight is fixed
                            if (light < LIGHT_SUN) 
                            {
                                // Unlight if not correct
                                if (!IsLightLocallyCorrect(map, nodeMgr, bank,
                                    Vector3<short>{(short)x, (short)y, (short)z} + 
                                    b->GetRelativePosition())) 
                                {
                                    // Initialize for unlighting
                                    node.SetLight(bank, 0, nodeMgr);
                                    b->SetNodeNoCheck(x, y, z, node);
                                    modifiedBlocks[b->GetPosition()] = b;
                                    disappearingLights.Push(light,
                                        RelativeV3{ (short)x, (short)y, (short)z }, 
                                        b->GetPosition(), b, 6);
                                }
                            }
                        }
                    }
                }
			}
		}
		// Remove lights
		UnspreadLight(map, nodeMgr, bank, disappearingLights, lightSources, modifiedBlocks);
		// Initialize light values for light spreading.
		for (uint8_t i = 0; i <= LIGHT_SUN; i++) 
        {
			const std::vector<ChangingLight>& lights = lightSources.lights[i];
			for (std::vector<ChangingLight>::const_iterator it = lights.begin(); it < lights.end(); ++it) 
            {
				MapNode n = it->block->GetNodeNoCheck(it->relPosition, &isValidPosition);
				n.SetLight(bank, i, nodeMgr);
				it->block->SetNodeNoCheck(it->relPosition, n);
			}
		}
		// Spread lights.
		SpreadLight(map, nodeMgr, bank, lightSources, modifiedBlocks);
	}
}

/*!
 * Resets the lighting of the given VoxelManipulator to
 * complete darkness and full sunlight.
 * Operates in one map sector.
 *
 * \param offset contains the least x and z node coordinates
 * of the map sector.
 * \param light incoming sunlight, light[x][z] is true if there
 * is sunlight above the voxel manipulator at the given x-z coordinates.
 * The array's indices are relative node coordinates in the sector.
 * After the procedure returns, this contains outgoing light at
 * the bottom of the voxel manipulator.
 */
void FillWithSunlight(MMVManip* vm, const NodeManager* nodeMgr, 
    Vector2<short> offset, bool light[MAP_BLOCKSIZE][MAP_BLOCKSIZE])
{
	// Distance in array between two nodes on top of each other.
	short ystride = vm->mArea.GetExtent()[0];
	// Cache the ignore node.
	MapNode ignore = MapNode(CONTENT_IGNORE);
	// For each column of nodes:
    for (short z = 0; z < MAP_BLOCKSIZE; z++)
    {
        for (short x = 0; x < MAP_BLOCKSIZE; x++)
        {
            // Position of the column on the map.
            Vector2<short> realpos = offset + Vector2<short>{x, z};
            // Array indices in the voxel manipulator
            int maxindex = vm->mArea.Index(realpos[0], vm->mArea.mMaxEdge[1], realpos[1]);
            int minindex = vm->mArea.Index(realpos[0], vm->mArea.mMinEdge[1], realpos[1]);
            // True if the current node has sunlight.
            bool lig = light[z][x];
            // For each node, downwards:
            for (int i = maxindex; i >= minindex; i -= ystride) 
            {
                MapNode* node;
                if (vm->mFlags[i] & VOXELFLAG_NO_DATA)
                    node = &ignore;
                else
                    node = &vm->mData[i];
                // Ignore IGNORE nodes, these are not generated yet.
                if (node->GetContent() == CONTENT_IGNORE)
                    continue;
                const ContentFeatures& cFeatures = nodeMgr->Get(node->GetContent());
                if (lig && !cFeatures.sunlightPropagates)
                {
                    // Sunlight is stopped.
                    lig = false;
                }

                // Reset light
                node->SetLight(LIGHTBANK_DAY, lig ? 15 : 0, cFeatures);
                node->SetLight(LIGHTBANK_NIGHT, 0, cFeatures);
            }
            // Output outgoing light.
            light[z][x] = lig;
        }
    }
}

/*!
 * Returns incoming sunlight for one map block.
 * If block above is not found, it is loaded.
 *
 * \param pos position of the map block that gets the sunlight.
 * \param light incoming sunlight, light[z][x] is true if there
 * is sunlight above the block at the given z-x relative
 * node coordinates.
 */
void IsSunlightAboveBlock(LogicMap* map, MapBlockV3 pos,
	const NodeManager* nodeMgr, bool light[MAP_BLOCKSIZE][MAP_BLOCKSIZE])
{
    MapBlockV3 sourceBlockPos = pos + Vector3<short>{0, 1, 0};
	// Get or load source block.
	// It might take a while to load, but correcting incorrect
	// sunlight may be even slower.
	MapBlock* sourceBlock = map->EmergeBlock(sourceBlockPos, false);
	// Trust only generated blocks.
	if (sourceBlock == NULL || sourceBlock->IsDummy() || !sourceBlock->IsGenerated()) 
    {
		// But if there is no block above, then use heuristics
		bool sunlight = true;
		MapBlock *nodeBlock = map->GetBlockNoCreateNoEx(pos);
        if (nodeBlock == NULL)
        {
            // This should not happen.
            sunlight = false;
        }
		else sunlight = !nodeBlock->IsUnderground();

		for (short z = 0; z < MAP_BLOCKSIZE; z++)
            for (short x = 0; x < MAP_BLOCKSIZE; x++)
                light[z][x] = sunlight;
	} 
    else 
    {
		// Dummy boolean, the position is valid.
		bool isValidPosition;
		// For each column:
        for (short z = 0; z < MAP_BLOCKSIZE; z++)
        {
            for (short x = 0; x < MAP_BLOCKSIZE; x++) 
            {
                // Get the bottom block.
                MapNode above = sourceBlock->GetNodeNoCheck(x, 0, z, &isValidPosition);
                light[z][x] = above.GetLight(LIGHTBANK_DAY, nodeMgr) == LIGHT_SUN;
            }
        }
	}
}

/*!
 * Propagates sunlight down in a given map block.
 *
 * \param data contains incoming sunlight and shadow and
 * the coordinates of the target block.
 * \param unlight propagated shadow is inserted here
 * \param relight propagated sunlight is inserted here
 *
 * \returns true if the block was modified, false otherwise.
 */
bool PropagateBlockSunlight(Map* map, const NodeManager* nodeMgr,
	SunlightPropagationData* data, UnlightQueue* unlight, ReLightQueue* relight)
{
	bool modified = false;
	// Get the block.
	MapBlock *block = map->GetBlockNoCreateNoEx(data->targetBlock);
	if (block == NULL || block->IsDummy()) 
    {
		// The work is done if the block does not contain data.
		data->data.clear();
		return false;
	}
	// Dummy boolean
	bool isValid;
	// For each changing column of nodes:
	size_t index;
	for (index = 0; index < data->data.size(); index++) 
    {
		SunlightPropagationUnit it = data->data[index];
		// Relative position of the currently inspected node.
        RelativeV3 currentPos{ it.relativePos[0], MAP_BLOCKSIZE - 1, it.relativePos[1] };
		if (it.isSunlit) 
        {
			// Propagate sunlight.
			// For each node downwards:
			for (; currentPos[1] >= 0; currentPos[1]--) 
            {
				MapNode node = block->GetNodeNoCheck(currentPos, &isValid);
				const ContentFeatures& cFeatures = nodeMgr->Get(node);
				if (node.GetLightRaw(LIGHTBANK_DAY, cFeatures) < LIGHT_SUN && cFeatures.sunlightPropagates)
                {
					// This node gets sunlight.
                    node.SetLight(LIGHTBANK_DAY, LIGHT_SUN, cFeatures);
					block->SetNodeNoCheck(currentPos, node);
					modified = true;
					relight->Push(LIGHT_SUN, currentPos, data->targetBlock, block, 4);
				} 
                else 
                {
					// Light already valid, propagation stopped.
					break;
				}
			}
		} 
        else 
        {
			// Propagate shadow.
			// For each node downwards:
			for (; currentPos[1] >= 0; currentPos[1]--) 
            {
				MapNode node = block->GetNodeNoCheck(currentPos, &isValid);
				const ContentFeatures& cFeatures = nodeMgr->Get(node);
				if (node.GetLightRaw(LIGHTBANK_DAY, cFeatures) == LIGHT_SUN)
                {
					// The sunlight is no longer valid.
                    node.SetLight(LIGHTBANK_DAY, 0, cFeatures);
					block->SetNodeNoCheck(currentPos, node);
					modified = true;
					unlight->Push(LIGHT_SUN, currentPos, data->targetBlock, block, 4);
				} 
                else 
                {
					// Reached shadow, propagation stopped.
					break;
				}
			}
		}
		if (currentPos[1] >= 0) 
        {
			// Propagation stopped, remove from data.
			data->data[index] = data->data.back();
			data->data.pop_back();
			index--;
		}
	}
	return modified;
}

/*!
 * Borders of a map block in relative node coordinates.
 * The areas do not overlap.
 * Compatible with type 'direction'.
 */
const VoxelArea BlockPad[] = {
    VoxelArea(Vector3<short>{15, 0, 0}, Vector3<short>{15, 15, 15}), //X+
    VoxelArea(Vector3<short>{1, 15, 0}, Vector3<short>{14, 15, 15}), //Y+
    VoxelArea(Vector3<short>{1, 1, 15}, Vector3<short>{14, 14, 15}), //Z+
    VoxelArea(Vector3<short>{1, 1, 0}, Vector3<short>{14, 14, 0}),   //Z-
    VoxelArea(Vector3<short>{1, 0, 0}, Vector3<short>{14, 0, 15}),   //Y-
    VoxelArea(Vector3<short>{0, 0, 0}, Vector3<short>{0, 15, 15})    //X-
};

/*!
 * The common part of bulk light updates - it is always executed.
 * The procedure takes the nodes that should be unlit, and the
 * full modified area.
 *
 * The procedure handles the correction of all lighting except
 * direct sunlight spreading.
 *
 * \param minblock least coordinates of the changed area in block
 * coordinates
 * \param maxblock greatest coordinates of the changed area in block
 * coordinates
 * \param unlight the first queue is for day light, the second is for
 * night light. Contains all nodes on the borders that need to be unlit.
 * \param relight the first queue is for day light, the second is for
 * night light. Contains nodes that were not modified, but got sunlight
 * because the changes.
 * \param modifiedBlocks the procedure adds all modified blocks to
 * this map
 */
void FinishBulkLightUpdate(Map* map, MapBlockV3 minblock,
	MapBlockV3 maxblock, UnlightQueue unlight[2], ReLightQueue relight[2],
	std::map<Vector3<short>, MapBlock*>* modifiedBlocks)
{
	const NodeManager* nodeMgr = map->GetNodeManager();
	// dummy boolean
	bool isValid;

	// --- STEP 1: Do unlighting

	for (size_t bank = 0; bank < 2; bank++) 
    {
		LightBank b = Banks[bank];
		UnspreadLight(map, nodeMgr, b, unlight[bank], relight[bank], *modifiedBlocks);
	}

	// --- STEP 2: Get all newly inserted light sources

	// For each block:
	Vector3<short> blockpos;
	Vector3<short> relpos;
    for (blockpos[0] = minblock[0]; blockpos[0] <= maxblock[0]; blockpos[0]++)
    {
        for (blockpos[1] = minblock[1]; blockpos[1] <= maxblock[1]; blockpos[1]++)
        {
            for (blockpos[2] = minblock[2]; blockpos[2] <= maxblock[2]; blockpos[2]++) 
            {
                MapBlock *block = map->GetBlockNoCreateNoEx(blockpos);
                if (!block || block->IsDummy())
                {
                    // Skip not existing blocks
                    continue;
                }

                // For each node in the block:
                for (relpos[0] = 0; relpos[0] < MAP_BLOCKSIZE; relpos[0]++)
                {
                    for (relpos[2] = 0; relpos[2] < MAP_BLOCKSIZE; relpos[2]++)
                    {
                        for (relpos[1] = 0; relpos[1] < MAP_BLOCKSIZE; relpos[1]++)
                        {
                            MapNode node = block->GetNodeNoCheck(relpos[0], relpos[1], relpos[2], &isValid);
                            const ContentFeatures& cFeatures = nodeMgr->Get(node);

                            // For each light bank
                            for (size_t b = 0; b < 2; b++) 
                            {
                                LightBank bank = Banks[b];
                                uint8_t light = cFeatures.paramType == CPT_LIGHT ?
                                    node.GetLightNoChecks(bank, &cFeatures) : cFeatures.lightSource;
                                if (light > 1)
                                    relight[b].Push(light, relpos, blockpos, block, 6);
                            } // end of banks
                        } // end of nodes
                    } // end of nodes
                } // end of nodes
            } // end of blocks
        } // end of blocks
    } // end of blocks

	// --- STEP 3: do light spreading

	// For each light bank:
	for (size_t b = 0; b < 2; b++) 
    {
		LightBank bank = Banks[b];
		// Sunlight is already initialized.
		uint8_t maxlight = (b == 0) ? LIGHT_MAX : LIGHT_SUN;
		// Initialize light values for light spreading.
		for (uint8_t i = 0; i <= maxlight; i++) 
        {
			const std::vector<ChangingLight>& lights = relight[b].lights[i];
			for (std::vector<ChangingLight>::const_iterator it = lights.begin(); it < lights.end(); ++it) 
            {
				MapNode n = it->block->GetNodeNoCheck(it->relPosition, &isValid);
				n.SetLight(bank, i, nodeMgr);
				it->block->SetNodeNoCheck(it->relPosition, n);
			}
		}
		// Spread lights.
		SpreadLight(map, nodeMgr, bank, relight[b], *modifiedBlocks);
	}
}

void BlitBackWithLight(LogicMap* map, MMVManip* vm,
	std::map<Vector3<short>, MapBlock*>* modifiedBlocks)
{
	const NodeManager* nodeMgr = map->GetNodeManager();

	MapBlockV3 minblock = GetNodeBlockPosition(vm->mArea.mMinEdge);
	MapBlockV3 maxblock = GetNodeBlockPosition(vm->mArea.mMaxEdge);
	// First queue is for day light, second is for night light.
	UnlightQueue unlight[] = { UnlightQueue(256), UnlightQueue(256) };
	ReLightQueue relight[] = { ReLightQueue(256), ReLightQueue(256) };
	// Will hold sunlight data.
	bool lights[MAP_BLOCKSIZE][MAP_BLOCKSIZE];
	SunlightPropagationData data;
	// Dummy boolean.
	bool isValid;

	// --- STEP 1: reset everything to sunlight

	// For each map block:
    for (short x = minblock[0]; x <= maxblock[0]; x++)
    {
        for (short z = minblock[2]; z <= maxblock[2]; z++)
        {
            // Extract sunlight above.
            IsSunlightAboveBlock(map, Vector3<short>{x, maxblock[1], z}, nodeMgr, lights);
            Vector2<short> offset{ x, z };
            offset *= (short)MAP_BLOCKSIZE;
            // Reset the voxel manipulator.
            FillWithSunlight(vm, nodeMgr, offset, lights);
            // Copy sunlight data
            data.targetBlock = Vector3<short>{ x, (short)(minblock[1] - 1), z };
            for (short z = 0; z < MAP_BLOCKSIZE; z++)
                for (short x = 0; x < MAP_BLOCKSIZE; x++)
                    data.data.emplace_back(Vector2<short>{x, z}, lights[z][x]);

            // Propagate sunlight and shadow below the voxel manipulator.
            while (!data.data.empty()) 
            {
                if (PropagateBlockSunlight(map, nodeMgr, &data, &unlight[0], &relight[0]))
                    (*modifiedBlocks)[data.targetBlock] = map->GetBlockNoCreateNoEx(data.targetBlock);

                // Step downwards.
                data.targetBlock[1]--;
            }
        }
    }

	// --- STEP 2: Get nodes from borders to unlight
	Vector3<short> blockpos;
	Vector3<short> relpos;

	// In case there are unloaded holes in the voxel manipulator
	// unlight each block.
	// For each block:
    for (blockpos[0] = minblock[0]; blockpos[0] <= maxblock[0]; blockpos[0]++)
    {
        for (blockpos[1] = minblock[1]; blockpos[1] <= maxblock[1]; blockpos[1]++)
        {
            for (blockpos[2] = minblock[2]; blockpos[2] <= maxblock[2]; blockpos[2]++)
            {
                MapBlock* block = map->GetBlockNoCreateNoEx(blockpos);
                if (!block || block->IsDummy())
                {
                    // Skip not existing blocks.
                    continue;
                }

                Vector3<short> offset = block->GetRelativePosition();
                // For each border of the block:
                for (const VoxelArea& a : BlockPad) 
                {
                    // For each node of the border:
                    for (relpos[0] = a.mMinEdge[0]; relpos[0] <= a.mMaxEdge[0]; relpos[0]++)
                    {
                        for (relpos[2] = a.mMinEdge[2]; relpos[2] <= a.mMaxEdge[2]; relpos[2]++)
                        {
                            for (relpos[1] = a.mMinEdge[1]; relpos[1] <= a.mMaxEdge[1]; relpos[1]++)
                            {
                                // Get old and new node
                                MapNode oldnode = block->GetNodeNoCheck(relpos, &isValid);
                                const ContentFeatures& oldf = nodeMgr->Get(oldnode);
                                MapNode newnode = vm->GetNodeNoExNoEmerge(relpos + offset);
                                const ContentFeatures& newf = oldnode == newnode ? oldf : nodeMgr->Get(newnode);

                                // For each light bank
                                for (size_t b = 0; b < 2; b++) 
                                {
                                    LightBank bank = Banks[b];
                                    // no light information, force unlighting
                                    uint8_t oldlight = oldf.paramType == CPT_LIGHT ?
                                        oldnode.GetLightNoChecks(bank, &oldf) : LIGHT_SUN; 
                                    uint8_t newlight = newf.paramType == CPT_LIGHT ?
                                        newnode.GetLightNoChecks(bank, &newf) : newf.lightSource;
                                    // If the new node is dimmer, unlight.
                                    if (oldlight > newlight)
                                        unlight[b].Push(oldlight, relpos, blockpos, block, 6);
                                } // end of banks
                            } // end of nodes
                        } // end of nodes
                    } // end of nodes
                } // end of borders
            } // end of blocks
        } // end of blocks
    } // end of blocks

	// --- STEP 3: All information extracted, overwrite

	vm->BlitBackAll(modifiedBlocks, true);

	// --- STEP 4: Finish light update

	FinishBulkLightUpdate(map, minblock, maxblock, unlight, relight, modifiedBlocks);
}

/*!
 * Resets the lighting of the given map block to
 * complete darkness and full sunlight.
 *
 * \param light incoming sunlight, light[x][z] is true if there
 * is sunlight above the map block at the given x-z coordinates.
 * The array's indices are relative node coordinates in the block.
 * After the procedure returns, this contains outgoing light at
 * the bottom of the map block.
 */
void FillWithSunlight(MapBlock* block, const NodeManager* nodeMgr,
	bool light[MAP_BLOCKSIZE][MAP_BLOCKSIZE])
{
	if (block->IsDummy())
		return;
	// dummy boolean
	bool isValid;
	// For each column of nodes:
    for (short z = 0; z < MAP_BLOCKSIZE; z++)
    {
        for (short x = 0; x < MAP_BLOCKSIZE; x++)
        {
            // True if the current node has sunlight.
            bool lig = light[z][x];
            // For each node, downwards:
            for (short y = MAP_BLOCKSIZE - 1; y >= 0; y--) 
            {
                MapNode node = block->GetNodeNoCheck(x, y, z, &isValid);
                // Ignore IGNORE nodes, these are not generated yet.
                if (node.GetContent() == CONTENT_IGNORE)
                    continue;
                const ContentFeatures& cFeatures = nodeMgr->Get(node.GetContent());
                if (lig && !cFeatures.sunlightPropagates) 
                {
                    // Sunlight is stopped.
                    lig = false;
                }
                // Reset light
                node.SetLight(LIGHTBANK_DAY, lig ? 15 : 0, cFeatures);
                node.SetLight(LIGHTBANK_NIGHT, 0, cFeatures);
                block->SetNodeNoCheck(x, y, z, node);
            }
            // Output outgoing light.
            light[z][x] = lig;
        }
    }
}

void RepairBlockLight(LogicMap* map, MapBlock* block,
	std::map<Vector3<short>, MapBlock*>* modifiedBlocks)
{
	if (!block || block->IsDummy())
		return;

	const NodeManager* nodeMgr = map->GetNodeManager();
	// First queue is for day light, second is for night light.
	UnlightQueue unlight[] = { UnlightQueue(256), UnlightQueue(256) };
	ReLightQueue relight[] = { ReLightQueue(256), ReLightQueue(256) };
	// Will hold sunlight data.
	bool lights[MAP_BLOCKSIZE][MAP_BLOCKSIZE];
	SunlightPropagationData data;
	// Dummy boolean.
	bool isValid;

	// --- STEP 1: reset everything to sunlight

	MapBlockV3 blockpos = block->GetPosition();
	(*modifiedBlocks)[blockpos] = block;
	// For each map block:
	// Extract sunlight above.
	IsSunlightAboveBlock(map, blockpos, nodeMgr, lights);
	// Reset the voxel manipulator.
	FillWithSunlight(block, nodeMgr, lights);
	// Copy sunlight data
    data.targetBlock = Vector3<short>{ blockpos[0], (short)(blockpos[1] - 1), blockpos[2] };
	for (short z = 0; z < MAP_BLOCKSIZE; z++)
	    for (short x = 0; x < MAP_BLOCKSIZE; x++)
            data.data.emplace_back(Vector2<short>{x, z}, lights[z][x]);

	// Propagate sunlight and shadow below the voxel manipulator.
	while (!data.data.empty()) 
    {
		if (PropagateBlockSunlight(map, nodeMgr, &data, &unlight[0], &relight[0]))
			(*modifiedBlocks)[data.targetBlock] =
				map->GetBlockNoCreateNoEx(data.targetBlock);
		// Step downwards.
		data.targetBlock[1]--;
	}

	// --- STEP 2: Get nodes from borders to unlight

	// For each border of the block:
	for (const VoxelArea &a : BlockPad) 
    {
		Vector3<short> relpos;
		// For each node of the border:
        for (relpos[0] = a.mMinEdge[0]; relpos[0] <= a.mMaxEdge[0]; relpos[0]++)
        {
            for (relpos[2] = a.mMinEdge[2]; relpos[2] <= a.mMaxEdge[2]; relpos[2]++)
            {
                for (relpos[1] = a.mMinEdge[1]; relpos[1] <= a.mMaxEdge[1]; relpos[1]++)
                {
                    // Get node
                    MapNode node = block->GetNodeNoCheck(relpos, &isValid);
                    const ContentFeatures& cFeatures = nodeMgr->Get(node);
                    // For each light bank
                    for (size_t b = 0; b < 2; b++) 
                    {
                        LightBank bank = Banks[b];
                        uint8_t light = cFeatures.paramType == CPT_LIGHT ?
                            node.GetLightNoChecks(bank, &cFeatures) : cFeatures.lightSource;
                        // If the new node is dimmer than sunlight, unlight.
                        // (if it has maximal light, it is pointless to remove
                        // surrounding light, as it can only become brighter)
                        if (LIGHT_SUN > light) 
                            unlight[b].Push(LIGHT_SUN, relpos, blockpos, block, 6);
                    } // end of banks
                } // end of nodes
            } // end of nodes
        } // end of nodes
	} // end of borders

	// STEP 3: Remove and spread light

	FinishBulkLightUpdate(map, blockpos, blockpos, unlight, relight, modifiedBlocks);
}

VoxelLineIterator::VoxelLineIterator(const Vector3<float>& startPos, const Vector3<float>& lineVector) :
	mStartPosition(startPos), mLineVector(lineVector)
{
    mCurrentNodePos[0] = (short)(mStartPosition[0] + (mStartPosition[0] > 0 ? 1.f / 2 : -1.f / 2));
    mCurrentNodePos[1] = (short)(mStartPosition[1] + (mStartPosition[1] > 0 ? 1.f / 2 : -1.f / 2));
    mCurrentNodePos[2] = (short)(mStartPosition[2] + (mStartPosition[2] > 0 ? 1.f / 2 : -1.f / 2));

	mStartNodePos = mCurrentNodePos;

    Vector3<float> p = startPos + lineVector;
    Vector3<short> pp;
    pp[0] = (short)(p[0] + (p[0] > 0 ? 1.f / 2 : -1.f / 2));
    pp[1] = (short)(p[1] + (p[1] > 0 ? 1.f / 2 : -1.f / 2));
    pp[2] = (short)(p[2] + (p[2] > 0 ? 1.f / 2 : -1.f / 2));
	mLastIndex = GetIndex(pp);

	if (mLineVector[0] > 0) 
    {
		mNextIntersectionMulti[0] = (floorf(mStartPosition[0] - 0.5f) + 1.5f - mStartPosition[0]) / mLineVector[0];
		mIntersectionMultiInc[0] = 1 / mLineVector[0];
	}
    else if (mLineVector[0] < 0) 
    {
		mNextIntersectionMulti[0] = (floorf(mStartPosition[0] - 0.5f) - mStartPosition[0] + 0.5f) / mLineVector[0];
		mIntersectionMultiInc[0] = -1 / mLineVector[0];
		mStepDirections[0] = -1;
	}

	if (mLineVector[1] > 0) 
    {
		mNextIntersectionMulti[1] = (floorf(mStartPosition[1] - 0.5f) + 1.5f - mStartPosition[1]) / mLineVector[1];
		mIntersectionMultiInc[1] = 1 / mLineVector[1];
	} 
    else if (mLineVector[1] < 0) 
    {
		mNextIntersectionMulti[1] = (floorf(mStartPosition[1] - 0.5f) - mStartPosition[1] + 0.5f) / mLineVector[1];
		mIntersectionMultiInc[1] = -1 / mLineVector[1];
		mStepDirections[1] = -1;
	}

	if (mLineVector[2] > 0) 
    {
		mNextIntersectionMulti[2] = (floorf(mStartPosition[2] - 0.5f) + 1.5f - mStartPosition[2]) / mLineVector[2];
		mIntersectionMultiInc[2] = 1 / mLineVector[2];
	}
    else if (mLineVector[2] < 0)
    {
		mNextIntersectionMulti[2] = (floorf(mStartPosition[2] - 0.5f) - mStartPosition[2] + 0.5f) / mLineVector[2];
		mIntersectionMultiInc[2] = -1 / mLineVector[2];
		mStepDirections[2] = -1;
	}
}

void VoxelLineIterator::Next()
{
	mCurrentIndex++;
	if ((mNextIntersectionMulti[0] < mNextIntersectionMulti[1]) && 
        (mNextIntersectionMulti[0] < mNextIntersectionMulti[2]))
    {
		mNextIntersectionMulti[0] += mIntersectionMultiInc[0];
		mCurrentNodePos[0] += mStepDirections[0];
	} 
    else if ((mNextIntersectionMulti[1] < mNextIntersectionMulti[2])) 
    {
		mNextIntersectionMulti[1] += mIntersectionMultiInc[1];
		mCurrentNodePos[1] += mStepDirections[1];
	} 
    else
    {
		mNextIntersectionMulti[2] += mIntersectionMultiInc[2];
		mCurrentNodePos[2] += mStepDirections[2];
	}
}

short VoxelLineIterator::GetIndex(Vector3<short> voxel)
{
	return
		abs(voxel[0] - mStartNodePos[0]) +
		abs(voxel[1] - mStartNodePos[1]) +
		abs(voxel[2] - mStartNodePos[2]);
}