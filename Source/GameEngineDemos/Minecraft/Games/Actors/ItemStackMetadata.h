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

#ifndef ITEMSTACKMETADATA_H
#define ITEMSTACKMETADATA_H

#include "../../Data/Metadata.h"

#include "Tool.h"

class Inventory;
class BaseItemManager;

class ItemStackMetadata : public Metadata
{
public:
	ItemStackMetadata() : mToolcapsOverridden(false) {}

	// Overrides
	void Clear() override;
	bool SetString(const std::string& name, const std::string& var) override;

	void Serialize(std::ostream& os) const;
	void Deserialize(std::istream& is);

	const ToolCapabilities& GetToolCapabilities(const ToolCapabilities& defaultCaps) const
	{
		return mToolcapsOverridden ? mToolcapsOverride : defaultCaps;
	}

	void SetToolCapabilities(const ToolCapabilities& caps);
	void ClearToolCapabilities();

private:
	void UpdateToolCapabilities();

	bool mToolcapsOverridden;
	ToolCapabilities mToolcapsOverride;
};

#endif