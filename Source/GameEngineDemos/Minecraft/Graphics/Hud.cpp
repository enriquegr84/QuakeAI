/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "Hud.h"
#include "Shader.h"
#include "WieldMesh.h"
#include "MeshUtil.h"
#include "PlayerCamera.h"

#include "Map/Minimap.h"

#include "Actors/ContentVisualActiveObject.h"
#include "Actors/VisualPlayer.h"

#include "../Data/Huddata.h"

#include "../Games/Environment/VisualEnvironment.h"

#include "../Games/Actors/Item.h"

#include "Core/OS/OS.h"
#include "Core/Utility/StringUtil.h"

#include "Graphic/Renderer/Renderer.h"

#include "Application/Settings.h"
#include "Application/System/System.h"

#define OBJECT_CROSSHAIR_LINE_SIZE 8
#define CROSSHAIR_LINE_SIZE 10


Hud::Hud(Scene* pScene, BaseUI* ui, VisualEnvironment* env,
    VisualPlayer* player, PlayerCamera* playerCamera, Inventory* inventory) : mUI(ui), mEnvironment(env)
{
    mScene = pScene;
    mPlayer = player;
    mPlayerCamera = playerCamera;
    mInventory = inventory;

    mScreenSize = Vector2<unsigned int>::Zero();
    mHudScaling = Settings::Get()->GetFloat("hud_scaling");
    mScaleFactor = mHudScaling * System::Get()->GetDisplayDensity();
    mHotbarImageSize = (int)std::floor(HOTBAR_IMAGE_SIZE * System::Get()->GetDisplayDensity() + 0.5f);
    mHotbarImageSize = (int)(mHotbarImageSize * mHudScaling);
    mPadding = mHotbarImageSize / 12;

    for (auto& hbarColor : mHBarColors)
        hbarColor = SColor(255, 255, 255, 255);

    mTextureSrc = mEnvironment->GetTextureSource();

    Vector3<float> crosshairColor = Settings::Get()->GetVector3("crosshair_color");
    unsigned int crossRed = std::clamp((int)round(crosshairColor[0]), 0, 255);
    unsigned int crossGreen = std::clamp((int)round(crosshairColor[1]), 0, 255);
    unsigned int crossBlue = std::clamp((int)round(crosshairColor[2]), 0, 255);
    unsigned int crossAlpha = std::clamp((int)Settings::Get()->GetInt("crosshair_alpha"), 0, 255);
    mCrosshairARGB = SColor(crossAlpha, crossRed, crossGreen, crossBlue);

    Vector3<float> selectionboxColor = Settings::Get()->GetVector3("selectionbox_color");
    unsigned int sboxRed = std::clamp((int)round(selectionboxColor[0]), 0, 255);
    unsigned int sboxGreen = std::clamp((int)round(selectionboxColor[1]), 0, 255);
    unsigned int sboxBlue = std::clamp((int)round(selectionboxColor[2]), 0, 255);
    mSelectionboxARGB = SColor(255, sboxRed, sboxGreen, sboxBlue);

    mUseCrosshairImage = mTextureSrc->IsKnownSourceImage("crosshair.png");
    mUseObjectCrosshairImage = mTextureSrc->IsKnownSourceImage("object_crosshair.png");

    mSelectionBoxes.clear();
    mHaloBoxes.clear();

    std::string modeSetting = Settings::Get()->Get("node_highlighting");

    if (modeSetting == "halo")
        mMode = HIGHLIGHT_HALO;
    else if (modeSetting == "none")
        mMode = HIGHLIGHT_NONE;
    else
        mMode = HIGHLIGHT_BOX;

    mSelectionMaterial.mLighting = false;
    if (Settings::Get()->GetBool("enable_shaders"))
    {
        BaseShaderSource* shaderSrc = mEnvironment->GetShaderSource();
        uint16_t shaderId = shaderSrc->GetShader(
            mMode == HIGHLIGHT_HALO ? "Selection" : "Default", TILE_MATERIAL_ALPHA);
        mSelectionMaterial.mType = shaderSrc->GetShaderInfo(shaderId).material;
        mSelectionMaterial.mTypeParam2 = shaderId;
    }
    else mSelectionMaterial.mType = MT_TRANSPARENT_ALPHA_CHANNEL;

    if (mMode == HIGHLIGHT_BOX) 
    {
        mSelectionMaterial.mThickness =
            (float)std::clamp(Settings::Get()->GetInt("selectionbox_width"), 1, 5);
    }
    else if (mMode == HIGHLIGHT_HALO) 
    {
        mSelectionMaterial.SetTexture(0, mTextureSrc->GetTextureForMesh("halo.png"));
        mSelectionMaterial.mCullMode = RasterizerState::CULL_BACK;
    }
    else mSelectionMaterial.mType = MT_SOLID;

    if (mSelectionMaterial.IsTransparent())
    {
        mSelectionMaterial.mBlendTarget.enable = true;
        mSelectionMaterial.mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        mSelectionMaterial.mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        mSelectionMaterial.mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        mSelectionMaterial.mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        mSelectionMaterial.mDepthBuffer = true;
        mSelectionMaterial.mDepthMask = DepthStencilState::MASK_ALL;
    }

    //basic visual effect
    {
        mBlendState = std::make_shared<BlendState>();
        mBlendState->mTarget[0].enable = true;
        mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        // Create a vertex buffer for a single triangle.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/ColorEffectVS.glsl");
        path.push_back("Effects/ColorEffectPS.glsl");
#else
        path.push_back("Effects/ColorEffectVS.hlsl");
        path.push_back("Effects/ColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

        // Create the geometric object for drawing.
        mVisualBackground = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

    std::shared_ptr<ResHandle>& resHandle =
        ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
    if (resHandle)
    {
        const std::shared_ptr<ImageResourceExtraData>& extra =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        extra->GetImage()->AutogenerateMipmaps();

        // Create a vertex buffer for a two-triangles square. The PNG is stored
        // in left-handed coordinates. The texture coordinates are chosen to
        // reflect the texture in the y-direction.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        // Create an effect for the vertex and pixel shaders. The texture is
        // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extraRes =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extraRes->GetProgram())
            extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        mEffect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

        // Create the geometric object for drawing.
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
    }

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
    vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);

    SColorF white = SColor(255, 255, 255, 255);
    Vector3<float> normal{ 0.f, 0.f, 1.f };

    MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
    mRotationMeshBuffer.reset(meshBuffer);

    //fill vertices
    mRotationMeshBuffer->Position(0) = Vector3<float>{ -1.f, -1.f, 0.f };
    mRotationMeshBuffer->Position(1) = Vector3<float>{ -1.f, 1.f, 0.f };
    mRotationMeshBuffer->Position(2) = Vector3<float>{ 1.f, 1.f, 0.f };
    mRotationMeshBuffer->Position(3) = Vector3<float>{ 1.f, -1.f, 0.f };

    mRotationMeshBuffer->Normal(0) = normal;
    mRotationMeshBuffer->Normal(1) = normal;
    mRotationMeshBuffer->Normal(2) = normal;
    mRotationMeshBuffer->Normal(3) = normal;

    mRotationMeshBuffer->Color(0, 0) = white.ToArray();
    mRotationMeshBuffer->Color(0, 1) = white.ToArray();
    mRotationMeshBuffer->Color(0, 2) = white.ToArray();
    mRotationMeshBuffer->Color(0, 3) = white.ToArray();

    mRotationMeshBuffer->TCoord(0, 0) = Vector2<float>{ 0.0f, 1.0f };
    mRotationMeshBuffer->TCoord(0, 1) = Vector2<float>{ 0.0f, 0.0f };
    mRotationMeshBuffer->TCoord(0, 2) = Vector2<float>{ 1.0f, 0.0f };
    mRotationMeshBuffer->TCoord(0, 3) = Vector2<float>{ 1.0f, 1.0f };

    // fill indices
    int vertices = 0;
    for (unsigned int i = 0; i < mRotationMeshBuffer->GetIndice()->GetNumPrimitives(); i += 2, vertices += 4)
    {
        mRotationMeshBuffer->GetIndice()->SetTriangle(i,
            (unsigned int)0 + vertices, (unsigned int)1 + vertices, (unsigned int)2 + vertices);
        mRotationMeshBuffer->GetIndice()->SetTriangle(i + 1,
            (unsigned int)2 + vertices, (unsigned int)3 + vertices, (unsigned int)0 + vertices);
    }

    // Set material
    mRotationMeshBuffer->GetMaterial()->mLighting = false;
    mRotationMeshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

    mRotationMeshBuffer->GetMaterial()->mBlendTarget.enable = true;
    mRotationMeshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
    mRotationMeshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
    mRotationMeshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
    mRotationMeshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    mRotationMeshBuffer->GetMaterial()->mDepthBuffer = true;
    mRotationMeshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

    mRotationMeshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
    mRotationMeshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;
}

Hud::~Hud()
{

}

void Hud::DrawItem(const ItemStack& item, const RectangleShape<2, int>& rect, bool selected)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Vector2<int> screenSize{(int)mScreenSize[0], (int)mScreenSize[1] };

    RectangleShape<2, int> clipRect;
    clipRect.mExtent = screenSize;
    clipRect.mCenter = screenSize / 2;

    if (selected) 
    {
        /* draw hihlighting around selected item */
        if (mUseHotbarSelectedImage) 
        {
            RectangleShape<2, int> imgRect = rect;
            imgRect.mExtent += Vector2<int>{(mPadding * 4), (mPadding * 4)};

            std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(mHotbarSelectedImage);
            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
            effect->SetTexture(texture);
            
            Vector2<unsigned int> texSize = mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(texture));

            RectangleShape<2, int> tcoordRect;
            tcoordRect.mExtent = {(int)texSize[0], (int)texSize[1]};
            tcoordRect.mCenter = tcoordRect.mExtent / 2;
            skin->Draw2DTextureFilterScaled(mVisual, imgRect, tcoordRect, mHBarColors);
        }
        else 
        {
            SColor cOutside(255, 255, 0, 0);

            int x1 = rect.GetVertice(RVP_UPPERLEFT)[0];
            int y1 = rect.GetVertice(RVP_UPPERLEFT)[1];
            int x2 = rect.GetVertice(RVP_LOWERRIGHT)[0];
            int y2 = rect.GetVertice(RVP_LOWERRIGHT)[1];

            // Black base borders
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ x2 + mPadding, y1 } - Vector2<int>{ x1 - mPadding, y1 - mPadding };
            rect.mCenter = Vector2<int>{ x1 - mPadding, y1 - mPadding } + rect.mExtent / 2;
            skin->Draw2DRectangle(cOutside, mVisualBackground, rect, &clipRect);

            rect.mExtent = Vector2<int>{ x2 + mPadding, y2 + mPadding } - Vector2<int>{ x1 - mPadding, y2 };
            rect.mCenter = Vector2<int>{ x1 - mPadding, y2 } + rect.mExtent / 2;
            skin->Draw2DRectangle(cOutside, mVisualBackground, rect, &clipRect);

            rect.mExtent = Vector2<int>{ x1, y2 } - Vector2<int>{ x1 - mPadding, y1 };
            rect.mCenter = Vector2<int>{ x1 - mPadding, y1 } + rect.mExtent / 2;
            skin->Draw2DRectangle(cOutside, mVisualBackground, rect, &clipRect);

            rect.mExtent = Vector2<int>{ x2, y1 } - Vector2<int>{ x2 - mPadding, y2 };
            rect.mCenter = Vector2<int>{ x2 - mPadding, y2 } + rect.mExtent / 2;
            skin->Draw2DRectangle(cOutside, mVisualBackground, rect, &clipRect);
        }
    }

    SColor bgColor(128, 0, 0, 0);
    if (!mUseHotbarImage)
        skin->Draw2DRectangle(bgColor, mVisualBackground, rect, &clipRect);
    DrawItemStack(mUI, mEnvironment, item, rect, &clipRect, selected ? IT_ROT_SELECTED : IT_ROT_NONE);
}

