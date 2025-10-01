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

#include "ContentMapBlock.h"
#include "MapBlockMesh.h"

#include "../MeshCollector.h"

#include "../../Games/Environment/VisualEnvironment.h"

#include "Core/OS/OS.h"

#include "Application/Settings.h"

// Distance of light extrapolation (for oversized nodes)
// After this distance, it gives up and considers light level constant
#define SMOOTH_LIGHTING_OVERSIZE 1.f

// Node edge count (for glasslike-framed)
#define FRAMED_EDGE_COUNT 12

// Node neighbor count, including edge-connected, but not vertex-connected
// (for glasslike-framed)
// Corresponding offsets are listed in g_27dirs
#define FRAMED_NEIGHBOR_COUNT 18

static const Vector3<short> LightDirs[8] = {
    Vector3<short>{-1, -1, -1},
    Vector3<short>{-1, -1,  1},
    Vector3<short>{-1,  1, -1},
    Vector3<short>{-1,  1,  1},
    Vector3<short>{ 1, -1, -1},
    Vector3<short>{ 1, -1,  1},
    Vector3<short>{ 1,  1, -1},
    Vector3<short>{ 1,  1,  1}};


/// Direction in the 6D format contains corresponding vectors.
/// Here P means Positive, N stands for Negative.
enum Direction6D 
{
    // 0
    D6D_ZP,
    D6D_YP,
    D6D_XP,
    D6D_ZN,
    D6D_YN,
    D6D_XN,
    // 6
    D6D_XN_YP,
    D6D_XP_YP,
    D6D_YP_ZP,
    D6D_YP_ZN,
    D6D_XN_ZP,
    D6D_XP_ZP,
    D6D_XN_ZN,
    D6D_XP_ZN,
    D6D_XN_YN,
    D6D_XP_YN,
    D6D_YN_ZP,
    D6D_YN_ZN,
    // 18
    D6D_XN_YP_ZP,
    D6D_XP_YP_ZP,
    D6D_XN_YP_ZN,
    D6D_XP_YP_ZN,
    D6D_XN_YN_ZP,
    D6D_XP_YN_ZP,
    D6D_XN_YN_ZN,
    D6D_XP_YN_ZN,
    // 26
    D6D,

    // aliases
    D6D_BACK = D6D_ZP,
    D6D_TOP = D6D_YP,
    D6D_RIGHT = D6D_XP,
    D6D_FRONT = D6D_ZN,
    D6D_BOTTOM = D6D_YN,
    D6D_LEFT = D6D_XN,
};


/// Direction in the wallmounted format.
/// P is Positive, N is Negative.
enum DirectionWallmounted 
{
    DWM_YP,
    DWM_YN,
    DWM_XP,
    DWM_XN,
    DWM_ZP,
    DWM_ZN,
};

const Vector3<short> Face26D[26] =
{
    // +right, +top, +back
    Vector3<short>{0, 0, 1}, // back
    Vector3<short>{0, 1, 0}, // top
    Vector3<short>{1, 0, 0}, // right
    Vector3<short>{0, 0,-1}, // front
    Vector3<short>{0,-1, 0}, // bottom
    Vector3<short>{-1, 0, 0}, // left
    // 6
    Vector3<short>{-1, 1, 0}, // top left
    Vector3<short>{1, 1, 0}, // top right
    Vector3<short>{0, 1, 1}, // top back
    Vector3<short>{0, 1,-1}, // top front
    Vector3<short>{-1, 0, 1}, // back left
    Vector3<short>{1, 0, 1}, // back right
    Vector3<short>{-1, 0,-1}, // front left
    Vector3<short>{1, 0,-1}, // front right
    Vector3<short>{-1,-1, 0}, // bottom left
    Vector3<short>{1,-1, 0}, // bottom right
    Vector3<short>{0,-1, 1}, // bottom back
    Vector3<short>{0,-1,-1}, // bottom front
    // 18
    Vector3<short>{-1, 1, 1}, // top back-left
    Vector3<short>{1, 1, 1}, // top back-right
    Vector3<short>{-1, 1,-1}, // top front-left
    Vector3<short>{1, 1,-1}, // top front-right
    Vector3<short>{-1,-1, 1}, // bottom back-left
    Vector3<short>{1,-1, 1}, // bottom back-right
    Vector3<short>{-1,-1,-1}, // bottom front-left
    Vector3<short>{1,-1,-1}, // bottom front-right
    // 26
};

const Vector3<short> Face6D[6] =
{
    // +right, +top, +back
    Vector3<short>{0, 0, 1}, // back
    Vector3<short>{0, 1, 0}, // top
    Vector3<short>{1, 0, 0}, // right
    Vector3<short>{0, 0,-1}, // front
    Vector3<short>{0,-1, 0}, // bottom
    Vector3<short>{-1, 0, 0} // left
};


const uint8_t WallmountedToFacedir[6] = 
{
    20,
    0,
    16 + 1,
    12 + 3,
    8,
    4 + 2
};

// Standard index set to make a quad on 4 vertices
static constexpr uint16_t QuadIndices[] = {0, 1, 2, 2, 3, 0};

const std::string MapblockMeshGenerator::mRaillikeGroupName = "connect_to_raillike";

MapblockMeshGenerator::MapblockMeshGenerator(MeshMakeData* input, MeshCollector* output)
{
	mData = input;
	mCollector = output;

	mNodeMgr = mData->mEnvironment->GetNodeManager();

    // Mesh cache is not supported with smooth lighting
	mEnableMeshCache = Settings::Get()->GetBool("enable_mesh_cache") && !mData->mSmoothLighting; 

	mBlockPosNodes = mData->mBlockPos * (short)MAP_BLOCKSIZE;
}

void MapblockMeshGenerator::UseTile(int index, uint8_t setFlags, uint8_t resetFlags, bool special)
{
	if (special)
		GetSpecialTile(index, &mTile, mPosition == mData->mCrackPosRelative);
	else
		GetTile(index, &mTile);
	if (!mData->mSmoothLighting)
		mColor = EncodeLight(mLight, mFeatures->lightSource);

	for (auto& layer : mTile.layers) 
    {
		layer.materialFlags |= setFlags;
		layer.materialFlags &= ~resetFlags;
	}
}

// Returns a tile, ready for use, non-rotated.
void MapblockMeshGenerator::GetTile(int index, TileSpec* tile)
{
	GetNodeTileN(mNode, mPosition, (uint8_t)index, mData, *tile);
}

// Returns a tile, ready for use, rotated according to the node facedir.
void MapblockMeshGenerator::GetTile(Vector3<short> direction, TileSpec *tile)
{
	GetNodeTile(mNode, mPosition, direction, mData, *tile);
}

// Returns a special tile, ready for use, non-rotated.
void MapblockMeshGenerator::GetSpecialTile(int index, TileSpec *tile, bool applyCrack)
{
	*tile = mFeatures->specialTiles[index];
	TileLayer* topLayer = nullptr;

	for (auto &layernum : tile->layers) 
    {
		TileLayer* layer = &layernum;
		if (layer->textureId == 0)
			continue;
		topLayer = layer;
		if (!layer->hasColor)
			mNode.GetColor(*mFeatures, &layer->color);
	}

	if (applyCrack)
		topLayer->materialFlags |= MATERIAL_FLAG_CRACK;
}

void MapblockMeshGenerator::DrawQuad(Vector3<float>* coords, const Vector3<short>& normal, float verticalTiling)
{
    const Vector2<float> tcoords[4] = { 
        Vector2<float>{0.0, 0.0}, Vector2<float>{1.0, 0.0},
        Vector2<float>{1.0, verticalTiling}, Vector2<float>{0.0, verticalTiling} };
	Vertex vertices[4];
	bool shadeFace = !mFeatures->lightSource && (normal != Vector3<short>::Zero());
    Vector3<float> normal2{ (float)normal[0], (float)normal[1], (float)normal[2] };
	for (int j = 0; j < 4; j++) 
    {
		vertices[j].position = coords[j] + mOrigin;
		vertices[j].normal = normal2;
        if (mData->mSmoothLighting)
        {
            SColor color = BlendLightColor(coords[j]);
            if (shadeFace)
                ApplyFacesShading(color, normal2);
            vertices[j].color = SColorF(color).ToArray();
        }
        else
        {
			SColor color = mColor;
            if (shadeFace)
                ApplyFacesShading(color, normal2);
            vertices[j].color = SColorF(color).ToArray();
        }

		vertices[j].tcoord = tcoords[j];
	}
	mCollector->Append(mTile, vertices, 4, QuadIndices, 6);
}

