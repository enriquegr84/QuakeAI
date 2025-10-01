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

#include "TileParams.h"

#include "Core/Utility/Serialize.h"
#include "Core/Utility/StringUtil.h"

std::map<std::string, unsigned int> TileAnimationTypes =
{
    {"none", TAT_NONE},
    {"verticalframes", TAT_VERTICAL_FRAMES},
    {"sheet2d", TAT_SHEET_2D}
};

void TileAnimationParams::Serialize(std::ostream& os) const
{
    WriteUInt8(os, type);
    if (type == TAT_VERTICAL_FRAMES) 
    {
        WriteUInt16(os, verticalFrames.aspectWidth);
        WriteUInt16(os, verticalFrames.aspectHeight);
        WriteFloat(os, verticalFrames.length);
    }
    else if (type == TAT_SHEET_2D) 
    {
        WriteUInt8(os, sheet2D.framesWidth);
        WriteUInt8(os, sheet2D.framesHeight);
        WriteFloat(os, sheet2D.frameLength);
    }
}

void TileAnimationParams::Deserialize(std::istream& is)
{
    type = (TileAnimationType)ReadUInt8(is);

    if (type == TAT_VERTICAL_FRAMES) 
    {
        verticalFrames.aspectWidth = ReadUInt16(is);
        verticalFrames.aspectHeight = ReadUInt16(is);
        verticalFrames.length = ReadFloat(is);
    }
    else if (type == TAT_SHEET_2D) 
    {
        sheet2D.framesWidth = ReadUInt8(is);
        sheet2D.framesHeight = ReadUInt8(is);
        sheet2D.frameLength = ReadFloat(is);
    }
}

void TileAnimationParams::DetermineParams(Vector2<unsigned int> textureSize, int* frameCount,
    int* frameLengthMs, Vector2<unsigned int>* frameSize) const
{
    if (type == TAT_VERTICAL_FRAMES) 
    {
        unsigned int frHeight = (unsigned int)(textureSize[0] / 
            (float)verticalFrames.aspectWidth *  (float)verticalFrames.aspectHeight);
        int frCount = textureSize[1] / frHeight;
        if (frameCount)
            *frameCount = frCount;
        if (frameLengthMs)
            *frameLengthMs = (int)(1000.0 * verticalFrames.length / frCount);
        if (frameSize)
            *frameSize = Vector2<unsigned int>{ textureSize[0], frHeight };
    }
    else if (type == TAT_SHEET_2D) 
    {
        if (frameCount)
            *frameCount = sheet2D.framesWidth * sheet2D.framesHeight;
        if (frameLengthMs)
            *frameLengthMs = (int)(1000 * sheet2D.frameLength);
        if (frameSize)
        {
            *frameSize = Vector2<unsigned int>{
                textureSize[0] / sheet2D.framesWidth, textureSize[1] / sheet2D.framesHeight };
        }
    }
    // caller should check for TAT_NONE
}

void TileAnimationParams::GetTextureModifer(std::ostream& os, Vector2<unsigned int> textureSize, int frame) const
{
    if (type == TAT_NONE)
        return;
    if (type == TAT_VERTICAL_FRAMES) 
    {
        int frameCount;
        DetermineParams(textureSize, &frameCount, NULL, NULL);
        os << "^[verticalframe:" << frameCount << ":" << frame;
    }
    else if (type == TAT_SHEET_2D) 
    {
        int q, r;
        q = frame / sheet2D.framesWidth;
        r = frame % sheet2D.framesWidth;
        os << "^[sheet:" << sheet2D.framesWidth << "x" << sheet2D.framesHeight
            << ":" << r << "," << q;
    }
}

Vector2<float> TileAnimationParams::GetTextureCoords(Vector2<unsigned int> textureSize, int frame) const
{
    Vector2<unsigned int> ret = Vector2<unsigned int>::Zero();
    if (type == TAT_VERTICAL_FRAMES) 
    {
        unsigned int frameHeight = (int)(textureSize[0] /
            (float)verticalFrames.aspectWidth * (float)verticalFrames.aspectHeight);
        ret = Vector2<unsigned int>{ 0, frameHeight * frame };
    }
    else if (type == TAT_SHEET_2D) 
    {
        Vector2<unsigned int> frameSize;
        DetermineParams(textureSize, NULL, NULL, &frameSize);
        int q, r;
        q = frame / sheet2D.framesWidth;
        r = frame % sheet2D.framesWidth;
        ret = Vector2<unsigned int>{ r * frameSize[0], q * frameSize[1] };
    }
    return Vector2<float>{ret[0] / (float)textureSize[0], ret[1] / (float)textureSize[1]};
}