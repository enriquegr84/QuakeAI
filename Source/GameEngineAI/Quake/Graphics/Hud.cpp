//========================================================================
// Hud.cpp : Hud Class
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
//    http://code.google.com/p/gamecode4/
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

#include "Hud.h"

#include "../Games/Actors/PlayerActor.h"

#include "Core/OS/OS.h"
#include "Core/Utility/StringUtil.h"

#include "Graphic/Renderer/Renderer.h"

#include "Application/Settings.h"
#include "Application/System/System.h"

#include "Application/GameApplication.h"

#define STAT_MINUS			10	// num frame for '-' stats digit

#define OBJECT_CROSSHAIR_LINE_SIZE 8
#define CROSSHAIR_LINE_SIZE 10


Hud::Hud(Scene* pScene, BaseUI* ui) : mUI(ui)
{
    mScene = pScene;

    mScreenSize = Renderer::Get()->GetScreenSize();
    mHudScaling = Settings::Get()->GetFloat("hud_scaling");
    mScaleFactor = mHudScaling * System::Get()->GetDisplayDensity();
    mDisplayCenter = Vector2<int>{ (int)mScreenSize[0] / 2, (int)mScreenSize[1] / 2 };

    for (auto& hbarColor : mHBarColors)
        hbarColor = SColor(255, 255, 255, 255);

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
    mSelectionMaterial.mType = MT_TRANSPARENT_ALPHA_CHANNEL;

    if (mMode == HIGHLIGHT_BOX) 
    {
        mSelectionMaterial.mThickness =
            (float)std::clamp(Settings::Get()->GetInt("selectionbox_width"), 1, 5);
    }
    else if (mMode == HIGHLIGHT_HALO) 
    {
        if (FileSystem::Get()->ExistFile(ToWideString("halo.png")))
        {
            std::shared_ptr<ResHandle> resHandle =
                ResCache::Get()->GetHandle(&BaseResource(ToWideString("halo.png")));
            if (resHandle)
            {
                std::shared_ptr<ImageResourceExtraData> resData =
                    std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
                if (resData)
                {
                    resData->GetImage()->AutogenerateMipmaps();
                    mSelectionMaterial.SetTexture(0, resData->GetImage());
                }
            }
        }
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
        if (resHandle)
        {
            const std::shared_ptr<ShaderResourceExtraData>& extra =
                std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
            if (!extra->GetProgram())
                extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

            std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
                ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

            // Create the geometric object for drawing.
            mVisualBackground = std::make_shared<Visual>(vbuffer, ibuffer, effect);
        }
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
        if (resHandle)
        {
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
    }
}

Hud::~Hud()
{

}

void Hud::DrawElements(const std::shared_ptr<PlayerActor>& player)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    std::shared_ptr<BaseUIFont> textFont = mUI->GetBuiltInFont();
    int textHeight = textFont->GetDimension(L"Ay")[1];

    // Reorder elements by z_index
    std::vector<HudElement*> elems;
    elems.reserve(player->MaxHudId());

    for (unsigned int i = 0; i != player->MaxHudId(); i++)
    {
        HudElement* el = player->GetHud(i);
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
                if (el->item == STAT_SCORE)
                {
                    int scoreBlue = 0, scoreRed = 0;

                    GameApplication* gameApp = (GameApplication*)Application::App;
                    const GameViewList& gameViews = gameApp->GetGameViews();
                    for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
                    {
                        std::shared_ptr<BaseGameView> pView = *it;
                        if (pView->GetType() == GV_AI && pView->GetActorId() != INVALID_ACTOR_ID)
                        {
                            if (gameApp->GetHumanView()->GetActorId() == pView->GetActorId())
                            {
                                auto playerBlue = std::dynamic_pointer_cast<PlayerActor>(
                                    GameLogic::Get()->GetActor(pView->GetActorId()).lock());
                                scoreBlue += playerBlue->GetState().persistant[PERS_SCORE];
                            }
                            else
                            {
                                auto playerRed = std::dynamic_pointer_cast<PlayerActor>(
                                    GameLogic::Get()->GetActor(pView->GetActorId()).lock());
                                scoreRed += playerRed->GetState().persistant[PERS_SCORE];
                            }
                        }
                    }

                    Vector2<int> offs{ (int)el->offset[0], (int)el->offset[1] };
                    el->text = std::to_string(scoreBlue);
                    el->text2 = std::to_string(scoreRed);
                    SColor bgColor(255, 0, 0, 255);
                    DrawScoreStat(pos, 0, el->direction, bgColor, el->text, el->item, offs, el->size);
                    bgColor = SColor(255, 255, 0, 0);
                    DrawScoreStat(pos, 1, el->direction, bgColor, el->text2, el->item, offs, el->size);
                    break;
                }
                else if (el->item == STAT_AMMO)
                {
                    if (player->GetState().weapon == WP_GAUNTLET)
                        el->number = 0;
                    else
                        el->number = player->GetState().ammo[player->GetState().weapon];
                }
                else if (el->item == STAT_HEALTH)
                    el->number = player->GetState().stats[STAT_HEALTH];
                else if (el->item == STAT_ARMOR)
                    el->number = player->GetState().stats[STAT_ARMOR];
                else break;
                
                Vector2<int> offs{ (int)el->offset[0], (int)el->offset[1] };
                DrawStatbar(pos, HUD_CORNER_UPPER, el->direction, 
                    el->text, el->number, el->item, offs, el->size);
                break; 
            }
            case HUD_ELEM_IMAGE: 
            {
                std::shared_ptr<ResHandle> resHandle = 
                    ResCache::Get()->GetHandle(&BaseResource(ToWideString(el->text.c_str())));
                if (!resHandle)
                    continue;

                std::shared_ptr<ImageResourceExtraData> resData =
                    std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
                std::shared_ptr<Texture2> texture = resData->GetImage();
                texture->AutogenerateMipmaps();

                const SColor color(255, 255, 255, 255);
                const SColor colors[] = { color, color, color, color };

                Vector2<unsigned int> imgSize = { texture->GetDimension(0), texture->GetDimension(1)};
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
            default:
                LogInformation("Hud::DrawElements: ignoring drawform " + 
                    std::to_string(el->type) + " due to unrecognized type");
                break;
        }
    }
}

