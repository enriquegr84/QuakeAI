/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2015-2018 paramat

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

#include "DungeonGenerator.h"
#include "MapGenerator.h"
#include "MapBlock.h"
#include "MapNode.h"
#include "Map.h"

#include "Voxel.h"

#include "../../Utils/Noise.h"

#include "../../Graphics/Node.h"

#include "Application/Settings.h"

//#define DGEN_USE_TORCHES


///////////////////////////////////////////////////////////////////////////////


DungeonGen::DungeonGen(const NodeManager* nodeMgr, GenerateNotifier* genNotify, DungeonParams* dparams)
{
	LogAssert(nodeMgr, "invalid node manager");

	this->mNodeMgr = nodeMgr;
	this->mGenNotify = genNotify;

#ifdef DGEN_USE_TORCHES
	mContentTorch  = nodeMgr->GetId("default:torch");
#endif

	if (dparams) 
    {
        mDungeonParams = *dparams;
	} 
    else 
    {
		// Default dungeon parameters
        mDungeonParams.seed = 0;

        mDungeonParams.contentWall = nodeMgr->GetId("mapgen_cobble");
        mDungeonParams.contentAltWall = nodeMgr->GetId("mapgen_mossycobble");
        mDungeonParams.contentStair = nodeMgr->GetId("mapgen_stair_cobble");

        mDungeonParams.diagonalDirections = false;
        mDungeonParams.onlyInGround = true;
        mDungeonParams.holeSize = Vector3<short>{ 1, 2, 1 };
        mDungeonParams.corridorLengthMin = 1;
        mDungeonParams.corridorLengthMax = 13;
        mDungeonParams.roomSizeMin = Vector3<short>{ 4, 4, 4 };
        mDungeonParams.roomSizeMax = Vector3<short>{ 8, 6, 8 };
        mDungeonParams.roomSizeLargeMin = Vector3<short>{ 8, 8, 8 };
        mDungeonParams.roomSizeLargeMax = Vector3<short>{ 16, 16, 16 };
        mDungeonParams.largeRoomChance = 1;
        mDungeonParams.numRooms = 8;
        mDungeonParams.numDungeons = 1;
        mDungeonParams.notifyType = GENNOTIFY_DUNGEON;

        mDungeonParams.npAltWall =
            NoiseParams(-0.4f, 1.f, Vector3<float>{40.f, 40.f, 40.f}, 32474, 6, 1.1f, 2.f);
	}
}