//NOTE: selectitem = 0 -> no selected; selectitem 1-based
void Hud::DrawItems(Vector2<int> upperleftpos, Vector2<int> screenOffset, int itemCount,
    int invOffset, InventoryList* mainlist, uint16_t selectitem, uint16_t direction)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Renderer::Get()->SetBlendState(mBlendState);

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    int height = mHotbarImageSize + mPadding * 2;
    int width = (itemCount - invOffset) * (mHotbarImageSize + mPadding * 2);

    if (direction == HUD_DIR_TOP_BOTTOM || direction == HUD_DIR_BOTTOM_TOP) 
    {
        int tmp = height;
        height = width;
        width = tmp;
    }

    // Position of upper left corner of bar
    Vector2<int> pos = screenOffset * (int)mScaleFactor;
    pos += upperleftpos;

    // Store mHotbarImage in member variable, used by DrawItem()
    if (mHotbarImage != mPlayer->mHotbarImage) 
    {
        mHotbarImage = mPlayer->mHotbarImage;
        mUseHotbarImage = !mHotbarImage.empty();
    }

    // Store mHotbarSelectedImage in member variable, used by DrawItem()
    if (mHotbarSelectedImage != mPlayer->mHotbarSelectedImage)
    {
        mHotbarSelectedImage = mPlayer->mHotbarSelectedImage;
        mUseHotbarSelectedImage = !mHotbarSelectedImage.empty();
    }

    // draw customized item background
    RectangleShape<2, int> imgRect;
    if (mUseHotbarImage)
    {
        imgRect.mExtent = Vector2<int>{width, height};
        imgRect.mCenter[0] = -mPadding / 2 + imgRect.mExtent[0] / 2;
        imgRect.mCenter[1] = -mPadding / 2 + imgRect.mExtent[1] / 2;
        imgRect.mCenter += pos;

        std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(mHotbarImage);
        auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
        effect->SetTexture(texture);

        Vector2<unsigned int> texSize = mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(texture));

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent = { (int)texSize[0], (int)texSize[1] };
        tcoordRect.mCenter = tcoordRect.mExtent / 2;
        skin->Draw2DTextureFilterScaled(mVisual, imgRect, tcoordRect, mHBarColors);
    }

    // Draw items
    imgRect.mExtent = Vector2<int>{ mHotbarImageSize, mHotbarImageSize };
    imgRect.mCenter = imgRect.mExtent / 2;

    for (int i = invOffset; i < itemCount && (size_t)i < mainlist->GetSize(); i++) 
    {
        int fullimgLen = mHotbarImageSize + mPadding * 2;

        Vector2<int> stepPos;
        switch (direction) 
        {
            case HUD_DIR_RIGHT_LEFT:
                stepPos = Vector2<int>{ -(mPadding + (i - invOffset) * fullimgLen), mPadding };
                break;
            case HUD_DIR_TOP_BOTTOM:
                stepPos = Vector2<int>{ mPadding, mPadding + (i - invOffset) * fullimgLen };
                break;
            case HUD_DIR_BOTTOM_TOP:
                stepPos = Vector2<int>{ mPadding, -(mPadding + (i - invOffset) * fullimgLen) };
                break;
            default:
                stepPos = Vector2<int>{ mPadding + (i - invOffset) * fullimgLen, mPadding };
                break;
        }

        RectangleShape<2, int> rect = imgRect;
        rect.mCenter += pos + stepPos;
        DrawItem(mainlist->GetItem(i), rect, (i + 1) == selectitem);
    }

    Renderer::Get()->SetDefaultBlendState();
}

bool Hud::HasElementOfType(HudElementType type)
{
    for (unsigned int i = 0; i != mPlayer->MaxHudId(); i++)
    {
        HudElement* el = mPlayer->GetHud(i);
        if (!el)
            continue;
        if (el->type == type)
            return true;
    }
    return false;
}

