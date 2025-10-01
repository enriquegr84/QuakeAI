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

#ifndef PLAYER_H
#define PLAYER_H

#include "Inventory.h"

#include "Game/Actor/Actor.h"


#define PLAYERNAME_SIZE 20

// Size of player's main inventory
#define PLAYER_INVENTORY_SIZE (8 * 4)

// Default maximum health points of a player
#define PLAYER_MAX_HP_DEFAULT 20

// Default maximal breath of a player
#define PLAYER_MAX_BREATH_DEFAULT 10

// Number of different files to try to save a player to if the first fails
// (because of a case-insensitive filesystem)
// TODO: Use case-insensitive player names instead of this hack.
#define PLAYER_FILE_ALTERNATE_TRIES 1000

#define PLAYERNAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
#define PLAYERNAME_ALLOWED_CHARS_USER_EXPL "'a' to 'z', 'A' to 'Z', '0' to '9', '-', '_'"

struct PlayerFov
{
	float fov;

	// Whether to multiply the visual's FOV or to override it
	bool isMultiplier;

	// The time to be take to trasition to the new FOV value.
	// Transition is instantaneous if omitted. Omitted by default.
	float transitionTime;
};

struct PlayerControl
{
	PlayerControl() = default;

    PlayerControl(
        bool aUp, bool aDown, bool aLeft, bool aRight, bool aJump,
        bool aAux1, bool aSneak, bool aZoom, bool aDig, bool aPlace,
        float aPitch, float aYaw) :
        up(aUp), down(aDown), left(aLeft), right(aRight), jump(aJump),
        aux1(aAux1), sneak(aSneak), zoom(aZoom), dig(aDig), place(aPlace),
        pitch(aPitch), yaw(aYaw)
    {
    }

	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool jump = false;
	bool aux1 = false;
	bool sneak = false;
	bool zoom = false;
	bool dig = false;
	bool place = false;
	float pitch = 0.0f;
	float yaw = 0.0f;
};

struct PlayerSettings
{
	bool freeMove = false;
	bool pitchMove = false;
	bool fastMove = false;
	bool continuousForward = false;
	bool alwaysFlyFast = false;
	bool aux1Descends = false;
	bool noClip = false;
	bool autojump = false;

	const std::string settingNames[8] = {
		"free_move", "pitch_move", "fast_move", "continuous_forward", "always_fly_fast",
		"aux1_descends", "noclip", "autojump"
	};
	void ReadGlobalSettings();
};

class Map;
struct CollisionInfo;
struct HudElement;
class Environment;

/*
    Player Actor class. `Player actor is a character (NPC or human) in the game world.
    Its entire purpose is to manage characters data and mantain components to make them
    as flexible and reusable as possible.
*/
class Player : public Actor
{
public:

    Player(ActorId id, const char* name, BaseItemManager* itemMgr);
    virtual ~Player();

	virtual void Move(float dTime, Environment* env, float posMaxDist) {}
	virtual void Move(float dTime, Environment* env, float posMaxDist,
			std::vector<CollisionInfo>* collisionInfo) {}

	const Vector3<float>& GetSpeed() const { return mSpeed; }
	void SetSpeed(const Vector3<float>& speed) { mSpeed = speed; }
	const char* GetName() const { return mName; }

	unsigned int GetFreeHudID()
	{
		unsigned int size = (unsigned int)mHud.size();
		for (unsigned int i = 0; i != size; i++)
			if (!mHud[i])
				return i;

		return size;
	}

	void SetId(ActorId aId) { mID = aId; }

	const PlayerControl& GetPlayerControl() { return mControl; }
	PlayerSettings& GetPlayerSettings() { return mPlayerSettings; }
	static void SettingsChangedCallback(const std::string& name, void* data);

	// Returns non-empty `selected` ItemStack. `hand` is a fallback, if specified
	ItemStack& GetWieldedItem(ItemStack* selected, ItemStack* hand) const;
	void SetWieldIndex(unsigned int index);
    unsigned int GetWieldIndex() const { return mWieldIndex; }

	void SetFov(const PlayerFov& pFov) { mFovOverride = pFov; }
	const PlayerFov& GetFov() const { return mFovOverride; }

    Vector3<float> mEyeOffsetFirst = Vector3<float>::Zero();
    Vector3<float> mEyeOffsetThird = Vector3<float>::Zero();

    Inventory mInventory;

    float mMovementAccelerationDefault;
    float mMovementAccelerationAir;
    float mMovementAccelerationFast;
    float mMovementSpeedWalk;
    float mMovementSpeedCrouch;
    float mMovementSpeedFast;
    float mMovementSpeedClimb;
    float mMovementSpeedJump;
    float mMovementLiquidFluidity;
    float mMovementLiquidFluiditySmooth;
    float mMovementLiquidSink;
    float mMovementGravity;

    std::string mInventoryForm;
    std::string mFormPrepend;

    PlayerControl mControl;

	unsigned int mKeyPressed = 0;

	HudElement* GetHud(unsigned int id);
	unsigned int AddHud(HudElement* hud);
	HudElement* RemoveHud(unsigned int id);
	void ClearHud();

	unsigned int mHudFlags;
	int mHudHotbarItemCount;

protected:
	char mName[PLAYERNAME_SIZE];
	Vector3<float> mSpeed = Vector3<float>::Zero();
	unsigned int mWieldIndex = 0;
	PlayerFov mFovOverride = { 0.0f, false, 0.0f };

	std::vector<HudElement*> mHud;
private:
	// Protect some critical areas
	// hud for example can be modified by EmergeThread
	std::mutex mMutex;
	PlayerSettings mPlayerSettings;
};

#endif