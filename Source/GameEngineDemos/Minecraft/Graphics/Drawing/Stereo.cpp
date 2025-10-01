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

#include "../../MinecraftStd.h"

#include "Stereo.h"

#include "../PlayerCamera.h"

#include "Application/Settings.h"

DrawingCoreStereo::DrawingCoreStereo(BaseUI* ui, VisualEnvironment* vEnv, Scene* pScene, Hud* hud)
	: DrawingCore(ui, vEnv, pScene, hud)
{
	mEyeOffset = BS * Settings::Get()->GetFloat("paralax3d_strength");
}

void DrawingCoreStereo::PreRender()
{
	mCamera = mPlayerCamera->GetCameraNode();
	mBaseTransform = mCamera->GetRelativeTransform();
}

void DrawingCoreStereo::UseEye(bool right)
{
	Transform move;
    move.MakeIdentity();
	move.SetTranslation(
        Vector3<float>{right ? mEyeOffset : -mEyeOffset, 0.0f, 0.0f});
	mCamera->GetRelativeTransform().SetTranslation((mBaseTransform * move).GetTranslation());
}

void DrawingCoreStereo::ResetEye()
{
    mCamera->GetRelativeTransform().SetTranslation(mBaseTransform.GetTranslation());
}

void DrawingCoreStereo::RenderImages()
{
	UseEye(false);
	Render3D();
	ResetEye();
	UseEye(true);
	Render3D();
	ResetEye();
}