// Calculates screen position of waypoint. Returns true if waypoint is visible (in front of the player), else false.
bool Hud::CalculateScreenPosition(const Vector3<short>& cameraOffset, HudElement* e, Vector2<int>* pos)
{
    Vector3<float> wPos = e->worldPosition * BS;

    std::shared_ptr<CameraNode> camera = mScene->GetActiveCamera();
    wPos -= {cameraOffset[0] * BS, cameraOffset[1] * BS, cameraOffset[2] * BS};
    Transform transform;
    transform.SetMatrix(camera->Get()->GetProjectionMatrix());
    transform.SetMatrix(transform * camera->Get()->GetViewMatrix());
    
    Vector4<float> transformedPos = { wPos[0], wPos[1], wPos[2], 1.0f };
    transformedPos = transform * transformedPos;

    if (transformedPos[3] < 0)
        return false;
    float zDiv = transformedPos[3] == 0.0f ? 1.0f : 1.f / transformedPos[3];
    (*pos)[0] = (int)(mScreenSize[0] * (0.5f * transformedPos[0] * zDiv + 0.5f));
    (*pos)[1] = (int)(mScreenSize[1] * (0.5f - transformedPos[1] * zDiv * 0.5f));
    return true;
}

void Hud::DrawElements(const Vector3<short>& cameraOffset)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    std::shared_ptr<BaseUIFont> textFont = mUI->GetBuiltInFont();
    int textHeight = textFont->GetDimension(L"Ay")[1];

    // Reorder elements by z_index
    std::vector<HudElement*> elems;
    elems.reserve(mPlayer->MaxHudId());

    for (unsigned int i = 0; i != mPlayer->MaxHudId(); i++)
    {
        HudElement* el = mPlayer->GetHud(i);
        if (!el)
            continue;

        auto it = elems.begin();
        while (it != elems.end() && (*it)->zIndex <= el->zIndex)
            ++it;

        elems.insert(it, el);
    }

    for (HudElement* el : elems) 
    {
        Vector2<int> pos{
            (int)floor(el->position[0] * (float)screenSize[0] + 0.5),
            (int)floor(el->position[1] * (float)screenSize[1] + 0.5) };
        switch (el->type) 
        {
            case HUD_ELEM_TEXT: 
            {
                /*
                unsigned int fontSize = g_fontengine->getDefaultFontSize();

                if (el->size[0] > 0)
                    fontSize *= el->size[0];

                if (fontSize != g_fontengine->getDefaultFontSize())
                    textFont = g_fontengine->getFont(fontSize);
                */
                SColor color(255, 
                    (el->number >> 16) & 0xFF,
                    (el->number >> 8) & 0xFF,
                    (el->number >> 0) & 0xFF);
                std::wstring text = ToWideString(el->text);
                Vector2<int> textSize = textFont->GetDimension(text);

                Vector2<int> offset{
                    (int)((el->align[0] - 1) * (textSize[0] / 2)),
                    (int)((el->align[1] - 1) * (textSize[1] / 2)) };
                Vector2<int> offs{
                    (int)(el->offset[0] * mScaleFactor),
                    (int)(el->offset[1] * mScaleFactor) };

                RectangleShape<2, int> size;
                size.mExtent = Vector2<int>{
                    (int)(el->scale[0] * mScaleFactor),
                    (int)(textHeight * el->scale[1] * mScaleFactor) };
                size.mCenter = size.mExtent / 2;
                size.mCenter += pos + offset + offs;

                textFont->Draw(text.c_str(), size, color, false, true);
                break; 
            }
            case HUD_ELEM_STATBAR: 
            {
                Vector2<int> offs{ (int)el->offset[0], (int)el->offset[1] };
                DrawStatbar(pos, HUD_CORNER_UPPER, 
                    el->direction, el->text, el->text2,
                    el->number, el->item, offs, el->size);
                break; 
            }
            case HUD_ELEM_INVENTORY: 
            {
                InventoryList* inv = mInventory->GetList(el->text);
                DrawItems(pos, 
                    Vector2<int>{(int)el->offset[0], (int)el->offset[1]}, 
                    el->number, 0, inv, el->item, el->direction);
                break; 
            }
            case HUD_ELEM_WAYPOINT: 
            {
                if (!CalculateScreenPosition(cameraOffset, el, &pos))
                    break;
                Vector3<float> playerPos = mPlayer->GetPosition() / BS;
                pos += Vector2<int>{(int)el->offset[0], (int)el->offset[1]};
                SColor color(255, 
                    (el->number >> 16) & 0xFF,
                    (el->number >> 8) & 0xFF,
                    (el->number >> 0) & 0xFF);
                std::wstring text = ToWideString(el->name);
                const std::string& unit = el->text;
                // waypoints reuse the item field to store precision, item = precision + 1
                unsigned int item = el->item;
                float precision = (item == 0) ? 10.0f : (item - 1.f);
                bool drawPrecision = precision > 0;

                RectangleShape<2, int> bounds;
                bounds.mExtent = Vector2<int>{
                    textFont->GetDimension(text)[0], (drawPrecision ? 2 : 1) * textHeight};
                pos[1] += (int)((el->align[1] - 1.f) * bounds.mExtent[1] / 2);
                
                bounds.mCenter = bounds.mExtent / 2;
                bounds.mCenter += pos;

                RectangleShape<2, int> drawRect = bounds;
                drawRect.mCenter += Vector2<int>{(int)((el->align[0] - 1.0) * bounds.mExtent[0] / 2), 0};
                textFont->Draw(text.c_str(), drawRect, color);
                if (drawPrecision) 
                {
                    std::ostringstream os;
                    float distance = std::floor(precision * Length( playerPos - el->worldPosition)) / precision;
                    os << distance << unit;
                    text = ToWideString(os.str());
                    bounds.mCenter[0] = 
                        bounds.GetVertice(RVP_LOWERRIGHT)[0] + textFont->GetDimension(text)[0] / 2;
                    bounds.mExtent[0] = textFont->GetDimension(text)[0];
                    
                    drawRect = bounds;
                    drawRect.mCenter += Vector2<int>{(int)((el->align[0] - 1.f) * bounds.mExtent[0] / 2), textHeight};
                    textFont->Draw(text.c_str(), drawRect, color);
                }
                break; 
            }
            case HUD_ELEM_IMAGE_WAYPOINT: 
            {
                if (!CalculateScreenPosition(cameraOffset, el, &pos))
                    break;
            }
            case HUD_ELEM_IMAGE: 
            {
                std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(el->text);
                if (!texture)
                    continue;

                const SColor color(255, 255, 255, 255);
                const SColor colors[] = { color, color, color, color };

                Vector2<unsigned int> imgSize =
                    mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(texture));
                Vector2<int> dstSize{ 
                    (int)(imgSize[0] * el->scale[0] * mScaleFactor),
                    (int)(imgSize[1] * el->scale[1] * mScaleFactor) };
                if (el->scale[0] < 0)
                    dstSize[0] = (int)(screenSize[0] * (el->scale[0] * -0.01f));
                if (el->scale[1] < 0)
                    dstSize[1] = (int)(screenSize[1] * (el->scale[1] * -0.01f));
                Vector2<int> offset{ 
                    (int)((el->align[0] - 1.f) * dstSize[0] / 2),
                    (int)((el->align[1] - 1.f) * dstSize[1] / 2)};
                RectangleShape<2, int> rect;
                rect.mExtent = dstSize;
                rect.mCenter = dstSize / 2;
                rect.mCenter += pos + offset;
                rect.mCenter += Vector2<int>{
                    (int)(el->offset[0] * mScaleFactor), 
                    (int)(el->offset[1] * mScaleFactor)};

                auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
                effect->SetTexture(texture);

                RectangleShape<2, int> tcoordRect;
                tcoordRect.mExtent = { (int)imgSize[0], (int)imgSize[1] };
                tcoordRect.mCenter = tcoordRect.mExtent / 2;
                skin->Draw2DTextureFilterScaled(mVisual, rect, tcoordRect, colors);
                break; 
            }
            case HUD_ELEM_COMPASS: 
            {
                std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(el->text);
                if (!texture)
                    continue;

                // Positionning :
                Vector2<int> dstSize{ el->size[0], el->size[1] };
                if (el->size[0] < 0)
                    dstSize[0] = (int)(screenSize[0] * (el->size[0] * -0.01f));
                if (el->size[1] < 0)
                    dstSize[1] = (int)(screenSize[1] * (el->size[1] * -0.01f));

                if (dstSize[0] <= 0 || dstSize[1] <= 0)
                    return; // Avoid zero divides

                // Angle according to camera view
                std::shared_ptr<CameraNode> camera = mScene->GetActiveCamera();

                // Limit angle and ajust with given offset
                int angle = (angle + (int)el->number) % 360;

                RectangleShape<2, int> dstRect;
                dstRect.mExtent = dstSize;
                dstRect.mCenter = dstSize / 2;
                dstRect.mCenter += pos;
                dstRect.mCenter += Vector2<int>{
                    (int)((el->align[0] - 1.0) * dstSize[0] / 2),
                    (int)((el->align[1] - 1.0) * dstSize[1] / 2)};
                dstRect.mCenter += Vector2<int>{
                    (int)(el->offset[0] * mHudScaling), 
                    (int)(el->offset[1] * mHudScaling)};

                switch (el->direction)
                {
                    case HUD_COMPASS_ROTATE:
                        DrawCompassRotate(el, texture, dstRect, angle);
                        break;
                    case HUD_COMPASS_ROTATE_REVERSE:
                        DrawCompassRotate(el, texture, dstRect, -angle);
                        break;
                    case HUD_COMPASS_TRANSLATE:
                        DrawCompassTranslate(el, texture, dstRect, angle);
                        break;
                    case HUD_COMPASS_TRANSLATE_REVERSE:
                        DrawCompassTranslate(el, texture, dstRect, -angle);
                        break;
                    default:
                        break;
                }
                break; 
            }
            case HUD_ELEM_MINIMAP: 
            {
                if (el->size[0] <= 0 || el->size[1] <= 0)
                    break;
                if (!mEnvironment->GetMinimap())
                    break;
                // Draw a minimap of size "size"
                Vector2<int> dstSize{ 
                    (int)(el->size[0] * mScaleFactor), 
                    (int)(el->size[1] * mScaleFactor) };
                // (no percent size as minimap would likely be anamorphosed)
                Vector2<int> offset{
                    (int)((el->align[0] - 1.0) * dstSize[0] / 2),
                    (int)((el->align[1] - 1.0) * dstSize[1] / 2) };
                RectangleShape<2, int> rect;
                rect.mExtent = dstSize;
                rect.mCenter = dstSize / 2;
                rect.mCenter += pos + offset;
                rect.mCenter += Vector2<int>{
                    (int)(el->offset[0] * mScaleFactor), 
                    (int)(el->offset[1] * mScaleFactor)};
                mEnvironment->GetMinimap()->DrawMinimap(mUI, rect);
                break; 
            }
            default:
                LogInformation("Hud::DrawElements: ignoring drawform " + 
                    std::to_string(el->type) + " due to unrecognized type");
                break;
        }
    }
}

