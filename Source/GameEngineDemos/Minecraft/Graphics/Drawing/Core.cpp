/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

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

#include "Core.h"

#include "../../Games/Environment/VisualEnvironment.h"
#include "../../Graphics/Actors/VisualPlayer.h"

#include "../Map/VisualMap.h"
#include "../Map/Minimap.h"
#include "../PlayerCamera.h"
#include "../Hud.h"

DrawingCore::DrawingCore(BaseUI* ui, VisualEnvironment* vEnv, Scene* pScene, Hud* hud)
	: mScene(pScene), mVisualEnv(vEnv), mHud(hud), mUI(ui)
{
	mMinimap = mVisualEnv->GetMinimap();
	mPlayerCamera = mVisualEnv->GetPlayerCamera();
    mScreenSize = Renderer::Get()->GetScreenSize();
	mVirtualSize = mScreenSize;
}

DrawingCore::~DrawingCore()
{
	ClearTextures();
}

void DrawingCore::Initialize()
{
	// have to be called late as the VMT is not ready in the constructor:
	InitTextures();
}

void DrawingCore::UpdateScreenSize()
{
	mVirtualSize = mScreenSize;
	ClearTextures();
	InitTextures();
}

void DrawingCore::Draw(SColor skyColor, 
	bool showHud, bool showMinimap, bool drawWieldTool, bool drawCrosshair)
{
	Vector2<unsigned int> ss = Renderer::Get()->GetScreenSize();
	if (mScreenSize != ss) 
    {
        mScreenSize = ss;
		UpdateScreenSize();
	}
	mSkyColor = skyColor;
	mShowHud = showHud;
	mShowMinimap = showMinimap;
	mDrawWieldTool = drawWieldTool;
	mDrawCrosshair = drawCrosshair;

	PreRender();
	OnRender();
}

void DrawingCore::Render3D()
{
	mScene->OnRender();
	if (!mShowHud)
		return;

	mHud->DrawSelectionMesh();
	if (mDrawWieldTool)
        mPlayerCamera->DrawWieldedTool();
}

void DrawingCore::RenderHUD()
{
	if (mShowHud) 
    {
		if (mDrawCrosshair)
			mHud->DrawCrosshair();
	
        mHud->DrawHotbar(mVisualEnv->GetPlayer()->GetWieldIndex());
        mHud->DrawElements(mPlayerCamera->GetOffset());
        mPlayerCamera->DrawNametags();
		if (mMinimap && mShowMinimap)
            mMinimap->DrawMinimap(mUI);
	}
}

void DrawingCore::RenderPostFx()
{
    mVisualEnv->GetVisualMap()->RenderPostFx(mUI, mPlayerCamera->GetCameraMode());
}
