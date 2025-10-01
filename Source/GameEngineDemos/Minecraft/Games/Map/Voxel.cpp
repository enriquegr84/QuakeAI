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

#include "Voxel.h"

#include "Map.h"

#include "Core/OS/OS.h"

/*
	Debug stuff
*/
uint64_t AddAreaTime = 0;
uint64_t EmergeTime = 0;
uint64_t EmergeLoadTime = 0;
uint64_t ClearFlagTime = 0;

VoxelManipulator::~VoxelManipulator()
{
	Clear();
}

void VoxelManipulator::Clear()
{
	// Reset area to volume=0
	mArea = VoxelArea();
	delete[] mData;
	mData = nullptr;
	delete[] mFlags;
	mFlags = nullptr;
}

void VoxelManipulator::Print(std::ostream& o, const NodeManager* nodeMgr, VoxelPrintMode mode)
{
	const Vector3<short>& em = mArea.GetExtent();
	Vector3<short> of = mArea.mMinEdge;
	o<<"size: "<<em[0]<<"x"<<em[1]<<"x"<<em[2]
	 <<" offset: ("<<of[0]<<","<<of[1]<<","<<of[2]<<")"<<std::endl;

	for(int y=mArea.mMaxEdge[1]; y>=mArea.mMinEdge[1]; y--)
	{
		if(em[0] >= 3 && em[1] >= 3)
		{
			if (y==mArea.mMinEdge[1]+2) o<<"^     ";
			else if(y==mArea.mMinEdge[1]+1) o<<"|     ";
			else if(y==mArea.mMinEdge[1]+0) o<<"y x-> ";
			else o<<"      ";
		}

		for(int z=mArea.mMinEdge[2]; z<=mArea.mMaxEdge[2]; z++)
		{
			for(int x=mArea.mMinEdge[0]; x<=mArea.mMaxEdge[0]; x++)
			{
				uint8_t f = mFlags[mArea.Index(x,y,z)];
				char c;
				if(f & VOXELFLAG_NO_DATA)
					c = 'N';
				else
				{
					c = 'X';
					MapNode n = mData[mArea.Index(x,y,z)];
					unsigned short m = n.GetContent();
					uint8_t pr = n.param2;
					if(mode == VOXELPRINT_MATERIAL)
					{
						if(m <= 9)
							c = m + '0';
					}
					else if(mode == VOXELPRINT_WATERPRESSURE)
					{
						if(nodeMgr->Get(m).IsLiquid())
						{
							c = 'w';
							if(pr <= 9)
								c = pr + '0';
						}
						else if(m == CONTENT_AIR)
						{
							c = ' ';
						}
						else
						{
							c = '#';
						}
					}
					else if(mode == VOXELPRINT_LIGHT_DAY)
					{
                        if (nodeMgr->Get(m).lightSource != 0)
                        {
                            c = 'S';
                        }
                        else if (!nodeMgr->Get(m).lightPropagates)
                        {
                            c = 'X';
                        }
						else
						{
							uint8_t light = n.GetLight(LIGHTBANK_DAY, nodeMgr);
							if(light < 10)
								c = '0' + light;
							else
								c = 'a' + (light-10);
						}
					}
				}
				o<<c;
			}
			o<<' ';
		}
		o<<std::endl;
	}
}

void VoxelManipulator::AddArea(const VoxelArea& area)
{
	// Cancel if requested area has zero volume
	if (area.HasEmptyExtent())
		return;

	// Cancel if mArea already contains the requested area
	if(mArea.Contains(area))
		return;

	TimeTaker timer("AddArea", &AddAreaTime);

	// Calculate new area
	VoxelArea newArea;
	// New area is the requested area if mArea has zero volume
	if(mArea.HasEmptyExtent())
	{
		newArea = area;
	}
	// Else add requested area to mArea
	else
	{
		newArea = mArea;
		newArea.AddArea(area);
	}

	int newSize = newArea.GetVolume();

	/*dstream<<"adding area ";
	area.print(dstream);
	dstream<<", old area ";
	mArea.print(dstream);
	dstream<<", new area ";
	newArea.print(dstream);
	dstream<<", newSize="<<newSize;
	dstream<<std::endl;*/

	// Allocate new data and clear flags
	MapNode* newData = new MapNode[newSize];
	LogAssert(newData, "invalid data");
	uint8_t *newFlags = new uint8_t[newSize];
    LogAssert(newFlags, "invalid flags");
	memset(newFlags, VOXELFLAG_NO_DATA, newSize);

	// Copy old data
	int oldXWidth = mArea.mMaxEdge[0] - mArea.mMinEdge[0] + 1;
    for (int z = mArea.mMinEdge[2]; z <= mArea.mMaxEdge[2]; z++)
    {
        for (int y = mArea.mMinEdge[1]; y <= mArea.mMaxEdge[1]; y++)
        {
            unsigned int oldIndex = mArea.Index(mArea.mMinEdge[0], y, z);
            unsigned int newIndex = newArea.Index(mArea.mMinEdge[0], y, z);

            memcpy(&newData[newIndex], &mData[oldIndex], oldXWidth * sizeof(MapNode));
            memcpy(&newFlags[newIndex], &mFlags[oldIndex], oldXWidth * sizeof(uint8_t));
        }
    }

	// Replace area, data and flags

	mArea = newArea;

	MapNode* oldData = mData;
	uint8_t* oldFlags = mFlags;

	/*dstream<<"oldData="<<(int)oldData<<", newData="<<(int)newData
	<<", oldFlags="<<(int)mFlags<<", newFlags="<<(int)newFlags<<std::endl;*/

	mData = newData;
	mFlags = newFlags;

	delete[] oldData;
	delete[] oldFlags;

	//dstream<<"AddArea done"<<std::endl;
}

