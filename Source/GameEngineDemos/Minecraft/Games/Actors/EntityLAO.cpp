/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2020 Minetest core developers & community

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

#include "EntityLAO.h"

#include "PlayerLAO.h"

#include "../Environment/LogicEnvironment.h"

#include "../../Games/Games.h"

#include "../../Physics/Collision.h"

EntityLAO::EntityLAO(LogicEnvironment* env, Vector3<float> pos, const std::string& data)
	: UnitLAO(env, pos)
{
	std::string name;
    std::string state;
	uint16_t hp = 1;
	Vector3<float> velocity = Vector3<float>::Zero();
	Vector3<float> rotation = Vector3<float>::Zero();

	while (!data.empty()) 
    { 
        // breakable, run for one iteration
		std::istringstream is(data, std::ios::binary);
		// 'version' does not allow to incrementally extend the parameter list thus
		// we need another variable to build on top of 'version=1'. Ugly hack but works™
		uint8_t version2 = 0;
        uint8_t version = ReadUInt8(is);

		name = DeserializeString16(is);
		state = DeserializeString32(is);

		if (version < 1)
			break;

		hp = ReadUInt16(is);

        velocity[0] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
        velocity[1] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
        velocity[2] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
        
		// yaw must be yaw to be backwards-compatible
		rotation[1] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;

		if (is.good()) // EOF for old formats
			version2 = ReadUInt8(is);

		if (version2 < 1) // PROTOCOL_VERSION < 37
			break;

		// version2 >= 1
		rotation[0] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
		rotation[2] = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;

		// if (version2 < 2)
		//     break;
		// <read new values>
		break;
	}
	// create object
	LogInformation("EntityLAO::create(name=\"" + name + "\" state=\"" + state + "\")");

	mInitName = name;
	mInitState = state;
	mHP = hp;
	mVelocity = velocity;
	mRotation = rotation;
	mAcceleration = Vector3<float>::Zero();
	mLastSentPosition = Vector3<float>::Zero();
	mLastSentVelocity = Vector3<float>::Zero();
	mLastSentRotation = Vector3<float>::Zero();
}

EntityLAO::~EntityLAO()
{
	if(mRegistered)
        BaseGame::Get()->RemoveEntity(this);

	for (unsigned int attachedParticleSpawner : mAttachedParticleSpawners)
		mEnvironment->DeleteParticleSpawner(attachedParticleSpawner, false);
}

void EntityLAO::AddedToEnvironment(unsigned int dTime)
{
	LogicActiveObject::AddedToEnvironment(dTime);

	// Create entity from name
	mRegistered = BaseGame::Get()->AddEntity(this, mInitName.c_str());

	if(mRegistered)
    {
		// Get properties
        BaseGame::Get()->GetPropertiesEntity(this, &mProp);
		// Initialize HP from properties
		mHP = mProp.hpMax;
		// Activate entity, supplying serialized state
        BaseGame::Get()->OnActivateEntity(this, mInitState, dTime);
	} 
    else 
    {
		mProp.infoText = mInitName;
	}
}

void EntityLAO::DispatchScriptDeactivate()
{
	// Ensure that this is in fact a registered entity,
	// and that it isn't already gone.
	// The latter also prevents this from ever being called twice.
	if (mRegistered && !IsGone())
        BaseGame::Get()->OnDeactivateEntity(this);
}

