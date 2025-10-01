/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "VisualPlayer.h"
#include "ContentVisualActiveObject.h"

#include "../../Games/Environment/VisualEnvironment.h"

#include "../../Games/Map/Map.h"
#include "../../Games/Map/MapNode.h"

#include "../../MinecraftEvents.h"

static BoundingBox<float> GetNodeBoundingBox(const std::vector<BoundingBox<float>>& nodeboxes)
{
    if (nodeboxes.empty())
        return BoundingBox<float>(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    std::vector<BoundingBox<float>>::const_iterator it = nodeboxes.begin();
    BoundingBox<float> bMax = BoundingBox<float>(it->mMinEdge, it->mMaxEdge);

    ++it;
    for (; it != nodeboxes.end(); ++it)
        bMax.GrowToContain(*it);

    return bMax;
}

/*
	VisualPlayer
*/

VisualPlayer::VisualPlayer(ActorId aId, const char* name, VisualEnvironment* vEnv) : 
    Player(aId, name, vEnv->GetItemManager()), mEnvironment(vEnv)
{
    mPosition = Vector3<float>::Zero();
    mLastPosition = Vector3<float>::Zero();
    mLastSpeed = Vector3<float>::Zero();
    mSpeed = Vector3<float>::Zero();

    mStandingNode = Vector3<short>::Zero();
}

bool VisualPlayer::UpdateSneakNode(std::shared_ptr<Map> map, const Vector3<float>& position, const Vector3<float>& sneakMax)
{
    static const Vector3<short> dir9Center[9] = {
        Vector3<short>{ 0, 0,  0},
        Vector3<short>{ 1, 0,  0},
        Vector3<short>{-1, 0, 0},
		Vector3<short>{ 0, 0,  1},
		Vector3<short>{ 0, 0, -1},
		Vector3<short>{ 1, 0,  1},
		Vector3<short>{-1, 0,  1},
		Vector3<short>{ 1, 0, -1},
		Vector3<short>{-1, 0, -1}};

    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();

	MapNode node;
	bool isValidPosition;
	bool newSneakNodeExists = mSneakNodeExists;

	// We want the top of the sneak node to be below the players feet
	float positionYMod = 0.05f * BS;
	if (mSneakNodeExists)
		positionYMod = mSneakNodeBBTop.mMaxEdge[1] - positionYMod;

	// Get position of current standing node
    Vector3<float> p = position - Vector3<float>{0.0f, positionYMod, 0.0f};
    Vector3<short> currentNode;
    currentNode[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    currentNode[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    currentNode[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

	if (currentNode != mSneakNode) 
    {
		newSneakNodeExists = false;
	} 
    else 
    {
		node = map->GetNode(currentNode, &isValidPosition);
		if (!isValidPosition || !nodeMgr->Get(node).walkable)
			newSneakNodeExists = false;
	}

	// Keep old sneak node
	if (newSneakNodeExists)
		return true;

	// Get new sneak node
	mSneakLadderDetected = false;
	float minDistance = 100000.0f * BS;

	for (const auto &d : dir9Center) 
    {
		const Vector3<short> p = currentNode + d;
        const Vector2<float> diff{ position[0] - p[0] * BS, position[2] - p[2] * BS };

		if (Length(diff) > minDistance ||
            fabs(diff[0]) > (0.5f + 0.1f) * BS + sneakMax[0] ||
            fabs(diff[1]) > (0.5f + 0.1f) * BS + sneakMax[2])
			continue;

		// The node to be sneaked on has to be walkable
		node = map->GetNode(p, &isValidPosition);
		if (!isValidPosition || !nodeMgr->Get(node).walkable)
			continue;
		// And the node(s) above have to be nonwalkable
		bool ok = true;
		if (!mPhysicsOverrideSneakGlitch) 
        {
			unsigned short height =
				(unsigned short)ceilf((mCollisionBox.mMaxEdge[1] - mCollisionBox.mMinEdge[1]) / BS);
			for (unsigned short y = 1; y <= height; y++) 
            {
                node = map->GetNode(p + Vector3<short>{0, (short)y, 0}, &isValidPosition);
				if (!isValidPosition || nodeMgr->Get(node).walkable) 
                {
					ok = false;
					break;
				}
			}
		} 
        else 
        {
			// legacy behaviour: check just one node
            node = map->GetNode(p + Vector3<short>{0, 1, 0}, &isValidPosition);
			ok = isValidPosition && !nodeMgr->Get(node).walkable;
		}
		if (!ok)
			continue;

		minDistance = Length(diff);
		mSneakNode = p;
		newSneakNodeExists = true;
	}

	if (!newSneakNodeExists)
		return false;

	// Update saved top bounding box of sneak node
	node = map->GetNode(mSneakNode);
	std::vector<BoundingBox<float>> nodeboxes;
	node.GetCollisionBoxes(nodeMgr, &nodeboxes);
	mSneakNodeBBTop = GetNodeBoundingBox(nodeboxes);

	if (mPhysicsOverrideSneakGlitch) 
    {
		// Detect sneak ladder:
		// Node two meters above sneak node must be solid
        node = map->GetNode(mSneakNode + Vector3<short>{0, 2, 0}, &isValidPosition);
		if (isValidPosition && nodeMgr->Get(node).walkable) 
        {
			// Node three meters above: must be non-solid
            node = map->GetNode(
                mSneakNode + Vector3<short>{0, 3, 0}, &isValidPosition);
			mSneakLadderDetected = isValidPosition && !nodeMgr->Get(node).walkable;
		}
	}
	return true;
}

void VisualPlayer::Move(float dTime, float posMaxDist, std::vector<CollisionInfo>* collisionInfo)
{
    // Node at feet position, update each VisualEnvironment::step()
    if (!collisionInfo || collisionInfo->empty())
    {
        mStandingNode[0] = (short)((mPosition[0] + (mPosition[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        mStandingNode[1] = (short)((mPosition[1] + (mPosition[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        mStandingNode[2] = (short)((mPosition[2] + (mPosition[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    }

    // Temporary option for old move code
    if (!mPhysicsOverrideNewMove)
    {
        OldMove(dTime, posMaxDist, collisionInfo);
        return;
    }

    std::shared_ptr<Map> map = mEnvironment->GetMap();

    Vector3<float> position = GetPosition();

    // Copy parent position if local player is attached
    if (GetParent())
    {
        SetPosition(mVao->GetPosition());
        mAddedVelocity = Vector3<float>::Zero(); // ignored
        return;
    }

    PlayerSettings &playerSettings = GetPlayerSettings();

    // Skip collision detection if noClip mode is used
    bool flyAllowed = mEnvironment->CheckLocalPrivilege("fly");
    bool noClip = mEnvironment->CheckLocalPrivilege("noclip") && playerSettings.noClip;
    bool freeMove = playerSettings.freeMove && flyAllowed;

    if (noClip && freeMove)
    {
        position += mSpeed * dTime;
        SetPosition(position);

        mTouchingGround = false;
        mAddedVelocity = Vector3<float>::Zero(); // ignored
        return;
    }

    mSpeed += mAddedVelocity;
    mAddedVelocity = Vector3<float>::Zero();

    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();

    /*
        Collision detection
    */

    bool isValidPosition;
    MapNode node;
    Vector3<float> p;
    Vector3<short> pp;

    /*
        Check if player is in liquid (the oscillating value)
    */

    // If in liquid, the threshold of coming out is at higher y
    if (mInLiquid)
    {
        p = position + Vector3<float>{0.0f, BS * 0.1f, 0.0f};
        pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
        node = map->GetNode(pp, &isValidPosition);
        if (isValidPosition)
        {
            mInLiquid = nodeMgr->Get(node.GetContent()).IsLiquid();
            mLiquidViscosity = nodeMgr->Get(node.GetContent()).liquidViscosity;
        }
        else
        {
            mInLiquid = false;
        }
    }
    else
    {
        // If not in liquid, the threshold of going in is at lower y
        p = position + Vector3<float>{0.0f, BS * 0.5f, 0.0f};
        pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        node = map->GetNode(pp, &isValidPosition);
        if (isValidPosition)
        {
            mInLiquid = nodeMgr->Get(node.GetContent()).IsLiquid();
            mLiquidViscosity = nodeMgr->Get(node.GetContent()).liquidViscosity;
        }
        else
        {
            mInLiquid = false;
        }
    }


    /*
        Check if player is in liquid (the stable value)
    */
    p = position + Vector3<float>::Zero();
    pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    node = map->GetNode(pp, &isValidPosition);
    if (isValidPosition)
    {
        mInLiquidStable = nodeMgr->Get(node.GetContent()).IsLiquid();
    }
    else
    {
        mInLiquidStable = false;
    }

    /*
        Check if player is climbing
    */

    p = position + Vector3<float>{0.0f, 0.5f * BS, 0.0f};
    pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    Vector3<float> p2 = position + Vector3<float>{0.0f, -0.2f * BS, 0.0f};
    Vector3<short> pp2;
    pp2[0] = (short)((p2[0] + (p2[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp2[1] = (short)((p2[1] + (p2[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp2[2] = (short)((p2[2] + (p2[2] > 0 ? BS / 2 : -BS / 2)) / BS);

	node = map->GetNode(pp, &isValidPosition);
	bool isValidPosition2;
	MapNode node2 = map->GetNode(pp2, &isValidPosition2);

	if (!(isValidPosition && isValidPosition2)) 
    {
		mIsClimbing = false;
	} 
    else 
    {
		mIsClimbing = (nodeMgr->Get(node.GetContent()).climbable ||
            nodeMgr->Get(node2.GetContent()).climbable) && !freeMove;
	}

	/*
		Collision uncertainty radius
		Make it a bit larger than the maximum distance of movement
	*/
	//float d = posMaxDist * 1.1;
	// A fairly large value in here makes moving smoother
	float d = 0.15f * BS;

	// This should always apply, otherwise there are glitches
	LogAssert(d > posMaxDist, "invalid");

	// Player object property step height is multiplied by BS in
	// /src/script/common/c_content.cpp and /src/content_playerLAO.cpp
	float playerStepHeight = (mVao == nullptr) ? 0.0f :
		(mTouchingGround ? mVao->GetStepHeight() : (0.2f * BS));

	Vector3<float> accel = Vector3<float>::Zero();
	const Vector3<float> initialPosition = position;
	const Vector3<float> initialSpeed = mSpeed;
    CollisionMoveResult result = CollisionMoveSimple(mEnvironment,
        posMaxDist, mCollisionBox, playerStepHeight, dTime, &position, &mSpeed, accel);

	bool couldSneak = mControl.sneak && !freeMove && 
        !mInLiquid && !mIsClimbing && mPhysicsOverrideSneak;

	// Add new collisions to the vector
	if (collisionInfo && !freeMove) 
    {
        Vector3<float> diff = Vector3<float>{ (float)mStandingNode[0], 
            (float)mStandingNode[1], (float)mStandingNode[2] } *BS - position;
		float distance = Length(diff);
		// Force update each VisualEnvironment::step()
		bool isFirst = collisionInfo->empty();

		for (const auto &colInfo : result.collisions) 
        {
			collisionInfo->push_back(colInfo);

			if (colInfo.type != COLLISION_NODE ||
                colInfo.axis != COLLISION_AXIS_Y ||
                (couldSneak && mSneakNodeExists))
				continue;

            diff = Vector3<float>{ (float)colInfo.node[0], 
                (float)colInfo.node[1], (float)colInfo.node[2] } * BS - position;

			// Find nearest colliding node
			float len = Length(diff);
			if (isFirst || len < distance) 
            {
				mStandingNode = colInfo.node;
				distance = len;
				isFirst = false;
			}
		}
	}

	/*
		If the player's feet touch the topside of any node, this is
		set to true.

		Player is allowed to jump when this is true.
	*/
	bool touchingGroundWas = mTouchingGround;
	mTouchingGround = result.touchingGround;
	bool sneakCanJump = false;

	// Max. distance (X, Z) over border for sneaking determined by collision box
	// * 0.49 to keep the center just barely on the node
	Vector3<float> sneakMax = mCollisionBox.GetExtent() * 0.49f;

	if (mSneakLadderDetected) 
    {
		// restore legacy behaviour (this makes the mSpeed.Y hack necessary)
        sneakMax = Vector3<float>{ 0.4f * BS, 0.0f, 0.4f * BS };
	}

	/*
		If sneaking, keep on top of last walked node and don't fall off
	*/
	if (couldSneak && mSneakNodeExists) 
    {
        const Vector3<float> sn = Vector3<float>{ (float)mSneakNode[0],
           (float)mSneakNode[1], (float)mSneakNode[2] } * BS;
		const Vector3<float> bmin = sn + mSneakNodeBBTop.mMinEdge;
		const Vector3<float> bmax = sn + mSneakNodeBBTop.mMaxEdge;
		const Vector3<float> oldPos = position;
		const Vector3<float> oldSpeed = mSpeed;
		float yDiff = bmax[1] - position[1];
		mStandingNode = mSneakNode;

		// (BS * 0.6f) is the basic stepHeight while standing on ground
		if (yDiff < BS * 0.6f) 
        {
			// Only center player when they're on the node
			position[0] = std::clamp(position[0],
				bmin[0] - sneakMax[0], bmax[0] + sneakMax[0]);
			position[2] = std::clamp(position[2],
				bmin[2] - sneakMax[2], bmax[2] + sneakMax[2]);

			if (position[0] != oldPos[0])
				mSpeed[0] = 0.0f;
			if (position[2] != oldPos[2])
				mSpeed[2] = 0.0f;
		}

		if (yDiff > 0 && mSpeed[1] <= 0.0f &&
            (mPhysicsOverrideSneakGlitch || yDiff < BS * 0.6f)) 
        {
			// Move player to the maximal height when falling or when
			// the ledge is climbed on the next step.

			// Smoothen the movement (based on 'position.Y = bmax.Y')
			position[1] += yDiff * dTime * 22.0f + BS * 0.01f;
			position[1] = std::min(position[1], bmax[1]);
			mSpeed[1] = 0.0f;
		}

		// Allow jumping on node edges while sneaking
		if (mSpeed[1] == 0.0f || mSneakLadderDetected)
			sneakCanJump = true;

		if (collisionInfo && mSpeed[1] - oldSpeed[1] > BS) 
        {
			// Collide with sneak node, report fall damage
			CollisionInfo snInfo;
			snInfo.node = mSneakNode;
			snInfo.oldSpeed = oldSpeed;
			snInfo.newSpeed = mSpeed;
			collisionInfo->push_back(snInfo);
		}
	}

	/*
		Find the next sneak node if necessary
	*/
	bool newSneakNodeExists = false;

	if (couldSneak)
		newSneakNodeExists = UpdateSneakNode(map, position, sneakMax);

	/*
		Set new position but keep sneak node set
	*/
	SetPosition(position);
	mSneakNodeExists = newSneakNodeExists;

	/*
		Report collisions
	*/
	if (!result.standingOnObject && !touchingGroundWas && mTouchingGround) 
    {
        EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlayerRegainGround>());

		// Set camera impact value to be used for view bobbing
		mCameraImpact = GetSpeed()[1] * -1;
	}

	{
		mCameraBarelyInCeiling = false;
        Vector3<float> eyePos = GetEyePosition();
        Vector3<short> cameraNodePos;
        cameraNodePos[0] = (short)((eyePos[0] + (eyePos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        cameraNodePos[0] = (short)((eyePos[1] + (eyePos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        cameraNodePos[0] = (short)((eyePos[2] + (eyePos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		MapNode n = map->GetNode(cameraNodePos);
		if (n.GetContent() != CONTENT_IGNORE) 
			if (nodeMgr->Get(n).walkable && nodeMgr->Get(n).solidness == 2)
				mCameraBarelyInCeiling = true;
	}

	/*
		Check properties of the node on which the player is standing
	*/
	const ContentFeatures& f = nodeMgr->Get(map->GetNode(mStandingNode));
    const ContentFeatures& f1 = nodeMgr->Get(map->GetNode(mStandingNode + Vector3<short>{0, 1, 0}));

	// Determine if jumping is possible
	mDisableJump = ItemGroupGet(f.groups, "DisableJump") || ItemGroupGet(f1.groups, "DisableJump");
	mCanJump = ((mTouchingGround && !mIsClimbing) || sneakCanJump) && !mDisableJump;

	// Jump key pressed while jumping off from a bouncy block
	if (mCanJump && mControl.jump && ItemGroupGet(f.groups, "Bouncy") && mSpeed[1] >= -0.5f * BS) 
    {
		float jumpspeed = mMovementSpeedJump * mPhysicsOverrideJump;
		if (mSpeed[1] > 1.0f) 
        {
			// Reduce boost when speed already is high
			mSpeed[1] += jumpspeed / (1.0f + (mSpeed[1] / 16.0f));
		} 
        else 
        {
			mSpeed[1] += jumpspeed;
		}
		SetSpeed(mSpeed);
		mCanJump = false;
	}

	// Autojump
	HandleAutojump(dTime, result, initialPosition, initialSpeed, posMaxDist);
}

void VisualPlayer::Move(float dTime, float posMaxDist)
{
	Move(dTime, posMaxDist, NULL);
}

void VisualPlayer::ApplyControl(float dTime)
{
	// Clear stuff
	mSwimmingVertical = false;
	mSwimmingPitch = false;

	SetPitch(mControl.pitch);
	SetYaw(mControl.yaw);

	// Nullify speed and don't run positioning code if the player is attached
	if (GetParent()) 
    {
		SetSpeed(Vector3<float>::Zero());
		return;
	}

	PlayerSettings& playerSettings = GetPlayerSettings();

	// All vectors are relative to the player's yaw,
	// (and pitch if pitch move mode enabled),
	// and will be rotated at the end
	Vector3<float> speedH, speedV; // Horizontal (X, Z) and Vertical (Y)
    speedH = Vector3<float>::Zero();
    speedV = Vector3<float>::Zero();

	bool flyAllowed = mEnvironment->CheckLocalPrivilege("fly");
	bool fastAllowed = mEnvironment->CheckLocalPrivilege("fast");

	bool freeMove = flyAllowed && playerSettings.freeMove;
	bool fastMove = fastAllowed && playerSettings.fastMove;
	bool pitchMove = (freeMove || mInLiquid) && playerSettings.pitchMove;
	// When aux1Descends is enabled the fast key is used to go down, so fast isn't possible
	bool fastClimb = fastMove && mControl.aux1 && !playerSettings.aux1Descends;
	bool alwaysFlyFast = playerSettings.alwaysFlyFast;

	// Whether superSpeed mode is used or not
	bool superSpeed = false;

	if (alwaysFlyFast && freeMove && fastMove)
		superSpeed = true;

	// Old descend control
	if (playerSettings.aux1Descends) 
    {
		// If free movement and fast movement, always move fast
		if (freeMove && fastMove)
			superSpeed = true;

		// Auxiliary button 1 (E)
		if (mControl.aux1) 
        {
			if (freeMove) 
            {
				// In free movement mode, aux1 descends
				if (fastMove)
					speedV[1] = -mMovementSpeedFast;
				else
					speedV[1] = -mMovementSpeedWalk;
			} 
            else if (mInLiquid || mInLiquidStable) 
            {
				speedV[1] = -mMovementSpeedWalk;
				mSwimmingVertical = true;
			} 
            else if (mIsClimbing) 
            {
				speedV[1] = -mMovementSpeedClimb;
			} 
            else 
            {
				// If not free movement but fast is allowed, aux1 is
				// "Turbo button"
				if (fastMove)
					superSpeed = true;
			}
		}
	} 
    else 
    {
		// New minecraft-like descend control

		// Auxiliary button 1 (E)
		if (mControl.aux1) 
        {
			if (!mIsClimbing) 
            {
				// aux1 is "Turbo button"
				if (fastMove)
					superSpeed = true;
			}
		}

		if (mControl.sneak) 
        {
			if (freeMove) 
            {
				// In free movement mode, sneak descends
				if (fastMove && (mControl.aux1 || alwaysFlyFast))
					speedV[1] = -mMovementSpeedFast;
				else
					speedV[1] = -mMovementSpeedWalk;
			} 
            else if (mInLiquid || mInLiquidStable) 
            {
				if (fastClimb)
					speedV[1] = -mMovementSpeedFast;
				else
					speedV[1] = -mMovementSpeedWalk;
				mSwimmingVertical = true;
			} 
            else if (mIsClimbing) 
            {
				if (fastClimb)
					speedV[1] = -mMovementSpeedFast;
				else
					speedV[1] = -mMovementSpeedClimb;
			}
		}
	}

	if (mControl.up)
        speedH += Vector3<float>{0.0f, 0.0f, 1.0f};

	if (mControl.down)
        speedH -= Vector3<float>{0.0f, 0.0f, 1.0f};

	if (mControl.left)
        speedH += Vector3<float>{1.0f, 0.0f, 0.0f};

	if (mControl.right)
        speedH -= Vector3<float>{1.0f, 0.0f, 0.0f};

	if (mAutojump) 
    {
		// release autojump after a given time
		mAutojumpTime -= dTime;
		if (mAutojumpTime <= 0.0f)
			mAutojump = false;
	}

	if (mControl.jump)
    {
		if (freeMove) 
        {
			if (playerSettings.aux1Descends || alwaysFlyFast) 
            {
				if (fastMove)
					speedV[1] = mMovementSpeedFast;
				else
					speedV[1] = mMovementSpeedWalk;
			} 
            else 
            {
				if (fastMove && mControl.aux1)
					speedV[1] = mMovementSpeedFast;
				else
					speedV[1] = mMovementSpeedWalk;
			}
		} 
        else if (mCanJump) 
        {
			/*
				NOTE: The d value in move() affects jump height by
				raising the height at which the jump speed is kept
				at its starting value
			*/
			Vector3<float> speedJ = GetSpeed();
			if (speedJ[1] >= -0.5f * BS) 
            {
				speedJ[1] = mMovementSpeedJump * mPhysicsOverrideJump;
				SetSpeed(speedJ);

                EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlayerJump>());
			}
		} 
        else if (mInLiquid && !mDisableJump) 
        {
			if (fastClimb)
				speedV[1] = mMovementSpeedFast;
			else
				speedV[1] = mMovementSpeedWalk;
			mSwimmingVertical = true;
		} 
        else if (mIsClimbing && !mDisableJump) 
        {
			if (fastClimb)
				speedV[1] = mMovementSpeedFast;
			else
				speedV[1] = mMovementSpeedClimb;
		}
	}

	// The speed of the player (Y is ignored)
    Normalize(speedH);
	if (superSpeed || (mIsClimbing && fastClimb) || 
        ((mInLiquid || mInLiquidStable) && fastClimb))
		speedH *= mMovementSpeedFast;
	else if (mControl.sneak && !freeMove && !mInLiquid && !mInLiquidStable)
		speedH *= mMovementSpeedCrouch;
	else
		speedH *= mMovementSpeedWalk;

	// Acceleration increase
	float incH = 0.0f; // Horizontal (X, Z)
	float incV = 0.0f; // Vertical (Y)
	if ((!mTouchingGround && !freeMove && !mIsClimbing && !mInLiquid) ||
        (!freeMove && mCanJump && mControl.jump))
    {
		// Jumping and falling
		if (superSpeed || (fastMove && mControl.aux1))
			incH = mMovementAccelerationFast * BS * dTime;
		else
			incH = mMovementAccelerationAir * BS * dTime;
		incV = 0.0f; // No vertical acceleration in air
	} 
    else if (superSpeed || (mIsClimbing && fastClimb) ||
			((mInLiquid || mInLiquidStable) && fastClimb)) 
    {
		incH = incV = mMovementAccelerationFast * BS * dTime;
	} 
    else 
    {
		incH = incV = mMovementAccelerationDefault * BS * dTime;
	}

	float slipFactor = 1.0f;
	if (!freeMove && !mInLiquid && !mInLiquidStable)
		slipFactor = GetSlipFactor(speedH);

	// Don't sink when swimming in pitch mode
	if (pitchMove && mInLiquid) 
    {
		Vector3<float> controlSpeed = speedH + speedV;
		if (Length(controlSpeed) > 0.01f)
			mSwimmingPitch = true;
	}

	// Accelerate to target speed with maximum increment
	Accelerate((speedH + speedV) * mPhysicsOverrideSpeed,
		incH * mPhysicsOverrideSpeed * slipFactor, 
        incV * mPhysicsOverrideSpeed, pitchMove);
}

Vector3<short> VisualPlayer::GetStandingNodePosition()
{
	if (mSneakNodeExists)
		return mSneakNode;

	return mStandingNode;
}

Vector3<short> VisualPlayer::GetFootstepNodePosition()
{
    Vector3<short> pp;

	// Emit swimming sound if the player is in liquid
    if (mInLiquidStable)
    {
        Vector3<float> p = GetPosition();
        pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
        return pp;
    }

	// BS * 0.05 below the player's feet ensures a 1/16th height
	// nodebox is detected instead of the node below it.
    if (mTouchingGround)
    {
        Vector3<float> p = GetPosition() - Vector3<float>{0.0f, BS * 0.05f, 0.0f};
        pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
        return pp;
    }

	// A larger distance below is necessary for a footstep sound
	// when landing after a jump or fall. BS * 0.5 ensures water
	// sounds when swimming in 1 node deep water.
    Vector3<float> p = GetPosition() - Vector3<float>{0.0f, BS * 0.5f, 0.0f};
    pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    return pp;
}

Vector3<short> VisualPlayer::GetLightPosition() const
{
    Vector3<float> p = GetPosition() + Vector3<float>{0.f, BS * 1.5f, 0.f};
    
    Vector3<short> pp;
    pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    return pp;
}

Vector3<float> VisualPlayer::GetEyeOffset() const
{
	float eyeHeight = mCameraBarelyInCeiling ? mEyeHeight - 0.125f : mEyeHeight;
    return Vector3<float>{0.0f, BS * eyeHeight, 0.0f};
}

VisualActiveObject* VisualPlayer::GetParent() const
{
	return mVao ? mVao->GetParent() : nullptr;
}

bool VisualPlayer::IsDead() const
{
	LogAssert(GetVAO(), "VisualPlayer's VAO isn't Initialized");
	return !GetVAO()->IsImmortal() && mHp == 0;
}

// 3D acceleration
void VisualPlayer::Accelerate(const Vector3<float>& targetSpeed, 
    const float maxIncreaseH, const float maxIncreaseV, const bool usePitch)
{
	const float yaw = GetYaw();
	const float pitch = GetPitch();
	Vector3<float> flatSpeed = mSpeed;
	// Rotate speed vector by -yaw and -pitch to make it relative to the player's yaw and pitch
    Quaternion<float> tgt = Rotation<3, float>(
        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -yaw * (float)GE_C_DEG_TO_RAD));
    flatSpeed = HProject(Rotate(tgt, HLift(flatSpeed, 0.f)));
    //flatSpeed.rotateXZBy(-yaw);
    if (usePitch)
    {
        tgt = Rotation<3, float>(
            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), pitch * (float)GE_C_DEG_TO_RAD));
        flatSpeed = HProject(Rotate(tgt, HLift(flatSpeed, 0.f)));
        //flatSpeed.rotateYZBy(-pitch);
    }

	Vector3<float> dWanted = targetSpeed - flatSpeed;
	Vector3<float> d = Vector3<float>::Zero();

	// Then compare the horizontal and vertical components with the wanted speed
	if (maxIncreaseH > 0.0f) 
    {
        Vector3<float> dWantedH = dWanted * Vector3<float>{1.0f, 0.0f, 1.0f};
        if (Length(dWantedH) > maxIncreaseH)
        {
            Normalize(dWantedH);
            d = dWantedH * maxIncreaseH;
        }
		else d += dWantedH;
	}

	if (maxIncreaseV > 0.0f) 
    {
		float dWantedV = dWanted[1];
		if (dWantedV > maxIncreaseV)
			d[1] += maxIncreaseV;
		else if (dWantedV < -maxIncreaseV)
			d[1] -= maxIncreaseV;
		else
			d[1] += dWantedV;
	}

	// Finally rotate it again
    if (usePitch)
    {
        tgt = Rotation<3, float>(
            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -pitch * (float)GE_C_DEG_TO_RAD));
        d = HProject(Rotate(tgt, HLift(d, 0.f)));
        //d.rotateYZBy(pitch);
    }
    tgt = Rotation<3, float>(
        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), yaw * (float)GE_C_DEG_TO_RAD));
    d = HProject(Rotate(tgt, HLift(d, 0.f)));
	//d.rotateXZBy(yaw);

	mSpeed += d;
}

// Temporary option for old move code
void VisualPlayer::OldMove(float dTime, float posMaxDist, std::vector<CollisionInfo>* collisionInfo)
{
    std::shared_ptr<Map> map = mEnvironment->GetMap();
    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();

    Vector3<float> position = GetPosition();

    // Copy parent position if local player is attached
    if (GetParent())
    {
        SetPosition(mVao->GetPosition());
        mSneakNodeExists = false;
        mAddedVelocity = Vector3<float>::Zero();
        return;
    }

    PlayerSettings& playerSettings = GetPlayerSettings();

    // Skip collision detection if noClip mode is used
    bool flyAllowed = mEnvironment->CheckLocalPrivilege("fly");
    bool noClip = mEnvironment->CheckLocalPrivilege("noclip") && playerSettings.noClip;
    bool freeMove = noClip && flyAllowed && playerSettings.freeMove;
    if (freeMove)
    {
        position += mSpeed * dTime;
        SetPosition(position);

        mTouchingGround = false;
        mSneakNodeExists = false;
        mAddedVelocity = Vector3<float>::Zero();
        return;
    }

    mSpeed += mAddedVelocity;
    mAddedVelocity = Vector3<float>::Zero();

    /*
        Collision detection
    */
    bool isValidPosition;
    MapNode node;
    Vector3<float> p;
    Vector3<short> pp;

    /*
        Check if player is in liquid (the oscillating value)
    */
    if (mInLiquid)
    {
        // If in liquid, the threshold of coming out is at higher y
        p = position + Vector3<float>{0.0f, BS * 0.1f, 0.0f};
        pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        node = map->GetNode(pp, &isValidPosition);
        if (isValidPosition)
        {
            mInLiquid = nodeMgr->Get(node.GetContent()).IsLiquid();
            mLiquidViscosity = nodeMgr->Get(node.GetContent()).liquidViscosity;
        }
        else
        {
            mInLiquid = false;
        }
    }
    else
    {
        // If not in liquid, the threshold of going in is at lower y
        p = position + Vector3<float>{0.0f, BS * 0.5f, 0.0f};
        pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        node = map->GetNode(pp, &isValidPosition);
        if (isValidPosition)
        {
            mInLiquid = nodeMgr->Get(node.GetContent()).IsLiquid();
            mLiquidViscosity = nodeMgr->Get(node.GetContent()).liquidViscosity;
        }
        else
        {
            mInLiquid = false;
        }
    }

    /*
        Check if player is in liquid (the stable value)
    */
    p = position + Vector3<float>::Zero();
    pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    node = map->GetNode(pp, &isValidPosition);
    if (isValidPosition)
        mInLiquidStable = nodeMgr->Get(node.GetContent()).IsLiquid();
    else
        mInLiquidStable = false;

    /*
        Check if player is climbing
    */
    p = position + Vector3<float>{0.0f, BS * 0.5f, 0.0f};
    pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    Vector3<float> p2 = position + Vector3<float>{0.0f, -0.2f * BS, 0.0f};
    Vector3<short> pp2;
    pp2[0] = (short)((p2[0] + (p2[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp2[1] = (short)((p2[1] + (p2[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pp2[2] = (short)((p2[2] + (p2[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    node = map->GetNode(pp, &isValidPosition);
    bool isValidPosition2;
    MapNode node2 = map->GetNode(pp2, &isValidPosition2);

    if (!(isValidPosition && isValidPosition2))
        mIsClimbing = false;
    else
        mIsClimbing = (nodeMgr->Get(node.GetContent()).climbable ||
            nodeMgr->Get(node2.GetContent()).climbable) && !freeMove;

    /*
        Collision uncertainty radius
        Make it a bit larger than the maximum distance of movement
    */
    //float d = posMaxDist * 1.1;
    // A fairly large value in here makes moving smoother
    float d = 0.15f * BS;
    // This should always apply, otherwise there are glitches
    LogAssert(d > posMaxDist, "invalid");
    // Maximum distance over border for sneaking
    float sneakMax = BS * 0.4f;

    /*
        If sneaking, keep in range from the last walked node and don't
        fall off from it
    */
    if (mControl.sneak && mSneakNodeExists &&
        !(flyAllowed && playerSettings.freeMove) && !mInLiquid && mPhysicsOverrideSneak)
    {
        float maxd = 0.5f * BS + sneakMax;
        Vector3<float> lwn = Vector3<float>{ (float)mSneakNode[0], 
            (float)mSneakNode[1], (float)mSneakNode[2] } * BS;
        position[0] = std::clamp(position[0], lwn[0] - maxd, lwn[0] + maxd);
        position[2] = std::clamp(position[2], lwn[2] - maxd, lwn[2] + maxd);

        if (!mIsClimbing)
        {
            // Move up if necessary
            float newY = (lwn[1] - 0.5f * BS) + mSneakNodeBBYmax;
            if (position[1] < newY) position[1] = newY;
            /*
                Collision seems broken, since player is sinking when
                sneaking over the edges of current sneaking_node.
                TODO (when fixed): Set Y-speed only to 0 when position.Y < newY.
            */
            if (mSpeed[1] < 0.0f) mSpeed[1] = 0.0f;
        }
    }

    // TODO: This shouldn't be hardcoded but decided by the logic
    float playerStepHeight = mTouchingGround ? (BS * 0.6f) : (BS * 0.2f);

    Vector3<float> accel;
    const Vector3<float> initialPosition = position;
    const Vector3<float> initialSpeed = mSpeed;

    CollisionMoveResult result = CollisionMoveSimple(mEnvironment, 
        posMaxDist, mCollisionBox, playerStepHeight, dTime, &position, &mSpeed, accel);

    // Positition was slightly changed; update standing node pos
    if (mTouchingGround)
    {
        p = mPosition - Vector3<float>{0.0f, 0.1f * BS, 0.0f};
        mStandingNode[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        mStandingNode[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        mStandingNode[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    }
    else
    {
        mStandingNode[0] = (short)((mPosition[0] + (mPosition[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        mStandingNode[1] = (short)((mPosition[1] + (mPosition[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        mStandingNode[2] = (short)((mPosition[2] + (mPosition[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    }

    /*
        If the player's feet touch the topside of any node, this is
        set to true.

        Player is allowed to jump when this is true.
    */
    bool touchingGroundWas = mTouchingGround;
    mTouchingGround = result.touchingGround;

    //bool standing_on_unloaded = result.standing_on_unloaded;

    /*
        Check the nodes under the player to see from which node the
        player is sneaking from, if any. If the node from under
        the player has been removed, the player falls.
    */
    float positionYMod = 0.05f * BS;
    if (mSneakNodeBBYmax > 0.0f)
        positionYMod = mSneakNodeBBYmax - positionYMod;

    p = position - Vector3<float>{0.0f, positionYMod, 0.0f};
    Vector3<short> currentNode;
    currentNode[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    currentNode[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    currentNode[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    if (mSneakNodeExists &&
        nodeMgr->Get(map->GetNode(mOldNodeBelow)).name == "air" && mOldNodeBelowType != "air")
    {
        // Old node appears to have been removed; that is,
        // it wasn't air before but now it is
        mNeedToGetNewSneakNode = false;
        mSneakNodeExists = false;
    }
    else if (nodeMgr->Get(map->GetNode(currentNode)).name != "air")
    {
        // We are on something, so make sure to recalculate the sneak
        // node.
        mNeedToGetNewSneakNode = true;
    }

    if (mNeedToGetNewSneakNode && mPhysicsOverrideSneak)
    {
        mSneakNodeBBYmax = 0.0f;
        p = position - Vector3<float>{0.0f, positionYMod, 0.0f};
        Vector3<short> posYBottom;
        posYBottom[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        posYBottom[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        posYBottom[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        Vector2<float> playerPos2D = { position[0], position[2] };
        float minDistance = 100000.0f * BS;
        // If already seeking from some node, compare to it.
        Vector3<short> newSneakNode = mSneakNode;
        for (short x = -1; x <= 1; x++)
        {
            for (short z = -1; z <= 1; z++)
            {
                Vector3<short> p = posYBottom + Vector3<short>{x, 0, z};
                Vector2<float> node2D{ p[0] * BS, p[2] * BS };
                float distance = Length(playerPos2D - node2D);
                float maxAxisDistance = std::max(
                    std::fabs(playerPos2D[0] - node2D[0]),
                    std::fabs(playerPos2D[1] - node2D[1]));

                if (distance > minDistance ||
                    maxAxisDistance > 0.5f * BS + sneakMax + 0.1f * BS)
                    continue;

                // The node to be sneaked on has to be walkable
                node = map->GetNode(p, &isValidPosition);
                if (!isValidPosition || !nodeMgr->Get(node).walkable)
                    continue;

                // And the node above it has to be nonwalkable
                node = map->GetNode(p + Vector3<short>{0, 1, 0}, &isValidPosition);
                if (!isValidPosition || nodeMgr->Get(node).walkable)
                    continue;

                // If not 'sneakGlitch' the node 2 nodes above it has to be nonwalkable
                if (!mPhysicsOverrideSneakGlitch)
                {
                    node = map->GetNode(p + Vector3<short>{0, 2, 0}, &isValidPosition);
                    if (!isValidPosition || nodeMgr->Get(node).walkable)
                        continue;
                }

                minDistance = distance;
                newSneakNode = p;
            }
        }

        bool sneakNodeFound = (minDistance < 100000.0f * BS * 0.9f);

        mSneakNode = newSneakNode;
        mSneakNodeExists = sneakNodeFound;

        if (sneakNodeFound)
        {
            float cbMax = 0.0f;
            MapNode n = map->GetNode(mSneakNode);
            std::vector<BoundingBox<float>> nodeboxes;
            n.GetCollisionBoxes(nodeMgr, &nodeboxes);
            for (const auto &box : nodeboxes)
                if (box.mMaxEdge[1] > cbMax)
                    cbMax = box.mMaxEdge[1];

            mSneakNodeBBYmax = cbMax;
        }

        /*
            If sneaking, the player's collision box can be in air, so
            this has to be set explicitly
        */
        if (sneakNodeFound && mControl.sneak)
            mTouchingGround = true;
    }

    /*
        Set new position but keep sneak node set
    */
    bool sneakNodeExists = mSneakNodeExists;
    SetPosition(position);
    mSneakNodeExists = sneakNodeExists;

    /*
        Report collisions
    */
    // Don't report if flying
    if (collisionInfo && !(playerSettings.freeMove && flyAllowed))
        for (const auto &info : result.collisions)
            collisionInfo->push_back(info);

    if (!result.standingOnObject && !touchingGroundWas && mTouchingGround)
    {
        EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlayerRegainGround>());

        // Set camera impact value to be used for view bobbing
        mCameraImpact = GetSpeed()[1] * -1.0f;
    }

    {
        mCameraBarelyInCeiling = false;
        p = GetEyePosition();
        Vector3<short> cameraNodePos;
        cameraNodePos[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        cameraNodePos[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        cameraNodePos[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        MapNode n = map->GetNode(cameraNodePos);
        if (n.GetContent() != CONTENT_IGNORE)
            if (nodeMgr->Get(n).walkable && nodeMgr->Get(n).solidness == 2)
                mCameraBarelyInCeiling = true;
    }

    /*
        Update the node last under the player
    */
    p = position - Vector3<float>{0.0f, BS / 2.0f, 0.0f};
    mOldNodeBelow[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    mOldNodeBelow[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    mOldNodeBelow[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

	mOldNodeBelowType = nodeMgr->Get(map->GetNode(mOldNodeBelow)).name;

	/*
		Check properties of the node on which the player is standing
	*/
	const ContentFeatures& cFeatures = nodeMgr->Get(map->GetNode(GetStandingNodePosition()));

	// Determine if jumping is possible
	mDisableJump = ItemGroupGet(cFeatures.groups, "DisableJump");
	mCanJump = mTouchingGround && !mDisableJump;

	// Jump key pressed while jumping off from a bouncy block
	if (mCanJump && mControl.jump && ItemGroupGet(cFeatures.groups, "Bouncy") && mSpeed[1] >= -0.5f * BS)
    {
		float jumpspeed = mMovementSpeedJump * mPhysicsOverrideJump;
		if (mSpeed[1] > 1.0f) 
        {
			// Reduce boost when speed already is high
			mSpeed[1] += jumpspeed / (1.0f + (mSpeed[1] / 16.0f));
		} 
        else 
        {
			mSpeed[1] += jumpspeed;
		}
		SetSpeed(mSpeed);
		mCanJump = false;
	}

	// Autojump
	HandleAutojump(dTime, result, initialPosition, initialSpeed, posMaxDist);
}

float VisualPlayer::GetSlipFactor(const Vector3<float>& speedH)
{
	// Slip on slippery nodes
    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();
	std::shared_ptr<Map> map = mEnvironment->GetMap();
	const ContentFeatures& cFeatures = nodeMgr->Get(map->GetNode(GetStandingNodePosition()));
	int slippery = 0;
	if (cFeatures.walkable)
		slippery = ItemGroupGet(cFeatures.groups, "Slippery");

	if (slippery >= 1) 
    {
		if (speedH == Vector3<float>::Zero())
			slippery *= 2;

		return std::clamp(1.0f / (slippery + 1), 0.001f, 1.0f);
	}
	return 1.0f;
}

void VisualPlayer::HandleAutojump(float dTime, const CollisionMoveResult& result, 
    const Vector3<float>& initialPosition, const Vector3<float>& initialSpeed, float posMaxDist)
{
	PlayerSettings& playerSettings = GetPlayerSettings();
	if (!playerSettings.autojump)
		return;

	if (mAutojump)
		return;

	bool controlForward = mControl.up;

	bool couldAutojump =
		mCanJump && !mControl.jump && !mControl.sneak && controlForward;

	if (!couldAutojump)
		return;

	bool horizontalCollision = false;
	for (const auto& colInfo : result.collisions) 
    {
		if (colInfo.type == COLLISION_NODE && colInfo.plane != 1) 
        {
			horizontalCollision = true;
			break; // one is enough
		}
	}

	// must be running against something to trigger autojumping
	if (!horizontalCollision)
		return;

	// check for nodes above
	Vector3<float> headPosMin = mPosition + mCollisionBox.mMinEdge * 0.99f;
	Vector3<float> headPosMax = mPosition + mCollisionBox.mMaxEdge * 0.99f;
	headPosMin[1] = headPosMax[1]; // top face of collision box
    Vector3<short> ceilPosMin;
    ceilPosMin[0] = (short)((headPosMin[0] + (headPosMin[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    ceilPosMin[1] = (short)((headPosMin[1] + (headPosMin[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    ceilPosMin[2] = (short)((headPosMin[2] + (headPosMin[2] > 0 ? BS / 2 : -BS / 2)) / BS);
    ceilPosMin += Vector3<short>{0, 1, 0};
        
    Vector3<short> ceilPosMax;
    ceilPosMax[0] = (short)((headPosMax[0] + (headPosMax[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    ceilPosMax[1] = (short)((headPosMax[1] + (headPosMax[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    ceilPosMax[2] = (short)((headPosMax[2] + (headPosMax[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();

	bool isPositionValid;
	for (short z = ceilPosMin[2]; z <= ceilPosMax[2]; ++z) 
    {
		for (short x = ceilPosMin[0]; x <= ceilPosMax[0]; ++x) 
        {
            MapNode node = mEnvironment->GetMap()->GetNode(Vector3<short>{x, ceilPosMax[1], z}, &isPositionValid);

			if (!isPositionValid)
				break;  // won't collide with the void outside
			if (node.GetContent() == CONTENT_IGNORE)
				return; // players collide with ignore blocks -> same as walkable
			const ContentFeatures& cFeatures = nodeMgr->Get(node);
			if (cFeatures.walkable)
				return; // would bump head, don't jump
		}
	}

	float jumpHeight = 1.1f; // TODO: better than a magic number
    Vector3<float> jumpPos = initialPosition + Vector3<float>{0.0f, jumpHeight * BS, 0.0f};
	Vector3<float> jumpSpeed = initialSpeed;

	// try at peak of jump, zero step height
	CollisionMoveResult jumpResult = CollisionMoveSimple(mEnvironment, posMaxDist, 
        mCollisionBox, 0.0f, dTime, &jumpPos, &jumpSpeed, Vector3<float>::Zero());

	// see if we can get a little bit farther horizontally if we had
	// jumped
	Vector3<float> runDelta = mPosition - initialPosition;
	runDelta[1] = 0.0f;
	Vector3<float> jumpDelta = jumpPos - initialPosition;
	jumpDelta[1] = 0.0f;
	if (LengthSq(jumpDelta) > LengthSq(runDelta) * 1.01f)
    {
		mAutojump = true;
		mAutojumpTime = 0.1f;
	}
}