void VoxelManipulator::CopyFrom(MapNode* src, const VoxelArea& srcArea,
		Vector3<short> fromPos, Vector3<short> toPos, const Vector3<short>& size)
{
	/* The reason for this optimised code is that we're a member function
	 * and the data type/layout of mData is know to us: it's stored as
	 * [z*h*w + y*h + x]. Therefore we can take the calls to mArea index
	 * (which performs the preceding mapping/indexing of mData) out of the
	 * inner loop and calculate the next index as we're iterating to gain
	 * performance.
	 *
	 * srcStep and destStep is the amount required to be added to our index
	 * every time y increments. Because the destination area may be larger
	 * than the source area we need one additional variable (otherwise we could
	 * just continue adding destStep as is done for the source data): destMod.
	 * destMod is the difference in size between a "row" in the source data
	 * and a "row" in the destination data (I am using the term row loosely
	 * and for illustrative purposes). E.g.
	 *
	 * src       <-------------------->|'''''' dest mod ''''''''
	 * dest      <--------------------------------------------->
	 *
	 * destMod (it's essentially a modulus) is added to the destination index
	 * after every full iteration of the y span.
	 *
	 * This method falls under the category "linear array and incrementing
	 * index".
	 */

	int srcStep = srcArea.GetExtent()[0];
	int destStep = mArea.GetExtent()[0];
	int destMod = mArea.Index(toPos[0], toPos[1], toPos[2] + 1)
        - mArea.Index(toPos[0], toPos[1], toPos[2]) - destStep * size[1];

	int indexSrc = srcArea.Index(fromPos[0], fromPos[1], fromPos[2]);
	int indexLocal = mArea.Index(toPos[0], toPos[1], toPos[2]);

	for (short z = 0; z < size[2]; z++) 
    {
		for (short y = 0; y < size[1]; y++) 
        {
			memcpy(&mData[indexLocal], &src[indexSrc], size[0] * sizeof(*mData));
			memset(&mFlags[indexLocal], 0, size[0]);
			indexSrc += srcStep;
			indexLocal += destStep;
		}
		indexLocal += destMod;
	}
}

void VoxelManipulator::CopyTo(MapNode* dst, const VoxelArea& dstArea,
		Vector3<short> dstPos, Vector3<short> fromPos, const Vector3<short>& size)
{
    for (short z = 0; z < size[2]; z++)
    {
        for (short y = 0; y < size[1]; y++)
        {
            int indexDst = dstArea.Index(dstPos[0], dstPos[1] + y, dstPos[2] + z);
            int indexLocal = mArea.Index(fromPos[0], fromPos[1] + y, fromPos[2] + z);
            for (short x = 0; x < size[0]; x++) 
            {
                if (mData[indexLocal].GetContent() != CONTENT_IGNORE)
                    dst[indexDst] = mData[indexLocal];
                indexDst++;
                indexLocal++;
            }
        }
    }
}

/*
	Algorithms
	-----------------------------------------------------
*/

void VoxelManipulator::ClearFlag(uint8_t flags)
{
	// 0-1ms on moderate area
	TimeTaker timer("ClearFlag", &ClearFlagTime);

	//Vector3<short> s = mArea.getExtent();

	/*dstream<<"ClearFlag clearing area of size "
			<<""<<s[0]<<"x"<<s[1]<<"x"<<s[2]<<""
			<<std::endl;*/

	//int count = 0;

	/*
    for(int z=mArea.mMinEdge[2]; z<=mArea.mMaxEdge[2]; z++)
    {
	    for(int y=mArea.mMinEdge[1]; y<=mArea.mMaxEdge[1]; y++)
        {
	        for(int x=mArea.mMinEdge[0]; x<=mArea.mMaxEdge[0]; x++)
	        {
		        uint8_t f = mFlags[mArea.index(x,y,z)];
		        mFlags[mArea.index(x,y,z)] &= ~flags;
		        if(mFlags[mArea.index(x,y,z)] != f)
			        count++;
	        }
        }
    }
    */

	int volume = mArea.GetVolume();
	for(int i=0; i<volume; i++)
		mFlags[i] &= ~flags;

	/*int volume = mArea.GetVolume();
	for(int i=0; i<volume; i++)
	{
		uint8_t f = mFlags[i];
		mFlags[i] &= ~flags;
		if(mFlags[i] != f)
			count++;
	}

	dstream<<"ClearFlag changed "<<count<<" flags out of "
			<<volume<<" nodes"<<std::endl;*/
}

const MapNode VoxelManipulator::ContentIgnoreNode = MapNode(CONTENT_IGNORE);

//END
