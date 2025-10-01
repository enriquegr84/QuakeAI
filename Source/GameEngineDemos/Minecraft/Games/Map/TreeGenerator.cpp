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

#include "TreeGenerator.h"

#include "Map.h"
#include "MapBlock.h"

#include "VoxelAlgorithms.h"

#include "../../Graphics/Node.h"

void MakeTree(MMVManip& vmanip, Vector3<short> p0, bool isAppleTree, const NodeManager* nodeMgr, int seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	MapNode treenode(nodeMgr->GetId("mapgen_tree"));
	MapNode leavesNode(nodeMgr->GetId("mapgen_leaves"));
	MapNode applenode(nodeMgr->GetId("mapgen_apple"));
	if (treenode == CONTENT_IGNORE)
		LogWarning("Treegen: MapGenerator alias 'mapgen_tree' is invalid!");
	if (leavesNode == CONTENT_IGNORE)
        LogWarning("Treegen: MapGenerator alias 'mapgen_leaves' is invalid!");
	if (applenode == CONTENT_IGNORE)
        LogWarning("Treegen: MapGenerator alias 'mapgen_apple' is invalid!");

	PseudoRandom pr(seed);
	int16_t trunkH = pr.Range(4, 5);
	Vector3<short> p1 = p0;
	for (int16_t ii = 0; ii < trunkH; ii++) 
    {
		if (vmanip.mArea.Contains(p1)) 
        {
			unsigned int vi = vmanip.mArea.Index(p1);
			vmanip.mData[vi] = treenode;
		}
		p1[1]++;
	}

	// p1 is now the last piece of the trunk
	p1[1] -= 1;

    VoxelArea leavesA(Vector3<short>{-2, -1, -2}, Vector3<short>{2, 2, 2});
	std::vector<unsigned char> leavesD(leavesA.GetVolume());
	for (int i = 0; i < leavesA.GetVolume(); i++)
		leavesD[i] = 0;

	// Force leaves at near the end of the trunk
	int16_t d = 1;
	for (int16_t z = -d; z <= d; z++)
	    for (int16_t y = -d; y <= d; y++)
	        for (int16_t x = -d; x <= d; x++)
                leavesD[leavesA.Index(Vector3<short>{x, y, z})] = 1;


	// Add leaves randomly
	for (unsigned int iii = 0; iii < 7; iii++) 
    {
        Vector3<short> p{
            (short)pr.Range(leavesA.mMinEdge[0], leavesA.mMaxEdge[0] - d),
            (short)pr.Range(leavesA.mMinEdge[1], leavesA.mMaxEdge[1] - d),
            (short)pr.Range(leavesA.mMinEdge[2], leavesA.mMaxEdge[2] - d) };

		for (int16_t z = 0; z <= d; z++)
		    for (int16_t y = 0; y <= d; y++)
		        for (int16_t x = 0; x <= d; x++)
                    leavesD[leavesA.Index(p + Vector3<short>{x, y, z})] = 1;
	}

	// Blit leaves to vmanip
    for (int16_t z = leavesA.mMinEdge[2]; z <= leavesA.mMaxEdge[2]; z++)
    {
        for (int16_t y = leavesA.mMinEdge[1]; y <= leavesA.mMaxEdge[1]; y++) 
        {
            Vector3<short> pmin{ leavesA.mMinEdge[0], y, z };
            unsigned int i = leavesA.Index(pmin);
            unsigned int vi = vmanip.mArea.Index(pmin + p1);
            for (int16_t x = leavesA.mMinEdge[0]; x <= leavesA.mMaxEdge[0]; x++)
            {
                Vector3<short> p{ x, y, z };
                if (vmanip.mArea.Contains(p + p1) &&
                    (vmanip.mData[vi].GetContent() == CONTENT_AIR || 
                    vmanip.mData[vi].GetContent() == CONTENT_IGNORE)) 
                {
                    if (leavesD[i] == 1) 
                    {
                        bool isApple = pr.Range(0, 99) < 10;
                        if (isAppleTree && isApple)
                            vmanip.mData[vi] = applenode;
                        else
                            vmanip.mData[vi] = leavesNode;
                    }
                }
                vi++;
                i++;
            }
        }
    }
}


