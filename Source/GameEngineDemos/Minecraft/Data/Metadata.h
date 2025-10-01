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

#ifndef METADATA_H
#define METADATA_H

#include "../MinecraftStd.h"

#include "Core/Utility/StringUtil.h"

struct ContentSpecification
{
    std::string type;
    std::string author;
    unsigned int release = 0;
    std::string name;
    std::string desc;
    std::string path;
};

void ParseContentInfo(ContentSpecification& spec);


class Metadata
{
public:
	virtual ~Metadata() = default;

	virtual void Clear();
	virtual bool Empty() const;

	bool operator==(const Metadata &other) const;
	inline bool operator!=(const Metadata &other) const { return !(*this == other); }

	//
	// Key-value related
	//

	size_t Size() const;
	bool Contains(const std::string& name) const;
	const std::string& GetString(const std::string& name, uint16_t recursion = 0) const;
	bool GetStringToRef(const std::string& name, std::string& str, uint16_t recursion = 0) const;
	virtual bool SetString(const std::string& name, const std::string& var);
	inline bool RemoveString(const std::string& name) { return SetString(name, ""); }
	const StringMap& GetStrings() const { return mStringVars; }
	// Add support for variable names in values
	const std::string& ResolveString(const std::string& str, uint16_t recursion = 0) const;

	inline bool IsModified() const  { return mModified; }
	inline void SetModified(bool m) { mModified = m; }

protected:
	StringMap mStringVars;

private:
    bool mModified = false;
};

#endif