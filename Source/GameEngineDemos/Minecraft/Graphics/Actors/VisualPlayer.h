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

#ifndef VISUALPLAYER_H
#define VISUALPLAYER_H

#include "../../Games/Actors/Player.h"

#include "../../Physics/Collision.h"

#include "Core/Logger/Logger.h"

#include "Mathematic/Algebra/Vector3.h"

#include "Graphic/Scene/Hierarchy/BoundingBox.h"

class VisualEnvironment;
class VisualActiveObject;
class GenericVisualActiveObject;

enum VisualPlayerAnimations
{
	NO_ANIM,
	WALK_ANIM,
	DIG_ANIM,
	WD_ANIM
}; // no local animation, walking, digging, both

class VisualPlayer : public Player
{
public:
	VisualPlayer(ActorId aId, const char* name, VisualEnvironment* vEnv);
	virtual ~VisualPlayer() = default;

	void Move(float dTime, float posMaxDist);
	void Move(float dTime, float posMaxDist,
			std::vector<CollisionInfo>* collisionInfo);
	// Temporary option for old move code
	void OldMove(float dTime, float posMaxDist,
			std::vector<CollisionInfo>* collisionInfo);

	void ApplyControl(float dTime);

	Vector3<short> GetStandingNodePosition();
	Vector3<short> GetFootstepNodePosition();

	GenericVisualActiveObject* GetVAO() const { return mVao; }

	VisualActiveObject* GetParent() const;

	void SetVAO(GenericVisualActiveObject* toSet)
	{
		LogAssert(toSet, "invalid vao"); // Pre-condition
		mVao = toSet;
	}

	unsigned int MaxHudId() const { return (unsigned int)mHud.size(); }

	unsigned short GetBreath() const { return mBreath; }
	void SetBreath(unsigned short breath) { mBreath = breath; }

	Vector3<short> GetLightPosition() const;

	void SetYaw(float yaw) { mYaw = yaw; }
	float GetYaw() const { return mYaw; }

	void SetPitch(float pitch) { mPitch = pitch; }
	float GetPitch() const { return mPitch; }

	inline void SetPosition(const Vector3<float>& position)
	{
		if (mPosition != position)
			mPosition = position;

		mSneakNodeExists = false;
	}

	Vector3<float> GetPosition() const { return mPosition; }

	// Non-transformed eye offset getters
	// For accurate positions, use the PlayerCamera functions
	Vector3<float> GetEyePosition() const { return mPosition + GetEyeOffset(); }
	Vector3<float> GetEyeOffset() const;
	void SetEyeHeight(float eyeHeight) { mEyeHeight = eyeHeight; }

	BoundingBox<float>& GetCollisionBox() { return mCollisionBox; }

	float GetZoomFOV() const { return mZoomFov; }
	void SetZoomFOV(float zoomFov) { mZoomFov = zoomFov; }

	bool GetAutoJump() const { return mAutojump; }

	bool IsDead() const;

	inline void AddVelocity(const Vector3<float>& vel)
	{
		mAddedVelocity += vel;
	}

    // Initialize hp to 0, so that no hearts will be shown if logic
    // doesn't support health points
    unsigned short mHp = 0;
    bool mTouchingGround = false;
    // This oscillates so that the player jumps a bit above the surface
    bool mInLiquid = false;
    // This is more stable and defines the maximum speed of the player
    bool mInLiquidStable = false;
    // Gets the viscosity of water to calculate friction
    unsigned char mLiquidViscosity = 0;
    bool mIsClimbing = false;
    bool mSwimmingVertical = false;
    bool mSwimmingPitch = false;

    float mPhysicsOverrideSpeed = 1.0f;
    float mPhysicsOverrideJump = 1.0f;
    float mPhysicsOverrideGravity = 1.0f;
    bool mPhysicsOverrideSneak = true;
    bool mPhysicsOverrideSneakGlitch = false;
    // Temporary option for old move code
    bool mPhysicsOverrideNewMove = true;

    // Used to check if anything changed and prevent sending packets if not
    Vector3<float> mLastPosition;
    Vector3<float> mLastSpeed;
    float mLastPitch = 0.0f;
    float mLastYaw = 0.0f;
    unsigned int mLastKeyPressed = 0;
	float mLastPlayerCameraFov = 0;
	float mLastWantedRange = 0;

    float mCameraImpact = 0.0f;

    bool mMakesFootstepSound = true;

    int mLastAnimation = NO_ANIM;
    float mLastAnimationSpeed = 0.0f;

	Vector2<short> mLocalAnimations[4];
	float mLocalAnimationSpeed = 0.0f;

    std::string mHotbarImage = "";
    std::string mHotbarSelectedImage = "";

    SColor mLightColor = SColor(255, 255, 255, 255);

    float mHurtTiltTimer = 0.0f;
    float mHurtTiltStrength = 0.0f;

private:
	void Accelerate(const Vector3<float>& targetSpeed, 
        const float maxIncreaseH, const float maxIncreaseV, const bool usePitch);
	bool UpdateSneakNode(std::shared_ptr<Map> map, 
        const Vector3<float>& position, const Vector3<float>& sneakMax);
	float GetSlipFactor(const Vector3<float>& speedH);
	void HandleAutojump(float dTime, const CollisionMoveResult& result,
		const Vector3<float>& positionBeforeMove, const Vector3<float>& speedBeforeMove, float posMaxDist);

    GenericVisualActiveObject* mVao = nullptr;

    VisualEnvironment* mEnvironment = nullptr;

	Vector3<float> mPosition;
	Vector3<short> mStandingNode;

    Vector3<short> mSneakNode = Vector3<short>{ 32767, 32767, 32767 };
	// Stores the top bounding box of mSneakNode
	BoundingBox<float> mSneakNodeBBTop = BoundingBox<float>(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	// Whether the player is allowed to sneak
	bool mSneakNodeExists = false;
	// Whether a "sneak ladder" structure is detected at the players pos
	// see detectSneakLadder() in the .cpp for more info (always false if disabled)
	bool mSneakLadderDetected = false;

	// ***** Variables for temporary option of the old move code *****
	// Stores the max player uplift by mSneakNode
	float mSneakNodeBBYmax = 0.0f;
	// Whether recalculation of mSneakNode and its top bbox is needed
	bool mNeedToGetNewSneakNode = true;
	// Node below player, used to determine whether it has been removed,
	// and its old type
    Vector3<short> mOldNodeBelow = Vector3<short>{ 32767, 32767, 32767 };
	std::string mOldNodeBelowType = "air";
	// ***** End of variables for temporary option *****

	bool mCanJump = false;
	bool mDisableJump = false;
	unsigned short mBreath = PLAYER_MAX_BREATH_DEFAULT;
	float mYaw = 0.0f;
	float mPitch = 0.0f;
	bool mCameraBarelyInCeiling = false;
	BoundingBox<float> mCollisionBox = 
        BoundingBox<float>(-BS * 0.30f, 0.0f, -BS * 0.30f, BS * 0.30f, BS * 1.75f, BS * 0.30f);
	float mEyeHeight = 1.625f;
	float mZoomFov = 0.0f;
	bool mAutojump = false;
	float mAutojumpTime = 0.0f;

    Vector3<float> mAddedVelocity = Vector3<float>::Zero(); // cleared on each move()
	// TODO: Rename to adhere to convention: mAddedVelocity
};

#endif