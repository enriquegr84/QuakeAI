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

#include "Map.h"
#include "Mapsector.h"
#include "Mapblock.h"

#include "Emerge.h"
#include "Reflowscan.h"
#include "VoxelAlgorithms.h"

#include "../../Data/Database.h"

#include "../../Games/Environment/LogicEnvironment.h"

#include "../../Utils/Util.h"

#include "Core/Utility/Serialize.h"
#include "Core/Utility/Profiler.h"
#include "Core/IO/FileSystem.h"

#include "Application/Settings.h"

/*
	Map
*/

Map::Map(Environment* env) : mEnvironment(env)
{
	mSectorCachePos = { 0, 0 };
}

Map::~Map()
{
	/*
		Free all MapSectors
	*/
	for (auto& sector : mSectors) 
    {
		delete sector.second;
	}
}

void Map::AddEventReceiver(MapEventReceiver* eventReceiver)
{
	mEventReceivers.insert(eventReceiver);
}

void Map::RemoveEventReceiver(MapEventReceiver* eventReceiver)
{
	mEventReceivers.erase(eventReceiver);
}

void Map::DispatchEvent(const MapEditEvent& evt)
{
	for (MapEventReceiver* eventReceiver : mEventReceivers) 
		eventReceiver->OnMapEditEvent(evt);
}

MapSector* Map::GetSectorNoGenerateNoLock(Vector2<short> pos)
{
	if(mSectorCache != NULL && pos == mSectorCachePos)
    {
		MapSector* sector = mSectorCache;
		return sector;
	}

	std::map<Vector2<short>, MapSector*>::iterator n = mSectors.find(pos);
	if (n == mSectors.end())
		return NULL;

	MapSector* sector = n->second;

	// Cache the last result
	mSectorCachePos = pos;
	mSectorCache = sector;

	return sector;
}

MapSector* Map::GetSectorNoGenerate(Vector2<short> pos)
{
	return GetSectorNoGenerateNoLock(pos);
}

MapBlock* Map::GetBlockNoCreateNoEx(Vector3<short> p3d)
{
    Vector2<short> p2d{ p3d[0], p3d[2] };
	MapSector* sector = GetSectorNoGenerate(p2d);
	if(sector == NULL)
		return NULL;
	MapBlock* block = sector->GetBlockNoCreateNoEx(p3d[1]);
	return block;
}

MapBlock* Map::GetBlockNoCreate(Vector3<short> p3d)
{
	MapBlock* block = GetBlockNoCreateNoEx(p3d);
	if(block == NULL)
		throw InvalidPositionException();
	return block;
}

bool Map::IsNodeUnderground(Vector3<short> pos)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	return block && block->IsUnderground();
}

bool Map::IsValidPosition(Vector3<short> pos)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	return (block != NULL);
}

// Returns a CONTENT_IGNORE node if not found
MapNode Map::GetNode(Vector3<short> pos, bool* isValidPosition)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if (block == NULL) 
    {
		if (isValidPosition != NULL)
			*isValidPosition = false;
		return {CONTENT_IGNORE};
	}

	Vector3<short> relPos = pos - blockPos * (short)MAP_BLOCKSIZE;
	bool isValidPos;
	MapNode node = block->GetNodeNoCheck(relPos, &isValidPos);
	if (isValidPosition != NULL)
		*isValidPosition = isValidPos;
	return node;
}

// throws InvalidPositionException if not found
void Map::SetNode(Vector3<short> pos, MapNode& node)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	MapBlock* block = GetBlockNoCreate(blockPos);
	Vector3<short> relPos = pos - blockPos * (short)MAP_BLOCKSIZE;
	// Never allow placing CONTENT_IGNORE, it causes problems
	if(node.GetContent() == CONTENT_IGNORE)
    {
		bool tempBool;
		LogError("Map::SetNode(): Not allowing to place CONTENT_IGNORE while trying to replace \"" +
            mEnvironment->GetNodeManager()->Get(block->GetNodeNoCheck(relPos, &tempBool)).name +
            "\" at (" + std::to_string(pos[0]) + "," + std::to_string(pos[1]) +
            "," + std::to_string(pos[2]) + ") (block (" + std::to_string(blockPos[0]) +
            "," + std::to_string(blockPos[1]) + "," + std::to_string(blockPos[2]) + "))");
		return;
	}
	block->SetNodeNoCheck(relPos, node);
}

void Map::AddNodeAndUpdate(Vector3<short> pos, MapNode node,
		std::map<Vector3<short>, MapBlock*>& modifiedBlocks, bool removeMetadata)
{
	// Collect old node for rollback
	//RollbackNode rollbackOldNode(this, pos, mGame);

	// This is needed for updating the lighting
	MapNode oldNode = GetNode(pos);

	// Remove node metadata
	if (removeMetadata)
		RemoveMapNodeMetadata(pos);

	// Set the node on the map
	// Ignore light (because calling voxalgo::UpdateLightingNodes)
	node.SetLight(LIGHTBANK_DAY, 0, mEnvironment->GetNodeManager());
	node.SetLight(LIGHTBANK_NIGHT, 0, mEnvironment->GetNodeManager());
	SetNode(pos, node);

	// Update lighting
	std::vector<std::pair<Vector3<short>, MapNode>> oldNodes;
	oldNodes.emplace_back(pos, oldNode);
	UpdateLightingNodes(this, oldNodes, modifiedBlocks);

	for (auto& modifiedBlock : modifiedBlocks) 
		modifiedBlock.second->ExpireDayNightDiff();

    /*
	// Report for rollback
	if(mGame->Rollback())
	{
		RollbackNode rollbackNewNode(this, pos, mGame);
		RollbackAction action;
		action.SetNode(pos, rollbackOldNode, rollbackNewNode);
		mGame->Rollback()->ReportAction(action);
	}
    */

	/*
		Add neighboring liquid nodes and this node to transform queue.
		(it's vital for the node itself to get updated last, if it was removed.)
	 */

    const Vector3<short> dirs[7] =
    {
        Vector3<short>{0,0,1}, // back
        Vector3<short>{0,1,0}, // top
        Vector3<short>{1,0,0}, // right
        Vector3<short>{0,0,-1}, // front
        Vector3<short>{0,-1,0}, // bottom
        Vector3<short>{-1,0,0}, // left
        Vector3<short>{0,0,0}, // self
    };
	for (const Vector3<short>& dir : dirs) 
    {
		Vector3<short> p2 = pos + dir;

		bool isValidPosition;
		MapNode n2 = GetNode(p2, &isValidPosition);
        if (isValidPosition && 
            (mEnvironment->GetNodeManager()->Get(n2).IsLiquid() || n2.GetContent() == CONTENT_AIR))
            mTransformingLiquid.push(p2);
	}
}

void Map::RemoveNodeAndUpdate(Vector3<short> pos, std::map<Vector3<short>, MapBlock*>& modifiedBlocks)
{
	AddNodeAndUpdate(pos, MapNode(CONTENT_AIR), modifiedBlocks, true);
}

bool Map::AddNodeWithEvent(Vector3<short> pos, MapNode n, bool removeMetadata)
{
	MapEditEvent evt;
	evt.type = removeMetadata ? MEET_ADDNODE : MEET_SWAPNODE;
	evt.position = pos;
	evt.node = n;

	bool succeeded = true;
	try
    {
		std::map<Vector3<short>, MapBlock*> modifiedBlocks;
		AddNodeAndUpdate(pos, n, modifiedBlocks, removeMetadata);

		// Copy modifiedBlocks to event
		for (auto& modifiedBlock : modifiedBlocks) 
        {
			evt.modifiedBlocks.insert(modifiedBlock.first);
		}
	}
	catch(InvalidPositionException&)
    {
		succeeded = false;
	}

	DispatchEvent(evt);
	return succeeded;
}

bool Map::RemoveNodeWithEvent(Vector3<short> pos)
{
	MapEditEvent evt;
	evt.type = MEET_REMOVENODE;
	evt.position = pos;

	bool succeeded = true;
	try
    {
		std::map<Vector3<short>, MapBlock*> modifiedBlocks;
		RemoveNodeAndUpdate(pos, modifiedBlocks);

		// Copy modifiedBlocks to event
		for (auto& modifiedBlock : modifiedBlocks)
			evt.modifiedBlocks.insert(modifiedBlock.first);
	}
	catch(InvalidPositionException&)
    {
		succeeded = false;
	}

	DispatchEvent(evt);
	return succeeded;
}

struct TimeOrderedMapBlock 
{
	MapSector* sect;
	MapBlock* block;

