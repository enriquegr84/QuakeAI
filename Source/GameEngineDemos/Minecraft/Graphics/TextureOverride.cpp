/*
Minetest
Copyright (C) 2020 Hugues Ross <hugues.ross@gmail.com>

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

#include "TextureOverride.h"

#include "Core/Logger/Logger.h"
#include "Core/Utility/StringUtil.h"

#define override_cast static_cast<uint16_t>

TextureOverrideSource::TextureOverrideSource(std::string filepath)
{
	std::ifstream infile(filepath);
	std::string line;
	int lineIndex = 0;
	while (std::getline(infile, line))
    {
		lineIndex++;

		// Also trim '\r' on DOS-style files
		line = Trim(line);

		// Ignore empty lines and comments
		if (line.empty() || line[0] == '#')
			continue;

		std::vector<std::string> splitted = StringSplit(line, ' ');
		if (splitted.size() != 3) 
        {
            LogWarning(filepath + ":" + std::to_string(lineIndex) +
                " Syntax error in texture override \"" + line +
                "\": Expected 3 arguments, got " + std::to_string(splitted.size()));
			continue;
		}

		TextureOverride textureOverride = {};
		textureOverride.id = splitted[0];
		textureOverride.texture = splitted[2];

		// Parse the target mask
        std::vector<std::string> targets = StringSplit(splitted[1], ',');
		for (const std::string &target : targets) 
        {
			if (target == "top")
				textureOverride.target |= override_cast(OverrideTarget::TOP);
			else if (target == "bottom")
				textureOverride.target |= override_cast(OverrideTarget::BOTTOM);
			else if (target == "left")
				textureOverride.target |= override_cast(OverrideTarget::LEFT);
			else if (target == "right")
				textureOverride.target |= override_cast(OverrideTarget::RIGHT);
			else if (target == "front")
				textureOverride.target |= override_cast(OverrideTarget::FRONT);
			else if (target == "back")
				textureOverride.target |= override_cast(OverrideTarget::BACK);
			else if (target == "inventory")
				textureOverride.target |= override_cast(OverrideTarget::INVENTORY);
			else if (target == "wield")
				textureOverride.target |= override_cast(OverrideTarget::WIELD);
			else if (target == "special1")
				textureOverride.target |= override_cast(OverrideTarget::SPECIAL_1);
			else if (target == "special2")
				textureOverride.target |= override_cast(OverrideTarget::SPECIAL_2);
			else if (target == "special3")
				textureOverride.target |= override_cast(OverrideTarget::SPECIAL_3);
			else if (target == "special4")
				textureOverride.target |= override_cast(OverrideTarget::SPECIAL_4);
			else if (target == "special5")
				textureOverride.target |= override_cast(OverrideTarget::SPECIAL_5);
			else if (target == "special6")
				textureOverride.target |= override_cast(OverrideTarget::SPECIAL_6);
			else if (target == "sides")
				textureOverride.target |= override_cast(OverrideTarget::SIDES);
			else if (target == "all" || target == "*")
				textureOverride.target |= override_cast(OverrideTarget::ALL_FACES);
			else 
            {
				// Report invalid target
				LogWarning(filepath + ":" + std::to_string(lineIndex) + 
                    " Syntax error in texture override \"" + line + 
                    "\": Unknown target \"" + target + "\"");
			}
		}

		// If there are no valid targets, skip adding this override
		if (textureOverride.target == override_cast(OverrideTarget::INVALID))
			continue;

		mOverrides.push_back(textureOverride);
	}
}

//! Get all overrides that apply to item definitions
std::vector<TextureOverride> TextureOverrideSource::GetItemTextureOverrides()
{
    std::vector<TextureOverride> foundOverrides;
	for (const TextureOverride& textureOverride : mOverrides)
		if (textureOverride.hasTarget(OverrideTarget::ITEM_TARGETS))
			foundOverrides.push_back(textureOverride);

	return foundOverrides;
}

//! Get all overrides that apply to node definitions
std::vector<TextureOverride> TextureOverrideSource::GetNodeTileOverrides()
{
    std::vector<TextureOverride> foundOverrides;
	for (const TextureOverride& textureOverride : mOverrides)
		if (textureOverride.hasTarget(OverrideTarget::NODE_TARGETS))
			foundOverrides.push_back(textureOverride);

	return foundOverrides;
}