void EntityLAO::Step(float dTime, bool sendRecommended)
{
	if(!mPropertiesSent)
	{
		mPropertiesSent = true;
		std::string str = GetPropertyPacket();
		// create message and add to list
		mMessagesOut.emplace((short)GetId(), true, str);
	}

	// If attached, check that our parent is still there. If it isn't, detach.
	if (mAttachmentParentId && !IsAttached()) 
    {
		// This is handled when objects are removed from the map
		LogWarning("EntityLAO::step() id=" + std::to_string(mId) +
			" is attached to nonexistent parent. This is a bug.");
		ClearParentAttachment();
		SendPosition(false, true);
	}

	mLastSentPositionTimer += dTime;

	CollisionMoveResult moveResult, *moveResultPointer = nullptr;

	// Each frame, parent position is copied if the object is attached, otherwise it's calculated normally
	// If the object gets detached this comes into effect automatically from the last known origin
	if (auto* parent = GetParent()) 
    {
		mBasePosition = parent->GetBasePosition();
		mVelocity = Vector3<float>::Zero();
		mAcceleration = Vector3<float>::Zero();
	} 
    else 
    {
		if(mProp.physical)
        {
			BoundingBox<float> box = mProp.collisionBox;
			box.mMinEdge *= BS;
			box.mMaxEdge *= BS;
			float posMaxDist = BS*0.25; // Distance per iteration
			Vector3<float> pos = mBasePosition;
			Vector3<float> velocity = mVelocity;
			Vector3<float> acceleration = mAcceleration;
			moveResult = CollisionMoveSimple(mEnvironment, posMaxDist, box, mProp.stepHeight, 
				dTime, &pos, &velocity, acceleration, this, mProp.collideWithObjects);
            moveResultPointer = &moveResult;

			// Apply results
			mBasePosition = pos;
			mVelocity = velocity;
			mAcceleration = acceleration;
		} 
        else 
        {
			mBasePosition += dTime * mVelocity + 0.5f * dTime * dTime * mAcceleration;
			mVelocity += dTime * mAcceleration;
		}

		if (mProp.automaticFaceMovementDir &&
            (fabs(mVelocity[2]) > 0.001 || fabs(mVelocity[0]) > 0.001)) 
        {
			float targetYaw = atan2(mVelocity[2], mVelocity[0]) * 180.f / (float)GE_C_PI
				+ mProp.automaticFaceMovementDirOffset;
			float maxRotationPerSec = mProp.automaticFaceMovementMaxRotationPerSec;

			if (maxRotationPerSec > 0) 
            {
				mRotation[1] = WrapDegrees360(mRotation[1]);
                WrappedApproachShortest(mRotation[1], targetYaw, dTime * maxRotationPerSec, 360.f);
			} 
            else 
            {
				// Negative values of max_rotation_per_sec mean disabled.
				mRotation[1] = targetYaw;
			}
		}
	}

	if(mRegistered)
        BaseGame::Get()->OnStepEntity(this, dTime, moveResultPointer);

	if (!sendRecommended)
		return;

	if(!IsAttached())
	{
		// TODO: force send when acceleration changes enough?
		float minchange = 0.2f * BS;
		if(mLastSentPositionTimer > 1.0)
			minchange = 0.01f * BS;
        else if(mLastSentPositionTimer > 0.2)
			minchange = 0.05f * BS;

		float moveDiff = Length(mBasePosition - mLastSentPosition);
        moveDiff += mLastSentMovePrecision;
		float velDiff = Length(mVelocity - mLastSentVelocity);
		if (moveDiff > minchange || velDiff > minchange ||
            std::fabs(mRotation[0] - mLastSentRotation[0]) > 1.0f ||
            std::fabs(mRotation[1] - mLastSentRotation[1]) > 1.0f ||
            std::fabs(mRotation[2] - mLastSentRotation[2]) > 1.0f) 
        {
			SendPosition(true, false);
		}
	}

	SendOutdatedData();
}

std::string EntityLAO::GetVisualInitializationData()
{
	std::ostringstream os(std::ios::binary);

	WriteUInt8(os, 1); // version
	os << SerializeString16(""); // name
	WriteUInt8(os, 0); // is_player
	WriteUInt16(os, GetId()); //id
	WriteV3Float(os, mBasePosition);
	WriteV3Float(os, mRotation);
	WriteUInt16(os, mHP);

	std::ostringstream msgOs(std::ios::binary);
	msgOs << SerializeString32(GetPropertyPacket()); // message 1
	msgOs << SerializeString32(GenerateUpdateArmorGroupsCommand()); // 2
	msgOs << SerializeString32(GenerateUpdateAnimationCommand()); // 3
	for (const auto& bonePos : mBonePosition) 
    {
		msgOs << SerializeString32(GenerateUpdateBonePositionCommand(
			bonePos.first, bonePos.second[0], bonePos.second[1])); // 3 + N
	}
	msgOs << SerializeString32(GenerateUpdateAttachmentCommand()); // 4 + mBonePosition.size

	unsigned int messageCount = 4 + (unsigned int)mBonePosition.size();

	for (const auto& id : GetAttachmentChildIds()) 
    {
		if (LogicActiveObject *obj = mEnvironment->GetActiveObject(id)) 
        {
			messageCount++;
			msgOs << SerializeString32(obj->GenerateUpdateInfantCommand(id));
		}
	}

	msgOs << SerializeString32(GenerateSetTextureModCommand());
	messageCount++;

	WriteUInt8(os, messageCount);
	std::string serialized = msgOs.str();
	os.write(serialized.c_str(), serialized.size());

	// return result
	return os.str();
}