	TimeOrderedMapBlock(MapSector* sect, MapBlock* block) :
		sect(sect), block(block)
	{}

	bool operator<(const TimeOrderedMapBlock& b) const
	{
		return block->GetUsageTimer() < b.block->GetUsageTimer();
	};
};

/*
	Updates usage timers
*/
void Map::TimerUpdate(float dTime, float unloadTimeout,
    unsigned int maxLoadedBlocks, std::vector<Vector3<short>>* unloadedBlocks)
{
	bool saveBeforeUnloading = (MapType() == MAPTYPE_LOGIC);

	// Profile modified reasons
	Profiler modProfiler;

	std::vector<Vector2<short>> sectorDeletionQueue;
	unsigned int deletedBlocksCount = 0;
	unsigned int savedBlocksCount = 0;
	unsigned int blockCountAll = 0;

	BeginSave();

	// If there is no practical limit, we spare creation of mapBlockQueue
	if (maxLoadedBlocks == 0xFFFFFFFF)
    {
		for (auto& sectorIt : mSectors)
        {
			MapSector* sector = sectorIt.second;

			bool allBlocksDeleted = true;

			MapBlockVec blocks;
			sector->GetBlocks(blocks);

			for (MapBlock* block : blocks) 
            {
				block->IncrementUsageTimer(dTime);

				if (block->RefGet() == 0 && block->GetUsageTimer() > unloadTimeout) 
                {
					Vector3<short> pos = block->GetPosition();

					// Save if modified
					if (block->GetModified() != MOD_STATE_CLEAN && saveBeforeUnloading) 
                    {
						modProfiler.Add(block->GetModifiedReasonString(), 1);
						if (!SaveBlock(block))
							continue;
						savedBlocksCount++;
					}

					// Delete from memory
					sector->DeleteBlock(block);

					if (unloadedBlocks)
						unloadedBlocks->push_back(pos);

					deletedBlocksCount++;
				} 
                else 
                {
					allBlocksDeleted = false;
					blockCountAll++;
				}
			}

			if (allBlocksDeleted)
				sectorDeletionQueue.push_back(sectorIt.first);
		}
	} 
    else 
    {
		std::priority_queue<TimeOrderedMapBlock> mapBlockQueue;
		for (auto& sectorIt : mSectors) 
        {
			MapSector* sector = sectorIt.second;

			MapBlockVec blocks;
			sector->GetBlocks(blocks);
			for (MapBlock* block : blocks) 
            {
				block->IncrementUsageTimer(dTime);
				mapBlockQueue.push(TimeOrderedMapBlock(sector, block));
			}
		}
		blockCountAll = (unsigned int)mapBlockQueue.size();
		// Delete old blocks, and blocks over the limit from the memory
		while (!mapBlockQueue.empty() && (mapBlockQueue.size() > maxLoadedBlocks
				|| mapBlockQueue.top().block->GetUsageTimer() > unloadTimeout)) 
        {
			TimeOrderedMapBlock b = mapBlockQueue.top();
			mapBlockQueue.pop();

			MapBlock* block = b.block;
			if (block->RefGet() != 0)
				continue;

			Vector3<short> p = block->GetPosition();

			// Save if modified
			if (block->GetModified() != MOD_STATE_CLEAN && saveBeforeUnloading) 
            {
				modProfiler.Add(block->GetModifiedReasonString(), 1);
				if (!SaveBlock(block))
					continue;
				savedBlocksCount++;
			}

			// Delete from memory
			b.sect->DeleteBlock(block);

			if (unloadedBlocks)
				unloadedBlocks->push_back(p);

			deletedBlocksCount++;
			blockCountAll--;
		}
		// Delete empty sectors
		for (auto& sectorIt : mSectors) 
        {
			if (sectorIt.second->Empty()) 
				sectorDeletionQueue.push_back(sectorIt.first);
		}
	}
	EndSave();

	// Finally delete the empty sectors
	DeleteSectors(sectorDeletionQueue);

	if(deletedBlocksCount != 0)
	{
        std::stringstream infostream;

		PrintInfo(infostream); // LogicMap/VisualMap:
		infostream << "Unloaded " << deletedBlocksCount << " blocks from memory";
		if(saveBeforeUnloading)
			infostream << ", of which " << savedBlocksCount << " were written";
		infostream << ", " << blockCountAll << " blocks in memory";
		infostream << "." << std::endl;
		if(savedBlocksCount != 0)
        {
			PrintInfo(infostream); // LogicMap/VisualMap:
			infostream << "Blocks modified by: " << std::endl;
			modProfiler.Print(infostream);
		}
        LogInformation(infostream.str());
	}
}

void Map::UnloadUnreferencedBlocks(std::vector<Vector3<short>>* unloadedBlocks)
{
	TimerUpdate(0.0, -1.0, 0, unloadedBlocks);
}

void Map::DeleteSectors(std::vector<Vector2<short>>& sectorList)
{
	for (Vector2<short> j : sectorList) 
    {
		MapSector *sector = mSectors[j];
		// If sector is in sector cache, remove it from there
		if(mSectorCache == sector)
			mSectorCache = NULL;
		// Remove from map and delete
		mSectors.erase(j);
		delete sector;
	}
}

void Map::PrintInfo(std::ostream& out)
{
	out<<"Map: ";
}

#define WATER_DROP_BOOST 4

const static Vector3<short> liquid6Dirs[6] = 
{
	// order: upper before same level before lower
    Vector3<short>{ 0, 1, 0},
    Vector3<short>{ 0, 0, 1},
    Vector3<short>{ 1, 0, 0},
    Vector3<short>{ 0, 0,-1},
    Vector3<short>{-1, 0, 0},
    Vector3<short>{ 0,-1, 0}
};

enum NeighborType : uint8_t 
{
	NEIGHBOR_UPPER,
	NEIGHBOR_SAME_LEVEL,
	NEIGHBOR_LOWER
};

struct NodeNeighbor 
{
	MapNode node;
	NeighborType type;
	Vector3<short> position;

	NodeNeighbor()
		: node(CONTENT_AIR), type(NEIGHBOR_SAME_LEVEL)
	{ }

	NodeNeighbor(const MapNode& n, NeighborType t, const Vector3<short>& p)
		: node(n), type(t), position(p)
	{ }
};

void Map::TransformingLiquidAdd(Vector3<short> pos) 
{
    mTransformingLiquid.push(pos);
}

