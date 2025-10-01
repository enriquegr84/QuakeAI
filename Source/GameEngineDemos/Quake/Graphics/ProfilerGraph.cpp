//========================================================================
// ProfilerGraph.cpp : ProfilerGraph Class
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
//    http://code.google.com/p/GameEngine/
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

#include "ProfilerGraph.h"

void ProfilerGraph::Put(const Profiler::GraphValues& values)
{
	mLog.emplace_back(values);

	while (mLog.size() > mLogMaxSize)
		mLog.erase(mLog.begin());
}

void ProfilerGraph::Draw(int xLeft, int yBottom, std::shared_ptr<BaseUIFont> font) const
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	// Do *not* use UNORDERED_MAP here as the order needs
	// to be the same for each call to prevent flickering
	std::map<std::string, Meta> profilerMeta;

	for (const Piece& piece : mLog) 
    {
		for (const auto& pieceValue : piece.values) 
        {
			const std::string& id = pieceValue.first;
			const float& value = pieceValue.second;
			std::map<std::string, Meta>::iterator it = profilerMeta.find(id);

			if (it == profilerMeta.end())
            {
                profilerMeta[id] = Meta(value);
				continue;
			}

			if (value < it->second.min)
                it->second.min = value;

			if (value > it->second.max)
                it->second.max = value;
		}
	}

	// Assign colors
	static const SColor UsableColors[] = {
        SColor(255, 255, 100, 100),
        SColor(255, 90, 225, 90),
        SColor(255, 100, 100, 255),
        SColor(255, 255, 150, 50),
        SColor(255, 220, 220, 100)};
	static const unsigned int UsableColorsCount =
			sizeof(UsableColors) / sizeof(*UsableColors);
	unsigned int nextColorIndex = 0;

	for (auto& m : profilerMeta)
    {
		Meta& meta = m.second;
		SColor color(255, 200, 200, 200);
		if (nextColorIndex < UsableColorsCount)
			color = UsableColors[nextColorIndex++];

		meta.color = color;
	}

	int graphh = 50;
    int textx = xLeft + mLogMaxSize + 15;
    int textx2 = textx + 200 - 15;
    int metaIndex = 0;

	for (const auto& m : profilerMeta) 
    {
		const std::string& id = m.first;
		const Meta& meta = m.second;
		int x = xLeft;
        int y = yBottom - metaIndex * 50;
		float showMin = meta.min;
		float showMax = meta.max;

		if (showMin >= -0.0001 && showMax >= -0.0001) 
			if (showMin <= showMax * 0.5)
				showMin = 0;

		int texth = 15;
		char buf[10];
		snprintf(buf, sizeof(buf), "%.3g", showMax);

        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{textx2 - textx, texth };
        rect.mCenter = Vector2<int>{textx, y - graphh} + rect.mExtent / 2;
		font->Draw(ToWideString(buf), rect, meta.color);
		snprintf(buf, sizeof(buf), "%.3g", showMin);

        rect.mExtent = Vector2<int>{ textx2 - textx, texth };
        rect.mCenter = Vector2<int>{ textx, y - texth } + rect.mExtent / 2;
		font->Draw(ToWideString(buf), rect, meta.color);

        rect.mExtent = Vector2<int>{ textx2 - textx, texth };
        rect.mCenter = Vector2<int>{ textx, y - graphh / 2 - texth / 2 } + rect.mExtent / 2;
		font->Draw(ToWideString(id), rect, meta.color);

		int graph1y = y;
		int graph1h = graphh;
		bool relativeGraph = (showMin != 0 && showMin != showMax);
		float lastScaledValue = 0.0;
		bool lastScaledValueExists = false;

		for (const Piece& piece : mLog) 
        {
			float value = 0;
			bool valueExists = false;
			Profiler::GraphValues::const_iterator itPiece = piece.values.find(id);

			if (itPiece != piece.values.end())
            {
				value = itPiece->second;
				valueExists = true;
			}

			if (!valueExists) 
            {
				x++;
				lastScaledValueExists = false;
				continue;
			}

			float scaledvalue = 1.0;
			if (showMax != showMin)
				scaledvalue = (value - showMin) / (showMax - showMin);

			if (scaledvalue == 1.0 && value == 0) 
            {
				x++;
				lastScaledValueExists = false;
				continue;
			}

			if (relativeGraph) 
            {
				if (lastScaledValueExists) 
                {
					int ivalue1 = (int)(lastScaledValue * graph1h);
                    int ivalue2 = (int)(scaledvalue * graph1h);
                    skin->Draw2DLine(meta.color,
                        Vector2<float>{(float)(x - 1), (float)(graph1y - ivalue1)},
                        Vector2<float>{(float)x, (float)(graph1y - ivalue2)});
				}

				lastScaledValue = scaledvalue;
				lastScaledValueExists = true;
			} 
            else 
            {
				int ivalue = (int)(scaledvalue * graph1h);
                skin->Draw2DLine(meta.color,
                    Vector2<float>{(float)x, (float)graph1y},
                    Vector2<float>{(float)x, (float)(graph1y - ivalue)});
			}

			x++;
		}

		metaIndex++;
	}
}