// L-System tree spawner
Result SpawnLTree(LogicMap* map, Vector3<short> p0, const NodeManager* nodeMgr, const TreeDefinition& tree)
{
	std::map<Vector3<short>, MapBlock*> modifiedBlocks;
	MMVManip vmanip(map);
	Vector3<short> treeBlockPos = GetNodeBlockPosition(p0);
	
    Result result;
    vmanip.InitialEmerge(
        treeBlockPos - Vector3<short>{1, 1, 1}, 
        treeBlockPos + Vector3<short>{1, 3, 1});
	result = MakeLTree(vmanip, p0, nodeMgr, tree);
	if (result != SUCCESS)
		return result;

    BlitBackWithLight(map, &vmanip, &modifiedBlocks);

	// Send a MEET_OTHER event
	MapEditEvent evt;
	evt.type = MEET_OTHER;
	for (auto& modifiedBlock : modifiedBlocks)
		evt.modifiedBlocks.insert(modifiedBlock.first);
	map->DispatchEvent(evt);
	return SUCCESS;
}


//L-System tree generator
Result MakeLTree(MMVManip& vmanip, Vector3<short> p0, const NodeManager* nodeMgr, TreeDefinition tree)
{
	int seed;
	if (tree.explicitSeed)
		seed = tree.seed + 14002;
	else
		seed = p0[0] * 2 + p0[1] * 4 + p0[2];  // use the tree position to seed PRNG
	PseudoRandom ps(seed);

	// chance of inserting abcd rules
	double prop_a = 9;
	double prop_b = 8;
	double prop_c = 7;
	double prop_d = 6;

	//randomize tree growth level, minimum=2
	int16_t iterations = tree.iterations;
	if (tree.iterationsRandomLevel > 0)
		iterations -= ps.Range(0, tree.iterationsRandomLevel);
	if (iterations < 2)
		iterations = 2;

	int16_t MAX_ANGLE_OFFSET = 5;
	double angleInRadians = (double)tree.angle * GE_C_PI / 180;
	double angleOffsetInRadians = (int16_t)(ps.Range(0, 1) % MAX_ANGLE_OFFSET) * GE_C_PI / 180;

	//initialize rotation matrix, position and stacks for branches
    Matrix4x4<float> rotation = MakeRotationAxisRadians(GE_C_PI / 2., Vector3<float>::Unit(2));
	Vector3<float> position;
	position[0] = p0[0];
	position[1] = p0[1];
	position[2] = p0[2];
	std::stack<Matrix4x4<float>> stackOrientation;
    std::stack<Vector3<float>> stackPosition;

	//generate axiom
	std::string axiom = tree.initialAxiom;
	for (int16_t i = 0; i < iterations; i++) 
    {
		std::string temp;
		for (int16_t j = 0; j < (int16_t)axiom.size(); j++) 
        {
			char axiomChar = axiom.at(j);
			switch (axiomChar)
            {
			    case 'A':
				    temp += tree.rulesA;
				    break;
			    case 'B':
				    temp += tree.rulesB;
				    break;
			    case 'C':
				    temp += tree.rulesC;
				    break;
			    case 'D':
				    temp += tree.rulesD;
				    break;
			    case 'a':
				    if (prop_a >= ps.Range(1, 10))
					    temp += tree.rulesA;
				    break;
			    case 'b':
				    if (prop_b >= ps.Range(1, 10))
					    temp += tree.rulesB;
				    break;
			    case 'c':
				    if (prop_c >= ps.Range(1, 10))
					    temp += tree.rulesC;
				    break;
			    case 'd':
				    if (prop_d >= ps.Range(1, 10))
					    temp += tree.rulesD;
				    break;
			    default:
				    temp += axiomChar;
				    break;
			}
		}
		axiom = temp;
	}

	// Add trunk nodes below a wide trunk to avoid gaps when tree is on sloping ground
	if (tree.trunkType == "double") 
    {
		TreeTrunkPlacement(
            vmanip, Vector3<float>{position[0] + 1, position[1] - 1, position[2]}, tree);
		TreeTrunkPlacement(
            vmanip, Vector3<float>{position[0], position[1] - 1, position[2] + 1}, tree);
		TreeTrunkPlacement(
            vmanip, Vector3<float>{position[0] + 1, position[1] - 1, position[2] + 1}, tree);
	}
    else if (tree.trunkType == "crossed") 
    {
		TreeTrunkPlacement(
            vmanip, Vector3<float>{position[0] + 1, position[1] - 1, position[2]}, tree);
		TreeTrunkPlacement(
			vmanip, Vector3<float>{position[0] - 1, position[1] - 1, position[2]}, tree);
		TreeTrunkPlacement(
			vmanip, Vector3<float>{position[0], position[1] - 1, position[2] + 1}, tree);
		TreeTrunkPlacement(
			vmanip, Vector3<float>{position[0], position[1] - 1, position[2] - 1}, tree);
	}

	/* build tree out of generated axiom

	Key for Special L-System Symbols used in Axioms

    G  - move forward one unit with the pen up
    F  - move forward one unit with the pen down drawing trunks and branches
    f  - move forward one unit with the pen down drawing leaves (100% chance)
    T  - move forward one unit with the pen down drawing trunks only
    R  - move forward one unit with the pen down placing fruit
    A  - replace with rules set A
    B  - replace with rules set B
    C  - replace with rules set C
    D  - replace with rules set D
    a  - replace with rules set A, chance 90%
    b  - replace with rules set B, chance 80%
    c  - replace with rules set C, chance 70%
    d  - replace with rules set D, chance 60%
    +  - yaw the turtle right by angle degrees
    -  - yaw the turtle left by angle degrees
    &  - pitch the turtle down by angle degrees
    ^  - pitch the turtle up by angle degrees
    /  - roll the turtle to the right by angle degrees
    *  - roll the turtle to the left by angle degrees
    [  - save in stack current state info
    ]  - recover from stack state info

    */

	int16_t x,y,z;
	for (int16_t i = 0; i < (int16_t)axiom.size(); i++) 
    {
		char axiomChar = axiom.at(i);
		Matrix4x4<float> tempRotation;
		tempRotation.MakeIdentity();

		Vector3<float> dir;
		switch (axiomChar) 
        {
		    case 'G':
                dir = Vector3<float>{ 1, 0, 0 };
			    dir = TransposeMatrix(rotation, dir);
			    position += dir;
			    break;
		    case 'T':
			    TreeTrunkPlacement(
				    vmanip, Vector3<float>{position[0], position[1], position[2]}, tree);
			    if (tree.trunkType == "double" && !tree.thinBranches) 
                {
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0] + 1, position[1], position[2]}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0], position[1], position[2] + 1}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0] + 1, position[1], position[2] + 1}, tree);
			    } 
                else if (tree.trunkType == "crossed" && !tree.thinBranches) 
                {
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0] + 1, position[1], position[2]}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0] - 1, position[1], position[2]}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0], position[1], position[2] + 1}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0], position[1], position[2] - 1}, tree);
			    }
                dir = Vector3<float>{ 1, 0, 0 };
			    dir = TransposeMatrix(rotation, dir);
			    position += dir;
			    break;
		    case 'F':
			    TreeTrunkPlacement(
				    vmanip, Vector3<float>{position[0], position[1], position[2]}, tree);
			    if ((stackOrientation.empty() && tree.trunkType == "double") ||
                    (!stackOrientation.empty() && tree.trunkType == "double" && !tree.thinBranches)) 
                {
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0] + 1, position[1], position[2]}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0], position[1], position[2] + 1}, tree);
				    TreeTrunkPlacement(
					    vmanip, Vector3<float>{position[0] + 1, position[1], position[2] + 1}, tree);
			    } 
                else if ((stackOrientation.empty() && tree.trunkType == "crossed") ||
                    (!stackOrientation.empty() && tree.trunkType == "crossed" && !tree.thinBranches))
                {
                    TreeTrunkPlacement(
                        vmanip, Vector3<float>{position[0] + 1, position[1], position[2]}, tree);
                    TreeTrunkPlacement(
                        vmanip, Vector3<float>{position[0] - 1, position[1], position[2]}, tree);
                    TreeTrunkPlacement(
                        vmanip, Vector3<float>{position[0], position[1], position[2] + 1}, tree);
                    TreeTrunkPlacement(
                        vmanip, Vector3<float>{position[0], position[1], position[2] - 1}, tree);
			    }
			    if (!stackOrientation.empty()) 
                {
				    int16_t size = 1;
                    for (x = -size; x <= size; x++)
                    {
                        for (y = -size; y <= size; y++)
                        {
                            for (z = -size; z <= size; z++) 
                            {
                                if (abs(x) == size && abs(y) == size && abs(z) == size) {
                                    TreeLeavesPlacement(
                                        vmanip,
                                        Vector3<float>{position[0] + x + 1, position[1] + y, position[2] + z},
                                        ps.Next(), 
                                        tree);
                                    TreeLeavesPlacement(
                                        vmanip,
                                        Vector3<float>{position[0] + x - 1, position[1] + y, position[2] + z},
                                        ps.Next(),
                                        tree
                                    );
                                    TreeLeavesPlacement(
                                        vmanip, 
                                        Vector3<float>{position[0] + x, position[1] + y, position[2] + z + 1},
                                        ps.Next(),
                                        tree
                                    );
                                    TreeLeavesPlacement(
                                        vmanip, 
                                        Vector3<float>{position[0] + x, position[1] + y, position[2] + z - 1},
                                        ps.Next(),
                                        tree
                                    );
                                }
                            }
                        }
                    }
			    }
                dir = Vector3<float>{ 1, 0, 0 };
			    dir = TransposeMatrix(rotation, dir);
			    position += dir;
			    break;
		    case 'f':
			    TreeSingleLeavesPlacement(
				    vmanip, Vector3<float>{position[0], position[1], position[2]}, ps.Next(), tree);
                dir = Vector3<float>{ 1, 0, 0 };
			    dir = TransposeMatrix(rotation, dir);
			    position += dir;
			    break;
		    case 'R':
			    TreeFruitPlacement(
                    vmanip, Vector3<float>{position[0], position[1], position[2]}, tree);
                dir = Vector3<float>{ 1, 0, 0 };
			    dir = TransposeMatrix(rotation, dir);
			    position += dir;
			    break;

		    // turtle orientation commands
		    case '[':
			    stackOrientation.push(rotation);
			    stackPosition.push(position);
			    break;
		    case ']':
			    if (stackOrientation.empty())
				    return UNBALANCED_BRACKETS;
			    rotation = stackOrientation.top();
			    stackOrientation.pop();
			    position = stackPosition.top();
			    stackPosition.pop();
			    break;
		    case '+':
			    tempRotation = MakeRotationAxisRadians(
                    angleInRadians + angleOffsetInRadians, Vector3<float>{0, 0, 1});
			    rotation = rotation * tempRotation;
			    break;
		    case '-':
				tempRotation = MakeRotationAxisRadians(
                    angleInRadians + angleOffsetInRadians, Vector3<float>{0, 0, -1});
			    rotation = rotation * tempRotation;
			    break;
		    case '&':
				tempRotation = MakeRotationAxisRadians(
                    angleInRadians + angleOffsetInRadians, Vector3<float>{0, 1, 0});
			    rotation = rotation * tempRotation;
			    break;
		    case '^':
				tempRotation = MakeRotationAxisRadians(
                    angleInRadians + angleOffsetInRadians, Vector3<float>{0, -1, 0});
			    rotation = rotation * tempRotation;
			    break;
		    case '*':
				tempRotation = MakeRotationAxisRadians(
                    angleInRadians, Vector3<float>{1, 0, 0});
			    rotation = rotation * tempRotation;
			    break;
		    case '/':
				tempRotation = MakeRotationAxisRadians(
                    angleInRadians, Vector3<float>{-1, 0, 0});
			    rotation = rotation * tempRotation;
			    break;
		    default:
			    break;
		}
	}

	return SUCCESS;
}


