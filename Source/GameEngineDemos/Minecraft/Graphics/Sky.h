/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef SKY_H
#define SKY_H

#include "Shader.h"

#include "PlayerCamera.h"

#include "../Data/SkyParams.h"

#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"

#include "Core/OS/OS.h"

#define SKY_MATERIAL_COUNT 12

// Skybox, rendered with zbuffer turned off, before all other nodes.
class Sky : public Node
{
public:
	//! constructor
	Sky(int id, BaseTextureSource* textureSrc, BaseShaderSource* shaderSrc);

	//! destructor
	virtual ~Sky() { }

	virtual bool PreRender(Scene* pScene);

	//! renders the node.
	virtual bool Render(Scene* pScene);

	BoundingBox<float>& GetBoundingBox() { return mBBox; }

	// Used by game engine for optimizing rendering
	virtual std::shared_ptr<Material> const& GetMaterial(unsigned int i) { return mMaterials[i]; }
	virtual size_t GetMaterialCount() const { return SKY_MATERIAL_COUNT; }

	void Update(float timeOfDay, float timeBrightness, float directBrightness,
			bool sunlightSeen, PlayerCameraMode camMode, float yaw, float pitch);

	float GetBrightness() { return mBrightness; }

	const SColor& GetBGColor() const { return mVisible ? mBGColor.ToSColor() : mFallbackBGColor; }
	const SColor& GetSkyColor() const { return mVisible ? mSkyColor.ToSColor() : mFallbackBGColor; }

	void SetSunVisible(bool sunVisible) { mSunParams.visible = sunVisible; }
	void SetSunTexture(const std::string& sunTexture,
		const std::string& sunToneMap, BaseTextureSource* textureSrc);
	void SetSunScale(float sunScale) { mSunParams.scale = sunScale; }
	void SetSunriseVisible(bool glowVisible) { mSunParams.sunriseVisible = glowVisible; }
	void SetSunriseTexture(const std::string& sunglowTexture, BaseTextureSource* textureSrc);

	void SetMoonVisible(bool moonVisible) { mMoonParams.visible = moonVisible; }
	void SetMoonTexture(const std::string& moonTexture,
		const std::string& moonToneMap, BaseTextureSource* textureSrc);
	void SetMoonScale(float moonScale) { mMoonParams.scale = moonScale; }

	void SetStarsVisible(bool starsVisible) { mStarParams.visible = starsVisible; }
	void SetStarCount(unsigned int starCount, bool forceUpdate);
	void SetStarColor(SColor starColor) { mStarParams.starcolor = starColor; }
	void SetStarScale(float starScale) { mStarParams.scale = starScale; UpdateStars(); }

	bool GetCloudsVisible() const { return mCloudsVisible && mCloudsEnabled; }
	const SColorF& GetCloudColor() const { return mCloudColor; }

	void SetVisible(bool visible) { mVisible = visible; }
	// Set only from SetSky API
	void SetCloudsEnabled(bool cloudsEnabled) { mCloudsEnabled = cloudsEnabled; }
	void SetFallbackBGColor(SColor fallbackBgColor) { mFallbackBGColor = fallbackBgColor; }
	void OverrideColors(SColor bgcolor, SColor skycolor)
	{
		mBGColor = bgcolor;
		mSkyColor = skycolor;
	}
	void SetSkyColors(const SkyColor& skyColor);
	void SetHorizonTint(SColor sunTint, SColor moonTint, const std::string& useSunTint);
	void SetInClouds(bool clouds) { mInClouds = clouds; }
	void ClearSkyboxTextures() { mSkyParams.textures.clear(); }
	void AddTextureToSkybox(const std::string& texture, int materialId, BaseTextureSource* textureSrc);
	const SColorF& GetCurrentStarColor() const { return mStarColor; }

private:

    PcgRandom mPcgRand;

	ShaderInfo mShader;

	BoundingBox<float> mBBox;
    std::shared_ptr<Material> mMaterials[SKY_MATERIAL_COUNT];

    std::shared_ptr<BlendState> mBlendState;
    std::shared_ptr<DepthStencilState> mDepthStencilState;
    std::shared_ptr<RasterizerState> mRasterizerState;

