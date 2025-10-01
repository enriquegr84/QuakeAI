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

#include "MapBlockMesh.h"

#include "Minimap.h"
#include "ContentMapblock.h"

#include "../MeshUtil.h"
#include "../MeshCollector.h"

#include "../../Utils/Noise.h"

#include "../../Games/Map/MapBlock.h"
#include "../../Games/Map/Map.h"

#include "../../Games/Environment/VisualEnvironment.h"

#include "Core/Utility/Profiler.h"

#include "Graphic/Scene/Mesh/NormalMesh.h"

#include "Application/Settings.h"

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

/*
	MeshMakeData
*/

MeshMakeData::MeshMakeData(VisualEnvironment* vEnv, bool useShaders) 
    : mEnvironment(vEnv), mUseShaders(useShaders)
{

}

void MeshMakeData::FillBlockDataBegin(const Vector3<short>& blockPos)
{
	mBlockPos = blockPos;

	Vector3<short> mBlockPosNodes = mBlockPos * (short)MAP_BLOCKSIZE;

	mVManip.Clear();
    VoxelArea voxelArea(mBlockPosNodes - Vector3<short>{1, 1, 1} *(short)MAP_BLOCKSIZE,
        mBlockPosNodes + Vector3<short>{1, 1, 1} *(short)MAP_BLOCKSIZE * (short)2 - Vector3<short>{1, 1, 1});
	mVManip.AddArea(voxelArea);
}

void MeshMakeData::FillBlockData(const Vector3<short>& blockOffset, MapNode* data)
{
    Vector3<short> dataSize{ MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE };
    VoxelArea dataArea(Vector3<short>::Zero(), dataSize - Vector3<short>{1, 1, 1});

	Vector3<short> bp = mBlockPos + blockOffset;
	Vector3<short> mBlockPosNodes = bp * (short)MAP_BLOCKSIZE;
	mVManip.CopyFrom(data, dataArea, Vector3<short>::Zero(), mBlockPosNodes, dataSize);
}

void MeshMakeData::Fill(MapBlock* block)
{
	FillBlockDataBegin(block->GetPosition());

	FillBlockData(Vector3<short>::Zero(), block->GetData());

	// Get map for reading neighbor blocks
	Map* map = block->GetParent();
	for (const Vector3<short>& dir : Face26D) 
    {
		Vector3<short> bp = mBlockPos + dir;
		MapBlock* b = map->GetBlockNoCreateNoEx(bp);
		if(b)
			FillBlockData(dir, b->GetData());
	}
}

void MeshMakeData::SetCrack(int crackLevel, Vector3<short> crackPos)
{
	if (crackLevel >= 0)
		mCrackPosRelative = crackPos - mBlockPos * (short)MAP_BLOCKSIZE;
}

void MeshMakeData::SetSmoothLighting(bool smoothLighting)
{
	mSmoothLighting = smoothLighting;
}

/*
	Light and vertex color functions
*/

/*
	Calculate non-smooth lighting at interior of node.
	Single light bank.
*/
static uint8_t GetInteriorLight(enum LightBank bank, 
    MapNode node, int increment, const NodeManager* nodeMgr)
{
	uint8_t light = node.GetLight(bank, nodeMgr);
	if (light > 0)
		light = std::clamp(light + increment, 0, LIGHT_SUN);
	return DecodeLight(light);
}

/*
	Calculate non-smooth lighting at interior of node.
	Both light banks.
*/
uint16_t GetInteriorLight(MapNode n, int increment, const NodeManager* nodeMgr)
{
	uint16_t day = GetInteriorLight(LIGHTBANK_DAY, n, increment, nodeMgr);
	uint16_t night = GetInteriorLight(LIGHTBANK_NIGHT, n, increment, nodeMgr);
	return day | (night << 8);
}

/*
	Calculate non-smooth lighting at face of node.
	Single light bank.
*/
static uint8_t GetFaceLight(
    enum LightBank bank, MapNode node, MapNode node2,
	Vector3<short> faceDir, const NodeManager* nodeMgr)
{
	uint8_t light;
	uint8_t l1 = node.GetLight(bank, nodeMgr);
	uint8_t l2 = node2.GetLight(bank, nodeMgr);
	if(l1 > l2)
		light = l1;
	else
		light = l2;

	// Boost light level for light sources
	uint8_t lightSource = std::max(
        nodeMgr->Get(node).lightSource, nodeMgr->Get(node2).lightSource);
	if(lightSource > light)
		light = lightSource;

	return DecodeLight(light);
}

/*
	Calculate non-smooth lighting at face of node.
	Both light banks.
*/
uint16_t GetFaceLight(MapNode node, MapNode node2, 
    const Vector3<short>& faceDir, const NodeManager* nodeMgr)
{
	uint16_t day = GetFaceLight(LIGHTBANK_DAY, node, node2, faceDir, nodeMgr);
	uint16_t night = GetFaceLight(LIGHTBANK_NIGHT, node, node2, faceDir, nodeMgr);
	return day | (night << 8);
}

