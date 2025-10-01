/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef VOXEL_H
#define VOXEL_H

#include "MapNode.h"

class NodeManager;

// For VC++
#undef min
#undef max

/*
	A fast voxel manipulator class.

	In normal operation, it fetches more map when it is requested.
	It can also be used so that all allowed area is fetched at the
	start, using ManualMapVoxelManipulator.

	Not thread-safe.
*/

/*
	Debug stuff
*/
extern uint64_t EmergeTime;
extern uint64_t EmergeLoadTime;

/*
	This class resembles aabbox3d<short> a lot, but has inclusive
	edges for saner handling of integer sizes
*/
class VoxelArea
{
public:
	// Starts as zero sized
	VoxelArea() = default;

	VoxelArea(const Vector3<short>& minEdge, const Vector3<short>& maxEdge) :
		mMinEdge(minEdge), mMaxEdge(maxEdge)
	{
		CacheExtent();
	}

	VoxelArea(const Vector3<short>& p) :
		mMinEdge(p), mMaxEdge(p)
	{
		CacheExtent();
	}

	/*
		Modifying methods
	*/

	void AddArea(const VoxelArea& a)
	{
		if (HasEmptyExtent())
		{
			*this = a;
			return;
		}
		if(a.mMinEdge[0] < mMinEdge[0]) mMinEdge[0] = a.mMinEdge[0];
		if(a.mMinEdge[1] < mMinEdge[1]) mMinEdge[1] = a.mMinEdge[1];
		if(a.mMinEdge[2] < mMinEdge[2]) mMinEdge[2] = a.mMinEdge[2];
		if(a.mMaxEdge[0] > mMaxEdge[0]) mMaxEdge[0] = a.mMaxEdge[0];
		if(a.mMaxEdge[1] > mMaxEdge[1]) mMaxEdge[1] = a.mMaxEdge[1];
		if(a.mMaxEdge[2] > mMaxEdge[2]) mMaxEdge[2] = a.mMaxEdge[2];
		CacheExtent();
	}

	void AddPoint(const Vector3<short>& p)
	{
		if(HasEmptyExtent())
		{
			mMinEdge = p;
			mMaxEdge = p;
			CacheExtent();
			return;
		}
		if(p[0] < mMinEdge[0]) mMinEdge[0] = p[0];
		if(p[1] < mMinEdge[1]) mMinEdge[1] = p[1];
		if(p[2] < mMinEdge[2]) mMinEdge[2] = p[2];
		if(p[0] > mMaxEdge[0]) mMaxEdge[0] = p[0];
		if(p[1] > mMaxEdge[1]) mMaxEdge[1] = p[1];
		if(p[2] > mMaxEdge[2]) mMaxEdge[2] = p[2];
		CacheExtent();
	}

	// Pad with d nodes
	void Pad(const Vector3<short>& d)
	{
		mMinEdge -= d;
		mMaxEdge += d;
	}

	/*
		const methods
	*/

	const Vector3<short>& GetExtent() const
	{
		return mCacheExtent;
	}

	/* Because mMaxEdge and mMinEdge are included in the voxel area an empty extent
	 * is not represented by (0, 0, 0), but instead (-1, -1, -1)
	 */
	bool HasEmptyExtent() const
	{
        return mMaxEdge - mMinEdge == Vector3<short>{-1, -1, -1};
	}

	int GetVolume() const
	{
		return (int)mCacheExtent[0] * (int)mCacheExtent[1] * (int)mCacheExtent[2];
	}

	bool Contains(const VoxelArea& a) const
	{
		// No area contains an empty area
		// NOTE: Algorithms depend on this, so do not change.
		if(a.HasEmptyExtent())
			return false;

		return(
			a.mMinEdge[0] >= mMinEdge[0] && a.mMaxEdge[0] <= mMaxEdge[0] &&
			a.mMinEdge[1] >= mMinEdge[1] && a.mMaxEdge[1] <= mMaxEdge[1] &&
			a.mMinEdge[2] >= mMinEdge[2] && a.mMaxEdge[2] <= mMaxEdge[2]
		);
	}

	bool Contains(Vector3<short> p) const
	{
		return(
			p[0] >= mMinEdge[0] && p[0] <= mMaxEdge[0] &&
			p[1] >= mMinEdge[1] && p[1] <= mMaxEdge[1] &&
			p[2] >= mMinEdge[2] && p[2] <= mMaxEdge[2]
		);
	}

	bool Contains(int i) const
	{
		return (i >= 0 && i < GetVolume());
	}

	bool operator==(const VoxelArea &other) const
	{
		return (mMinEdge == other.mMinEdge
				&& mMaxEdge == other.mMaxEdge);
	}

	VoxelArea operator+(const Vector3<short>& off) const
	{
		return {mMinEdge+off, mMaxEdge+off};
	}

	VoxelArea operator-(const Vector3<short>& off) const
	{
		return {mMinEdge-off, mMaxEdge-off};
	}

