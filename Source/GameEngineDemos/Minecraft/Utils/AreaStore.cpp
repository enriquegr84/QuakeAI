/*
Minetest
Copyright (C) 2015 est31 <mtest31@outlook.com>

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

#include "AreaStore.h"

#include "Core/Utility/Serialize.h"

AreaStore* AreaStore::GetOptimalImplementation()
{
	return new VectorAreaStore();
}

const Area* AreaStore::GetArea(const std::string& name) const
{
	AreaMap::const_iterator it = mAreasMap.find(name);
	if (it == mAreasMap.end())
		return nullptr;
	return &it->second;
}


AreaParseEvent AreaStore::ParseObject(const std::string& line, std::string& name, std::string& value)
{
	std::string trimmedLine = Trim(line);

	if (trimmedLine.empty())
		return APE_NONE;
	if (trimmedLine[0] == '#')
		return APE_COMMENT;
	if (trimmedLine == "}")
		return APE_END;
	if (trimmedLine == "{")
		return APE_GROUP;

	size_t pos = trimmedLine.find('=');
	if (pos == std::string::npos)
		return APE_INVALID;

	name = Trim(trimmedLine.substr(0, pos));
	value = Trim(trimmedLine.substr(pos + 1));

	if (value == "\"\"\"")
		return APE_MULTILINE;

	return APE_KVPAIR;
}


void AreaStore::Serialize(std::ostream& os) const
{
	/*
	for (const auto& it : mAreasMap)
	{
		const Area& a = it.second;
		WriteV3Short(os, a.minEdge);
		WriteV3Short(os, a.maxEdge);
		WriteUInt16(os, a.data.size());
		os.write(a.data.data(), a.data.size());
	}
	*/
}

void AreaStore::Deserialize(std::istream& is)
{
	Area* area;
	std::vector<Area> areas;

    std::string line, name, value;

    while (is.good()) 
    {
        std::getline(is, line);
        AreaParseEvent evt = ParseObject(line, name, value);

        switch (evt)
        {
            case APE_NONE:
            case APE_INVALID:
            case APE_COMMENT:
                break;
            case APE_KVPAIR:
				area = &areas.back();
				if (name == "name")
					area->name = value;
				else if (name == "owner")
					area->owner = value;
				else if (name == "hidden")
					area->hidden = value == "true" ? true : false;
				else if (name == "pos1")
				{
					Strfnd f(value);
					f.Next("(");
					area->box.mMinEdge = {
						(short)atoi(f.Next(",").c_str()),
						(short)atoi(f.Next(",").c_str()),
						(short)atoi(f.Next(")").c_str()) };
				}
				else if (name == "pos2")
				{
					Strfnd f(value);
					f.Next("(");
					area->box.mMaxEdge = {
						(short)atoi(f.Next(",").c_str()),
						(short)atoi(f.Next(",").c_str()),
						(short)atoi(f.Next(")").c_str()) };
				}
                break;
            case APE_END:
                break;
            case APE_GROUP: 
            {
				areas.emplace_back(Area());
                break;
            }
            case APE_MULTILINE:
                break;
        }
    }

	for (auto area : areas)
		InsertArea(&area);
}

void AreaStore::InvalidateCache()
{
	if (mCacheEnabled)
		mResCache.Invalidate();
}

void AreaStore::SetCacheParams(bool enabled, uint8_t blockRadius, size_t limit)
{
	mCacheEnabled = enabled;
	mCacheBlockRadius = std::max(blockRadius, (uint8_t)16);
	mResCache.SetLimit(std::max(limit, (size_t)20));
	InvalidateCache();
}

void AreaStore::CacheMiss(void* data, const Vector3<short>& mpos, std::vector<Area*>* dest)
{
	AreaStore* as = (AreaStore*)data;
	uint8_t r = as->mCacheBlockRadius;

	// get the points at the edges of the mapblock
	Vector3<short> minEdge{
		(short)(mpos[0] * r), (short)(mpos[1] * r), (short)(mpos[2] * r) };
	Vector3<short> maxEdge{
		(short)(minEdge[0] + r - 1), (short)(minEdge[1] + r - 1), (short)(minEdge[2] + r - 1)};
	BoundingBox<short> box(minEdge, maxEdge);

	as->GetAreasInArea(dest, box, true);

	/* infostream << "Cache miss with " << dest->size() << " areas, between ("
			<< minedge.X << ", " << minedge.Y << ", " << minedge.Z << ") and ("
			<< maxedge.X << ", " << maxedge.Y << ", " << maxedge.Z << ")" << std::endl; // */
}

void AreaStore::GetAreasForPosition(std::vector<Area*>* result, Vector3<short> pos)
{
	if (mCacheEnabled) 
	{
		Vector3<short> block = GetContainerPosition(pos, mCacheBlockRadius);
		const std::vector<Area*>* preList = mResCache.LookupCache(block);

		size_t sizePreList = preList->size();
		for (size_t i = 0; i < sizePreList; i++) 
		{
			Area* area = (*preList)[i];
			if (area->box.IsPointInside(pos))
				result->push_back(area);
		}
	} 
	else return GetAreasForPositionImpl(result, pos);
}


////
// VectorAreaStore
////


bool VectorAreaStore::InsertArea(Area* area)
{
	std::pair<AreaMap::iterator, bool> res = 
		mAreasMap.insert(std::make_pair(area->name, *area));
	if (!res.second)
	{
		// ID is not unique
		return false;
	}
	mAreas.push_back(&res.first->second);
	InvalidateCache();
	return true;
}

bool VectorAreaStore::RemoveArea(const std::string& name)
{
	AreaMap::iterator it = mAreasMap.find(name);
	if (it == mAreasMap.end())
		return false;
	Area* area = &it->second;
	std::vector<Area*>::iterator vIt = mAreas.begin();
	for (; vIt != mAreas.end(); ++vIt) 
	{
		if (*vIt == area)
		{
			mAreas.erase(vIt);
			break;
		}
	}
	mAreasMap.erase(it);
	InvalidateCache();
	return true;
}

void VectorAreaStore::GetAreasForPositionImpl(std::vector<Area*>* result, Vector3<short> pos)
{
	for (Area* area : mAreas) 
		if (area->box.IsPointInside(pos))
			result->push_back(area);
}

void VectorAreaStore::GetAreasInArea(std::vector<Area*>* result,
	BoundingBox<short> box, bool acceptOverlap)
{
	for (Area* area : mAreas) 
		if (acceptOverlap ? area->box.Intersect(box) : area->box.IsFullInside(box))
			result->push_back(area);
}