void EntityLAO::GetStaticData(std::string* result) const
{
	std::ostringstream os(std::ios::binary);
	// version must be 1 to keep backwards-compatibility. See version2
	WriteUInt8(os, 1);
	// name
	os<<SerializeString16(mInitName);
	// state
	if(mRegistered)
    {
		std::string state = BaseGame::Get()->GetStaticDataEntity(this);
		os<<SerializeString32(state);
	} 
    else 
    {
		os<<SerializeString32(mInitState);
	}
	WriteUInt16(os, mHP);

    LogAssert(mVelocity[0] >= FLOAT_MIN && mVelocity[0] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mVelocity[0] * FIXEDPOINT_FACTOR));
    LogAssert(mVelocity[1] >= FLOAT_MIN && mVelocity[1] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mVelocity[1] * FIXEDPOINT_FACTOR));
    LogAssert(mVelocity[2] >= FLOAT_MIN && mVelocity[2] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mVelocity[2] * FIXEDPOINT_FACTOR));

	// yaw
    LogAssert(mRotation[1] >= FLOAT_MIN && mRotation[1] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mRotation[1] * FIXEDPOINT_FACTOR));

	// version2. Increase this variable for new values
	WriteUInt8(os, 1); // PROTOCOL_VERSION >= 37

    LogAssert(mRotation[0] >= FLOAT_MIN && mRotation[0] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mRotation[0] * FIXEDPOINT_FACTOR));
    LogAssert(mRotation[2] >= FLOAT_MIN && mRotation[2] <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mRotation[2] * FIXEDPOINT_FACTOR));

	// <write new values>

	*result = os.str();
}

uint16_t EntityLAO::Punch(
    Vector3<float> dir, const ToolCapabilities* toolcap, 
    LogicActiveObject* puncher, float timeFromLastPunch)
{
	if (!mRegistered) 
    {
		// Delete unknown Entities when punched
		MarkForRemoval();
		return 0;
	}

	LogAssert(puncher, "Punch action called without LAO");

	int oldHp = GetHP();
	ItemStack selectedItem, handItem;
	ItemStack toolItem = puncher->GetWieldedItem(&selectedItem, &handItem);

	PunchDamageResult result = GetPunchDamage(
        mArmorGroups, toolcap, &toolItem, timeFromLastPunch);

	bool damageHandled = BaseGame::Get()->OnPunchEntity(this, puncher,
        timeFromLastPunch, toolcap, dir, result.didPunch ? result.damage : 0);

	if (!damageHandled) 
    {
		if (result.didPunch) 
        {
			SetHP((int)GetHP() - result.damage,
				PlayerHPChangeReason(PlayerHPChangeReason::PLAYER_PUNCH, puncher));

			// create message and add to list
			SendPunchCommand();
		}
	}

	if (GetHP() == 0 && !IsGone()) 
    {
		ClearParentAttachment();
		ClearChildAttachments();
        BaseGame::Get()->OnDeathEntity(this, puncher);
		MarkForRemoval();
	}

	LogInformation(puncher->GetDescription() + " (id=" + std::to_string(puncher->GetId()) +
        ", hp=" + std::to_string(puncher->GetHP()) + ") punched " + GetDescription() + 
        " (id=" + std::to_string(mId) + ", hp=" + std::to_string(mHP) + "), damage=" + 
        std::to_string(oldHp - (int)GetHP()) + (damageHandled ? " (handled by )" : ""));

	// TODO: give  control over wear
	return result.wear;
}

void EntityLAO::RightClick(LogicActiveObject *clicker)
{
	if (!mRegistered)
		return;

    BaseGame::Get()->OnRightClickEntity(this, clicker);
}

void EntityLAO::SetPosition(const Vector3<float> &pos)
{
	if(IsAttached())
		return;
	mBasePosition = pos;
	SendPosition(false, true);
}

void EntityLAO::MoveTo(Vector3<float> pos, bool continuous)
{
	if(IsAttached())
		return;
	mBasePosition = pos;
	if(!continuous)
		SendPosition(true, true);
}

float EntityLAO::GetMinimumSavedMovement()
{
	return 0.1 * BS;
}