	/*
		Returns 0-6 non-overlapping areas that can be added to
		a to make up this area.

		a: area inside *this
	*/
	void Diff(const VoxelArea& a, std::list<VoxelArea>& result)
	{
		/*
			This can result in a maximum of 6 areas
		*/

		// If a is an empty area, return the current area as a whole
		if(a.GetExtent() == Vector3<short>::Zero())
		{
			VoxelArea b = *this;
			if(b.GetVolume() != 0)
				result.push_back(b);
			return;
		}

		LogAssert(Contains(a), "area not contained");	// pre-condition

		// Take back area, XY inclusive
		{
            Vector3<short> min{ mMinEdge[0], mMinEdge[1], (short)(a.mMaxEdge[2] + 1) };
            Vector3<short> max{ mMaxEdge[0], mMaxEdge[1], mMaxEdge[2] };
			VoxelArea b(min, max);
			if(b.GetVolume() != 0)
				result.push_back(b);
		}

		// Take front area, XY inclusive
		{
            Vector3<short> min{ mMinEdge[0], mMinEdge[1], mMinEdge[2] };
            Vector3<short> max{ mMaxEdge[0], mMaxEdge[1], (short)(a.mMinEdge[2] - 1) };
			VoxelArea b(min, max);
			if(b.GetVolume() != 0)
				result.push_back(b);
		}

		// Take top area, X inclusive
		{
            Vector3<short> min{ mMinEdge[0], (short)(a.mMaxEdge[1] + 1), a.mMinEdge[2] };
            Vector3<short> max{ mMaxEdge[0], mMaxEdge[1], a.mMaxEdge[2] };
			VoxelArea b(min, max);
			if(b.GetVolume() != 0)
				result.push_back(b);
		}

		// Take bottom area, X inclusive
		{
            Vector3<short> min{ mMinEdge[0], mMinEdge[1], a.mMinEdge[2] };
            Vector3<short> max{ mMaxEdge[0], (short)(a.mMinEdge[1] - 1), a.mMaxEdge[2] };
			VoxelArea b(min, max);
			if(b.GetVolume() != 0)
				result.push_back(b);
		}

		// Take left area, non-inclusive
		{
            Vector3<short> min{ mMinEdge[0], a.mMinEdge[1], a.mMinEdge[2] };
            Vector3<short> max{ (short)(a.mMinEdge[0] - 1), a.mMaxEdge[1], a.mMaxEdge[2] };
			VoxelArea b(min, max);
			if(b.GetVolume() != 0)
				result.push_back(b);
		}

		// Take right area, non-inclusive
		{
            Vector3<short> min{ (short)(a.mMaxEdge[0] + 1), a.mMinEdge[1], a.mMinEdge[2] };
            Vector3<short> max{ mMaxEdge[0], a.mMaxEdge[1], a.mMaxEdge[2] };
			VoxelArea b(min, max);
			if(b.GetVolume() != 0)
				result.push_back(b);
		}

	}

	/*
		Translates position from virtual coordinates to array index
	*/
	int Index(short x, short y, short z) const
	{
		int i = (int)(z - mMinEdge[2]) * mCacheExtent[1] * mCacheExtent[0]
			+ (y - mMinEdge[1]) * mCacheExtent[0]
			+ (x - mMinEdge[0]);
		return i;
	}

	int Index(Vector3<short> p) const
	{
		return Index(p[0], p[1], p[2]);
	}

	/**
	 * Translate index in the X coordinate
	 */
	static void AddX(const Vector3<short>& extent, uint32_t& i, short a)
	{
		i += a;
	}

	/**
	 * Translate index in the Y coordinate
	 */
	static void AddY(const Vector3<short>& extent, uint32_t& i, short a)
	{
		i += a * extent[0];
	}

	/**
	 * Translate index in the Z coordinate
	 */
	static void AddZ(const Vector3<short>& extent, uint32_t& i, short a)
	{
		i += a * extent[0] * extent[1];
	}

	/**
	 * Translate index in space
	 */
	static void AddP(const Vector3<short>& extent, uint32_t& i, Vector3<short> a)
	{
		i += a[2] * extent[0] * extent[1] + a[1] * extent[0] + a[0];
	}

	/*
		Print method for debugging
	*/
	void Print(std::ostream &o) const
	{
		o << "(" << mMinEdge[0] << "," << mMinEdge[1] << "," << mMinEdge[2] << ")"
            << "(" << mMaxEdge[0] << "," << mMaxEdge[1] << "," << mMaxEdge[2] << ") ="
			<< mCacheExtent[0] << "x" << mCacheExtent[1] << "x" << mCacheExtent[2]
			<< "=" << GetVolume();
	}

	// Edges are inclusive
    Vector3<short> mMinEdge = Vector3<short>{ 1,1,1 };
	Vector3<short> mMaxEdge;

private:
	void CacheExtent()
	{
        mCacheExtent = mMaxEdge - mMinEdge + Vector3<short>{1, 1, 1};
	}

	Vector3<short> mCacheExtent = Vector3<short>::Zero();
};

// unused
#define VOXELFLAG_UNUSED   (1 << 0)
// no data about that node
#define VOXELFLAG_NO_DATA  (1 << 1)
// Algorithm-dependent
#define VOXELFLAG_CHECKED1 (1 << 2)
// Algorithm-dependent
#define VOXELFLAG_CHECKED2 (1 << 3)
// Algorithm-dependent
#define VOXELFLAG_CHECKED3 (1 << 4)
// Algorithm-dependent
#define VOXELFLAG_CHECKED4 (1 << 5)

