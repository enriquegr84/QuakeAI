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

#ifndef KEYEVENT_H
#define KEYEVENT_H

#include "KeyCodes.h"

#include "EventSystem.h"


/* A key press, consisting of either an engine keycode or an actual char */
class KeyAction
{
public:
	KeyAction() = default;

	KeyAction(const char *name);

	KeyAction(const Event::KeyInput& in, bool preferCharacter = false);

	bool operator==(const KeyAction& a) const
	{
		return (mChar > 0 && mChar == a.mChar) || (ValidKeyCode(mKey) && mKey == a.mKey);
	}

	const char* Sym() const;
	const char* Name() const;

protected:
	static bool ValidKeyCode(KeyCode k)
	{
		return k > 0 && k < KEY_KEY_CODES_COUNT;
	}

    KeyCode mKey = KEY_KEY_CODES_COUNT;
	wchar_t mChar = L'\0';
	std::string mName = "";
};

extern const KeyAction EscapeKey;
extern const KeyAction CancelKey;

// Key configuration getter
KeyAction GetKeySetting(const char *settingname);

// Clear fast lookup cache
void ClearKeyCache();

KeyCode KeyNameToKeyCode(const char *name);

#endif //KEYEVENT_H