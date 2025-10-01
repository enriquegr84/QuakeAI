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

#include "MapBlock.h"

#include "ContentMapNode.h"
#include "ContentNodeMeta.h"

#include "Map.h"
#include "Voxel.h"

#include "Core/Utility/Serialize.h"

static const char *ModifiedReasonStrings[] = {
	"Initial",
	"Reallocate",
	"SetIsUnderground",
	"SetLightingExpired",
	"SetGenerated",
	"SetNode",
	"SetNodeNoCheck",
	"SetTimestamp",
	"NodeMeta::reportMetadataChange",
	"ClearAllObjects",
	"Timestamp expired (step)",
	"AddActiveObjectRaw",
	"RemoveRemovedObjects/remove",
	"RemoveRemovedObjects/deactivate",
	"Stored list cleared in activateObjects due to overflow",
	"DeactivateFarObjects: Static data moved in",
	"DeactivateFarObjects: Static data moved out",
	"DeactivateFarObjects: Static data changed considerably",
	"FinishBlockMake: ExpireDayNightDiff",
	"Unknown",
};


/*
	MapBlock
*/

MapBlock::MapBlock(Map* parent, Environment* env, Vector3<short> pos, bool dummy) :
    mParent(parent), mEnvironment(env), mPosition(pos), mRelativePosition(pos * (short)MAP_BLOCKSIZE)
{
	if (!dummy)
		Reallocate();
}

MapBlock::~MapBlock()
{
    mMesh.reset();
    mMesh = nullptr;

	delete[] mData;
}

bool MapBlock::IsValidPositionParent(Vector3<short> pos)
{
	if (IsValidPosition(pos))
		return true;

	return mParent->IsValidPosition(GetRelativePosition() + pos);
}

MapNode MapBlock::GetNodeParent(Vector3<short> pos, bool* isValidPosition)
{
	if (!IsValidPosition(pos))
		return mParent->GetNode(GetRelativePosition() + pos, isValidPosition);

	if (!mData) 
    {
		if (isValidPosition)
			*isValidPosition = false;
		return {CONTENT_IGNORE};
	}
	if (isValidPosition)
		*isValidPosition = true;
	return mData[pos[2] * ZStride + pos[1] * YStride + pos[0]];
}

std::string MapBlock::GetModifiedReasonString()
{
	std::string reason;

	const size_t ubound = std::min(sizeof(mModifiedReason) * CHAR_BIT,
		sizeof(ModifiedReasonStrings) / sizeof(ModifiedReasonStrings[0]));

	for (unsigned int i = 0; i != ubound; i++) 
    {
		if ((mModifiedReason & (1 << i)) == 0)
			continue;

		reason += ModifiedReasonStrings[i];
		reason += ", ";
	}

	if (reason.length() > 2)
		reason.resize(reason.length() - 2);

	return reason;
}


void MapBlock::CopyTo(VoxelManipulator& dst)
{
    Vector3<short> dataSize{ MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE };
    VoxelArea dataArea(Vector3<short>::Zero(), dataSize - Vector3<short>{1, 1, 1});

	// Copy from data to VoxelManipulator
    dst.CopyFrom(mData, dataArea, Vector3<short>::Zero(), GetRelativePosition(), dataSize);
}

void MapBlock::CopyFrom(VoxelManipulator& dst)
{
    Vector3<short> dataSize{ MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE };
    VoxelArea dataArea(Vector3<short>::Zero(), dataSize - Vector3<short>{1, 1, 1});

	// Copy from VoxelManipulator to data
    dst.CopyTo(mData, dataArea, Vector3<short>{0, 0, 0}, GetRelativePosition(), dataSize);
}