enum VoxelPrintMode
{
	VOXELPRINT_NOTHING,
	VOXELPRINT_MATERIAL,
	VOXELPRINT_WATERPRESSURE,
	VOXELPRINT_LIGHT_DAY,
};

class VoxelManipulator
{
public:
	VoxelManipulator() = default;
	virtual ~VoxelManipulator();

	/*
		These are a bit slow and shouldn't be used internally.
		Use mData[mArea.index(p)] instead.
	*/
	MapNode GetNode(const Vector3<short>& p)
	{
		VoxelArea voxelArea(p);
		AddArea(voxelArea);

		if (mFlags[mArea.Index(p)] & VOXELFLAG_NO_DATA) 
        {
			/*dstream<<"EXCEPT: VoxelManipulator::getNode(): "
					<<"p=("<<p[0]<<","<<p[1]<<","<<p[2]<<")"
					<<", index="<<mArea.index(p)
					<<", flags="<<(int)mFlags[mArea.index(p)]
					<<" is inexistent"<<std::endl;*/
			throw InvalidPositionException("VoxelManipulator: getNode: inexistent");
		}

		return mData[mArea.Index(p)];
	}

	MapNode GetNodeNoEx(const Vector3<short>& p)
	{
		VoxelArea voxelArea(p);
		AddArea(voxelArea);

		if (mFlags[mArea.Index(p)] & VOXELFLAG_NO_DATA)
			return {CONTENT_IGNORE};

		return mData[mArea.Index(p)];
	}

	MapNode GetNodeNoExNoEmerge(const Vector3<short>& p)
	{
		if (!mArea.Contains(p))
			return {CONTENT_IGNORE};
		if (mFlags[mArea.Index(p)] & VOXELFLAG_NO_DATA)
			return {CONTENT_IGNORE};
		return mData[mArea.Index(p)];
	}
	// Stuff explodes if non-emerged area is touched with this.
	// Emerge first, and check VOXELFLAG_NO_DATA if appropriate.
	MapNode& GetNodeRefUnsafe(const Vector3<short>& p)
	{
		return mData[mArea.Index(p)];
	}

	const MapNode& GetNodeRefUnsafeCheckFlags(const Vector3<short>& p)
	{
		int index = mArea.Index(p);
		if (mFlags[index] & VOXELFLAG_NO_DATA)
			return ContentIgnoreNode;

		return mData[index];
	}

	uint8_t& GetFlagsRefUnsafe(const Vector3<short>& p)
	{
		return mFlags[mArea.Index(p)];
	}

	bool Exists(const Vector3<short>& p)
	{
		return mArea.Contains(p) && !(GetFlagsRefUnsafe(p) & VOXELFLAG_NO_DATA);
	}

	void SetNode(const Vector3<short>& p, const MapNode& n)
	{
		VoxelArea voxelArea(p);
		AddArea(voxelArea);

		mData[mArea.Index(p)] = n;
		mFlags[mArea.Index(p)] &= ~VOXELFLAG_NO_DATA;
	}

	// TODO: Should be removed and replaced with setNode
	void SetNodeNoRef(const Vector3<short>& p, const MapNode& n)
	{
		SetNode(p, n);
	}

	/*
		Set stuff if available without an emerge.
		Return false if failed.
		This is convenient but slower than playing around directly
		with the mData table with indices.
	*/
	bool SetNodeNoEmerge(const Vector3<short>& p, MapNode n)
	{
		if(!mArea.Contains(p))
			return false;
		mData[mArea.Index(p)] = n;
		return true;
	}

	/*
		Control
	*/

	virtual void Clear();

	void Print(std::ostream &o, const NodeManager* nodeMgr,
			VoxelPrintMode mode=VOXELPRINT_MATERIAL);

	void AddArea(const VoxelArea& area);

	/*
		Copy data and set flags to 0
		dstArea.getExtent() <= srcArea.getExtent()
	*/
	void CopyFrom(MapNode* src, const VoxelArea& srcArea,
			Vector3<short> fromPos, Vector3<short> toPos, const Vector3<short>& size);

	// Copy data
	void CopyTo(MapNode* dst, const VoxelArea& dstArea,
			Vector3<short> dstPos, Vector3<short> fromPos, const Vector3<short>& size);

	/*
		Algorithms
	*/

	void ClearFlag(uint8_t flag);

	/*
		Member variables
	*/

	/*
		The area that is stored in mData.
		addInternalBox should not be used if getExtent() == Vector3<short>(0,0,0)
		mMaxEdge is 1 higher than maximum allowed position
	*/
	VoxelArea mArea;

	/*
		nullptr if data size is 0 (extent (0,0,0))
		Data is stored as [z*h*w + y*h + x]
	*/
	MapNode* mData = nullptr;

	/*
		Flags of all nodes
	*/
	uint8_t* mFlags = nullptr;

	static const MapNode ContentIgnoreNode;
};

#endif