void TreeTrunkPlacement(MMVManip& vmanip, Vector3<float> p0, TreeDefinition& tree)
{
    Vector3<short> p1 = Vector3<short>{ 
        (short)round(p0[0]), (short)round(p0[1]), (short)round(p0[2]) };
	if (!vmanip.mArea.Contains(p1))
		return;

	unsigned int vi = vmanip.mArea.Index(p1);
	uint16_t currentNode = vmanip.mData[vi].GetContent();
	if (currentNode != CONTENT_AIR && 
        currentNode != CONTENT_IGNORE && 
        currentNode != tree.leavesNode.GetContent() && 
        currentNode != tree.leaves2Node.GetContent() && 
        currentNode != tree.fruitNode.GetContent())
		return;
	vmanip.mData[vi] = tree.trunkNode;
}


void TreeLeavesPlacement(MMVManip& vmanip, Vector3<float> p0, PseudoRandom ps, TreeDefinition& tree)
{
	MapNode leavesNode = tree.leavesNode;
	if (ps.Range(1, 100) > 100 - tree.leaves2Chance)
		leavesNode = tree.leaves2Node;
    Vector3<short> p1 = Vector3<short>{ (
        short)round(p0[0]), (short)round(p0[1]), (short)round(p0[2]) };
	if (!vmanip.mArea.Contains(p1))
		return;

	unsigned int vi = vmanip.mArea.Index(p1);
	if (vmanip.mData[vi].GetContent() != CONTENT_AIR && 
        vmanip.mData[vi].GetContent() != CONTENT_IGNORE)
		return;

	if (tree.fruitChance > 0) 
    {
		if (ps.Range(1, 100) > 100 - tree.fruitChance)
			vmanip.mData[vmanip.mArea.Index(p1)] = tree.fruitNode;
		else
			vmanip.mData[vmanip.mArea.Index(p1)] = leavesNode;
	}
    else if (ps.Range(1, 100) > 20) 
    {
		vmanip.mData[vmanip.mArea.Index(p1)] = leavesNode;
	}
}


