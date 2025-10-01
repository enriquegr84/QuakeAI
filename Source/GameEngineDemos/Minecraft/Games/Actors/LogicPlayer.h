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

#ifndef LOGICPLAYER_H
#define LOGICPLAYER_H

#include "Player.h"
#include "PlayerLAO.h"

#include "VisualComponent.h"

#include "../../Data/SkyParams.h"
#include "../../Data/CloudParams.h"

enum LogicPlayerChatResult
{
	LPLAYER_CHATRESULT_OK,
	LPLAYER_CHATRESULT_FLOODING,
	LPLAYER_CHATRESULT_KICK,
};

/*
	Logic Player
*/
class LogicPlayer : public Player
{
	friend class PlayerDatabaseFiles;

public:
	LogicPlayer(const char* name, BaseItemManager* itemMgr);
	virtual ~LogicPlayer() = default;

	PlayerLAO* GetPlayerLAO() { return mLAO; }
	void SetPlayerLAO(PlayerLAO* playerLAO) { mLAO = playerLAO; }

	const LogicPlayerChatResult CanSendChatMessage();

	//Called when a player's appearance needs to be updated
	void SetModel(const std::string& modelName);

	void SetHotbarItemCount(int hotbarItemCount)
	{
		mHudHotbarItemCount = hotbarItemCount;
	}

	int GetHotbarItemCount() const { return mHudHotbarItemCount; }

	void OverrideDayNightRatio(bool doOverride, float ratio)
	{
		mDayNightRatioDoOverride = doOverride;
		mDayNightRatio = ratio;
	}

	void GetDayNightRatio(bool* doOverride, float* ratio)
	{
		*doOverride = mDayNightRatioDoOverride;
		*ratio = mDayNightRatio;
	}

	void SetHotbarImage(const std::string& name) { mHudHotbarImage = name; }

	const std::string& GetHotbarImage() const { return mHudHotbarImage; }

	void SetHotbarSelectedImage(const std::string& name)
	{
		mHudHotbarSelectedImage = name;
	}

	const std::string& GetHotbarSelectedImage() const
	{
		return mHudHotbarSelectedImage;
	}

	void SetSky(const SkyboxParams& skyboxParams)
	{
		mSkyboxParams = skyboxParams;
	}

	const SkyboxParams& GetSkyParams() const { return mSkyboxParams; }

	void SetSun(const SunParams& sunParams) { mSunParams = sunParams; }

	const SunParams& GetSunParams() const { return mSunParams; }

	void SetMoon(const MoonParams& moonParams) { mMoonParams = moonParams; }

	const MoonParams& GetMoonParams() const { return mMoonParams; }

	void SetStars(const StarParams& starParams) { mStarParams = starParams; }

	const StarParams& GetStarParams() const { return mStarParams; }

	void SetCloudParams(const CloudParams& cloudParams) { mCloudParams = cloudParams;}

	const CloudParams& GetCloudParams() const { return mCloudParams; }

	bool CheckModified() const { return mDirty || mInventory.CheckModified(); }

	inline void SetModified(const bool x) { mDirty = x; }

	void SetLocalAnimations(std::map<std::string, Vector2<short>> frames, float frameSpeed)
	{
		std::shared_ptr<VisualComponent> pMesh =
			this->GetComponent<VisualComponent>(VisualComponent::Name).lock();

		for (auto animationFrames : pMesh->GetAnimationFrames())
			animationFrames.second = frames[animationFrames.first];
		pMesh->SetAnimationSpeed(frameSpeed);
	}

	void GetLocalAnimations(std::map<std::string, Vector2<short>>* frames, float* frameSpeed)
	{
		std::shared_ptr<VisualComponent> pMesh =
			this->GetComponent<VisualComponent>(VisualComponent::Name).lock();

		for (auto animationFrames : pMesh->GetAnimationFrames())
			(*frames)[animationFrames.first] = animationFrames.second;
		*frameSpeed = pMesh->GetAnimationSpeed();
	}

	void SetDirty(bool dirty) { mDirty = true; }

	// v1 for visuals older than 5.1.0-dev
	uint16_t mFormVersion = 1;

	void OnSuccessfulSave();

private:
	PlayerLAO* mLAO = nullptr;
	bool mDirty = false;

	static bool mSettingCacheLoaded;
	static float mSettingChatMessageLimitPer10sec;
	static uint16_t mSettingChatMessageLimitTriggerKick;

	unsigned int mLastChatMessageSent = 0;
	float mChatMessageAllowance = 5.0f;
	unsigned short mMessageRateOverhead = 0;

	bool mDayNightRatioDoOverride = false;
	float mDayNightRatio;
	std::string mHudHotbarImage = "";
    std::string mHudHotbarSelectedImage = "";

	CloudParams mCloudParams;

	SkyboxParams mSkyboxParams;
	SunParams mSunParams;
	MoonParams mMoonParams;
	StarParams mStarParams;
};

#endif