std::string EntityLAO::GetDescription()
{
	std::ostringstream oss;
	oss << "EntityLAO \"" << mInitName << "\" ";
    Vector3<short> pos;
    pos[0] = (short)((mBasePosition[0] + (mBasePosition[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    pos[1] = (short)((mBasePosition[1] + (mBasePosition[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    pos[2] = (short)((mBasePosition[2] + (mBasePosition[2] > 0 ? BS / 2 : -BS / 2)) / BS);
	oss << "at (" << pos[0] << "," << pos[1] << "," << pos[2] << ")";
	return oss.str();
}

void EntityLAO::SetHP(int hp, const PlayerHPChangeReason &reason)
{
	mHP = std::clamp(hp, 0, 0xFFFF);
}

uint16_t EntityLAO::GetHP() const
{
	return mHP;
}

void EntityLAO::SetVelocity(Vector3<float> velocity)
{
	mVelocity = velocity;
}

Vector3<float> EntityLAO::GetVelocity()
{
	return mVelocity;
}

void EntityLAO::SetAcceleration(Vector3<float> acceleration)
{
	mAcceleration = acceleration;
}

Vector3<float> EntityLAO::GetAcceleration()
{
	return mAcceleration;
}

void EntityLAO::SetTextureMod(const std::string& mod)
{
	mCurrentTextureModifier = mod;
	// create message and add to list
	mMessagesOut.emplace((short)GetId(), true, GenerateSetTextureModCommand());
}

std::string EntityLAO::GetTextureMod() const
{
	return mCurrentTextureModifier;
}


std::string EntityLAO::GenerateSetTextureModCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_SET_TEXTURE_MOD);
	// parameters
	os << SerializeString16(mCurrentTextureModifier);
	return os.str();
}

std::string EntityLAO::GenerateSetSpriteCommand(Vector2<short> p, 
    uint16_t numFrames, float frameLength, bool selectHorizByYawpitch)
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_SET_SPRITE);
	// parameters
	WriteV2Short(os, p);
	WriteUInt16(os, numFrames);
	WriteFloat(os, frameLength);
	WriteUInt8(os, selectHorizByYawpitch);
	return os.str();
}

void EntityLAO::SetSprite(Vector2<short> p, 
    int numFrames, float frameLength, bool selectHorizByYawpitch)
{
	std::string str = GenerateSetSpriteCommand(
		p, numFrames, frameLength, selectHorizByYawpitch);
	// create message and add to list
	mMessagesOut.emplace((short)GetId(), true, str);
}

std::string EntityLAO::GetName() const
{
	return mInitName;
}

std::string EntityLAO::GetPropertyPacket()
{
	return GenerateSetPropertiesCommand(mProp);
}

void EntityLAO::SendPosition(bool doInterpolate, bool isMovementEnd)
{
	// If the object is attached visual-side, don't waste bandwidth sending its position to visuals
	if(IsAttached())
		return;

	// Send attachment updates instantly to the visual prior updating position
	SendOutdatedData();

	mLastSentMovePrecision = Length(mBasePosition - mLastSentPosition);
	mLastSentPositionTimer = 0;
	mLastSentPosition = mBasePosition;
	mLastSentVelocity = mVelocity;
	//m_last_sent_acceleration = mAcceleration;
	mLastSentRotation = mRotation;

	float updateInterval = mEnvironment->GetSendRecommendedInterval();

	std::string str = GenerateUpdatePositionCommand(
		mBasePosition, mVelocity, mAcceleration, mRotation,
		doInterpolate, isMovementEnd, updateInterval);
	// create message and add to list
	mMessagesOut.emplace((short)GetId(), false, str);
}

bool EntityLAO::GetCollisionBox(BoundingBox<float> *toset) const
{
	if (mProp.physical)
	{
		//update collision box
		toset->mMinEdge = mProp.collisionBox.mMinEdge * BS;
		toset->mMaxEdge = mProp.collisionBox.mMaxEdge * BS;

		toset->mMinEdge += mBasePosition;
		toset->mMaxEdge += mBasePosition;

		return true;
	}

	return false;
}

bool EntityLAO::GetSelectionBox(BoundingBox<float> *toset) const
{
	if (!mProp.isVisible || !mProp.pointable)
		return false;

	toset->mMinEdge = mProp.selectionBox.mMinEdge * BS;
	toset->mMaxEdge = mProp.selectionBox.mMaxEdge * BS;

	return true;
}

bool EntityLAO::CollideWithObjects() const
{
	return mProp.collideWithObjects;
}
