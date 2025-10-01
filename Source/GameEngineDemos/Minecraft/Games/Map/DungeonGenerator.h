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

#ifndef DUNGEONGENERATOR_H
#define DUNGEONGENERATOR_H

#include "Voxel.h"

#include "../../Utils/Noise.h"

#include "MapGenerator.h"

#include "Core/OS/OS.h"

#define VMANIP_FLAG_DUNGEON_INSIDE VOXELFLAG_CHECKED1
#define VMANIP_FLAG_DUNGEON_PRESERVE VOXELFLAG_CHECKED2
#define VMANIP_FLAG_DUNGEON_UNTOUCHABLE (\
		VMANIP_FLAG_DUNGEON_INSIDE|VMANIP_FLAG_DUNGEON_PRESERVE)

class MMVManip;
class NodeManager;

Vector3<short> RandomOrthoDirection(PseudoRandom& random, bool diagonalDirections);
Vector3<short> TurnXZ(Vector3<short> olddir, int t);
void RandomTurn(PseudoRandom& random, Vector3<short>& dir);
int DirectionToFaceDirection(Vector3<short> dir);


struct DungeonParams 
{
	int seed;

	uint16_t contentWall;
	// Randomly scattered alternative wall nodes
	uint16_t contentAltWall;
	uint16_t contentStair;

	// 3D noise that determines which contentWall nodes are converted to contentAltWall
	NoiseParams npAltWall;

	// Number of dungeons generated in mapchunk. All will use the same set of dungeonparams.
	uint16_t numDungeons;
	// Dungeons only generate in ground
	bool onlyInGround;
	// Number of rooms
	uint16_t numRooms;
	// Room size random range. Includes walls / floor / ceilng
	Vector3<short> roomSizeMin;
	Vector3<short> roomSizeMax;
	// Large room size random range. Includes walls / floor / ceilng
	Vector3<short> roomSizeLargeMin;
	Vector3<short> roomSizeLargeMax;
	// Value 0 disables large rooms.
	// Value 1 results in 1 large room, the first generated room.
	// Value > 1 makes the first generated room large, all other rooms have a
	// '1 in value' chance of being large.
	uint16_t largeRoomChance;
	// Dimensions of 3D 'brush' that creates corridors.
	// Dimensions are of the empty space, not including walls / floor / ceilng.
	// Diagonal corridors must have hole width >=2 to be passable.
	// Currently, hole width >= 3 causes stair corridor bugs.
	Vector3<short> holeSize;
	// Corridor length random range
	uint16_t corridorLengthMin;
	uint16_t corridorLengthMax;
	// Diagonal corridors are possible, 1 in 4 corridors will be diagonal
	bool diagonalDirections;
	// Usually 'GENNOTIFY_DUNGEON', but mapgen v6 uses 'GENNOTIFY_TEMPLE' for
	// desert dungeons.
	GenNotifyType notifyType;
};

class DungeonGen 
{
public:
	MMVManip* mMMVManip = nullptr;
	const NodeManager* mNodeMgr;
	GenerateNotifier* mGenNotify;

	unsigned int mBlockSeed;
	PseudoRandom mRandom;
	Vector3<short> mChunkSize;

	uint16_t mContentTorch;
	DungeonParams mDungeonParams;

	// RoomWalker
	Vector3<short> mPosition;
	Vector3<short> mDirection;

	DungeonGen(const NodeManager* nodeMgr, GenerateNotifier* genNotify, DungeonParams* dParams);

	void Generate(MMVManip* vm, unsigned int bseed, Vector3<short> mFullNodeMin, Vector3<short> mFullNodeMax);

	void MakeDungeon(Vector3<short> startPadding);
	void MakeRoom(Vector3<short> roomSize, Vector3<short> roomPlace);
	void MakeCorridor(Vector3<short> doorPlace, Vector3<short> doorDir,
		Vector3<short>& resultPlace, Vector3<short>& resultDir);
	void MakeDoor(Vector3<short> doorPlace, Vector3<short> doorDir);
	void MakeFill(Vector3<short> place, Vector3<short> size, 
        unsigned char avoidFlags, MapNode n, unsigned char orFlags);
	void MakeHole(Vector3<short> place);

	bool FindPlaceForDoor(Vector3<short>& resultPlace, Vector3<short>& resultDir);
	bool FindPlaceForRoomDoor(Vector3<short> roomSize, Vector3<short>& resultDoorPlace,
        Vector3<short>& resultDoorDir, Vector3<short>& resultRoomPlace);

	inline void RandomizeDirection()
	{
		mDirection = RandomOrthoDirection(mRandom, mDungeonParams.diagonalDirections);
	}
};

extern NoiseParams NoiseParamsDungeonDensity;
extern NoiseParams NoiseParamsDungeonAltWall;

#endif