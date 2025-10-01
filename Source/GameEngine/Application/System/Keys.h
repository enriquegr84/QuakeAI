/*
Minetest
Copyright (C) 2016 est31, <MTest31@outlook.com>

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

#ifndef KEYS_H
#define KEYS_H

#include "GameEngineStd.h"

#include "KeyEvent.h"

class KeyType
{
public:
	enum Type
	{
		// Action
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		JUMP,
		AUX1,
		SNEAK,
		AUTOFORWARD,
		DIG,
		PLACE,

		ESC,

		// Other
		DROP,
		INVENTORY,
		CHAT,
		CMD,
		CMD_LOCAL,
		CONSOLE,
		MINIMAP,
		FREEMOVE,
		PITCHMOVE,
		FASTMOVE,
		NOCLIP,
		HOTBAR_PREV,
		HOTBAR_NEXT,
		MUTE,
		INC_VOLUME,
		DEC_VOLUME,
		CINEMATIC,
		SCREENSHOT,
		TOGGLE_HUD,
		TOGGLE_CHAT,
		TOGGLE_FOG,
		TOGGLE_UPDATE_CAMERA,
		TOGGLE_DEBUG,
		TOGGLE_PROFILER,
		CAMERA_MODE,
		INCREASE_VIEWING_RANGE,
		DECREASE_VIEWING_RANGE,
		RANGESELECT,
		ZOOM,

		// hotbar
		SLOT_1,
		SLOT_2,
		SLOT_3,
		SLOT_4,
		SLOT_5,
		SLOT_6,
		SLOT_7,
		SLOT_8,
		SLOT_9,
		SLOT_10,
		SLOT_11,
		SLOT_12,
		SLOT_13,
		SLOT_14,
		SLOT_15,
		SLOT_16,
		SLOT_17,
		SLOT_18,
		SLOT_19,
		SLOT_20,
		SLOT_21,
		SLOT_22,
		SLOT_23,
		SLOT_24,
		SLOT_25,
		SLOT_26,
		SLOT_27,
		SLOT_28,
		SLOT_29,
		SLOT_30,
		SLOT_31,
		SLOT_32,

		// Fake keycode for array size and internal checks
		INTERNAL_ENUM_COUNT

	};
};

typedef KeyType::Type GameKeyType;


/****************************************************************************
 Fast key cache
 ****************************************************************************/

 /* This is faster than using GetKeySetting with the tradeoff that functions
  * using it must make sure that it's initialised before using it and there is
  * no error handling (for example bounds checking).
  */
struct KeyCache
{
    KeyCache()
    {
        Populate();
        PopulateNonChanging();
    }

    void Populate();

    // Keys that are not settings dependent
    void PopulateNonChanging();

    //check whether a specific key is in the cache
    int Find(const KeyAction& key);

    std::map<unsigned int, KeyAction> keys;
};

class KeyList : private std::list<KeyAction>
{
    typedef std::list<KeyAction> super;
    typedef super::iterator iterator;
    typedef super::const_iterator const_iterator;

    virtual const_iterator Find(const KeyAction& key) const
    {
        const_iterator f(begin());
        const_iterator e(end());

        while (f != e)
        {
            if (*f == key)
                return f;

            ++f;
        }

        return e;
    }

    virtual iterator Find(const KeyAction& key)
    {
        iterator f(begin());
        iterator e(end());

        while (f != e)
        {
            if (*f == key)
                return f;

            ++f;
        }

        return e;
    }

public:
    void Clear() { super::clear(); }

    void Set(const KeyAction& key)
    {
        if (Find(key) == end())
            push_back(key);
    }

    void Unset(const KeyAction& key)
    {
        iterator p(Find(key));

        if (p != end())
            erase(p);
    }

    void Toggle(const KeyAction& key)
    {
        iterator p(this->Find(key));

        if (p != end())
            erase(p);
        else
            push_back(key);
    }

    bool operator[](const KeyAction& key) const { return Find(key) != end(); }
};

#endif