	// How much sun & moon transition should affect horizon color
	float HorizonBlend()
	{
		if (!mSunlightSeen)
			return 0;
		float x = mTimeOfDay >= 0.5f ? (1 - mTimeOfDay) * 2 : mTimeOfDay * 2;

		if (x <= 0.3f)
			return 0;
		if (x <= 0.4f) // when the sun and moon are aligned
			return (x - 0.3f) * 10;
		if (x <= 0.5f)
			return (0.5f - x) * 10;
		return 0;
	}

	// Mix two colors by a given amount
	static SColor MixColor(SColor col1, SColor col2, float factor)
	{
		SColor result = SColor(
            (uint32_t)(col1.GetAlpha() * (1 - factor) + col2.GetAlpha() * factor),
            (uint32_t)(col1.GetRed() * (1 - factor) + col2.GetRed() * factor),
            (uint32_t)(col1.GetGreen() * (1 - factor) + col2.GetGreen() * factor),
            (uint32_t)(col1.GetBlue() * (1 - factor) + col2.GetBlue() * factor));
		return result;
	}
	static SColorF MixColor(SColorF col1, SColorF col2, float factor)
	{
		SColorF result = SColorF(
            col1.mRed * (1 - factor) + col2.mRed * factor,
            col1.mGreen * (1 - factor) + col2.mGreen * factor,
            col1.mBlue * (1 - factor) + col2.mBlue * factor,
            col1.mAlpha * (1 - factor) + col2.mAlpha * factor);
		return result;
	}

	bool mVisible = true;
	// Used when mVisible=false
	SColor mFallbackBGColor = SColor(255, 255, 255, 255);
	bool mFirstUpdate = true;
	float mTimeOfDay;
	float mTimeBrightness;
	bool mSunlightSeen;
	float mBrightness = 0.5f;
	float mCloudBrightness = 0.5f;
	bool mCloudsVisible; // Whether clouds are disabled due to player underground
	bool mCloudsEnabled = true; // Initialised to true, reset only by SetSky API
	bool mDirectionalColoredFog;
	bool mInClouds = true; // Prevent duplicating bools to remember old values
	bool mEnableShaders = false;

	SColorF mBGColorBright = SColorF(1.0f, 1.0f, 1.0f, 1.0f);
	SColorF mSkycolorBright = SColorF(1.0f, 1.0f, 1.0f, 1.0f);
	SColorF mCloudcolorBright = SColorF(1.0f, 1.0f, 1.0f, 1.0f);
	SColorF mBGColor;
	SColorF mSkyColor;
	SColorF mCloudColor;

	// pure white: becomes "diffuse light component" for clouds
	SColorF mCloudColorDay = SColorF(1, 1, 1, 1);
	// dawn-factoring version of pure white (note: R is above 1.0)
	SColorF mCloudColorDawn = SColorF(
		255.0f/240.0f, 223.0f/240.0f, 191.0f/255.0f );

	SkyboxParams mSkyParams;
	SunParams mSunParams;
	MoonParams mMoonParams;
	StarParams mStarParams;

	bool mDefaultTint = true;

	uint64_t mSeed = 0;
	SColorF mStarColor;
    std::shared_ptr<MeshBuffer> mStars;

	std::shared_ptr<Texture2> mSunTexture;
    std::shared_ptr<Texture2> mMoonTexture;
    std::shared_ptr<Texture2> mSunToneMap;
    std::shared_ptr<Texture2> mMoonToneMap;

	void UpdateStars();

	void DrawSkyBox(Scene* pScene);
	void DrawSunrise(Scene* pScene, float wickedTimeOfDay);
	void DrawCloudyFog(Scene* pScene);
	void DrawCloudyFogBelow(Scene* pScene);
	void DrawSun(Scene* pScene, float sunSize,
        const SColor& sunColor, const SColor& sunColor2, float wickedTimeOfDay);
	void DrawMoon(Scene* pScene, float moonSize,
        const SColor& moonColor, const SColor& moonColor2, float wickedTimeOfDay);
	void DrawStars(Scene* pScene, float wickedTimeOfDay);
	void SetSkyDefaults();
};

#endif