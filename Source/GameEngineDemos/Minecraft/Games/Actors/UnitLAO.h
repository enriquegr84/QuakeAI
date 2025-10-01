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


#ifndef UNITLAO_H
#define UNITLAO_H

#include "ObjectProperties.h"
#include "LogicActiveObject.h"

class UnitLAO : public LogicActiveObject
{
public:
	UnitLAO(LogicEnvironment* env, Vector3<float> pos);
	virtual ~UnitLAO() = default;

	uint16_t GetHP() const { return mHP; }
	// Use a function, if IsDead can be defined by other conditions
	bool IsDead() const { return mHP == 0; }

	// Rotation
	void SetRotation(Vector3<float> rotation) { mRotation = rotation; }
	const Vector3<float>& GetRotation() const { return mRotation; }
	Vector3<float> GetRadRotation() { return mRotation * (float)GE_C_DEG_TO_RAD; }

	// Deprecated
	float GetRadYawDep() const { return (mRotation[1] + 90.f) * (float)GE_C_DEG_TO_RAD; }

	// Armor groups
	inline bool IsImmortal() const { return ItemGroupGet(GetArmorGroups(), "Immortal"); }
	void SetArmorGroups(const ItemGroupList& armorGroups);
	const ItemGroupList& GetArmorGroups() const;

	// Animation
	void SetAnimation(Vector2<float> frameRange, float frameSpeed, float frameBlend, bool frameLoop);
	void GetAnimation(Vector2<float>* frameRange, float* frameSpeed, float* frameBlend, bool* frameLoop);
	void SetAnimationSpeed(float frameSpeed);

	// Bone position
	void SetBonePosition(const std::string& bone, Vector3<float> position, Vector3<float> rotation);
	void GetBonePosition(const std::string& bone, Vector3<float>* position, Vector3<float>* rotation);

	// Attachments
	LogicActiveObject* GetParent() const;
	inline bool IsAttached() const { return GetParent(); }
	void SetAttachment(int parentId, const std::string& bone, Vector3<float> position,
			Vector3<float> rotation, bool forceVisible);
	void GetAttachment(int* parentId, std::string* bone, Vector3<float>* position,
			Vector3<float>* rotation, bool* forceVisible) const;
	void ClearChildAttachments();
	void ClearParentAttachment();
	void AddAttachmentChild(int childId);
	void RemoveAttachmentChild(int childId);
	const std::unordered_set<int>& GetAttachmentChildIds() const;

	// Object properties
	ObjectProperties* AccessObjectProperties();
	void NotifyObjectPropertiesModified();
	void SendOutdatedData();

	// Update packets
	std::string GenerateUpdateAttachmentCommand() const;
    std::string GenerateUpdateAnimationSpeedCommand() const;
    std::string GenerateUpdateAnimationCommand() const;
    std::string GenerateUpdateArmorGroupsCommand() const;
	static std::string GenerateUpdatePositionCommand(
        const Vector3<float>& position, const Vector3<float>& velocity, 
        const Vector3<float>& acceleration, const Vector3<float>& rotation,
        bool doInterpolate, bool isMovementEnd, float updateInterval);
	std::string GenerateSetPropertiesCommand(const ObjectProperties& prop) const;
	static std::string GenerateUpdateBonePositionCommand(const std::string& bone,
			const Vector3<float>& position, const Vector3<float>& rotation);
	void SendPunchCommand();

protected:
	uint16_t mHP = 1;

	Vector3<float> mRotation = Vector3<float>::Zero();

	ItemGroupList mArmorGroups;

	// Object properties
	bool mPropertiesSent = true;
	ObjectProperties mProp;

	// Stores position and rotation for each bone name
	std::unordered_map<std::string, Vector2<Vector3<float>>> mBonePosition;

	int mAttachmentParentId = 0;

private:
	void OnAttach(int parentId);
	void OnDetach(int parentId);

	std::string GeneratePunchCommand(uint16_t resultHP) const;

	// Armor groups
	bool mArmorGroupsSent = false;

	// Animation
	Vector2<float> mAnimationRange = Vector2<float>::Zero();
	float mAnimationSpeed = 0.0f;
	float mAnimationBlend = 0.0f;
	bool mAnimationLoop = true;
	bool mAnimationSent = false;
	bool mAnimationSpeedSent = false;

	// Bone positions
	bool mBonePositionSent = false;

	// Attachments
	std::unordered_set<int> mAttachmentChildIds;
    std::string mAttachmentBone = "";
	Vector3<float> mAttachmentPosition = Vector3<float>::Zero();
	Vector3<float> mAttachmentRotation = Vector3<float>::Zero();
	bool mAttachmentSent = false;
	bool mForceVisible = false;
};

#endif