void DungeonGen::Generate(MMVManip* vm, unsigned int bseed, Vector3<short> nmin, Vector3<short> nmax)
{
	if (mDungeonParams.numDungeons == 0)
		return;

	LogAssert(vm, "invalid VManip");

	//TimeTaker t("gen dungeons");

	this->mMMVManip = vm;
	this->mBlockSeed = bseed;
	mRandom.Seed(bseed + 2);

	// Dungeon generator doesn't modify places which have this set
	vm->ClearFlag(VMANIP_FLAG_DUNGEON_INSIDE | VMANIP_FLAG_DUNGEON_PRESERVE);

	if (mDungeonParams.onlyInGround) 
    {
		// Set all air and liquid drawtypes to be untouchable to make dungeons generate
		// in ground only.
		// Set 'ignore' to be untouchable to prevent generation in ungenerated neighbor
		// mapchunks, to avoid dungeon rooms generating outside ground.
		// Like randomwalk caves, preserve nodes that have 'IsGroundContent = false',
		// to avoid dungeons that generate out beyond the edge of a mapchunk destroying
		// nodes added by mods in 'register_on_generated()'.
		for (int16_t z = nmin[2]; z <= nmax[2]; z++) 
        {
			for (int16_t y = nmin[1]; y <= nmax[1]; y++) 
            {
				unsigned int i = vm->mArea.Index(nmin[0], y, z);
				for (int16_t x = nmin[0]; x <= nmax[0]; x++) {
					uint16_t c = vm->mData[i].GetContent();
					NodeDrawType dtype = mNodeMgr->Get(c).drawType;
					if (dtype == NDT_AIRLIKE || dtype == NDT_LIQUID ||
							c == CONTENT_IGNORE || !mNodeMgr->Get(c).isGroundContent)
						vm->mFlags[i] |= VMANIP_FLAG_DUNGEON_PRESERVE;
					i++;
				}
			}
		}
	}

	// Add them
	for (unsigned int i = 0; i < mDungeonParams.numDungeons; i++)
        MakeDungeon(Vector3<short>{1, 1, 1} * (short)MAP_BLOCKSIZE);

	// Optionally convert some structure to alternative structure
	if (mDungeonParams.contentAltWall == CONTENT_IGNORE)
		return;

    for (int16_t z = nmin[2]; z <= nmax[2]; z++)
    {
        for (int16_t y = nmin[1]; y <= nmax[1]; y++) 
        {
            unsigned int i = vm->mArea.Index(nmin[0], y, z);
            for (int16_t x = nmin[0]; x <= nmax[0]; x++) 
            {
                if (vm->mData[i].GetContent() == mDungeonParams.contentWall) 
                    if (NoisePerlin3D(&mDungeonParams.npAltWall, x, y, z, mBlockSeed) > 0.0f)
                        vm->mData[i].SetContent(mDungeonParams.contentAltWall);

                i++;
            }
        }
    }

	//printf("== gen dungeons: %dms\n", t.stop());
}