void MapBlock::ActuallyUpdateDayNightDiff()
{
	const NodeManager* nodeMgr = mEnvironment->GetNodeManager();

	// Running this function un-expires mDayNightDiffers
	mDayNightDiffersExpired = false;
	if (!mData) 
    {
		mDayNightDiffers = false;
		return;
	}

	bool differs = false;

	/*
		Check if any lighting value differs
	*/

	MapNode previousNode(CONTENT_IGNORE);
	for (unsigned int i = 0; i < NodeCount; i++) 
    {
		MapNode n = mData[i];

		// If node is identical to previous node, don't verify if it differs
		if (n == previousNode)
			continue;

		differs = !n.IsLightDayNightEq(nodeMgr);
		if (differs)
			break;
        previousNode = n;
	}

	/*
		If some lighting values differ, check if the whole thing is
		just air. If it is just air, differs = false
	*/
	if (differs) 
    {
		bool onlyAir = true;
		for (unsigned int i = 0; i < NodeCount; i++) 
        {
			MapNode &n = mData[i];
			if (n.GetContent() != CONTENT_AIR) 
            {
				onlyAir = false;
				break;
			}
		}
		if (onlyAir)
			differs = false;
	}

	// Set member variable
	mDayNightDiffers = differs;
}

void MapBlock::ExpireDayNightDiff()
{
	if (!mData) 
    {
		mDayNightDiffers = false;
		mDayNightDiffersExpired = false;
		return;
	}

	mDayNightDiffersExpired = true;
}

short MapBlock::GetGroundLevel(Vector2<short> p2d)
{
	if(IsDummy())
		return -3;
	try
	{
		short y = MAP_BLOCKSIZE-1;
		for(; y>=0; y--)
		{
			MapNode n = GetNodeRef(p2d[0], y, p2d[1]);
			if (mEnvironment->GetNodeManager()->Get(n).walkable)
            {
				if(y == MAP_BLOCKSIZE-1)
					return -2;

				return y;
			}
		}
		return -1;
	}
	catch(InvalidPositionException&)
	{
		return -3;
	}
}