/*
	Calculate smooth lighting at the XYZ- corner of p.
	Both light banks
*/
static uint16_t GetSmoothLightCombined(const Vector3<short>& p,
	const std::array<Vector3<short>,8>& dirs, MeshMakeData* data)
{
	const NodeManager* nodeMgr = data->mEnvironment->GetNodeManager();

	uint16_t ambientOcclusion = 0;
	uint16_t lightCount = 0;
	uint8_t lightSourceMax = 0;
	uint16_t lightDay = 0;
	uint16_t lightNight = 0;
	bool directSunlight = false;

	auto AddNode = [&] (uint8_t i, bool obstructed = false) -> bool 
    {
		if (obstructed) 
        {
			ambientOcclusion++;
			return false;
		}
		MapNode node = data->mVManip.GetNodeNoExNoEmerge(p + dirs[i]);
		if (node.GetContent() == CONTENT_IGNORE)
			return true;
		const ContentFeatures& f = nodeMgr->Get(node);
		if (f.lightSource > lightSourceMax)
			lightSourceMax = f.lightSource;
		// Check f.solidness because fast-style leaves look better this way
		if (f.paramType == CPT_LIGHT && f.solidness != 2) 
        {
			uint8_t lightLevelDay = node.GetLightNoChecks(LIGHTBANK_DAY, &f);
			uint8_t lightLevelNight = node.GetLightNoChecks(LIGHTBANK_NIGHT, &f);
			if (lightLevelDay == LIGHT_SUN)
				directSunlight = true;
			lightDay += DecodeLight(lightLevelDay);
			lightNight += DecodeLight(lightLevelNight);
			lightCount++;
		} else ambientOcclusion++;

		return f.lightPropagates;
	};

	bool obstructed[4] = { true, true, true, true };
    AddNode(0);
	bool opaque1 = !AddNode(1);
	bool opaque2 = !AddNode(2);
	bool opaque3 = !AddNode(3);
	obstructed[0] = opaque1 && opaque2;
	obstructed[1] = opaque1 && opaque3;
	obstructed[2] = opaque2 && opaque3;
	for (uint8_t k = 0; k < 3; ++k)
		if (AddNode(k + 4, obstructed[k]))
			obstructed[3] = false;
	if (AddNode(7, obstructed[3])) 
    { 
        // wrap light around nodes
		ambientOcclusion -= 3;
		for (uint8_t k = 0; k < 3; ++k)
            AddNode(k + 4, !obstructed[k]);
	}

	if (lightCount == 0) 
    {
		lightDay = lightNight = 0;
	} 
    else 
    {
		lightDay /= lightCount;
		lightNight /= lightCount;
	}

	// boost direct sunlight, if any
	if (directSunlight)
		lightDay = 0xFF;

	// Boost brightness around light sources
	bool skipAmbientOcclusionDay = false;
	if (DecodeLight(lightSourceMax) >= lightDay) 
    {
		lightDay = DecodeLight(lightSourceMax);
		skipAmbientOcclusionDay = true;
	}

	bool skipAmbientOcclusionNight = false;
	if(DecodeLight(lightSourceMax) >= lightNight) 
    {
		lightNight = DecodeLight(lightSourceMax);
		skipAmbientOcclusionNight = true;
	}

	if (ambientOcclusion > 4) 
    {
		static thread_local const float aOGamma = std::clamp(
			Settings::Get()->GetFloat("ambient_occlusion_gamma"), 0.25f, 4.0f);

		// Table of gamma space multiply factors.
		static thread_local const float lightAmount[3] = {
			powf(0.75f, 1.f / aOGamma), powf(0.5f,  1.f / aOGamma), powf(0.25f, 1.f / aOGamma)};

		//calculate table index for gamma space multiplier
		ambientOcclusion -= 5;

        if (!skipAmbientOcclusionDay)
        {
            lightDay = std::clamp((int)round(
                lightDay * lightAmount[ambientOcclusion]), 0, 255);
        }
        if (!skipAmbientOcclusionNight)
        {
            lightNight = std::clamp((int)round(
                lightNight * lightAmount[ambientOcclusion]), 0, 255);
        }
	}

	return lightDay | (lightNight << 8);
}

/*
	Calculate smooth lighting at the given corner of p.
	Both light banks.
	Node at p is solid, and thus the lighting is face-dependent.
*/
uint16_t GetSmoothLightSolid(
    const Vector3<short>& p, const Vector3<short>& faceDir, 
    const Vector3<short>& corner, MeshMakeData* data)
{
	return GetSmoothLightTransparent(p + faceDir, corner - (short)2 * faceDir, data);
}

/*
	Calculate smooth lighting at the given corner of p.
	Both light banks.
	Node at p is not solid, and the lighting is not face-dependent.
*/
uint16_t GetSmoothLightTransparent(const Vector3<short>& p, 
    const Vector3<short>& corner, MeshMakeData* data)
{
	const std::array<Vector3<short>,8> dirs = 
    {{
		// Always shine light
        Vector3<short>{0,0,0},
        Vector3<short>{corner[0],0,0},
        Vector3<short>{0,corner[1],0},
        Vector3<short>{0,0,corner[2]},

		// Can be obstructed
        Vector3<short>{corner[0],corner[1],0},
        Vector3<short>{corner[0],0,corner[2]},
        Vector3<short>{0,corner[1],corner[2]},
        Vector3<short>{corner[0],corner[1],corner[2]}
	}};
	return GetSmoothLightCombined(p, dirs, data);
}

void GetSunlightColor(SColorF* sunlight, unsigned int dayNightRatio)
{
	float rg = dayNightRatio / 1000.0f - 0.04f;
	float b = (0.98f * dayNightRatio) / 1000.0f + 0.078f;
	sunlight->mRed = rg;
	sunlight->mGreen = rg;
	sunlight->mBlue = b;
}

void FinalColorBlend(SColor* result, uint16_t light, unsigned int dayNightRatio)
{
	SColorF dayLight;
	GetSunlightColor(&dayLight, dayNightRatio);
	FinalColorBlend(result, EncodeLight(light, 0), dayLight);
}