// Create a cuboid.
//  tiles     - the tiles (materials) to use (for all 6 faces)
//  tilecount - number of entries in tiles, 1<=tilecount<=6
//  lights    - vertex light levels. The order is the same as in LightDirs.
//              NULL may be passed if smooth lighting is disabled.
//  txc       - texture coordinates - this is a list of texture coordinates
//              for the opposite corners of each face - therefore, there
//              should be (2+2)*6=24 values in the list. The order of
//              the faces in the list is up-down-right-left-back-front
//              (compatible with ContentFeatures).
void MapblockMeshGenerator::DrawCuboid(const BoundingBox<float>& box,
	TileSpec* tiles, int tilecount, const LightInfo* lights, const float* txc)
{
	LogAssert(tilecount >= 1 && tilecount <= 6, "invalid tile count"); // pre-condition

	Vector3<float> min = box.mMinEdge;
	Vector3<float> max = box.mMaxEdge;

	SColor colors[6];
	if (!mData->mSmoothLighting) 
    {
		for (int face = 0; face != 6; ++face) 
			colors[face] = EncodeLight(mLight, mFeatures->lightSource);

		if (!mFeatures->lightSource) 
        {
            ApplyFacesShading(colors[0], Vector3<float>{0, 1, 0});
            ApplyFacesShading(colors[1], Vector3<float>{0, -1, 0});
            ApplyFacesShading(colors[2], Vector3<float>{1, 0, 0});
            ApplyFacesShading(colors[3], Vector3<float>{-1, 0, 0});
            ApplyFacesShading(colors[4], Vector3<float>{0, 0, 1});
            ApplyFacesShading(colors[5], Vector3<float>{0, 0, -1});
		}
	}

    unsigned int index = 0;
    Vertex vertices[24]; 
   
    {
		// top
        vertices[0 + index].position = Vector3<float>{ min[0], max[1], max[2] };
        vertices[1 + index].position = Vector3<float>{ max[0], max[1], max[2] };
        vertices[2 + index].position = Vector3<float>{ max[0], max[1], min[2] };
        vertices[3 + index].position = Vector3<float>{ min[0], max[1], min[2] };

        vertices[0 + index].normal = Vector3<float>{ 0, 1, 0 };
        vertices[1 + index].normal = Vector3<float>{ 0, 1, 0 };
        vertices[2 + index].normal = Vector3<float>{ 0, 1, 0 };
        vertices[3 + index].normal = Vector3<float>{ 0, 1, 0 };

        vertices[0 + index].tcoord = Vector2<float>{ txc[0], txc[1] };
        vertices[1 + index].tcoord = Vector2<float>{ txc[2], txc[1] };
        vertices[2 + index].tcoord = Vector2<float>{ txc[2], txc[3] };
        vertices[3 + index].tcoord = Vector2<float>{ txc[0], txc[3] };

        vertices[0 + index].color = SColorF(colors[0]).ToArray();
        vertices[1 + index].color = SColorF(colors[0]).ToArray();
        vertices[2 + index].color = SColorF(colors[0]).ToArray();
        vertices[3 + index].color = SColorF(colors[0]).ToArray();
        index += 4;
   }


    {
		// bottom
        vertices[0 + index].position = Vector3<float>{ min[0], min[1], min[2] };
        vertices[1 + index].position = Vector3<float>{ max[0], min[1], min[2] };
        vertices[2 + index].position = Vector3<float>{ max[0], min[1], max[2] };
        vertices[3 + index].position = Vector3<float>{ min[0], min[1], max[2] };

        vertices[0 + index].normal = Vector3<float>{ 0, -1, 0 };
        vertices[1 + index].normal = Vector3<float>{ 0, -1, 0 };
        vertices[2 + index].normal = Vector3<float>{ 0, -1, 0 };
        vertices[3 + index].normal = Vector3<float>{ 0, -1, 0 };

        vertices[0 + index].tcoord = Vector2<float>{ txc[4], txc[5] };
        vertices[1 + index].tcoord = Vector2<float>{ txc[6], txc[5] };
        vertices[2 + index].tcoord = Vector2<float>{ txc[6], txc[7] };
        vertices[3 + index].tcoord = Vector2<float>{ txc[4], txc[7] };

        vertices[0 + index].color = SColorF(colors[1]).ToArray();
        vertices[1 + index].color = SColorF(colors[1]).ToArray();
        vertices[2 + index].color = SColorF(colors[1]).ToArray();
        vertices[3 + index].color = SColorF(colors[1]).ToArray();
        index += 4;
    }


    {
		// right
        vertices[0 + index].position = Vector3<float>{ max[0], max[1], min[2] };
        vertices[1 + index].position = Vector3<float>{ max[0], max[1], max[2] };
        vertices[2 + index].position = Vector3<float>{ max[0], min[1], max[2] };
        vertices[3 + index].position = Vector3<float>{ max[0], min[1], min[2] };

        vertices[0 + index].normal = Vector3<float>{ 1, 0, 0 };
        vertices[1 + index].normal = Vector3<float>{ 1, 0, 0 };
        vertices[2 + index].normal = Vector3<float>{ 1, 0, 0 };
        vertices[3 + index].normal = Vector3<float>{ 1, 0, 0 };

        vertices[0 + index].tcoord = Vector2<float>{ txc[8], txc[9] };
        vertices[1 + index].tcoord = Vector2<float>{ txc[10], txc[9] };
        vertices[2 + index].tcoord = Vector2<float>{ txc[10], txc[11] };
        vertices[3 + index].tcoord = Vector2<float>{ txc[8], txc[11] };

        vertices[0 + index].color = SColorF(colors[2]).ToArray();
        vertices[1 + index].color = SColorF(colors[2]).ToArray();
        vertices[2 + index].color = SColorF(colors[2]).ToArray();
        vertices[3 + index].color = SColorF(colors[2]).ToArray();
        index += 4;
    }


    {
		// left
        vertices[0 + index].position = Vector3<float>{ min[0], max[1], max[2] };
        vertices[1 + index].position = Vector3<float>{ min[0], max[1], min[2] };
        vertices[2 + index].position = Vector3<float>{ min[0], min[1], min[2] };
        vertices[3 + index].position = Vector3<float>{ min[0], min[1], max[2] };

        vertices[0 + index].normal = Vector3<float>{ -1, 0, 0 };
        vertices[1 + index].normal = Vector3<float>{ -1, 0, 0 };
        vertices[2 + index].normal = Vector3<float>{ -1, 0, 0 };
        vertices[3 + index].normal = Vector3<float>{ -1, 0, 0 };

        vertices[0 + index].tcoord = Vector2<float>{ txc[12], txc[13] };
        vertices[1 + index].tcoord = Vector2<float>{ txc[14], txc[13] };
        vertices[2 + index].tcoord = Vector2<float>{ txc[14], txc[15] };
        vertices[3 + index].tcoord = Vector2<float>{ txc[12], txc[15] };

        vertices[0 + index].color = SColorF(colors[3]).ToArray();
        vertices[1 + index].color = SColorF(colors[3]).ToArray();
        vertices[2 + index].color = SColorF(colors[3]).ToArray();
        vertices[3 + index].color = SColorF(colors[3]).ToArray();
        index += 4;
    }


    {
		// back
        vertices[0 + index].position = Vector3<float>{ max[0], max[1], max[2] };
        vertices[1 + index].position = Vector3<float>{ min[0], max[1], max[2] };
        vertices[2 + index].position = Vector3<float>{ min[0], min[1], max[2] };
        vertices[3 + index].position = Vector3<float>{ max[0], min[1], max[2] };

        vertices[0 + index].normal = Vector3<float>{ 0, 0, 1 };
        vertices[1 + index].normal = Vector3<float>{ 0, 0, 1 };
        vertices[2 + index].normal = Vector3<float>{ 0, 0, 1 };
        vertices[3 + index].normal = Vector3<float>{ 0, 0, 1 };

        vertices[0 + index].tcoord = Vector2<float>{ txc[16], txc[17] };
        vertices[1 + index].tcoord = Vector2<float>{ txc[18], txc[17] };
        vertices[2 + index].tcoord = Vector2<float>{ txc[18], txc[19] };
        vertices[3 + index].tcoord = Vector2<float>{ txc[16], txc[19] };

        vertices[0 + index].color = SColorF(colors[4]).ToArray();
        vertices[1 + index].color = SColorF(colors[4]).ToArray();
        vertices[2 + index].color = SColorF(colors[4]).ToArray();
        vertices[3 + index].color = SColorF(colors[4]).ToArray();
        index += 4;
    }

    {
		// front
        vertices[0 + index].position = Vector3<float>{ min[0], max[1], min[2] };
        vertices[1 + index].position = Vector3<float>{ max[0], max[1], min[2] };
        vertices[2 + index].position = Vector3<float>{ max[0], min[1], min[2] };
        vertices[3 + index].position = Vector3<float>{ min[0], min[1], min[2] };

        vertices[0 + index].normal = Vector3<float>{ 0, 0, -1 };
        vertices[1 + index].normal = Vector3<float>{ 0, 0, -1 };
        vertices[2 + index].normal = Vector3<float>{ 0, 0, -1 };
        vertices[3 + index].normal = Vector3<float>{ 0, 0, -1 };

        vertices[0 + index].tcoord = Vector2<float>{ txc[20], txc[21] };
        vertices[1 + index].tcoord = Vector2<float>{ txc[22], txc[21] };
        vertices[2 + index].tcoord = Vector2<float>{ txc[22], txc[23] };
        vertices[3 + index].tcoord = Vector2<float>{ txc[20], txc[23] };

        vertices[0 + index].color = SColorF(colors[5]).ToArray();
        vertices[1 + index].color = SColorF(colors[5]).ToArray();
        vertices[2 + index].color = SColorF(colors[5]).ToArray();
        vertices[3 + index].color = SColorF(colors[5]).ToArray();
	}

	static const uint8_t lightIndices[24] = {
		3, 7, 6, 2,
		0, 4, 5, 1,
		6, 7, 5, 4,
		3, 2, 0, 1,
		7, 3, 1, 5,
		2, 6, 4, 0
	};

	for (int face = 0; face < 6; face++) 
    {
		int tileIndex = std::min(face, tilecount - 1);
		const TileSpec& tile = tiles[tileIndex];
		for (int j = 0; j < 4; j++) 
        {
			Vertex& vertex = vertices[face * 4 + j];
			switch (tile.rotation) 
            {
			    case 0:
				    break;
			    case 1: // R90
                    vertex.tcoord = Rotate(90, vertex.tcoord);
				    break;
			    case 2: // R180
                    vertex.tcoord = Rotate(180, vertex.tcoord);
				    break;
			    case 3: // R270
                    vertex.tcoord = Rotate(270, vertex.tcoord);
				    break;
			    case 4: // FXR90
                    vertex.tcoord[0] = 1.f - vertex.tcoord[0];
                    vertex.tcoord = Rotate(90, vertex.tcoord);
				    break;
			    case 5: // FXR270
                    vertex.tcoord[0] = 1.f - vertex.tcoord[0];
                    vertex.tcoord = Rotate(270, vertex.tcoord);
				    break;
			    case 6: // FYR90
                    vertex.tcoord[1] = 1.f - vertex.tcoord[1];
                    vertex.tcoord = Rotate(90, vertex.tcoord);
				    break;
			    case 7: // FYR270
                    vertex.tcoord[1] = 1.f - vertex.tcoord[1];
                    vertex.tcoord = Rotate(270, vertex.tcoord);
				    break;
			    case 8: // FX
                    vertex.tcoord[0] = 1.f - vertex.tcoord[0];
				    break;
			    case 9: // FY
                    vertex.tcoord[1] = 1.f - vertex.tcoord[1];
				    break;
			    default:
				    break;
			}
		}
	}

	if (mData->mSmoothLighting) 
    {
		for (int j = 0; j < 24; ++j) 
        {
			Vertex& vertex = vertices[j];
			SColor color = EncodeLight(
				lights[lightIndices[j]].GetPair(std::max(0.0f, vertex.normal[1])), mFeatures->lightSource);
			if (!mFeatures->lightSource)
				ApplyFacesShading(color, vertex.normal);
            vertex.color = SColorF(color).ToArray();
		}
	}

	// Add to mesh collector
	for (int k = 0; k < 6; ++k) 
    {
		int tileIndex = std::min(k, tilecount - 1);
		mCollector->Append(tiles[tileIndex], vertices + 4 * k, 4, QuadIndices, 6);
	}
}