/*
	Serialization
*/
// List relevant id-name pairs for ids in the block using nodedef
// Renumbers the content IDs (starting at 0 and incrementing
// use static memory requires about 65535 * sizeof(int) ram in order to be
// sure we can handle all content ids. But it's absolutely worth it as it's
// a speedup of 4 for one of the major time consuming functions on storing
// mapblocks.
static uint16_t GetBlockNodeIdMapping[USHRT_MAX + 1];
static void GetBlockNodeIdMap(NameIdMapping* nimap, MapNode* nodes, const NodeManager* nodedef)
{
	memset(GetBlockNodeIdMapping, 0xFF, (USHRT_MAX + 1) * sizeof(uint16_t));

	std::set<uint16_t> unknownContents;
	uint16_t idCounter = 0;
	for (unsigned int i = 0; i < MapBlock::NodeCount; i++) 
    {
		uint16_t globalId = nodes[i].GetContent();
		uint16_t id = CONTENT_IGNORE;

		// Try to find an existing mapping
		if (GetBlockNodeIdMapping[globalId] != 0xFFFF) 
        {
			id = GetBlockNodeIdMapping[globalId];
		}
		else
		{
			// We have to assign a new mapping
			id = idCounter++;
			GetBlockNodeIdMapping[globalId] = id;

			const ContentFeatures& f = nodedef->Get(globalId);
			const std::string& name = f.name;
			if (name.empty())
				unknownContents.insert(globalId);
			else
				nimap->Set(id, name);
		}

		// Update the MapNode
		nodes[i].SetContent(id);
	}
	for (uint16_t unknownContent : unknownContents)
    {
		LogError ("GetBlockNodeIdMap(): IGNORING ERROR: "
				"Name for node id " + std::to_string(unknownContent) + " not known");
	}
}
// Correct ids in the block to match nodemgr based on names.
// Unknown ones are added to nodedef.
// Will not update itself to match id-name pairs in nodedef.
static void CorrectBlockNodeIds(const NameIdMapping* nimap, MapNode* nodes, Environment* env)
{
	const NodeManager* nodeMgr = env->GetNodeManager();
	// This means the block contains incorrect ids, and we contain
	// the information to convert those to names.
	// nodemgr contains information to convert our names to globally
	// correct ids.
	std::unordered_set<uint16_t> unnamedContents;
	std::unordered_set<std::string> unallocatableContents;

	bool previousExists = false;
	uint16_t previousLocalId = CONTENT_IGNORE;
	uint16_t previousGlobalId = CONTENT_IGNORE;

	for (unsigned int i = 0; i < MapBlock::NodeCount; i++) 
    {
		uint16_t localId = nodes[i].GetContent();
		// If previous node localId was found and same than before, don't lookup maps
		// apply directly previous resolved id
		// This permits to massively improve loading performance when nodes are similar
		// example: default:air, default:stone are massively present
		if (previousExists && localId == previousLocalId) 
        {
			nodes[i].SetContent(previousGlobalId);
			continue;
		}

		std::string name;
		if (!nimap->GetName(localId, name)) 
        {
			unnamedContents.insert(localId);
			previousExists = false;
			continue;
		}

		uint16_t globalId;
		if (!nodeMgr->GetId(name, globalId)) 
        {
			globalId = env->GetNodeManager()->AllocateDummy(name);
			if (globalId == CONTENT_IGNORE) 
            {
				unallocatableContents.insert(name);
				previousExists = false;
				continue;
			}
		}
		nodes[i].SetContent(globalId);

		// Save previous node localId & globalId result
		previousLocalId = localId;
		previousGlobalId = globalId;
		previousExists = true;
	}

	for (const uint16_t content: unnamedContents)
    {
		LogError("CorrectBlockNodeIds(): IGNORING ERROR: "
            "Block contains id " + std::to_string(content) + " with no name mapping");
	}
	for (const std::string &nodeName: unallocatableContents) 
    {
        LogError("CorrectBlockNodeIds(): IGNORING ERROR: "
            "Could not allocate global id for node name \"" + nodeName + "\"");
	}
}

