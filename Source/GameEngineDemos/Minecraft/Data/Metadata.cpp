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

#include "Metadata.h"

#include "Application/Settings.h"

enum ContentType
{
    ECT_UNKNOWN,
    ECT_MOD,
    ECT_MODPACK,
    ECT_GAME,
    ECT_TXP
};

ContentType GetContentType(const ContentSpecification& spec)
{
    std::ifstream modpackIs((spec.path + "\\modpack.txt"));
    if (modpackIs.good())
    {
        modpackIs.close();
        return ECT_MODPACK;
    }

    std::ifstream modpack2Is((spec.path + "\\modpack.conf"));
    if (modpack2Is.good())
    {
        modpack2Is.close();
        return ECT_MODPACK;
    }

    std::ifstream initIs((spec.path + "\\init.conf"));
    if (initIs.good())
    {
        initIs.close();
        return ECT_MOD;
    }

    std::ifstream gameIs((spec.path + "\\game.conf"));
    if (gameIs.good())
    {
        gameIs.close();
        return ECT_GAME;
    }

    std::ifstream txpIs((spec.path + "\\texture_pack.conf"));
    if (txpIs.good())
    {
        txpIs.close();
        return ECT_TXP;
    }

    return ECT_UNKNOWN;
}

void ParseContentInfo(ContentSpecification& spec)
{
    std::string confPath;

    switch (GetContentType(spec))
    {
        case ECT_MOD:
            spec.type = "mod";
            confPath = spec.path + "\\mod.conf";
            break;
        case ECT_MODPACK:
            spec.type = "modpack";
            confPath = spec.path + "\\modpack.conf";
            break;
        case ECT_GAME:
            spec.type = "game";
            confPath = spec.path + "\\game.conf";
            break;
        case ECT_TXP:
            spec.type = "txp";
            confPath = spec.path + "\\texture_pack.conf";
            break;
        default:
            spec.type = "unknown";
            break;
    }

    Settings conf;
    if (!confPath.empty() && conf.ReadConfigFile(confPath.c_str()))
    {
        if (conf.Exists("name"))
            spec.name = conf.Get("name");

        if (conf.Exists("description"))
            spec.desc = conf.Get("description");

        if (conf.Exists("author"))
            spec.author = conf.Get("author");

        if (conf.Exists("release"))
            spec.release = conf.GetInt("release");
    }

    if (spec.desc.empty())
    {
        std::ifstream is(spec.path + "\\description.txt");
        spec.desc = std::string((std::istreambuf_iterator<char>(is)),
            std::istreambuf_iterator<char>()).c_str();
    }
}

/*
	Metadata
*/
void Metadata::Clear()
{
	mStringVars.clear();
	mModified = true;
}

bool Metadata::Empty() const
{
	return mStringVars.empty();
}

size_t Metadata::Size() const
{
	return mStringVars.size();
}

bool Metadata::Contains(const std::string& name) const
{
	return mStringVars.find(name) != mStringVars.end();
}

bool Metadata::operator==(const Metadata& other) const
{
	if (Size() != other.Size())
		return false;

	for (const auto& sv : mStringVars)
		if (!other.Contains(sv.first) || other.GetString(sv.first) != sv.second)
			return false;

	return true;
}

const std::string& Metadata::GetString(const std::string& name, uint16_t recursion) const
{
	StringMap::const_iterator it = mStringVars.find(name);
	if (it == mStringVars.end()) 
    {
		static const std::string emptyString = std::string("");
		return emptyString;
	}

	return ResolveString(it->second, recursion);
}

bool Metadata::GetStringToRef(
		const std::string& name, std::string& str, uint16_t recursion) const
{
	StringMap::const_iterator it = mStringVars.find(name);
	if (it == mStringVars.end())
		return false;

	str = ResolveString(it->second, recursion);
	return true;
}

/**
 * Sets var to name key in the metadata storage
 *
 * @param name
 * @param var
 * @return true if key-value pair is created or changed
 */
bool Metadata::SetString(const std::string& name, const std::string& var)
{
	if (var.empty()) 
    {
		mStringVars.erase(name);
		return true;
	}

	StringMap::iterator it = mStringVars.find(name);
	if (it != mStringVars.end() && it->second == var)
		return false;

	mStringVars[name] = var;
	mModified = true;
	return true;
}

const std::string& Metadata::ResolveString(const std::string& str, uint16_t recursion) const
{
	if (recursion <= 1 && str.substr(0, 2) == "${" && str[str.length() - 1] == '}')
		return GetString(str.substr(2, str.length() - 3), recursion + 1);

	return str;
}