void TreeSingleLeavesPlacement(MMVManip& vmanip, Vector3<float> p0, PseudoRandom ps, TreeDefinition& tree)
{
	MapNode leavesNode = tree.leavesNode;
	if (ps.Range(1, 100) > 100 - tree.leaves2Chance)
		leavesNode = tree.leaves2Node;
    Vector3<short> p1 = Vector3<short>{
        (short)round(p0[0]), (short)round(p0[1]), (short)round(p0[2]) };
	if (!vmanip.mArea.Contains(p1))
		return;

	unsigned int vi = vmanip.mArea.Index(p1);
	if (vmanip.mData[vi].GetContent() != CONTENT_AIR && 
        vmanip.mData[vi].GetContent() != CONTENT_IGNORE)
		return;
	vmanip.mData[vmanip.mArea.Index(p1)] = leavesNode;
}


void TreeFruitPlacement(MMVManip& vmanip, Vector3<float> p0, TreeDefinition& tree)
{
    Vector3<short> p1 = Vector3<short>{ 
        (short)round(p0[0]), (short)round(p0[1]), (short)round(p0[2]) };
	if (!vmanip.mArea.Contains(p1))
		return;

	unsigned int vi = vmanip.mArea.Index(p1);
	if (vmanip.mData[vi].GetContent() != CONTENT_AIR && 
        vmanip.mData[vi].GetContent() != CONTENT_IGNORE)
		return;
	vmanip.mData[vmanip.mArea.Index(p1)] = tree.fruitNode;
}


