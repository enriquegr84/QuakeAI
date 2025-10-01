//========================================================================
// StringUtil.h : Defines some useful string utility functions
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
//    http://code.google.com/p/gamecode4/
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

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include "Translation.h"

#include "Core/CoreStd.h"

class SColor;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

typedef std::unordered_map<std::string, std::string> StringMap;


struct FlagDescription 
{
    const char* name;
    unsigned int flag;
};

unsigned int ReadFlagString(std::string str, const FlagDescription* flagdesc, unsigned int* flagmask);
std::string WriteFlagString(unsigned int flags, const FlagDescription* flagdesc, unsigned int flagmask);


static const char HexChars[] = "0123456789abcdef";

static inline std::string HexEncode(const char* data, size_t dataSize)
{
    std::string ret;
    ret.reserve(dataSize * 2);

    char buf2[3];
    buf2[2] = '\0';
    for (unsigned int i = 0; i < dataSize; i++) 
    {
        unsigned char c = (unsigned char)data[i];
        buf2[0] = HexChars[(c & 0xf0) >> 4];
        buf2[1] = HexChars[c & 0x0f];
        ret.append(buf2);
    }

    return ret;
}

static inline std::string HexEncode(const std::string& data)
{
    return HexEncode(data.c_str(), data.size());
}

static inline bool HexDigitDecode(char hexdigit, unsigned char& value)
{
    if (hexdigit >= '0' && hexdigit <= '9')
        value = hexdigit - '0';
    else if (hexdigit >= 'A' && hexdigit <= 'F')
        value = hexdigit - 'A' + 10;
    else if (hexdigit >= 'a' && hexdigit <= 'f')
        value = hexdigit - 'a' + 10;
    else
        return false;
    return true;
}

static const std::string Base64Chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static inline bool IsBase64(unsigned char c)
{
    return isalnum(c) || c == '+' || c == '/' || c == '=';
}

static inline bool Base64IsValid(std::string const& s)
{
    for (char i : s)
        if (!IsBase64(i))
            return false;
    return true;
}

static inline std::string Base64Encode(unsigned char const* bytesToEncode, unsigned int inLen)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char charArray3[3];
    unsigned char charArray4[4];

    while (inLen--)
    {
        charArray3[i++] = *(bytesToEncode++);
        if (i == 3)
        {
            charArray4[0] = (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] = charArray3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += Base64Chars[charArray4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            charArray3[j] = '\0';

        charArray4[0] = (charArray3[0] & 0xfc) >> 2;
        charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
        charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
        charArray4[3] = charArray3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += Base64Chars[charArray4[j]];

        // Don't pad it with =
            /*while((i++ < 3))
                ret += '=';*/
    }

    return ret;
}

static inline std::string Base64Decode(const std::string& encodedString)
{
    int i = 0;
    int j = 0;
    int in = 0;
    unsigned char charArray4[4], charArray3[3];
    std::string ret;

    size_t inLen = encodedString.size();
    while (inLen-- && (encodedString[in] != '=') && IsBase64(encodedString[in]))
    {
        charArray4[i++] = encodedString[in]; in++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                charArray4[i] = (unsigned char)Base64Chars.find(charArray4[i]);

            charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
            charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

            for (i = 0; (i < 3); i++)
                ret += charArray3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
            charArray4[j] = 0;

        for (j = 0; j < 4; j++)
            charArray4[j] = (unsigned char)Base64Chars.find(charArray4[j]);

        charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
        charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
        charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

        for (j = 0; (j < i - 1); j++) ret += charArray3[j];
    }

    return ret;
}

inline std::string ToString(const wchar_t* wstr)
{
    size_t strLen = wcslen(wstr) + 1;
    char* str = new char[strLen];
    memset(str, 0, strLen);
    WideCharToMultiByte(CP_ACP, 0, wstr, (int)wcslen(wstr), str, (int)strLen, NULL, NULL);
    std::string out(str);
    delete[] str;
    return out;
}

inline std::string ToString(const std::wstring& wstr)
{
    size_t strLen = wstr.length() + 1;
    char* str = new char[strLen];
    memset(str, 0, strLen);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), str, (int)strLen, NULL, NULL);
    std::string out(str);
    delete[] str;
    return out;
}

inline std::wstring ToWideString(const char* str)
{
    size_t wstrLen = strlen(str) + 1;
    wchar_t* wstr = new wchar_t[wstrLen];
    memset(wstr, 0, wstrLen * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, str, (int)strlen(str), wstr, (int)wstrLen);
    std::wstring out(wstr);
    delete[] wstr;
    return out;
}

