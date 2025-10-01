/*
Minetest
Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

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

#include "ItemStackMetadata.h"

#include "Core/Utility/Serialize.h"

#define DESERIALIZE_START '\x01'
#define DESERIALIZE_KV_DELIM '\x02'
#define DESERIALIZE_PAIR_DELIM '\x03'
#define DESERIALIZE_START_STR "\x01"
#define DESERIALIZE_KV_DELIM_STR "\x02"
#define DESERIALIZE_PAIR_DELIM_STR "\x03"

#define TOOLCAP_KEY "ToolCapabilities"

void ItemStackMetadata::Clear()
{
	Metadata::Clear();
	UpdateToolCapabilities();
}

static void SanitizeString(std::string& str)
{
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_START), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_KV_DELIM), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_PAIR_DELIM), str.end());
}

bool ItemStackMetadata::SetString(const std::string& name, const std::string& var)
{
	std::string cleanName = name;
	std::string cleanVar = var;
	SanitizeString(cleanName);
	SanitizeString(cleanVar);

	bool result = Metadata::SetString(cleanName, cleanVar);
	if (cleanName == TOOLCAP_KEY)
		UpdateToolCapabilities();
	return result;
}

void ItemStackMetadata::Serialize(std::ostream& os) const
{
	std::ostringstream os2;
	os2 << DESERIALIZE_START;
	for (const auto& stringVar : mStringVars) 
    {
		if (!stringVar.first.empty() || !stringVar.second.empty())
			os2 << stringVar.first << DESERIALIZE_KV_DELIM
				<< stringVar.second << DESERIALIZE_PAIR_DELIM;
	}
	os << SerializeJsonStringIfNeeded(os2.str());
}

void ItemStackMetadata::Deserialize(std::istream &is)
{
	std::string in = DeserializeJsonStringIfNeeded(is);

	mStringVars.clear();

	if (!in.empty()) 
    {
		if (in[0] == DESERIALIZE_START) 
        {
			Strfnd fnd(in);
			fnd.To(1);
			while (!fnd.AtEnd()) 
            {
				std::string name = fnd.Next(DESERIALIZE_KV_DELIM_STR);
				std::string var  = fnd.Next(DESERIALIZE_PAIR_DELIM_STR);
				mStringVars[name] = var;
			}
		} 
        else 
        {
			// BACKWARDS COMPATIBILITY
			mStringVars[""] = in;
		}
	}
	UpdateToolCapabilities();
}

void ItemStackMetadata::UpdateToolCapabilities()
{
	if (Contains(TOOLCAP_KEY)) 
    {
		mToolcapsOverridden = true;
		mToolcapsOverride = ToolCapabilities();
		std::istringstream is(GetString(TOOLCAP_KEY));
		mToolcapsOverride.DeserializeJson(is);
	} 
    else 
    {
		mToolcapsOverridden = false;
	}
}

void ItemStackMetadata::SetToolCapabilities(const ToolCapabilities& caps)
{
	std::ostringstream os;
	caps.SerializeJson(os);
	SetString(TOOLCAP_KEY, os.str());
}

void ItemStackMetadata::ClearToolCapabilities()
{
	SetString(TOOLCAP_KEY, "");
}