void DungeonGen::MakeDungeon(Vector3<short> startPadding)
{
	const Vector3<short>& areasize = mMMVManip->mArea.GetExtent();
	Vector3<short> roomSize;
	Vector3<short> roomPlace;

	/*
		Find place for first room.
	*/
	bool fits = false;
	for (unsigned int i = 0; i < 100 && !fits; i++) 
    {
		if (mDungeonParams.largeRoomChance >= 1) 
        {
			roomSize[2] = mRandom.Range(
                mDungeonParams.roomSizeLargeMin[2], mDungeonParams.roomSizeLargeMax[2]);
			roomSize[1] = mRandom.Range(
                mDungeonParams.roomSizeLargeMin[1], mDungeonParams.roomSizeLargeMax[1]);
			roomSize[0] = mRandom.Range(
                mDungeonParams.roomSizeLargeMin[0], mDungeonParams.roomSizeLargeMax[0]);
		} else 
        {
			roomSize[2] = mRandom.Range(
                mDungeonParams.roomSizeMin[2], mDungeonParams.roomSizeMax[2]);
			roomSize[1] = mRandom.Range(
                mDungeonParams.roomSizeMin[1], mDungeonParams.roomSizeMax[1]);
			roomSize[0] = mRandom.Range(
                mDungeonParams.roomSizeMin[0], mDungeonParams.roomSizeMax[0]);
		}

		// startPadding is used to disallow starting the generation of
		// a dungeon in a neighboring generation chunk
		roomPlace = mMMVManip->mArea.mMinEdge + startPadding;
		roomPlace[2] += mRandom.Range(0, areasize[2] - roomSize[2] - startPadding[2]);
		roomPlace[1] += mRandom.Range(0, areasize[1] - roomSize[1] - startPadding[1]);
		roomPlace[0] += mRandom.Range(0, areasize[0] - roomSize[0] - startPadding[0]);

		/*
			Check that we're not putting the room to an unknown place,
			otherwise it might end up floating in the air
		*/
		fits = true;
        for (int16_t z = 0; z < roomSize[2]; z++) 
        {
            for (int16_t y = 0; y < roomSize[1]; y++) 
            {
                for (int16_t x = 0; x < roomSize[0]; x++) 
                {
                    Vector3<short> p = roomPlace + Vector3<short>{x, y, z};
                    unsigned int vi = mMMVManip->mArea.Index(p);
                    if ((mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE) ||
                        mMMVManip->mData[vi].GetContent() == CONTENT_IGNORE) 
                    {
                        fits = false;
                        break;
                    }
                }
            }
        }
	}
	// No place found
	if (!fits)
		return;

	/*
		Stores the center position of the last room made, so that
		a new corridor can be started from the last room instead of
		the new room, if chosen so.
	*/
    Vector3<short> lastRoomCenter = roomPlace + 
        Vector3<short>{(short)(roomSize[0] / 2), 1, (short)(roomSize[2] / 2)};

	for (unsigned int i = 0; i < mDungeonParams.numRooms; i++)
    {
		// Make a room to the determined place
		MakeRoom(roomSize, roomPlace);

        Vector3<short> roomCenter = roomPlace + 
            Vector3<short>{(short)(roomSize[0] / 2), (short)1, (short)(roomSize[2] / 2)};
		if (mGenNotify)
			mGenNotify->AddEvent(mDungeonParams.notifyType, roomCenter);

#ifdef DGEN_USE_TORCHES
		// Place torch at room center (for testing)
		vm->mData[vm->mArea.index(roomCenter)] = MapNode(mContentTorch);
#endif

		// Quit if last room
		if (i + 1 == mDungeonParams.numRooms)
			break;

		// Determine walker start position

		bool startInLastRoom = (mRandom.Range(0, 2) != 0);

		Vector3<short> walkerStartPlace;

		if (startInLastRoom) 
        {
			walkerStartPlace = lastRoomCenter;
		} 
        else
        {
			walkerStartPlace = roomCenter;
			// Store center of current room as the last one
			lastRoomCenter = roomCenter;
		}

		// Create walker and find a place for a door
		Vector3<short> doorPlace;
		Vector3<short> doorDir;

		mPosition = walkerStartPlace;
		if (!FindPlaceForDoor(doorPlace, doorDir))
			return;

        if (mRandom.Range(0, 1) == 0)
        {
            // Make the door
            MakeDoor(doorPlace, doorDir);
        }
        else
        {
            // Don't actually make a door
            doorPlace -= doorDir;
        }

		// Make a random corridor starting from the door
		Vector3<short> corridorEnd;
		Vector3<short> corridorEndDir;
		MakeCorridor(doorPlace, doorDir, corridorEnd, corridorEndDir);

		// Find a place for a random sized room
		if (mDungeonParams.largeRoomChance > 1 && mRandom.Range(1, mDungeonParams.largeRoomChance) == 1) 
        {
			// Large room
			roomSize[2] = mRandom.Range(
                mDungeonParams.roomSizeLargeMin[2], mDungeonParams.roomSizeLargeMax[2]);
			roomSize[1] = mRandom.Range(
                mDungeonParams.roomSizeLargeMin[1], mDungeonParams.roomSizeLargeMax[1]);
			roomSize[0] = mRandom.Range(
                mDungeonParams.roomSizeLargeMin[0], mDungeonParams.roomSizeLargeMax[0]);
		} 
        else 
        {
			roomSize[2] = mRandom.Range(
                mDungeonParams.roomSizeMin[2], mDungeonParams.roomSizeMax[2]);
			roomSize[1] = mRandom.Range(
                mDungeonParams.roomSizeMin[1], mDungeonParams.roomSizeMax[1]);
			roomSize[0] = mRandom.Range(
                mDungeonParams.roomSizeMin[0], mDungeonParams.roomSizeMax[0]);
		}

		mPosition = corridorEnd;
		mDirection = corridorEndDir;
		if (!FindPlaceForRoomDoor(roomSize, doorPlace, doorDir, roomPlace))
			return;

        if (mRandom.Range(0, 1) == 0)
        {
            // Make the door
            MakeDoor(doorPlace, doorDir);
        }
        else
        {
            // Don't actually make a door
            roomPlace -= doorDir;
        }
	}
}