inline std::wstring ToWideString(const std::string& str)
{
    size_t wstrLen = str.length() + 1;
    wchar_t* wstr = new wchar_t[wstrLen];
    memset(wstr, 0, wstrLen * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), wstr, (int)wstrLen);
    std::wstring out(wstr);
    delete[] wstr;
    return out;
}

/**
 * @param str
 * @return A copy of \p str converted to all lowercase characters.
 */
inline std::string ToLowerString(const std::string& str)
{
    std::string s2;

    s2.reserve(str.size());
    for (char i : str)
        s2 += tolower(i);

    return s2;
}

/**
 * @param str
 * @return A copy of \p str with leading and trailing whitespace removed.
 */
inline std::string Trim(const std::string& str)
{
    size_t front = 0;
    while (isspace(str[front]))
        ++front;

    size_t back = str.size();
    while (back > front && isspace(str[back - 1]))
        --back;

    return str.substr(front, back - front);
}


/**
 * Checks that all characters in \p toCheck are a decimal digits.
 *
 * @param toCheck
 * @return true if toCheck is not empty and all characters in toCheck are
 *	decimal digits, otherwise false
 */
inline bool IsNumber(const std::string& toCheck)
{
    for (char i : toCheck)
        if (!std::isdigit(i))
            return false;

    return !toCheck.empty();
}


/**
 * Returns whether \p str should be regarded as (bool) true.  Case and leading
 * and trailing whitespace are ignored.  Values that will return
 * true are "y", "yes", "true" and any number that is not 0.
 * @param str
 */
inline bool IsYes(const std::string& str)
{
    std::string s2 = ToLowerString(Trim(str));
    return s2 == "y" || s2 == "yes" || s2 == "true" || atoi(s2.c_str()) != 0;
}

/**
 * Replace all occurrences of \p pattern in \p str with \p replacement.
 *
 * @param str String to replace pattern with replacement within.
 * @param pattern The pattern to replace.
 * @param replacement What to replace the pattern with.
 */
inline void StringReplace(std::string& str, const std::string& pattern, const std::string& replacement)
{
    std::string::size_type start = str.find(pattern, 0);
    while (start != str.npos) 
    {
        str = str.replace(start, pattern.size(), replacement);
        start = str.find(pattern, start + replacement.size());
    }
}

/**
 * Escapes characters [ ] \ , ; that can not be used in forms
 */
inline void StringFormEscape(std::string& str)
{
    StringReplace(str, "\\", "\\\\");
    StringReplace(str, "]", "\\]");
    StringReplace(str, "[", "\\[");
    StringReplace(str, ";", "\\;");
    StringReplace(str, ",", "\\,");
}

/**
 * Check that a string only contains whitelisted characters. This is the
 * opposite of StringAllowedBlacklist().
 *
 * @param str The string to be checked.
 * @param allowed_chars A string containing permitted characters.
 * @return true if the string is allowed, otherwise false.
 *
 * @see StringAllowedBlacklist()
 */
inline bool StringAllowed(const std::string& str, const std::string& allowedChars)
{
    return str.find_first_not_of(allowedChars) == str.npos;
}


/**
 * Check that a string contains no blacklisted characters. This is the
 * opposite of StringAllowed().
 *
 * @param str The string to be checked.
 * @param blacklisted_chars A string containing prohibited characters.
 * @return true if the string is allowed, otherwise false.

 * @see StringAllowed()
 */
inline bool StringAllowedBlacklist(const std::string& str, const std::string& blacklisted_chars)
{
    return str.find_first_of(blacklisted_chars) == str.npos;
}


template <typename T>
std::vector<std::basic_string<T>> Split(const std::basic_string<T>& s, T delim)
{
    std::vector<std::basic_string<T> > tokens;

    std::basic_string<T> current;
    bool lastWasEscape = false;
    for (size_t i = 0; i < s.length(); i++)
    {
        T si = s[i];
        if (lastWasEscape)
        {
            current += '\\';
            current += si;
            lastWasEscape = false;
        }
        else
        {
            if (si == delim)
            {
                tokens.push_back(current);
                current = std::basic_string<T>();
                lastWasEscape = false;
            }
            else if (si == '\\')
            {
                lastWasEscape = true;
            }
            else
            {
                current += si;
                lastWasEscape = false;
            }
        }
    }
    //push last element
    tokens.push_back(current);

    return tokens;
}

/**
 * Removes backslashes from an escaped string
 */