void FinalColorBlend(SColor* result, const SColor& data, const SColorF& dayLight)
{
	static const SColorF artificialColor(1.04f, 1.04f, 1.04f);

	SColorF c(data);
	float n = 1 - c.mAlpha;

	float r = c.mRed * (c.mAlpha * dayLight.mRed + n * artificialColor.mRed) * 2.0f;
	float g = c.mGreen * (c.mAlpha * dayLight.mGreen + n * artificialColor.mGreen) * 2.0f;
	float b = c.mBlue * (c.mAlpha * dayLight.mBlue + n * artificialColor.mBlue) * 2.0f;

	// Emphase blue a bit in darker places
	// Each entry of this array represents a range of 8 blue levels
	static const uint8_t emphaseBlueWhenDark[32] = {
		1, 4, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	b += emphaseBlueWhenDark[std::clamp((int) ((r + g + b) / 3 * 255), 0, 255) / 8] / 255.0f;

	result->SetRed(std::clamp((int) (r * 255.0f), 0, 255));
	result->SetGreen(std::clamp((int) (g * 255.0f), 0, 255));
	result->SetBlue(std::clamp((int) (b * 255.0f), 0, 255));
}

/*
	Mesh generation helpers
*/

// This table is moved outside GetNodeVertexDirs to avoid the compiler using
// a mutex to initialize this table at runtime right in the hot path.
// For details search the internet for "cxa_guard_acquire".
static const Vector3<short> VertexDirsTable[] = {
	// ( 1, 0, 0)
    Vector3<short>{1,-1, 1}, Vector3<short>{1,-1,-1},
    Vector3<short>{1, 1,-1}, Vector3<short>{1, 1, 1},
	// ( 0, 1, 0)
    Vector3<short>{1, 1,-1}, Vector3<short>{-1, 1,-1},
    Vector3<short>{-1, 1, 1}, Vector3<short>{1, 1, 1},
	// ( 0, 0, 1)
    Vector3<short>{-1,-1, 1}, Vector3<short>{1,-1, 1},
    Vector3<short>{1, 1, 1}, Vector3<short>{-1, 1, 1},
	// invalid
	Vector3<short>(), Vector3<short>(), Vector3<short>(), Vector3<short>(),
	// ( 0, 0,-1)
    Vector3<short>{1,-1,-1}, Vector3<short>{-1,-1,-1},
    Vector3<short>{-1, 1,-1}, Vector3<short>{1, 1,-1},
	// ( 0,-1, 0)
    Vector3<short>{1,-1, 1}, Vector3<short>{-1,-1, 1},
    Vector3<short>{-1,-1,-1}, Vector3<short>{1,-1,-1},
	// (-1, 0, 0)
    Vector3<short>{-1,-1,-1}, Vector3<short>{-1,-1, 1},
    Vector3<short>{-1, 1, 1}, Vector3<short>{-1, 1,-1}
};

/*
	vertexDirs: Vector3<short>[4]
*/
static void GetNodeVertexDirs(const Vector3<short>& dir, Vector3<short>* vertexDirs)
{
	/*
		If looked from outside the node towards the face, the corners are:
		0: bottom-right
		1: bottom-left
		2: top-left
		3: top-right
	*/

	// Direction must be (1,0,0), (-1,0,0), (0,1,0), (0,-1,0),
	// (0,0,1), (0,0,-1)
	LogAssert(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2] == 1, "invalid direction");

	// Convert direction to single integer for table lookup
	uint8_t idx = (dir[0] + 2 * dir[1] + 3 * dir[2]) & 7;
	idx = (idx - 1) * 4;
	memcpy(vertexDirs, &VertexDirsTable[idx], 4 * sizeof(Vector3<short>));
}

static void GetNodeTextureCoords(Vector3<float> base, 
    const Vector3<float>& scale, const Vector3<short>& dir, float* u, float* v)
{
	if (dir[0] > 0 || dir[1] != 0 || dir[2] < 0)
		base -= scale;
    if (dir == Vector3<short>{0, 0, 1})
    {
		*u = -base[0] - 1;
		*v = -base[1] - 1;
    }
    else if (dir == Vector3<short>{0, 0, -1}) 
    {
		*u = base[0] + 1;
		*v = -base[1] - 2;
    }
    else if (dir == Vector3<short>{1, 0, 0}) 
    {
		*u = base[2] + 1;
		*v = -base[1] - 2;
    }
    else if (dir == Vector3<short>{-1, 0, 0}) 
    {
		*u = -base[2] - 1;
		*v = -base[1] - 1;
    }
    else if (dir == Vector3<short>{0, 1, 0}) 
    {
		*u = base[0] + 1;
		*v = -base[2] - 2;
    }
    else if (dir == Vector3<short>{0, -1, 0}) 
    {
		*u = base[0] + 1;
		*v = base[2] + 1;
	}
}

struct FastFace
{
	TileSpec tile;
    Vertex vertices[4]; // Precalculated vertices
	/*!
	 * The face is divided into two triangles. If this is true,
	 * vertices 0 and 2 are connected, othervise vertices 1 and 3
	 * are connected.
	 */
	bool vertex02Connected;
};