void DungeonGen::MakeRoom(Vector3<short> roomSize, Vector3<short> roomPlace)
{
	MapNode nodeWall(mDungeonParams.contentWall);
	MapNode nodeAir(CONTENT_AIR);

	// Make +-X walls
    for (int16_t z = 0; z < roomSize[2]; z++)
    {
        for (int16_t y = 0; y < roomSize[1]; y++)
        {
            {
                Vector3<short> p = roomPlace + Vector3<short>{0, y, z};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE)
                    continue;
                mMMVManip->mData[vi] = nodeWall;
            }
            {
                Vector3<short> p = roomPlace + Vector3<short>{(short)(roomSize[0] - 1), y, z};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE)
                    continue;
                mMMVManip->mData[vi] = nodeWall;
            }
        }
    }

	// Make +-Z walls
    for (int16_t x = 0; x < roomSize[0]; x++)
    {
        for (int16_t y = 0; y < roomSize[1]; y++) 
        {
            {
                Vector3<short> p = roomPlace + Vector3<short>{x, y, 0};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE)
                    continue;
                mMMVManip->mData[vi] = nodeWall;
            }
            {
                Vector3<short> p = roomPlace + Vector3<short>{x, y, (short)(roomSize[2] - 1)};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE)
                    continue;
                mMMVManip->mData[vi] = nodeWall;
            }
        }
    }

	// Make +-Y walls (floor and ceiling)
    for (int16_t z = 0; z < roomSize[2]; z++)
    {
        for (int16_t x = 0; x < roomSize[0]; x++) 
        {
            {
                Vector3<short> p = roomPlace + Vector3<short>{x, 0, z};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE)
                    continue;
                mMMVManip->mData[vi] = nodeWall;
            }
            {
                Vector3<short> p = roomPlace + Vector3<short>{x, (short)(roomSize[1] - 1), z};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & VMANIP_FLAG_DUNGEON_UNTOUCHABLE)
                    continue;
                mMMVManip->mData[vi] = nodeWall;
            }
        }
    }

	// Fill with air
    for (int16_t z = 1; z < roomSize[2] - 1; z++)
    {
        for (int16_t y = 1; y < roomSize[1] - 1; y++)
        {
            for (int16_t x = 1; x < roomSize[0] - 1; x++) 
            {
                Vector3<short> p = roomPlace + Vector3<short>{x, y, z};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                mMMVManip->mFlags[vi] |= VMANIP_FLAG_DUNGEON_UNTOUCHABLE;
                mMMVManip->mData[vi] = nodeAir;
            }
        }
    }
}


void DungeonGen::MakeFill(Vector3<short> place, Vector3<short> size,
	unsigned char avoidFlags, MapNode n, unsigned char orFlags)
{
    for (int16_t z = 0; z < size[2]; z++)
    {
        for (int16_t y = 0; y < size[1]; y++)
        {
            for (int16_t x = 0; x < size[0]; x++)
            {
                Vector3<short> p = place + Vector3<short>{x, y, z};
                if (!mMMVManip->mArea.Contains(p))
                    continue;
                unsigned int vi = mMMVManip->mArea.Index(p);
                if (mMMVManip->mFlags[vi] & avoidFlags)
                    continue;
                mMMVManip->mFlags[vi] |= orFlags;
                mMMVManip->mData[vi] = n;
            }
        }
    }
}


void DungeonGen::MakeHole(Vector3<short> place)
{
	MakeFill(place, mDungeonParams.holeSize, 0, MapNode(CONTENT_AIR), VMANIP_FLAG_DUNGEON_INSIDE);
}


void DungeonGen::MakeDoor(Vector3<short> doorPlace, Vector3<short> doorDir)
{
	MakeHole(doorPlace);

#ifdef DGEN_USE_TORCHES
	// Place torch (for testing)
    mMMVManip->mData[vm->mArea.index(doorPlace)] = MapNode(mContentTorch);
#endif
}