// Gets the base lighting values for a node
void MapblockMeshGenerator::GetSmoothLightFrame()
{
	for (int k = 0; k < 8; ++k)
		mFrame.sunlight[k] = false;
	for (int k = 0; k < 8; ++k) 
    {
		LightPair light(GetSmoothLightTransparent(mBlockPosNodes + mPosition, LightDirs[k], mData));
        mFrame.lightsDay[k] = light.lightDay;
        mFrame.lightsNight[k] = light.lightNight;
		// If there is direct sunlight and no ambient occlusion at some corner,
		// mark the vertical edge (top and bottom corners) containing it.
		if (light.lightDay == 255) 
        {
            mFrame.sunlight[k] = true;
            mFrame.sunlight[k ^ 2] = true;
		}
	}
}

// Calculates vertex light level
//  vertexPos - vertex position in the node (coordinates are clamped to [0.0, 1.0] or so)
LightInfo MapblockMeshGenerator::BlendLight(const Vector3<float>& vertexPos)
{
	// Light levels at (logical) node corners are known. Here,
	// trilinear interpolation is used to calculate light level
	// at a given point in the node.
	float x = std::clamp(vertexPos[0] / BS + 0.5f, 0.f - SMOOTH_LIGHTING_OVERSIZE, 1.f + SMOOTH_LIGHTING_OVERSIZE);
	float y = std::clamp(vertexPos[1] / BS + 0.5f, 0.f - SMOOTH_LIGHTING_OVERSIZE, 1.f + SMOOTH_LIGHTING_OVERSIZE);
	float z = std::clamp(vertexPos[2] / BS + 0.5f, 0.f - SMOOTH_LIGHTING_OVERSIZE, 1.f + SMOOTH_LIGHTING_OVERSIZE);
	float lightDay = 0.0; // daylight
	float lightNight = 0.0;
	float lightBoosted = 0.0; // daylight + direct sunlight, if any
	for (int k = 0; k < 8; ++k) 
    {
		float dx = (k & 4) ? x : 1 - x;
		float dy = (k & 2) ? y : 1 - y;
		float dz = (k & 1) ? z : 1 - z;
		// Use direct sunlight (255), if any; use daylight otherwise.
		float lightBoosted = mFrame.sunlight[k] ? 255 : mFrame.lightsDay[k];
		lightDay += dx * dy * dz * mFrame.lightsDay[k];
		lightNight += dx * dy * dz * mFrame.lightsNight[k];
		lightBoosted += dx * dy * dz * lightBoosted;
	}
	return LightInfo{lightDay, lightNight, lightBoosted};
}

// Calculates vertex color to be used in mapblock mesh
//  vertexPos - vertex position in the node (coordinates are clamped to [0.0, 1.0] or so)
//  tile_color - node's tile color
SColor MapblockMeshGenerator::BlendLightColor(const Vector3<float>& vertexPos)
{
	LightInfo light = BlendLight(vertexPos);
	return EncodeLight(light.GetPair(), mFeatures->lightSource);
}

SColor MapblockMeshGenerator::BlendLightColor(
    const Vector3<float>& vertexPos, const Vector3<float>& vertexNormal)
{
	LightInfo light = BlendLight(vertexPos);
	SColor color = EncodeLight(
        light.GetPair(std::max(0.0f, vertexNormal[1])), mFeatures->lightSource);
	if (!mFeatures->lightSource)
		ApplyFacesShading(color, vertexNormal);
	return color;
}

void MapblockMeshGenerator::GenerateCuboidTextureCoords(const BoundingBox<float>& box, float* coords)
{
	float tx1 = (box.mMinEdge[0] / BS) + 0.5f;
	float ty1 = (box.mMinEdge[1] / BS) + 0.5f;
	float tz1 = (box.mMinEdge[2] / BS) + 0.5f;
	float tx2 = (box.mMaxEdge[0] / BS) + 0.5f;
	float ty2 = (box.mMaxEdge[1] / BS) + 0.5f;
	float tz2 = (box.mMaxEdge[2] / BS) + 0.5f;
	float txc[24] = {
		    tx1, 1 - tz2,     tx2, 1 - tz1, // up
		    tx1,     tz1,     tx2,     tz2, // down
		    tz1, 1 - ty2,     tz2, 1 - ty1, // right
		1 - tz2, 1 - ty2, 1 - tz1, 1 - ty1, // left
		1 - tx2, 1 - ty2, 1 - tx1, 1 - ty1, // back
		    tx1, 1 - ty2,     tx2, 1 - ty1, // front
	};
	for (int i = 0; i != 24; ++i)
		coords[i] = txc[i];
}

void MapblockMeshGenerator::DrawAutoLightedCuboid(BoundingBox<float> box, 
    const float* txc, TileSpec* tiles, int tileCount)
{
	bool scale = std::fabs(mFeatures->visualScale - 1.0f) > 1e-3f;
	float textureCoordBuf[24];
	float dx1 = box.mMinEdge[0];
	float dy1 = box.mMinEdge[1];
	float dz1 = box.mMinEdge[2];
	float dx2 = box.mMaxEdge[0];
	float dy2 = box.mMaxEdge[1];
	float dz2 = box.mMaxEdge[2];
	if (scale) 
    {
		if (!txc) 
        { // generate texture coords before scaling
			GenerateCuboidTextureCoords(box, textureCoordBuf);
			txc = textureCoordBuf;
		}
		box.mMinEdge *= mFeatures->visualScale;
		box.mMaxEdge *= mFeatures->visualScale;
	}
	box.mMinEdge += mOrigin;
	box.mMaxEdge += mOrigin;
	if (!txc) 
    {
		GenerateCuboidTextureCoords(box, textureCoordBuf);
		txc = textureCoordBuf;
	}
	if (!tiles) 
    {
		tiles = &mTile;
		tileCount = 1;
	}
	if (mData->mSmoothLighting) 
    {
		LightInfo lights[8];
		for (int j = 0; j < 8; ++j) 
        {
			Vector3<float> d;
			d[0] = (j & 4) ? dx2 : dx1;
			d[1] = (j & 2) ? dy2 : dy1;
			d[2] = (j & 1) ? dz2 : dz1;
			lights[j] = BlendLight(d);
		}
		DrawCuboid(box, tiles, tileCount, lights, txc);
	} 
    else 
    {
		DrawCuboid(box, tiles, tileCount, nullptr, txc);
	}
}

void MapblockMeshGenerator::PrepareLiquidNodeDrawing()
{
	GetSpecialTile(0, &mTileLiquidTop);
	GetSpecialTile(1, &mTileLiquid);

    MapNode ntop = mData->mVManip.GetNodeNoEx(mBlockPosNodes + 
        Vector3<short>{mPosition[0], (short)(mPosition[1] + 1), mPosition[2]});
    MapNode nbottom = mData->mVManip.GetNodeNoEx(mBlockPosNodes + 
        Vector3<short>{mPosition[0], (short)(mPosition[1] - 1), mPosition[2]});
	mContentFlowing = mFeatures->liquidAlternativeFlowingId;
	mContentSource = mFeatures->liquidAlternativeSourceId;
	mTopIsSameLiquid = 
        (ntop.GetContent() == mContentFlowing) || 
        (ntop.GetContent() == mContentSource);
	mDrawLiquidBottom = 
        (nbottom.GetContent() != mContentFlowing) && 
        (nbottom.GetContent() != mContentSource);
	if (mDrawLiquidBottom) 
    {
		const ContentFeatures& f2 = mNodeMgr->Get(nbottom.GetContent());
		if (f2.solidness > 1)
			mDrawLiquidBottom = false;
	}

	if (mData->mSmoothLighting)
		return; // don't need to pre-compute anything in this case

	if (mFeatures->lightSource != 0) 
    {
		// If this liquid emits light and doesn't contain light, draw
		// it at what it emits, for an increased effect
		uint8_t e = DecodeLight(mFeatures->lightSource);
		mLight = LightPair(std::max(e, mLight.lightDay), std::max(e, mLight.lightNight));
	} 
    else if (mNodeMgr->Get(ntop).paramType == CPT_LIGHT) 
    {
		// Otherwise, use the light of the node on top if possible
        mLight = LightPair(GetInteriorLight(ntop, 0, mNodeMgr));
	}

	mColorLiquidTop = EncodeLight(mLight, mFeatures->lightSource);
	mColor = EncodeLight(mLight, mFeatures->lightSource);
}

