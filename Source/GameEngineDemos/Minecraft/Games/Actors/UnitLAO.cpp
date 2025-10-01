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

#include "UnitLAO.h"

#include "../Environment/LogicEnvironment.h"

#include "../../Games/Games.h"

#include "Core/Utility/Serialize.h"


UnitLAO::UnitLAO(LogicEnvironment *env, Vector3<float> pos) : LogicActiveObject(env, pos)
{
	// Initialize something to armor groups
	mArmorGroups["Fleshy"] = 100;
}

LogicActiveObject* UnitLAO::GetParent() const
{
	if (!mAttachmentParentId)
		return nullptr;

	// Check if the parent still exists
	LogicActiveObject* obj = mEnvironment->GetActiveObject(mAttachmentParentId);
	return obj;
}

void UnitLAO::SetArmorGroups(const ItemGroupList& armorGroups)
{
	mArmorGroups = armorGroups;
	mArmorGroupsSent = false;
}

const ItemGroupList& UnitLAO::GetArmorGroups() const
{
	return mArmorGroups;
}

void UnitLAO::SetAnimation(
		Vector2<float> frameRange, float frameSpeed, float frameBlend, bool frameLoop)
{
	// store these so they can be updated to visuals
	mAnimationRange = frameRange;
	mAnimationSpeed = frameSpeed;
	mAnimationBlend = frameBlend;
	mAnimationLoop = frameLoop;
	mAnimationSent = false;
}

void UnitLAO::GetAnimation(
    Vector2<float>* frameRange, float* frameSpeed, float* frameBlend, bool* frameLoop)
{
	*frameRange = mAnimationRange;
	*frameSpeed = mAnimationSpeed;
	*frameBlend = mAnimationBlend;
	*frameLoop = mAnimationLoop;
}

void UnitLAO::SetAnimationSpeed(float frameSpeed)
{
	mAnimationSpeed = frameSpeed;
	mAnimationSpeedSent = false;
}

void UnitLAO::SetBonePosition(const std::string& bone, Vector3<float> position, Vector3<float> rotation)
{
	// store these so they can be updated to visuals
    mBonePosition[bone] = Vector2<Vector3<float>>{ position, rotation };
	mBonePositionSent = false;
}

void UnitLAO::GetBonePosition(const std::string& bone, Vector3<float>* position, Vector3<float>* rotation)
{
	*position = mBonePosition[bone][0];
	*rotation = mBonePosition[bone][1];
}

// clang-format off
void UnitLAO::SendOutdatedData()
{
	if (!mArmorGroupsSent) 
    {
		mArmorGroupsSent = true;
		mMessagesOut.emplace((short)GetId(), true, GenerateUpdateArmorGroupsCommand());
	}

	if (!mAnimationSent) 
    {
		mAnimationSent = true;
		mAnimationSpeedSent = true;
		mMessagesOut.emplace((short)GetId(), true, GenerateUpdateAnimationCommand());
	} 
    else if (!mAnimationSpeedSent) 
    {
		// Animation speed is also sent when 'mAnimationSent == false'
		mAnimationSpeedSent = true;
		mMessagesOut.emplace((short)GetId(), true, GenerateUpdateAnimationSpeedCommand());
	}

	if (!mBonePositionSent) 
    {
		mBonePositionSent = true;
		for (const auto& bonePos : mBonePosition) 
        {
			mMessagesOut.emplace((short)GetId(), true, GenerateUpdateBonePositionCommand(
                bonePos.first, bonePos.second[0], bonePos.second[1]));
		}
	}

	if (!mAttachmentSent) 
    {
		mAttachmentSent = true;
		mMessagesOut.emplace((short)GetId(), true, GenerateUpdateAttachmentCommand());
	}
}
// clang-format on

void UnitLAO::SetAttachment(int parentId, const std::string& bone, 
    Vector3<float> position, Vector3<float> rotation, bool forceVisible)
{
	// Attachments need to be handled on both the logic and visual.
	// If we just attach on the logic, we can only copy the position of the parent.
	// Attachments are still sent to visuals at an interval so players might see them
	// lagging, plus we can't read and attach to skeletal bones. If we just attach on
	// the visual, the logic still sees the child at its original location. This
	// breaks some things so we also give the logic the most accurate representation
	// even if players only see the visual changes.

	int oldParent = mAttachmentParentId;
	mAttachmentParentId = parentId;
	mAttachmentBone = bone;
	mAttachmentPosition = position;
	mAttachmentRotation = rotation;
	mForceVisible = forceVisible;
	mAttachmentSent = false;

	if (parentId != oldParent) 
    {
		OnDetach(oldParent);
		OnAttach(parentId);
	}
}

void UnitLAO::GetAttachment(int* parentId, std::string* bone, 
    Vector3<float>* position, Vector3<float>* rotation, bool* forceVisible) const
{
	*parentId = mAttachmentParentId;
	*bone = mAttachmentBone;
	*position = mAttachmentPosition;
	*rotation = mAttachmentRotation;
	*forceVisible = mForceVisible;
}

void UnitLAO::ClearChildAttachments()
{
	for (int childId : mAttachmentChildIds) 
    {
		// Child can be NULL if it was deleted earlier
		if (LogicActiveObject* child = mEnvironment->GetActiveObject(childId))
			child->SetAttachment(0, "", Vector3<float>::Zero(), Vector3<float>::Zero(), false);
	}
	mAttachmentChildIds.clear();
}