void DungeonGen::MakeCorridor(Vector3<short> doorPlace, Vector3<short> doorDir,
	Vector3<short>& resultPlace, Vector3<short>& resultDir)
{
	MakeHole(doorPlace);
	Vector3<short> p0 = doorPlace;
	Vector3<short> dir = doorDir;
	unsigned int length = mRandom.Range(mDungeonParams.corridorLengthMin, mDungeonParams.corridorLengthMax);
	unsigned int partlength = mRandom.Range(mDungeonParams.corridorLengthMin, mDungeonParams.corridorLengthMax);
	unsigned int partcount = 0;
	int16_t makeStairs = 0;

	if (mRandom.Next() % 2 == 0 && partlength >= 3)
		makeStairs = mRandom.Next() % 2 ? 1 : -1;

	for (unsigned int i = 0; i < length; i++) 
    {
		Vector3<short> p = p0 + dir;
		if (partcount != 0)
			p[1] += makeStairs;

		// Check segment of minimum size corridor is in voxelmanip
        if (mMMVManip->mArea.Contains(p) && mMMVManip->mArea.Contains(p + Vector3<short>{0, 1, 0})) 
        {
			if (makeStairs) 
            {
                MakeFill(p + Vector3<short>{-1, -1, -1},
                    mDungeonParams.holeSize + Vector3<short>{2, 3, 2},
					VMANIP_FLAG_DUNGEON_UNTOUCHABLE,
					MapNode(mDungeonParams.contentWall), 0);
				MakeFill(p, mDungeonParams.holeSize, VMANIP_FLAG_DUNGEON_UNTOUCHABLE,
					MapNode(CONTENT_AIR), VMANIP_FLAG_DUNGEON_INSIDE);
				MakeFill(p - dir, mDungeonParams.holeSize, VMANIP_FLAG_DUNGEON_UNTOUCHABLE,
					MapNode(CONTENT_AIR), VMANIP_FLAG_DUNGEON_INSIDE);

				// TODO: fix stairs code so it works 100%
				// (quite difficult)

				// exclude stairs from the bottom step
				// exclude stairs from diagonal steps
				if (((dir[0] ^ dir[2]) & 1) &&
                    (((makeStairs ==  1) && i != 0) ||
                    ((makeStairs == -1) && i != length - 1))) 
                {
					// rotate face 180 deg if
					// making stairs backwards
					int facedir = DirectionToFaceDirection(dir * makeStairs);
					Vector3<short> ps = p;
					uint16_t stairWidth = (dir[2] != 0) ? 
                        mDungeonParams.holeSize[0] : mDungeonParams.holeSize[2];
					// Stair width direction vector
                    Vector3<short> swv = (dir[2] != 0) ? 
                        Vector3<short>{1, 0, 0} : Vector3<short>{ 0, 0, 1 };

					for (uint16_t st = 0; st < stairWidth; st++) 
                    {
						if (makeStairs == -1) 
                        {
							unsigned int vi = mMMVManip->mArea.Index(ps[0] - dir[0], ps[1] - 1, ps[2] - dir[2]);
                            if (mMMVManip->mArea.Contains(ps + Vector3<short>{(short)-dir[0], (short)-1, (short)-dir[2]}) &&
                                mMMVManip->mData[vi].GetContent() == mDungeonParams.contentWall) 
                            {
                                mMMVManip->mFlags[vi] |= VMANIP_FLAG_DUNGEON_UNTOUCHABLE;
                                mMMVManip->mData[vi] = MapNode(mDungeonParams.contentStair, 0, facedir);
							}
						} 
                        else if (makeStairs == 1) 
                        {
							unsigned int vi = mMMVManip->mArea.Index(ps[0], ps[1] - 1, ps[2]);
                            if (mMMVManip->mArea.Contains(ps + Vector3<short>{0, -1, 0}) &&
                                mMMVManip->mData[vi].GetContent() == mDungeonParams.contentWall) 
                            {
                                mMMVManip->mFlags[vi] |= VMANIP_FLAG_DUNGEON_UNTOUCHABLE;
                                mMMVManip->mData[vi] = MapNode(mDungeonParams.contentStair, 0, facedir);
							}
						}
						ps += swv;
					}
				}
			} 
            else 
            {
                MakeFill(p + Vector3<short>{-1, -1, -1},
                    mDungeonParams.holeSize + Vector3<short>{2, 2, 2},
					VMANIP_FLAG_DUNGEON_UNTOUCHABLE,
					MapNode(mDungeonParams.contentWall), 0);
				MakeHole(p);
			}

			p0 = p;
		} 
        else 
        {
			// Can't go here, turn away
			dir = TurnXZ(dir, mRandom.Range(0, 1));
			makeStairs = -makeStairs;
			partcount = 0;
			partlength = mRandom.Range(1, length);
			continue;
		}

		partcount++;
		if (partcount >= partlength) 
        {
			partcount = 0;

			RandomTurn(mRandom, dir);

			partlength = mRandom.Range(1, length);

			makeStairs = 0;
			if (mRandom.Next() % 2 == 0 && partlength >= 3)
				makeStairs = mRandom.Next() % 2 ? 1 : -1;
		}
	}
	resultPlace = p0;
	resultDir = dir;
}