template <typename T>
std::basic_string<T> UnescapeString(const std::basic_string<T>& s)
{
    std::basic_string<T> res;
    for (size_t i = 0; i < s.length(); i++)
    {
        if (s[i] == '\\')
        {
            i++;
            if (i >= s.length())
                break;
        }
        res += s[i];
    }

    return res;
}

/**
 * Remove all escape sequences in \p s.
 *
 * @param s The string in which to remove escape sequences.
 * @return \p s, with escape sequences removed.
 */
template <typename T>
std::basic_string<T> UnescapeEnriched(const std::basic_string<T>& s)
{
    std::basic_string<T> output;
    size_t i = 0;
    while (i < s.length())
    {
        if (s[i] == '\x1b')
        {
            ++i;
            if (i == s.length())
                continue;
            if (s[i] == '(')
            {
                ++i;
                while (i < s.length() && s[i] != ')')
                {
                    if (s[i] == '\\') ++i;
                    ++i;
                }
                ++i;
            }
            else ++i;

            continue;
        }
        output += s[i];
        ++i;
    }
    return output;
}

bool ParseColorString(const std::string& value, SColor& color, bool quiet, unsigned char defaultAlpha = 0xff);
bool ParseHexColorString(const std::string& value, SColor& color, unsigned char defaultAlpha = 0xff);
bool ParseNamedColorString(const std::string& value, SColor& color);

size_t Stringlcpy(char* dst, const char* src, size_t size);
char* Stringtokr(char* s, const char* sep, char** lasts);

/**
 * Returns a version of \p str with the first occurrence of a string
 * contained within ends[] removed from the end of the string.
 *
 * @param str
 * @param ends A NULL- or ""- terminated array of strings to remove from s in
 *	the copy produced.  Note that once one of these strings is removed
 *	that no further postfixes contained within this array are removed.
 *
 * @return If no end could be removed then "" is returned.
 */
std::string StringRemoveEnd(const std::string& str, const char* ends[]);

/**
* Check two strings for equivalence.If \p case_insensitive is true
* then the case of the strings is ignored(default is false).
*
* @param s1
* @param s2
* @param case_insensitive
* @return true if the strings match
*/
template <typename T>
inline bool StringEqual(const std::basic_string<T>& s1,
    const std::basic_string<T>& s2, bool caseInsensitive = false)
{
    if (!caseInsensitive)
        return s1 == s2;

    if (s1.size() != s2.size())
        return false;

    for (size_t i = 0; i < s1.size(); ++i)
        if (tolower(s1[i]) != tolower(s2[i]))
            return false;

    return true;
}


/**
 * Check whether \p str begins with the string prefix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param prefix
 * @param case_insensitive
 * @return true if the str begins with prefix
 */
template <typename T>
inline bool StringStartsWith(const std::basic_string<T>& str,
    const std::basic_string<T>& prefix, bool caseInsensitive = false)
{
    if (str.size() < prefix.size())
        return false;

    if (!caseInsensitive)
        return str.compare(0, prefix.size(), prefix) == 0;

    for (size_t i = 0; i < prefix.size(); ++i)
        if (tolower(str[i]) != tolower(prefix[i]))
            return false;
    return true;
}

/**
 * Check whether \p str begins with the string prefix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param prefix
 * @param case_insensitive
 * @return true if the str begins with prefix
 */
template <typename T>
inline bool StringStartsWith(const std::basic_string<T>& str,
    const T* prefix, bool caseInsensitive = false)
{
    return StringStartsWith(str, std::basic_string<T>(prefix), caseInsensitive);
}


/**
 * Check whether \p str ends with the string suffix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param suffix
 * @param case_insensitive
 * @return true if the str begins with suffix
 */
template <typename T>
inline bool StringEndsWith(const std::basic_string<T>& str,
    const std::basic_string<T>& suffix, bool caseInsensitive = false)
{
    if (str.size() < suffix.size())
        return false;

    size_t start = str.size() - suffix.size();
    if (!caseInsensitive)
        return str.compare(start, suffix.size(), suffix) == 0;

    for (size_t i = 0; i < suffix.size(); ++i)
        if (tolower(str[start + i]) != tolower(suffix[i]))
            return false;
    return true;
}


/**
 * Check whether \p str ends with the string suffix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param suffix
 * @param case_insensitive
 * @return true if the str begins with suffix
 */
template <typename T>
inline bool StringEndsWith(const std::basic_string<T>& str,
    const T* suffix, bool caseInsensitive = false)
{
    return StringEndsWith(str, std::basic_string<T>(suffix), caseInsensitive);
}


