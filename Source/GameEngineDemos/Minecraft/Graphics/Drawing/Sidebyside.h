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


#ifndef SIDEBYSIDE_H
#define SIDEBYSIDE_H

#include "Stereo.h"

class DrawingCoreSideBySide : public DrawingCoreStereo
{
public:
	DrawingCoreSideBySide(BaseUI* ui, VisualEnvironment* vEnv, 
        Scene* pScene, Hud* hud, bool horizontal = false, bool flipped = false);
	void OnRender() override;

protected:
    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<Texture2Effect> mEffect;
    std::shared_ptr<DrawTarget> mDrawTarget;
    std::shared_ptr<Texture2> mTexture;

    bool mHorizontal = false;
    bool mFlipped = false;
    Vector2<unsigned int> mImageSize;
    RectangleShape<2, int> mRectanglePos;

    void InitTextures() override;
    void ClearTextures() override;
    void UseEye(bool right) override;
    void ResetEye() override;
};

#endif