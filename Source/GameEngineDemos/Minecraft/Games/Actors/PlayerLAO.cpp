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

#include "PlayerLAO.h"
#include "LogicPlayer.h"

#include "../Environment/LogicEnvironment.h"

#include "../Map/Map.h"
#include "../Map/MapNode.h"

#include "../../Graphics/Node.h"

#include "../../Games/Games.h"

#include "Application/Settings.h"

#include "Core/Utility/Serialize.h"


PlayerLAO::PlayerLAO(LogicEnvironment* env, LogicPlayer* player, bool isSingleplayer) :
	UnitLAO(env, Vector3<float>::Zero()), mPlayer(player), mIsSingleplayer(isSingleplayer)
{
	mId = player->GetId();
	LogAssert(mId != INVALID_ACTOR_ID, "inexistent actor id");

	mProp.hpMax = PLAYER_MAX_HP_DEFAULT;
	mProp.breathMax = PLAYER_MAX_BREATH_DEFAULT;
	mProp.physical = false;
	mProp.collisionBox = BoundingBox<float>(-0.3f, 0.0f, -0.3f, 0.3f, 1.77f, 0.3f);
	mProp.selectionBox = BoundingBox<float>(-0.3f, 0.0f, -0.3f, 0.3f, 1.77f, 0.3f);
	mProp.pointable = true;
	// Start of default appearance, this should be overwritten
	mProp.visual = "upright_sprite";
    mProp.visualSize = Vector3<float>{ 1, 2, 1 };
	mProp.textures.clear();
	mProp.textures.emplace_back("player.png");
	mProp.textures.emplace_back("player_back.png");
	mProp.colors.clear();
	mProp.colors.emplace_back(255, 255, 255, 255);
    mProp.spriteDiv = Vector2<short>{ 1, 1 };
	mProp.eyeHeight = 1.625f;
	// End of default appearance
	mProp.isVisible = true;
	mProp.backfaceCulling = false;
	mProp.makesFootstepSound = true;
	mProp.stepHeight = PLAYER_DEFAULT_STEPHEIGHT * BS;
	mProp.showOnMinimap = true;
	mHP = mProp.hpMax;
	mBreath = mProp.breathMax;
	// Disable zoom in survival mode using a value of 0
	mProp.zoomFov = Settings::Get()->GetBool("creative_mode") ? 15.0f : 0.0f;

	if (!Settings::Get()->GetBool("enable_damage"))
		mArmorGroups["Immortal"] = 1;
}

void PlayerLAO::Finalize(LogicPlayer* player, const std::set<std::string>& privs)
{
	LogAssert(player, "invalid player");
	mPlayer = player;
	mPrivs = privs;
}

Vector3<float> PlayerLAO::GetEyeOffset() const
{
    return Vector3<float>{0, BS * mProp.eyeHeight, 0};
}

std::string PlayerLAO::GetDescription()
{
	return std::string("player ") + mPlayer->GetName();
}

// Called after id has been set and has been inserted in environment
void PlayerLAO::AddedToEnvironment(unsigned int dTime)
{
	LogicActiveObject::AddedToEnvironment(dTime);
	LogicActiveObject::SetBasePosition(mBasePosition);
	mPlayer->SetPlayerLAO(this);
	mLastGoodPosition = mBasePosition;
}

// Called before removing from environment
void PlayerLAO::RemovingFromEnvironment()
{
	LogicActiveObject::RemovingFromEnvironment();
	if (mPlayer->GetPlayerLAO() == this) 
    {
		UnlinkPlayerSessionAndSave();
		for (unsigned int attachedParticleSpawner : mAttachedParticleSpawners)
			mEnvironment->DeleteParticleSpawner(attachedParticleSpawner, false);
	}
}