bool Hud::HasElementOfType(const std::shared_ptr<PlayerActor>& player, HudElementType type)
{
    for (unsigned int i = 0; i != player->MaxHudId(); i++)
    {
        HudElement* el = player->GetHud(i);
        if (!el)
            continue;
        if (el->type == type)
            return true;
    }
    return false;
}

void Hud::DrawScoreStat(Vector2<int> pos, int step, uint16_t drawdir, SColor bgColor, 
    const std::string& score, int item, Vector2<int> offset, Vector2<int> size)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    std::shared_ptr<BaseUIFont> textFont = mUI->GetBuiltInFont();
    int textHeight = textFont->GetDimension(L"Ay")[1];

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    Vector2<int> dstd;
    dstd[1] = (int)(size[1] * mScaleFactor);
    dstd[0] = (int)(size[0] * mScaleFactor);

    offset[0] = (int)(offset[0] * mScaleFactor);
    offset[1] = (int)(offset[1] * mScaleFactor);

    Vector2<int> stepPos;
    switch (drawdir)
    {
        case HUD_DIR_RIGHT_LEFT:
            stepPos = Vector2<int>{ -1, 0 };
            break;
        case HUD_DIR_TOP_BOTTOM:
            stepPos = Vector2<int>{ 0, 1 };
            break;
        case HUD_DIR_BOTTOM_TOP:
            stepPos = Vector2<int>{ 0, -1 };
            break;
        default:
            // From left to right
            stepPos = Vector2<int>{ 1, 0 };
            break;
    }
    stepPos[0] *= offset[0];
    stepPos[1] *= offset[1];

    Renderer::Get()->SetBlendState(mBlendState);

    Vector2<int> p = pos;
    p += step * stepPos;

    //draw score
    {
        RectangleShape<2, int> dstRect;
        dstRect.mExtent = dstd;
        dstRect.mCenter = -dstd;
        dstRect.mCenter += p;

        // draw background
        skin->Draw2DRectangle(bgColor, mVisualBackground, dstRect, nullptr);

        SColor color(255, 255, 255, 255);
        textFont->Draw(ToWideString(score), dstRect, color, false, false);
    }

    Renderer::Get()->SetDefaultBlendState();
}