void Hud::DrawCompassTranslate(HudElement* el, 
    std::shared_ptr<Texture2> texture, const RectangleShape<2, int>& rect, int angle)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    // Compute source image scaling

    Vector2<unsigned int> imgSize = 
        mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(texture));
    Vector2<int> dstSize{
        (int)(rect.mExtent[1] * el->scale[0] * imgSize[0] / imgSize[1]),
        (int)(rect.mExtent[1] * el->scale[1]) };

    // Avoid infinite loop
    if (dstSize[0] <= 0 || dstSize[1] <= 0)
        return;

    RectangleShape<2, int> tgtRect;
    tgtRect.mExtent = dstSize;
    tgtRect.mCenter = dstSize / 2;
    tgtRect.mCenter += Vector2<int>{
        (rect.mExtent[0] - dstSize[0]) / 2,
        (rect.mExtent[1] - dstSize[1]) / 2};
    tgtRect.mCenter += rect.GetVertice(RVP_UPPERLEFT);

    int offset = (int)(angle * GE_C_RAD_TO_DEG * dstSize[0] / 360.f);
    tgtRect.mCenter += Vector2<int>{offset, 0};

    // Repeat image as much as needed
    while (tgtRect.GetVertice(RVP_UPPERLEFT)[0] > rect.GetVertice(RVP_UPPERLEFT)[0])
        tgtRect.mCenter -= Vector2<int>{dstSize[0], 0};

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
    effect->SetTexture(texture);

    RectangleShape<2, int> tcoordRect;
    tcoordRect.mExtent = { (int)imgSize[0], (int)imgSize[1] };
    tcoordRect.mCenter = tcoordRect.mExtent / 2;

    skin->Draw2DTextureFilterScaled(mVisual, tgtRect, tcoordRect, colors);
    tgtRect.mCenter += Vector2<int>{dstSize[0], 0};

    while (tgtRect.GetVertice(RVP_UPPERLEFT)[0] < rect.GetVertice(RVP_LOWERRIGHT)[0]) 
    {
        skin->Draw2DTextureFilterScaled(mVisual, tgtRect, tcoordRect, colors);
        tgtRect.mCenter += Vector2<int>{dstSize[0], 0};
    }
}

void Hud::DrawCompassRotate(HudElement* el, 
    std::shared_ptr<Texture2> texture, const RectangleShape<2, int>& rect, int angle)
{ 
    int viewX, viewY, viewW, viewH;
    Renderer::Get()->GetViewport(viewX, viewY, viewW, viewH);

    Vector2<int> viewSize = rect.mExtent;
    Vector2<int> viewOrigin = rect.GetVertice(RVP_UPPERLEFT);
    Renderer::Get()->SetViewport(viewOrigin[0], viewOrigin[1], viewSize[0], viewSize[1]);

    float yaw = 0.f;
    float pitch = 0.f;
    float roll = angle * (float)GE_C_DEG_TO_RAD;;

    Matrix4x4<float> yawRotation = Rotation<4, float>(
        AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), yaw));
    Matrix4x4<float> pitchRotation = Rotation<4, float>(
        AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), pitch));
    Matrix4x4<float> rollRotation = Rotation<4, float>(
        AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), roll));

    Transform transform;
    transform.MakeIdentity();
    transform.SetRotation(yawRotation * pitchRotation * rollRotation);

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
    effect->SetTexture(texture);

    std::shared_ptr<ConstantBuffer> cbuffer;
    cbuffer = effect->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
    *cbuffer->Get<Matrix4x4<float>>() = transform.GetMatrix();

    Renderer* renderer = Renderer::Get();
    renderer->Update(cbuffer);
    renderer->Update(mRotationMeshBuffer->GetVertice());
    renderer->Draw(mVisual);

    // restore the view area
    Renderer::Get()->SetViewport(viewX, viewY, viewW, viewH);
}

