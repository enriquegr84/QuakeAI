/*
Minetest
Copyright (C) 2010-2016 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2014-2016 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#include "LogicPlayer.h"

#include "../Games.h"

#include "Application/Settings.h"

#include "Game/GameLogic.h"

/*
	LogicPlayer
*/
// static config cache for remoteplayer
bool LogicPlayer::mSettingCacheLoaded = false;
float LogicPlayer::mSettingChatMessageLimitPer10sec = 0.0f;
uint16_t LogicPlayer::mSettingChatMessageLimitTriggerKick = 0;

LogicPlayer::LogicPlayer(const char* name, BaseItemManager* itemMgr) :
	Player(INVALID_ACTOR_ID, name, itemMgr)
{
	if (!LogicPlayer::mSettingCacheLoaded) 
    {
		LogicPlayer::mSettingChatMessageLimitPer10sec =
			Settings::Get()->GetFloat("chat_message_limit_per_10sec");
		LogicPlayer::mSettingChatMessageLimitTriggerKick =
            Settings::Get()->GetUInt16("chat_message_limit_trigger_kick");
		LogicPlayer::mSettingCacheLoaded = true;
	}

    mLastChatMessageSent = (unsigned int)time(0);

    mMovementAccelerationDefault = Settings::Get()->GetFloat("movement_acceleration_default") * BS;
    mMovementAccelerationAir = Settings::Get()->GetFloat("movement_acceleration_air") * BS;
    mMovementAccelerationFast = Settings::Get()->GetFloat("movement_acceleration_fast") * BS;
    mMovementSpeedWalk = Settings::Get()->GetFloat("movement_speed_walk") * BS;
    mMovementSpeedCrouch = Settings::Get()->GetFloat("movement_speed_crouch") * BS;
    mMovementSpeedFast = Settings::Get()->GetFloat("movement_speed_fast") * BS;
    mMovementSpeedClimb = Settings::Get()->GetFloat("movement_speed_climb") * BS;
    mMovementSpeedJump = Settings::Get()->GetFloat("movement_speed_jump") * BS;
    mMovementLiquidFluidity = Settings::Get()->GetFloat("movement_liquid_fluidity") * BS;
    mMovementLiquidFluiditySmooth = Settings::Get()->GetFloat("movement_liquid_fluidity_smooth") * BS;
    mMovementLiquidSink = Settings::Get()->GetFloat("movement_liquid_sink") * BS;
    mMovementGravity = Settings::Get()->GetFloat("movement_gravity") * BS;

	// copy defaults
	mCloudParams.density = 0.4f;
	mCloudParams.colorBright = SColor(229, 240, 240, 255);
	mCloudParams.colorAmbient = SColor(255, 0, 0, 0);
	mCloudParams.height = 120.0f;
	mCloudParams.thickness = 16.0f;
    mCloudParams.speed = Vector2<float>{ 0.0f, -2.0f };

	// Skybox defaults:

	SkyboxDefaults skyDefaults;

	mSkyboxParams.skyColor = skyDefaults.GetSkyColorDefaults();
	mSkyboxParams.type = "regular";
	mSkyboxParams.clouds = true;
	mSkyboxParams.fogSunTint = SColor(255, 244, 125, 29);
	mSkyboxParams.fogMoonTint = SColorF(0.5f, 0.6f, 0.8f).ToSColor();
	mSkyboxParams.fogTintType = "default";

	mSunParams = skyDefaults.GetSunDefaults();
	mMoonParams = skyDefaults.GetMoonDefaults();
	mStarParams = skyDefaults.GetStarDefaults();
}


const LogicPlayerChatResult LogicPlayer::CanSendChatMessage()
{
	// Rate limit messages
	unsigned int now = (unsigned int)time(NULL);
	float timePassed = (float)(now - mLastChatMessageSent);
	mLastChatMessageSent = now;

	// If this feature is disabled
	if (mSettingChatMessageLimitPer10sec <= 0.0)
		return LPLAYER_CHATRESULT_OK;

	mChatMessageAllowance += timePassed * (mSettingChatMessageLimitPer10sec / 8.0f);
	if (mChatMessageAllowance > mSettingChatMessageLimitPer10sec)
		mChatMessageAllowance = mSettingChatMessageLimitPer10sec;

	if (mChatMessageAllowance < 1.0f) 
    {
		LogInformation("Player " + std::string(mName) + 
            " chat limited due to excessive message amount.");

		// Kick player if flooding is too intensive
		mMessageRateOverhead++;
		if (mMessageRateOverhead > LogicPlayer::mSettingChatMessageLimitTriggerKick)
			return LPLAYER_CHATRESULT_KICK;

		return LPLAYER_CHATRESULT_FLOODING;
	}

	// Reinit message overhead
	if (mMessageRateOverhead > 0)
		mMessageRateOverhead = 0;

	mChatMessageAllowance -= 1.0f;
	return LPLAYER_CHATRESULT_OK;
}

void LogicPlayer::OnSuccessfulSave()
{
	SetModified(false);
	if (mLAO)
		mLAO->GetMeta().SetModified(false);
}

void LogicPlayer::SetModel(const std::string& modelName)
{

}