/**
 * Splits a string into its component parts separated by the character
 * \p delimiter.
 *
 * @return An std::vector<std::basic_string<T> > of the component parts
 */
template <typename T>
inline std::vector<std::basic_string<T> > StringSplit(
    const std::basic_string<T> &str, T delimiter)
{
    std::vector<std::basic_string<T> > parts;
    std::basic_stringstream<T> sstr(str);
    std::basic_string<T> part;

    while (std::getline(sstr, part, delimiter))
        parts.push_back(part);

    return parts;
}

/* Translated strings have the following format:
 * \x1bT marks the beginning of a translated string
 * \x1bE marks its end
 *
 * \x1bF marks the beginning of an argument, and \x1bE its end.
 *
 * Arguments are *not* translated, as they may contain escape codes.
 * Thus, if you want a translated argument, it should be inside \x1bT/\x1bE tags as well.
 *
 * This representation is chosen so that clients ignoring escape codes will
 * see untranslated strings.
 *
 * For instance, suppose we have a string such as "@1 Wool" with the argument "White"
 * The string will be sent as "\x1bT\x1bF\x1bTWhite\x1bE\x1bE Wool\x1bE"
 * To translate this string, we extract what is inside \x1bT/\x1bE tags.
 * When we notice the \x1bF tag, we recursively extract what is there up to the \x1bE end tag,
 * translating it as well.
 * We get the argument "White", translated, and create a template string with "@1" instead of it.
 * We finally get the template "@1 Wool" that was used in the beginning, which we translate
 * before filling it again.
 */

void TranslateAll(const std::wstring& str, size_t& idx, Translations* translations, std::wstring& res);

std::wstring TranslateString(const std::wstring& str, Translations* translations);

std::wstring UnescapeTranslate(const std::wstring& str, Translations* translations);


struct EnumString
{
    int num;
    const char* str;
};


// Does a classic * & ? pattern match on a file name - this is case sensitive!
bool WildcardMatch(const wchar_t *pat, const wchar_t *str);

//	A hashed string.  It retains the initial (ANSI) string in 
//	addition to the hash value for easy reference.

// class HashedString				- Chapter 10, page 274
class HashedString
{
public:
	explicit HashedString(char const * const pIdentstring)
		: mID(HashName(pIdentstring)), mIDStr(pIdentstring)
	{

	}

	unsigned long GetHashValue(void) const
	{
		return mID;
	}

	const std::string & GetStr() const
	{
		return mIDStr;
	}

	unsigned long HashName(char const * pIdentStr);

	bool operator< (HashedString const & o) const
	{
		bool r = (GetHashValue() < o.GetHashValue());
		return r;
	}

	bool operator== (HashedString const & o) const
	{
		bool r = (GetHashValue() == o.GetHashValue());
		return r;
	}

private:

	unsigned long mID;
	std::string mIDStr;
};

template <typename T>
class BasicStrfnd 
{
    typedef std::basic_string<T> String;
    String mStr;
    size_t mPos;

public:
    BasicStrfnd(const String& str) : mStr(str), mPos(0) {}
    void Start(const String& str) { mStr = str; mPos = 0; }
    size_t Where() { return mPos; }
    void To(size_t i) { mPos = i; }
    bool AtEnd() { return mPos >= mStr.size(); }
    String What() { return mStr; }

    String Next(const String& sep)
    {
        if (mPos >= mStr.size())
            return String();

        size_t n;
        if (sep.empty() || (n = mStr.find(sep, mPos)) == String::npos)
            n = mStr.size();

        String ret = mStr.substr(mPos, n - mPos);
        mPos = n + sep.size();
        return ret;
    }

    // Returns substr up to the next occurence of sep that isn't escaped with esc ('\\')
    String NextEsc(const String& sep, T esc = static_cast<T>('\\'))
    {
        if (mPos >= mStr.size())
            return String();

        size_t n, oldPos = mPos;
        do 
        {
            if (sep.empty() || (n = mStr.find(sep, mPos)) == String::npos)
            {
                mPos = n = mStr.size();
                break;
            }
            mPos = n + sep.length();
        } while (n > 0 && mStr[n - 1] == esc);

        return mStr.substr(oldPos, n - oldPos);
    }

    void SkipOver(const String& chars)
    {
        size_t p = mStr.find_first_not_of(chars, mPos);
        if (p != String::npos)
            mPos = p;
    }
};

typedef BasicStrfnd<char> Strfnd;
typedef BasicStrfnd<wchar_t> WStrfnd;

#endif