bool DungeonGen::FindPlaceForDoor(Vector3<short>& resultPlace, Vector3<short>& resultDir)
{
	for (unsigned int i = 0; i < 100; i++) 
    {
		Vector3<short> p = mPosition + mDirection;
        Vector3<short> p1 = p + Vector3<short>{0, 1, 0};
		if (!mMMVManip->mArea.Contains(p) || !mMMVManip->mArea.Contains(p1) || i % 4 == 0) 
        {
			RandomizeDirection();
			continue;
		}
		if (mMMVManip->GetNodeNoExNoEmerge(p).GetContent() == mDungeonParams.contentWall &&
            mMMVManip->GetNodeNoExNoEmerge(p1).GetContent() == mDungeonParams.contentWall)
        {
			// Found wall, this is a good place!
			resultPlace = p;
			resultDir = mDirection;
			// Randomize next direction
			RandomizeDirection();
			return true;
		}
		/*
			Determine where to move next
		*/
		// Jump one up if the actual space is there
		if (mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, 0, 0}).GetContent() == mDungeonParams.contentWall &&
            mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, 1, 0}).GetContent() == CONTENT_AIR &&
            mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, 2, 0}).GetContent() == CONTENT_AIR)
            p += Vector3<short>{0, 1, 0};
		// Jump one down if the actual space is there
		if (mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, 1, 0}).GetContent() == mDungeonParams.contentWall &&
            mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, 0, 0}).GetContent() == CONTENT_AIR &&
            mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, -1, 0}).GetContent() == CONTENT_AIR)
            p += Vector3<short>{0, -1, 0};
		// Check if walking is now possible
		if (mMMVManip->GetNodeNoExNoEmerge(p).GetContent() != CONTENT_AIR ||
            mMMVManip->GetNodeNoExNoEmerge(p +
                Vector3<short>{0, 1, 0}).GetContent() != CONTENT_AIR) 
        {
			// Cannot continue walking here
			RandomizeDirection();
			continue;
		}
		// Move there
		mPosition = p;
	}
	return false;
}


