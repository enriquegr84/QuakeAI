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


#ifndef ENTITYLAO_H
#define ENTITYLAO_H

#include "UnitLAO.h"

class EntityLAO : public UnitLAO
{
public:
	EntityLAO() = delete;
	// Used by the environment to load LAO
	EntityLAO(LogicEnvironment* env, Vector3<float> pos, const std::string& data);
	// Used by the  API
	EntityLAO(LogicEnvironment *env, Vector3<float> pos, 
        const std::string& name, const std::string& state) 
        : UnitLAO(env, pos), mInitName(name), mInitState(state)
	{

	}

	~EntityLAO();

	virtual void AddedToEnvironment(unsigned int dTime);
	virtual void GetStaticData(std::string* result) const;

	ActiveObjectType GetType() const { return ACTIVEOBJECT_TYPE_ENTITY; }
	ActiveObjectType GetSendType() const { return ACTIVEOBJECT_TYPE_GENERIC; }
	void Step(float deltaTime, bool sendRecommended);
	std::string GetVisualInitializationData();
	bool IsStaticAllowed() const { return mProp.staticSave; }
	bool ShouldUnload() const { return true; }
	uint16_t Punch(Vector3<float> dir, const ToolCapabilities* toolcap = nullptr,
        LogicActiveObject* puncher = nullptr, float timeFromLastPunch = 1000000.0f);
	void RightClick(LogicActiveObject* clicker);
	void SetPosition(const Vector3<float>& pos);
	void MoveTo(Vector3<float> pos, bool continuous);
	float GetMinimumSavedMovement();
	std::string GetDescription();
	void SetHP(int hp, const PlayerHPChangeReason &reason);
	uint16_t GetHP() const;

	/* EntityLAO-specific */
	void SetVelocity(Vector3<float> velocity);
	void AddVelocity(Vector3<float> velocity) { mVelocity += velocity; }
	Vector3<float> GetVelocity();
	void SetAcceleration(Vector3<float> acceleration);
	Vector3<float> GetAcceleration();

	void SetTextureMod(const std::string& mod);
	std::string GetTextureMod() const;
	void SetSprite(Vector2<short> p, int numFrames, float frameLength, bool selectHorizByYawpitch);
	std::string GetName() const;
	bool GetCollisionBox(BoundingBox<float>* toset) const;
	bool GetSelectionBox(BoundingBox<float>* toset) const;
	bool CollideWithObjects() const;

protected:
	void DispatchScriptDeactivate();
	virtual void OnMarkedForDeactivation() { DispatchScriptDeactivate(); }
	virtual void OnMarkedForRemoval() { DispatchScriptDeactivate(); }

private:
	std::string GetPropertyPacket();
	void SendPosition(bool doInterpolate, bool isMovementEnd);
	std::string GenerateSetTextureModCommand() const;
	static std::string GenerateSetSpriteCommand(Vector2<short> p, uint16_t numFrames,
        float frameLength, bool selectHorizByYawpitch);

    std::string mInitName;
    std::string mInitState;
	bool mRegistered = false;

	Vector3<float> mVelocity = Vector3<float>::Zero();
	Vector3<float> mAcceleration = Vector3<float>::Zero();

	Vector3<float> mLastSentPosition = Vector3<float>::Zero();
	Vector3<float> mLastSentVelocity = Vector3<float>::Zero();
	Vector3<float> mLastSentRotation = Vector3<float>::Zero();
	float mLastSentPositionTimer = 0.0f;
	float mLastSentMovePrecision = 0.0f;
	std::string mCurrentTextureModifier = "";
};


#endif