void MapblockMeshGenerator::GetLiquidNeighborhood()
{
	uint8_t range = std::clamp((int)mNodeMgr->Get(mContentFlowing).liquidRange, 1, 8);

    for (short w = -1; w <= 1; w++)
    {
        for (short u = -1; u <= 1; u++)
        {
            NeighborData& neighbor = mLiquidNeighbors[w + 1][u + 1];
            Vector3<short> p2 = mPosition + Vector3<short>{u, 0, w};
            MapNode n2 = mData->mVManip.GetNodeNoEx(mBlockPosNodes + p2);
            neighbor.content = n2.GetContent();
            neighbor.level = -0.5 * BS;
            neighbor.isSameLiquid = false;
            neighbor.topIsSameLiquid = false;

            if (neighbor.content == CONTENT_IGNORE)
                continue;

            if (neighbor.content == mContentSource) 
            {
                neighbor.isSameLiquid = true;
                neighbor.level = 0.5 * BS;
            }
            else if (neighbor.content == mContentFlowing) 
            {
                neighbor.isSameLiquid = true;
                uint8_t liquidLevel = (n2.param2 & LIQUID_LEVEL_MASK);
                if (liquidLevel <= LIQUID_LEVEL_MAX + 1 - range)
                    liquidLevel = 0;
                else
                    liquidLevel -= (LIQUID_LEVEL_MAX + 1 - range);
                neighbor.level = (-0.5f + (liquidLevel + 0.5f) / range) * BS;
            }

            // Check node above neighbor.
            // NOTE: This doesn't get executed if neighbor
            //       doesn't exist
            p2[1]++;
            n2 = mData->mVManip.GetNodeNoEx(mBlockPosNodes + p2);
            if (n2.GetContent() == mContentSource || n2.GetContent() == mContentFlowing)
                neighbor.topIsSameLiquid = true;
        }
    }
}

void MapblockMeshGenerator::CalculateCornerLevels()
{
	for (int k = 0; k < 2; k++)
	    for (int i = 0; i < 2; i++)
		    mCornerLevels[k][i] = GetCornerLevel(i, k);
}

float MapblockMeshGenerator::GetCornerLevel(int i, int k)
{
	float sum = 0;
	int count = 0;
	int airCount = 0;
    for (int dk = 0; dk < 2; dk++)
    {
        for (int di = 0; di < 2; di++) 
        {
            NeighborData& neighborData = mLiquidNeighbors[k + dk][i + di];
            uint16_t content = neighborData.content;

            // If top is liquid, draw starting from top of node
            if (neighborData.topIsSameLiquid)
                return 0.5 * BS;

            // Source always has the full height
            if (content == mContentSource)
                return 0.5 * BS;

            // Flowing liquid has level information
            if (content == mContentFlowing) 
            {
                sum += neighborData.level;
                count++;
            }
            else if (content == CONTENT_AIR) 
            {
                airCount++;
            }
        }
    }
	if (airCount >= 2)
		return -0.5f * BS + 0.2f;
	if (count > 0)
		return sum / count;
	return 0;
}

struct LiquidFaceDesc 
{
	Vector3<short> dir; // XZ
	Vector3<short> p[2]; // XZ only; 1 means +, 0 means -
};

struct UV 
{
	int u, v;
};
static const LiquidFaceDesc LiquidBaseFaces[4] = {
    {Vector3<short>{ 1, 0,  0}, {Vector3<short>{1, 0, 1}, Vector3<short>{1, 0, 0}}},
    {Vector3<short>{-1, 0,  0}, {Vector3<short>{0, 0, 0}, Vector3<short>{0, 0, 1}}},
    {Vector3<short>{ 0, 0,  1}, {Vector3<short>{0, 0, 1}, Vector3<short>{1, 0, 1}}},
    {Vector3<short>{ 0, 0, -1}, {Vector3<short>{1, 0, 0}, Vector3<short>{0, 0, 0}}},
};
static const UV LiquidBaseVertices[4] = {
	{0, 1},
	{1, 1},
	{1, 0},
	{0, 0}};

void MapblockMeshGenerator::DrawLiquidSides()
{
	for (const auto& face : LiquidBaseFaces) 
    {
		const NeighborData& neighbor = mLiquidNeighbors[face.dir[2] + 1][face.dir[0] + 1];

		// No face between nodes of the same liquid, unless there is node
		// at the top to which it should be connected. Again, unless the face
		// there would be inside the liquid
		if (neighbor.isSameLiquid) 
        {
			if (!mTopIsSameLiquid)
				continue;
			if (neighbor.topIsSameLiquid)
				continue;
		}

		const ContentFeatures& neighborFeatures = mNodeMgr->Get(neighbor.content);
		// Don't draw face if neighbor is blocking the view
		if (neighborFeatures.solidness == 2)
			continue;

		Vertex vertices[4];
		for (int j = 0; j < 4; j++) 
        {
			const UV& vertex = LiquidBaseVertices[j];
			const Vector3<short>& base = face.p[vertex.u];
			float v = (float)vertex.v;

			Vector3<float> pos;
			pos[0] = (base[0] - 0.5f) * BS;
			pos[2] = (base[2] - 0.5f) * BS;
			if (vertex.v) 
            {
				pos[1] = neighbor.isSameLiquid ? 
                    mCornerLevels[base[2]][base[0]] : -0.5f * BS;
			} 
            else if (mTopIsSameLiquid) 
            {
				pos[1] = 0.5f * BS;
			} 
            else 
            {
				pos[1] = mCornerLevels[base[2]][base[0]];
				v += (0.5f * BS - mCornerLevels[base[2]][base[0]]) / BS;
			}

			if (mData->mSmoothLighting)
				mColor = BlendLightColor(pos);
			pos += mOrigin;
            vertices[j].position = Vector3<float>{ pos[0], pos[1], pos[2] };
            vertices[j].normal = Vector3<float>{ 0, 0, 0 };
            vertices[j].color = SColorF(mColor).ToArray();
            vertices[j].tcoord = Vector2<float>{ (float)vertex.u, v };
		};
		mCollector->Append(mTileLiquid, vertices, 4, QuadIndices, 6);
	}
}

