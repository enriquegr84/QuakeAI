/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#include "Keys.h"

void KeyCache::PopulateNonChanging()
{
    keys[KeyType::ESC] = KeyAction("KEY_ESCAPE");
}

void KeyCache::Populate()
{
    keys[KeyType::FORWARD] = GetKeySetting("keymap_forward");
    keys[KeyType::BACKWARD] = GetKeySetting("keymap_backward");
    keys[KeyType::LEFT] = GetKeySetting("keymap_left");
    keys[KeyType::RIGHT] = GetKeySetting("keymap_right");
    keys[KeyType::JUMP] = GetKeySetting("keymap_jump");
    keys[KeyType::AUX1] = GetKeySetting("keymap_aux1");
    keys[KeyType::SNEAK] = GetKeySetting("keymap_sneak");
    keys[KeyType::DIG] = GetKeySetting("keymap_dig");
    keys[KeyType::PLACE] = GetKeySetting("keymap_place");

    keys[KeyType::AUTOFORWARD] = GetKeySetting("keymap_autoforward");

    keys[KeyType::DROP] = GetKeySetting("keymap_drop");
    keys[KeyType::INVENTORY] = GetKeySetting("keymap_inventory");
    keys[KeyType::CHAT] = GetKeySetting("keymap_chat");
    keys[KeyType::CMD] = GetKeySetting("keymap_cmd");
    keys[KeyType::CMD_LOCAL] = GetKeySetting("keymap_cmd_local");
    keys[KeyType::CONSOLE] = GetKeySetting("keymap_console");
    keys[KeyType::MINIMAP] = GetKeySetting("keymap_minimap");
    keys[KeyType::FREEMOVE] = GetKeySetting("keymap_freemove");
    keys[KeyType::PITCHMOVE] = GetKeySetting("keymap_pitchmove");
    keys[KeyType::FASTMOVE] = GetKeySetting("keymap_fastmove");
    keys[KeyType::NOCLIP] = GetKeySetting("keymap_noclip");
    keys[KeyType::HOTBAR_PREV] = GetKeySetting("keymap_hotbar_previous");
    keys[KeyType::HOTBAR_NEXT] = GetKeySetting("keymap_hotbar_next");
    keys[KeyType::MUTE] = GetKeySetting("keymap_mute");
    keys[KeyType::INC_VOLUME] = GetKeySetting("keymap_increase_volume");
    keys[KeyType::DEC_VOLUME] = GetKeySetting("keymap_decrease_volume");
    keys[KeyType::CINEMATIC] = GetKeySetting("keymap_cinematic");
    keys[KeyType::SCREENSHOT] = GetKeySetting("keymap_screenshot");
    keys[KeyType::TOGGLE_HUD] = GetKeySetting("keymap_toggle_hud");
    keys[KeyType::TOGGLE_CHAT] = GetKeySetting("keymap_toggle_chat");
    keys[KeyType::TOGGLE_FOG] = GetKeySetting("keymap_toggle_fog");
    keys[KeyType::TOGGLE_UPDATE_CAMERA] = GetKeySetting("keymap_toggle_update_camera");
    keys[KeyType::TOGGLE_DEBUG] = GetKeySetting("keymap_toggle_debug");
    keys[KeyType::TOGGLE_PROFILER] = GetKeySetting("keymap_toggle_profiler");
    keys[KeyType::CAMERA_MODE] = GetKeySetting("keymap_camera_mode");
    keys[KeyType::INCREASE_VIEWING_RANGE] =
			GetKeySetting("keymap_increase_viewing_range_min");
    keys[KeyType::DECREASE_VIEWING_RANGE] =
			GetKeySetting("keymap_decrease_viewing_range_min");
    keys[KeyType::RANGESELECT] = GetKeySetting("keymap_rangeselect");
    keys[KeyType::ZOOM] = GetKeySetting("keymap_zoom");

    keys[KeyType::ESC] = KeyAction("KEY_ESCAPE");

	for (unsigned int i = 0; i < 10; i++) 
    {
		std::string slotKeyName = "keymap_slot" + std::to_string(i + 1);
        keys[KeyType::SLOT_1 + i] = GetKeySetting(slotKeyName.c_str());
	}
}

int KeyCache::Find(const KeyAction& a)
{
    for (auto const& key : keys)
        if (key.second == a)
            return key.first;

    return -1;
}