void Hud::DrawStatbar(
    Vector2<int> pos, uint16_t corner, uint16_t drawdir,
    const std::string& texture, int number, int item, 
    Vector2<int> offset, Vector2<int> size)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    Vector2<int> screenSize{ (int)mScreenSize[0], (int)mScreenSize[1] };

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    Vector2<int> srcd = size;
    Vector2<int> dstd;
    dstd[1] = (int)(size[1] * mScaleFactor);
    dstd[0] = (int)(size[0] * mScaleFactor);

    offset[0] = (int)(offset[0] * mScaleFactor);
    offset[1] = (int)(offset[1] * mScaleFactor);

    Vector2<int> stepPos;
    switch (drawdir) 
    {
        case HUD_DIR_RIGHT_LEFT:
            stepPos = Vector2<int>{ -1, 0 };
            break;
        case HUD_DIR_TOP_BOTTOM:
            stepPos = Vector2<int>{ 0, 1 };
            break;
        case HUD_DIR_BOTTOM_TOP:
            stepPos = Vector2<int>{ 0, -1 };
            break;
        default:
            // From left to right
            stepPos = Vector2<int>{ 1, 0 };
            break;
    }
    stepPos[0] *= offset[0];
    stepPos[1] *= offset[1];

    Renderer::Get()->SetBlendState(mBlendState);

    // Draw texture numbers
    char num[16];
    sprintf(num, "%i", std::clamp(number, -999, 999));
    char* sbNums[11] =
    {
        "art/quake/gfx/2d/numbers/zero_32b.png",
        "art/quake/gfx/2d/numbers/one_32b.png",
        "art/quake/gfx/2d/numbers/two_32b.png",
        "art/quake/gfx/2d/numbers/three_32b.png",
        "art/quake/gfx/2d/numbers/four_32b.png",
        "art/quake/gfx/2d/numbers/five_32b.png",
        "art/quake/gfx/2d/numbers/six_32b.png",
        "art/quake/gfx/2d/numbers/seven_32b.png",
        "art/quake/gfx/2d/numbers/eight_32b.png",
        "art/quake/gfx/2d/numbers/nine_32b.png",
        "art/quake/gfx/2d/numbers/minus_32b.png",
    };

    int n = (int)strlen(num);
    Vector2<int> p = pos;
    if (n < 3) p += (3 - n) * stepPos;

    for (n = 0; n < strlen(num); n++)
    {
        RectangleShape<2, int> srcRect;
        srcRect.mExtent = srcd;
        srcRect.mCenter = srcd / 2;

        RectangleShape<2, int> dstRect;
        dstRect.mExtent = dstd;
        dstRect.mCenter = -dstd;
        dstRect.mCenter += p;

        if (num[n] == '-')
        {
            std::shared_ptr<ResHandle> resHandle = ResCache::Get()->GetHandle(
                &BaseResource(ToWideString(sbNums[STAT_MINUS])));
            if (resHandle)
            {
                std::shared_ptr<ImageResourceExtraData> resData =
                    std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());

                auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
                effect->SetTexture(resData->GetImage());
            }
        }
        else
        {
            std::shared_ptr<ResHandle> resHandle = ResCache::Get()->GetHandle(
                &BaseResource(ToWideString(sbNums[num[n] - '0'])));
            if (resHandle)
            {
                std::shared_ptr<ImageResourceExtraData> resData =
                    std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());

                auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
                effect->SetTexture(resData->GetImage());
            }
        }
        skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);

        p += stepPos;
    }

    //draw icon
    {
        p += stepPos;

        srcd = Vector2<int>{ICON_SIZE, ICON_SIZE};
        dstd[1] = (int)(srcd[1] * mScaleFactor);
        dstd[0] = (int)(srcd[0] * mScaleFactor);

        RectangleShape<2, int> srcRect;
        srcRect.mExtent = srcd;
        srcRect.mCenter = srcd / 2;

        RectangleShape<2, int> dstRect;
        dstRect.mExtent = dstd;
        dstRect.mCenter = -dstd;
        dstRect.mCenter += p;

        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(texture)));
        if (resHandle)
        {
            std::shared_ptr<ImageResourceExtraData> resData =
                std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());

            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
            effect->SetTexture(resData->GetImage());
            skin->Draw2DTextureFilterScaled(mVisual, dstRect, srcRect, colors);
        }
    }

    Renderer::Get()->SetDefaultBlendState();
}

void Hud::DrawCrosshair(const std::wstring& crosshair)
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    std::shared_ptr<ResHandle> resHandle = ResCache::Get()->GetHandle(&BaseResource(crosshair));
    if (resHandle)
    {
        std::shared_ptr<ImageResourceExtraData> resData =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        std::shared_ptr<Texture2> crosshair = resData->GetImage();
        crosshair->AutogenerateMipmaps();

        RectangleShape<2, int> dstRect;
        dstRect.mExtent = { (int)crosshair->GetDimension(0), (int)crosshair->GetDimension(1)};
        dstRect.mCenter = mDisplayCenter;

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent = { (int)crosshair->GetDimension(0), (int)crosshair->GetDimension(1) };
        tcoordRect.mCenter = tcoordRect.mExtent / 2;

        Renderer::Get()->SetBlendState(mBlendState);

        auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
        effect->SetTexture(crosshair);
        skin->Draw2DTexture(mVisual, dstRect, tcoordRect);

        Renderer::Get()->SetDefaultBlendState();
    }
}

void Hud::SetSelectionPosition(const Vector3<float>& pos, const Vector3<short>& cameraOffset)
{
    mCameraOffset = cameraOffset;

    mSelectionPos = pos;
    mSelectionPosWithOffset = pos - 
        Vector3<float>{cameraOffset[0] * 10.f, cameraOffset[1] * 10.f, cameraOffset[2] * 10.f};
}

void Hud::DrawSelectionMesh()
{
    /*
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
        unsigned int mc = mSelectionMesh->GetMeshBufferCount();
        for (unsigned int i = 0; i < mc; i++) 
        {
            std::shared_ptr<BaseMeshBuffer> meshBuffer = mesh->GetMeshBuffer(i);

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
    */
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
    //mSelectionMesh = ConvertNodeBoxesToMesh(mHaloBoxes, textureUV, 0.5);
}