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

#ifndef MAPBLOCKMESH_H
#define MAPBLOCKMESH_H

#include "GameEngineStd.h"

#include "../../Games/Map/Voxel.h"

#include "../Shader.h"

#include "Core/OS/OS.h"

class BaseMesh;
class MapBlock;
class VisualEnvironment;

struct MinimapMapblock;

/*
    Mesh making stuff
*/

struct MeshMakeData
{
	VoxelManipulator mVManip;
    Vector3<short> mBlockPos = Vector3<short>{ -1337,-1337,-1337 };
    Vector3<short> mCrackPosRelative = Vector3<short>{ -1337,-1337,-1337 };
	bool mSmoothLighting = false;
	bool mUseShaders;

    VisualEnvironment* mEnvironment;

	MeshMakeData(VisualEnvironment* vEnv, bool useShaders);

	/*
		Copy block data manually (to allow optimizations by the caller)
	*/
	void FillBlockDataBegin(const Vector3<short>& blockPos);
	void FillBlockData(const Vector3<short>& blockOffset, MapNode* data);

	/*
		Copy central data directly from block, and other data from
		parent of block.
	*/
	void Fill(MapBlock* block);

	/*
		Set the (node) position of a crack
	*/
	void SetCrack(int crackLevel, Vector3<short> crackPos);

	/*
		Enable or disable smooth lighting
	*/
	void SetSmoothLighting(bool smoothLighting);
};

/*
	Holds a mesh for a mapblock.

	Besides the SMesh*, this contains information used for animating
	the vertex positions, colors and texture coordinates of the mesh.
	For example:
	- cracks [implemented]
	- day/night transitions [implemented]
	- animated flowing liquids [not implemented]
	- animating vertex positions for e.g. axles [not implemented]
*/
class MapBlockMesh
{
public:
	// Builds the mesh given
	MapBlockMesh(MeshMakeData* data, Vector3<short> cameraOffset);
	~MapBlockMesh();

	// Main animation function, parameters:
	//   farAway: whether the block is far away from the camera (~50 nodes)
	//   time: the global animation time, 0 .. 60 (repeats every minute)
	//   dayNightRatio: 0 .. 1000
	//   crack: -1 .. CRACK_ANIMATION_LENGTH-1 (-1 for off)
	// Returns true if anything has been changed.
	bool Animate(bool farAway, float time, int crack, unsigned int dayNightRatio);

	std::shared_ptr<BaseMesh> GetMesh()
	{
		return mMesh[0];
	}

	std::shared_ptr<BaseMesh> GetMesh(uint8_t layer)
	{
		return mMesh[layer];
	}

	MinimapMapblock* MoveMinimapMapblock()
	{
		MinimapMapblock* p = mMinimapMapblock;
		mMinimapMapblock = NULL;
		return p;
	}

	bool IsAnimationForced() const
	{
		return mAnimationForceTimer == 0;
	}

	void DecreaseAnimationForceTimer()
	{
		if(mAnimationForceTimer > 0)
			mAnimationForceTimer--;
	}

private:

    PcgRandom mPcgRand;

	std::shared_ptr<BaseMesh> mMesh[MAX_TILE_LAYERS];
	MinimapMapblock* mMinimapMapblock;
	BaseTextureSource* mTextureSrc;
	BaseShaderSource* mShaderSrc;

	bool mEnableShaders;
	bool mEnableVBO;

	// Must animate() be called before rendering?
	bool mHasAnimation;
	int mAnimationForceTimer;

	// Animation info: cracks
	// Last crack value passed to animate()
	int mLastCrack;
	// Maps mesh and mesh buffer (i.e. material) indices to base texture names
	std::map<std::pair<uint8_t, unsigned int>, std::string> mCrackMaterials;

	// Animation info: texture animationi
	// Maps mesh and mesh buffer indices to TileSpecs
	// Keys are pairs of (mesh index, buffer index in the mesh)
    std::map<std::pair<uint8_t, unsigned int>, TileLayer> mAnimationTiles;
    std::map<std::pair<uint8_t, unsigned int>, int> mAnimationFrames; // last animation frame
    std::map<std::pair<uint8_t, unsigned int>, int> mAnimationFrameOffsets;

	// Animation info: day/night transitions
	// Last dayNightRatio value passed to animate()
	unsigned int mLastDayNightRatio;
	// For each mesh and mesh buffer, stores pre-baked colors
	// of sunlit vertices
	// Keys are pairs of (mesh index, buffer index in the mesh)
    std::map<std::pair<uint8_t, unsigned int>, std::map<unsigned int, SColor>> mDayNightDiffs;
};

/*!
 * Encodes light of a node.
 * The result is not the final color, but a
 * half-baked vertex color.
 * You have to multiply the resulting color
 * with the node's color.
 *
 * \param light the first 8 bits are day light,
 * the last 8 bits are night light
 * \param emissiveLight amount of light the surface emits,
 * from 0 to LIGHT_SUN.
 */
SColor EncodeLight(uint16_t light, uint8_t emissiveLight);

// Compute light at node
uint16_t GetInteriorLight(MapNode n, int increment, const NodeManager* nodeMgr);
uint16_t GetFaceLight(MapNode n, MapNode n2, const Vector3<short>& faceDir, const NodeManager* nodeMgr);
uint16_t GetSmoothLightSolid(const Vector3<short>& p, 
    const Vector3<short>& faceDir, const Vector3<short>& corner, MeshMakeData* data);
uint16_t GetSmoothLightTransparent(const Vector3<short>& p, const Vector3<short>& corner, MeshMakeData* data);

/*!
 * Returns the sunlight's color from the current
 * day-night ratio.
 */
void GetSunlightColor(SColorF* sunlight, unsigned int dayNightRatio);

/*!
 * Gives the final  SColor shown on screen.
 *
 * \param result output color
 * \param light first 8 bits are day light, second 8 bits are
 * night light
 */
void FinalColorBlend(SColor* result, uint16_t light, unsigned int dayNightRatio);

/*!
 * Gives the final  SColor shown on screen.
 *
 * \param result output color
 * \param data the half-baked vertex color
 * \param dayLight color of the sunlight
 */
void FinalColorBlend(SColor* result, const SColor& data, const SColorF& dayLight);

// Retrieves the TileSpec of a face of a node
// Adds MATERIAL_FLAG_CRACK if the node is cracked
// TileSpec should be passed as reference due to the underlying TileFrame and its vector
// TileFrame vector copy cost very much to visual
void GetNodeTileN(MapNode mn, const Vector3<short>& pos, 
    uint8_t tileIndex, MeshMakeData* data, TileSpec& tile);
void GetNodeTile(MapNode mn, const Vector3<short>& pos, 
    const Vector3<short>& dir, MeshMakeData* data, TileSpec& tile);

#endif