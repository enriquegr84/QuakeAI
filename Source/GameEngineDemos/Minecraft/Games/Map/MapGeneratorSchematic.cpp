/*
Minetest
Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#include "MapGeneratorSchematic.h"
#include "MapGenerator.h"
#include "MapBlock.h"
#include "Map.h"

#include "VoxelAlgorithms.h"
#include "Emerge.h"

#include "Core/Utility/Serialize.h"
#include "Core/IO/FileSystem.h"
#include "Core/Logger/Logger.h"


///////////////////////////////////////////////////////////////////////////////


SchematicManager::SchematicManager(Environment* env) : ObjectManager(env, OBJ_SCHEMATIC)
{
}


SchematicManager* SchematicManager::Clone() const
{
	auto mgr = new SchematicManager(mEnvironment);
	LogAssert(mgr, "invalid schematic manager");
	ObjectManager::CloneTo(mgr);
	return mgr;
}


void SchematicManager::Clear()
{
	// Remove all dangling references in Decorations
	DecorationManager* decomgr = EmergeManager::Get()->GetWritableDecorationManager();
	for (unsigned int i = 0; i != (unsigned int)decomgr->GetNumObjects(); i++)
    {
		Decoration *deco = (Decoration *)decomgr->GetRaw(i);

		try 
        {
			DecoSchematic* dschem = dynamic_cast<DecoSchematic *>(deco);
			if (dschem)
				dschem->mSchematic = NULL;
		} 
        catch (const std::bad_cast &) 
        {

		}
	}

	ObjectManager::Clear();
}


///////////////////////////////////////////////////////////////////////////////


Schematic::~Schematic()
{
	delete []mSchemData;
	delete []mSliceProbs;
}

Object* Schematic::Clone() const
{
	auto schematic = new Schematic();
	Object::CloneTo(schematic);
	NodeResolver::CloneTo(schematic);

    schematic->mContentNodes = mContentNodes;
    schematic->mFlags = mFlags;
    schematic->mSize = mSize;
	
    LogAssert(!mSchemData, "Schematic can only be cloned after loading");
	unsigned int nodecount = mSize[0] * mSize[1] * mSize[2];
    schematic->mSchemData = new MapNode[nodecount];
	memcpy(schematic->mSchemData, mSchemData, sizeof(MapNode) * nodecount);
    schematic->mSliceProbs = new unsigned char[mSize[1]];
	memcpy(schematic->mSliceProbs, mSliceProbs, sizeof(unsigned char) * mSize[1]);

	return schematic;
}


void Schematic::ResolveNodeNames()
{
	mContentNodes.clear();
	GetIdsFromNrBacklog(&mContentNodes, true, CONTENT_AIR);

	size_t bufSize = mSize[0] * mSize[1] * mSize[2];
	for (size_t i = 0; i != bufSize; i++) 
    {
		uint16_t contentOriginal = mSchemData[i].GetContent();
		if (contentOriginal >= mContentNodes.size())
        {
			LogWarning("Corrupt schematic. name=\"" + mName + 
                "\" at index " + std::to_string(i));
            contentOriginal = 0;
		}
		// Unfold condensed ID layout to uint16_t
		mSchemData[i].SetContent(mContentNodes[contentOriginal]);
	}
}


void Schematic::BlitToVManip(MMVManip* vm, Vector3<short> p, RotationDegrees rot, bool forcePlace)
{
	LogAssert(mSchemData && mSliceProbs, "invalid data");
	LogAssert(mNodeManager, "invalid node manager");

	int xstride = 1;
	int ystride = mSize[0];
	int zstride = mSize[0] * mSize[1];

	int16_t sx = mSize[0];
	int16_t sy = mSize[1];
	int16_t sz = mSize[2];

	int iStart, iStepX, iStepZ;
	switch (rot) 
    {
		case ROTATE_90:
			iStart  = sx - 1;
			iStepX = zstride;
			iStepZ = -xstride;
			std::swap(sx, sz);
			break;
		case ROTATE_180:
			iStart  = zstride * (sz - 1) + sx - 1;
			iStepX = -xstride;
			iStepZ = -zstride;
			break;
		case ROTATE_270:
			iStart  = zstride * (sz - 1);
			iStepX = -zstride;
			iStepZ = xstride;
			std::swap(sx, sz);
			break;
		default:
			iStart  = 0;
			iStepX = xstride;
			iStepZ = zstride;
	}

	int16_t yMap = p[1];
	for (int16_t y = 0; y != sy; y++) 
    {
		if ((mSliceProbs[y] != MTSCHEM_PROB_ALWAYS) &&
			(mSliceProbs[y] <= mPcgRandom.Range(1, MTSCHEM_PROB_ALWAYS)))
			continue;

		for (int16_t z = 0; z != sz; z++) 
        {
			unsigned int i = z * iStepZ + y * ystride + iStart;
			for (int16_t x = 0; x != sx; x++, i += iStepX) 
            {
                Vector3<short> pos{ (short)(p[0] + x), yMap, (short)(p[2] + z) };
				if (!vm->mArea.Contains(pos))
					continue;

				if (mSchemData[i].GetContent() == CONTENT_IGNORE)
					continue;

				unsigned char placement_prob = mSchemData[i].param1 & MTSCHEM_PROB_MASK;
				bool forcePlace_node = mSchemData[i].param1 & MTSCHEM_FORCE_PLACE;

				if (placement_prob == MTSCHEM_PROB_NEVER)
					continue;

				unsigned int vi = vm->mArea.Index(pos);
				if (!forcePlace && !forcePlace_node) 
                {
					uint16_t c = vm->mData[vi].GetContent();
					if (c != CONTENT_AIR && c != CONTENT_IGNORE)
						continue;
				}

				if ((placement_prob != MTSCHEM_PROB_ALWAYS) &&
					(placement_prob <= mPcgRandom.Range(1, MTSCHEM_PROB_ALWAYS)))
					continue;

				vm->mData[vi] = mSchemData[i];
				vm->mData[vi].param1 = 0;

				if (rot)
					vm->mData[vi].RotateAlongYAxis(mNodeManager, rot);
			}
		}
		yMap++;
	}
}


bool Schematic::PlaceOnVManip(MMVManip* vm, Vector3<short> pos, unsigned int flags, RotationDegrees rot, bool forcePlace)
{
	LogAssert(vm != NULL, "invalid vmanip");
	LogAssert(mSchemData && mSliceProbs, "invalid data");
	LogAssert(mNodeManager != NULL, "invalid node manager");

	//// Determine effective rotation and effective schematic dimensions
	if (rot == ROTATE_RAND)
		rot = (RotationDegrees)mPcgRandom.Range(ROTATE_0, ROTATE_270);

	Vector3<short> s = (rot == ROTATE_90 || rot == ROTATE_270) ?
        Vector3<short>{mSize[2], mSize[1], mSize[0]} : mSize;

	//// Adjust placement position if necessary
	if (flags & DECO_PLACE_CENTER_X)
        pos[0] -= (s[0] - 1) / 2;
	if (flags & DECO_PLACE_CENTER_Y)
        pos[1] -= (s[1] - 1) / 2;
	if (flags & DECO_PLACE_CENTER_Z)
        pos[2] -= (s[2] - 1) / 2;

	BlitToVManip(vm, pos, rot, forcePlace);

    return vm->mArea.Contains(VoxelArea(pos, pos + s - Vector3<short>{1, 1, 1}));
}

void Schematic::PlaceOnMap(LogicMap* map, Vector3<short> pos, unsigned int flags, RotationDegrees rot, bool forcePlace)
{
	std::map<Vector3<short>, MapBlock*> lightingModifiedBlocks;
    std::map<Vector3<short>, MapBlock*> modifiedBlocks;
    std::map<Vector3<short>, MapBlock*>::iterator it;

	LogAssert(map != NULL, "invalid map");
	LogAssert(mSchemData != NULL, "invalid schematic data");
	LogAssert(mNodeManager != NULL, "invalid node manager");

	//// Determine effective rotation and effective schematic dimensions
	if (rot == ROTATE_RAND)
		rot = (RotationDegrees)mPcgRandom.Range(ROTATE_0, ROTATE_270);

	Vector3<short> size = (rot == ROTATE_90 || rot == ROTATE_270) ?
        Vector3<short>{mSize[2], mSize[1], mSize[0]} : mSize;

	//// Adjust placement position if necessary
	if (flags & DECO_PLACE_CENTER_X)
        pos[0] -= (size[0] - 1) / 2;
	if (flags & DECO_PLACE_CENTER_Y)
        pos[1] -= (size[1] - 1) / 2;
	if (flags & DECO_PLACE_CENTER_Z)
        pos[2] -= (size[2] - 1) / 2;

	//// Create VManip for effected area, emerge our area, modify area
	//// inside VManip, then blit back.
	Vector3<short> bp1 = GetNodeBlockPosition(pos);
    Vector3<short> bp2 = GetNodeBlockPosition(pos + size - Vector3<short>{1, 1, 1});

	MMVManip vm(map);
	vm.InitialEmerge(bp1, bp2);

	BlitToVManip(&vm, pos, rot, forcePlace);
	BlitBackWithLight(map, &vm, &modifiedBlocks);

	//// Carry out post-map-modification actions

	//// Create & dispatch map modification events to observers
	MapEditEvent evt;
	evt.type = MEET_OTHER;
	for (it = modifiedBlocks.begin(); it != modifiedBlocks.end(); ++it)
		evt.modifiedBlocks.insert(it->first);

	map->DispatchEvent(evt);
}


bool Schematic::DeserializeFromMts(std::istream* is)
{
	std::istream &ss = *is;
	uint16_t contentIgnore = CONTENT_IGNORE;
	bool haveContentIgnore = false;

	//// Read signature
	unsigned int signature = ReadUInt32(ss);
	if (signature != MTSCHEM_FILE_SIGNATURE) 
    {
		LogWarning("Invalid schematic file");
		return false;
	}

	//// Read version
	uint16_t version = ReadUInt16(ss);
	if (version > MTSCHEM_FILE_VER_HIGHEST_READ) 
    {
		LogWarning(": unsupported schematic file version");
		return false;
	}

	//// Read size
	mSize = ReadV3Short(ss);

	//// Read Y-slice probability values
	delete []mSliceProbs;
	mSliceProbs = new unsigned char[mSize[1]];
	for (int y = 0; y != mSize[1]; y++)
		mSliceProbs[y] = (version >= 3) ? ReadUInt8(ss) : MTSCHEM_PROB_ALWAYS_OLD;

	//// Read node names
	NodeResolver::Reset();

	uint16_t nidmapcount = ReadUInt16(ss);
	for (int i = 0; i != nidmapcount; i++) 
    {
		std::string name = DeserializeString16(ss);

		// Instances of "ignore" from v1 are converted to air (and instances
		// are fixed to have MTSCHEM_PROB_NEVER later on).
		if (name == "ignore") 
        {
			name = "air";
			contentIgnore = i;
			haveContentIgnore = true;
		}
        mNodeNames.push_back(name);
	}

	// Prepare for node resolver
    mNodeListSizes.push_back(mNodeNames.size());

	//// Read node data
	unsigned int nodecount = mSize[0] * mSize[1] * mSize[2];

	delete []mSchemData;
	mSchemData = new MapNode[nodecount];

	MapNode::DeserializeBulk(ss, SER_FMT_VER_HIGHEST_READ, mSchemData, nodecount, 2, 2);

	// Fix probability values for nodes that were ignore; removed in v2
	if (version < 2) 
    {
		for (size_t i = 0; i != nodecount; i++) 
        {
			if (mSchemData[i].param1 == 0)
				mSchemData[i].param1 = MTSCHEM_PROB_ALWAYS_OLD;
			if (haveContentIgnore && mSchemData[i].GetContent() == contentIgnore)
				mSchemData[i].param1 = MTSCHEM_PROB_NEVER;
		}
	}

	// Fix probability values for probability range truncation introduced in v4
	if (version < 4) 
    {
		for (int16_t y = 0; y != mSize[1]; y++)
			mSliceProbs[y] >>= 1;
		for (size_t i = 0; i != nodecount; i++)
			mSchemData[i].param1 >>= 1;
	}

	return true;
}


bool Schematic::SerializeToMts(std::ostream* os) const
{
	// Nodes must not be resolved (-> condensed)
	// checking here is not possible because "mSchemData" might be temporary.
	std::ostream& ss = *os;
	WriteUInt32(ss, MTSCHEM_FILE_SIGNATURE);         // signature
	WriteUInt16(ss, MTSCHEM_FILE_VER_HIGHEST_WRITE); // version
	WriteV3Short(ss, mSize);                         // schematic size

	for (int y = 0; y != mSize[1]; y++)             // Y slice probabilities
		WriteUInt8(ss, mSliceProbs[y]);

	WriteUInt16(ss, (uint16_t)mNodeNames.size()); // name count
	for (size_t i = 0; i != mNodeNames.size(); i++)
		ss << SerializeString16(mNodeNames[i]); // node names

	// compressed bulk node data
	MapNode::SerializeBulk(ss, SER_FMT_VER_HIGHEST_WRITE,
		mSchemData, mSize[0] * mSize[1] * mSize[2], 2, 2, -1);

	return true;
}


bool Schematic::SerializeToAny(std::ostream* os, bool useComments, unsigned int indentSpaces) const
{
	std::ostream &ss = *os;

	std::string indent("\t");
	if (indentSpaces > 0)
		indent.assign(indentSpaces, ' ');

	bool resolveDone = IsResolveDone();
	LogAssert(resolveDone && !mNodeManager, "SerializeToAny: NodeManager is required");

	//// Write header
	{
		ss << "schematic = {" << std::endl;
		ss << indent << "size = " << "{x=" << mSize[0] << ", y=" 
            << mSize[1] << ", z=" << mSize[2] << "}," << std::endl;
	}

	//// Write y-slice probabilities
	{
		ss << indent << "yslice_prob = {" << std::endl;
		for (uint16_t y = 0; y != mSize[1]; y++) 
        {
			unsigned char probability = mSliceProbs[y] & MTSCHEM_PROB_MASK;

			ss << indent << indent << "{" << "ypos=" << y
				<< ", prob=" << (uint16_t)probability * 2 << "}," << std::endl;
		}

		ss << indent << "}," << std::endl;
	}

	//// Write node data
	{
		ss << indent << "data = {" << std::endl;

		unsigned int i = 0;
        for (uint16_t z = 0; z != mSize[2]; z++)
        {
            for (uint16_t y = 0; y != mSize[1]; y++) 
            {
                if (useComments) 
                {
                    ss << std::endl
                        << indent << indent
                        << "-- z=" << z
                        << ", y=" << y << std::endl;
                }

                for (uint16_t x = 0; x != mSize[0]; x++, i++) 
                {
                    unsigned char probability = mSchemData[i].param1 & MTSCHEM_PROB_MASK;
                    bool forcePlace = mSchemData[i].param1 & MTSCHEM_FORCE_PLACE;

                    // After node resolving: real uint16_t, lookup using NodeManager
                    // Prior node resolving: condensed ID, lookup using m_nodenames
                    uint16_t c = mSchemData[i].GetContent();

                    ss << indent << indent << "{" << "name=\"";

                    if (!resolveDone) 
                    {
                        // Prior node resolving (eg. direct schematic load)
                        LogAssert(c >= mNodeNames.size(), "Invalid node list");
                        ss << mNodeNames[c];
                    }
                    else 
                    {
                        // After node resolving (eg. biome decoration)
                        ss << mNodeManager->Get(c).name;
                    }

                    ss << "\", prob=" << (uint16_t)probability * 2
                        << ", param2=" << (uint16_t)mSchemData[i].param2;

                    if (forcePlace)
                        ss << ", forcePlace=true";

                    ss << "}," << std::endl;
                }
            }
        }

		ss << indent << "}," << std::endl;
	}

	ss << "}" << std::endl;

	return true;
}


bool Schematic::LoadSchematicFromFile(const std::string& filename,
	const NodeManager* nodeMgr, StringMap* replaceNames)
{
	std::ifstream is(filename, std::ios_base::binary);
	if (!is.good()) 
    {
		LogWarning(": unable to open file '" + filename + "'");
		return false;
	}

	if (!mNodeManager)
        mNodeManager = nodeMgr;

	if (!DeserializeFromMts(&is))
		return false;

	mName = filename;

	if (replaceNames) 
    {
		for (std::string& nodeName : mNodeNames) 
        {
			StringMap::iterator it = replaceNames->find(nodeName);
			if (it != replaceNames->end())
                nodeName = it->second;
		}
	}

	if (mNodeManager)
        mNodeManager->PendNodeResolve(this);

	return true;
}


bool Schematic::SaveSchematicToFile(const std::string& filename, const NodeManager* nodeMgr)
{
	Schematic *schem = this;

	bool needsCondense = IsResolveDone();

	if (!mNodeManager)
        mNodeManager = nodeMgr;

	if (needsCondense) 
    {
		if (!mNodeManager)
			return false;

		schem = (Schematic*)this->Clone();
		schem->CondenseContentIds();
	}

	std::ostringstream os(std::ios_base::binary);
	bool status = schem->SerializeToMts(&os);

	if (needsCondense)
		delete schem;

	if (!status)
		return false;

	return FileSystem::Get()->SafeWriteToFile(filename, os.str());
}


bool Schematic::GetSchematicFromMap(Map* map, Vector3<short> p1, Vector3<short> p2)
{
	MMVManip* vm = new MMVManip(map);

	Vector3<short> bp1 = GetNodeBlockPosition(p1);
	Vector3<short> bp2 = GetNodeBlockPosition(p2);
	vm->InitialEmerge(bp1, bp2);

    mSize = p2 - p1 + Vector3<short>{1, 1, 1};

	mSliceProbs = new unsigned char[mSize[1]];
	for (int16_t y = 0; y != mSize[1]; y++)
		mSliceProbs[y] = MTSCHEM_PROB_ALWAYS;

	mSchemData = new MapNode[mSize[0] * mSize[1] * mSize[2]];

	unsigned int i = 0;
    for (int16_t z = p1[2]; z <= p2[2]; z++)
    {
        for (int16_t y = p1[1]; y <= p2[1]; y++) 
        {
            unsigned int vi = vm->mArea.Index(p1[0], y, z);
            for (int16_t x = p1[0]; x <= p2[0]; x++, i++, vi++) 
            {
                mSchemData[i] = vm->mData[vi];
                mSchemData[i].param1 = MTSCHEM_PROB_ALWAYS;
            }
        }
    }

	delete vm;

	// Reset and mark as complete
	NodeResolver::Reset(true);

	return true;
}


void Schematic::ApplyProbabilities(Vector3<short> p0,
	std::vector<std::pair<Vector3<short>, unsigned char> > *plist,
    std::vector<std::pair<int16_t, unsigned char> > *splist)
{
	for (size_t i = 0; i != plist->size(); i++) 
    {
		Vector3<short> p = (*plist)[i].first - p0;
		int index = p[2] * (mSize[1] * mSize[0]) + p[1] * mSize[0] + p[0];
		if (index < mSize[2] * mSize[1] * mSize[0]) 
        {
			unsigned char prob = (*plist)[i].second;
			mSchemData[index].param1 = prob;

			// trim unnecessary node names from schematic
			if (prob == MTSCHEM_PROB_NEVER)
				mSchemData[index].SetContent(CONTENT_AIR);
		}
	}

	for (size_t i = 0; i != splist->size(); i++) 
    {
		int16_t y = (*splist)[i].first - p0[1];
		mSliceProbs[y] = (*splist)[i].second;
	}
}


void Schematic::CondenseContentIds()
{
	std::unordered_map<uint16_t, uint16_t> nodeIdMap;
	uint16_t numids = 0;

	// Reset node resolve fields
	NodeResolver::Reset();

	size_t nodecount = mSize[0] * mSize[1] * mSize[2];
	for (size_t i = 0; i != nodecount; i++) 
    {
		uint16_t id;
		uint16_t c = mSchemData[i].GetContent();

		auto it = nodeIdMap.find(c);
		if (it == nodeIdMap.end()) 
        {
			id = numids;
			numids++;

			mNodeNames.push_back(mNodeManager->Get(c).name);
			nodeIdMap.emplace(std::make_pair(c, id));
		} 
        else 
        {
			id = it->second;
		}
		mSchemData[i].SetContent(id);
	}
}
