/*
Minetest
Copyright (C) 2016 MillersMan <millersman@users.noreply.github.com>

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

#ifndef REFLOWSCAN_H
#define REFLOWSCAN_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector3.h"

class NodeManager;
class Map;
class MapBlock;

class ReflowScan {
public:
	ReflowScan(Map* map, const NodeManager* nodeMgr);
	void Scan(MapBlock* block, std::queue<Vector3<short>>* liquidQueue);

private:

	MapBlock* LookupBlock(int x, int y, int z);
	bool IsLiquidFlowableTo(int x, int y, int z);
	bool IsLiquidHorizontallyFlowable(int x, int y, int z);
	void ScanColumn(int x, int z);

private:
	Map* mMap = nullptr;
	const NodeManager* mNodeMgr = nullptr;
	Vector3<short> mBlockPos, mRelBlockPos;
	std::queue<Vector3<short>>* mLiquidQueue = nullptr;
	MapBlock* mLookup[3 * 3 * 3];
	unsigned int mLookupStateBitset;
};

#endif