bool DungeonGen::FindPlaceForRoomDoor(Vector3<short> roomSize, Vector3<short>& resultDoorPlace,
	Vector3<short>& resultDoorDir, Vector3<short>& resultRoomPlace)
{
	for (int16_t trycount = 0; trycount < 30; trycount++) 
    {
		Vector3<short> doorPlace;
		Vector3<short> doorDir;
		bool r = FindPlaceForDoor(doorPlace, doorDir);
		if (!r)
			continue;
		Vector3<short> roomPlace;
		// X east, Z north, Y up
        if (doorDir == Vector3<short>{1, 0, 0}) // X+
			roomPlace = doorPlace + 
                Vector3<short>{0, -1, (short)mRandom.Range(-roomSize[2] + 2, -2)};
        if (doorDir == Vector3<short>{-1, 0, 0}) // X-
			roomPlace = doorPlace +
                Vector3<short>{(short)(-roomSize[0] + 1, -1), (short)mRandom.Range(-roomSize[2] + 2, -2)};
        if (doorDir == Vector3<short>{0, 0, 1}) // Z+
			roomPlace = doorPlace +
                Vector3<short>{(short)mRandom.Range(-roomSize[0] + 2, -2), -1, 0};
        if (doorDir == Vector3<short>{0, 0, -1}) // Z-
			roomPlace = doorPlace +
                Vector3<short>{(short)mRandom.Range(-roomSize[0] + 2, -2), -1, (short)(-roomSize[2] + 1)};

		// Check fit
		bool fits = true;
        for (int16_t z = 1; z < roomSize[2] - 1; z++)
        {
            for (int16_t y = 1; y < roomSize[1] - 1; y++)
            {
                for (int16_t x = 1; x < roomSize[0] - 1; x++) 
                {
                    Vector3<short> p = roomPlace + Vector3<short>{x, y, z};
                    if (!mMMVManip->mArea.Contains(p)) 
                    {
                        fits = false;
                        break;
                    }
                    if (mMMVManip->mFlags[mMMVManip->mArea.Index(p)] & VMANIP_FLAG_DUNGEON_INSIDE) 
                    {
                        fits = false;
                        break;
                    }
                }
            }
        }
		if (!fits) 
        {
			// Find new place
			continue;
		}
		resultDoorPlace = doorPlace;
		resultDoorDir = doorDir;
		resultRoomPlace = roomPlace;
		return true;
	}
	return false;
}


Vector3<short> RandomOrthoDirection(PseudoRandom& random, bool diagonalDirections)
{
	// Make diagonal directions somewhat rare
	if (diagonalDirections && (random.Next() % 4 == 0)) 
    {
		Vector3<short> dir;
		int trycount = 0;

		do 
        {
			trycount++;

			dir[2] = random.Next() % 3 - 1;
			dir[1] = 0;
			dir[0] = random.Next() % 3 - 1;
		} while ((dir[0] == 0 || dir[2] == 0) && trycount < 10);

		return dir;
	}

	if (random.Next() % 2 == 0)
        return random.Next() % 2 ? Vector3<short>{-1, 0, 0} : Vector3<short>{ 1, 0, 0 };

    return random.Next() % 2 ? Vector3<short>{0, 0, -1} : Vector3<short>{ 0, 0, 1 };
}


Vector3<short> TurnXZ(Vector3<short> olddir, int t)
{
	Vector3<short> dir;
	if (t == 0) 
    {
		// Turn right
		dir[0] = olddir[2];
		dir[2] = -olddir[0];
		dir[1] = olddir[1];
	} 
    else
    {
		// Turn left
		dir[0] = -olddir[2];
		dir[2] = olddir[0];
		dir[1] = olddir[1];
	}
	return dir;
}


void RandomTurn(PseudoRandom& random, Vector3<short>& dir)
{
	int turn = random.Range(0, 2);
	if (turn == 0) 
    {
		// Go straight: nothing to do
		return;
	} 
    else if (turn == 1) 
    {
		// Turn right
		dir = TurnXZ(dir, 0);
	} 
    else 
    {
		// Turn left
		dir = TurnXZ(dir, 1);
	}
}


int DirectionToFaceDirection(Vector3<short> d)
{
	if (abs(d[0]) > abs(d[2]))
		return d[0] < 0 ? 3 : 1;

	return d[2] < 0 ? 2 : 0;
}
