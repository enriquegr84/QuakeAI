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

#ifndef PROFILERGRAPH_H
#define PROFILERGRAPH_H

#include "Graphic/Resource/Color.h"
#include "Graphic/UI/Element/UIFont.h"

#include "Core/Utility/Profiler.h"

/* Profiler display */
class ProfilerGraph
{
public:
	unsigned int mLogMaxSize = 200;

    ProfilerGraph(BaseUI* ui) : mUI(ui)
    {

    }

	void Put(const Profiler::GraphValues& values);

	void Draw(int xLeft, int yBottom, std::shared_ptr<BaseUIFont> font) const;

private:
    struct Piece
    {
        Piece(Profiler::GraphValues v) : values(std::move(v)) {}
        Profiler::GraphValues values;
    };
    struct Meta
    {
        float min;
        float max;
        SColor color;
        Meta(float initial = 0, SColor color = SColor(255, 255, 255, 255))
            : min(initial), max(initial), color(color)
        {
        }
    };
    std::deque<Piece> mLog;

    BaseUI* mUI;
};

#endif