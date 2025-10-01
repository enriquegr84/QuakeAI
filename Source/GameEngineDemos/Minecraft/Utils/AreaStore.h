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

#ifndef AREASTORE_H
#define AREASTORE_H

#include "Util.h"
#include "Noise.h" // for PcgRandom


enum AreaParseEvent
{
	APE_NONE,
	APE_INVALID,
	APE_COMMENT,
	APE_KVPAIR,
	APE_END,
	APE_GROUP,
	APE_MULTILINE
};

struct Area 
{
	Area(const std::string& n = "") : name(n) {}

	Area(const std::string& n, const std::string& o, bool h,
		const Vector3<short>& mine, const Vector3<short>& maxe) :
		name(n), owner(o), hidden(h)
	{
		box = BoundingBox<short>(mine, maxe);
		SortBoxVertices(box.mMinEdge, box.mMaxEdge);
	}

	bool hidden = false;
	std::string name, owner;
	BoundingBox<short> box;
};


class AreaStore 
{
public:
	AreaStore() :
		mResCache(1000, &CacheMiss, this)
	{}

	virtual ~AreaStore() = default;

	static AreaStore* GetOptimalImplementation();

	virtual void Reserve(size_t count) {};
	size_t Size() const { return mAreasMap.size(); }

	/// Add an area to the store.
	/// Updates the area's ID if it hasn't already been set.
	/// @return Whether the area insertion was successful.
	virtual bool InsertArea(Area* area) = 0;

	/// Removes an area from the store by ID.
	/// @return Whether the area was in the store and removed.
	virtual bool RemoveArea(const std::string& name) = 0;

	/// Finds areas that the passed position is contained in.
	/// Stores output in passed vector.
	void GetAreasForPosition(std::vector<Area*>* result, Vector3<short> pos);

	/// Finds areas that are completely contained inside the area defined
	/// by the passed edges.  If @p accept_overlap is true this finds any
	/// areas that intersect with the passed area at any point.
	virtual void GetAreasInArea(std::vector<Area*>* result,
		BoundingBox<short> box, bool acceptOverlap) = 0;

	/// Sets cache parameters.
	void SetCacheParams(bool enabled, uint8_t blockRadius, size_t limit);

	/// Returns a pointer to the area coresponding to the passed ID,
	/// or NULL if it doesn't exist.
	const Area* GetArea(const std::string& name) const;

	/// Serializes the store's areas to a binary ostream.
	void Serialize(std::ostream& os) const;

	/// Deserializes the Areas from a binary istream.
	/// This does not currently clear the AreaStore before adding the
	/// areas, making it possible to deserialize multiple serialized
	/// AreaStores.
	void Deserialize(std::istream& is);

protected:
	/// Invalidates the getAreasForPos cache.
	/// Call after adding or removing an area.
	void InvalidateCache();

	/// Implementation of getAreasForPos.
	/// GetAreasForPosition calls this if the cache is disabled.
	virtual void GetAreasForPositionImpl(std::vector<Area*>* result, Vector3<short> pos) = 0;

	// Note: This can't be an unordered_map, since all
	// references would be invalidated on rehash.
	typedef std::map<std::string, Area> AreaMap;
	AreaMap mAreasMap;

private:
	/// Called by the cache when a value isn't found in the cache.
	static void CacheMiss(void* data, const Vector3<short>& mpos, std::vector<Area*>* dest);

	AreaParseEvent ParseObject(const std::string& line, std::string& name, std::string& value);

	bool mCacheEnabled = false;
	/// Range, in nodes, of the getAreasForPos cache.
	/// If you modify this, call invalidateCache()
	uint8_t mCacheBlockRadius = 64;
	LRUCache<Vector3<short>, std::vector<Area*>> mResCache;
};


class VectorAreaStore : public AreaStore 
{
public:
	virtual void Reserve(size_t count) { mAreas.reserve(count); }
	virtual bool InsertArea(Area* area);
	virtual bool RemoveArea(const std::string& name);
	virtual void GetAreasInArea(std::vector<Area*>* result,
		BoundingBox<short> box, bool acceptOverlap);

protected:
	virtual void GetAreasForPositionImpl(std::vector<Area*>* result, Vector3<short> pos);

private:
	std::vector<Area*> mAreas;
};

#endif