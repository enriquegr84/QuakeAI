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

#ifndef STEREO_H
#define STEREO_H

#include "Core.h"

class DrawingCoreStereo : public DrawingCore
{
public:
	DrawingCoreStereo(BaseUI* ui, VisualEnvironment* vEnv, Scene* pScene, Hud* hud);

protected:

    std::shared_ptr<CameraNode> mCamera;
    Transform mBaseTransform;
    float mEyeOffset;

    void PreRender() override;
    virtual void UseEye(bool right);
    virtual void ResetEye();
    void RenderImages();
};

#endif