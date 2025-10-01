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

#ifndef MAPNODE_H
#define MAPNODE_H

#include "../../MinecraftStd.h"

#include "../../Utils/Util.h"
#include "../../Utils/NameIdMapping.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Effect/Lighting.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"

#include "Mathematic/Algebra/Vector3.h"

class NodeManager;
class Map;

/*
    The maximum node ID that can be registered by mods. This must
    be significantly lower than the maximum uint16_t value, so that
    there is enough room for dummy node IDs, which are created when
    a MapBlock containing unknown node names is loaded from disk.
*/
#define MAX_REGISTERED_CONTENT 0x7fffU

/*
    A solid walkable node with the texture unknown_node.png.

    For example, used on the visual to display unregistered node IDs
    (instead of expanding the vector of node definitions each time
    such a node is received).
*/
#define CONTENT_UNKNOWN 125

/*
    The common material through which the player can walk and which
    is transparent to light
*/
#define CONTENT_AIR 126

/*
    Ignored node.

    Unloaded chunks are considered to consist of this. Several other
    methods return this when an error occurs. Also, during
    map generation this means the node has not been set yet.

    Doesn't create faces with anything and is considered being
    out-of-map in the game map.
*/
#define CONTENT_IGNORE 127

enum LightBank
{
    LIGHTBANK_DAY,
    LIGHTBANK_NIGHT
};

/*
    Simple rotation enum.
*/
enum RotationDegrees 
{
    ROTATE_0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
    ROTATE_RAND,
};

/*
    Masks for MapNode.param2 of flowing liquids
 */
#define LIQUID_LEVEL_MASK 0x07
#define LIQUID_FLOW_DOWN_MASK 0x08

 //#define LIQUID_LEVEL_MASK 0x3f // better finite water
 //#define LIQUID_FLOW_DOWN_MASK 0x40 // not used when finite water

 /* maximum amount of liquid in a block */
#define LIQUID_LEVEL_MAX LIQUID_LEVEL_MASK
#define LIQUID_LEVEL_SOURCE (LIQUID_LEVEL_MAX+1)

#define LIQUID_INFINITY_MASK 0x80 //0b10000000

// mask for leveled nodebox param2
#define LEVELED_MASK 0x7F
#define LEVELED_MAX LEVELED_MASK


struct ContentFeatures;

/*
	This is the stuff what the whole world consists of.
*/


struct MapNode
{
	/*
		Main content
	*/
	uint16_t param0;

	/*
		Misc parameter. Initialized to 0.
		- For lightPropagates() blocks, this is light intensity,
		  stored logarithmically from 0 to LIGHT_MAX.
		  Sunlight is LIGHT_SUN, which is LIGHT_MAX+1.
		  - Contains 2 values, day- and night lighting. Each takes 4 bits.
		- Uhh... well, most blocks have light or nothing in here.
	*/
	uint8_t param1;

	/*
		The second parameter. Initialized to 0.
		E.g. direction for torches and flowing water.
	*/
	uint8_t param2;

	MapNode() = default;

	MapNode(unsigned short content, uint8_t aParam1=0, uint8_t aParam2=0) noexcept
		: param0(content),
		  param1(aParam1),
		  param2(aParam2)
	{ }

	bool operator==(const MapNode &other) const noexcept
	{
		return (param0 == other.param0
				&& param1 == other.param1
				&& param2 == other.param2);
	}

	// To be used everywhere
    uint16_t GetContent() const noexcept
	{
		return param0;
	}
	void SetContent(uint16_t c) noexcept
	{
		param0 = c;
	}
	uint8_t GetParam1() const noexcept
	{
		return param1;
	}
	void SetParam1(uint8_t p) noexcept
	{
		param1 = p;
	}
	uint8_t GetParam2() const noexcept
	{
		return param2;
	}
	void SetParam2(uint8_t p) noexcept
	{
		param2 = p;
	}

	/*!
	 * Returns the color of the node.
	 *
	 * \param f content features of this node
	 * \param color output, contains the node's color.
	 */
	void GetColor(const ContentFeatures& f, SColor* color) const;

	void SetLight(LightBank bank, uint8_t aLight, const ContentFeatures& f) noexcept;

	void SetLight(LightBank bank, uint8_t aLight, const NodeManager* nodeMgr);

	/**
	 * Check if the light value for night differs from the light value for day.
	 *
	 * @return If the light values are equal, returns true; otherwise false
	 */
	bool IsLightDayNightEq(const NodeManager* nodeMgr) const;

