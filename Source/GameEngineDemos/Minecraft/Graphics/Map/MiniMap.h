/*
Minetest
Copyright (C) 2010-2015 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef MINIMAP_H
#define MINIMAP_H

#include "../Hud.h"

#include "../../Games/Map/Voxel.h"

#include "Core/Threading/Thread.h"

#include "Mathematic/Geometric/Rectangle.h"

#define MINIMAP_MAX_SX 512
#define MINIMAP_MAX_SY 512

class BaseUI;
class BaseShaderSource;
class BaseTextureSource;

enum MinimapShape 
{
	MINIMAP_SHAPE_SQUARE,
	MINIMAP_SHAPE_ROUND,
};

struct MinimapMode 
{
	MinimapType type;
	std::string label;
	uint16_t scanHeight;
	uint16_t mapSize;
	std::string texture;
	uint16_t scale;
};

struct MinimapMarker 
{
	MinimapMarker(Node* pNode) : parentNode(pNode)
	{

	}

    Node* parentNode;
};
struct MinimapPixel 
{
	//! The topmost node that the minimap displays.
	MapNode node;
	uint16_t height;
	uint16_t airCount;
};

struct MinimapMapblock 
{
	void GetMinimapNodes(VoxelManipulator* vmanip, const Vector3<short>& pos);

	MinimapPixel data[MAP_BLOCKSIZE * MAP_BLOCKSIZE];
};

struct MinimapData 
{
	MinimapMode mode;
	Vector3<short> position;
	Vector3<short> oldPosition;
	MinimapPixel minimapScan[MINIMAP_MAX_SX * MINIMAP_MAX_SY];
	bool mapInvalidated;
	bool minimapShapeRound;
	std::shared_ptr<Texture2> minimapMaskRound = nullptr;
    std::shared_ptr<Texture2> minimapMaskSquare = nullptr;
    std::shared_ptr<Texture2> texture = nullptr;
    std::shared_ptr<Texture2> heightmapTexture = nullptr;
    std::shared_ptr<Texture2> minimapOverlayRound = nullptr;
    std::shared_ptr<Texture2> minimapOverlaySquare = nullptr;
    std::shared_ptr<Texture2> playerMarker = nullptr;
    std::shared_ptr<Texture2> objectMarkerRed = nullptr;
};

struct QueuedMinimapUpdate 
{
	Vector3<short> pos;
	MinimapMapblock* data = nullptr;
};

class MinimapUpdateThread : public UpdateThread 
{
public:
	MinimapUpdateThread() : UpdateThread("Minimap") {}
	virtual ~MinimapUpdateThread();

	void GetMap(Vector3<short> pos, short size, short height);
	void EnqueueBlock(Vector3<short> pos, MinimapMapblock* data);
	bool PushBlockUpdate(Vector3<short> pos, MinimapMapblock* data);
	bool PopBlockUpdate(QueuedMinimapUpdate *update);

	MinimapData *data = nullptr;

protected:
	virtual void DoUpdate();

private:
	std::mutex mQueueMutex;
	std::deque<QueuedMinimapUpdate> mUpdateQueue;
    std::map<Vector3<short>, MinimapMapblock*> mBlocksCache;
};

class Minimap 
{
public:
	Minimap(VisualEnvironment* env);
	~Minimap();

	void AddBlock(Vector3<short> pos, MinimapMapblock* data);

	Vector3<float> GetYawVec();

	void SetPosition(Vector3<short> pos);
	Vector3<short> GetPosition() const { return mData->position; }
	void SetAngle(float angle);
	float GetAngle() const { return mAngle; }
	void ToggleMinimapShape();
	void SetMinimapShape(MinimapShape shape);
	MinimapShape GetMinimapShape();

	void ClearModes() { mModes.clear(); };
	void AddMode(MinimapMode mode);
	void AddMode(MinimapType type, uint16_t size = 0, std::string label = "",
			std::string texture = "", uint16_t scale = 1);

	void SetModeIndex(size_t index);
	size_t GetModeIndex() const { return mCurrentModeIndex; };
	size_t GetMaxModeIndex() const { return mModes.size() - 1; };
	void NextMode();

	MinimapMode GetMode() const { return mData->mode; }

	std::shared_ptr<Texture2> GetMinimapTexture();

	void BlitMinimapPixelsToImageRadar(std::shared_ptr<Texture2> mapImage);
	void BlitMinimapPixelsToImageSurface(
        std::shared_ptr<Texture2> mapImage, std::shared_ptr<Texture2> heightmapImage);

	MeshBuffer* GetMinimapMeshBuffer();

	MinimapMarker* AddMarker(Node* parentNode);
	void RemoveMarker(MinimapMarker **marker);

	void UpdateActiveMarkers();
	void DrawMinimap(BaseUI* ui);
	void DrawMinimap(BaseUI* ui, RectangleShape<2, int> rect);

	MinimapData* mData;

private:

    VisualEnvironment* mEnvironment;

	BaseTextureSource* mTextureSrc;
	BaseShaderSource* mShaderSrc;
	const NodeManager* mNodeMgr;
	MinimapUpdateThread* mMinimapUpdateThread = nullptr;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<MeshBuffer> mMeshBuffer;

	bool mEnableShaders;
	std::vector<MinimapMode> mModes;
	size_t mCurrentModeIndex;
	uint16_t mSurfaceModeScanHeight;
	float mAngle;
	std::mutex mMutex;
	std::list<MinimapMarker*> mMarkers;
	std::list<Vector2<float>> mActiveMarkers;
};

#endif