static void MakeFastFace(
    const TileSpec& tile, uint16_t li0, uint16_t li1, uint16_t li2, uint16_t li3,
    const Vector3<float>& tp, const Vector3<float>& p, const Vector3<short>& dir,
    const Vector3<float>& scale, std::vector<FastFace>& dest)
{
    // Position is at the center of the cube.
    Vector3<float> pos = p * BS;

    float x0 = 0.0f;
    float y0 = 0.0f;
    float w = 1.0f;
    float h = 1.0f;

    Vector3<float> vertexPos[4];
    Vector3<short> vertexDirs[4];
    GetNodeVertexDirs(dir, vertexDirs);
    if (tile.worldAligned)
        GetNodeTextureCoords(tp, scale, dir, &x0, &y0);

    Vector3<short> t;
    uint16_t t1;
    switch (tile.rotation)
    {
        case 0:
            break;
        case 1: //R90
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[3];
            vertexDirs[3] = vertexDirs[2];
            vertexDirs[2] = vertexDirs[1];
            vertexDirs[1] = t;
            t1 = li0;
            li0 = li3;
            li3 = li2;
            li2 = li1;
            li1 = t1;
            break;
        case 2: //R180
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[2];
            vertexDirs[2] = t;
            t = vertexDirs[1];
            vertexDirs[1] = vertexDirs[3];
            vertexDirs[3] = t;
            t1 = li0;
            li0 = li2;
            li2 = t1;
            t1 = li1;
            li1 = li3;
            li3 = t1;
            break;
        case 3: //R270
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[1];
            vertexDirs[1] = vertexDirs[2];
            vertexDirs[2] = vertexDirs[3];
            vertexDirs[3] = t;
            t1 = li0;
            li0 = li1;
            li1 = li2;
            li2 = li3;
            li3 = t1;
            break;
        case 4: //FXR90
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[3];
            vertexDirs[3] = vertexDirs[2];
            vertexDirs[2] = vertexDirs[1];
            vertexDirs[1] = t;
            t1 = li0;
            li0 = li3;
            li3 = li2;
            li2 = li1;
            li1 = t1;
            y0 += h;
            h *= -1;
            break;
        case 5: //FXR270
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[1];
            vertexDirs[1] = vertexDirs[2];
            vertexDirs[2] = vertexDirs[3];
            vertexDirs[3] = t;
            t1 = li0;
            li0 = li1;
            li1 = li2;
            li2 = li3;
            li3 = t1;
            y0 += h;
            h *= -1;
            break;
        case 6: //FYR90
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[3];
            vertexDirs[3] = vertexDirs[2];
            vertexDirs[2] = vertexDirs[1];
            vertexDirs[1] = t;
            t1 = li0;
            li0 = li3;
            li3 = li2;
            li2 = li1;
            li1 = t1;
            x0 += w;
            w *= -1;
            break;
        case 7: //FYR270
            t = vertexDirs[0];
            vertexDirs[0] = vertexDirs[1];
            vertexDirs[1] = vertexDirs[2];
            vertexDirs[2] = vertexDirs[3];
            vertexDirs[3] = t;
            t1 = li0;
            li0 = li1;
            li1 = li2;
            li2 = li3;
            li3 = t1;
            x0 += w;
            w *= -1;
            break;
        case 8: //FX
            y0 += h;
            h *= -1;
            break;
        case 9: //FY
            x0 += w;
            w *= -1;
            break;
        default:
            break;
    }

    for (uint16_t i = 0; i < 4; i++)
    {
        vertexPos[i] = Vector3<float>{
            BS / 2 * vertexDirs[i][0],
            BS / 2 * vertexDirs[i][1],
            BS / 2 * vertexDirs[i][2]
        };
    }

    for (Vector3<float>& vPos : vertexPos)
    {
        vPos[0] *= scale[0];
        vPos[1] *= scale[1];
        vPos[2] *= scale[2];
        vPos += pos;
    }

    float absScale = 1.0f;
    if (scale[0] < 0.999f || scale[0] > 1.001f) absScale = scale[0];
    else if (scale[1] < 0.999f || scale[1] > 1.001f) absScale = scale[1];
    else if (scale[2] < 0.999f || scale[2] > 1.001f) absScale = scale[2];

    Vector3<float> normal{ (float)dir[0], (float)dir[1], (float)dir[2] };

	uint16_t li[4] = { li0, li1, li2, li3 };
	uint16_t day[4];
	uint16_t night[4];

	for (uint8_t i = 0; i < 4; i++) 
    {
		day[i] = li[i] >> 8;
		night[i] = li[i] & 0xFF;
	}

	bool vertex02Connected = abs(day[0] - day[2]) + abs(night[0] - night[2])
        < abs(day[1] - day[3]) + abs(night[1] - night[3]);

	Vector2<float> f[4] = {
        Vector2<float>{x0 + w * absScale, y0 + h},
        Vector2<float>{x0, y0 + h},
        Vector2<float>{x0, y0},
        Vector2<float>{x0 + w * absScale, y0} };

	// equivalent to dest.push_back(FastFace()) but faster
	dest.emplace_back();
	FastFace& face = *dest.rbegin();

	for (uint8_t i = 0; i < 4; i++) 
    {
		SColor color = EncodeLight(li[i], tile.emissiveLight);
		if (!tile.emissiveLight)
			ApplyFacesShading(color, normal);

        face.vertices[i].position = vertexPos[i];
        face.vertices[i].normal = normal;
        face.vertices[i].color = SColorF(color).ToArray();
        face.vertices[i].tcoord = f[i];
	}

	/*
		Revert triangles for nicer looking gradient if the
		brightness of vertices 1 and 3 differ less than
		the brightness of vertices 0 and 2.
		*/
	face.vertex02Connected = vertex02Connected;
	face.tile = tile;
}

/*
	Nodes make a face if contents differ and solidness differs.
	Return value:
		0: No face
		1: Face uses m1's content
		2: Face uses m2's content
	equivalent: Whether the blocks share the same face (eg. water and glass)

	TODO: Add 3: Both faces drawn with backface culling, remove equivalent
*/
static uint8_t FaceContents(uint16_t m1, uint16_t m2, bool* equivalent, const NodeManager* nodeMgr)
{
	*equivalent = false;

	if (m1 == m2 || m1 == CONTENT_IGNORE || m2 == CONTENT_IGNORE)
		return 0;

	const ContentFeatures& f1 = nodeMgr->Get(m1);
	const ContentFeatures& f2 = nodeMgr->Get(m2);

	// Contents don't differ for different forms of same liquid
	if (f1.SameLiquid(f2))
		return 0;

	uint8_t c1 = f1.solidness;
	uint8_t c2 = f2.solidness;

	if (c1 == c2)
		return 0;

	if (c1 == 0)
		c1 = f1.visualSolidness;
	else if (c2 == 0)
		c2 = f2.visualSolidness;

	if (c1 == c2) 
	{
		*equivalent = true;
		// If same solidness, liquid takes precense
		if (f1.IsLiquid())
			return 1;
		if (f2.IsLiquid())
			return 2;
	}

	if (c1 > c2)
		return 1;

	return 2;
}

/*
	Gets nth node tile (0 <= n <= 5).
*/
void GetNodeTileN(MapNode mn, const Vector3<short>& pos, 
    uint8_t tileIndex, MeshMakeData* data, TileSpec& tile)
{
    const NodeManager* nodeMgr = data->mEnvironment->GetNodeManager();

	const ContentFeatures& cFeatures = nodeMgr->Get(mn);
	tile = cFeatures.tiles[tileIndex];
	bool hasCrack = pos == data->mCrackPosRelative;
	for (TileLayer& layer : tile.layers) 
    {
		if (layer.textureId == 0)
			continue;
		if (!layer.hasColor)
			mn.GetColor(cFeatures, &(layer.color));
		// Apply temporary crack
		if (hasCrack)
			layer.materialFlags |= MATERIAL_FLAG_CRACK;
	}
}