void Hud::DrawStatbar(Vector2<int> pos, uint16_t corner, uint16_t drawdir,
    const std::string& texture, const std::string& bgtexture,
    int count, int maxcount, Vector2<int> offset, Vector2<int> size)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    std::shared_ptr<Texture2> statTexture = mTextureSrc->GetTexture(texture);
    if (!statTexture)
        return;

    std::shared_ptr<Texture2> statTextureBG = nullptr;
    if (!bgtexture.empty())
        statTextureBG = mTextureSrc->GetTexture(bgtexture);

    Vector2<unsigned int> src = 
        mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(statTexture));
    Vector2<int> srcd = {(int)src[0], (int)src[1]};
    Vector2<int> dstd;
    if (size == Vector2<int>::Zero()) 
    {
        dstd[1] = (int)(srcd[1] * mScaleFactor);
        dstd[0] = (int)(srcd[0] * mScaleFactor);
    }
    else 
    {
        dstd[1] = (int)(size[1] * mScaleFactor);
        dstd[0] = (int)(size[0] * mScaleFactor);
    }
    offset[0] = (int)(offset[0] * mScaleFactor);
    offset[1] = (int)(offset[1] * mScaleFactor);

    Vector2<int> p = pos;
    if (corner & HUD_CORNER_LOWER)
        p -= Vector2<int>{dstd[1], dstd[1]};

    p += offset;

    Vector2<int> steppos;
    switch (drawdir) 
    {
        case HUD_DIR_RIGHT_LEFT:
            steppos = Vector2<int>{ -1, 0 };
            break;
        case HUD_DIR_TOP_BOTTOM:
            steppos = Vector2<int>{ 0, 1 };
            break;
        case HUD_DIR_BOTTOM_TOP:
            steppos = Vector2<int>{ 0, -1 };
            break;
        default:
            // From left to right
            steppos = Vector2<int>{ 1, 0 };
            break;
    }

    auto CalculateClippingRect = [](Vector2<int> src,
        Vector2<int> steppos) -> RectangleShape<2, int> {

            // Create basic rectangle
            RectangleShape<2, int> rect;
            rect.mExtent = Vector2<int>{ 
                src[0] - std::abs(steppos[0]) * src[0] / 2,
                src[1] - std::abs(steppos[1]) * src[1] / 2 };
            rect.mCenter = rect.mExtent / 2;

            // Move rectangle left or down
            if (steppos[0] == -1)
                rect.mCenter += Vector2<int>{src[0] / 2, 0};
            if (steppos[1] == -1)
                rect.mCenter += Vector2<int>{0, src[1] / 2};
            return rect;
    };
    // Rectangles for 1/2 the actual value to display
    RectangleShape<2, int> srcHalfRect, dstHalfRect;
    // Rectangles for 1/2 the "off state" texture
    RectangleShape<2, int> srcHalfRect2, dstHalfRect2;

    if (count % 2 == 1) 
    {
        // Need to draw halves: Calculate rectangles
        srcHalfRect = CalculateClippingRect(srcd, steppos);
        dstHalfRect = CalculateClippingRect(dstd, steppos);
        srcHalfRect2 = CalculateClippingRect(srcd, steppos * -1);
        dstHalfRect2 = CalculateClippingRect(dstd, steppos * -1);
    }

    steppos[0] *= dstd[0];
    steppos[1] *= dstd[1];

    // Draw full textures
    for (int i = 0; i < count / 2; i++) 
    {
        RectangleShape<2, int> srcRect;
        srcRect.mExtent = srcd;
        srcRect.mCenter = srcd / 2;

        RectangleShape<2, int> dstRect;
        dstRect.mExtent = dstd;
        dstRect.mCenter = dstd / 2;
        dstRect.mCenter += p;

        auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
        effect->SetTexture(statTexture);
        skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);

        p += steppos;
    }

    if (count % 2 == 1) 
    {
        // Draw half a texture
        RectangleShape<2, int> srcRect = srcHalfRect;
        RectangleShape<2, int> dstRect = dstHalfRect;
        dstRect.mCenter += p;

        auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
        effect->SetTexture(statTexture);
        skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);

        if (statTextureBG && maxcount > count) 
        {
            srcRect = srcHalfRect2;
            dstRect = dstHalfRect2;
            dstRect.mCenter += p;

            skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);
            p += steppos;
        }
    }

    if (statTextureBG && maxcount > count / 2) 
    {
        // Draw "off state" textures
        int start_offset;
        if (count % 2 == 1)
            start_offset = count / 2 + 1;
        else
            start_offset = count / 2;
        for (int i = start_offset; i < maxcount / 2; i++) 
        {
            RectangleShape<2, int> srcRect;
            srcRect.mExtent = srcd;
            srcRect.mCenter = srcd / 2;

            RectangleShape<2, int> dstRect;
            dstRect.mExtent = dstd;
            dstRect.mCenter = dstd / 2;
            dstRect.mCenter += p;

            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
            effect->SetTexture(statTextureBG);
            skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);

            p += steppos;
        }

        if (maxcount % 2 == 1) 
        {
            RectangleShape<2, int> srcRect = srcHalfRect;
            RectangleShape<2, int> dstRect = dstHalfRect;
            dstRect.mCenter += p;

            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
            effect->SetTexture(statTextureBG);
            skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);
        }
    }
}


void Hud::DrawHotbar(uint16_t playeritem) 
{
    Vector2<int> centerLowerPos{ mDisplayCenter[0], (int)mScreenSize[1] };

    InventoryList* mainlist = mInventory->GetList("main");
    if (mainlist == NULL) 
    {
        //silently ignore this we may not be initialized completely
        return;
    }

    int hotbarItemCount = mPlayer->mHudHotbarItemCount;
    int width = hotbarItemCount * (mHotbarImageSize + mPadding * 2);
    Vector2<int> pos = centerLowerPos - Vector2<int>{width / 2, mHotbarImageSize + mPadding * 3};

    if ((float)width / (float)mScreenSize[0] <= Settings::Get()->GetFloat("hud_hotbar_max_width")) 
    {
        if (mPlayer->mHudFlags & HUD_FLAG_HOTBAR_VISIBLE)
            DrawItems(pos, Vector2<int>::Zero(), hotbarItemCount, 0, mainlist, playeritem + 1, 0);
    }
    else 
    {
        pos[0] += width / 4;

        Vector2<int> secondpos = pos;
        pos = pos - Vector2<int>{0, mHotbarImageSize + mPadding};

        if (mPlayer->mHudFlags & HUD_FLAG_HOTBAR_VISIBLE)
        {
            DrawItems(pos, Vector2<int>::Zero(), 
                hotbarItemCount / 2, 0, mainlist, playeritem + 1, 0);
            DrawItems(secondpos, Vector2<int>::Zero(), 
                hotbarItemCount, hotbarItemCount / 2, mainlist, playeritem + 1, 0);
        }
    }
}


void Hud::DrawCrosshair()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    if (mPointingAtObject) 
    {
        if (mUseObjectCrosshairImage) 
        {
            std::shared_ptr<Texture2> objectCrosshair = mTextureSrc->GetTexture("object_crosshair.png");
            Vector2<unsigned int> imgSize = 
                mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(objectCrosshair));

            RectangleShape<2, int> dstRect;
            dstRect.mExtent = { (int)imgSize[0], (int)imgSize[1] };
            dstRect.mCenter = mDisplayCenter + (dstRect.mExtent / 2);

            RectangleShape<2, int> tcoordRect;
            tcoordRect.mExtent = { (int)imgSize[0], (int)imgSize[1] };;
            tcoordRect.mCenter = tcoordRect.mExtent / 2;

            const SColor colors[] = { mCrosshairARGB };
            
            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
            effect->SetTexture(objectCrosshair);
            skin->Draw2DTexture(mVisual, dstRect, tcoordRect, colors);
        }
        else 
        {
            Vector2<float> start{
                (float)mDisplayCenter[0] - OBJECT_CROSSHAIR_LINE_SIZE, 
                (float)mDisplayCenter[1] - OBJECT_CROSSHAIR_LINE_SIZE };
            Vector2<float> end{
                (float)mDisplayCenter[0] + OBJECT_CROSSHAIR_LINE_SIZE,
                (float)mDisplayCenter[1] + OBJECT_CROSSHAIR_LINE_SIZE };
            skin->Draw2DLine(mCrosshairARGB, start, end);

            start = Vector2<float>{
                (float)mDisplayCenter[0] + OBJECT_CROSSHAIR_LINE_SIZE,
                (float)mDisplayCenter[1] - OBJECT_CROSSHAIR_LINE_SIZE };
            end = Vector2<float>{
                (float)mDisplayCenter[0] - OBJECT_CROSSHAIR_LINE_SIZE,
                (float)mDisplayCenter[1] + OBJECT_CROSSHAIR_LINE_SIZE };
            skin->Draw2DLine(mCrosshairARGB, start, end);
        }

        return;
    }

    if (mUseCrosshairImage) 
    {
        std::shared_ptr<Texture2> crosshair = mTextureSrc->GetTexture("crosshair.png");
        Vector2<unsigned int> imgSize =
            mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(crosshair));

        RectangleShape<2, int> dstRect;
        dstRect.mExtent = { (int)imgSize[0], (int)imgSize[1] };
        dstRect.mCenter = mDisplayCenter + (dstRect.mExtent / 2);

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent = { (int)imgSize[0], (int)imgSize[1] };;
        tcoordRect.mCenter = tcoordRect.mExtent / 2;

        const SColor colors[] = { mCrosshairARGB };

        auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
        effect->SetTexture(crosshair);
        skin->Draw2DTexture(mVisual, dstRect, tcoordRect, colors);
    }
    else 
    {
        Vector2<float> start{
            (float)mDisplayCenter[0] - CROSSHAIR_LINE_SIZE, (float)mDisplayCenter[1]};
        Vector2<float> end{
            (float)mDisplayCenter[0] + CROSSHAIR_LINE_SIZE, (float)mDisplayCenter[1]};
        skin->Draw2DLine(mCrosshairARGB, start, end);

        start = Vector2<float>{
            (float)mDisplayCenter[0], (float)mDisplayCenter[1] - CROSSHAIR_LINE_SIZE };
        end = Vector2<float>{
            (float)mDisplayCenter[0], (float)mDisplayCenter[1] + CROSSHAIR_LINE_SIZE };
        skin->Draw2DLine(mCrosshairARGB, start, end);
    }
}

