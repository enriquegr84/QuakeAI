/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>,
Copyright (C) 2012-2018 RealBadAngel, Maciej Kasatkin
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

#ifndef TREEGENERATOR_H
#define TREEGENERATOR_H

#include "GameEngineStd.h"

#include "../../Utils/Noise.h"

#include "MapNode.h"

#include "Mathematic/Algebra/Vector3.h"
#include "Mathematic/Algebra/Matrix4x4.h"

#include "Core/OS/OS.h"

class MMVManip;
class NodeManager;
class LogicMap;

enum Result 
{
	SUCCESS,
	UNBALANCED_BRACKETS
};

struct TreeDefinition 
{
	std::string initialAxiom;
    std::string rulesA;
    std::string rulesB;
    std::string rulesC;
    std::string rulesD;

	MapNode trunkNode;
	MapNode leavesNode;
	MapNode leaves2Node;

	int leaves2Chance;
	int angle;
	int iterations;
	int iterationsRandomLevel;
	std::string trunkType;
	bool thinBranches;
	MapNode fruitNode;
	int fruitChance;
	int seed;
	bool explicitSeed;
};

// Add default tree
void MakeTree(MMVManip& vmanip, Vector3<short> p0, bool isAppleTree, const NodeManager* nodeMgr, int seed);
// Add jungle tree
void MakeJungleTree(MMVManip& vmanip, Vector3<short> p0, const NodeManager* nodeMgr, int seed);
// Add pine tree
void MakePineTree(MMVManip& vmanip, Vector3<short> p0, const NodeManager* nodeMgr, int seed);

// Add L-Systems tree (used by engine)
Result MakeLTree(MMVManip& vmanip, Vector3<short> p0, const NodeManager* nodeMgr, TreeDefinition treeDef);
// Spawn L-systems tree
Result SpawnLTree (LogicMap* map, Vector3<short> p0, const NodeManager* nodeMgr, const TreeDefinition& treeDef);

// L-System tree gen helper functions
void TreeTrunkPlacement(MMVManip& vmanip, Vector3<float> p0, TreeDefinition& treeDef);
void TreeLeavesPlacement(MMVManip& vmanip, Vector3<float> p0, PseudoRandom ps, TreeDefinition& treeDef);
void TreeSingleLeavesPlacement(MMVManip &vmanip, Vector3<float> p0, PseudoRandom ps, TreeDefinition& treeDef);
void TreeFruitPlacement(MMVManip& vmanip, Vector3<float> p0, TreeDefinition& treeDef);

#endif