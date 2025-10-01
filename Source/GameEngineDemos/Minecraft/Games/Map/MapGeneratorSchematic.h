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

#ifndef MAPGENERATORSCHEMATIC_H
#define MAPGENERATORSCHEMATIC_H

#include "MapGeneratorDecoration.h"

#include "Core/Utility/StringUtil.h"

class Map;
class LogicMap;
class MapGenerator;
class MMVManip;
class PseudoRandom;
class NodeResolver;

/*
	Minetest Schematic File Format

	All values are stored in big-endian byte order.
	[unsigned int] signature: 'MTSM'
	[uint16_t] version: 4
	[uint16_t] size X
	[uint16_t] size Y
	[uint16_t] size Z
	For each Y:
		[unsigned char] slice probability value
	[Name-ID table] Name ID Mapping Table
		[uint16_t] name-id count
		For each name-id mapping:
			[uint16_t] name length
			[unsigned char[]] name
	ZLib deflated {
	For each node in schematic:  (for z, y, x)
		[uint16_t] content
	For each node in schematic:
		[unsigned char] param1
		  bit 0-6: probability
		  bit 7:   specific node force placement
	For each node in schematic:
		[unsigned char] param2
	}

	Version changes:
	1 - Initial version
	2 - Fixed messy never/always place; 0 probability is now never, 0xFF is always
	3 - Added y-slice probabilities; this allows for variable height structures
	4 - Compressed range of node occurence prob., added per-node force placement bit
*/

//// Schematic constants
#define MTSCHEM_FILE_SIGNATURE 0x4d54534d // 'MTSM'
#define MTSCHEM_FILE_VER_HIGHEST_READ  4
#define MTSCHEM_FILE_VER_HIGHEST_WRITE 4

#define MTSCHEM_PROB_MASK       0x7F

#define MTSCHEM_PROB_NEVER      0x00
#define MTSCHEM_PROB_ALWAYS     0x7F
#define MTSCHEM_PROB_ALWAYS_OLD 0xFF

#define MTSCHEM_FORCE_PLACE     0x80

enum SchematicType
{
	SCHEMATIC_NORMAL,
};

enum SchematicFormatType 
{
	SCHEM_FMT_HANDLE,
	SCHEM_FMT_MTS,
	SCHEM_FMT_ANY,
};

class Schematic : public Object, public NodeResolver 
{
public:
	Schematic() = default;
	virtual ~Schematic();

	Object* Clone() const;

	virtual void ResolveNodeNames();

	bool LoadSchematicFromFile(const std::string& filename,
		const NodeManager* nodeMgr, StringMap* replaceNames = NULL);
	bool SaveSchematicToFile(const std::string& filename, const NodeManager* nodeMgr);
	bool GetSchematicFromMap(Map* map, Vector3<short> p1, Vector3<short> p2);

	bool DeserializeFromMts(std::istream* is);
	bool SerializeToMts(std::ostream* os) const;
	bool SerializeToAny(std::ostream* os, bool useComments, unsigned int indentSpaces) const;

	void BlitToVManip(MMVManip *vm, Vector3<short> pos, RotationDegrees rot, bool forcePlace);
	bool PlaceOnVManip(MMVManip *vm, Vector3<short> pos, unsigned int flags, RotationDegrees rot, bool forcePlace);
	void PlaceOnMap(LogicMap *map, Vector3<short> pos, unsigned int flags, RotationDegrees rot, bool forcePlace);

	void ApplyProbabilities(Vector3<short> p0,
		std::vector<std::pair<Vector3<short>, unsigned char>>* plist,
        std::vector<std::pair<int16_t, unsigned char>>* splist);

	std::vector<uint16_t> mContentNodes;
	unsigned int mFlags = 0;
	Vector3<short> mSize;
	MapNode* mSchemData = nullptr;
	unsigned char* mSliceProbs = nullptr;

private:
	// Counterpart to the node resolver: Condense uint16_t to a sequential "m_nodenames" list
	void CondenseContentIds();

    PcgRandom mPcgRandom;
};

class SchematicManager : public ObjectManager 
{
public:
	SchematicManager(Environment* env);
	virtual ~SchematicManager() = default;

	SchematicManager* Clone() const;

	virtual void Clear();

	const char* GetObjectTitle() const
	{
		return "schematic";
	}

	static Schematic* Create(SchematicType type)
	{
		return new Schematic;
	}
};


#endif