void Map::TransformLiquids(std::map<Vector3<short>, MapBlock*>& modifiedBlocks, LogicEnvironment *env)
{
	unsigned int loopCount = 0;
	unsigned int initialSize = (unsigned int)mTransformingLiquid.size();

	/*if(initialSize != 0)
		infostream<<"TransformLiquids(): initialSize="<<initialSize<<std::endl;*/

	// list of nodes that due to viscosity have not reached their max level height
	std::deque<Vector3<short>> mustReflow;

	std::vector<std::pair<Vector3<short>, MapNode> > changedNodes;

	unsigned int liquidLoopMax = Settings::Get()->GetInt("liquid_loop_max");
	unsigned int loopMax = liquidLoopMax;

	while (mTransformingLiquid.size() != 0)
	{
		// This should be done here so that it is done when continue is used
		if (loopCount >= initialSize || loopCount >= loopMax)
			break;
		loopCount++;

		/*
			Get a queued transforming liquid node
		*/
		Vector3<short> p0 = mTransformingLiquid.front();
		mTransformingLiquid.pop();

		MapNode n0 = GetNode(p0);

		/*
			Collect information about current node
		 */
		int8_t liquidLevel = -1;
		// The liquid node which will be placed there if
		// the liquid flows into this node.
		unsigned short liquidKind = CONTENT_IGNORE;
		// The node which will be placed there if liquid
		// can't flow into this node.
		unsigned short floodableNode = CONTENT_AIR;
		const ContentFeatures& cf = mEnvironment->GetNodeManager()->Get(n0);
		LiquidType liquidType = cf.liquidType;
		switch (liquidType) 
        {
			case LIQUID_SOURCE:
				liquidLevel = LIQUID_LEVEL_SOURCE;
				liquidKind = cf.liquidAlternativeFlowingId;
				break;
			case LIQUID_FLOWING:
				liquidLevel = (n0.param2 & LIQUID_LEVEL_MASK);
				liquidKind = n0.GetContent();
				break;
			case LIQUID_NONE:
				// if this node is 'floodable', it *could* be transformed
				// into a liquid, otherwise, continue with the next node.
				if (!cf.floodable)
					continue;
				floodableNode = n0.GetContent();
				liquidKind = CONTENT_AIR;
				break;
		}

		/*
			Collect information about the environment
		 */
		NodeNeighbor sources[6]; // surrounding sources
		int numSources = 0;
		NodeNeighbor flows[6]; // surrounding flowing liquid nodes
		int numFlows = 0;
		NodeNeighbor airs[6]; // surrounding air
		int numAirs = 0;
		NodeNeighbor neutrals[6]; // nodes that are solid or another kind of liquid
		int numNeutrals = 0;
		bool flowingDown = false;
		bool ignoredSources = false;
		for (unsigned short i = 0; i < 6; i++) 
        {
			NeighborType nt = NEIGHBOR_SAME_LEVEL;
			switch (i) 
            {
				case 0:
					nt = NEIGHBOR_UPPER;
					break;
				case 5:
					nt = NEIGHBOR_LOWER;
					break;
				default:
					break;
			}
			Vector3<short> npos = p0 + liquid6Dirs[i];
			NodeNeighbor nb(GetNode(npos), nt, npos);
			const ContentFeatures& cfnb = mEnvironment->GetNodeManager()->Get(nb.node);
			switch (mEnvironment->GetNodeManager()->Get(nb.node.GetContent()).liquidType)
            {
				case LIQUID_NONE:
					if (cfnb.floodable) 
                    {
						airs[numAirs++] = nb;
						// if the current node is a water source the neighbor
						// should be enqueded for transformation regardless of whether the
						// current node changes or not.
						if (nb.type != NEIGHBOR_UPPER && liquidType != LIQUID_NONE)
							mTransformingLiquid.push(npos);
						// if the current node happens to be a flowing node, it will start to flow down here.
						if (nb.type == NEIGHBOR_LOWER)
							flowingDown = true;
					} 
                    else 
                    {
						neutrals[numNeutrals++] = nb;
						if (nb.node.GetContent() == CONTENT_IGNORE) 
                        {
							// If node below is ignore prevent water from
							// spreading outwards and otherwise prevent from
							// flowing away as ignore node might be the source
							if (nb.type == NEIGHBOR_LOWER)
								flowingDown = true;
							else
								ignoredSources = true;
						}
					}
					break;
				case LIQUID_SOURCE:
					// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
					if (liquidKind == CONTENT_AIR)
						liquidKind = cfnb.liquidAlternativeFlowingId;
					if (cfnb.liquidAlternativeFlowingId != liquidKind) 
                    {
						neutrals[numNeutrals++] = nb;
					} 
                    else 
                    {
						// Do not count bottom source, it will screw things up
						if(nt != NEIGHBOR_LOWER)
							sources[numSources++] = nb;
					}
					break;
				case LIQUID_FLOWING:
					if (nb.type != NEIGHBOR_SAME_LEVEL ||
						(nb.node.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK) 
                    {
						// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
						// but exclude falling liquids on the same level, they cannot flow here anyway
						if (liquidKind == CONTENT_AIR)
							liquidKind = cfnb.liquidAlternativeFlowingId;
					}
					if (cfnb.liquidAlternativeFlowingId != liquidKind) 
                    {
						neutrals[numNeutrals++] = nb;
					} 
                    else 
                    {
						flows[numFlows++] = nb;
						if (nb.type == NEIGHBOR_LOWER)
							flowingDown = true;
					}
					break;
			}
		}

		/*
			decide on the type (and possibly level) of the current node
		 */
		unsigned short newNodeContent;
		int8_t newNodeLevel = -1;
		int8_t maxNodeLevel = -1;

		uint8_t range = mEnvironment->GetNodeManager()->Get(liquidKind).liquidRange;
		if (range > LIQUID_LEVEL_MAX + 1)
			range = LIQUID_LEVEL_MAX + 1;

		if ((numSources >= 2 && 
			mEnvironment->GetNodeManager()->Get(liquidKind).liquidRenewable) || 
			liquidType == LIQUID_SOURCE)
        {
			// liquidKind will be set to either the flowing alternative of the node (if it's a liquid)
			// or the flowing alternative of the first of the surrounding sources (if it's air), so
			// it's perfectly safe to use liquidKind here to determine the new node content.
			newNodeContent = mEnvironment->GetNodeManager()->Get(liquidKind).liquidAlternativeSourceId;
		} 
        else if (numSources >= 1 && sources[0].type != NEIGHBOR_LOWER) 
        {
			// liquidKind is set properly, see above
			maxNodeLevel = newNodeLevel = LIQUID_LEVEL_MAX;
			if (newNodeLevel >= (LIQUID_LEVEL_MAX + 1 - range))
				newNodeContent = liquidKind;
			else
				newNodeContent = floodableNode;
		} 
        else if (ignoredSources && liquidLevel >= 0) 
        {
			// Maybe there are neighbouring sources that aren't loaded yet
			// so prevent flowing away.
			newNodeLevel = liquidLevel;
			newNodeContent = liquidKind;
		} 
        else 
        {
			// no surrounding sources, so get the maximum level that can flow into this node
			for (unsigned short i = 0; i < numFlows; i++) 
            {
				uint8_t nbLiquidLevel = (flows[i].node.param2 & LIQUID_LEVEL_MASK);
				switch (flows[i].type) 
                {
					case NEIGHBOR_UPPER:
						if (nbLiquidLevel + WATER_DROP_BOOST > maxNodeLevel) 
                        {
							maxNodeLevel = LIQUID_LEVEL_MAX;
							if (nbLiquidLevel + WATER_DROP_BOOST < LIQUID_LEVEL_MAX)
								maxNodeLevel = nbLiquidLevel + WATER_DROP_BOOST;
						} 
                        else if (nbLiquidLevel > maxNodeLevel) 
                        {
							maxNodeLevel = nbLiquidLevel;
						}
						break;
					case NEIGHBOR_LOWER:
						break;
					case NEIGHBOR_SAME_LEVEL:
						if ((flows[i].node.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK &&
								nbLiquidLevel > 0 && nbLiquidLevel - 1 > maxNodeLevel)
							maxNodeLevel = nbLiquidLevel - 1;
						break;
				}
			}

			uint8_t viscosity = mEnvironment->GetNodeManager()->Get(liquidKind).liquidViscosity;
			if (viscosity > 1 && maxNodeLevel != liquidLevel) 
            {
				// amount to gain, limited by viscosity
				// must be at least 1 in absolute value
				int8_t levelInc = maxNodeLevel - liquidLevel;
				if (levelInc < -viscosity || levelInc > viscosity)
					newNodeLevel = liquidLevel + levelInc/viscosity;
				else if (levelInc < 0)
					newNodeLevel = liquidLevel - 1;
				else if (levelInc > 0)
					newNodeLevel = liquidLevel + 1;
				if (newNodeLevel != maxNodeLevel)
					mustReflow.push_back(p0);
			} 
            else 
            {
				newNodeLevel = maxNodeLevel;
			}

			if (maxNodeLevel >= (LIQUID_LEVEL_MAX + 1 - range))
				newNodeContent = liquidKind;
			else
				newNodeContent = floodableNode;

		}

		/*
			check if anything has changed. if not, just continue with the next node.
		 */
		if (newNodeContent == n0.GetContent() &&
            (mEnvironment->GetNodeManager()->Get(n0.GetContent()).liquidType != LIQUID_FLOWING ||
            ((n0.param2 & LIQUID_LEVEL_MASK) == (uint8_t)newNodeLevel &&
            ((n0.param2 & LIQUID_FLOW_DOWN_MASK) == LIQUID_FLOW_DOWN_MASK) == flowingDown)))
			continue;

		/*
			update the current node
		 */
		MapNode n00 = n0;
		//bool flowDownEnabled = (flowingDown && ((n0.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK));
		if (mEnvironment->GetNodeManager()->Get(newNodeContent).liquidType == LIQUID_FLOWING)
        {
			// set level to last 3 bits, flowing down bit to 4th bit
			n0.param2 = (flowingDown ? LIQUID_FLOW_DOWN_MASK : 0x00) | (newNodeLevel & LIQUID_LEVEL_MASK);
		} 
        else 
        {
			// set the liquid level and flow bits to 0
			n0.param2 &= ~(LIQUID_LEVEL_MASK | LIQUID_FLOW_DOWN_MASK);
		}

		// change the node.
		n0.SetContent(newNodeContent);

		// on_flood() the node
		if (floodableNode != CONTENT_AIR) 
        {
			if (BaseGame::Get()->OnFloodNode(p0, n00, n0))
				continue;
		}

		// Ignore light (because calling voxalgo::UpdateLightingNodes)
		n0.SetLight(LIGHTBANK_DAY, 0, mEnvironment->GetNodeManager());
		n0.SetLight(LIGHTBANK_NIGHT, 0, mEnvironment->GetNodeManager());

        /*
		// Find out whether there is a suspect for this action
		std::string suspect;
		if (mGame->Rollback())
			suspect = mGame->Rollback()->GetSuspect(p0, 83, 1);

		if (mGame->Rollback() && !suspect.Empty()) 
        {
			// Blame suspect
			RollbackScopeActor rollbackScope(mGame->Rollback(), suspect, true);
			// Get old node for rollback
			RollbackNode rollbackOldNode(this, p0, mGame);
			// Set node
			SetNode(p0, n0);
			// Report
			RollbackNode rollbackNewNode(this, p0, mGame);
			RollbackAction action;
			action.setSetNode(p0, rollbackOldNode, rollbackNewNode);
			mGame->Rollback()->reportAction(action);
		} 
        else 
        */
        {
			// Set node
			SetNode(p0, n0);
		}

		Vector3<short> blockPos = GetNodeBlockPosition(p0);
		MapBlock* block = GetBlockNoCreateNoEx(blockPos);
		if (block != NULL) 
        {
			modifiedBlocks[blockPos] =  block;
			changedNodes.emplace_back(p0, n00);
		}

		/*
			enqueue neighbors for update if neccessary
		 */
		switch (mEnvironment->GetNodeManager()->Get(n0.GetContent()).liquidType)
        {
			case LIQUID_SOURCE:
			case LIQUID_FLOWING:
				// make sure source flows into all neighboring nodes
				for (unsigned short i = 0; i < numFlows; i++)
					if (flows[i].type != NEIGHBOR_UPPER)
						mTransformingLiquid.push(flows[i].position);
				for (unsigned short i = 0; i < numAirs; i++)
					if (airs[i].type != NEIGHBOR_UPPER)
						mTransformingLiquid.push(airs[i].position);
				break;
			case LIQUID_NONE:
				// this flow has turned to air; neighboring flows might need to do the same
				for (unsigned short i = 0; i < numFlows; i++)
					mTransformingLiquid.push(flows[i].position);
				break;
		}
	}
	//infostream<<"Map::TransformLiquids(): loopCount="<<loopCount<<std::endl;

	for (auto& iter : mustReflow)
		mTransformingLiquid.push(iter);

	UpdateLightingNodes(this, changedNodes, modifiedBlocks);


	/* ----------------------------------------------------------------------
	 * Manage the queue so that it does not grow inodeMgrinately
	 */
	unsigned short timeUntilPurge = Settings::Get()->GetUInt16("liquid_queue_purge_time");

	if (timeUntilPurge == 0)
		return; // Feature disabled

	timeUntilPurge *= 1000;	// seconds -> milliseconds

	unsigned int currTime = Timer::GetRealTime();
	unsigned int prevUnprocessed = mUnprocessedCount;
	mUnprocessedCount = (unsigned int)mTransformingLiquid.size();

	// if unprocessed block count is decreasing or stable
	if (mUnprocessedCount <= prevUnprocessed) 
    {
		mQueueSizeTimerStarted = false;
	} 
    else 
    {
		if (!mQueueSizeTimerStarted)
			mIncTrendingUpStartTime = currTime;
		mQueueSizeTimerStarted = true;
	}

	// Account for currTime overflowing
	if (mQueueSizeTimerStarted && mIncTrendingUpStartTime > currTime)
		mQueueSizeTimerStarted = false;

	/* If the queue has been growing for more than liquid_queue_purge_time seconds
	 * and the number of unprocessed blocks is still > liquidLoopMax then we
	 * cannot keep up; dump the oldest blocks from the queue so that the queue
	 * has liquidLoopMax items in it
	 */
	if (mQueueSizeTimerStarted && 
        currTime - mIncTrendingUpStartTime > timeUntilPurge && 
        mUnprocessedCount > liquidLoopMax) 
    {
		size_t dumpqty = mUnprocessedCount - liquidLoopMax;

		LogInformation("TransformLiquids(): DUMPING " + 
            std::to_string(dumpqty) + " blocks from the queue");

		while (dumpqty--)
			mTransformingLiquid.pop();

		mQueueSizeTimerStarted = false; // optimistically assume we can keep up now
		mUnprocessedCount = (unsigned int)mTransformingLiquid.size();
	}
}

std::vector<Vector3<short>> Map::FindNodesWithMetadata(Vector3<short> p1, Vector3<short> p2)
{
	std::vector<Vector3<short>> positionsWithMeta;

	SortBoxVertices(p1, p2);
	Vector3<short> bpmin = GetNodeBlockPosition(p1);
	Vector3<short> bpmax = GetNodeBlockPosition(p2);

	VoxelArea area(p1, p2);

    for (short z = bpmin[2]; z <= bpmax[2]; z++)
    {
        for (short y = bpmin[1]; y <= bpmax[1]; y++)
        {
            for (short x = bpmin[0]; x <= bpmax[0]; x++)
            {
                Vector3<short> blockPos{ x, y, z };
                MapBlock* block = GetBlockNoCreateNoEx(blockPos);
                if (!block) 
                {
                    LogInformation ("Map::GetMapNodeMetadata(): Need to emerge (" + std::to_string(blockPos[0]) + 
                        "," + std::to_string(blockPos[1]) + "," + std::to_string(blockPos[2]) + ")");
                    block = EmergeBlock(blockPos, false);
                }
                if (!block) 
                {
                    LogWarning("Map::GetMapNodeMetadata(): Block not found");
                    continue;
                }

                Vector3<short> pBase = blockPos * (short)MAP_BLOCKSIZE;
                std::vector<Vector3<short>> keys = block->mMapNodeMetadata.GetAllKeys();
                for (size_t i = 0; i != keys.size(); i++) 
                {
                    Vector3<short> p(keys[i] + pBase);
                    if (!area.Contains(p))
                        continue;

                    positionsWithMeta.push_back(p);
                }
            }
        }
    }

	return positionsWithMeta;
}

MapNodeMetadata* Map::GetMapNodeMetadata(Vector3<short> pos)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	Vector3<short> pRel = pos - blockPos * (short)MAP_BLOCKSIZE;
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if(!block)
    {
		LogInformation("Map::GetMapNodeMetadata(): Need to emerge (" + std::to_string(blockPos[0]) +
            "," + std::to_string(blockPos[1]) + "," + std::to_string(blockPos[2]) + ")");
		block = EmergeBlock(blockPos, false);
	}
	if(!block)
    {
		LogWarning("Map::GetMapNodeMetadata(): Block not found");
		return NULL;
	}
	MapNodeMetadata* meta = block->mMapNodeMetadata.Get(pRel);
	return meta;
}

bool Map::SetMapNodeMetadata(Vector3<short> pos, MapNodeMetadata* meta)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	Vector3<short> pRel = pos - blockPos * (short)MAP_BLOCKSIZE;
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if(!block)
    {
		LogInformation("Map::SetMapNodeMetadata(): Need to emerge (" + std::to_string(blockPos[0]) +
            "," + std::to_string(blockPos[1]) + "," + std::to_string(blockPos[2]) + ")");
		block = EmergeBlock(blockPos, false);
	}
	if(!block)
    {
        LogWarning("Map::SetMapNodeMetadata(): Block not found");
		return false;
	}
	block->mMapNodeMetadata.Set(pRel, meta);
	return true;
}