void MakeJungleTree(MMVManip& vmanip, Vector3<short> p0, const NodeManager* nodeMgr, int seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	uint16_t contentTree   = nodeMgr->GetId("mapgen_jungletree");
	uint16_t contentLeaves = nodeMgr->GetId("mapgen_jungleleaves");
	if (contentTree == CONTENT_IGNORE)
		contentTree = nodeMgr->GetId("mapgen_tree");
	if (contentLeaves == CONTENT_IGNORE)
		contentLeaves = nodeMgr->GetId("mapgen_leaves");
	if (contentTree == CONTENT_IGNORE)
		LogWarning("Treegen: MapGenerator alias 'mapgen_jungletree' is invalid!");
	if (contentLeaves == CONTENT_IGNORE)
		LogWarning("Treegen: MapGenerator alias 'mapgen_jungleleaves' is invalid!");

	MapNode treenode(contentTree);
	MapNode leavesNode(contentLeaves);

	PseudoRandom pr(seed);
    for (int16_t x = -1; x <= 1; x++)
    {
        for (int16_t z = -1; z <= 1; z++)
        {
            if (pr.Range(0, 2) == 0)
                continue;
            Vector3<short> p1 = p0 + Vector3<short>{x, 0, z};
            Vector3<short> p2 = p0 + Vector3<short>{x, -1, z};
            unsigned int vi1 = vmanip.mArea.Index(p1);
            unsigned int vi2 = vmanip.mArea.Index(p2);

            if (vmanip.mArea.Contains(p2) && vmanip.mData[vi2].GetContent() == CONTENT_AIR)
                vmanip.mData[vi2] = treenode;
            else if (vmanip.mArea.Contains(p1) && vmanip.mData[vi1].GetContent() == CONTENT_AIR)
                vmanip.mData[vi1] = treenode;
        }
    }
	vmanip.mData[vmanip.mArea.Index(p0)] = treenode;

	int16_t trunkH = pr.Range(8, 12);
	Vector3<short> p1 = p0;
	for (int16_t ii = 0; ii < trunkH; ii++) 
    {
		if (vmanip.mArea.Contains(p1)) 
        {
			unsigned int vi = vmanip.mArea.Index(p1);
			vmanip.mData[vi] = treenode;
		}
		p1[1]++;
	}

	// p1 is now the last piece of the trunk
	p1[1] -= 1;

    VoxelArea leavesA(Vector3<short>{-3, -2, -3}, Vector3<short>{3, 2, 3});
	//SharedPtr<unsigned char> leavesD(new unsigned char[leavesA.getVolume()]);
	std::vector<unsigned char> leavesD(leavesA.GetVolume());
	for (int i = 0; i < leavesA.GetVolume(); i++)
		leavesD[i] = 0;

	// Force leaves at near the end of the trunk
	int16_t d = 1;
	for (int16_t z = -d; z <= d; z++)
	    for (int16_t y = -d; y <= d; y++)
	        for (int16_t x = -d; x <= d; x++)
                leavesD[leavesA.Index(Vector3<short>{x, y, z})] = 1;

	// Add leaves randomly
	for (unsigned int iii = 0; iii < 30; iii++) 
    {
        Vector3<short> p{
            (short)pr.Range(leavesA.mMinEdge[0], leavesA.mMaxEdge[0] - d),
            (short)pr.Range(leavesA.mMinEdge[1], leavesA.mMaxEdge[1] - d),
            (short)pr.Range(leavesA.mMinEdge[2], leavesA.mMaxEdge[2] - d) };

		for (int16_t z = 0; z <= d; z++)
		    for (int16_t y = 0; y <= d; y++)
		        for (int16_t x = 0; x <= d; x++)
                    leavesD[leavesA.Index(p + Vector3<short>{x, y, z})] = 1;
	}

	// Blit leaves to vmanip
    for (int16_t z = leavesA.mMinEdge[2]; z <= leavesA.mMaxEdge[2]; z++)
    {
        for (int16_t y = leavesA.mMinEdge[1]; y <= leavesA.mMaxEdge[1]; y++) 
        {
            Vector3<short> pmin{ leavesA.mMinEdge[0], y, z };
            unsigned int i = leavesA.Index(pmin);
            unsigned int vi = vmanip.mArea.Index(pmin + p1);
            for (int16_t x = leavesA.mMinEdge[0]; x <= leavesA.mMaxEdge[0]; x++) 
            {
                Vector3<short> p{ x, y, z };
                if (vmanip.mArea.Contains(p + p1) &&
                    (vmanip.mData[vi].GetContent() == CONTENT_AIR || 
                    vmanip.mData[vi].GetContent() == CONTENT_IGNORE)) 
                {
                    if (leavesD[i] == 1)
                        vmanip.mData[vi] = leavesNode;
                }
                vi++;
                i++;
            }
        }
    }
}