/*
	Gets node tile given a face direction.
*/
void GetNodeTile(MapNode mn, const Vector3<short>& pos, 
    const Vector3<short>& dir, MeshMakeData* data, TileSpec& tile)
{
	const NodeManager* nodeMgr = data->mEnvironment->GetNodeManager();

	// Direction must be (1,0,0), (-1,0,0), (0,1,0), (0,-1,0),
	// (0,0,1), (0,0,-1) or (0,0,0)
	LogAssert(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2] <= 1, "invalid direction");

	// Convert direction to single integer for table lookup
	//  0 = (0,0,0)
	//  1 = (1,0,0)
	//  2 = (0,1,0)
	//  3 = (0,0,1)
	//  4 = invalid, treat as (0,0,0)
	//  5 = (0,0,-1)
	//  6 = (0,-1,0)
	//  7 = (-1,0,0)

	// Get rotation for things like chests
	uint8_t facedir = mn.GetFaceDir(nodeMgr, true);
	static const uint16_t dirToTile[24 * 16] =
	{
		// 0     +X    +Y    +Z       -Z    -Y    -X   ->   value=tile,rotation
		0,0,  2,0 , 0,0 , 4,0 ,  0,0,  5,0 , 1,0 , 3,0 ,  // rotate around y+ 0 - 3
		0,0,  4,0 , 0,3 , 3,0 ,  0,0,  2,0 , 1,1 , 5,0 ,
		0,0,  3,0 , 0,2 , 5,0 ,  0,0,  4,0 , 1,2 , 2,0 ,
		0,0,  5,0 , 0,1 , 2,0 ,  0,0,  3,0 , 1,3 , 4,0 ,

		0,0,  2,3 , 5,0 , 0,2 ,  0,0,  1,0 , 4,2 , 3,1 ,  // rotate around z+ 4 - 7
		0,0,  4,3 , 2,0 , 0,1 ,  0,0,  1,1 , 3,2 , 5,1 ,
		0,0,  3,3 , 4,0 , 0,0 ,  0,0,  1,2 , 5,2 , 2,1 ,
		0,0,  5,3 , 3,0 , 0,3 ,  0,0,  1,3 , 2,2 , 4,1 ,

		0,0,  2,1 , 4,2 , 1,2 ,  0,0,  0,0 , 5,0 , 3,3 ,  // rotate around z- 8 - 11
		0,0,  4,1 , 3,2 , 1,3 ,  0,0,  0,3 , 2,0 , 5,3 ,
		0,0,  3,1 , 5,2 , 1,0 ,  0,0,  0,2 , 4,0 , 2,3 ,
		0,0,  5,1 , 2,2 , 1,1 ,  0,0,  0,1 , 3,0 , 4,3 ,

		0,0,  0,3 , 3,3 , 4,1 ,  0,0,  5,3 , 2,3 , 1,3 ,  // rotate around x+ 12 - 15
		0,0,  0,2 , 5,3 , 3,1 ,  0,0,  2,3 , 4,3 , 1,0 ,
		0,0,  0,1 , 2,3 , 5,1 ,  0,0,  4,3 , 3,3 , 1,1 ,
		0,0,  0,0 , 4,3 , 2,1 ,  0,0,  3,3 , 5,3 , 1,2 ,

		0,0,  1,1 , 2,1 , 4,3 ,  0,0,  5,1 , 3,1 , 0,1 ,  // rotate around x- 16 - 19
		0,0,  1,2 , 4,1 , 3,3 ,  0,0,  2,1 , 5,1 , 0,0 ,
		0,0,  1,3 , 3,1 , 5,3 ,  0,0,  4,1 , 2,1 , 0,3 ,
		0,0,  1,0 , 5,1 , 2,3 ,  0,0,  3,1 , 4,1 , 0,2 ,

		0,0,  3,2 , 1,2 , 4,2 ,  0,0,  5,2 , 0,2 , 2,2 ,  // rotate around y- 20 - 23
		0,0,  5,2 , 1,3 , 3,2 ,  0,0,  2,2 , 0,1 , 4,2 ,
		0,0,  2,2 , 1,0 , 5,2 ,  0,0,  4,2 , 0,0 , 3,2 ,
		0,0,  4,2 , 1,1 , 2,2 ,  0,0,  3,2 , 0,3 , 5,2

	};
	uint16_t tileIndex = facedir * 16 + 
        ((dir[0] + 2 * dir[1] + 3 * dir[2]) & 7) * 2;
	GetNodeTileN(mn, pos, (uint8_t)dirToTile[tileIndex], data, tile);
	tile.rotation = tile.worldAligned ? 0 : dirToTile[tileIndex + 1];
}

static void GetTileInfo(
    // Input:
    MeshMakeData* data, const Vector3<short>& p, const Vector3<short>& faceDir,
    // Output:
    bool& makesFace, Vector3<short>& pCorrected, Vector3<short>& faceDirCorrected, 
    uint16_t* lights, uint8_t& waving, TileSpec& tile)
{
	VoxelManipulator& vmanip = data->mVManip;
	const NodeManager* nodeMgr = data->mEnvironment->GetNodeManager();
	Vector3<short> mBlockPosNodes = data->mBlockPos * (short)MAP_BLOCKSIZE;

	const MapNode& n0 = vmanip.GetNodeRefUnsafe(mBlockPosNodes + p);

	// Don't even try to get n1 if n0 is already CONTENT_IGNORE
	if (n0.GetContent() == CONTENT_IGNORE) 
    {
		makesFace = false;
		return;
	}

	const MapNode& n1 = vmanip.GetNodeRefUnsafeCheckFlags(mBlockPosNodes + p + faceDir);

	if (n1.GetContent() == CONTENT_IGNORE) 
    {
		makesFace = false;
		return;
	}

	// This is hackish
	bool equivalent = false;
	uint8_t mf = FaceContents(n0.GetContent(), n1.GetContent(), &equivalent, nodeMgr);

	if (mf == 0) 
    {
		makesFace = false;
		return;
	}

	makesFace = true;

	MapNode n = n0;
	if (mf == 1) 
    {
		pCorrected = p;
		faceDirCorrected = faceDir;
	} 
    else 
    {
		n = n1;
		pCorrected = p + faceDir;
		faceDirCorrected = -faceDir;
	}

	GetNodeTile(n, pCorrected, faceDirCorrected, data, tile);
	const ContentFeatures& cFeatures = nodeMgr->Get(n);
	waving = cFeatures.waving;
	tile.emissiveLight = cFeatures.lightSource;

	// eg. water and glass
	if (equivalent) 
		for (TileLayer &layer : tile.layers)
			layer.materialFlags |= MATERIAL_FLAG_BACKFACE_CULLING;

	if (!data->mSmoothLighting) 
    {
		lights[0] = lights[1] = lights[2] = lights[3] =
            GetFaceLight(n0, n1, faceDir, nodeMgr);
	} 
    else 
    {
		Vector3<short> vertexDirs[4];
		GetNodeVertexDirs(faceDirCorrected, vertexDirs);

		Vector3<short> light_p = mBlockPosNodes + pCorrected;
		for (uint16_t i = 0; i < 4; i++)
			lights[i] = GetSmoothLightSolid(light_p, faceDirCorrected, vertexDirs[i], data);
	}
}

