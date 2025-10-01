/*
Minetest
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2017 red-001 <red-001@outlook.ie>

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

#ifndef HUD_H
#define HUD_H

#include "../MinecraftStd.h"

#include "Tile.h"

#include "../Data/Huddata.h"

#include "Graphic/UI/UIEngine.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"
#include "Graphic/Scene/Mesh/Mesh.h"

#include "Mathematic/Geometric/Rectangle.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

// Minimap stuff

enum MinimapType 
{
	MINIMAP_TYPE_OFF,
	MINIMAP_TYPE_SURFACE,
	MINIMAP_TYPE_RADAR,
	MINIMAP_TYPE_TEXTURE,
};

class Inventory;
class InventoryList;
class VisualEnvironment;
struct ItemStack;
class VisualPlayer;
class PlayerCamera;
class Scene;

class Hud
{
public:

    Hud(Scene* pScene, BaseUI* ui, VisualEnvironment* env, 
        VisualPlayer* player, PlayerCamera* playerCamera, Inventory* inventory);
    ~Hud();

    void DrawHotbar(uint16_t playeritem);
    void ResizeHotbar();
    void DrawCrosshair();
    void DrawSelectionMesh();
    void UpdateSelectionMesh(const Vector3<short>& cameraOffset);

    std::vector<BoundingBox<float>>* GetSelectionBoxes() { return &mSelectionBoxes; }

    void SetSelectionPosition(const Vector3<float>& pos, const Vector3<short>& cameraOffset);

    Vector3<float> GetSelectionPosition() const { return mSelectionPos; }

    void SetSelectionMeshColor(const SColor& color)
    {
        mSelectionMeshColor = color;
    }

    void SetSelectedFaceNormal(const Vector3<float>& faceNormal)
    {
        mSelectedFaceNormal = faceNormal;
    }

    bool HasElementOfType(HudElementType type);

    void DrawElements(const Vector3<short>& cameraOffset);

    Scene* mScene;
    Inventory* mInventory;
    VisualPlayer* mPlayer;
    PlayerCamera* mPlayerCamera;
    BaseTextureSource* mTextureSrc;

    SColor mCrosshairARGB;
    SColor mSelectionboxARGB;

    bool mUseCrosshairImage = false;
    bool mUseObjectCrosshairImage = false;
    std::string mHotbarImage = "";
    bool mUseHotbarImage = false;
    std::string mHotbarSelectedImage = "";
    bool mUseHotbarSelectedImage = false;

    bool mPointingAtObject = false;

private:

    bool CalculateScreenPosition(const Vector3<short>& cameraOffset, HudElement* el, Vector2<int>* pos);
    void DrawStatbar(Vector2<int> pos, uint16_t corner, uint16_t drawdir,
        const std::string& texture, const std::string& bgtexture,
        int count, int maxcount, Vector2<int> offset, Vector2<int> size = Vector2<int>());
    void DrawItems(Vector2<int> upperleftpos, Vector2<int> screenOffset, int itemCount,
        int invOffset, InventoryList* mainlist, uint16_t selectitem, uint16_t direction);
    void DrawItem(const ItemStack& item, const RectangleShape<2, int>& rect, bool selected);
    void DrawCompassTranslate(HudElement* el, std::shared_ptr<Texture2> texture, 
        const RectangleShape<2, int>& rect, int angle);
    void DrawCompassRotate(HudElement* el, std::shared_ptr<Texture2> texture, 
        const RectangleShape<2, int>& rect, int angle);

    VisualEnvironment* mEnvironment;
    BaseUI* mUI;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<Visual> mVisualBackground;
    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;

    float mHudScaling; // cached minetest setting
    float mScaleFactor;
    Vector3<short> mCameraOffset;
    Vector2<unsigned int> mScreenSize;
    Vector2<int> mDisplayCenter;
    int mHotbarImageSize; // Takes hud_scaling into account, updated by ResizeHotbar()
    int mPadding; // Takes hud_scaling into account, updated by ResizeHotbar()
    SColor mHBarColors[4];

    std::vector<BoundingBox<float>> mSelectionBoxes;
    std::vector<BoundingBox<float>> mHaloBoxes;
    Vector3<float> mSelectionPosWithOffset;
    Vector3<float> mSelectionPos;

    SColor mSelectionMeshColor;
    Vector3<float> mSelectedFaceNormal;
    std::shared_ptr<BaseMesh> mSelectionMesh = nullptr;

    Material mSelectionMaterial;
    std::shared_ptr<MeshBuffer> mRotationMeshBuffer;

    enum
    {
        HIGHLIGHT_BOX,
        HIGHLIGHT_HALO,
        HIGHLIGHT_NONE
    } mMode;
};

enum ItemRotationKind
{
    IT_ROT_SELECTED,
    IT_ROT_HOVERED,
    IT_ROT_DRAGGED,
    IT_ROT_OTHER,
    IT_ROT_NONE, // Must be last, also serves as number
};

void DrawItemStack(BaseUI* ui, VisualEnvironment* env,
    const ItemStack& item, const RectangleShape<2, int>& rect, 
    const RectangleShape<2, int>* clip, ItemRotationKind rotationKind);

void DrawItemStack(BaseUI* ui, VisualEnvironment* env, 
    const ItemStack& item, const RectangleShape<2, int>& rect,
    const RectangleShape<2, int>* clip, ItemRotationKind rotationKind,
    const Vector3<short>& angle, const Vector3<short>& rotationSpeed);


#endif