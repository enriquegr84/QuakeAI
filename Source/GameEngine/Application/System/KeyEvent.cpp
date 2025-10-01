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

#include "KeyEvent.h"

#include "../Settings.h"

#include "Core/Logger/Logger.h"

#include "Core/Utility/StringUtil.h"

struct TableKey 
{
	const char* mName;
	KeyCode mKey;
	wchar_t mChar; // L'\0' means no character assigned
	const char* mLangName; // NULL means it doesn't have a human description
};

#define DEFINEKEY1(x, lang) /* key without character */ \
	{ #x, x, L'\0', lang },
#define DEFINEKEY2(x, ch, lang) /* key with character */ \
	{ #x, x, ch, lang },
#define DEFINEKEY3(ch) /* single key (e.g. KEY_KEY_X) */ \
	{ "KEY_KEY_" TOSTRING(ch), KEY_KEY_ ## ch, (wchar_t) *TOSTRING(ch), TOSTRING(ch) },
#define DEFINEKEY4(ch) /* single function key (e.g. KEY_F3) */ \
	{ "KEY_F" TOSTRING(ch), KEY_F ## ch, L'\0', "F" TOSTRING(ch) },
#define DEFINEKEY5(ch) /* key without keycode */ \
	{ ch, KEY_KEY_CODES_COUNT, (wchar_t) *ch, ch },

#define N_(text) text

static const struct TableKey table[] = {
	// Keys that can be reliably mapped between Char and Key
	DEFINEKEY3(0)
	DEFINEKEY3(1)
	DEFINEKEY3(2)
	DEFINEKEY3(3)
	DEFINEKEY3(4)
	DEFINEKEY3(5)
	DEFINEKEY3(6)
	DEFINEKEY3(7)
	DEFINEKEY3(8)
	DEFINEKEY3(9)
	DEFINEKEY3(A)
	DEFINEKEY3(B)
	DEFINEKEY3(C)
	DEFINEKEY3(D)
	DEFINEKEY3(E)
	DEFINEKEY3(F)
	DEFINEKEY3(G)
	DEFINEKEY3(H)
	DEFINEKEY3(I)
	DEFINEKEY3(J)
	DEFINEKEY3(K)
	DEFINEKEY3(L)
	DEFINEKEY3(M)
	DEFINEKEY3(N)
	DEFINEKEY3(O)
	DEFINEKEY3(P)
	DEFINEKEY3(Q)
	DEFINEKEY3(R)
	DEFINEKEY3(S)
	DEFINEKEY3(T)
	DEFINEKEY3(U)
	DEFINEKEY3(V)
	DEFINEKEY3(W)
	DEFINEKEY3(X)
	DEFINEKEY3(Y)
	DEFINEKEY3(Z)
	DEFINEKEY2(KEY_PLUS, L'+', "+")
	DEFINEKEY2(KEY_COMMA, L',', ",")
	DEFINEKEY2(KEY_MINUS, L'-', "-")
	DEFINEKEY2(KEY_PERIOD, L'.', ".")

	// Keys without a Char
	DEFINEKEY1(KEY_LBUTTON, N_("Left Button"))
	DEFINEKEY1(KEY_RBUTTON, N_("Right Button"))
	DEFINEKEY1(KEY_CANCEL, N_("Cancel"))
	DEFINEKEY1(KEY_MBUTTON, N_("Middle Button"))
	DEFINEKEY1(KEY_XBUTTON1, N_("X Button 1"))
	DEFINEKEY1(KEY_XBUTTON2, N_("X Button 2"))
	DEFINEKEY1(KEY_BACK, N_("Backspace"))
	DEFINEKEY1(KEY_TAB, N_("Tab"))
	DEFINEKEY1(KEY_CLEAR, N_("Clear"))
	DEFINEKEY1(KEY_RETURN, N_("Return"))
	DEFINEKEY1(KEY_SHIFT, N_("Shift"))
	DEFINEKEY1(KEY_CONTROL, N_("Control"))
	//~ Key name, common on Windows keyboards
	DEFINEKEY1(KEY_MENU, N_("Menu"))
	DEFINEKEY1(KEY_PAUSE, N_("Pause"))
	DEFINEKEY1(KEY_CAPITAL, N_("Caps Lock"))
	DEFINEKEY1(KEY_SPACE, N_("Space"))
	DEFINEKEY1(KEY_PRIOR, N_("Page up"))
	DEFINEKEY1(KEY_NEXT, N_("Page down"))
	DEFINEKEY1(KEY_END, N_("End"))
	DEFINEKEY1(KEY_HOME, N_("Home"))
	DEFINEKEY1(KEY_LEFT, N_("Left"))
	DEFINEKEY1(KEY_UP, N_("Up"))
	DEFINEKEY1(KEY_RIGHT, N_("Right"))
	DEFINEKEY1(KEY_DOWN, N_("Down"))
	//~ Key name
	DEFINEKEY1(KEY_SELECT, N_("Select"))
	//~ "Print screen" key
	DEFINEKEY1(KEY_PRINT, N_("Print"))
	DEFINEKEY1(KEY_EXECUT, N_("Execute"))
	DEFINEKEY1(KEY_SNAPSHOT, N_("Snapshot"))
	DEFINEKEY1(KEY_INSERT, N_("Insert"))
	DEFINEKEY1(KEY_DELETE, N_("Delete"))
	DEFINEKEY1(KEY_HELP, N_("Help"))
	DEFINEKEY1(KEY_LWIN, N_("Left Windows"))
	DEFINEKEY1(KEY_RWIN, N_("Right Windows"))
	DEFINEKEY1(KEY_NUMPAD0, N_("Numpad 0")) // These are not assigned to a char
	DEFINEKEY1(KEY_NUMPAD1, N_("Numpad 1")) // to prevent interference with KEY_KEY_[0-9].
	DEFINEKEY1(KEY_NUMPAD2, N_("Numpad 2"))
	DEFINEKEY1(KEY_NUMPAD3, N_("Numpad 3"))
	DEFINEKEY1(KEY_NUMPAD4, N_("Numpad 4"))
	DEFINEKEY1(KEY_NUMPAD5, N_("Numpad 5"))
	DEFINEKEY1(KEY_NUMPAD6, N_("Numpad 6"))
	DEFINEKEY1(KEY_NUMPAD7, N_("Numpad 7"))
	DEFINEKEY1(KEY_NUMPAD8, N_("Numpad 8"))
	DEFINEKEY1(KEY_NUMPAD9, N_("Numpad 9"))
	DEFINEKEY1(KEY_MULTIPLY, N_("Numpad *"))
	DEFINEKEY1(KEY_ADD, N_("Numpad +"))
	DEFINEKEY1(KEY_SEPARATOR, N_("Numpad ."))
	DEFINEKEY1(KEY_SUBTRACT, N_("Numpad -"))
	DEFINEKEY1(KEY_DECIMAL, NULL)
	DEFINEKEY1(KEY_DIVIDE, N_("Numpad /"))
	DEFINEKEY4(1)
	DEFINEKEY4(2)
	DEFINEKEY4(3)
	DEFINEKEY4(4)
	DEFINEKEY4(5)
	DEFINEKEY4(6)
	DEFINEKEY4(7)
	DEFINEKEY4(8)
	DEFINEKEY4(9)
	DEFINEKEY4(10)
	DEFINEKEY4(11)
	DEFINEKEY4(12)
	DEFINEKEY4(13)
	DEFINEKEY4(14)
	DEFINEKEY4(15)
	DEFINEKEY4(16)
	DEFINEKEY4(17)
	DEFINEKEY4(18)
	DEFINEKEY4(19)
	DEFINEKEY4(20)
	DEFINEKEY4(21)
	DEFINEKEY4(22)
	DEFINEKEY4(23)
	DEFINEKEY4(24)
	DEFINEKEY1(KEY_NUMLOCK, N_("Num Lock"))
	DEFINEKEY1(KEY_SCROLL, N_("Scroll Lock"))
	DEFINEKEY1(KEY_LSHIFT, N_("Left Shift"))
	DEFINEKEY1(KEY_RSHIFT, N_("Right Shift"))
	DEFINEKEY1(KEY_LCONTROL, N_("Left Control"))
	DEFINEKEY1(KEY_RCONTROL, N_("Right Control"))
	DEFINEKEY1(KEY_LMENU, N_("Left Menu"))
	DEFINEKEY1(KEY_RMENU, N_("Right Menu"))

	// Rare/weird keys
	DEFINEKEY1(KEY_KANA, "Kana")
	DEFINEKEY1(KEY_HANGUEL, "Hangul")
	DEFINEKEY1(KEY_HANGUL, "Hangul")
	DEFINEKEY1(KEY_JUNJA, "Junja")
	DEFINEKEY1(KEY_FINAL, "Final")
	DEFINEKEY1(KEY_KANJI, "Kanji")
	DEFINEKEY1(KEY_HANJA, "Hanja")
	DEFINEKEY1(KEY_ESCAPE, N_("IME Escape"))
	DEFINEKEY1(KEY_CONVERT, N_("IME Convert"))
	DEFINEKEY1(KEY_NONCONVERT, N_("IME Nonconvert"))
	DEFINEKEY1(KEY_ACCEPT, N_("IME Accept"))
	DEFINEKEY1(KEY_MODECHANGE, N_("IME Mode Change"))
	DEFINEKEY1(KEY_APPS, N_("Apps"))
	DEFINEKEY1(KEY_SLEEP, N_("Sleep"))
	DEFINEKEY1(KEY_OEM_1, "OEM 1") // KEY_OEM_[0-9] and KEY_OEM_102 are assigned to multiple
	DEFINEKEY1(KEY_OEM_2, "OEM 2") // different chars (on different platforms too) and thus w/o char
	DEFINEKEY1(KEY_OEM_3, "OEM 3")
	DEFINEKEY1(KEY_OEM_4, "OEM 4")
	DEFINEKEY1(KEY_OEM_5, "OEM 5")
	DEFINEKEY1(KEY_OEM_6, "OEM 6")
	DEFINEKEY1(KEY_OEM_7, "OEM 7")
	DEFINEKEY1(KEY_OEM_8, "OEM 8")
	DEFINEKEY1(KEY_OEM_AX, "OEM AX")
	DEFINEKEY1(KEY_OEM_102, "OEM 102")
	DEFINEKEY1(KEY_ATTN, "Attn")
	DEFINEKEY1(KEY_CRSEL, "CrSel")
	DEFINEKEY1(KEY_EXSEL, "ExSel")
	DEFINEKEY1(KEY_EREOF, N_("Erase EOF"))
	DEFINEKEY1(KEY_PLAY, N_("Play"))
	DEFINEKEY1(KEY_ZOOM, N_("Zoom"))
	DEFINEKEY1(KEY_PA1, "PA1")
	DEFINEKEY1(KEY_OEM_CLEAR, N_("OEM Clear"))

	// Keys without Irrlicht keycode
	DEFINEKEY5("!")
	DEFINEKEY5("\"")
	DEFINEKEY5("#")
	DEFINEKEY5("$")
	DEFINEKEY5("%")
	DEFINEKEY5("&")
	DEFINEKEY5("'")
	DEFINEKEY5("(")
	DEFINEKEY5(")")
	DEFINEKEY5("*")
	DEFINEKEY5("/")
	DEFINEKEY5(":")
	DEFINEKEY5(";")
	DEFINEKEY5("<")
	DEFINEKEY5("=")
	DEFINEKEY5(">")
	DEFINEKEY5("?")
	DEFINEKEY5("@")
	DEFINEKEY5("[")
	DEFINEKEY5("\\")
	DEFINEKEY5("]")
	DEFINEKEY5("^")
	DEFINEKEY5("_")
};

#undef N_

struct TableKey LookupKeyName(const char* name)
{
	for (auto const& tableKey : table)
		if (strcmp(tableKey.mName, name) == 0)
			return tableKey;

	throw UnknownKeycode(name);
}

struct TableKey LookupKeyKey(KeyCode key)
{
	for (auto const& tableKey : table)
		if (tableKey.mKey == key)
			return tableKey;

	std::ostringstream os;
	os << "<Keycode " << (int) key << ">";
	throw UnknownKeycode(os.str().c_str());
}

struct TableKey LookupKeyChar(wchar_t lookChar)
{
	for (auto const& tableKey : table) 
		if (tableKey.mChar == lookChar)
			return tableKey;

	std::ostringstream os;
	os << "<Char " << HexEncode((char*)&lookChar, sizeof(wchar_t)).c_str() << ">";
	throw UnknownKeycode(os.str().c_str());
}

KeyAction::KeyAction(const char* name)
{
	if (strlen(name) == 0) 
    {
		mKey = KEY_KEY_CODES_COUNT;
		mChar = L'\0';
		mName = "";
		return;
	}

	if (strlen(name) <= 4) 
    {
		// Lookup by resulting character
		int charsRead = mbtowc(&mChar, name, 1);
		LogAssert(charsRead == 1, "Unexpected multibyte character");
		try 
        {
			struct TableKey k = LookupKeyChar(mChar);
			mName = k.mName;
			mKey = k.mKey;
			return;
		} catch (UnknownKeycode &) {};
	} 
    else 
    {
		// Lookup by name
		mName = name;
		try 
        {
			struct TableKey k = LookupKeyName(name);
			mKey = k.mKey;
			mChar = k.mChar;
			return;
		} catch (UnknownKeycode &) {};
	}

	// It's not a known key, complain and try to do something
	mKey = KEY_KEY_CODES_COUNT;
	int charsRead = mbtowc(&mChar, name, 1);
	LogAssert(charsRead == 1, "Unexpected multibyte character");
	mName = "";
	LogWarning("KeyAction: Unknown key '" + std::string(name) + "', falling back to first char.");
}

KeyAction::KeyAction(const Event::KeyInput& in, bool preferCharacter)
{
	if (preferCharacter)
		mKey = KEY_KEY_CODES_COUNT;
	else
		mKey = in.mKey;
	mChar = in.mChar;

	try 
    {
		if (ValidKeyCode(mKey))
			mName = LookupKeyKey(mKey).mName;
		else
			mName = LookupKeyChar(mChar).mName;
	} 
    catch (UnknownKeycode &) 
    {
		mName = "";
	};
}

const char* KeyAction::Sym() const
{
	return mName.c_str();
}

const char* KeyAction::Name() const
{
	if (mName.empty())
		return "";
	const char *ret;
	if (ValidKeyCode(mKey))
		ret = LookupKeyKey(mKey).mLangName;
	else
		ret = LookupKeyChar(mChar).mLangName;
	return ret ? ret : "<Unnamed key>";
}


const KeyAction EscapeKey("KEY_ESCAPE");
const KeyAction CancelKey("KEY_CANCEL");


/*
	Key config
*/

// A simple cache for quicker lookup
std::unordered_map<std::string, KeyAction> KeySettingCache;

KeyAction GetKeySetting(const char* settingName)
{
    std::unordered_map<std::string, KeyAction>::iterator n;
	n = KeySettingCache.find(settingName);
	if (n != KeySettingCache.end())
		return n->second;

	KeyAction key(Settings::Get()->Get(settingName).c_str());
	KeySettingCache[settingName] = key;
	return key;
}

void ClearKeyCache()
{
	KeySettingCache.clear();
}

KeyCode KeyNameToKeyCode(const char* name)
{
	return LookupKeyName(name).mKey;
}