void MapblockMeshGenerator::DrawLiquidTop()
{
	// To get backface culling right, the vertices need to go
	// clockwise around the front of the face. And we happened to
	// calculate corner levels in exact reverse order.
	static const int cornerResolve[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

    Vertex vertices[4];
    {
        vertices[0].position = Vector3<float>{ -BS / 2, 0,  BS / 2 };
        vertices[0].normal = Vector3<float>{ 0, 0, 0 };
        vertices[0].color = SColorF(mColorLiquidTop).ToArray();
        vertices[0].tcoord = Vector2<float>{ 0, 1 };

        vertices[1].position = Vector3<float>{ BS / 2, 0,  BS / 2 };
        vertices[1].normal = Vector3<float>{ 0, 0, 0 };
        vertices[1].color = SColorF(mColorLiquidTop).ToArray();
        vertices[1].tcoord = Vector2<float>{ 1, 1 };

        vertices[2].position = Vector3<float>{ BS / 2, 0, -BS / 2 };
        vertices[2].normal = Vector3<float>{ 0, 0, 0 };
        vertices[2].color = SColorF(mColorLiquidTop).ToArray();
        vertices[2].tcoord = Vector2<float>{ 1, 0 };

        vertices[3].position = Vector3<float>{ -BS / 2, 0, -BS / 2 };
        vertices[3].normal = Vector3<float>{ 0, 0, 0 };
        vertices[3].color = SColorF(mColorLiquidTop).ToArray();
        vertices[3].tcoord = Vector2<float>{ 0, 0 };
	}

	for (int i = 0; i < 4; i++) 
    {
		int u = cornerResolve[i][0];
		int w = cornerResolve[i][1];
		vertices[i].position[1] += mCornerLevels[w][u];
		if (mData->mSmoothLighting)
			vertices[i].color = SColorF(BlendLightColor(vertices[i].position)).ToArray();
		vertices[i].position += mOrigin;
	}

	// Default downwards-flowing texture animation goes from
	// -Z towards +Z, thus the direction is +Z.
	// Rotate texture to make animation go in flow direction
	// Positive if liquid moves towards +Z
	float dz = (mCornerLevels[0][0] + mCornerLevels[0][1]) -
	         (mCornerLevels[1][0] + mCornerLevels[1][1]);
	// Positive if liquid moves towards +X
	float dx = (mCornerLevels[0][0] + mCornerLevels[1][0]) -
	         (mCornerLevels[0][1] + mCornerLevels[1][1]);
	float tCoordAngle = atan2(dz, dx) * (float)GE_C_DEG_TO_RAD;
    Vector2<float> tCoordCenter{ 0.5f, 0.5f };
    Vector2<float> tCoordTranslate{ 
        (float)(mBlockPosNodes[2] + mPosition[2]), 
        (float)(mBlockPosNodes[0] + mPosition[0]) };
    tCoordTranslate = Rotate(tCoordAngle, tCoordTranslate);
    tCoordTranslate[0] -= floor(tCoordTranslate[0]);
    tCoordTranslate[1] -= floor(tCoordTranslate[1]);

	for (Vertex& vertex : vertices) 
    {
        vertex.tcoord = Rotate(tCoordAngle, vertex.tcoord, tCoordCenter);
		vertex.tcoord += tCoordTranslate;
	}

	std::swap(vertices[0].tcoord, vertices[2].tcoord);
	mCollector->Append(mTileLiquidTop, vertices, 4, QuadIndices, 6);
}

void MapblockMeshGenerator::DrawLiquidBottom()
{
    Vertex vertices[4];
    {
        vertices[0].position = Vector3<float>{ -BS / 2, -BS / 2, -BS / 2 };
        vertices[0].normal = Vector3<float>{ 0, 0, 0 };
        vertices[0].color = SColorF(mColorLiquidTop).ToArray();
        vertices[0].tcoord = Vector2<float>{ 0, 0 };

        vertices[1].position = Vector3<float>{ BS / 2, -BS / 2, -BS / 2 };
        vertices[1].normal = Vector3<float>{ 0, 0, 0 };
        vertices[1].color = SColorF(mColorLiquidTop).ToArray();
        vertices[1].tcoord = Vector2<float>{ 1, 0 };

        vertices[2].position = Vector3<float>{ BS / 2, -BS / 2,  BS / 2 };
        vertices[2].normal = Vector3<float>{ 0, 0, 0 };
        vertices[2].color = SColorF(mColorLiquidTop).ToArray();
        vertices[2].tcoord = Vector2<float>{ 1, 1 };

        vertices[3].position = Vector3<float>{ -BS / 2, -BS / 2,  BS / 2 };
        vertices[3].normal = Vector3<float>{ 0, 0, 0 };
        vertices[3].color = SColorF(mColorLiquidTop).ToArray();
        vertices[3].tcoord = Vector2<float>{ 0, 1 };
    }

	for (int i = 0; i < 4; i++) 
    {
		if (mData->mSmoothLighting)
			vertices[i].color = SColorF(BlendLightColor(vertices[i].position)).ToArray();
		vertices[i].position += mOrigin;
	}

	mCollector->Append(mTileLiquidTop, vertices, 4, QuadIndices, 6);
}

void MapblockMeshGenerator::DrawLiquidNode()
{
	PrepareLiquidNodeDrawing();
	GetLiquidNeighborhood();
	CalculateCornerLevels();
	DrawLiquidSides();
	if (!mTopIsSameLiquid)
		DrawLiquidTop();
	if (mDrawLiquidBottom)
		DrawLiquidBottom();
}

void MapblockMeshGenerator::DrawGlasslikeNode()
{
	UseTile(0, 0, 0);

	for (int face = 0; face < 6; face++) 
    {
		// Check this neighbor
		Vector3<short> dir = Face6D[face];
		Vector3<short> neighborPos = mBlockPosNodes + mPosition + dir;
		MapNode neighbor = mData->mVManip.GetNodeNoExNoEmerge(neighborPos);
		// Don't make face if neighbor is of same type
		if (neighbor.GetContent() == mNode.GetContent())
			continue;

		// Face at Z-
		Vector3<float> vertices[4] = {
            Vector3<float>{-BS / 2,  BS / 2, -BS / 2},
            Vector3<float>{ BS / 2,  BS / 2, -BS / 2},
            Vector3<float>{ BS / 2, -BS / 2, -BS / 2},
            Vector3<float>{-BS / 2, -BS / 2, -BS / 2}
		};

        Quaternion<float> tgt;
        for (Vector3<float>& vertex : vertices)
        {
            switch (face)
            {
				case D6D_ZP:
					tgt = Rotation<3, float>(
						AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
					vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy(180);
					break;
				case D6D_YP:
					tgt = Rotation<3, float>(
						AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
					vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateYZBy( 90);
					break;
				case D6D_XP:
					tgt = Rotation<3, float>(
						AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
					vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy( 90);
					break;
				case D6D_ZN:
					tgt = Rotation<3, float>(
						AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 0 * (float)GE_C_DEG_TO_RAD));
					vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy(  0);
					break;
				case D6D_YN:
					tgt = Rotation<3, float>(
						AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
					vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateYZBy(-90);
					break;
				case D6D_XN:
					tgt = Rotation<3, float>(
						AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
					vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy(-90);
					break;
            }
        }
		DrawQuad(vertices, dir);
	}
}

void MapblockMeshGenerator::DrawGlasslikeFramedNode()
{
	TileSpec tiles[6];
	for (int face = 0; face < 6; face++)
		GetTile(Face6D[face], &tiles[face]);

	if (!mData->mSmoothLighting)
		mColor = EncodeLight(mLight, mFeatures->lightSource);

	TileSpec glassTiles[6];
	for (auto& glassTile : glassTiles)
		glassTile = tiles[4];

	// Only respect H/V merge bits when paramtype2 = "glasslikeliquidlevel" (liquid tank)
	uint8_t param2 = 
        (mFeatures->paramType2 == CPT2_GLASSLIKE_LIQUID_LEVEL) ? mNode.GetParam2() : 0;
	bool hMerge = !(param2 & 128);
	bool vMerge = !(param2 & 64);
	param2 &= 63;

	static const float a = BS / 2.0f;
	static const float g = a - 0.03f;
	static const float b = 0.876f * (BS / 2.0f);

	static const BoundingBox<float> frameEdges[FRAMED_EDGE_COUNT] = {
		BoundingBox<float>( b,  b, -a,  a,  a,  a), // y+
		BoundingBox<float>(-a,  b, -a, -b,  a,  a), // y+
		BoundingBox<float>( b, -a, -a,  a, -b,  a), // y-
		BoundingBox<float>(-a, -a, -a, -b, -b,  a), // y-
		BoundingBox<float>( b, -a,  b,  a,  a,  a), // x+
		BoundingBox<float>( b, -a, -a,  a,  a, -b), // x+
		BoundingBox<float>(-a, -a,  b, -b,  a,  a), // x-
		BoundingBox<float>(-a, -a, -a, -b,  a, -b), // x-
		BoundingBox<float>(-a,  b,  b,  a,  a,  a), // z+
		BoundingBox<float>(-a, -a,  b,  a, -b,  a), // z+
		BoundingBox<float>(-a, -a, -a,  a, -b, -b), // z-
		BoundingBox<float>(-a,  b, -a,  a,  a, -b), // z-
	};

	// tables of neighbour (connect if same type and merge allowed),
	// checked with g_26dirs

	// 1 = connect, 0 = face visible
	bool nb[FRAMED_NEIGHBOR_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	// 1 = check
	static const bool checkNeighborVertical [FRAMED_NEIGHBOR_COUNT] =
		{0,1,0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
	static const bool checkNeighborHorizontal [FRAMED_NEIGHBOR_COUNT] =
		{1,0,1,1,0,1, 0,0,0,0, 1,1,1,1, 0,0,0,0};
	static const bool checkNeighborAll [FRAMED_NEIGHBOR_COUNT] =
		{1,1,1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};
	const bool* checkNeighbor = checkNeighborAll;

	// neighbours checks for frames visibility
	if (hMerge || vMerge) 
    {
		if (!hMerge)
			checkNeighbor = checkNeighborVertical; // vertical-only merge
		if (!vMerge)
			checkNeighbor = checkNeighborHorizontal; // horizontal-only merge
		uint16_t current = mNode.GetContent();
		for (int i = 0; i < FRAMED_NEIGHBOR_COUNT; i++) 
        {
			if (!checkNeighbor[i])
				continue;
			Vector3<short> node2Position = mBlockPosNodes + mPosition + Face26D[i];
			MapNode node2 = mData->mVManip.GetNodeNoEx(node2Position);
			uint16_t node2Content = node2.GetContent();
			if (node2Content == current)
				nb[i] = 1;
		}
	}

	// edge visibility

	static const uint8_t neighborTriplet[FRAMED_EDGE_COUNT][3] = {
		{1, 2,  7}, {1, 5,  6}, {4, 2, 15}, {4, 5, 14},
		{2, 0, 11}, {2, 3, 13}, {5, 0, 10}, {5, 3, 12},
		{0, 1,  8}, {0, 4, 16}, {3, 4, 17}, {3, 1,  9},
	};

	mTile = tiles[1];
	for (int edge = 0; edge < FRAMED_EDGE_COUNT; edge++) 
    {
		bool edgeInvisible;
		if (nb[neighborTriplet[edge][2]])
			edgeInvisible = nb[neighborTriplet[edge][0]] & nb[neighborTriplet[edge][1]];
		else
			edgeInvisible = nb[neighborTriplet[edge][0]] ^ nb[neighborTriplet[edge][1]];
		if (edgeInvisible)
			continue;
		DrawAutoLightedCuboid(frameEdges[edge]);
	}

	for (int face = 0; face < 6; face++) 
    {
		if (nb[face])
			continue;

		mTile = glassTiles[face];
		// Face at Z-
		Vector3<float> vertices[4] = {
            Vector3<float>{-a,  a, -g},
            Vector3<float>{ a,  a, -g},
            Vector3<float>{ a, -a, -g},
            Vector3<float>{-a, -a, -g},
		};

        Quaternion<float> tgt;
		for (Vector3<float>& vertex : vertices) 
        {
			switch (face) 
            {
                case D6D_ZP:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y),180 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy(180);
                    break;
                case D6D_YP:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateYZBy( 90);
                    break;
                case D6D_XP:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy( 90);
                    break;
                case D6D_ZN:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 0 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					break;
                    //vertex.rotateXZBy(  0);
                case D6D_YN:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateYZBy(-90);
                    break;
                case D6D_XN:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
					//vertex.rotateXZBy(-90);
                    break;
			}
		}
		Vector3<short> dir = Face6D[face];
		DrawQuad(vertices, dir);
	}

	// Optionally render internal liquid level defined by param2
	// Liquid is textured with 1 tile defined in nodeMgr 'specialTiles'
	if (param2 > 0 && mFeatures->paramType2 == CPT2_GLASSLIKE_LIQUID_LEVEL &&
			mFeatures->specialTiles[0].layers[0].texture) 
    {
		// Internal liquid level has param2 range 0 .. 63,
		// convert it to -0.5 .. 0.5
		float vlev = (param2 / 63.0f) * 2.0f - 1.0f;
		GetSpecialTile(0, &mTile);
		DrawAutoLightedCuboid(BoundingBox<float>(-(nb[5] ? g : b), -(nb[4] ? g : b), 
            -(nb[3] ? g : b), (nb[2] ? g : b), (nb[1] ? g : b) * vlev, (nb[0] ? g : b)));
	}
}

void MapblockMeshGenerator::DrawAllfacesNode()
{
	static const BoundingBox<float> box(-BS / 2, -BS / 2, -BS / 2, BS / 2, BS / 2, BS / 2);
	UseTile(0, 0, 0);
	DrawAutoLightedCuboid(box);
}

void MapblockMeshGenerator::DrawTorchlikeNode()
{
	uint8_t wall = mNode.GetWallMounted(mNodeMgr);
	uint8_t tileIndex = 0;
	switch (wall) 
    {
		case DWM_YP: tileIndex = 1; break; // ceiling
		case DWM_YN: tileIndex = 0; break; // floor
		default:     tileIndex = 2; // side (or invalid—should we care?)
	}
	UseTile(tileIndex, MATERIAL_FLAG_CRACK_OVERLAY, MATERIAL_FLAG_BACKFACE_CULLING);

	float size = BS / 2 * mFeatures->visualScale;
	Vector3<float> vertices[4] = {
        Vector3<float>{-size,  size, 0},
        Vector3<float>{ size,  size, 0},
        Vector3<float>{ size, -size, 0},
        Vector3<float>{-size, -size, 0},
	};

    Quaternion<float> tgt;
	for (Vector3<float> &vertex : vertices) 
    {
		switch (wall) 
        {
			case DWM_YP:
				vertex[1] += -size + BS/2;
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -45 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                //vertex.rotateXZBy(-45);
				break;
			case DWM_YN:
				vertex[1] += size - BS/2;
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 45 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                //vertex.rotateXZBy(45);
				break;
			case DWM_XP:
				vertex[0] += -size + BS/2;
				break;
			case DWM_XN:
				vertex[0] += -size + BS/2;
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                //vertex.rotateXZBy(180);
				break;
			case DWM_ZP:
				vertex[0] += -size + BS/2;
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                //vertex.rotateXZBy(90);
				break;
			case DWM_ZN:
				vertex[0] += -size + BS/2;
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                //vertex.rotateXZBy(-90);
				break;
		}
	}
	DrawQuad(vertices);
}

void MapblockMeshGenerator::DrawSignlikeNode()
{
	uint8_t wall = mNode.GetWallMounted(mNodeMgr);
	UseTile(0, MATERIAL_FLAG_CRACK_OVERLAY, MATERIAL_FLAG_BACKFACE_CULLING);
	static const float offset = BS / 16;
	float size = BS / 2 * mFeatures->visualScale;
	// Wall at X+ of node
	Vector3<float> vertices[4] = {
        Vector3<float>{BS / 2 - offset,  size,  size},
        Vector3<float>{BS / 2 - offset,  size, -size},
        Vector3<float>{BS / 2 - offset, -size, -size},
        Vector3<float>{BS / 2 - offset, -size,  size},
	};

    Quaternion<float> tgt;
	for (Vector3<float> &vertex : vertices) 
    {
		switch (wall)
        {
			case DWM_YP:
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
				//vertex.rotateXYBy( 90); 
                break;
			case DWM_YN:
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
				//vertex.rotateXYBy(-90); 
                break;
			case DWM_XP:
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 0 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
				//vertex.rotateXZBy(  0); 
                break;
			case DWM_XN:
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
				//vertex.rotateXZBy(180); 
                break;
			case DWM_ZP:
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
				//vertex.rotateXZBy( 90); 
                break;
			case DWM_ZN:
                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
				//vertex.rotateXZBy(-90); 
                break;
		}
	}
	DrawQuad(vertices);
}

void MapblockMeshGenerator::DrawPlantlikeQuad(
    float rotation, float quadOffset, bool offsetTopOnly)
{
	Vector3<float> vertices[4] = {
        Vector3<float>{-mScale, -BS / 2 + 2.f * mScale * mPlantHeight, 0},
        Vector3<float>{ mScale, -BS / 2 + 2.f * mScale * mPlantHeight, 0},
        Vector3<float>{ mScale, -BS / 2, 0},
        Vector3<float>{-mScale, -BS / 2, 0},
	};
	if (mRandomOffsetY) 
    {
        PseudoRandom yrng(mFaceNum++ |
            mPosition[0] << 16 | mPosition[2] << 8 | mPosition[1] << 24);
		mOffset[1] = -BS * ((yrng.Next() % 16 / 16.f) * 0.125f);
	}
	int offset_count = offsetTopOnly ? 2 : 4;
	for (int i = 0; i < offset_count; i++)
		vertices[i][2] += quadOffset;

    Quaternion<float> tgt;
	for (Vector3<float>& vertex : vertices) 
    {
        tgt = Rotation<3, float>(AxisAngle<3, float>(
			-Vector3<float>::Unit(AXIS_Y), rotation + mRotateDegree * (float)GE_C_DEG_TO_RAD));
        vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
		//vertex.rotateXZBy(rotation + mRotateDegree);

		vertex += mOffset;
	}
	DrawQuad(vertices, Vector3<short>::Zero(), mPlantHeight);
}

void MapblockMeshGenerator::DrawPlantlike()
{
	mDrawStyle = PLANT_STYLE_CROSS;
	mScale = BS / 2 * mFeatures->visualScale;
	mOffset.MakeZero();
	mRotateDegree = 0.0f;
	mRandomOffsetY = false;
	mFaceNum = 0;
	mPlantHeight = 1.0;

	switch (mFeatures->paramType2) 
    {
	    case CPT2_MESHOPTIONS:
		    mDrawStyle = PlantlikeStyle(mNode.param2 & MO_MASK_STYLE);
		    if (mNode.param2 & MO_BIT_SCALE_SQRT2)
			    mScale *= 1.41421f;
		    if (mNode.param2 & MO_BIT_RANDOM_OFFSET) 
            {
                PseudoRandom rng(mPosition[0] << 8 | mPosition[2] | mPosition[1] << 16);
			    mOffset[0] = BS * ((rng.Next() % 16 / 16.f) * 0.29f - 0.145f);
			    mOffset[2] = BS * ((rng.Next() % 16 / 16.f) * 0.29f - 0.145f);
		    }
		    if (mNode.param2 & MO_BIT_RANDOM_OFFSET_Y)
			    mRandomOffsetY = true;
		    break;

	    case CPT2_DEGROTATE:
	    case CPT2_COLORED_DEGROTATE:
		    mRotateDegree = 1.5f * mNode.GetDegRotate(mNodeMgr);
		    break;

	    case CPT2_LEVELED:
		    mPlantHeight = mNode.param2 / 16.f;
		    break;

	    default:
		    break;
	}

	switch (mDrawStyle) 
    {
	    case PLANT_STYLE_CROSS:
		    DrawPlantlikeQuad(46);
		    DrawPlantlikeQuad(-44);
		    break;

	    case PLANT_STYLE_CROSS2:
		    DrawPlantlikeQuad(91);
		    DrawPlantlikeQuad(1);
		    break;

	    case PLANT_STYLE_STAR:
		    DrawPlantlikeQuad(121);
		    DrawPlantlikeQuad(241);
		    DrawPlantlikeQuad(1);
		    break;

	    case PLANT_STYLE_HASH:
		    DrawPlantlikeQuad(  1, BS / 4);
		    DrawPlantlikeQuad( 91, BS / 4);
		    DrawPlantlikeQuad(181, BS / 4);
		    DrawPlantlikeQuad(271, BS / 4);
		    break;

	    case PLANT_STYLE_HASH2:
		    DrawPlantlikeQuad(  1, -BS / 2, true);
		    DrawPlantlikeQuad( 91, -BS / 2, true);
		    DrawPlantlikeQuad(181, -BS / 2, true);
		    DrawPlantlikeQuad(271, -BS / 2, true);
		    break;
	}
}

void MapblockMeshGenerator::DrawPlantlikeNode()
{
	UseTile();
	DrawPlantlike();
}

void MapblockMeshGenerator::DrawPlantlikeRootedNode()
{
	UseTile(0, MATERIAL_FLAG_CRACK_OVERLAY, 0, true);
    mOrigin += Vector3<float>{0.0, BS, 0.0};
	mPosition[1]++;
	if (mData->mSmoothLighting) 
    {
		GetSmoothLightFrame();
	} 
    else 
    {
		MapNode nodeTop = mData->mVManip.GetNodeNoEx(mBlockPosNodes + mPosition);
		mLight = LightPair(GetInteriorLight(nodeTop, 1, mNodeMgr));
	}
	DrawPlantlike();
	mPosition[1]--;
}

void MapblockMeshGenerator::DrawFirelikeQuad(float rotation, 
    float openingAngle, float offsetH, float offsetVertical)
{
	Vector3<float> vertices[4] = {
        Vector3<float>{-mScale, -BS / 2 + mScale * 2, 0},
        Vector3<float>{ mScale, -BS / 2 + mScale * 2, 0},
        Vector3<float>{ mScale, -BS / 2, 0},
        Vector3<float>{-mScale, -BS / 2, 0}
    };

    Quaternion<float> tgt;
	for (Vector3<float> &vertex : vertices) 
    {
        tgt = Rotation<3, float>(AxisAngle<3, float>(
			-Vector3<float>::Unit(AXIS_X), -openingAngle * (float)GE_C_DEG_TO_RAD));
        vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
		//vertex.rotateYZBy(openingAngle);
		vertex[2] += offsetH;

        tgt = Rotation<3, float>(AxisAngle<3, float>(
			-Vector3<float>::Unit(AXIS_Y), rotation * (float)GE_C_DEG_TO_RAD));
        vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
		//vertex.rotateXZBy(rotation);
		vertex[1] += offsetVertical;
	}
	DrawQuad(vertices);
}

void MapblockMeshGenerator::DrawFirelikeNode()
{
	UseTile();
	mScale = BS / 2 * mFeatures->visualScale;

	// Check for adjacent nodes
	bool neighbors = false;
	bool neighbor[6] = {0, 0, 0, 0, 0, 0};
	uint16_t current = mNode.GetContent();
	for (int i = 0; i < 6; i++) 
    {
		Vector3<short> node2Pos = mBlockPosNodes + mPosition + Face6D[i];
		MapNode node2 = mData->mVManip.GetNodeNoEx(node2Pos);
		uint16_t node2Content = node2.GetContent();
		if (node2Content != CONTENT_IGNORE && node2Content != CONTENT_AIR && node2Content != current) 
        {
			neighbor[i] = true;
			neighbors = true;
		}
	}
	bool drawBasicFire = neighbor[D6D_YN] || !neighbors;
	bool drawBottomFire = neighbor[D6D_YP];

	if (drawBasicFire || neighbor[D6D_ZP])
		DrawFirelikeQuad(0, -10, 0.4f * BS);
	else if (drawBottomFire)
		DrawFirelikeQuad(0, 70, 0.47f * BS, 0.484f * BS);

	if (drawBasicFire || neighbor[D6D_XN])
		DrawFirelikeQuad(90, -10, 0.4f * BS);
	else if (drawBottomFire)
		DrawFirelikeQuad(90, 70, 0.47f * BS, 0.484f * BS);

	if (drawBasicFire || neighbor[D6D_ZN])
		DrawFirelikeQuad(180, -10, 0.4f * BS);
	else if (drawBottomFire)
		DrawFirelikeQuad(180, 70, 0.47f * BS, 0.484f * BS);

	if (drawBasicFire || neighbor[D6D_XP])
		DrawFirelikeQuad(270, -10, 0.4f * BS);
	else if (drawBottomFire)
		DrawFirelikeQuad(270, 70, 0.47f * BS, 0.484f * BS);

	if (drawBasicFire) 
    {
		DrawFirelikeQuad(45, 0, 0.f);
		DrawFirelikeQuad(-45, 0, 0.f);
	}
}

void MapblockMeshGenerator::DrawFencelikeNode()
{
	UseTile(0, 0, 0);
	TileSpec tileNoCrack = mTile;

	for (auto& layer : tileNoCrack.layers)
		layer.materialFlags &= ~MATERIAL_FLAG_CRACK;

	// Put wood the right way around in the posts
	TileSpec tileRot = mTile;
	tileRot.rotation = 1;

	static const float postRad = BS / 8;
	static const float barRad  = BS / 16;
	static const float barLen  = BS / 2 - postRad;

	// The post - always present
	static const BoundingBox<float> post(
        -postRad, -BS / 2, -postRad, postRad,  BS / 2,  postRad);
	static const float postuv[24] = {
		0.375, 0.375, 0.625, 0.625,
		0.375, 0.375, 0.625, 0.625,
		0.000, 0.000, 0.250, 1.000,
		0.250, 0.000, 0.500, 1.000,
		0.500, 0.000, 0.750, 1.000,
		0.750, 0.000, 1.000, 1.000,
	};
	mTile = tileRot;
	DrawAutoLightedCuboid(post, postuv);

	mTile = tileNoCrack;

	// Now a section of fence, +X, if there's a post there
	Vector3<short> position2 = mPosition;
	position2[0]++;
	MapNode node2 = mData->mVManip.GetNodeNoEx(mBlockPosNodes + position2);
	const ContentFeatures* features2 = &mNodeMgr->Get(node2);
	if (features2->drawType == NDT_FENCELIKE)
    {
		static const BoundingBox<float> barX1(
            BS / 2 - barLen,  BS / 4 - barRad, -barRad,
            BS / 2 + barLen,  BS / 4 + barRad,  barRad);
		static const BoundingBox<float> barX2(
            BS / 2 - barLen, -BS / 4 - barRad, -barRad,
            BS / 2 + barLen, -BS / 4 + barRad,  barRad);
		static const float xrailuv[24] = {
			0.000, 0.125, 1.000, 0.250,
			0.000, 0.250, 1.000, 0.375,
			0.375, 0.375, 0.500, 0.500,
			0.625, 0.625, 0.750, 0.750,
			0.000, 0.500, 1.000, 0.625,
			0.000, 0.875, 1.000, 1.000,
		};
		DrawAutoLightedCuboid(barX1, xrailuv);
		DrawAutoLightedCuboid(barX2, xrailuv);
	}

	// Now a section of fence, +Z, if there's a post there
	position2 = mPosition;
	position2[2]++;
	node2 = mData->mVManip.GetNodeNoEx(mBlockPosNodes + position2);
	features2 = &mNodeMgr->Get(node2);
	if (features2->drawType == NDT_FENCELIKE)
    {
		static const BoundingBox<float> barZ1(
            -barRad,  BS / 4 - barRad, BS / 2 - barLen,
            barRad,  BS / 4 + barRad, BS / 2 + barLen);
		static const BoundingBox<float> barZ2(
            -barRad, -BS / 4 - barRad, BS / 2 - barLen,
            barRad, -BS / 4 + barRad, BS / 2 + barLen);
		static const float zrailuv[24] = {
			0.1875, 0.0625, 0.3125, 0.3125, // cannot rotate; stretch
			0.2500, 0.0625, 0.3750, 0.3125, // for wood texture instead
			0.0000, 0.5625, 1.0000, 0.6875,
			0.0000, 0.3750, 1.0000, 0.5000,
			0.3750, 0.3750, 0.5000, 0.5000,
			0.6250, 0.6250, 0.7500, 0.7500,
		};
		DrawAutoLightedCuboid(barZ1, zrailuv);
		DrawAutoLightedCuboid(barZ2, zrailuv);
	}
}

bool MapblockMeshGenerator::IsSameRail(Vector3<short> dir)
{
	MapNode node2 = mData->mVManip.GetNodeNoEx(mBlockPosNodes + mPosition + dir);
	if (node2.GetContent() == mNode.GetContent())
		return true;
	const ContentFeatures& cFeatures2 = mNodeMgr->Get(node2);
	return ((cFeatures2.drawType == NDT_RAILLIKE) &&
		(cFeatures2.GetGroup(mRaillikeGroupName) == mRaillikeGroup));
}

static const Vector3<short> RailDirection[4] = {
    Vector3<short>{ 0, 0,  1},
    Vector3<short>{ 0, 0, -1},
    Vector3<short>{-1, 0,  0},
    Vector3<short>{ 1, 0,  0}};
static const int RailSlopeAngle[4] = {0, 180, 90, -90};

enum RailTile 
{
	straight,
	curved,
	junction,
	cross,
};
struct RailDesc 
{
	int tileIndex;
	int angle;
};
static const RailDesc RailKinds[16] = {
		            // +x -x -z +z
		            //-------------
	{straight,   0}, //  .  .  .  .
	{straight,   0}, //  .  .  . +Z
	{straight,   0}, //  .  . -Z  .
	{straight,   0}, //  .  . -Z +Z
	{straight,  90}, //  . -X  .  .
	{  curved, 180}, //  . -X  . +Z
	{  curved, 270}, //  . -X -Z  .
	{junction, 180}, //  . -X -Z +Z
	{straight,  90}, // +X  .  .  .
	{  curved,  90}, // +X  .  . +Z
	{  curved,   0}, // +X  . -Z  .
	{junction,   0}, // +X  . -Z +Z
	{straight,  90}, // +X -X  .  .
	{junction,  90}, // +X -X  . +Z
	{junction, 270}, // +X -X -Z  .
	{   cross,   0}, // +X -X -Z +Z
};

void MapblockMeshGenerator::DrawRaillikeNode()
{
	mRaillikeGroup = mNodeMgr->Get(mNode).GetGroup(mRaillikeGroupName);

	int code = 0;
	int angle;
	int tileIndex;
	bool sloped = false;
	for (int dir = 0; dir < 4; dir++) 
    {
        bool railAbove = IsSameRail(RailDirection[dir] + Vector3<short>{0, 1, 0});
		if (railAbove)
        {
			sloped = true;
			angle = RailSlopeAngle[dir];
		}
        if (railAbove ||
            IsSameRail(RailDirection[dir]) ||
            IsSameRail(RailDirection[dir] + Vector3<short>{0, -1, 0}))
        {
            code |= 1 << dir;
        }
	}

	if (sloped) 
    {
		tileIndex = straight;
	} 
    else 
    {
		tileIndex = RailKinds[code].tileIndex;
		angle = RailKinds[code].angle;
	}

	UseTile(tileIndex, MATERIAL_FLAG_CRACK_OVERLAY, MATERIAL_FLAG_BACKFACE_CULLING);

	static const float offset = BS / 64;
	static const float size   = BS / 2;
	float y2 = sloped ? size : -size;
	Vector3<float> vertices[4] = {
        Vector3<float>{-size,    y2 + offset,  size},
        Vector3<float>{ size,    y2 + offset,  size},
        Vector3<float>{ size, -size + offset, -size},
        Vector3<float>{-size, -size + offset, -size}};
    if (angle)
    {
        Quaternion<float> tgt;
        for (Vector3<float>& vertex : vertices)
        {
            tgt = Rotation<3, float>(AxisAngle<3, float>(
				-Vector3<float>::Unit(AXIS_Y), angle * (float)GE_C_DEG_TO_RAD));
            vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
            //vertex.rotateXZBy(angle);
        }
    }

	DrawQuad(vertices);
}

static const Vector3<short> NodeboxTileDirs[6] = {
    Vector3<short>{0, 1, 0},
    Vector3<short>{0, -1, 0},
    Vector3<short>{1, 0, 0},
    Vector3<short>{-1, 0, 0},
    Vector3<short>{0, 0, 1},
    Vector3<short>{0, 0, -1}
};

// we have this order for some reason...
static const Vector3<short> NodeboxConnectionDirs[6] = {
    Vector3<short>{ 0,  1,  0}, // top
    Vector3<short>{ 0, -1,  0}, // bottom
    Vector3<short>{ 0,  0, -1}, // front
    Vector3<short>{-1,  0,  0}, // left
    Vector3<short>{ 0,  0,  1}, // back
    Vector3<short>{ 1,  0,  0}, // right
};

void MapblockMeshGenerator::DrawNodeboxNode()
{
	TileSpec tiles[6];
	for (int face = 0; face < 6; face++) 
    {
		// Handles facedir rotation for textures
		GetTile(NodeboxTileDirs[face], &tiles[face]);
	}

	// locate possible neighboring nodes to connect to
	uint8_t neighborsSet = 0;
	if (mFeatures->nodeBox.type == NODEBOX_CONNECTED) 
    {
		for (int dir = 0; dir != 6; dir++) 
        {
			uint8_t flag = 1 << dir;
			Vector3<short> position2 = mBlockPosNodes + mPosition + NodeboxConnectionDirs[dir];
			MapNode node2 = mData->mVManip.GetNodeNoEx(position2);
			if (mNodeMgr->NodeboxConnects(mNode, node2, flag))
				neighborsSet |= flag;
		}
	}

	std::vector<BoundingBox<float>> boxes;
	mNode.GetNodeBoxes(mNodeMgr, &boxes, neighborsSet);
	for (auto& box : boxes)
		DrawAutoLightedCuboid(box, nullptr, tiles, 6);
}

void MapblockMeshGenerator::DrawMeshNode()
{
	uint8_t facedir = 0;
	std::shared_ptr<BaseMesh> mesh;
	int degrotate = 0;

	if (mFeatures->paramType2 == CPT2_FACEDIR ||
		mFeatures->paramType2 == CPT2_COLORED_FACEDIR) 
    {
		facedir = mNode.GetFaceDir(mNodeMgr);
	} 
    else if (mFeatures->paramType2 == CPT2_WALLMOUNTED ||
        mFeatures->paramType2 == CPT2_COLORED_WALLMOUNTED) 
    {
		// Convert wallmounted to 6dfacedir.
		// When cache enabled, it is already converted.
		facedir = mNode.GetWallMounted(mNodeMgr);
		if (!mEnableMeshCache)
			facedir = WallmountedToFacedir[facedir];
	} 
    else if (mFeatures->paramType2 == CPT2_DEGROTATE ||
			mFeatures->paramType2 == CPT2_COLORED_DEGROTATE) 
    {
		degrotate = mNode.GetDegRotate(mNodeMgr);
	}

	if (!mData->mSmoothLighting && mFeatures->visualMesh[facedir] && !degrotate) 
    {
		// use cached meshes
		mesh = mFeatures->visualMesh[facedir];
	} 
    else if (mFeatures->visualMesh[0]) 
    {
		// no cache, clone and rotate mesh
		mesh = CloneMesh(mFeatures->visualMesh[0]);
		if (facedir)
			RotateMeshBy6DFaceDir(mesh, facedir);
		else if (degrotate)
			RotateMeshXZBy(mesh, 1.5f * degrotate);
		RecalculateBoundingBox(mesh);
		RecalculateNormals(mesh, true, false);
	} 
    else return;

	unsigned int meshBufferCount = (unsigned int)mesh->GetMeshBufferCount();
	for (unsigned int j = 0; j < meshBufferCount; j++) 
    {
		UseTile(j);
		std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(j);
		Vertex* vertices = buf->GetVertice()->Get<Vertex>();
		int vertexCount = buf->GetVertice()->GetNumElements();
        uint16_t* indices = buf->GetIndice()->Get<uint16_t>();
        int indexCount = buf->GetIndice()->GetNumElements();

		if (mData->mSmoothLighting) 
        {
			// Mesh is always private here. So the lighting is applied to each
			// vertex right here.
			for (int k = 0; k < vertexCount; k++) 
            {
				Vertex& vertex = vertices[k];
				vertex.color = SColorF(BlendLightColor(vertex.position, vertex.normal)).ToArray();
				vertex.position += mOrigin;
			}
			mCollector->Append(mTile, vertices, vertexCount, indices, indexCount);
		} 
        else 
        {
			// Don't modify the mesh, it may not be private here.
			// Instead, let the collector process colors, etc.
			mCollector->Append(mTile, vertices, vertexCount, indices, indexCount, 
                mOrigin, mColor, mFeatures->lightSource);
		}
	}
}

// also called when the drawtype is known but should have been pre-converted
void MapblockMeshGenerator::ErrorUnknownDrawtype()
{
	LogInformation("Got drawtype " + mFeatures->drawType);
	LogError("Unknown drawtype");
}

void MapblockMeshGenerator::DrawNode()
{
	// skip some drawtypes early
	switch (mFeatures->drawType) 
    {
		case NDT_NORMAL:   // Drawn by MapBlockMesh
		case NDT_AIRLIKE:  // Not drawn at all
		case NDT_LIQUID:   // Drawn by MapBlockMesh
			return;
		default:
			break;
	}
    mOrigin = Vector3<float>{ (float)mPosition[0], (float)mPosition[1], (float)mPosition[2] } * BS;
	if (mData->mSmoothLighting)
		GetSmoothLightFrame();
	else
		mLight = LightPair(GetInteriorLight(mNode, 1, mNodeMgr));
	switch (mFeatures->drawType) 
    {
		case NDT_FLOWINGLIQUID:     DrawLiquidNode(); break;
		case NDT_GLASSLIKE:         DrawGlasslikeNode(); break;
		case NDT_GLASSLIKE_FRAMED:  DrawGlasslikeFramedNode(); break;
		case NDT_ALLFACES:          DrawAllfacesNode(); break;
		case NDT_TORCHLIKE:         DrawTorchlikeNode(); break;
		case NDT_SIGNLIKE:          DrawSignlikeNode(); break;
		case NDT_PLANTLIKE:         DrawPlantlikeNode(); break;
		case NDT_PLANTLIKE_ROOTED:  DrawPlantlikeRootedNode(); break;
		case NDT_FIRELIKE:          DrawFirelikeNode(); break;
		case NDT_FENCELIKE:         DrawFencelikeNode(); break;
		case NDT_RAILLIKE:          DrawRaillikeNode(); break;
		case NDT_NODEBOX:           DrawNodeboxNode(); break;
		case NDT_MESH:              DrawMeshNode(); break;
		default:                    ErrorUnknownDrawtype(); break;
	}
}

/*
	TODO: Fix alpha blending for special nodes
	Currently only the last element rendered is blended correct
*/
void MapblockMeshGenerator::Generate()
{
    for (mPosition[2] = 0; mPosition[2] < MAP_BLOCKSIZE; mPosition[2]++)
    {
        for (mPosition[1] = 0; mPosition[1] < MAP_BLOCKSIZE; mPosition[1]++)
        {
            for (mPosition[0] = 0; mPosition[0] < MAP_BLOCKSIZE; mPosition[0]++)
            {
                mNode = mData->mVManip.GetNodeNoEx(mBlockPosNodes + mPosition);
                mFeatures = &mNodeMgr->Get(mNode);
                DrawNode();
            }
        }
    }
}

void MapblockMeshGenerator::RenderSingle(uint16_t node, uint8_t param2)
{
	mPosition = {0, 0, 0};
	mNode = MapNode(node, 0xff, param2);
	mFeatures = &mNodeMgr->Get(mNode);
	DrawNode();
}
