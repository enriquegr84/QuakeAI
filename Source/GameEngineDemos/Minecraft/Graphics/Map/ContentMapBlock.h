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

#ifndef CONTENTMAPBLOCK_H
#define CONTENTMAPBLOCK_H

#include "../../Utils/Util.h"

#include "../Tile.h"
#include "../Node.h"
#include "../MeshUtil.h"

struct MeshMakeData;
struct MeshCollector;

struct LightPair 
{
	uint8_t lightDay;
	uint8_t lightNight;

	LightPair() = default;
	explicit LightPair(uint16_t value) : lightDay(value & 0xff), lightNight(value >> 8) {}
	LightPair(uint8_t valueA, uint8_t valueB) : lightDay(valueA), lightNight(valueB) {}
	LightPair(float valueA, float valueB) :
		lightDay(std::clamp((int)round(valueA), 0, 255)),
		lightNight(std::clamp((int)round(valueB), 0, 255)) {}
	operator uint16_t() const { return lightDay | lightNight << 8; }
};

struct LightInfo 
{
	float lightDay;
	float lightNight;
	float lightBoosted;

	LightPair GetPair(float sunlightBoost = 0.0) const
	{
		return LightPair((1 - sunlightBoost) * lightDay + sunlightBoost * lightBoosted, lightNight);
	}
};

struct LightFrame 
{
	float lightsDay[8];
	float lightsNight[8];
	bool sunlight[8];
};

class MapblockMeshGenerator
{
public:
	MeshMakeData* mData;
	MeshCollector* mCollector;

	const NodeManager* mNodeMgr;

// options
	bool mEnableMeshCache;

// current node
	Vector3<short> mBlockPosNodes;
	Vector3<short> mPosition;
	Vector3<float> mOrigin;
	MapNode mNode;
	const ContentFeatures* mFeatures;
	LightPair mLight;
	LightFrame mFrame;
	SColor mColor;
	TileSpec mTile;
	float mScale;

// lighting
	void GetSmoothLightFrame();
	LightInfo BlendLight(const Vector3<float>& vertexPos);
    SColor BlendLightColor(const Vector3<float>& vertexPos);
    SColor BlendLightColor(const Vector3<float>& vertexPos, const Vector3<float>& vertexNormal);

	void UseTile(int index = 0, uint8_t setFlags = MATERIAL_FLAG_CRACK_OVERLAY,
		uint8_t resetFlags = 0, bool special = false);
	void GetTile(int index, TileSpec *tile);
	void GetTile(Vector3<short> direction, TileSpec* tile);
	void GetSpecialTile(int index, TileSpec* tile, bool applyCrack = false);

// face drawing
	void DrawQuad(Vector3<float>* vertices, 
        const Vector3<short>& normal = Vector3<short>::Zero(), float verticalTiling = 1.0);

// cuboid drawing!
	void DrawCuboid(const BoundingBox<float>& box, TileSpec* tiles, int tilecount,
		const LightInfo* lights , const float* txc);
	void GenerateCuboidTextureCoords(BoundingBox<float> const& box, float* coords);
	void DrawAutoLightedCuboid(BoundingBox<float> box, const float* txc = NULL,
		TileSpec* tiles = NULL, int tileCount = 0);

// liquid-specific
	bool mTopIsSameLiquid;
	bool mDrawLiquidBottom;
	TileSpec mTileLiquid;
	TileSpec mTileLiquidTop;
	uint16_t mContentFlowing;
	uint16_t mContentSource;
	SColor mColorLiquidTop;
	struct NeighborData 
    {
		float level;
		uint16_t content;
		bool isSameLiquid;
		bool topIsSameLiquid;
	};
	NeighborData mLiquidNeighbors[3][3];
	float mCornerLevels[2][2];

	void PrepareLiquidNodeDrawing();
	void GetLiquidNeighborhood();
	void CalculateCornerLevels();
	float GetCornerLevel(int i, int k);
	void DrawLiquidSides();
	void DrawLiquidTop();
	void DrawLiquidBottom();

    // raillike-specific
	// name of the group that enables connecting to raillike nodes of different kind
	static const std::string mRaillikeGroupName;
	int mRaillikeGroup;
	bool IsSameRail(Vector3<short> dir);

// plantlike-specific
	PlantlikeStyle mDrawStyle;
	Vector3<float> mOffset = Vector3<float>::Zero();
	float mRotateDegree;
	bool mRandomOffsetY;
	int mFaceNum;
	float mPlantHeight;

	void DrawPlantlikeQuad(float rotation, float quadOffset = 0, bool offsetTopOnly = false);
	void DrawPlantlike();

// firelike-specific
	void DrawFirelikeQuad(float rotation, float openingAngle,
		float offsetHorizontal, float offsetVertical = 0.0);

// drawtypes
	void DrawLiquidNode();
	void DrawGlasslikeNode();
	void DrawGlasslikeFramedNode();
	void DrawAllfacesNode();
	void DrawTorchlikeNode();
	void DrawSignlikeNode();
	void DrawPlantlikeNode();
	void DrawPlantlikeRootedNode();
	void DrawFirelikeNode();
	void DrawFencelikeNode();
	void DrawRaillikeNode();
	void DrawNodeboxNode();
	void DrawMeshNode();

// common
	void ErrorUnknownDrawtype();
	void DrawNode();

public:
	MapblockMeshGenerator(MeshMakeData* input, MeshCollector* output);
	void Generate();
	void RenderSingle(uint16_t node, uint8_t param2 = 0x00);
};

#endif