void Map::RemoveMapNodeMetadata(Vector3<short> pos)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	Vector3<short> pRel = pos - blockPos * (short)MAP_BLOCKSIZE;
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if(block == NULL)
	{
		LogWarning("Map::RemoveMapNodeMetadata(): Block not found");
		return;
	}
	block->mMapNodeMetadata.Remove(pRel);
}

NodeTimer Map::GetNodeTimer(Vector3<short> pos)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	Vector3<short> pRel = pos - blockPos * (short)MAP_BLOCKSIZE;
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if(!block)
    {
		LogInformation("Map::GetNodeTimer(): Need to emerge (" + std::to_string(blockPos[0]) +
            "," + std::to_string(blockPos[1]) + "," + std::to_string(blockPos[2]) + ")");
		block = EmergeBlock(blockPos, false);
	}
	if(!block)
    {
		LogWarning("Map::GetNodeTimer(): Block not found");
		return NodeTimer();
	}
	NodeTimer timer = block->mNodeTimers.Get(pRel);
	NodeTimer nodeTimer(timer.mTimeout, timer.mElapsed, pos);
	return nodeTimer;
}

void Map::SetNodeTimer(const NodeTimer& timer)
{
	Vector3<short> pos = timer.mPosition;
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	Vector3<short> pRel = pos - blockPos * (short)MAP_BLOCKSIZE;
	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if(!block)
    {
		LogInformation("Map::SetNodeTimer(): Need to emerge (" + std::to_string(blockPos[0]) +
            "," + std::to_string(blockPos[1]) + "," + std::to_string(blockPos[2]) + ")");
		block = EmergeBlock(blockPos, false);
	}
	if(!block)
    {
		LogWarning("Map::SetNodeTimer(): Block not found");
		return;
	}
	NodeTimer nodeTimer(timer.mTimeout, timer.mElapsed, pRel);
	block->mNodeTimers.Set(nodeTimer);
}