/*
	startPos:
	translateDir: unit vector with only one of x, y or z
	faceDir: unit vector with only one of x, y or z
*/
static void UpdateFastFaceRow(MeshMakeData* data, const Vector3<short>&& startPos,
    Vector3<short> translateDir, const Vector3<float>&& translateDirFloat,
    const Vector3<short>&& faceDir, std::vector<FastFace>& dest)
{
	static thread_local const bool wavingLiquids =
		Settings::Get()->GetBool("enable_shaders") &&
		Settings::Get()->GetBool("enable_waving_water");

	Vector3<short> p = startPos;

	uint16_t continuousTilesCount = 1;

	bool makesFace = false;
	Vector3<short> pCorrected = Vector3<short>::Zero();
	Vector3<short> faceDirCorrected = Vector3<short>::Zero();
	uint16_t lights[4] = {0, 0, 0, 0};
	uint8_t waving = 0;
	TileSpec tile;

	// Get info of first tile
	GetTileInfo(data, p, faceDir, makesFace, pCorrected, faceDirCorrected, lights, waving, tile);

	// Unroll this variable which has a significant build cost
	TileSpec nextTile;
	for (uint16_t j = 0; j < MAP_BLOCKSIZE; j++) 
    {
		// If tiling can be done, this is set to false in the next step
		bool nextIsDifferent = true;

		bool nextMakesFace = false;
		Vector3<short> nextPCorrected = Vector3<short>::Zero();
		Vector3<short> nextFaceDirCorrected = Vector3<short>::Zero();
		uint16_t nextLights[4] = {0, 0, 0, 0};

		// If at last position, there is nothing to compare to and
		// the face must be drawn anyway
		if (j != MAP_BLOCKSIZE - 1) 
        {
			p += translateDir;

			GetTileInfo(data, p, faceDir,
					nextMakesFace, nextPCorrected,
					nextFaceDirCorrected, nextLights,
					waving,
					nextTile);

			if (nextMakesFace == makesFace && 
				nextPCorrected == pCorrected + translateDir && 
				nextFaceDirCorrected == faceDirCorrected && 
				memcmp(nextLights, lights, sizeof(lights)) == 0 &&
				// Don't apply fast faces to waving water.
				(waving != 3 || !wavingLiquids) &&
				nextTile.IsTileable(tile))
            {
				nextIsDifferent = false;
				continuousTilesCount++;
			}
		}
		if (nextIsDifferent) 
        {
			/*
				Create a face if there should be one
			*/
            if (makesFace)
            {
                // Floating point conversion of the position vector
                Vector3<float> pf{(float)pCorrected[0], (float)pCorrected[1], (float)pCorrected[2]};
				// Center point of face (kind of)
				Vector3<float> sp = 
                    pf - ((float)continuousTilesCount * 0.5f - 0.5f) * translateDirFloat;
                Vector3<float> scale{ 1, 1, 1 };

				if (translateDir[0] != 0)
					scale[0] = continuousTilesCount;
				if (translateDir[1] != 0)
					scale[1] = continuousTilesCount;
				if (translateDir[2] != 0)
					scale[2] = continuousTilesCount;

				MakeFastFace(tile, lights[0], lights[1], lights[2], lights[3],
						pf, sp, faceDirCorrected, scale, dest);
				Profiling->Avg("Meshgen: Tiles per face [#]", continuousTilesCount);
			}

			continuousTilesCount = 1;
		}

		makesFace = nextMakesFace;
		pCorrected = nextPCorrected;
		faceDirCorrected = nextFaceDirCorrected;
		memcpy(lights, nextLights, sizeof(lights));
		if (nextIsDifferent)
			tile = std::move(nextTile); // faster than copy
	}
}

static void UpdateAllFastFaceRows(MeshMakeData* data, std::vector<FastFace>& dest)
{
	/*
		Go through every y,z and get top(y+) faces in rows of x+
	*/
    for (int16_t y = 0; y < MAP_BLOCKSIZE; y++)
    {
        for (int16_t z = 0; z < MAP_BLOCKSIZE; z++)
        {
            UpdateFastFaceRow(data,
                Vector3<short>{0, y, z},
                Vector3<short>{1, 0, 0}, //dir
                Vector3<float>{1, 0, 0},
                Vector3<short>{0, 1, 0}, //face dir
                dest);
        }
    }

	/*
		Go through every x,y and get right(x+) faces in rows of z+
	*/
    for (int16_t x = 0; x < MAP_BLOCKSIZE; x++)
    {
        for (int16_t y = 0; y < MAP_BLOCKSIZE; y++)
        {
            UpdateFastFaceRow(data,
                Vector3<short>{x, y, 0},
                Vector3<short>{0, 0, 1}, //dir
                Vector3<float>{0, 0, 1},
                Vector3<short>{1, 0, 0}, //face dir
                dest);
        }
    }

	/*
		Go through every y,z and get back(z+) faces in rows of x+
	*/
    for (int16_t z = 0; z < MAP_BLOCKSIZE; z++)
    {
        for (int16_t y = 0; y < MAP_BLOCKSIZE; y++)
        {
            UpdateFastFaceRow(data,
                Vector3<short>{0, y, z},
                Vector3<short>{1, 0, 0}, //dir
                Vector3<float>{1, 0, 0},
                Vector3<short>{0, 0, 1}, //face dir
                dest);
        }
    }
}

static void ApplyTileColor(PreMeshBuffer& pmb)
{
	SColor tileColor = pmb.layer.color;
	if (tileColor == SColor(0xFFFFFFFF))
		return;
	for (Vertex& vertex : pmb.vertices) 
    {
		SColor color = SColorF(vertex.color[0], 
            vertex.color[1], vertex.color[2], vertex.color[3]).ToSColor();
        color.Set(color.GetAlpha(),
            color.GetRed() * tileColor.GetRed() / 255,
            color.GetGreen() * tileColor.GetGreen() / 255,
            color.GetBlue() * tileColor.GetBlue() / 255);
        vertex.color = SColorF(color).ToArray();
	}
}

/*
	MapBlockMesh
*/