	uint8_t GetLight(LightBank bank, const NodeManager* nodeMgr) const;

	/*!
	 * Returns the node's light level from param1.
	 * If the node emits light, it is ignored.
	 * \param f the ContentFeatures of this node.
	 */
	uint8_t GetLightRaw(LightBank bank, const ContentFeatures& f) const noexcept;

	/**
	 * This function differs from GetLight(LightBank bank, NodeManager *nodeMgr)
	 * in that the ContentFeatures of the node in question are not retrieved by
	 * the function itself.  Thus, if you have already called nodeMgr->get() to
	 * get the ContentFeatures you pass it to this function instead of the
	 * function getting ContentFeatures itself.  Since NodeManager::get()
	 * is relatively expensive this can lead to significant performance
	 * improvements in some situations.  Call this function if (and only if)
	 * you have already retrieved the ContentFeatures by calling
	 * NodeManager::get() for the node you're working with and the
	 * pre-conditions listed are true.
	 *
	 * @pre f != NULL
	 * @pre f->paramType == CPT_LIGHT
	 */
	uint8_t GetLightNoChecks(LightBank bank, const ContentFeatures* f) const noexcept;

	bool GetLightBanks(uint8_t& lightDay, uint8_t& lightNight, const NodeManager* nodeMgr) const;

	// 0 <= dayLightFactor <= 1000
	// 0 <= return value <= LIGHT_SUN
	uint8_t GetLightBlend(unsigned int dayLightFactor, const NodeManager* nodeMgr) const
	{
		uint8_t lightDay = 0;
		uint8_t lightNight = 0;
		GetLightBanks(lightDay, lightNight, nodeMgr);
		return BlendLight(dayLightFactor, lightDay, lightNight);
	}

	uint8_t GetFaceDir(const NodeManager* nodeMgr, bool allowWallmounted = false) const;
	uint8_t GetWallMounted(const NodeManager* nodeMgr) const;
	Vector3<short> GetWallMountedDir(const NodeManager* nodeMgr) const;

	/// @returns Rotation in range 0–239 (in 1.5° steps)
	uint8_t GetDegRotate(const NodeManager* nodeMgr) const;

	void RotateAlongYAxis(const NodeManager* nodeMgr, RotationDegrees rot);

	/*!
	 * Checks which neighbors does this node connect to.
	 *
	 * \param p coordinates of the node
	 */
	uint8_t GetNeighbors(Vector3<short> pos, std::shared_ptr<Map> map) const;

	/*
		Gets list of node boxes (used for rendering (NDT_NODEBOX))
	*/
	void GetNodeBoxes(const NodeManager* nodeMgr, 
        std::vector<BoundingBox<float>>* boxes, uint8_t neighbors = 0) const;

	/*
		Gets list of selection boxes
	*/
	void GetSelectionBoxes(const NodeManager* nodemg,
		std::vector<BoundingBox<float>>* boxes, uint8_t neighbors = 0) const;

	/*
		Gets list of collision boxes
	*/
	void GetCollisionBoxes(const NodeManager* nodeMgr,
		std::vector<BoundingBox<float>>* boxes, uint8_t neighbors = 0) const;

	/*
		Liquid/leveled helpers
	*/
	uint8_t GetMaxLevel(const NodeManager* nodeMgr) const;
	uint8_t GetLevel(const NodeManager* nodeMgr) const;
	int8_t SetLevel(const NodeManager* nodeMgr, short level = 1);
	int8_t AddLevel(const NodeManager* nodeMgr, short add = 1);

	/*
		Serialization functions
	*/

	static unsigned int SerializedLength(uint8_t version);
	void Serialize(uint8_t* dest, uint8_t version) const;
	void Deserialize(uint8_t* source, uint8_t version);

	// Serializes or Deserializes a list of nodes in bulk format (first the
	// content of all nodes, then the param1 of all nodes, then the param2
	// of all nodes).
	//   version = serialization version. Must be >= 22
	//   contentWidth = the number of bytes of content per node
	//   paramsWidth = the number of bytes of params per node
	//   compressed = true to zlib-compress output
	static void SerializeBulk(std::ostream& os, int version,
			const MapNode* nodes, unsigned int mNodeCount,
			uint8_t contentWidth, uint8_t paramsWidth, int compressionLevel);
	static void DeserializeBulk(std::istream& is, int version,
			MapNode* nodes, unsigned int mNodeCount,
			uint8_t contentWidth, uint8_t paramsWidth);

private:
	// Deprecated serialization methods
	void DeserializePre22(const uint8_t *source, uint8_t version);
};

#endif