void MakePineTree(MMVManip& vmanip, Vector3<short> p0, const NodeManager* nodeMgr, int seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	uint16_t contentTree = nodeMgr->GetId("mapgen_pine_tree");
	uint16_t contentLeaves = nodeMgr->GetId("mapgen_pine_needles");
	uint16_t contentSnow = nodeMgr->GetId("mapgen_snow");
	if (contentTree == CONTENT_IGNORE)
		contentTree = nodeMgr->GetId("mapgen_tree");
	if (contentLeaves == CONTENT_IGNORE)
		contentLeaves = nodeMgr->GetId("mapgen_leaves");
	if (contentSnow == CONTENT_IGNORE)
        contentSnow = CONTENT_AIR;
	if (contentTree == CONTENT_IGNORE)
		LogWarning("Treegen: MapGenerator alias 'mapgen_pine_tree' is invalid!");
	if (contentLeaves == CONTENT_IGNORE)
        LogWarning("Treegen: MapGenerator alias 'mapgen_pine_needles' is invalid!");

	MapNode treenode(contentTree);
	MapNode leavesNode(contentLeaves);
	MapNode snowNode(contentSnow);

	PseudoRandom pr(seed);
	uint16_t trunkH = pr.Range(9, 13);
	Vector3<short> p1 = p0;
	for (uint16_t ii = 0; ii < trunkH; ii++) 
    {
		if (vmanip.mArea.Contains(p1)) 
        {
			unsigned int vi = vmanip.mArea.Index(p1);
			vmanip.mData[vi] = treenode;
		}
		p1[1]++;
	}

	// Make p1 the top node of the trunk
	p1[1] -= 1;

    VoxelArea leavesA(Vector3<short>{-3, -6, -3}, Vector3<short>{3, 3, 3});
	std::vector<unsigned char> leavesD(leavesA.GetVolume());
	for (int i = 0; i < leavesA.GetVolume(); i++)
		leavesD[i] = 0;

	// Upper branches
	uint16_t dev = 3;
	for (int16_t yy = -1; yy <= 1; yy++) 
    {
		for (int16_t zz = -dev; zz <= dev; zz++) 
        {
            unsigned int i = leavesA.Index(Vector3<short>{(short)-dev, yy, zz});
            unsigned int ia = leavesA.Index(Vector3<short>{(short)-dev, (short)(yy + 1), zz});
			for (int16_t xx = -dev; xx <= dev; xx++) 
            {
				if (pr.Range(0, 20) <= 19 - dev) 
                {
					leavesD[i] = 1;
					leavesD[ia] = 2;
				}
				i++;
				ia++;
			}
		}
		dev--;
	}

	// Centre top nodes
    leavesD[leavesA.Index(Vector3<short>{0, 1, 0})] = 1;
    leavesD[leavesA.Index(Vector3<short>{0, 2, 0})] = 1;
    leavesD[leavesA.Index(Vector3<short>{0, 3, 0})] = 2;

	// Lower branches
	int16_t my = -6;
	for (unsigned int iii = 0; iii < 20; iii++) 
    {
		int16_t xi = pr.Range(-3, 2);
		int16_t yy = pr.Range(-6, -5);
		int16_t zi = pr.Range(-3, 2);
		if (yy > my)
			my = yy;
		for (int16_t zz = zi; zz <= zi + 1; zz++) 
        {
            unsigned int i = leavesA.Index(Vector3<short>{xi, yy, zz});
            unsigned int ia = leavesA.Index(Vector3<short>{xi, (short)(yy + 1), zz});
			for (int xx = xi; xx <= xi + 1; xx++) 
            {
				leavesD[i] = 1;
				if (leavesD[ia] == 0)
					leavesD[ia] = 2;
				i++;
				ia++;
			}
		}
	}

	dev = 2;
	for (int16_t yy = my + 1; yy <= my + 2; yy++) 
    {
		for (int16_t zz = -dev; zz <= dev; zz++) 
        {
            unsigned int i = leavesA.Index(Vector3<short>{(short)-dev, yy, zz});
            unsigned int ia = leavesA.Index(Vector3<short>{(short)-dev, (short)(yy + 1), zz});
			for (int16_t xx = -dev; xx <= dev; xx++) 
            {
				if (pr.Range(0, 20) <= 19 - dev) 
                {
					leavesD[i] = 1;
					leavesD[ia] = 2;
				}
				i++;
				ia++;
			}
		}
		dev--;
	}

	// Blit leaves to vmanip
    for (int16_t z = leavesA.mMinEdge[2]; z <= leavesA.mMaxEdge[2]; z++)
    {
        for (int16_t y = leavesA.mMinEdge[1]; y <= leavesA.mMaxEdge[1]; y++)
        {
            Vector3<short> pmin{ leavesA.mMinEdge[0], y, z };
            unsigned int i = leavesA.Index(pmin);
            unsigned int vi = vmanip.mArea.Index(pmin + p1);
            for (int16_t x = leavesA.mMinEdge[0]; x <= leavesA.mMaxEdge[0]; x++) 
            {
                Vector3<short> p{ x, y, z };
                if (vmanip.mArea.Contains(p + p1) &&
                    (vmanip.mData[vi].GetContent() == CONTENT_AIR ||
                    vmanip.mData[vi].GetContent() == CONTENT_IGNORE ||
                    vmanip.mData[vi] == snowNode)) 
                {
                    if (leavesD[i] == 1)
                        vmanip.mData[vi] = leavesNode;
                    else if (leavesD[i] == 2)
                        vmanip.mData[vi] = snowNode;
                }
                vi++;
                i++;
            }
        }
    }
}