void Hud::SetSelectionPosition(const Vector3<float>& pos, const Vector3<short>& cameraOffset)
{
    mCameraOffset = cameraOffset;

    mSelectionPos = pos;
    mSelectionPosWithOffset = pos - 
        Vector3<float>{cameraOffset[0] * BS, cameraOffset[1] * BS, cameraOffset[2] * BS};
}

void Hud::DrawSelectionMesh()
{
    if (mMode == HIGHLIGHT_BOX) 
    {
        // Draw 3D selection boxes
        for (auto& selectionBox : mSelectionBoxes) 
        {
            BoundingBox<float> bbox = BoundingBox<float>(
                selectionBox.mMinEdge + mSelectionPosWithOffset,
                selectionBox.mMaxEdge + mSelectionPosWithOffset);
            Vector3<float> size = bbox.GetExtent();

            unsigned int r = (mSelectionboxARGB.GetRed() * mSelectionMeshColor.GetRed() / 255);
            unsigned int g = (mSelectionboxARGB.GetGreen() * mSelectionMeshColor.GetGreen() / 255);
            unsigned int b = (mSelectionboxARGB.GetBlue() * mSelectionMeshColor.GetBlue() / 255);
            SColor color(255, r, g, b);

            struct Vertex
            {
                Vector3<float> position;
                Vector4<float> color;
            };
            VertexFormat vformat;
            vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
            vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

            MeshFactory mf;
            mf.SetVertexFormat(vformat);
            mf.SetVertexBufferUsage(Resource::DYNAMIC_UPDATE);
            std::shared_ptr<Visual> visual = mf.CreateBox(size[0], size[1], size[2]);

            // Multiply the texture coordinates by a factor to enhance the wrap-around.
            std::shared_ptr<VertexBuffer> vbuffer = visual->GetVertexBuffer();
            std::shared_ptr<IndexBuffer> ibuffer = visual->GetIndexBuffer();
            Vertex* vertex = vbuffer->Get<Vertex>();
            for (unsigned int idx = 0; idx != vbuffer->GetNumElements(); ++idx)
            {
                vertex[idx].position += bbox.GetCenter();
                vertex[idx].color = SColorF(color).ToArray();
            }

            ShaderInfo shader = static_cast<VisualEnvironment*>(
                mEnvironment)->GetShaderSource()->GetShaderInfo(mSelectionMaterial.mTypeParam2);
            std::shared_ptr<DefaultEffect> effect = std::make_shared<DefaultEffect>(
                ProgramFactory::Get()->CreateFromProgram(shader.visualProgram));
            visual->SetEffect(effect);

            auto blendState = std::make_shared<BlendState>();
            auto depthStencilState = std::make_shared<DepthStencilState>();
            auto rasterizerState = std::make_shared<RasterizerState>();
            if (mSelectionMaterial.Update(blendState))
                Renderer::Get()->Unbind(blendState);
            if (mSelectionMaterial.Update(depthStencilState))
                Renderer::Get()->Unbind(depthStencilState);
            if (mSelectionMaterial.Update(rasterizerState))
                Renderer::Get()->Unbind(rasterizerState);

            Renderer::Get()->SetBlendState(blendState);
            Renderer::Get()->SetDepthStencilState(depthStencilState);
            Renderer::Get()->SetRasterizerState(rasterizerState);

            effect->SetPVWMatrix(mScene->GetActiveCamera()->Get()->GetProjectionViewMatrix());
            Renderer::Get()->Update(effect->GetPVWMatrixConstant());
            Renderer::Get()->Update(visual->GetVertexBuffer());
            Renderer::Get()->Draw(visual);

            Renderer::Get()->SetDefaultBlendState();
            Renderer::Get()->SetDefaultDepthStencilState();
            Renderer::Get()->SetDefaultRasterizerState();
        }
    }
    else if (mMode == HIGHLIGHT_HALO && mSelectionMesh) 
    {
        // Draw selection mesh

        SetMeshColor(mSelectionMesh, mSelectionMeshColor);
        SColor faceColor(0,
            std::min(255, (int)(mSelectionMeshColor.GetRed() * 1.5)),
            std::min(255, (int)(mSelectionMeshColor.GetGreen() * 1.5)),
            std::min(255, (int)(mSelectionMeshColor.GetBlue() * 1.5)));
        SetMeshColorByNormal(mSelectionMesh, mSelectedFaceNormal, faceColor);
        std::shared_ptr<BaseMesh> mesh = CloneMesh(mSelectionMesh);
        TranslateMesh(mesh, mSelectionPosWithOffset);
        unsigned int mc = (unsigned int)mSelectionMesh->GetMeshBufferCount();
        for (unsigned int i = 0; i < mc; i++) 
        {
            std::shared_ptr<BaseMeshBuffer> meshBuffer = mesh->GetMeshBuffer(i);

            ShaderInfo shader = static_cast<VisualEnvironment*>(
                mEnvironment)->GetShaderSource()->GetShaderInfo(mSelectionMaterial.mTypeParam2);
            std::shared_ptr<SelectionEffect> effect = std::make_shared<SelectionEffect>(
                ProgramFactory::Get()->CreateFromProgram(shader.visualProgram),
                mSelectionMaterial.GetTexture(TT_DIFFUSE),
                mSelectionMaterial.mTextureLayer[TT_DIFFUSE].mFilter,
                mSelectionMaterial.mTextureLayer[TT_DIFFUSE].mModeU,
                mSelectionMaterial.mTextureLayer[TT_DIFFUSE].mModeV);

            std::shared_ptr<Visual> visual = std::make_shared<Visual>(
                meshBuffer->GetVertice(), meshBuffer->GetIndice(), effect);

            auto blendState = std::make_shared<BlendState>();
            auto depthStencilState = std::make_shared<DepthStencilState>();
            auto rasterizerState = std::make_shared<RasterizerState>();
            if (mSelectionMaterial.Update(blendState))
                Renderer::Get()->Unbind(blendState);
            if (mSelectionMaterial.Update(depthStencilState))
                Renderer::Get()->Unbind(depthStencilState);
            if (mSelectionMaterial.Update(rasterizerState))
                Renderer::Get()->Unbind(rasterizerState);

            Renderer::Get()->SetBlendState(blendState);
            Renderer::Get()->SetDepthStencilState(depthStencilState);
            Renderer::Get()->SetRasterizerState(rasterizerState);

            effect->SetPVWMatrix(mScene->GetActiveCamera()->Get()->GetProjectionViewMatrix());
            Renderer::Get()->Update(effect->GetPVWMatrixConstant());
            Renderer::Get()->Update(visual->GetVertexBuffer());
            Renderer::Get()->Draw(visual);

            Renderer::Get()->SetDefaultBlendState();
            Renderer::Get()->SetDefaultDepthStencilState();
            Renderer::Get()->SetDefaultRasterizerState();
        }
    }
}

void Hud::UpdateSelectionMesh(const Vector3<short>& cameraOffset)
{
    mCameraOffset = cameraOffset;
    if (mMode != HIGHLIGHT_HALO)
        return;

    if (mSelectionMesh)
        mSelectionMesh = NULL;

    if (mSelectionBoxes.empty()) 
    {
        // No pointed object
        return;
    }

    // New pointed object, create new mesh.

    // Texture UV coordinates for selection boxes
    static float textureUV[24] = 
    {
        0,0,1,1,
        0,0,1,1,
        0,0,1,1,
        0,0,1,1,
        0,0,1,1,
        0,0,1,1
    };

    // Use single halo box instead of multiple overlapping boxes.
    // Temporary solution - problem can be solved with multiple
    // rendering targets, or some method to remove inner surfaces.
    // Thats because of halo transparency.

    BoundingBox<float> haloBBox(100.0, 100.0, 100.0, -100.0, -100.0, -100.0);
    mHaloBoxes.clear();

    for (const auto& selectionBox : mSelectionBoxes)
        haloBBox.GrowToContain(selectionBox);

    mHaloBoxes.push_back(haloBBox);
    mSelectionMesh = ConvertNodeBoxesToMesh(mHaloBoxes, textureUV, 0.5);
}

