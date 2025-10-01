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

#ifndef DRAWINGCORE_H
#define DRAWINGCORE_H

#include "GameEngineStd.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/UI/UIEngine.h"

#include "Mathematic/Algebra/Vector2.h"

class VisualEnvironment;
class PlayerCamera;
class Minimap;
class Hud;

class DrawingCore
{
public:
	DrawingCore(BaseUI* ui, VisualEnvironment* vEnv, Scene* pScene, Hud* hud);
	DrawingCore(const DrawingCore&) = delete;
	DrawingCore(DrawingCore&&) = delete;
	virtual ~DrawingCore();

	DrawingCore &operator=(const DrawingCore&) = delete;
	DrawingCore &operator=(DrawingCore&&) = delete;

	void Initialize();
	void Draw(SColor skyColor, bool showHud, bool showMinimap,
			bool drawWieldTool, bool drawCrosshair);

	inline Vector2<unsigned int> GetVirtualSize() const { return mVirtualSize; }

protected:
    Vector2<unsigned int> mScreenSize;
    Vector2<unsigned int> mVirtualSize;
    SColor mSkyColor;
    bool mShowHud;
    bool mShowMinimap;
    bool mDrawWieldTool;
    bool mDrawCrosshair;

    Scene* mScene;
    BaseUI* mUI;

    VisualEnvironment* mVisualEnv;
    PlayerCamera* mPlayerCamera;
    Minimap* mMinimap;
    Hud* mHud;

    void UpdateScreenSize();
    virtual void InitTextures() {}
    virtual void ClearTextures() {}

    virtual void PreRender() {}
    virtual void OnRender() = 0;

    void Render3D();
    void RenderHUD();
    void RenderPostFx();
};

#endif