void Map::RemoveNodeTimer(Vector3<short> pos)
{
	Vector3<short> blockPos = GetNodeBlockPosition(pos);
	Vector3<short> pRel = pos - blockPos * (short)MAP_BLOCKSIZE;
	MapBlock *block = GetBlockNoCreateNoEx(blockPos);
	if(block == NULL)
	{
		LogWarning("Map::RemoveNodeTimer(): Block not found");
		return;
	}
	block->mNodeTimers.Remove(pRel);
}

bool Map::DetermineAdditionalOcclusionCheck(const Vector3<short>& posCamera, 
    const BoundingBox<short>& blockBounds, Vector3<short>& check)
{
	/*
		This functions determines the node inside the target block that is
		closest to the camera position. This increases the occlusion culling
		accuracy in straight and diagonal corridors.
		The returned position will be occlusion checked first in addition to the
		others (8 corners + center).
		No position is returned if
		- the closest node is a corner, corners are checked anyway.
		- the camera is inside the target block, it will never be occluded.
	*/

	bool xInside = (blockBounds.mMinEdge[0] <= posCamera[0]) && (posCamera[0] <= blockBounds.mMaxEdge[0]);
	bool yInside = (blockBounds.mMinEdge[1] <= posCamera[1]) && (posCamera[1] <= blockBounds.mMaxEdge[1]);
	bool zInside = (blockBounds.mMinEdge[2] <= posCamera[2]) && (posCamera[2] <= blockBounds.mMaxEdge[2]);
	if (xInside && yInside && zInside)
		return false; // Camera inside target mapblock

	// straight
	if (xInside && yInside) 
    {
        check = Vector3<short>{ posCamera[0], posCamera[1], 0 };
        check[2] = (posCamera[2] <= blockBounds.mMinEdge[2]) ?
            blockBounds.mMinEdge[2] : blockBounds.mMaxEdge[2];
		return true;
	} 
    else if (yInside && zInside) 
    {
        check = Vector3<short>{ 0, posCamera[1], posCamera[2] };
		check[0] = (posCamera[0] <= blockBounds.mMinEdge[0]) ?
            blockBounds.mMinEdge[0] : blockBounds.mMaxEdge[0];
		return true;
	} 
    else if (xInside && zInside) 
    {
        check = Vector3<short>{ posCamera[0], 0, posCamera[2] };
		check[1] = (posCamera[1] <= blockBounds.mMinEdge[1]) ?
            blockBounds.mMinEdge[1] : blockBounds.mMaxEdge[1];
		return true;
	}

	// diagonal
	if (xInside) 
    {
        check = Vector3<short>{ posCamera[0], 0, 0 };
		check[1] = (posCamera[1] <= blockBounds.mMinEdge[1]) ?
            blockBounds.mMinEdge[1] : blockBounds.mMaxEdge[1];
		check[2] = (posCamera[2] <= blockBounds.mMinEdge[2]) ?
            blockBounds.mMinEdge[2] : blockBounds.mMaxEdge[2];
		return true;
	} 
    else if (yInside) 
    {
        check = Vector3<short>{ 0, posCamera[1], 0 };
		check[0] = (posCamera[0] <= blockBounds.mMinEdge[0]) ?
            blockBounds.mMinEdge[0] : blockBounds.mMaxEdge[0];
		check[2] = (posCamera[2] <= blockBounds.mMinEdge[2]) ?
            blockBounds.mMinEdge[2] : blockBounds.mMaxEdge[2];
		return true;
	} 
    else if (zInside) 
    {
        check = Vector3<short>{ 0, 0, posCamera[2] };
		check[0] = (posCamera[0] <= blockBounds.mMinEdge[0]) ?
            blockBounds.mMinEdge[0] : blockBounds.mMaxEdge[0];
		check[1] = (posCamera[1] <= blockBounds.mMinEdge[1]) ?
            blockBounds.mMinEdge[1] : blockBounds.mMaxEdge[1];
		return true;
	}

	// Closest node would be a corner, none returned
	return false;
}