void Hud::ResizeHotbar() 
{
    const Vector2<unsigned int>& windowSize = Renderer::Get()->GetScreenSize();

    if (mScreenSize != windowSize) 
    {
        mHotbarImageSize = (int)floor(HOTBAR_IMAGE_SIZE * System::Get()->GetDisplayDensity() + 0.5);
        mHotbarImageSize = (int)(mHotbarImageSize * mHudScaling);
        mPadding = mHotbarImageSize / 12;
        mScreenSize = windowSize;
        mDisplayCenter = Vector2<int>{ (int)mScreenSize[0] / 2, (int)mScreenSize[1] / 2 };
    }
}

struct MeshTimeInfo 
{
    unsigned int time;
    std::shared_ptr<BaseMesh> mesh = nullptr;
};

void DrawItemStack(BaseUI* ui, VisualEnvironment* env,
    const ItemStack& itemStack, const RectangleShape<2, int>& rect,
    const RectangleShape<2, int>* clip, ItemRotationKind rotationKind,
    const Vector3<short>& angle, const Vector3<short>& rotationSpeed)
{
    std::shared_ptr<BaseUISkin> skin = ui->GetSkin();
    if (!skin)
        return;

    static MeshTimeInfo rotationTimeInfos[IT_ROT_NONE];

    if (itemStack.IsEmpty()) 
    {
        if (rotationKind < IT_ROT_NONE && rotationKind != IT_ROT_OTHER)
            rotationTimeInfos[rotationKind].mesh = NULL;
        return;
    }

    const static thread_local bool enableAnimations =
        Settings::Get()->GetBool("inventory_items_animations");

    const Item& item = itemStack.GetDefinition(env->GetItemManager());

    // Render as mesh if animated or no inventory image
    if ((enableAnimations && rotationKind < IT_ROT_NONE) || item.inventoryImage.empty()) 
    {
        ItemMesh* imesh = env->GetItemManager()->GetWieldMesh(item.name, env);
        if (!imesh || !imesh->mesh)
            return;

        std::shared_ptr<BaseMesh> mesh = imesh->mesh;
        Renderer::Get()->ClearDepthBuffer();
        int delta = 0;
        if (rotationKind < IT_ROT_NONE) 
        {
            MeshTimeInfo& ti = rotationTimeInfos[rotationKind];
            if (mesh != ti.mesh && rotationKind != IT_ROT_OTHER) 
            {
                ti.mesh = mesh;
                ti.time = Timer::GetRealTime();
            }
            else delta = std::abs((int)(ti.time - Timer::GetRealTime())) % 100000;
        }

        int viewX, viewY, viewW, viewH;
        Renderer::Get()->GetViewport(viewX, viewY, viewW, viewH);

        RectangleShape<2, int> viewRect = rect;
        if (clip)
            viewRect.ClipAgainst(*clip);

        bool isDepthRangeZeroToOne = true;
#if defined(_OPENGL_)
        isDepthRangeZeroToOne = false;
#endif

        ViewVolume viewVolume = ViewVolume(false, isDepthRangeZeroToOne);
        viewVolume.SetFrustum(-1.0f, 100.0f,
            -1.f * viewRect.mExtent[1] / rect.mExtent[1], 1.f * viewRect.mExtent[1] / rect.mExtent[1], 
            -1.f * viewRect.mExtent[0] / rect.mExtent[0], 1.f * viewRect.mExtent[0] / rect.mExtent[0]);

        Vector2<int> rectLowerRight = rect.GetVertice(RVP_LOWERRIGHT);
        Vector2<int> rectUpperLeft = rect.GetVertice(RVP_UPPERLEFT);
        Vector2<int> viewLowerRight = viewRect.GetVertice(RVP_LOWERRIGHT);
        Vector2<int> viewUpperLeft = viewRect.GetVertice(RVP_UPPERLEFT);
        
        viewVolume.SetPosition(Vector4<float>{
            1.0f * (rectLowerRight[0] + rectUpperLeft[0] - viewLowerRight[0] - viewUpperLeft[0]) / viewRect.mExtent[0],
            1.0f * (viewLowerRight[1] + viewUpperLeft[1] - rectLowerRight[1] - rectUpperLeft[1]) / viewRect.mExtent[1],
            0.f, 0.f});

        Transform worldTransform;
        if (enableAnimations) 
        {
            float timer = (float)delta / 5000.f;

            float yaw = angle[1] + rotationSpeed[1] * 3.60f * timer;
            float pitch = angle[2] + rotationSpeed[2] * 3.60f * timer;
            float roll = angle[0] + rotationSpeed[0] * 3.60f * timer;

            Matrix4x4<float> yawRotation = Rotation<4, float>(
                AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), yaw));
            Matrix4x4<float> pitchRotation = Rotation<4, float>(
                AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), pitch));
            Matrix4x4<float> rollRotation = Rotation<4, float>(
                AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), roll));
            worldTransform.SetRotation(yawRotation * pitchRotation * rollRotation);
        }

        Matrix4x4<float> pvw = viewVolume.GetProjectionViewMatrix() * worldTransform.GetMatrix();

        Vector2<int> viewSize = viewRect.mExtent;
        Vector2<int> viewOrigin = viewRect.GetVertice(RVP_UPPERLEFT);
        Renderer::Get()->SetViewport(viewOrigin[0], viewOrigin[1], viewSize[0], viewSize[1]);

        SColor baseColor = env->GetItemManager()->GetItemstackColor(itemStack, env);

        unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
        for (unsigned int j = 0; j < mc; ++j) 
        {
            std::shared_ptr<BaseMeshBuffer> meshBuffer = mesh->GetMeshBuffer(j);
            // we can modify vertices relatively fast,
            // because these meshes are not buffered.
            SColor color = baseColor;

            if (imesh->bufferColors.size() > j) 
            {
                ItemPartColor* p = &imesh->bufferColors[j];
                if (p->overrideBase)
                    color = p->color;
            }

            if (imesh->needsShading)
                ColorizeMeshBuffer(meshBuffer, &color);
            else
                SetMeshBufferColor(meshBuffer, color);

            // Create an effect for the vertex and pixel shaders. The texture is
            // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
            std::vector<std::string> path;
#if defined(_OPENGL_)
            path.push_back("Effects/Texture2ColorEffectVS.glsl");
            path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
            path.push_back("Effects/Texture2ColorEffectVS.hlsl");
            path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
            std::shared_ptr<ResHandle> resHandle =
                ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

            const std::shared_ptr<ShaderResourceExtraData>& extra =
                std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
            if (!extra->GetProgram())
                extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

            auto effect = std::make_shared<Texture2Effect>(ProgramFactory::Get()->CreateFromProgram(
                extra->GetProgram()), meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE),
                SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

            std::shared_ptr<ConstantBuffer> cbuffer;
            cbuffer = effect->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
            *cbuffer->Get<Matrix4x4<float>>() = pvw;

            std::shared_ptr<Material>& material = meshBuffer->GetMaterial();
            material->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;
            material->mLighting = false;

            material->mBlendTarget.enable = true;
            material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
            material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
            material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
            material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

            material->mDepthBuffer = true;
            material->mDepthMask = DepthStencilState::MASK_ALL;

            material->mFillMode = RasterizerState::FILL_SOLID;
            material->mCullMode = RasterizerState::CULL_NONE;

            std::shared_ptr<Visual> visual = std::make_shared<Visual>(
                meshBuffer->GetVertice(), meshBuffer->GetIndice(), effect);

            auto blendState = std::make_shared<BlendState>();
            auto depthStencilState = std::make_shared<DepthStencilState>();
            auto rasterizerState = std::make_shared<RasterizerState>();
            if (material->Update(blendState))
                Renderer::Get()->Unbind(blendState);
            if (material->Update(depthStencilState))
                Renderer::Get()->Unbind(depthStencilState);
            if (material->Update(rasterizerState))
                Renderer::Get()->Unbind(rasterizerState);

            Renderer::Get()->SetBlendState(blendState);
            Renderer::Get()->SetDepthStencilState(depthStencilState);
            Renderer::Get()->SetRasterizerState(rasterizerState);

            Renderer::Get()->Update(visual->GetVertexBuffer());
            Renderer::Get()->Update(cbuffer);
            Renderer::Get()->Draw(visual);

            Renderer::Get()->SetDefaultBlendState();
            Renderer::Get()->SetDefaultDepthStencilState();
            Renderer::Get()->SetDefaultRasterizerState();
        }

        // restore the view area
        Renderer::Get()->SetViewport(viewX, viewY, viewW, viewH);
    }
    else 
    { 
        // Otherwise just draw as 2D
        std::shared_ptr<Texture2> texture = 
            env->GetItemManager()->GetInventoryTexture(item.name, env);
        if (!texture)
            return;
        SColor color = env->GetItemManager()->GetItemstackColor(itemStack, env);
        const SColor colors[] = { color, color, color, color };

        Vector2<unsigned int> textureSize =
            env->GetTextureSource()->GetTextureOriginalSize(env->GetTextureSource()->GetTextureId(texture));

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent = { (int)textureSize[0], (int)textureSize[1] };
        tcoordRect.mCenter = tcoordRect.mExtent / 2;

        // Create a vertex buffer for a two-triangles square. The PNG is stored
        // in left-handed coordinates. The texture coordinates are chosen to
        // reflect the texture in the y-direction.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        // Create an effect for the vertex and pixel shaders. The texture is
        // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        auto effect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), texture,
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

        // Create the geometric object for drawing.
        std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);

        auto blendState = std::make_shared<BlendState>();
        blendState->mTarget[0].enable = true;
        blendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        blendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        blendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        blendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        Renderer::Get()->SetBlendState(blendState);

        skin->Draw2DTextureFilterScaled(visual, rect, tcoordRect, colors);

        Renderer::Get()->SetDefaultBlendState();
    }

    // draw the inventory_overlay
    if (item.type == ITEM_NODE && item.inventoryImage.empty() && !item.inventoryOverlay.empty()) 
    {
        std::shared_ptr<Texture2> overlayTexture = 
            env->GetTextureSource()->GetTexture(item.inventoryOverlay);
        Vector2<unsigned int> textureSize =
            env->GetTextureSource()->GetTextureOriginalSize(env->GetTextureSource()->GetTextureId(overlayTexture));

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent = { (int)textureSize[0], (int)textureSize[1] };
        tcoordRect.mCenter = tcoordRect.mExtent / 2;

        // Create a vertex buffer for a two-triangles square. The PNG is stored
        // in left-handed coordinates. The texture coordinates are chosen to
        // reflect the texture in the y-direction.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        // Create an effect for the vertex and pixel shaders. The texture is
        // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        auto effect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), overlayTexture,
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

        // Create the geometric object for drawing.
        std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);

        skin->Draw2DTextureFilterScaled(visual, rect, tcoordRect);
    }

    if (item.type == ITEM_TOOL && itemStack.wear != 0) 
    {
        // Draw a progressbar
        float barheight = static_cast<float>(rect.mExtent[1]) / 16;
        float barpadX = static_cast<float>(rect.mExtent[0]) / 16;
        float barpadY = static_cast<float>(rect.mExtent[1]) / 16;

        Vector2<int> upperLeft = rect.GetVertice(RVP_UPPERLEFT);
        Vector2<int> lowerRight = rect.GetVertice(RVP_LOWERRIGHT);

        RectangleShape<2, int> progressRect;
        progressRect.mExtent[0] = (int)((lowerRight[0] - barpadX) - (upperLeft[0] + barpadX));
        progressRect.mExtent[1] = (int)barheight;
        progressRect.mCenter[0] = (int)((upperLeft[0] + barpadX) + progressRect.mExtent[0] / 2);
        progressRect.mCenter[1] = (int)((lowerRight[1] - barpadY) - progressRect.mExtent[1] / 2);

        // Shrink progressRect by amount of tool damage
        float wear = itemStack.wear / 65535.f;
        int progressmid = (int)
            (wear * progressRect.GetVertice(RVP_UPPERLEFT)[0] +
            (1 - wear) * progressRect.GetVertice(RVP_LOWERRIGHT)[0]);

        // Compute progressbar color
        //   wear = 0.0: green
        //   wear = 0.5: yellow
        //   wear = 1.0: red
        SColor color(255, 255, 255, 255);
        int wearValue = std::min((int)std::floor(wear * 600), 511);
        wearValue = std::min(wearValue + 10, 511);

        if (wearValue <= 255)
            color.Set(255, wearValue, 255, 0);
        else
            color.Set(255, 255, 511 - wearValue, 0);

        upperLeft = progressRect.GetVertice(RVP_UPPERLEFT);
        lowerRight = progressRect.GetVertice(RVP_LOWERRIGHT);

        // Create a vertex buffer for a single triangle.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/ColorEffectVS.glsl");
        path.push_back("Effects/ColorEffectPS.glsl");
#else
        path.push_back("Effects/ColorEffectVS.hlsl");
        path.push_back("Effects/ColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

        // Create the geometric object for drawing.
        std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);

        RectangleShape<2, int> progressRect2 = progressRect;
        progressRect2.mExtent[0] = progressmid - upperLeft[0];
        progressRect2.mCenter[0] = upperLeft[0] + progressRect2.mExtent[0] / 2;

        skin->Draw2DRectangle(color, visual, progressRect2, clip);

        effect = std::make_shared<ColorEffect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

        // Create the geometric object for drawing.
        visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);

        color = SColor(255, 0, 0, 0);
        progressRect2 = progressRect;
        progressRect2.mExtent[0] = lowerRight[0] - progressmid;
        progressRect2.mCenter[0] = progressmid + progressRect2.mExtent[0] / 2;

        skin->Draw2DRectangle(color, visual, progressRect2, clip);
    }

    if (itemStack.count >= 2) 
    {
        // Get the item count as a string
        std::shared_ptr<BaseUIFont> font = skin->GetFont();

        std::string text = std::to_string(itemStack.count);
        Vector2<int> dim = font->GetDimension(ToWideString(text));

        //check if it is valid rectangle
        if (dim >= rect.GetVertice(RVP_LOWERRIGHT) - dim)
        {
            RectangleShape<2, int> rect2;
            rect2.mExtent = rect.GetVertice(RVP_LOWERRIGHT);
            rect2.mCenter = dim + rect2.mExtent / 2;

            // Create a vertex buffer for a single triangle.
            VertexFormat vformat;
            vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
            vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

            std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
            std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
            vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

            std::vector<std::string> path;
#if defined(_OPENGL_)
            path.push_back("Effects/ColorEffectVS.glsl");
            path.push_back("Effects/ColorEffectPS.glsl");
#else
            path.push_back("Effects/ColorEffectVS.hlsl");
            path.push_back("Effects/ColorEffectPS.hlsl");
#endif
            std::shared_ptr<ResHandle> resHandle =
                ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

            const std::shared_ptr<ShaderResourceExtraData>& extra =
                std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
            if (!extra->GetProgram())
                extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

            std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
                ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

            // Create the geometric object for drawing.
            std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);

            SColor bgcolor(128, 0, 0, 0);
            skin->Draw2DRectangle(bgcolor, visual, rect2, clip);

            SColor color(255, 255, 255, 255);
            font->Draw(ToWideString(text), rect2, color, false, false, clip);
        }
        else
        {
            RectangleShape<2, int> rect2;
            rect2.mExtent = dim;
            rect2.mCenter = rect.GetVertice(RVP_LOWERRIGHT) - dim / 2;

            SColor color(255, 255, 255, 255);
            font->Draw(ToWideString(text), rect2, color, false, false, clip);
        }

    }
}

void DrawItemStack(BaseUI* ui, VisualEnvironment* env,
    const ItemStack& item, const RectangleShape<2, int>& rect,  
    const RectangleShape<2, int>* clip, ItemRotationKind rotationKind)
{
    DrawItemStack(ui, env, item, rect, clip, rotationKind,
        Vector3<short>::Zero(), Vector3<short>{0, 100, 0});
}