void UnitLAO::ClearParentAttachment()
{
	LogicActiveObject* parent = nullptr;
	if (mAttachmentParentId) 
    {
		parent = mEnvironment->GetActiveObject(mAttachmentParentId);
		SetAttachment(0, "", mAttachmentPosition, mAttachmentRotation, false);
	} 
    else 
    {
		SetAttachment(0, "", Vector3<float>::Zero(), Vector3<float>::Zero(), false);
	}
	// Do it
	if (parent)
		parent->RemoveAttachmentChild(mId);
}

void UnitLAO::AddAttachmentChild(int childId)
{
	mAttachmentChildIds.insert(childId);
}

void UnitLAO::RemoveAttachmentChild(int childId)
{
	mAttachmentChildIds.erase(childId);
}

const std::unordered_set<int>& UnitLAO::GetAttachmentChildIds() const
{
	return mAttachmentChildIds;
}

void UnitLAO::OnAttach(int parentId)
{
	if (!parentId)
		return;

	LogicActiveObject* parent = mEnvironment->GetActiveObject(parentId);

	if (!parent || parent->IsGone())
		return; // Do not try to notify soon gone parent

	if (parent->GetType() == ACTIVEOBJECT_TYPE_ENTITY) 
    {
		// Call parent's on_attach field
        BaseGame::Get()->OnAttachChildEntity(parent, this);
	}
}

void UnitLAO::OnDetach(int parentId)
{
	if (!parentId)
		return;

	LogicActiveObject* parent = mEnvironment->GetActiveObject(parentId);
	if (GetType() == ACTIVEOBJECT_TYPE_ENTITY)
        BaseGame::Get()->OnDetachChildEntity(this, parent);

	if (!parent || parent->IsGone())
		return; // Do not try to notify soon gone parent

	if (parent->GetType() == ACTIVEOBJECT_TYPE_ENTITY)
        BaseGame::Get()->OnDetachChildEntity(parent, this);
}

ObjectProperties* UnitLAO::AccessObjectProperties()
{
	return &mProp;
}

void UnitLAO::NotifyObjectPropertiesModified()
{
	mPropertiesSent = false;
}

std::string UnitLAO::GenerateUpdateAttachmentCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_ATTACH_TO);
	// parameters
	WriteInt16(os, mAttachmentParentId);
	os << SerializeString16(mAttachmentBone);
	WriteV3Float(os, mAttachmentPosition);
    WriteV3Float(os, mAttachmentRotation);
	WriteUInt8(os, mForceVisible);
	return os.str();
}

std::string UnitLAO::GenerateUpdateBonePositionCommand(
    const std::string& bone, const Vector3<float>& position, const Vector3<float>& rotation)
{
	std::ostringstream os(std::ios::binary);
	// command
    WriteUInt8(os, AO_CMD_SET_BONE_POSITION);
	// parameters
	os << SerializeString16(bone);
    WriteV3Float(os, position);
    WriteV3Float(os, rotation);
	return os.str();
}

std::string UnitLAO::GenerateUpdateAnimationSpeedCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
    WriteUInt8(os, AO_CMD_SET_ANIMATION_SPEED);
	// parameters
    WriteFloat(os, mAnimationSpeed);
	return os.str();
}

std::string UnitLAO::GenerateUpdateAnimationCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
    WriteUInt8(os, AO_CMD_SET_ANIMATION);
	// parameters
	WriteV2Float(os, mAnimationRange);
	WriteFloat(os, mAnimationSpeed);
    WriteFloat(os, mAnimationBlend);
	// these are sent inverted so we get true when the logic sends nothing
    WriteUInt8(os, !mAnimationLoop);
	return os.str();
}

std::string UnitLAO::GenerateUpdateArmorGroupsCommand() const
{
	std::ostringstream os(std::ios::binary);
	WriteUInt8(os, AO_CMD_UPDATE_ARMOR_GROUPS);
	WriteUInt16(os, (uint16_t)mArmorGroups.size());
	for (const auto& armorGroup : mArmorGroups) 
    {
		os << SerializeString16(armorGroup.first);
		WriteInt16(os, armorGroup.second);
	}
	return os.str();
}

std::string UnitLAO::GenerateUpdatePositionCommand(
    const Vector3<float>& position, const Vector3<float>& velocity, 
    const Vector3<float>& acceleration, const Vector3<float>& rotation,
    bool doInterpolate, bool isMovementEnd, float updateInterval)
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_UPDATE_POSITION);
	// pos
	WriteV3Float(os, position);
	// velocity
	WriteV3Float(os, velocity);
	// acceleration
	WriteV3Float(os, acceleration);
	// rotation
	WriteV3Float(os, rotation);
	// doInterpolate
	WriteUInt8(os, doInterpolate);
	// is_end_position (for interpolation)
	WriteUInt8(os, isMovementEnd);
	// updateInterval (for interpolation)
	WriteFloat(os, updateInterval);
	return os.str();
}

std::string UnitLAO::GenerateSetPropertiesCommand(const ObjectProperties& prop) const
{
	std::ostringstream os(std::ios::binary);
	WriteUInt8(os, AO_CMD_SET_PROPERTIES);
	prop.Serialize(os);
	return os.str();
}

std::string UnitLAO::GeneratePunchCommand(uint16_t resultHP) const
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_PUNCHED);
	// resultHP
	WriteUInt16(os, resultHP);
	return os.str();
}

void UnitLAO::SendPunchCommand()
{
	mMessagesOut.emplace((short)GetId(), true, GeneratePunchCommand(GetHP()));
}
