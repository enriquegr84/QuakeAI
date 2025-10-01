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

#ifndef PLAYERLAO_H
#define PLAYERLAO_H

#include "GameEngineStd.h"

#include "UnitLAO.h"

#include "../../Utils/Util.h"

#include "Core/OS/OS.h"


/*
	PlayerLAO needs some internals exposed.
*/

class LagPool
{

public:
	LagPool() = default;

	void SetMax(float newMax)
	{
		mMax = newMax;
		if (mPool > newMax)
			mPool = newMax;
	}

	void Add(float dTime)
	{
		mPool -= dTime;
		if (mPool < 0)
			mPool = 0;
	}

	void Empty() { mPool = mMax; }

	bool Grab(float dTime)
	{
		if (dTime <= 0)
			return true;
		if (mPool + dTime > mMax)
			return false;
		mPool += dTime;
		return true;
	}

private:

    float mPool = 15.0f;
    float mMax = 15.0f;
};

class LogicPlayer;

class PlayerLAO : public UnitLAO
{
public:
	PlayerLAO(LogicEnvironment* env, LogicPlayer* player, bool isSingleplayer);

	ActiveObjectType GetType() const { return ACTIVEOBJECT_TYPE_PLAYER; }
	ActiveObjectType GetSendType() const { return ACTIVEOBJECT_TYPE_GENERIC; }
	std::string GetDescription();

	/*
		Active object <-> environment interface
	*/
	virtual void GetStaticData(std::string* result) const;

	void AddedToEnvironment(unsigned int dTime);
	void RemovingFromEnvironment();
	bool IsStaticAllowed() const { return false; }
	bool ShouldUnload() const { return false; }
	std::string GetVisualInitializationData();
	void Step(float dTime, bool sendRecommended);
	void SetBasePosition(const Vector3<float>& position);
	void SetPosition(const Vector3<float>& pos);
	void MoveToPosition(Vector3<float> pos, bool continuous);
	void SetPlayerYaw(const float yaw);
	// Data should not be sent at player initialization
	void SetPlayerYawAndSend(const float yaw);
	void SetLookPitch(const float pitch);
	// Data should not be sent at player initialization
	void SetLookPitchAndSend(const float pitch);
	float GetLookPitch() const { return mPitch; }
	float GetRadLookPitch() const { return mPitch * (float)GE_C_DEG_TO_RAD; }
	// Deprecated
	float GetRadLookPitchDep() const { return -1.f * mPitch * (float)GE_C_DEG_TO_RAD; }
	void SetFov(const float pitch);
	float GetFov() const { return mFov; }
	void SetWantedRange(const short range);
	short GetWantedRange() const { return mWantedRange; }

	/*
		Interaction interface
	*/

	uint16_t Punch(Vector3<float> dir, const ToolCapabilities* toolcap, 
        LogicActiveObject* puncher, float timeFromLastPunch);
	void RightClick(LogicActiveObject* clicker);
	void SetHP(int hp, const PlayerHPChangeReason& reason);
	void SetHPRaw(uint16_t hp) { mHP = hp; }
	uint16_t GetBreath() const { return mBreath; }
	void SetBreath(const uint16_t breath, bool send = true);

	/*
		Inventory interface
	*/
	Inventory* GetInventory() const;
	InventoryLocation GetInventoryLocation() const;
	void SetInventoryModified() {}
	std::string GetWieldList() const { return "main"; }
	uint16_t GetWieldIndex() const;
	ItemStack GetWieldedItem(ItemStack* selected, ItemStack* hand = nullptr) const;
	bool SetWieldedItem(const ItemStack& item);

	/*
		PlayerLAO-specific
	*/

	void Disconnected();

	LogicPlayer* GetPlayer() { return mPlayer; }

	// Cheat prevention

	Vector3<float> GetLastGoodPosition() const { return mLastGoodPosition; }
	float ResetTimeFromLastPunch()
	{
		float r = mTimeFromLastPunch;
		mTimeFromLastPunch = 0.0;
		return r;
	}
	void NoCheatDigStart(const Vector3<short>& p)
	{
		mNoCheatDigPos = p;
		mNoCheatDigTime = 0;
	}
	Vector3<short> GetNoCheatDigPosition() { return mNoCheatDigPos; }
	float GetNoCheatDigTime() { return mNoCheatDigTime; }
    void NoCheatDigEnd() { mNoCheatDigPos = Vector3<short>{ 32767, 32767, 32767 }; }
	LagPool& GetDigPool() { return mDigPool; }
	void SetMaxSpeedOverride(const Vector3<float>& vel);
	// Returns true if cheated
	bool CheckMovementCheat();

