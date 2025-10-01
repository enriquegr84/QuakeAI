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

#ifndef TILEPARAMS_H
#define TILEPARAMS_H

#include "Graphic/Resource/Color.h"
#include "Graphic/Effect/Material.h"
#include "Graphic/Resource/Texture/Texture2.h"

#include "Mathematic/Algebra/Vector2.h"

enum TileAnimationType
{
    TAT_NONE = 0,
    TAT_VERTICAL_FRAMES = 1,
    TAT_SHEET_2D = 2,
};

extern std::map<std::string, unsigned int> TileAnimationTypes;

struct TileAnimationParams
{
    enum TileAnimationType type;
    union
    {
        struct
        {
            int aspectWidth; // width for aspect ratio
            int aspectHeight; // height for aspect ratio
            float length; // seconds
        } verticalFrames;
        struct
        {
            int framesWidth;       // number of frames left-to-right
            int framesHeight;       // number of frames top-to-bottom
            float frameLength; // seconds
        } sheet2D;
    };

    void Serialize(std::ostream& os) const;
    void Deserialize(std::istream& is);
    void DetermineParams(Vector2<unsigned int> textureSize, int* frameCount,
        int* frameLengthMs, Vector2<unsigned int>* frameSize) const;
    void GetTextureModifer(std::ostream& os, Vector2<unsigned int> textureSize, int frame) const;
    Vector2<float> GetTextureCoords(Vector2<unsigned int> textureSize, int frame) const;
};

typedef std::vector<SColor> Palette;

#endif