MapBlockMesh::MapBlockMesh(MeshMakeData* data, Vector3<short> cameraOffset):
	mMinimapMapblock(NULL),
	mTextureSrc(data->mEnvironment->GetTextureSource()),
	mShaderSrc(data->mEnvironment->GetShaderSource()),
	mAnimationForceTimer(0), // force initial animation
	mLastCrack(-1),
	mLastDayNightRatio((unsigned int) -1)
{
	for (auto& mesh : mMesh)
        mesh = std::make_shared<NormalMesh>();
	mEnableShaders = data->mUseShaders;
	mEnableVBO = Settings::Get()->GetBool("enable_vbo");

	if (data->mEnvironment->GetMinimap())
    {
		mMinimapMapblock = new MinimapMapblock;
		mMinimapMapblock->GetMinimapNodes(
			&data->mVManip, data->mBlockPos * (short)MAP_BLOCKSIZE);
	}

	// 4-21ms for MAP_BLOCKSIZE=16  (NOTE: probably outdated)
	// 24-155ms for MAP_BLOCKSIZE=32  (NOTE: probably outdated)
	//TimeTaker timer1("MapBlockMesh()");

	std::vector<FastFace> fastFacesNew;
	fastFacesNew.reserve(512);

	/*
		We are including the faces of the trailing edges of the block.
		This means that when something changes, the caller must
		also update the meshes of the blocks at the leading edges.

		NOTE: This is the slowest part of this method.
	*/
	{
		// 4-23ms for MAP_BLOCKSIZE=16  (NOTE: probably outdated)
		//TimeTaker timer2("UpdateAllFastFaceRows()");
		UpdateAllFastFaceRows(data, fastFacesNew);
	}
	// End of slow part

	/*
		Convert FastFaces to MeshCollector
	*/

	MeshCollector collector;
	{
		// avg 0ms (100ms spikes when loading textures the first time)
		// (NOTE: probably outdated)
		//TimeTaker timer2("MeshCollector building");

		for (const FastFace& f : fastFacesNew) 
        {
			static const uint16_t indices[] = {0, 1, 2, 2, 3, 0};
			static const uint16_t indicesAlternate[] = {0, 1, 3, 2, 3, 1};
			const uint16_t* indicesP =
				f.vertex02Connected ? indices : indicesAlternate;
			collector.Append(f.tile, f.vertices, 4, indicesP, 6);
		}
	}

	/*
		Add special graphics:
		- torches
		- flowing water
		- fences
		- whatever
	*/

	{
		MapblockMeshGenerator generator(data, &collector);
		generator.Generate();
	}

	/*
		Convert MeshCollector to SMesh
	*/

	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) 
    {
		std::shared_ptr<BaseMesh> mesh = mMesh[layer];
		for(unsigned int i = 0; i < collector.prebuffers[layer].size(); i++)
		{
			PreMeshBuffer& p = collector.prebuffers[layer][i];

			ApplyTileColor(p);

			// Generate animation data
			// - Cracks
			if (p.layer.materialFlags & MATERIAL_FLAG_CRACK) 
            {
				// Find the texture name plus ^[crack:N:
				std::ostringstream os(std::ios::binary);
				os << mTextureSrc->GetTextureName(p.layer.textureId).c_str() << "^[crack";
				if (p.layer.materialFlags & MATERIAL_FLAG_CRACK_OVERLAY)
					os << "o";  // use ^[cracko
				uint8_t tiles = p.layer.scale;
				if (tiles > 1)
					os << ":" << (unsigned int)tiles;
				os << ":" << (unsigned int)p.layer.animationFrameCount << ":";
				mCrackMaterials.insert(std::make_pair(
                    std::pair<uint8_t, unsigned int>(layer, i), os.str()));
				// Replace tile texture with the cracked one
				p.layer.texture = mTextureSrc->GetTextureForMesh(
                    os.str() + "0", &p.layer.textureId);
			}
			// - Texture animation
			if (p.layer.materialFlags & MATERIAL_FLAG_ANIMATION) 
            {
				// Add to MapBlockMesh in order to animate these tiles
				mAnimationTiles[std::pair<uint8_t, unsigned int>(layer, i)] = p.layer;
				mAnimationFrames[std::pair<uint8_t, unsigned int>(layer, i)] = 0;
				if (Settings::Get()->GetBool("desynchronize_mapblock_texture_animation")) 
                {
					// Get starting position from noise
					mAnimationFrameOffsets[std::pair<uint8_t, unsigned int>(layer, i)] =
                        100000 * (unsigned int)(2.f + Noise3d(data->mBlockPos[0], data->mBlockPos[1], data->mBlockPos[2], 0));
				} 
                else 
                {
					// Play all synchronized
					mAnimationFrameOffsets[std::pair<uint8_t, unsigned int>(layer, i)] = 0;
				}
				// Replace tile texture with the first animation frame
				p.layer.texture = (*p.layer.frames)[0].texture;
			}

			if (!mEnableShaders) 
            {
				// Extract colors for day-night animation
				// Dummy sunlight to handle non-sunlit areas
				SColorF sunlight;
				GetSunlightColor(&sunlight, 0);
				unsigned int vertexCount = (unsigned int)p.vertices.size();
				for (unsigned int j = 0; j < vertexCount; j++) 
                {
					SColor vc = SColorF(
                        p.vertices[j].color[0], p.vertices[j].color[1], 
                        p.vertices[j].color[2], p.vertices[j].color[3]).ToSColor();
					SColor copy = vc;
					if (vc.GetAlpha() == 0) // No sunlight - no need to animate
						FinalColorBlend(&vc, copy, sunlight); // Finalize color
					else // Record color to animate
						mDayNightDiffs[std::pair<uint8_t, unsigned int>(layer, i)][j] = copy;

					// The sunlight ratio has been stored,
					// delete alpha (for the final rendering).
					vc.SetAlpha(255);
                    p.vertices[j].color = SColorF(vc).ToArray();
				}
			}

			// Create material
			std::shared_ptr<Material> material(new Material());
            material->mLighting = false;
            material->mCullMode = RasterizerState::CULL_BACK;
            material->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
			//material.SetFlag(MF_FOG_ENABLE, true);
			material->SetTexture(0, p.layer.texture);

			if (mEnableShaders) 
            {
				material->mType = mShaderSrc->GetShaderInfo(p.layer.shaderId).material;
				material->mTypeParam2 = p.layer.shaderId;
				p.layer.ApplyMaterialOptionsWithShaders(material);
				if (p.layer.normalTexture)
					material->SetTexture(1, p.layer.normalTexture);
				material->SetTexture(2, p.layer.flagsTexture);
			} 
            else 
            {
				p.layer.ApplyMaterialOptions(material);
			}

            VertexFormat vformat;
            vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
            vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
            vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
			vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);
			MeshBuffer* buf = new MeshBuffer(vformat, 
				(unsigned int)p.vertices.size(), (unsigned int)p.indices.size() / 3, sizeof(unsigned int));

			// fill vertices
			Vertex* vertex = buf->GetVertice()->Get<Vertex>();
			for (unsigned int i = 0; i < p.vertices.size(); i++)
			{
				vertex[i].position = p.vertices[i].position;
				vertex[i].tcoord = p.vertices[i].tcoord;
				vertex[i].color = p.vertices[i].color;
				vertex[i].normal = p.vertices[i].normal;
			}

			//fill indices
			unsigned int idx = 0;
			for (unsigned int i = 0; i < p.indices.size(); i += 3)
			{
				buf->GetIndice()->SetTriangle(idx++,
					p.indices[i], p.indices[i + 1], p.indices[i + 2]);
			}
			buf->GetMaterial() = material;

			mesh->AddMeshBuffer(buf);
		}
		RecalculateBoundingBox(mesh);
        /*
		if (mMesh[layer]) 
        {
			// Use VBO for mesh (this just would set this for ever buffer)
			if (mEnableVBO)
				mMesh[layer]->SetHardwareMappingHint(HM_STATIC);
		}
        */
	}

	//std::cout<<"added "<<fastfaces.getSize()<<" faces."<<std::endl;

	// Check if animation is required for this mesh
	mHasAnimation =
        !mCrackMaterials.empty() || !mDayNightDiffs.empty() || !mAnimationTiles.empty();
}

