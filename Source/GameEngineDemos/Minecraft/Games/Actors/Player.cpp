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

#include "Player.h"

#include "../Games.h"

#include "../../Data/Huddata.h"

#include "Application/Settings.h"

#include "Core/Threading/MutexAutolock.h"


Player::Player(ActorId id, const char* name, BaseItemManager* itemMgr) :
    Actor(id), mInventory(itemMgr)
{
	Stringlcpy(mName, name, PLAYERNAME_SIZE);

	mInventory.Clear();
    mInventory.AddList("main", PLAYER_INVENTORY_SIZE);
	InventoryList* craft = mInventory.AddList("craft", 9);
	craft->SetWidth(3);
    mInventory.AddList("craftpreview", 1);
    mInventory.AddList("craftresult", 1);
    mInventory.SetModified(false);

	mInventoryForm = "size[8,7.5]"
		//"image[1,0.6;1,2;player.png]"
		"list[current_player;main;0,3.5;8,4;]"
		"list[current_player;craft;3,0;3,3;]"
		"listring[]"
		"list[current_player;craftpreview;7,1;1,1;]";

	// Initialize movement settings at default values
	mMovementAccelerationDefault = 3 * BS;
	mMovementAccelerationAir = 2 * BS;
	mMovementAccelerationFast = 10 * BS;
	mMovementSpeedWalk = 4 * BS;
	mMovementSpeedCrouch = 1.35 * BS;
	mMovementSpeedFast = 20 * BS;
	mMovementSpeedClimb = 2 * BS;
	mMovementSpeedJump = 6.5f * BS;
	mMovementLiquidFluidity = 1 * BS;
	mMovementLiquidFluiditySmooth = 0.5f * BS;
	mMovementLiquidSink = 10 * BS;
	mMovementGravity = 9.81f * BS;

	mHudFlags =
		HUD_FLAG_HOTBAR_VISIBLE    | HUD_FLAG_HEALTHBAR_VISIBLE |
		HUD_FLAG_CROSSHAIR_VISIBLE | HUD_FLAG_WIELDITEM_VISIBLE |
		HUD_FLAG_BREATHBAR_VISIBLE | HUD_FLAG_MINIMAP_VISIBLE   |
		HUD_FLAG_MINIMAP_RADAR_VISIBLE;

	mHudHotbarItemCount = HUD_HOTBAR_ITEMCOUNT_DEFAULT;

	mPlayerSettings.ReadGlobalSettings();
	// Register player setting callbacks
    for (const std::string& name : mPlayerSettings.settingNames)
    {
        Settings::Get()->RegisterChangedCallback(name,
            &Player::SettingsChangedCallback, &mPlayerSettings);
    }
}

Player::~Player()
{
	// mPlayerSettings becomes invalid, remove callbacks
    for (const std::string& name : mPlayerSettings.settingNames)
    {
        Settings::Get()->DeregisterChangedCallback(name,
            &Player::SettingsChangedCallback, &mPlayerSettings);
    }
	ClearHud();
}

void Player::SetWieldIndex(unsigned int index)
{
	const InventoryList* mainList = mInventory.GetList("main");
	mWieldIndex = std::min(index, mainList ? mainList->GetSize() : 0);
}

ItemStack& Player::GetWieldedItem(ItemStack* selected, ItemStack* hand) const
{
	LogAssert(selected, "invalid item");

	const InventoryList* mainList = mInventory.GetList("main"); // TODO: Make this generic
	const InventoryList* handList = mInventory.GetList("hand");

	if (mainList && mWieldIndex < mainList->GetSize())
		*selected = mainList->GetItem(mWieldIndex);

	if (hand && handList)
		*hand = handList->GetItem(0);

	// Return effective tool item
	return (hand && selected->name.empty()) ? *hand : *selected;
}

unsigned int Player::AddHud(HudElement* toadd)
{
	MutexAutoLock lock(mMutex);

	unsigned int id = GetFreeHudID();

	if (id < mHud.size())
        mHud[id] = toadd;
	else
        mHud.push_back(toadd);

	return id;
}

HudElement* Player::GetHud(unsigned int id)
{
	MutexAutoLock lock(mMutex);

	if (id < mHud.size())
		return mHud[id];

	return NULL;
}

HudElement* Player::RemoveHud(unsigned int id)
{
	MutexAutoLock lock(mMutex);

	HudElement* retval = NULL;
	if (id < mHud.size())
    {
		retval = mHud[id];
        mHud[id] = NULL;
	}
	return retval;
}

void Player::ClearHud()
{
	MutexAutoLock lock(mMutex);

	while(!mHud.empty()) 
    {
		delete mHud.back();
        mHud.pop_back();
	}
}

void PlayerSettings::ReadGlobalSettings()
{
	freeMove = Settings::Get()->GetBool("free_move");
	pitchMove = Settings::Get()->GetBool("pitch_move");
	fastMove = Settings::Get()->GetBool("fast_move");
	continuousForward = Settings::Get()->GetBool("continuous_forward");
	alwaysFlyFast = Settings::Get()->GetBool("always_fly_fast");
	aux1Descends = Settings::Get()->GetBool("aux1_descends");
	noClip = Settings::Get()->GetBool("noclip");
	autojump = Settings::Get()->GetBool("autojump");
}

void Player::SettingsChangedCallback(const std::string& name, void* data)
{
	((PlayerSettings*)data)->ReadGlobalSettings();
}