	// Other

	void UpdatePrivileges(const std::set<std::string>& privs, bool isSingleplayer)
	{
		mPrivs = privs;
		mIsSingleplayer = isSingleplayer;
	}

	bool GetCollisionBox(BoundingBox<float>* toset) const;
	bool GetSelectionBox(BoundingBox<float>* toset) const;
	bool CollideWithObjects() const { return true; }

	void Finalize(LogicPlayer* player, const std::set<std::string>& privs);

	Vector3<float> GetEyePosition() const { return mBasePosition + GetEyeOffset(); }
	Vector3<float> GetEyeOffset() const;
	float GetZoomFOV() const;

	inline Metadata& GetMeta() { return mMeta; }

private:
	std::string GetPropertyPacket();
	void UnlinkPlayerSessionAndSave();
	std::string GenerateUpdatePhysicsOverrideCommand() const;

	LogicPlayer* mPlayer = nullptr;

	// Cheat prevention
	LagPool mDigPool;
	LagPool mMovePool;
	Vector3<float> mLastGoodPosition;
	float mTimeFromLastTeleport = 0.0f;
	float mTimeFromLastPunch = 0.0f;
    Vector3<short> mNoCheatDigPos = Vector3<short>{ 32767, 32767, 32767 };
	float mNoCheatDigTime = 0.0f;
	float mMaxSpeedOverrideTime = 0.0f;
	Vector3<float> mMaxSpeedOverride = Vector3<float>::Zero();

	// Timers
	IntervalLimiter mBreathingInterval;
	IntervalLimiter mDrowningInterval;
	IntervalLimiter mNodeHurtInterval;

	bool mPositionNotSent = false;

	// Cached privileges for enforcement
	std::set<std::string> mPrivs;
	bool mIsSingleplayer;

	uint16_t mBreath = PLAYER_MAX_BREATH_DEFAULT;
	float mPitch = 0.0f;
	float mFov = 0.0f;
	short mWantedRange = 0;

	Metadata mMeta;

public:

	float mPhysicsOverrideSpeed = 1.0f;
	float mPhysicsOverrideJump = 1.0f;
	float mPhysicsOverrideGravity = 1.0f;
	bool mPhysicsOverrideSneak = true;
	bool mPhysicsOverrideSneakGlitch = false;
	bool mPhysicsOverrideNewMove = true;
	bool mPhysicsOverrideSent = false;
};

struct PlayerHPChangeReason
{
	enum Type : uint8_t
	{
		SET_HP,
		PLAYER_PUNCH,
		FALL,
		NODE_DAMAGE,
		DROWNING,
		RESPAWN
	};

	Type type = SET_HP;
	bool fromMod = false;

	// For PLAYER_PUNCH
	LogicActiveObject* object = nullptr;
	// For NODE_DAMAGE
	std::string node;

	bool SetTypeFromString(const std::string &typestr)
	{
		if (typestr == "set_hp")
			type = SET_HP;
		else if (typestr == "punch")
			type = PLAYER_PUNCH;
		else if (typestr == "fall")
			type = FALL;
		else if (typestr == "node_damage")
			type = NODE_DAMAGE;
		else if (typestr == "drown")
			type = DROWNING;
		else if (typestr == "respawn")
			type = RESPAWN;
		else
			return false;

		return true;
	}

	std::string GetTypeAsString() const
	{
		switch (type) 
        {
		    case PlayerHPChangeReason::SET_HP:
			    return "set_hp";
		    case PlayerHPChangeReason::PLAYER_PUNCH:
			    return "punch";
		    case PlayerHPChangeReason::FALL:
			    return "fall";
		    case PlayerHPChangeReason::NODE_DAMAGE:
			    return "node_damage";
		    case PlayerHPChangeReason::DROWNING:
			    return "drown";
		    case PlayerHPChangeReason::RESPAWN:
			    return "respawn";
		    default:
			    return "?";
		}
	}

	PlayerHPChangeReason(Type type) : type(type) {}

	PlayerHPChangeReason(Type type, LogicActiveObject *object) :
			type(type), object(object)
	{
	}

	PlayerHPChangeReason(Type type, std::string node) : type(type), node(node) {}
};

#endif