MapBlockMesh::~MapBlockMesh()
{
	for (std::shared_ptr<BaseMesh> mesh : mMesh) 
    {
		if (mEnableVBO) 
        {
			for (unsigned int i = 0; i < mesh->GetMeshBufferCount(); i++)
            {
				std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(i);
				//RenderingEngine::get_video_driver()->RemoveHardwareBuffer(buf);
			}
		}
	}
	delete mMinimapMapblock;
}

bool MapBlockMesh::Animate(bool farAway, float time, int crack, unsigned int dayNightRatio)
{
	if (!mHasAnimation) 
    {
		mAnimationForceTimer = 100000;
		return false;
	}
    mAnimationForceTimer = mPcgRand.Range(5, 100);

	// Cracks
	if (crack != mLastCrack) 
    {
		for (auto& crackMaterial : mCrackMaterials) 
        {
			std::shared_ptr<BaseMeshBuffer> buf = 
                mMesh[crackMaterial.first.first]->GetMeshBuffer(crackMaterial.first.second);
			std::string basename = crackMaterial.second;

			// Create new texture name from original
			std::ostringstream os;
			os << basename << crack;
			unsigned int newTextureId = 0;
			std::shared_ptr<Texture2> newTexture =
                mTextureSrc->GetTextureForMesh(os.str(), &newTextureId);
			buf->GetMaterial()->SetTexture(0, newTexture);

			// If the current material is also animated,
			// update animation info
			auto animIter = mAnimationTiles.find(crackMaterial.first);
			if (animIter != mAnimationTiles.end()) 
            {
				TileLayer& tile = animIter->second;
				tile.texture = newTexture;
				tile.textureId = newTextureId;
				// force animation update
				mAnimationFrames[crackMaterial.first] = -1;
			}
		}

		mLastCrack = crack;
	}

	// Texture animation
	for (auto& animationTile : mAnimationTiles) 
    {
		const TileLayer& tile = animationTile.second;
		// Figure out current frame
		int frameOffset = mAnimationFrameOffsets[animationTile.first];
		int frame = (int)(time * 1000 / tile.animationFrameLengthMs
            + frameOffset) % tile.animationFrameCount;
		// If frame doesn't change, skip
		if (frame == mAnimationFrames[animationTile.first])
			continue;

		mAnimationFrames[animationTile.first] = frame;

		std::shared_ptr<BaseMeshBuffer> buf = 
            mMesh[animationTile.first.first]->GetMeshBuffer(animationTile.first.second);

		const FrameSpec& animationFrame = (*tile.frames)[frame];
		buf->GetMaterial()->SetTexture(0, animationFrame.texture);
		if (mEnableShaders) 
        {
			if (animationFrame.normalTexture)
				buf->GetMaterial()->SetTexture(1, animationFrame.normalTexture);
			buf->GetMaterial()->SetTexture(3, animationFrame.flagsTexture);
		}
	}

	// Day-night transition
	if (!mEnableShaders && (dayNightRatio != mLastDayNightRatio)) 
    {
		// Force reload mesh to VBO
        if (mEnableVBO)
        {
            for (std::shared_ptr<BaseMesh> mesh : mMesh)
            {
                // set dirty flag to make sure that hardware copies of this buffer are also updated
                //mesh->SetDirty();
            }
        }

		SColorF dayColor;
		GetSunlightColor(&dayColor, dayNightRatio);

		for (auto& dayNightDiff : mDayNightDiffs) 
        {
			std::shared_ptr<BaseMeshBuffer> buf = 
                mMesh[dayNightDiff.first.first]->GetMeshBuffer(dayNightDiff.first.second);
			Vertex* vertices = (Vertex *)buf->GetVertice()->Get<Vertex>();
            for (const auto& j : dayNightDiff.second)
            {
                SColor color = SColorF(
                    vertices[j.first].color[0], vertices[j.first].color[1], 
                    vertices[j.first].color[2], vertices[j.first].color[3]).ToSColor();
                FinalColorBlend(&color, j.second, dayColor);
                vertices[j.first].color = SColorF(color).ToArray();
            }

		}
		mLastDayNightRatio = dayNightRatio;
	}

	return true;
}

SColor EncodeLight(uint16_t light, uint8_t emissiveLight)
{
	// Get components
	unsigned int day = (light & 0xff);
	unsigned int night = (light >> 8);
	// Add emissive light
	night += (unsigned int)(emissiveLight * 2.5f);
	if (night > 255)
		night = 255;
	// Since we don't know if the day light is sunlight or
	// artificial light, assume it is artificial when the night
	// light bank is also lit.
	if (day < night)
		day = 0;
	else
		day = day - night;
	unsigned int sum = day + night;
	// Ratio of sunlight:
	unsigned int r;
	if (sum > 0)
		r = day * 255 / sum;
	else
		r = 0;
	// Average light:
	unsigned int b = (day + night) / 2;
	return SColor(r, b, b, b);
}
