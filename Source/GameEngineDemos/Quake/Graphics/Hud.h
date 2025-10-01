//========================================================================
// Hud.h : Hud Class
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/GameEngine/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#ifndef HUD_H
#define HUD_H

#include "../QuakeStd.h"

#include "../Data/Huddata.h"

#include "Graphic/UI/UIEngine.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Resource/Texture/Texture2.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"
#include "Graphic/Scene/Mesh/Mesh.h"

#include "Mathematic/Geometric/Rectangle.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

#define	ICON_SIZE			48
#define	CHAR_WIDTH			32
#define	CHAR_HEIGHT			48
#define	TEXT_ICON_SPACE		4

class Scene;
class PlayerActor;

class Hud
{
public:

    Hud(Scene* pScene, BaseUI* ui);
    ~Hud();

    void DrawSelectionMesh();
    void DrawCrosshair(const std::wstring& crosshair);
    void DrawElements(const std::shared_ptr<PlayerActor>& player);
    
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

    bool HasElementOfType(const std::shared_ptr<PlayerActor>& player, HudElementType type);

    Scene* mScene;

    SColor mCrosshairARGB;
    SColor mSelectionboxARGB;

    std::string mHotbarImage = "";
    bool mUseHotbarImage = false;
    std::string mHotbarSelectedImage = "";
    bool mUseHotbarSelectedImage = false;

private:

    bool CalculateScreenPosition(const Vector3<short>& cameraOffset, HudElement* el, Vector2<int>* pos);
    void DrawStatbar(Vector2<int> pos, uint16_t corner, uint16_t drawdir,
        const std::string& texture, const std::string& bgtexture,
        int count, int maxcount, Vector2<int> offset, Vector2<int> size = Vector2<int>());
   
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

#endif