std::string PlayerLAO::GetVisualInitializationData()
{
	std::ostringstream os(std::ios::binary);

	// Protocol >= 15
	WriteUInt8(os, 1); // version
	os << SerializeString16(mPlayer->GetName()); // name
	WriteUInt8(os, 1); // is_player
	WriteInt16(os, GetId()); // id
	WriteV3Float(os, mBasePosition);
	WriteV3Float(os, mRotation);
	WriteUInt16(os, GetHP());

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
	msgOs << SerializeString32(GenerateUpdatePhysicsOverrideCommand()); // 5 + mBonePosition.size

	unsigned int messageCount = 5 + (unsigned int)mBonePosition.size();
	for (const auto& id : GetAttachmentChildIds()) 
    {
		if (LogicActiveObject* obj = mEnvironment->GetActiveObject(id)) 
        {
			messageCount++;
			msgOs << SerializeString32(obj->GenerateUpdateInfantCommand(id));
		}
	}

	WriteUInt8(os, messageCount);
	std::string serialized = msgOs.str();
	os.write(serialized.c_str(), serialized.size());

	// return result
	return os.str();
}

void PlayerLAO::GetStaticData(std::string* result) const
{
	LogError("This function shall not be called for PlayerLAO");
}

void PlayerLAO::Step(float dTime, bool sendRecommended)
{
	if (!IsImmortal() && mDrowningInterval.Step(dTime, 2.0f))
    {
		// Get nose/mouth position, approximate with eye position
        Vector3<float> eyePos = GetEyePosition();
        Vector3<short> p;
        p[0] = (short)((eyePos[0] + (eyePos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[1] = (short)((eyePos[1] + (eyePos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[2] = (short)((eyePos[2] + (eyePos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		MapNode node = mEnvironment->GetMap()->GetNode(p);
		const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
		// If node generates drown
		if (cFeatures.drowning > 0 && mHP > 0)
        {
			if (mBreath > 0)
				SetBreath(mBreath - 1);

			// No more breath, damage player
			if (mBreath == 0) 
            {
				PlayerHPChangeReason reason(PlayerHPChangeReason::DROWNING);
				SetHP(mHP - cFeatures.drowning, reason);
				mEnvironment->SendPlayerHPOrDie(this, reason);
			}
		}
	}

	if (mBreathingInterval.Step(dTime, 0.5f) && !IsImmortal())
    {
		// Get nose/mouth position, approximate with eye position
        Vector3<float> eyePos = GetEyePosition();
        Vector3<short> p;
        p[0] = (short)((eyePos[0] + (eyePos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[1] = (short)((eyePos[1] + (eyePos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[2] = (short)((eyePos[2] + (eyePos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        MapNode node = mEnvironment->GetMap()->GetNode(p);
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
		// If player is alive & not drowning & not in ignore & not immortal, breathe
		if (mBreath < mProp.breathMax && cFeatures.drowning == 0 && node.GetContent() != CONTENT_IGNORE && mHP > 0)
			SetBreath(mBreath + 1);
	}

	if (!IsImmortal() && mNodeHurtInterval.Step(dTime, 1.0f))
    {
		unsigned int damagePerSecond = 0;
		std::string nodename;
		// Lowest and highest damage points are 0.1 within collisionbox
		float damTop = mProp.collisionBox.mMaxEdge[1] - 0.1f;

		// Sequence of damage points, starting 0.1 above feet and progressing
		// upwards in 1 node intervals, stopping below top damage point.
		for (float damHeight = 0.1f; damHeight < damTop; damHeight++) 
        {
            Vector3<float> pos = mBasePosition + Vector3<float>{0.0f, damHeight * BS, 0.0f};
            Vector3<short> p;
            p[0] = (short)((pos[0] + (pos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
            p[1] = (short)((pos[1] + (pos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
            p[2] = (short)((pos[2] + (pos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

            MapNode node = mEnvironment->GetMap()->GetNode(p);
            const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
			if (cFeatures.damagePerSecond > damagePerSecond)
            {
				damagePerSecond = cFeatures.damagePerSecond;
				nodename = cFeatures.name;
			}
		}

		// Top damage point
        Vector3<float> pos = mBasePosition + Vector3<float>{0.0f, damTop * BS, 0.0f};
        Vector3<short> ptop;
        ptop[0] = (short)((pos[0] + (pos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        ptop[1] = (short)((pos[1] + (pos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        ptop[2] = (short)((pos[2] + (pos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		MapNode ntop = mEnvironment->GetMap()->GetNode(ptop);
		const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(ntop);
		if (cFeatures.damagePerSecond > damagePerSecond)
        {
			damagePerSecond = cFeatures.damagePerSecond;
			nodename = cFeatures.name;
		}

		if (damagePerSecond != 0 && mHP > 0) 
        {
			int newhp = (int)mHP - (int)damagePerSecond;
			PlayerHPChangeReason reason(PlayerHPChangeReason::NODE_DAMAGE, nodename);
			SetHP(newhp, reason);
            mEnvironment->SendPlayerHPOrDie(this, reason);
		}
	}

	if (!mPropertiesSent) 
    {
		mPropertiesSent = true;
		std::string str = GetPropertyPacket();
		// create message and add to list
		mMessagesOut.emplace((short)GetId(), true, str);
        BaseGame::Get()->OnEventPlayer(this, "properties_changed");
	}

	// If attached, check that our parent is still there. If it isn't, detach.
	if (mAttachmentParentId && !IsAttached()) 
    {
		// This is handled when objects are removed from the map
		LogWarning("PlayerLAO::step() id=" + std::to_string(mId) +
			" is attached to nonexistent parent. This is a bug.");
		ClearParentAttachment();
		SetBasePosition(mLastGoodPosition);
        mEnvironment->SendPlayerMove(this);
	}

	//dstream<<"PlayerLAO::step: dtime: "<<dtime<<std::endl;

	// Set lag pool maximums based on estimated lag
	const float LAG_POOL_MIN = 5.0f;
	float lagPoolMax = mEnvironment->GetMaxLagEstimate() * 2.0f;
	if(lagPoolMax < LAG_POOL_MIN)
		lagPoolMax = LAG_POOL_MIN;
	mDigPool.SetMax(lagPoolMax);
	mMovePool.SetMax(lagPoolMax);

	// Increment cheat prevention timers
	mDigPool.Add(dTime);
	mMovePool.Add(dTime);
	mTimeFromLastTeleport += dTime;
	mTimeFromLastPunch += dTime;
	mNoCheatDigTime += dTime;
	mMaxSpeedOverrideTime = std::max(mMaxSpeedOverrideTime - dTime, 0.0f);

	// Each frame, parent position is copied if the object is attached,
	// otherwise it's calculated normally.
	// If the object gets detached this comes into effect automatically from
	// the last known origin.
	if (auto* parent = GetParent()) 
    {
		Vector3<float> pos = parent->GetBasePosition();
		mLastGoodPosition = pos;
		SetBasePosition(pos);

		if (mPlayer)
			mPlayer->SetSpeed(Vector3<float>());
	}

	if (!sendRecommended)
		return;

	if (mPositionNotSent) 
    {
		mPositionNotSent = false;
		float updateInterval = mEnvironment->GetSendRecommendedInterval();
		Vector3<float> pos;
		// When attached, the position is only sent to visuals where the
		// parent isn't known
		if (IsAttached())
			pos = mLastGoodPosition;
		else
			pos = mBasePosition;

		std::string str = GenerateUpdatePositionCommand(
            pos, Vector3<float>::Zero(), Vector3<float>::Zero(),
			mRotation, true, false, updateInterval);
		// create message and add to list
		mMessagesOut.emplace((short)GetId(), false, str);
	}

	if (!mPhysicsOverrideSent) 
    {
		mPhysicsOverrideSent = true;
		// create message and add to list
		mMessagesOut.emplace((short)GetId(), true, GenerateUpdatePhysicsOverrideCommand());
	}

	SendOutdatedData();
}

std::string PlayerLAO::GenerateUpdatePhysicsOverrideCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_SET_PHYSICS_OVERRIDE);
	// parameters
	WriteFloat(os, mPhysicsOverrideSpeed);
	WriteFloat(os, mPhysicsOverrideJump);
	WriteFloat(os, mPhysicsOverrideGravity);
	// these are sent inverted so we get true when the logic sends nothing
	WriteUInt8(os, !mPhysicsOverrideSneak);
	WriteUInt8(os, !mPhysicsOverrideSneakGlitch);
	WriteUInt8(os, !mPhysicsOverrideNewMove);
	return os.str();
}

void PlayerLAO::SetBasePosition(const Vector3<float>& position)
{
	if (mPlayer && position != mBasePosition)
		mPlayer->SetDirty(true);

	// This needs to be ran for attachments too
	LogicActiveObject::SetBasePosition(position);

	// Updating is not wanted/required for player migration
	if (mEnvironment)
		mPositionNotSent = true;
}

void PlayerLAO::SetPosition(const Vector3<float>& pos)
{
	if(IsAttached())
		return;

	// Send mapblock of target location
    Vector3<short> blockpos = Vector3<short>{ 
        (short)(pos[0] / MAP_BLOCKSIZE), 
        (short)(pos[1] / MAP_BLOCKSIZE), 
        (short)(pos[2] / MAP_BLOCKSIZE) };
    mEnvironment->SendBlock(mId, blockpos);

	SetBasePosition(pos);
	// Movement caused by this command is always valid
	mLastGoodPosition = pos;
	mMovePool.Empty();
	mTimeFromLastTeleport = 0.0;
    mEnvironment->SendPlayerMove(this);
}

void PlayerLAO::MoveToPosition(Vector3<float> pos, bool continuous)
{
	if(IsAttached())
		return;

	SetBasePosition(pos);
	// Movement caused by this command is always valid
	mLastGoodPosition = pos;
	mMovePool.Empty();
	mTimeFromLastTeleport = 0.0;
    mEnvironment->SendPlayerMove(this);
}

void PlayerLAO::SetPlayerYaw(const float yaw)
{
    Vector3<float> rotation{ 0, yaw, 0 };
	if (mPlayer && yaw != mRotation[1])
		mPlayer->SetDirty(true);

	// Set player model yaw, not look view
	UnitLAO::SetRotation(rotation);
}

void PlayerLAO::SetFov(const float fov)
{
	if (mPlayer && fov != mFov)
		mPlayer->SetDirty(true);

	mFov = fov;
}

void PlayerLAO::SetWantedRange(const short range)
{
	if (mPlayer && range != mWantedRange)
		mPlayer->SetDirty(true);

	mWantedRange = range;
}

void PlayerLAO::SetPlayerYawAndSend(const float yaw)
{
	SetPlayerYaw(yaw);
    mEnvironment->SendPlayerMove(this);
}

void PlayerLAO::SetLookPitch(const float pitch)
{
	if (mPlayer && pitch != mPitch)
		mPlayer->SetDirty(true);

	mPitch = pitch;
}

void PlayerLAO::SetLookPitchAndSend(const float pitch)
{
	SetLookPitch(pitch);
    mEnvironment->SendPlayerMove(this);
}

uint16_t PlayerLAO::Punch(Vector3<float> dir,
	const ToolCapabilities* toolcap, LogicActiveObject* puncher, float timeFromLastPunch)
{
	if (!toolcap)
		return 0;

	LogAssert(puncher, "Punch action called without LAO");

	// No effect if PvP disabled or if immortal
	if (IsImmortal() || !Settings::Get()->GetBool("enable_pvp")) 
    {
		if (puncher->GetType() == ACTIVEOBJECT_TYPE_PLAYER) 
        {
			// create message and add to list
			SendPunchCommand();
			return 0;
		}
	}

	int oldHp = GetHP();
	HitParams hitparams = GetHitParams(mArmorGroups, toolcap, timeFromLastPunch);

	PlayerLAO* playerLAO = mPlayer->GetPlayerLAO();
	bool damageHandled = BaseGame::Get()->OnPunchPlayer(playerLAO,
        puncher, timeFromLastPunch, toolcap, dir, hitparams.hp);

	if (!damageHandled) 
    {
		SetHP((int)GetHP() - (int)hitparams.hp,
            PlayerHPChangeReason(PlayerHPChangeReason::PLAYER_PUNCH, puncher));
	} 
    else 
    { 
        // override visual prediction
		if (puncher->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
        {
			// create message and add to list
			SendPunchCommand();
		}
	}

	LogInformation(puncher->GetDescription() + " (id=" + std::to_string(puncher->GetId()) +
		", hp=" + std::to_string(puncher->GetHP()) + ") punched " + GetDescription() + 
        " (id=" + std::to_string(mId) + ", hp=" + std::to_string(mHP) +
		"), damage=" + std::to_string(oldHp - (int)GetHP()) + 
        (damageHandled ? " (handled by scripting)" : ""));

	return hitparams.wear;
}

void PlayerLAO::RightClick(LogicActiveObject* clicker)
{
    BaseGame::Get()->OnRightClickPlayer(this, clicker);
}

void PlayerLAO::SetHP(int hp, const PlayerHPChangeReason& reason)
{
	if (hp == (int)mHP)
		return; // Nothing to do

	if (mHP <= 0 && hp < (int)mHP)
		return; // Cannot take more damage

	{
		int hpChange = BaseGame::Get()->OnHPChangePlayer(this, hp - mHP, reason);
		if (hpChange == 0)
			return;

		hp = mHP + hpChange;
	}

	int oldhp = mHP;
	hp = std::clamp(hp, 0, (int)mProp.hpMax);

	if (hp < oldhp && IsImmortal())
		return; // Do not allow immortal players to be damaged

	mHP = hp;

	// Update properties on death
	if ((hp == 0) != (oldhp == 0))
		mPropertiesSent = false;
}

void PlayerLAO::SetBreath(const uint16_t breath, bool send)
{
	if (mPlayer && breath != mBreath)
		mPlayer->SetDirty(true);

	mBreath = std::clamp(breath, (uint16_t)0, mProp.breathMax);

	if (send)
        mEnvironment->SendPlayerBreath(this);
}

Inventory* PlayerLAO::GetInventory() const
{
	return mPlayer ? &mPlayer->mInventory : nullptr;
}

InventoryLocation PlayerLAO::GetInventoryLocation() const
{
	InventoryLocation loc;
	loc.SetPlayer(mPlayer->GetName());
	return loc;
}

uint16_t PlayerLAO::GetWieldIndex() const
{
	return mPlayer->GetWieldIndex();
}

ItemStack PlayerLAO::GetWieldedItem(ItemStack* selected, ItemStack* hand) const
{
	return mPlayer->GetWieldedItem(selected, hand);
}

bool PlayerLAO::SetWieldedItem(const ItemStack &item)
{
	InventoryList* mlist = mPlayer->mInventory.GetList(GetWieldList());
	if (mlist) 
    {
		mlist->ChangeItem(mPlayer->GetWieldIndex(), item);
		return true;
	}
	return false;
}

void PlayerLAO::Disconnected()
{
	mId = INVALID_ACTOR_ID;
	MarkForRemoval();
}

void PlayerLAO::UnlinkPlayerSessionAndSave()
{
	LogAssert(mPlayer->GetPlayerLAO() == this, "invalid player");
    mEnvironment->SavePlayer(mPlayer);
	mPlayer->SetPlayerLAO(NULL);
    mEnvironment->RemovePlayer(mPlayer->GetId());
}

std::string PlayerLAO::GetPropertyPacket()
{
	mProp.isVisible = (true);
	return GenerateSetPropertiesCommand(mProp);
}

void PlayerLAO::SetMaxSpeedOverride(const Vector3<float>& vel)
{
	if (mMaxSpeedOverrideTime == 0.0f)
		mMaxSpeedOverride = vel;
	else
		mMaxSpeedOverride += vel;
	if (mPlayer) 
    {
		float accel = std::min(
            mPlayer->mMovementAccelerationDefault,
            mPlayer->mMovementAccelerationAir);
		mMaxSpeedOverrideTime = Length(mMaxSpeedOverride) / accel / BS;
	}
}

bool PlayerLAO::CheckMovementCheat()
{
	if (mIsSingleplayer || IsAttached() || Settings::Get()->GetBool("disable_anticheat")) 
    {
		mLastGoodPosition = mBasePosition;
		return false;
	}

	bool cheated = false;
	/*
		Check player movements

		NOTE: Actually the logic should handle player physics like the
		visual does and compare player's position to what is calculated
		on our side. This is required when eg. players fly due to an
		explosion. Altough a node-based alternative might be possible
		too, and much more lightweight.
	*/

	float overrideMaxH, overrideMaxV;
	if (mMaxSpeedOverrideTime > 0.0f) 
    {
		overrideMaxH = std::max(fabs(mMaxSpeedOverride[0]), fabs(mMaxSpeedOverride[2]));
		overrideMaxV = fabs(mMaxSpeedOverride[1]);
	} 
    else 
    {
		overrideMaxH = overrideMaxV = 0.0f;
	}

	float playerMaxWalk = 0; // horizontal movement
	float playerMaxJump = 0; // vertical upwards movement

	if (mPrivs.count("fast") != 0)
		playerMaxWalk = mPlayer->mMovementSpeedFast; // Fast speed
	else
		playerMaxWalk = mPlayer->mMovementSpeedWalk; // Normal speed
	playerMaxWalk *= mPhysicsOverrideSpeed;
	playerMaxWalk = std::max(playerMaxWalk, overrideMaxH);

	playerMaxJump = mPlayer->mMovementSpeedJump * mPhysicsOverrideJump;
	// FIXME: Bouncy nodes cause practically unbound increase in Y speed,
	//        until this can be verified correctly, tolerate higher jumping speeds
	playerMaxJump *= 2.0;
	playerMaxJump = std::max(playerMaxJump, overrideMaxV);

	// Don't divide by zero!
	if (playerMaxWalk < 0.0001f)
		playerMaxWalk = 0.0001f;
	if (playerMaxJump < 0.0001f)
		playerMaxJump = 0.0001f;

	Vector3<float> diff = (mBasePosition - mLastGoodPosition);
	float dVert = diff[1];
	diff[1] = 0;
	float dHor = Length(diff);
	float requiredTime = dHor / playerMaxWalk;

	// FIXME: Checking downwards movement is not easily possible currently,
	//        the logic could calculate speed differences to examine the gravity
	if (dVert > 0) 
    {
		// In certain cases (water, ladders) walking speed is applied vertically
		float s = std::max(playerMaxJump, playerMaxWalk);
		requiredTime = std::max(requiredTime, dVert / s);
	}

	if (mMovePool.Grab(requiredTime)) 
    {
		mLastGoodPosition = mBasePosition;
	} 
    else 
    {
		const float LAG_POOL_MIN = 5.f;
		float lagPoolMax = mEnvironment->GetMaxLagEstimate() * 2.f;
		lagPoolMax = std::max(lagPoolMax, LAG_POOL_MIN);
		if (mTimeFromLastTeleport > lagPoolMax) 
        {
			LogInformation(std::string(mPlayer->GetName()) + 
                " moved too fast: V=" + std::to_string(dVert) + ", H=" + 
                std::to_string(dHor) + "; resetting position.");
			cheated = true;
		}
		SetBasePosition(mLastGoodPosition);
	}
	return cheated;
}

bool PlayerLAO::GetCollisionBox(BoundingBox<float>* toset) const
{
	//update collision box
	toset->mMinEdge = mProp.collisionBox.mMinEdge * BS;
	toset->mMaxEdge = mProp.collisionBox.mMaxEdge * BS;

	toset->mMinEdge += mBasePosition;
	toset->mMaxEdge += mBasePosition;
	return true;
}

bool PlayerLAO::GetSelectionBox(BoundingBox<float>* toset) const
{
	if (!mProp.isVisible || !mProp.pointable)
		return false;

	toset->mMinEdge = mProp.selectionBox.mMinEdge * BS;
	toset->mMaxEdge = mProp.selectionBox.mMaxEdge * BS;

	return true;
}

float PlayerLAO::GetZoomFOV() const
{
	return mProp.zoomFov;
}