bool Map::IsOccluded(const Vector3<short>& posCamera, const Vector3<short>& posTarget,
	float step, float stepfac, float offset, float endOffset, unsigned int neededCount)
{
    Vector3<short> dir = (posTarget - posCamera) * (short)BS;
    Vector3<float> direction{ (float)dir[0], (float)dir[1], (float)dir[2] };
	float distance = Length(direction);

	// Normalize direction vector
	if (distance > 0.0f)
		direction /= distance;

    Vector3<short> pos = posCamera * (short)BS;
    Vector3<float> posOrigin{ (float)pos[0], (float)pos[1], (float)pos[2]};
	unsigned int count = 0;
	bool isValidPosition;

	for (; offset < distance + endOffset; offset += step) 
    {
        Vector3<float> p = posOrigin + direction * offset;
        Vector3<short> posNode;
        posNode[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        posNode[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        posNode[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
            
		MapNode node = GetNode(posNode, &isValidPosition);
		if (isValidPosition && !mEnvironment->GetNodeManager()->Get(node).lightPropagates)
        {
			// Cannot see through light-blocking nodes --> occluded
			count++;
			if (count >= neededCount)
				return true;
		}
		step *= stepfac;
	}
	return false;
}

bool Map::IsBlockOccluded(MapBlock* block, Vector3<short> camPositionNodes)
{
	// Check occlusion for center and all 8 corners of the mapblock
	// Overshoot a little for less flickering
	static const short bs2 = MAP_BLOCKSIZE / 2 + 1;
	static const Vector3<short> dir9[9] = {
        Vector3<short>{ 0,  0,  0},
        Vector3<short>{ 1,  1,  1} *bs2,
        Vector3<short>{ 1,  1, -1} *bs2,
        Vector3<short>{ 1, -1,  1} *bs2,
        Vector3<short>{ 1, -1, -1} *bs2,
        Vector3<short>{-1,  1,  1} *bs2,
        Vector3<short>{-1,  1, -1} *bs2,
        Vector3<short>{-1, -1,  1} *bs2,
        Vector3<short>{-1, -1, -1} *bs2,
	};

	Vector3<short> posBlockCenter = 
        block->GetRelativePosition() + Vector3<short>{MAP_BLOCKSIZE / 2};

	// Starting step size, value between 1m and sqrt(3)m
	float step = BS * 1.2f;
	// Multiply step by each iteraction by 'stepfac' to reduce checks in distance
	float stepfac = 1.05f;

	float startOffset = BS * 1.0f;

	// The occlusion search of 'IsOccluded()' must stop short of the target
	// point by distance 'endOffset' to not enter the target mapblock.
	// For the 8 mapblock corners 'endOffset' must therefore be the maximum
	// diagonal of a mapblock, because we must consider all view angles.
	// sqrt(1^2 + 1^2 + 1^2) = 1.732
	float endOffset = -BS * MAP_BLOCKSIZE * 1.732f;

	// to reduce the likelihood of falsely occluded blocks
	// require at least two solid blocks
	// this is a HACK, we should think of a more precise algorithm
	unsigned int neededCount = 2;

	// Additional occlusion check, see comments in that function
	Vector3<short> check;
	if (DetermineAdditionalOcclusionCheck(camPositionNodes, block->GetBoundingBox(), check)) 
    {
		// node is always on a side facing the camera, endOffset can be lower
		if (!IsOccluded(camPositionNodes, check, step, stepfac, startOffset, -1.0f, neededCount))
			return false;
	}

	for (const Vector3<short>& dir : dir9) 
    {
		if (!IsOccluded(camPositionNodes, posBlockCenter + dir, step, stepfac, startOffset, endOffset, neededCount))
			return false;
	}
	return true;
}

/*
	LogicMap
*/
LogicMap::LogicMap(const std::string& savedir, MetricsBackend* mb, LogicEnvironment* env):
	Map(env), mSettingsMgr(savedir + "/map_meta.txt")
{
	LogInformation(FUNCTION_NAME);

	/*
		Try to load map; if not found, create a new one.
	*/

	// Determine which database backend to use
	std::string confPath = savedir + "/world.mt";
	Settings conf;
	bool succeeded = conf.ReadConfigFile(confPath.c_str());
	if (!succeeded || !conf.Exists("backend")) 
    {
		// fall back to db
		conf.Set("backend", "db");
	}
	std::string backend = conf.Get("backend");
	mDatabase = CreateDatabase(backend, savedir);
	if (conf.Exists("readonly_backend")) 
    {
		std::string readonlyDir = savedir + "/readonly";
		mDatabaseRo = CreateDatabase(conf.Get("readonly_backend"), readonlyDir);
	}
	if (!conf.UpdateConfigFile(confPath.c_str()))
		LogError("LogicMap::LogicMap(): Failed to update world.mt!");

	mSavedir = ToWideString(savedir);
	mMapSavingEnabled = false;

	mSaveTimeCounter = mb->AddCounter(
        "minetest_core_map_save_time", "Map save time (in nanoseconds)");

	mMapCompressionLevel = 
        std::clamp(Settings::Get()->GetInt("map_compression_level_disk"), -1, 9);

	try 
    {

		// If directory exists, check contents and load if possible
		if (FileSystem::Get()->ExistDirectory(mSavedir))
        {
			// If directory is empty, it is safe to save into it.
            std::vector<std::wstring> menuFiles;
            FileSystem::Get()->GetFileList(menuFiles, mSavedir);

			if (menuFiles.empty())
            {
				LogInformation("LogicMap: Empty save directory is valid.");
				mMapSavingEnabled = true;
			}
			else
			{
				if (mSettingsMgr.LoadMapMeta()) 
                {
					LogInformation("LogicMap: Metadata loaded from " + savedir);
				} 
                else 
                {
					LogInformation("LogicMap: Metadata could not be loaded "
						"from " + savedir + ", assuming valid save directory.");
				}

				mMapSavingEnabled = true;
				// Map loaded, not creating new one
				return;
			}
		}
		// If directory doesn't exist, it is safe to save to it
		else
        {
			mMapSavingEnabled = true;
		}
	}
	catch(std::exception& e)
	{
		LogWarning("LogicMap: Failed to load map from " + savedir + ", exception: " + e.what());
		LogInformation("Please remove the map or fix it.");
		LogWarning("Map saving will be disabled.");
	}
}

LogicMap::~LogicMap()
{
	LogInformation(FUNCTION_NAME);

	try
	{
		if (mMapSavingEnabled) 
        {
			// Save only changed parts
			Save(ModifiedState::MOD_STATE_WRITE_AT_UNLOAD);
			LogInformation(L"LogicMap: Saved map to " + mSavedir);
		} 
        else 
        {
			LogInformation("LogicMap: Map not saved");
		}
	}
	catch(std::exception& e)
	{
		LogInformation("LogicMap: Failed to save map to " + 
            ToString(mSavedir) + ", exception: " + e.what());
	}

	/*
		Close database if it was opened
	*/
	delete mDatabase;
	delete mDatabaseRo;
}

MapGeneratorParams* LogicMap::GetMapGeneratorParams()
{
	// GetMapGeneratorParams() should only ever be called after Logic is initialized
	LogAssert(mSettingsMgr.mMapGenParams != NULL, "invalid mapgen parameters");
	return mSettingsMgr.mMapGenParams;
}

uint64_t LogicMap::GetSeed()
{
	return GetMapGeneratorParams()->seed;
}

bool LogicMap::BlockPositionOverMapgenLimit(Vector3<short> pos)
{
	const short mapgenLimitBP = 
        std::clamp(GetMapGeneratorParams()->mapgenLimit, (short)0, (short)MAX_MAP_GENERATION_LIMIT) / MAP_BLOCKSIZE;
	return pos[0] < -mapgenLimitBP || pos[0] >  mapgenLimitBP ||
        pos[1] < -mapgenLimitBP || pos[1] >  mapgenLimitBP ||
        pos[2] < -mapgenLimitBP || pos[2] >  mapgenLimitBP;
}

bool LogicMap::InitBlockMake(Vector3<short> blockPos, BlockMakeData* data)
{
	short csize = GetMapGeneratorParams()->chunkSize;
	Vector3<short> bpmin = EmergeManager::GetContainingChunk(blockPos, csize);
    Vector3<short> bpmax = bpmin + Vector3<short>{1, 1, 1} * (short)(csize - 1);

	if (!mChunksInProgress.insert(bpmin).second)
		return false;

    if (EmergeManager::Get()->mEnableMapgenDebugInfo)
        LogInformation("InitBlockMake(): (" + std::to_string(bpmin[0]) +
            "," + std::to_string(bpmin[1]) + "," + std::to_string(bpmin[2]) + 
            ") - (" + std::to_string(bpmax[0]) + "," + std::to_string(bpmax[1]) + 
            "," + std::to_string(bpmax[2]) + ")");

    Vector3<short> extraBorders{ 1, 1, 1 };
	Vector3<short> fullBPmin = bpmin - extraBorders;
	Vector3<short> fullBPmax = bpmax + extraBorders;

	// Do nothing if not inside mapgen limits (+-1 because of neighbors)
	if (BlockPositionOverMapgenLimit(fullBPmin) ||
        BlockPositionOverMapgenLimit(fullBPmax))
		return false;

	data->seed = GetSeed();
	data->blockPosMin = bpmin;
	data->blockPosMax = bpmax;
	data->nodeMgr = mEnvironment->GetNodeManager();

	/*
		Create the whole area of this and the neighboring blocks
	*/
    for (short x = fullBPmin[0]; x <= fullBPmax[0]; x++)
    {
        for (short z = fullBPmin[2]; z <= fullBPmax[2]; z++) 
        {
            Vector2<short> sectorPos{ x, z };
            // Sector metadata is loaded from disk if not already loaded.
            MapSector* sector = CreateSector(sectorPos);
            LogAssert(sector, "CreateSector() failed");

            for (short y = fullBPmin[1]; y <= fullBPmax[1]; y++) 
            {
                Vector3<short> p{ x, y, z };

                MapBlock* block = EmergeBlock(p, false);
                if (block == NULL) 
                {
                    block = CreateBlock(p);

                    // Block gets sunlight if this is true.
                    // Refer to the map generator heuristics.
                    bool ug = EmergeManager::Get()->IsBlockUnderground(p);
                    block->SetIsUnderground(ug);
                }
            }
        }
    }

	/*
		Now we have a big empty area.

		Make a ManualMapVoxelManipulator that contains this and the
		neighboring blocks
	*/

	data->vmanip = new MMVManip(this);
	data->vmanip->InitialEmerge(fullBPmin, fullBPmax);

	// Data is ready now.
	return true;
}

void LogicMap::FinishBlockMake(BlockMakeData* data,
	std::map<Vector3<short>, MapBlock*>* changedBlocks)
{
	Vector3<short> bpmin = data->blockPosMin;
	Vector3<short> bpmax = data->blockPosMax;

	if (EmergeManager::Get()->mEnableMapgenDebugInfo)
        LogInformation("FinishBlockMake(): (" + std::to_string(bpmin[0]) +
            "," + std::to_string(bpmin[1]) + "," + std::to_string(bpmin[2]) +
            ") - (" + std::to_string(bpmax[0]) + "," + std::to_string(bpmax[1]) +
            "," + std::to_string(bpmax[2]) + ")");

	/*
		Blit generated stuff to map
		NOTE: BlitBackAll adds nearly everything to ChangedBlocks
	*/
	data->vmanip->BlitBackAll(changedBlocks);

    LogInformation("FinishBlockMake: ChangedBlocks.size()=" + 
        std::to_string(changedBlocks->size()));

	/*
		Copy transforming liquid information
	*/
	while (data->transformingLiquid.size()) 
    {
		mTransformingLiquid.push(data->transformingLiquid.front());
		data->transformingLiquid.pop();
	}

	for (auto& changedBlock : *changedBlocks) 
    {
		MapBlock* block = changedBlock.second;
		if (!block)
			continue;
		/*
			Update day/night difference cache of the MapBlocks
		*/
		block->ExpireDayNightDiff();
		/*
			Set block as modified
		*/
		block->RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_EXPIRE_DAYNIGHTDIFF);
	}

	/*
		Set central blocks as generated
	*/
    for (short x = bpmin[0]; x <= bpmax[0]; x++)
    {
        for (short z = bpmin[2]; z <= bpmax[2]; z++)
        {
            for (short y = bpmin[1]; y <= bpmax[1]; y++) 
            {
                MapBlock* block = GetBlockNoCreateNoEx(Vector3<short>{x, y, z});
                if (!block)
                    continue;

                block->SetGenerated(true);
            }
        }
    }

	/*
		Save changed parts of map
		NOTE: Will be saved later.
	*/
	//Save(MOD_STATE_WRITE_AT_UNLOAD);
	mChunksInProgress.erase(bpmin);
}

MapSector* LogicMap::CreateSector(Vector2<short> p2d)
{
	/*
		Check if it exists already in memory
	*/
	MapSector* sector = GetSectorNoGenerate(p2d);
	if (sector)
		return sector;

	/*
		Do not create over max mapgen limit
	*/
	const short maxLimitBP = MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE;
	if (p2d[0] < -maxLimitBP || p2d[0] >  maxLimitBP ||
        p2d[1] < -maxLimitBP || p2d[1] >  maxLimitBP)
		throw InvalidPositionException("CreateSector(): pos. over max mapgen limit");

	/*
		Generate blank sector
	*/

	sector = new MapSector(this, p2d);

	// Sector position on map in nodes
	//Vector2<short> nodepos2d = p2d * MAP_BLOCKSIZE;

	/*
		Insert to container
	*/
	mSectors[p2d] = sector;
	return sector;
}

MapBlock* LogicMap::CreateBlock(Vector3<short> pos)
{
	/*
		Do not create over max mapgen limit
	*/
	if (BlockPositionOverMaxLimit(pos))
		throw InvalidPositionException("CreateBlock(): pos. over max mapgen limit");

    Vector2<short> p2d{ pos[0], pos[2] };
	short blockY = pos[1];
	/*
		This will create or load a sector if not found in memory.
		If block exists on disk, it will be loaded.

		NOTE: On old save formats, this will be slow, as it generates
		      lighting on blocks for them.
	*/
	MapSector* sector;
	try 
    {
		sector = CreateSector(p2d);
	} 
    catch (InvalidPositionException &e) 
    {
		LogInformation("CreateBlock: CreateSector() failed");
		throw e;
	}

	/*
		Try to get a block from the sector
	*/

	MapBlock* block = sector->GetBlockNoCreateNoEx(blockY);
	if (block) 
    {
		if(block->IsDummy())
			block->UnDummify();
		return block;
	}
	// Create blank
	block = sector->CreateBlankBlock(blockY, mEnvironment);
	return block;
}

MapBlock* LogicMap::EmergeBlock(Vector3<short> pos, bool createBlank)
{
	{
		MapBlock* block = GetBlockNoCreateNoEx(pos);
		if (block && !block->IsDummy())
			return block;
	}

	{
		MapBlock* block = LoadBlock(pos);
		if(block)
			return block;
	}

	if (createBlank) 
    {
        MapSector* sector = CreateSector(Vector2<short>{pos[0], pos[2]});
		MapBlock* block = sector->CreateBlankBlock(pos[1], mEnvironment);

		return block;
	}

	return NULL;
}

MapBlock* LogicMap::GetBlockOrEmerge(Vector3<short> p3d)
{
	MapBlock* block = GetBlockNoCreateNoEx(p3d);
	if (block == NULL)
        EmergeManager::Get()->EnqueueBlockEmerge(INVALID_ACTOR_ID, p3d, false);

	return block;
}

// N.B.  This requires no synchronization, since data will not be modified unless
// the VoxelManipulator being updated belongs to the same thread.
void LogicMap::UpdateVManip(Vector3<short> pos)
{
	MapGenerator* mg = EmergeManager::Get()->GetCurrentMapGenerator();
	if (!mg)
		return;

	MMVManip* vm = mg->mMMVManip;
	if (!vm)
		return;

	if (!vm->mArea.Contains(pos))
		return;

	int idx = vm->mArea.Index(pos);
	vm->mData[idx] = GetNode(pos);
	vm->mFlags[idx] &= ~VOXELFLAG_NO_DATA;

	vm->mIsDirty = true;
}

void LogicMap::Save(ModifiedState saveLevel)
{
	if (!mMapSavingEnabled) 
    {
		LogWarning("Not saving map, saving disabled.");
		return;
	}

	unsigned int startTime = Timer::GetRealTime();

	if(saveLevel == MOD_STATE_CLEAN)
		LogInformation("LogicMap: Saving whole map, this can take time.");

	if (mMapMetadataChanged || saveLevel == MOD_STATE_CLEAN) 
    {
		if (mSettingsMgr.SaveMapMeta())
			mMapMetadataChanged = false;
	}

	// Profile modified reasons
	Profiler modProfiler;

	unsigned int blockCount = 0;
	unsigned int blockCountAll = 0; // Number of blocks in memory

	// Don't do anything with db unless something is really saved
	bool saveStarted = false;

	for (auto& sectorIt : mSectors) 
    {
		MapSector* sector = sectorIt.second;

		MapBlockVec blocks;
		sector->GetBlocks(blocks);

		for (MapBlock* block : blocks) 
        {
			blockCountAll++;

			if(block->GetModified() >= (unsigned int)saveLevel) 
            {
				// Lazy BeginSave()
				if(!saveStarted) 
                {
					BeginSave();
					saveStarted = true;
				}

				modProfiler.Add(block->GetModifiedReasonString(), 1);

				SaveBlock(block);
				blockCount++;
			}
		}
	}

	if(saveStarted)
		EndSave();

	/*
		Only print if something happened or saved whole map
	*/
	if(saveLevel == MOD_STATE_CLEAN || blockCount != 0) 
    {
        std::stringstream infostream;
        infostream << "LogicMap: Written: " << blockCount << " blocks"
            << ", " << blockCountAll << " blocks in memory." << std::endl;
        PrintInfo(infostream); // LogicMap/VisualMap:
        infostream << "Blocks modified by: " << std::endl;
		modProfiler.Print(infostream);
        LogInformation(infostream.str());
	}

    auto endTime = Timer::GetRealTime();
	mSaveTimeCounter->Increment(endTime - startTime);
}

void LogicMap::ListAllLoadableBlocks(std::vector<Vector3<short>> &dst)
{
	mDatabase->ListAllLoadableBlocks(dst);
	if (mDatabaseRo)
		mDatabaseRo->ListAllLoadableBlocks(dst);
}

void LogicMap::ListAllLoadedBlocks(std::vector<Vector3<short>> &dst)
{
	for (auto &sectorIt : mSectors) 
    {
		MapSector* sector = sectorIt.second;

		MapBlockVec blocks;
		sector->GetBlocks(blocks);
		for (MapBlock* block : blocks) 
        {
			Vector3<short> p = block->GetPosition();
			dst.push_back(p);
		}
	}
}

MapDatabase* LogicMap::CreateDatabase(
	const std::string& name, const std::string& savedir)
{
    MapDatabase* map = new MapDatabase(savedir, "map.bin");
    map->LoadMap(savedir + "/map.bin");
    return map;
}

void LogicMap::BeginSave()
{
	mDatabase->BeginSave();
}

void LogicMap::EndSave()
{
	mDatabase->EndSave();
}

bool LogicMap::SaveBlock(MapBlock* block)
{
	return SaveBlock(block, mDatabase, mMapCompressionLevel);
}

bool LogicMap::SaveBlock(MapBlock* block, MapDatabase* db, int compressionLevel)
{
	Vector3<short> p3d = block->GetPosition();

	// Dummy blocks are not written
	if (block->IsDummy()) 
    {
		LogWarning("SaveBlock: Not writing dummy block (" + std::to_string(p3d[0]) + 
            "," + std::to_string(p3d[1]) + "," + std::to_string(p3d[2]) + ")");
		return true;
	}

	// Format used for writing
	uint8_t version = SER_FMT_VER_HIGHEST_WRITE;

	/*
		[0] uint8_t serialization version
		[1] data
	*/
	std::ostringstream o(std::ios_base::binary);
	o.write((char*) &version, 1);
	block->Serialize(o, version, true, compressionLevel);

	bool ret = db->SaveBlock(p3d, o.str());
	if (ret) 
    {
		// We just wrote it to the disk so clear modified flag
		block->ResetModified();
	}
	return ret;
}

void LogicMap::LoadBlock(std::string* blob, Vector3<short> p3d, MapSector* sector, bool saveAfterLoad)
{
	try 
    {
		std::istringstream is(*blob, std::ios_base::binary);

		uint8_t version = SER_FMT_VER_INVALID;
		is.read((char*)&version, 1);

		if(is.fail())
			throw SerializationError("LogicMap::LoadBlock(): Failed to read MapBlock version");

		MapBlock* block = NULL;
		bool createdNew = false;
		block = sector->GetBlockNoCreateNoEx(p3d[1]);
		if(block == NULL)
		{
			block = sector->CreateBlankBlockNoInsert(p3d[1], mEnvironment);
			createdNew = true;
		}

		// Read basic data
		block->Deserialize(is, version, true);

		// If it's a new block, insert it to the map
		if (createdNew) 
        {
			sector->InsertBlock(block);
			ReflowScan scanner(this, EmergeManager::Get()->mNodeMgr);
			scanner.Scan(block, &mTransformingLiquid);
		}

		/*
			Save blocks loaded in old format in new format
		*/

		//if(version < SER_FMT_VER_HIGHEST_READ || saveAfterLoad)
		// Only save if asked to; no need to update version
		if(saveAfterLoad)
			SaveBlock(block);

		// We just loaded it from, so it's up-to-date.
		block->ResetModified();
	}
	catch(SerializationError& e)
	{
		LogWarning("Invalid block data in database (" + 
            std::to_string(p3d[0]) + "," + std::to_string(p3d[1]) + "," + 
            std::to_string(p3d[2]) + ") (SerializationError): " + e.what());

		// TODO: Block should be marked as invalid in memory so that it is
		// not touched but the game can run

		if(Settings::Get()->GetBool("ignore_world_load_errors"))
        {
			LogWarning("Ignoring block load error. Duck and cover!");
		} 
        else 
        {
			throw SerializationError("Invalid block data in database");
		}
	}
}

MapBlock* LogicMap::LoadBlock(Vector3<short> blockPos)
{
	bool createdNew = (GetBlockNoCreateNoEx(blockPos) == NULL);

    Vector2<short> p2d{ blockPos[0], blockPos[2] };

	std::string ret;
	mDatabase->LoadBlock(blockPos, &ret);

	if (!ret.empty()) 
    {
		LoadBlock(&ret, blockPos, CreateSector(p2d), false);
	} 
    else if (mDatabaseRo) 
    {
		mDatabaseRo->LoadBlock(blockPos, &ret);
		if (!ret.empty()) 
        {
			LoadBlock(&ret, blockPos, CreateSector(p2d), false);
		}
	} 
    else 
    {
		return NULL;
	}

	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if (createdNew && (block != NULL)) 
    {
		std::map<Vector3<short>, MapBlock*> modifiedBlocks;
		// Fix lighting if necessary
		UpdateBlockBorderLighting(this, block, modifiedBlocks);
		if (!modifiedBlocks.empty()) 
        {
			//Modified lighting, send event
			MapEditEvent evt;
			evt.type = MEET_OTHER;
			std::map<Vector3<short>, MapBlock *>::iterator it;
			for (it = modifiedBlocks.begin(); it != modifiedBlocks.end(); ++it) 
				evt.modifiedBlocks.insert(it->first);
			DispatchEvent(evt);
		}
	}

	return block;
}

bool LogicMap::DeleteBlock(Vector3<short> blockPos)
{
	if (!mDatabase->DeleteBlock(blockPos))
		return false;

	MapBlock* block = GetBlockNoCreateNoEx(blockPos);
	if (block) 
    {
        Vector2<short> p2d{ blockPos[0], blockPos[2] };
		MapSector* sector = GetSectorNoGenerate(p2d);
		if (!sector)
			return false;
		sector->DeleteBlock(block);
	}

	return true;
}

void LogicMap::PrintInfo(std::ostream& out)
{
	out<<"LogicMap: ";
}

bool LogicMap::RepairBlockLight(Vector3<short> blockPos,
	std::map<Vector3<short>, MapBlock *>* modifiedBlocks)
{
	MapBlock* block = EmergeBlock(blockPos, false);
	if (!block || !block->IsGenerated())
		return false;
	::RepairBlockLight(this, block, modifiedBlocks);
	return true;
}

MMVManip::MMVManip(Map *map) : VoxelManipulator(), mMap(map)
{

}

void MMVManip::InitialEmerge(Vector3<short> blockPosMin, Vector3<short> blockPosMax, bool loadIfInexistent)
{
	TimeTaker timer1("InitialEmerge", &EmergeTime);

	// Units of these are MapBlocks
	Vector3<short> pMin = blockPosMin;
	Vector3<short> pMax = blockPosMax;

    VoxelArea blockAreaNodes(pMin * (short)MAP_BLOCKSIZE, 
        (pMax + Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1});

	unsigned int sizeMB = blockAreaNodes.GetVolume()*4/1000000;
	if(sizeMB >= 1)
	{
        std::stringstream infostream;
        infostream << "InitialEmerge: area: ";
		blockAreaNodes.Print(infostream);
		infostream<<" ("<<sizeMB<<"MB)";
		infostream<<std::endl;
        LogInformation(infostream.str());
	}

	AddArea(blockAreaNodes);

    for (int z = pMin[2]; z <= pMax[2]; z++)
    {
        for (int y = pMin[1]; y <= pMax[1]; y++)
        {
            for (int x = pMin[0]; x <= pMax[0]; x++)
            {
                uint8_t flags = 0;
                MapBlock* block;
                Vector3<short> p{ (short)x, (short)y, (short)z };
                std::map<Vector3<short>, uint8_t>::iterator n;
                n = mLoadedBlocks.find(p);
                if (n != mLoadedBlocks.end())
                    continue;

                bool blockDataInexistent = false;
                {
                    TimeTaker timer2("emerge load", &EmergeLoadTime);

                    block = mMap->GetBlockNoCreateNoEx(p);
                    if (!block || block->IsDummy())
                        blockDataInexistent = true;
                    else
                        block->CopyTo(*this);
                }

                if (blockDataInexistent)
                {

                    if (loadIfInexistent && !BlockPositionOverMaxLimit(p)) 
                    {
                        LogicMap* svrmap = (LogicMap *)mMap;
                        block = svrmap->EmergeBlock(p, false);
                        if (block == NULL)
                            block = svrmap->CreateBlock(p);
                        block->CopyTo(*this);
                    }
                    else 
                    {
                        flags |= VMANIP_BLOCK_DATA_INEXIST;

                        /*
                            Mark area inexistent
                        */
                        VoxelArea area(p * (short)MAP_BLOCKSIZE, 
                            (p + Vector3<short>{1, 1, 1}) * (short)MAP_BLOCKSIZE - Vector3<short>{1, 1, 1});
                        // Fill with VOXELFLAG_NO_DATA
                        for (int z = area.mMinEdge[2]; z <= area.mMaxEdge[2]; z++)
                        {
                            for (int y = area.mMinEdge[1]; y <= area.mMaxEdge[1]; y++)
                            {
                                int i = mArea.Index(area.mMinEdge[0], y, z);
                                memset(&mFlags[i], VOXELFLAG_NO_DATA, MAP_BLOCKSIZE);
                            }
                        }
                    }
                }
                /*else if (block->GetNode(0, 0, 0).GetContent() == CONTENT_IGNORE)
                {
                    // Mark that block was loaded as blank
                    flags |= VMANIP_BLOCK_CONTAINS_CIGNORE;
                }*/

                mLoadedBlocks[p] = flags;
            }
        }
    }

	mIsDirty = false;
}

void MMVManip::BlitBackAll(std::map<Vector3<short>, MapBlock*>* modifiedBlocks, bool overwriteGenerated)
{
	if(mArea.GetExtent() == Vector3<short>::Zero())
		return;

	/*
		Copy data of all blocks
	*/
	for (auto& loadedBlock : mLoadedBlocks) 
    {
		Vector3<short> p = loadedBlock.first;
		MapBlock* block = mMap->GetBlockNoCreateNoEx(p);
		bool existed = !(loadedBlock.second & VMANIP_BLOCK_DATA_INEXIST);
		if (!existed || (block == NULL) ||
			(!overwriteGenerated && block->IsGenerated()))
			continue;

		block->CopyFrom(*this);
		block->RaiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_VMANIP);

		if(modifiedBlocks)
			(*modifiedBlocks)[p] = block;
	}
}

//END