void MapBlock::Serialize(std::ostream& os, uint8_t version, bool disk, int compressionLevel)
{
	if(!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

	if (!mData)
		throw SerializationError("ERROR: Not writing dummy block.");

	LogAssert(version >= SER_FMT_VER_LOWEST_WRITE, "Serialization version error");

	// First byte
    uint8_t flags = 0;
	if(mIsUnderground)
		flags |= 0x01;
	if(GetDayNightDiff())
		flags |= 0x02;
	if (!mGenerated)
		flags |= 0x08;
	WriteUInt8(os, flags);
	if (version >= 27)
		WriteUInt16(os, mLightingComplete);

	/*
		Bulk node data
	*/
	NameIdMapping nimap;
	if(disk)
	{
		MapNode* tmpNodes = new MapNode[NodeCount];
		for(unsigned int i=0; i<NodeCount; i++)
			tmpNodes[i] = mData[i];
		GetBlockNodeIdMap(&nimap, tmpNodes, mEnvironment->GetNodeManager());

        uint8_t contentWidth = 2;
        uint8_t paramsWidth = 2;
		WriteUInt8(os, contentWidth);
		WriteUInt8(os, paramsWidth);
		MapNode::SerializeBulk(os, version, tmpNodes, NodeCount, contentWidth, paramsWidth, compressionLevel);
		delete[] tmpNodes;
	}
	else
	{
		uint8_t contentWidth = 2;
		uint8_t paramsWidth = 2;
		WriteUInt8(os, contentWidth);
		WriteUInt8(os, paramsWidth);
		MapNode::SerializeBulk(os, version, mData, NodeCount, contentWidth, paramsWidth, compressionLevel);
	}

	/*
		Node metadata
	*/
	std::ostringstream oss(std::ios_base::binary);
    mMapNodeMetadata.Serialize(oss, version, disk);
	CompressZlib(oss.str(), os, compressionLevel);

	/*
		Data that goes to disk, but not the network
	*/
	if(disk)
	{
		if(version <= 24)
        {
			// Node timers
			mNodeTimers.Serialize(os, version);
		}

		// Static objects
		mStaticObjects.Serialize(os);

		// Timestamp
		WriteUInt32(os, GetTimestamp());

		// Write block-specific node definition id mapping
		nimap.Serialize(os);

		if(version >= 25)
        {
			// Node timers
			mNodeTimers.Serialize(os, version);
		}
	}
}

void MapBlock::SerializeNetworkSpecific(std::ostream& os)
{
	if (!mData) {
		throw SerializationError("ERROR: Not writing dummy block.");
	}

	WriteUInt8(os, 2); // version
}

void MapBlock::Deserialize(std::istream& is, uint8_t version, bool disk)
{
	if(!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

    Vector3<short> pos(GetPosition());
	/*
	LogInformation("MapBlock::Deserialize (" + std::to_string(pos[0]) + "," + 
        std::to_string(pos[1]) + "," + std::to_string(pos[2]) + ")");
	*/
	mDayNightDiffersExpired = false;
	if(version <= 21)
	{
		DeserializePre22(is, version, disk);
		return;
	}

    uint8_t flags = ReadUInt8(is);
	mIsUnderground = (flags & 0x01) != 0;
	mDayNightDiffers = (flags & 0x02) != 0;
	if (version < 27)
		mLightingComplete = 0xFFFF;
	else
		mLightingComplete = ReadUInt16(is);
	mGenerated = (flags & 0x08) == 0;

	/*
		Bulk node data
	
    LogInformation("MapBlock::Deserialize (" + std::to_string(pos[0]) + "," +
        std::to_string(pos[1]) + "," + std::to_string(pos[2]) + "): Bulk node data");
	*/
    uint8_t contentWidth = ReadUInt8(is);
    uint8_t paramsWidth = ReadUInt8(is);
	if(contentWidth != 1 && contentWidth != 2)
		throw SerializationError("MapBlock::Deserialize(): invalid contentWidth");
	if(paramsWidth != 2)
		throw SerializationError("MapBlock::Deserialize(): invalid paramsWidth");
	MapNode::DeserializeBulk(is, version, mData, NodeCount, contentWidth, paramsWidth);

	/*
		MapNodeMetadata

    LogInformation("MapBlock::Deserialize (" + std::to_string(pos[0]) + "," +
        std::to_string(pos[1]) + "," + std::to_string(pos[2]) + "): Node metadata");
	*/
	// Ignore errors
	try 
    {
		std::ostringstream oss(std::ios_base::binary);
		DecompressZlib(is, oss);
		std::istringstream iss(oss.str(), std::ios_base::binary);
		if (version >= 23)
			mMapNodeMetadata.Deserialize(iss, mEnvironment->GetItemManager());
		else
			ContentNodeMetaDeserializeLegacy(iss, &mMapNodeMetadata, &mNodeTimers, mEnvironment->GetItemManager());
	} 
    catch(SerializationError &e) 
    {
		LogWarning(
            "MapBlock::Deserialize(): Ignoring an error while deserializing node metadata at(" + 
            std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," + 
            std::to_string(pos[2]) + "): " + e.what());
	}

	/*
		Data that is only on disk
	*/
	if(disk)
	{
		// Node timers
		if(version == 23)
        {
			// Read unused zero
			ReadUInt8(is);
		}
		if(version == 24)
        {
			/*
			LogInformation("MapBlock::Deserialize (" +
                std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," +
                std::to_string(pos[2]) + "): Node timers (ver==24)");
			*/
			mNodeTimers.Deserialize(is, version);
		}

		/* Static objects
		LogInformation("MapBlock::Deserialize (" +
            std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," +
            std::to_string(pos[2]) + "): Static objects");
		*/
		mStaticObjects.Deserialize(is);

		/* Timestamp
        LogInformation("MapBlock::Deserialize (" +
            std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," +
            std::to_string(pos[2]) + "): Timestamp");
		*/
		SetTimestampNoChangedFlag(ReadUInt32(is));
		mDiskTimestamp = mTimestamp;

		/* Dynamically re - set ids based on node names
		LogInformation("MapBlock::Deserialize (" +
            std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," +
            std::to_string(pos[2]) + "): NameIdMapping");
		*/
		NameIdMapping nimap;
		nimap.Deserialize(is);
		CorrectBlockNodeIds(&nimap, mData, mEnvironment);

		if(version >= 25)
        {
			/*
            LogInformation("MapBlock::Deserialize (" +
                std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," +
                std::to_string(pos[2]) + ") Node timers (ver>=25)");
			*/
			mNodeTimers.Deserialize(is, version);
		}
	}
	/*
	LogInformation("MapBlock::Deserialize (" +
        std::to_string(pos[0]) + ", " + std::to_string(pos[1]) + "," +
        std::to_string(pos[2]) + "): Done.");
	*/
}

void MapBlock::DeserializeNetworkSpecific(std::istream& is)
{
	try 
    {
		ReadUInt8(is);
		//const uint8_t version = ReadUInt8(is);
		//if (version != 1)
			//throw SerializationError("unsupported MapBlock version");

	} 
    catch(SerializationError &e) 
    {
		LogWarning("MapBlock::DeserializeNetworkSpecific(): Ignoring an error : " + 
            std::string(e.what()));
	}
}

/*
	Legacy serialization
*/

void MapBlock::DeserializePre22(std::istream& is, uint8_t version, bool disk)
{
	// Initialize default flags
	mIsUnderground = false;
	mDayNightDiffers = false;
	mLightingComplete = 0xFFFF;
	mGenerated = true;

	// Make a temporary buffer
	unsigned int serLength = MapNode::SerializedLength(version);
	uint8_t* dataBufNodelist = new uint8_t[NodeCount * serLength];

	// These have no compression
	if (version <= 3 || version == 5 || version == 6) 
    {
		char tmp;
		is.read(&tmp, 1);
		if (is.gcount() != 1)
			throw SerializationError(std::string(FUNCTION_NAME) + ": not enough input data");
		mIsUnderground = tmp;
		is.read((char*)dataBufNodelist, NodeCount * serLength);
		if ((unsigned int)is.gcount() != NodeCount * serLength)
			throw SerializationError(std::string(FUNCTION_NAME) + ": not enough input data");
	} 
    else if (version <= 10) 
    {
        uint8_t t8;
		is.read((char *)&t8, 1);
		mIsUnderground = t8;

		{
			// Uncompress and set material data
			std::ostringstream os(std::ios_base::binary);
			Decompress(is, os, version);
			std::string s = os.str();
			if (s.size() != NodeCount)
				throw SerializationError(std::string(FUNCTION_NAME) + ": not enough input data");
			for (unsigned int i = 0; i < s.size(); i++) 
				dataBufNodelist[i*serLength] = s[i];
		}
		{
			// Uncompress and set param data
			std::ostringstream os(std::ios_base::binary);
			Decompress(is, os, version);
			std::string s = os.str();
			if (s.size() != NodeCount)
				throw SerializationError(std::string(FUNCTION_NAME) + ": not enough input data");
			for (unsigned int i = 0; i < s.size(); i++)
                dataBufNodelist[i*serLength + 1] = s[i];
		}

		if (version >= 10) 
        {
			// Uncompress and set param2 data
			std::ostringstream os(std::ios_base::binary);
			Decompress(is, os, version);
			std::string s = os.str();
			if (s.size() != NodeCount)
				throw SerializationError(std::string(FUNCTION_NAME) + ": not enough input data");
			for (unsigned int i = 0; i < s.size(); i++) 
                dataBufNodelist[i*serLength + 2] = s[i];
		}
	} 
    else 
    { // All other versions (10 to 21)
		uint8_t flags;
		is.read((char*)&flags, 1);
		mIsUnderground = (flags & 0x01) != 0;
		mDayNightDiffers = (flags & 0x02) != 0;
		if(version >= 18)
			mGenerated = (flags & 0x08) == 0;

		// Uncompress data
		std::ostringstream os(std::ios_base::binary);
		Decompress(is, os, version);
		std::string str = os.str();
		if (str.size() != NodeCount * 3)
			throw SerializationError(std::string(FUNCTION_NAME)
				+ ": decompress resulted in size other than mNodeCount*3");

		// Deserialize nodes from buffer
		for (unsigned int i = 0; i < NodeCount; i++) 
        {
            dataBufNodelist[i*serLength] = str[i];
            dataBufNodelist[i*serLength + 1] = str[i+NodeCount];
            dataBufNodelist[i*serLength + 2] = str[i+NodeCount*2];
		}

		/*
			MapNodeMetadata
		*/
		if (version >= 14) 
        {
			// Ignore errors
			try 
            {
				if (version <= 15) 
                {
					std::string data = DeserializeString16(is);
					std::istringstream iss(data, std::ios_base::binary);
                    ContentNodeMetaDeserializeLegacy(iss,
						&mMapNodeMetadata, &mNodeTimers, mEnvironment->GetItemManager());
				} 
                else 
                {
					//std::string data = DeserializeString32(is);
					std::ostringstream oss(std::ios_base::binary);
					DecompressZlib(is, oss);
					std::istringstream iss(oss.str(), std::ios_base::binary);
                    ContentNodeMetaDeserializeLegacy(iss,
						&mMapNodeMetadata, &mNodeTimers, mEnvironment->GetItemManager());
				}
			} 
            catch(SerializationError&) 
            {
				LogWarning("MapBlock::Deserialize(): Ignoring an error while deserializing node metadata");
			}
		}
	}

	// Deserialize node data
	for (unsigned int i = 0; i < NodeCount; i++) 
		mData[i].Deserialize(&dataBufNodelist[i * serLength], version);

	delete[] dataBufNodelist;

	if (disk) 
    {
		/*
			Versions up from 9 have block objects. (DEPRECATED)
		*/
		if (version >= 9) 
        {
			unsigned short count = ReadUInt16(is);
			// Not supported and length not known if count is not 0
			if(count != 0)
            {
				LogWarning("MapBlock::DeserializePre22(): Ignoring stuff coming at and after MBOs");
				return;
			}
		}

		/*
			Versions up from 15 have static objects.
		*/
		if (version >= 15)
			mStaticObjects.Deserialize(is);

		// Timestamp
		if (version >= 17) 
        {
			SetTimestampNoChangedFlag(ReadUInt32(is));
			mDiskTimestamp = mTimestamp;
		}
        else 
        {
			SetTimestampNoChangedFlag(BLOCK_TIMESTAMP_UNDEFINED);
		}

		// Dynamically re-set ids based on node names
		NameIdMapping nimap;
		// If supported, read node definition id mapping
		if (version >= 21) 
        {
			nimap.Deserialize(is);
		// Else set the legacy mapping
		} 
        else 
        {
			ContentMapNodeGetNameIdMapping(&nimap);
		}
		CorrectBlockNodeIds(&nimap, mData, mEnvironment);
	}


	// Legacy data changes
	// This code has to convert from pre-22 to post-22 format.
	const NodeManager* nodeMgr = mEnvironment->GetNodeManager();
	for(unsigned int i=0; i<NodeCount; i++)
	{
		const ContentFeatures& f = nodeMgr->Get(mData[i].GetContent());
		// Mineral
		if(nodeMgr->GetId("default:stone") == mData[i].GetContent() && mData[i].GetParam1() == 1)
		{
			mData[i].SetContent(nodeMgr->GetId("default:stone_with_coal"));
            mData[i].SetParam1(0);
		}
		else if(nodeMgr->GetId("default:stone") == mData[i].GetContent() && mData[i].GetParam1() == 2)
		{
            mData[i].SetContent(nodeMgr->GetId("default:stone_with_iron"));
            mData[i].SetParam1(0);
		}
		// faceDir_simple
		if(f.legacyFacedirSimple)
		{
            mData[i].SetParam2(mData[i].GetParam1());
            mData[i].SetParam1(0);
		}
		// wall_mounted
		if(f.legacyWallmounted)
		{
			uint8_t wallmountedNewToOld[8] = {0x04, 0x08, 0x01, 0x02, 0x10, 0x20, 0, 0};
            uint8_t dirOldFormat = mData[i].GetParam2();
            uint8_t dirNewFormat = 0;
			for(uint8_t j=0; j<8; j++)
			{
				if((dirOldFormat & wallmountedNewToOld[j]) != 0)
				{
					dirNewFormat = j;
					break;
				}
			}
            mData[i].SetParam2(dirNewFormat);
		}
	}
}

/*
	Get a quick string to describe what a block actually contains
*/
std::string AnalyzeBlock(MapBlock *block)
{
	if(block == NULL)
		return "NULL";

	std::ostringstream desc;

	Vector3<short> p = block->GetPosition();
	char spos[25];
	std::snprintf(spos, sizeof(spos), "(%2d,%2d,%2d), ", p[0], p[1], p[2]);
	desc<<spos;

	switch(block->GetModified())
	{
	    case MOD_STATE_CLEAN:
		    desc<<"CLEAN,           ";
		    break;
	    case MOD_STATE_WRITE_AT_UNLOAD:
		    desc<<"WRITE_AT_UNLOAD, ";
		    break;
	    case MOD_STATE_WRITE_NEEDED:
		    desc<<"WRITE_NEEDED,    ";
		    break;
	    default:
		    desc<<"unknown GetModified()=" << block->GetModified() << ", ";
	}

	if(block->IsGenerated())
		desc<<"is_gen [X], ";
	else
		desc<<"is_gen [ ], ";

	if(block->IsUnderground())
		desc<<"is_ug [X], ";
	else
		desc<<"is_ug [ ], ";

	desc<<"lighting_complete: "<<block->GetLightingComplete()<<", ";

	if(block->IsDummy())
	{
		desc<<"Dummy, ";
	}
	else
	{
		bool fullIgnore = true;
		bool someIgnore = false;
		bool fullAir = true;
		bool someAir = false;
		for(short z0=0; z0<MAP_BLOCKSIZE; z0++)
        {
            for (short y0 = 0; y0 < MAP_BLOCKSIZE; y0++)
            {
                for (short x0 = 0; x0 < MAP_BLOCKSIZE; x0++)
                {
                    Vector3<short> p{ x0, y0, z0 };
                    MapNode n = block->GetNodeNoEx(p);
                    unsigned short c = n.GetContent();
                    if (c == CONTENT_IGNORE)
                        someIgnore = true;
                    else
                        fullIgnore = false;
                    if (c == CONTENT_AIR)
                        someAir = true;
                    else
                        fullAir = false;
                }
            }
		}

		desc<<"content {";

		std::ostringstream ss;

		if(fullIgnore)
			ss<<"IGNORE (full), ";
		else if(someIgnore)
			ss<<"IGNORE, ";

		if(fullAir)
			ss<<"AIR (full), ";
		else if(someAir)
			ss<<"AIR, ";

		if(ss.str().size()>=2)
			desc<<ss.str().substr(0, ss.str().size()-2);

		desc<<"}, ";
	}

	return desc.str().substr(0, desc.str().size()-2);
}