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

#ifndef VISUALMAP_H
#define VISUALMAP_H

#include "../../Games/Map/Map.h"

#include "../Tile.h"
#include "../PlayerCamera.h"

struct MapDrawControl
{
	// Overrides limits by drawing everything
	bool rangeAll = false;
	// Wanted drawing range
	float wantedRange = 0.0f;
    // Fog distance applied
    float fogRange = 0.0f;
	// show a wire frame for debugging
	bool showWireframe = false;
};

struct VisualData
{
    std::shared_ptr<Material> material;
    std::shared_ptr<Visual> visual;
};

struct VisualLayerList
{
    /*!
     * Stores the visuals of the world.
     * The array index is the material's layer.
     * The vector part groups vertices by material.
     */
    std::vector<VisualData> visualLayers[MAX_TILE_LAYERS];

    void Clear();
    void Add(std::shared_ptr<Visual> visual, const std::shared_ptr<Material>& material, uint8_t layer);
};

struct MeshBufferList
{
	std::shared_ptr<Material> material;
	std::vector<std::pair<Vector3<short>, std::shared_ptr<BaseMeshBuffer>>> buffers;
};

struct MeshBufferLayerList
{
	/*!
	 * Stores the mesh buffers of the world.
	 * The array index is the material's layer.
	 * The vector part groups vertices by material.
	 */
	std::vector<MeshBufferList> bufferLayers[MAX_TILE_LAYERS];

	void Clear();
	void Add(std::shared_ptr<BaseMeshBuffer> buffer, Vector3<short> position, uint8_t layer);
};

/*
	VisualMap

	This is the only map class that is able to render itself on screen.
*/

class VisualMap : public Map, public Node
{
public:
    VisualMap(int id, Scene* scene, MapDrawControl* control);

    virtual ~VisualMap();

    int MapType() const
    {
        return MAPTYPE_VISUAL;
    }

    void UpdateCamera(const Vector3<float>& pos,
        const Vector3<float>& dir, float fov, const Vector3<short>& offset)
    {
        mCameraPosition = pos;
        mCameraDirection = dir;
        mCameraFov = fov;
        mCameraOffset = offset;
    }

    /*
        Forcefully get a sector from somewhere
    */
    MapSector* EmergeSector(Vector2<short> pos);

    //void deSerializeSector(Vector2<short> p2d, std::istream &is);

    /*
        ISceneNode methods
    */

    virtual bool PreRender(Scene* pScene);

    virtual bool Render(Scene* pScene);

    virtual BoundingBox<float>& GetBoundingBox()
    {
        return mBoundingBox;
    }

    void GetBlocksInViewRange(Vector3<short> camPosNodes,
        Vector3<short>* pBlocksMin, Vector3<short>* pBlocksMax);
    void UpdateDrawList();

    int GetBackgroundBrightness(float maxD, unsigned int daylightFactor,
        int oldvalue, bool* sunlightSeenResult);

    void RenderPostFx(BaseUI* ui, PlayerCameraMode camMode);

    // For debug printing
    virtual void PrintInfo(std::ostream& out);

    float GetCameraFov() const { return mCameraFov; }
    MapDrawControl* GetControl() { return mControl; }

    void SetEnvironment(VisualEnvironment* env) { mEnvironment = env; }

private:

    void UpdateShaderConstants(std::shared_ptr<Visual> visual, Scene* pScene);

    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<Visual> mVisual;

    PcgRandom mPcgRand;

    BoundingBox<float> mBoundingBox = BoundingBox<float>(
        -BS * 1000000, -BS * 1000000, -BS * 1000000,
        BS * 1000000, BS * 1000000, BS * 1000000);

    MapDrawControl* mControl;

    Vector3<float> mCameraPosition = Vector3<float>{0, 0, 0};
    Vector3<float> mCameraDirection = Vector3<float>{0, 0, 1};
	Vector3<short> mCameraOffset;
    float mCameraFov;

	std::map<Vector3<short>, MapBlock*> mDrawBlocks;
    MeshBufferLayerList mDrawMeshes;
    VisualLayerList mDrawVisuals;

    std::set<Vector2<short>> mLastDrawnSectors;

	bool mCacheTrilinearFilter;
	bool mCacheBilinearFilter;
	bool mCacheAnistropicFilter;
};

#endif