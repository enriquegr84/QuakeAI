/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2020 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

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

#include "Sky.h"

#include "../Utils/Noise.h"

#include "Core/OS/OS.h"
#include "Core/Utility/Profiler.h"

#include "Application/Settings.h"

#include "Graphic/Scene/Scene.h"
#include "Graphic/Scene/Element/CameraNode.h"
#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"


static std::shared_ptr<Material> BaseMaterial()
{
    std::shared_ptr<Material> mat(new Material());
	mat->mLighting = false;
    mat->mDepthBuffer = false;
    mat->mDepthMask = DepthStencilState::MASK_ZERO;

	mat->mAntiAliasing = 0;
    mat->mTextureLayer[0].mModeU = SamplerState::CLAMP;
	mat->mTextureLayer[0].mModeV = SamplerState::CLAMP;
    mat->mCullMode = RasterizerState::CULL_FRONT;
	return mat;
};

Sky::Sky(int id, BaseTextureSource* textureSrc, BaseShaderSource* shaderSrc) : 
    Node(id, NT_SKY)
{
	mSeed = (uint64_t)mPcgRand.Next() << 32 | mPcgRand.Next();

    SetCullingMode(CULL_NEVER);
    mBBox.mMaxEdge.MakeZero();
	mBBox.mMinEdge.MakeZero();

    mEnableShaders = Settings::Get()->GetBool("enable_shaders");
    
    unsigned int shaderId = shaderSrc->GetShader("Stars", TILE_MATERIAL_ALPHA);
    mShader = shaderSrc->GetShaderInfo(shaderId);

    mBlendState = std::make_shared<BlendState>();
    mDepthStencilState = std::make_shared<DepthStencilState>();
    mRasterizerState = std::make_shared<RasterizerState>();

	// Create materials
	mMaterials[0] = BaseMaterial();
	mMaterials[0]->mType = mShader.material;
	mMaterials[0]->mLighting = true;

	mMaterials[1] = BaseMaterial();
	//mMaterials[1]->mMaterialType = MT_TRANSPARENT_VERTEX_ALPHA;
	mMaterials[1]->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

	mMaterials[2] = BaseMaterial();
	mMaterials[2]->SetTexture(0, textureSrc->GetTextureForMesh("sunrisebg.png"));
	mMaterials[2]->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
	//mMaterials[2].MaterialType = video::EMT_TRANSPARENT_ADD_COLOR;

	// Ensures that sun and moon textures and toneMaps are correct.
	SetSkyDefaults();
	mSunTexture = textureSrc->IsKnownSourceImage(mSunParams.texture) ?
        textureSrc->GetTextureForMesh(mSunParams.texture) : nullptr;
	mMoonTexture = textureSrc->IsKnownSourceImage(mMoonParams.texture) ?
        textureSrc->GetTextureForMesh(mMoonParams.texture) : nullptr;
	mSunToneMap = textureSrc->IsKnownSourceImage(mSunParams.toneMap) ?
        textureSrc->GetTexture(mSunParams.toneMap) : nullptr;
	mMoonToneMap = textureSrc->IsKnownSourceImage(mMoonParams.toneMap) ?
        textureSrc->GetTexture(mMoonParams.toneMap) : nullptr;

	if (mSunTexture) 
    {
		mMaterials[3] = BaseMaterial();
		mMaterials[3]->SetTexture(0, mSunTexture);
		mMaterials[3]->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
		// Disables texture filtering
        //mMaterials[3]->mTextureLayer[0].mFilter

		// Use toneMaps if available
		if (mSunToneMap)
			mMaterials[3]->mLighting = true;
	}
	if (mMoonTexture) 
    {
		mMaterials[4] = BaseMaterial();
		mMaterials[4]->SetTexture(0, mMoonTexture);
		mMaterials[4]->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
        // Disables texture filtering
        //mMaterials[4]->mTextureLayer[0].mFilter

		// Use toneMaps if available
		if (mMoonToneMap)
			mMaterials[4]->mLighting = true;
	}

	for (int i = 5; i < 11; i++) 
    {
		mMaterials[i] = BaseMaterial();
		mMaterials[i]->mLighting = true;
		mMaterials[i]->mType = MT_SOLID;
	}

    for (int i = 0; i < 11; i++)
    {
        if (mMaterials[i] && mMaterials[i]->IsTransparent())
        {
            mMaterials[i]->mBlendTarget.enable = true;
            mMaterials[i]->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
            mMaterials[i]->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
            mMaterials[i]->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
            mMaterials[i]->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

            mMaterials[i]->mDepthBuffer = true;
            mMaterials[i]->mDepthMask = DepthStencilState::MASK_ZERO;

            mMaterials[i]->mFillMode = RasterizerState::FILL_SOLID;
            mMaterials[i]->mCullMode = RasterizerState::CULL_NONE;
        }
    }

	mDirectionalColoredFog = Settings::Get()->GetBool("directional_colored_fog");
	SetStarCount(1000, true);
}

bool Sky::PreRender(Scene* pScene)
{
	if (IsVisible())
		pScene->AddToRenderQueue(RP_SKY, shared_from_this());

	return Node::PreRender(pScene);
}

bool Sky::Render(Scene* pScene)
{
	std::shared_ptr<CameraNode> camera = pScene->GetActiveCamera();

	if (!camera)
		return false;

	ScopeProfiler sp(Profiling, "Sky::Render()", SPT_AVG);

    // Draw the sky box between the near and far clip plane
    const float viewDistance = (camera->Get()->GetDMin() + camera->Get()->GetDMax()) * 0.05f;

    mWorldTransform.SetTranslation(camera->GetAbsoluteTransform().GetTranslation());
    mWorldTransform.SetUniformScale(viewDistance);

    if (mSunlightSeen)
    {
        float sunSize = 0.07f;
        SColorF sunColor(1, 1, 0, 1);
        //sunColor.r = 1;
        //sunColor.g = std::max(0.3, std::min(1.0, 0.7 + mTimeBrightness * 0.5));
        //sunColor.b = std::max(0.0, mBrightness * 0.95);
        SColorF sunColor2(1, 1, 1, 1);
        // The values below were probably meant to be sunColor2 instead of a
        // reassignment of sunColor. However, the resulting colour was chosen
        // and is our long-running classic colour. So preserve, but comment-out
        // the unnecessary first assignments above.
        sunColor.mRed = 1;
        sunColor.mGreen = std::max(0.3f, std::min(1.f, 0.85f + mTimeBrightness * 0.5f));
        sunColor.mBlue = std::max(0.f, mBrightness);

        float moonSize = 0.04f;
        SColorF moonColor(0.50f, 0.57f, 0.65f, 1.f);
        SColorF moonColor2(0.85f, 0.875f, 0.9f, 1.f);

        float nightlength = 0.415f;
        float wn = nightlength / 2;
        float wickedTimeOfDay = 0;
        if (mTimeOfDay > wn && mTimeOfDay < 1.0 - wn)
            wickedTimeOfDay = (mTimeOfDay - wn) / (1.f - wn * 2) * 0.5f + 0.25f;
        else if (mTimeOfDay < 0.5f)
            wickedTimeOfDay = mTimeOfDay / wn * 0.25f;
        else
            wickedTimeOfDay = 1.f - ((1.f - mTimeOfDay) / wn * 0.25f);
        /*std::cerr<<"timeOfDay="<<mTimeOfDay<<" -> "
                <<"wickedTimeOfDay="<<wickedTimeOfDay<<std::endl;*/

                // Calculate offset normalized to the X dimension of a 512x1 px toneMap
        float offset = (1.f - (float)fabs(sin((mTimeOfDay - 0.5f) * (float)GE_C_PI))) * 511;

        if (mSunToneMap)
        {
            char* texels = (char*)mSunToneMap->GetData();
            SColor* texel = (SColor*)(texels + (unsigned int)offset * 4);
            SColor texelColor(255, texel->GetRed(), texel->GetGreen(), texel->GetBlue());

            mMaterials[3]->mEmissive = SColorF(texelColor).ToArray();
        }

        if (mMoonToneMap)
        {
            char* texels = (char*)mMoonToneMap->GetData();
            SColor* texel = (SColor*)(texels + (unsigned int)offset * 4);
            SColor texelColor(255, texel->GetRed(), texel->GetGreen(), texel->GetBlue());

            mMaterials[4]->mEmissive = SColorF(texelColor).ToArray();
        }

        // Abort rendering if we're in the clouds.
        // Stops rendering a pure white hole in the bottom of the skybox.
        if (mInClouds)
            return false;

        // Draw the six sided skybox,
        if (mSkyParams.textures.size() == 6)
            DrawSkyBox(pScene);

        // Draw far cloudy fog thing blended with skycolor
        if (mVisible)
            DrawCloudyFog(pScene);

		// Draw stars before sun and moon to be behind them
		if (mStarParams.visible)
			DrawStars(pScene, wickedTimeOfDay);

		// Draw sunrise/sunset horizon glow texture
		// (textures/base/pack/sunrisebg.png)
		if (mSunParams.sunriseVisible) 
            DrawSunrise(pScene, wickedTimeOfDay);

		// Draw sun
		if (mSunParams.visible)
			DrawSun(pScene, sunSize, sunColor.ToSColor(), sunColor2.ToSColor(), wickedTimeOfDay);

		// Draw moon
		if (mMoonParams.visible)
			DrawMoon(pScene, moonSize, moonColor.ToSColor(), moonColor2.ToSColor(), wickedTimeOfDay);

		// Draw far cloudy fog thing below all horizons in front of sun, moon
		// and stars.
        if (mVisible)
            DrawCloudyFogBelow(pScene);
	}

    return true;
}

void Sky::Update(float timeOfDay, float timeBrightness, float directBrightness, 
    bool sunlightSeen, PlayerCameraMode camMode, float yaw, float pitch)
{
	// Stabilize initial brightness and color values by flooding updates
	if (mFirstUpdate) 
    {
		/*dstream<<"First update with timeOfDay="<<timeOfDay
				<<" timeBrightness="<<timeBrightness
				<<" directBrightness="<<directBrightness
				<<" sunlightSeen="<<sunlightSeen<<std::endl;*/
		mFirstUpdate = false;
		for (unsigned int i = 0; i < 100; i++) 
        {
			Update(timeOfDay, timeBrightness, directBrightness,
					sunlightSeen, camMode, yaw, pitch);
		}
		return;
	}

	mTimeOfDay = timeOfDay;
	mTimeBrightness = timeBrightness;
	mSunlightSeen = sunlightSeen;
	mInClouds = false;

	bool isDawn = (timeBrightness >= 0.20 && timeBrightness < 0.35);

	/*
	Development colours

	SColorF bgColorBrightNormal(170. / 255, 200. / 255, 230. / 255, 1.0);
	SColorF bgColorBrightDawn(0.666, 200. / 255 * 0.7, 230. / 255 * 0.5, 1.0);
	SColorF bgColorBrightDawn(0.666, 0.549, 0.220, 1.0);
	SColorF bgColorBrightDawn(0.666 * 1.2, 0.549 * 1.0, 0.220 * 1.0, 1.0);
	SColorF bgColorBrightDawn(0.666 * 1.2, 0.549 * 1.0, 0.220 * 1.2, 1.0);

	SColorF cloudColorBrightDawn(1.0, 0.591, 0.4);
	SColorF cloudColorBrightDawn(1.0, 0.65, 0.44);
	SColorF cloudColorBrightDawn(1.0, 0.7, 0.5);
	*/

	SColorF bgColorBrightNormal = mSkyParams.skyColor.dayHorizon;
	SColorF bgColorBrightIndoor = mSkyParams.skyColor.indoors;
	SColorF bgColorBrightDawn = mSkyParams.skyColor.dawnHorizon;
	SColorF bgColorBrightNight = mSkyParams.skyColor.nightHorizon;

	SColorF skyColorBrightNormal = mSkyParams.skyColor.daySky;
	SColorF skyColorBrightDawn = mSkyParams.skyColor.dawnSky;
	SColorF skyColorBrightNight = mSkyParams.skyColor.nightSky;

	SColorF cloudColorBrightNormal = mCloudColorDay;
	SColorF cloudColorBrightDawn = mCloudColorDawn;

	float cloudColorChangeFraction = 0.95f;
	if (sunlightSeen) 
    {
		if (std::fabs(timeBrightness - mBrightness) < 0.2f) 
        {
			mBrightness = mBrightness * 0.95f + timeBrightness * 0.05f;
		} 
        else 
        {
			mBrightness = mBrightness * 0.8f + timeBrightness * 0.2f;
			cloudColorChangeFraction = 0.f;
		}
	}
    else 
    {
		if (directBrightness < mBrightness)
			mBrightness = mBrightness * 0.95f + directBrightness * 0.05f;
		else
			mBrightness = mBrightness * 0.98f + directBrightness * 0.02f;
	}

	mCloudsVisible = true;
	float colorChangeFraction = 0.98f;
	if (sunlightSeen) 
    {
		if (isDawn) 
        { // Dawn
			mBGColorBright = mBGColorBright.GetInterpolated(
				bgColorBrightDawn, colorChangeFraction);
			mSkycolorBright = mSkycolorBright.GetInterpolated(
				skyColorBrightDawn, colorChangeFraction);
			mCloudcolorBright = mCloudcolorBright.GetInterpolated(
				cloudColorBrightDawn, colorChangeFraction);
		} 
        else 
        {
			if (timeBrightness < 0.13f) 
            { // Night
				mBGColorBright = mBGColorBright.GetInterpolated(
					bgColorBrightNight, colorChangeFraction);
				mSkycolorBright = mSkycolorBright.GetInterpolated(
					skyColorBrightNight, colorChangeFraction);
			} 
            else 
            { // Day
				mBGColorBright = mBGColorBright.GetInterpolated(
					bgColorBrightNormal, colorChangeFraction);
				mSkycolorBright = mSkycolorBright.GetInterpolated(
					skyColorBrightNormal, colorChangeFraction);
			}

			mCloudcolorBright = mCloudcolorBright.GetInterpolated(
				cloudColorBrightNormal, colorChangeFraction);
		}
	} 
    else
    {
		mBGColorBright = mBGColorBright.GetInterpolated(
			bgColorBrightIndoor, colorChangeFraction);
		mSkycolorBright = mSkycolorBright.GetInterpolated(
			bgColorBrightIndoor, colorChangeFraction);
		mCloudcolorBright = mCloudcolorBright.GetInterpolated(
			cloudColorBrightNormal, colorChangeFraction);
		mCloudsVisible = false;
	}

	SColor bgColorBright = mBGColorBright.ToSColor();
	mBGColor = SColor(255,
		(uint32_t)(bgColorBright.GetRed() * mBrightness),
        (uint32_t)(bgColorBright.GetGreen() * mBrightness),
        (uint32_t)(bgColorBright.GetBlue() * mBrightness));

	SColor skyColorBright = mSkycolorBright.ToSColor();
	mSkyColor = SColor(255,
        (uint32_t)(skyColorBright.GetRed() * mBrightness),
        (uint32_t)(skyColorBright.GetGreen() * mBrightness),
        (uint32_t)(skyColorBright.GetBlue() * mBrightness));

	// Horizon coloring based on sun and moon direction during sunset and sunrise
	SColorF pointColor = SColorF(1.f, 1.f, 1.f, mBGColor.GetAlpha());
	if (mDirectionalColoredFog) 
    {
		if (HorizonBlend() != 0) 
        {
			// Calculate hemisphere value from yaw, (inverted in third person front view)
			int8_t dirFactor = 1;
			if (camMode > CAMERA_MODE_THIRD)
				dirFactor = -1;
			float pointColorBlend = WrapDegrees360(yaw * dirFactor + 90);
			if (pointColorBlend > 180)
				pointColorBlend = 360 - pointColorBlend;
			pointColorBlend /= 180;
			// Bound view angle to determine where transition starts and ends
			pointColorBlend = std::clamp(1 - pointColorBlend * 1.375f, 0.f, 1 / 1.375f) * 1.375f;
			// Combine the colors when looking up or down, otherwise turning looks weird
			pointColorBlend += (0.5f - pointColorBlend) *
				(1 - std::min((90 - std::fabs(pitch)) / 90.f * 1.5f, 1.f));
			// Invert direction to match where the sun and moon are rising
			if (mTimeOfDay > 0.5)
				pointColorBlend = 1 - pointColorBlend;
			// Horizon colors of sun and moon
			float pointColorLight = std::clamp(mTimeBrightness * 3, 0.2f, 1.f);

			SColorF pointColorSun(1, 1, 1, 1);
			// Use toneMap only if default sun/moon tinting is used
			// which keeps previous behaviour.
			if (mSunToneMap && mDefaultTint)
            {
				pointColorSun.mRed = pointColorLight *
					(float)mMaterials[3]->mEmissive[0] / 255;
				pointColorSun.mBlue = pointColorLight *
					(float)mMaterials[3]->mEmissive[2] / 255;
				pointColorSun.mGreen = pointColorLight *
					(float)mMaterials[3]->mEmissive[1] / 255;
			}
            else if (!mDefaultTint) 
            {
				pointColorSun = mSkyParams.fogSunTint;
			}
            else
            {
				pointColorSun.mRed = pointColorLight * 1;
				pointColorSun.mBlue = pointColorLight *
					(0.25f + (std::clamp(mTimeBrightness, 0.25f, 0.75f) - 0.25f) * 2 * 0.75f);
				pointColorSun.mGreen = pointColorLight * (pointColorSun.mBlue * 0.375f +
					(std::clamp(mTimeBrightness, 0.05f, 0.15f) - 0.05f) * 10 * 0.625f);
			}

			SColorF pointColorMoon;
			if (mDefaultTint) 
            {
				pointColorMoon = SColorF(
					0.5f * pointColorLight,
					0.6f * pointColorLight,
					0.8f * pointColorLight, 1);
			} 
            else
            {
				pointColorMoon = SColorF(
					(mSkyParams.fogMoonTint.GetRed() / 255) * pointColorLight,
					(mSkyParams.fogMoonTint.GetGreen() / 255) * pointColorLight,
					(mSkyParams.fogMoonTint.GetBlue() / 255) * pointColorLight, 1);
			}
			if (mMoonToneMap && mDefaultTint) 
            {
				pointColorMoon.mRed = pointColorLight *
					(float)mMaterials[4]->mEmissive[0] / 255;
				pointColorMoon.mBlue = pointColorLight *
					(float)mMaterials[4]->mEmissive[2] / 255;
				pointColorMoon.mGreen = pointColorLight *
					(float)mMaterials[4]->mEmissive[1] / 255;
			}

			// Calculate the blend color
			pointColor = MixColor(pointColorMoon, pointColorSun, pointColorBlend);
		}
		mBGColor = MixColor(mBGColor, pointColor, HorizonBlend() * 0.5f);
		mSkyColor = MixColor(mSkyColor, pointColor, HorizonBlend() * 0.25f);
	}

	float cloudDirectBrightness = 0.0f;
	if (sunlightSeen) 
    {
		if (!mDirectionalColoredFog) 
        {
			cloudDirectBrightness = timeBrightness;
			// Boost cloud brightness relative to sky, at dawn, dusk and at night
			if (timeBrightness < 0.7f)
				cloudDirectBrightness *= 1.3f;
		} 
        else 
        {
			cloudDirectBrightness = std::fmin(HorizonBlend() * 0.15f + mTimeBrightness, 1.0f);
			// Set the same minimum cloud brightness at night
			if (timeBrightness < 0.5f)
				cloudDirectBrightness = std::fmax(cloudDirectBrightness, timeBrightness * 1.3f);
		}
	} 
    else 
    {
		cloudDirectBrightness = directBrightness;
	}

	mCloudBrightness = mCloudBrightness * cloudColorChangeFraction +
		cloudDirectBrightness * (1.f - cloudColorChangeFraction);
	mCloudColor = SColorF(
		mCloudcolorBright.mRed * mCloudBrightness,
		mCloudcolorBright.mGreen * mCloudBrightness,
		mCloudcolorBright.mBlue * mCloudBrightness, 1.f );
	if (mDirectionalColoredFog) 
		mCloudColor = MixColor(mCloudColor, SColorF(pointColor), HorizonBlend() * 0.25f);
}

/* 
 * Draw sun in the sky.
 * driver: Video driver object used to draw
 * sunSize: the default size of the sun
 * sunColor: main sun color
 * sunColor2: second sun color
 * wickedTimeOfDay: current time of day, to know where should be the sun in the sky
 */
void Sky::DrawSun(Scene* pScene, float sunSize,
    const SColor& sunColor, const SColor& sunColor2, float wickedTimeOfDay)
{
	if (!mSunTexture) 
    {
        struct Vertex
        {
            Vector3<float> position;
            Vector4<float> color;
        };
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        // fill vertices
        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

		const float sunSizes[4] = 
        {
			(sunSize * 1.7f) * mSunParams.scale,
			(sunSize * 1.2f) * mSunParams.scale,
			(sunSize) * mSunParams.scale,
			(sunSize * 0.7f) * mSunParams.scale
		};
		SColor c1 = sunColor;
		SColor c2 = sunColor;
		c1.SetAlpha((uint32_t)(0.05f * 255));
		c2.SetAlpha((uint32_t)(0.15f * 255));
		const SColorF colors[4] = {c1, c2, sunColor, sunColor2};
		for (int i = 0; i < 4; i++) 
        {
            /*
                Create an array of vertices with the dimensions specified.
                position of the body's vertices
                c: color of the body
            */
            vertices = vertexBuffer->Get<Vertex>();
            vertices[0].position = Vector3<float>{ -sunSizes[i], -sunSizes[i], -1 };
            vertices[1].position = Vector3<float>{ sunSizes[i], -sunSizes[i], -1 };
            vertices[2].position = Vector3<float>{ sunSizes[i], sunSizes[i], -1 };
            vertices[3].position = Vector3<float>{ -sunSizes[i], sunSizes[i], -1 };

            vertices[0].color = colors[i].ToArray();
            vertices[1].color = colors[i].ToArray();
            vertices[2].color = colors[i].ToArray();
            vertices[3].color = colors[i].ToArray();

            /*
                Place body in the sky.
                vertices: The body as a rectangle of 4 vertices
                horizonPosition: turn the body around the Y axis
                dayPosition: turn the body around the Z axis, to place it depending of the time of the day
            */
            float horizonPosition = 90;
            float dayPosition = wickedTimeOfDay * 360 - 90;
            for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
            {
                // Body is directed to -Z (south) by default
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), horizonPosition * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(horizonPosition);

                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -dayPosition * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXYBy(dayPosition);
            }

            std::vector<std::string> path;
#if defined(_OPENGL_)
            path.push_back("Effects/VertexColorEffectVS.glsl");
            path.push_back("Effects/VertexColorEffectPS.glsl");
#else
            path.push_back("Effects/VertexColorEffectVS.hlsl");
            path.push_back("Effects/VertexColorEffectPS.hlsl");
#endif
            std::shared_ptr<ResHandle> resHandle =
                ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

            const std::shared_ptr<ShaderResourceExtraData>& extra =
                std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
            if (!extra->GetProgram())
                extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

            std::shared_ptr<ColorEffect> effect = std::make_shared<ColorEffect>(
                ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
            std::shared_ptr<Visual> visual =
                std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
            visual->UpdateModelBound();

            if (mMaterials[1]->Update(mBlendState))
                Renderer::Get()->Unbind(mBlendState);
            if (mMaterials[1]->Update(mDepthStencilState))
                Renderer::Get()->Unbind(mDepthStencilState);
            if (mMaterials[1]->Update(mRasterizerState))
                Renderer::Get()->Unbind(mRasterizerState);

            Renderer::Get()->SetBlendState(mBlendState);
            Renderer::Get()->SetDepthStencilState(mDepthStencilState);
            Renderer::Get()->SetRasterizerState(mRasterizerState);

            std::shared_ptr<ConstantBuffer> cbuffer;
            cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");

            Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
            Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
            Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
            *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

            Renderer* renderer = Renderer::Get();
            renderer->Update(cbuffer);
            renderer->Update(vertexBuffer);
            renderer->Draw(visual);

            Renderer::Get()->SetDefaultBlendState();
            Renderer::Get()->SetDefaultDepthStencilState();
            Renderer::Get()->SetDefaultRasterizerState();
		}
	} 
    else 
    {
        struct Vertex
        {
            Vector3<float> position;
            Vector2<float> tcoord;
            Vector4<float> color;
        };
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        // fill vertices
        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

		float d = (sunSize * 1.7f) * mSunParams.scale;
		SColorF c;
		if (mSunToneMap)
			c = SColor(0, 0, 0, 0);
		else
			c = SColor(255, 255, 255, 255);

        /*
            Create an array of vertices with the dimensions specified.
            position of the body's vertices
            c: color of the body
        */
        vertices = vertexBuffer->Get<Vertex>();
        vertices[0].position = Vector3<float>{ -d, -d, -1 };
        vertices[1].position = Vector3<float>{ d, -d, -1 };
        vertices[2].position = Vector3<float>{ d, d, -1 };
        vertices[3].position = Vector3<float>{ -d, d, -1 };

        vertices[0].tcoord = Vector2<float>{ 0.0f, 0.0f };
        vertices[1].tcoord = Vector2<float>{ 0.0f, 1.0f };
        vertices[2].tcoord = Vector2<float>{ 1.0f, 1.0f };
        vertices[3].tcoord = Vector2<float>{ 1.0f, 0.0f };

        vertices[0].color = c.ToArray();
        vertices[1].color = c.ToArray();
        vertices[2].color = c.ToArray();
        vertices[3].color = c.ToArray();

        /*
            Place body in the sky.
            vertices: The body as a rectangle of 4 vertices
            horizonPosition: turn the body around the Y axis
            dayPosition: turn the body around the Z axis, to place it depending of the time of the day
        */
        float horizonPosition = 90;
        float dayPosition = wickedTimeOfDay * 360 - 90;
        for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
        {
            // Body is directed to -Z (south) by default
            Quaternion<float> tgt = Rotation<3, float>(
                AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), horizonPosition * (float)GE_C_DEG_TO_RAD));
            vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
            //vertices[i].position.rotateXZBy(horizonPosition);

            tgt = Rotation<3, float>(
                AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -dayPosition * (float)GE_C_DEG_TO_RAD));
            vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
            //vertices[i].position.rotateXYBy(dayPosition);
        }

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2EffectVS.glsl");
        path.push_back("Effects/Texture2EffectPS.glsl");
#else
        path.push_back("Effects/Texture2EffectVS.hlsl");
        path.push_back("Effects/Texture2EffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
            mMaterials[3]->GetTexture(TT_DIFFUSE),
            mMaterials[3]->mTextureLayer[TT_DIFFUSE].mFilter,
            mMaterials[3]->mTextureLayer[TT_DIFFUSE].mModeU,
            mMaterials[3]->mTextureLayer[TT_DIFFUSE].mModeV);
        std::shared_ptr<Visual> visual =
            std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
        visual->UpdateModelBound();

        if (mMaterials[3]->Update(mBlendState))
            Renderer::Get()->Unbind(mBlendState);
        if (mMaterials[3]->Update(mDepthStencilState))
            Renderer::Get()->Unbind(mDepthStencilState);
        if (mMaterials[3]->Update(mRasterizerState))
            Renderer::Get()->Unbind(mRasterizerState);

        Renderer::Get()->SetBlendState(mBlendState);
        Renderer::Get()->SetDepthStencilState(mDepthStencilState);
        Renderer::Get()->SetRasterizerState(mRasterizerState);

        std::shared_ptr<ConstantBuffer> cbuffer;
        cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
        Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
        Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
        Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
        *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

        Renderer* renderer = Renderer::Get();
        renderer->Update(cbuffer);
        renderer->Update(vertexBuffer);
        renderer->Draw(visual);

        Renderer::Get()->SetDefaultBlendState();
        Renderer::Get()->SetDefaultDepthStencilState();
        Renderer::Get()->SetDefaultRasterizerState();
	}
}

/*
 * Draw moon in the sky.
 * driver: Video driver object used to draw
 * moonSize: the default size of the moon
 * moonColor: main moon color
 * moonColor2: second moon color
 * wickedTimeOfDay: current time of day, to know where should be the moon in the sky
*/
void Sky::DrawMoon(Scene* pScene, float moonSize,
    const SColor& moonColor, const SColor& moonColor2, float wickedTimeOfDay)
{
	if (!mMoonTexture) 
    {
        struct Vertex
        {
            Vector3<float> position;
            Vector4<float> color;
        };
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        // fill vertices
        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

		const float moonSizes1[4] = {
			(-moonSize * 1.9f) * mMoonParams.scale,
			(-moonSize * 1.3f) * mMoonParams.scale,
			(-moonSize) * mMoonParams.scale,
			(-moonSize) * mMoonParams.scale};
		const float moonSizes2[4] = {
			(moonSize * 1.9f) * mMoonParams.scale,
			(moonSize * 1.3f) * mMoonParams.scale,
			(moonSize) *mMoonParams.scale,
			(moonSize * 0.6f) * mMoonParams.scale};
		SColor c1 = moonColor;
		SColor c2 = moonColor;
		c1.SetAlpha((uint32_t)(0.05f * 255));
		c2.SetAlpha((uint32_t)(0.15f * 255));
		const SColorF colors[4] = {c1, c2, moonColor, moonColor2};
		for (int i = 0; i < 4; i++) 
        {
            /*
                Create an array of vertices with the dimensions specified.
                position of the body's vertices
                c: color of the body
            */
            vertices = vertexBuffer->Get<Vertex>();
            vertices[0].position = Vector3<float>{ moonSizes1[i], moonSizes1[i], -1 };
            vertices[1].position = Vector3<float>{ moonSizes2[i], moonSizes1[i], -1 };
            vertices[2].position = Vector3<float>{ moonSizes2[i], moonSizes2[i], -1 };
            vertices[3].position = Vector3<float>{ moonSizes1[i], moonSizes2[i], -1 };

            vertices[0].color = colors[i].ToArray();
            vertices[1].color = colors[i].ToArray();
            vertices[2].color = colors[i].ToArray();
            vertices[3].color = colors[i].ToArray();

            /*
                Place body in the sky.
                vertices: The body as a rectangle of 4 vertices
                horizonPosition: turn the body around the Y axis
                dayPosition: turn the body around the Z axis, to place it depending of the time of the day
            */
            float horizonPosition = -90;
            float dayPosition = wickedTimeOfDay * 360 - 90;
            for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
            {
                // Body is directed to -Z (south) by default
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), horizonPosition * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(horizonPosition);

                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -dayPosition * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXYBy(dayPosition);
            }
            std::vector<std::string> path;
#if defined(_OPENGL_)
            path.push_back("Effects/VertexColorEffectVS.glsl");
            path.push_back("Effects/VertexColorEffectPS.glsl");
#else
            path.push_back("Effects/VertexColorEffectVS.hlsl");
            path.push_back("Effects/VertexColorEffectPS.hlsl");
#endif
            std::shared_ptr<ResHandle> resHandle =
                ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

            const std::shared_ptr<ShaderResourceExtraData>& extra =
                std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
            if (!extra->GetProgram())
                extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

            std::shared_ptr<ColorEffect> effect = std::make_shared<ColorEffect>(
                ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
            std::shared_ptr<Visual> visual =
                std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
            visual->UpdateModelBound();

            if (mMaterials[1]->Update(mBlendState))
                Renderer::Get()->Unbind(mBlendState);
            if (mMaterials[1]->Update(mDepthStencilState))
                Renderer::Get()->Unbind(mDepthStencilState);
            if (mMaterials[1]->Update(mRasterizerState))
                Renderer::Get()->Unbind(mRasterizerState);

            Renderer::Get()->SetBlendState(mBlendState);
            Renderer::Get()->SetDepthStencilState(mDepthStencilState);
            Renderer::Get()->SetRasterizerState(mRasterizerState);

            std::shared_ptr<ConstantBuffer> cbuffer;
            cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
            Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
            Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
            Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
            *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

            Renderer* renderer = Renderer::Get();
            renderer->Update(cbuffer);
            renderer->Update(vertexBuffer);
            renderer->Draw(visual);

            Renderer::Get()->SetDefaultBlendState();
            Renderer::Get()->SetDefaultDepthStencilState();
            Renderer::Get()->SetDefaultRasterizerState();
		}
	} 
    else 
    {
        struct Vertex
        {
            Vector3<float> position;
            Vector2<float> tcoord;
            Vector4<float> color;
        };
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        // fill vertices
        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

		float d = (moonSize * 1.9f) * mMoonParams.scale;
		SColorF c;
		if (mMoonToneMap)
			c = SColor(0, 0, 0, 0);
		else
			c = SColor(255, 255, 255, 255);

        /*
            Create an array of vertices with the dimensions specified.
            position of the body's vertices
            c: color of the body
        */
        vertices = vertexBuffer->Get<Vertex>();
        vertices[0].position = Vector3<float>{ -d, -d, -1 };
        vertices[1].position = Vector3<float>{ d, -d, -1 };
        vertices[2].position = Vector3<float>{ d, d, -1 };
        vertices[3].position = Vector3<float>{ -d, d, -1 };

        vertices[0].tcoord = Vector2<float>{ 0.0f, 0.0f };
        vertices[1].tcoord = Vector2<float>{ 0.0f, 1.0f };
        vertices[2].tcoord = Vector2<float>{ 1.0f, 1.0f };
        vertices[3].tcoord = Vector2<float>{ 1.0f, 0.0f };

        vertices[0].color = c.ToArray();
        vertices[1].color = c.ToArray();
        vertices[2].color = c.ToArray();
        vertices[3].color = c.ToArray();

        /*
            Place body in the sky.
            vertices: The body as a rectangle of 4 vertices
            horizonPosition: turn the body around the Y axis
            dayPosition: turn the body around the Z axis, to place it depending of the time of the day
        */
        float horizonPosition = -90;
        float dayPosition = wickedTimeOfDay * 360 - 90;
        for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
        {
            // Body is directed to -Z (south) by default
            Quaternion<float> tgt = Rotation<3, float>(
                AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), horizonPosition * (float)GE_C_DEG_TO_RAD));
            vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
            //vertices[i].position.rotateXZBy(horizonPosition);

            tgt = Rotation<3, float>(
                AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -dayPosition * (float)GE_C_DEG_TO_RAD));
            vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
            //vertices[i].position.rotateXYBy(dayPosition);
        }

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2EffectVS.glsl");
        path.push_back("Effects/Texture2EffectPS.glsl");
#else
        path.push_back("Effects/Texture2EffectVS.hlsl");
        path.push_back("Effects/Texture2EffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
            mMaterials[4]->GetTexture(TT_DIFFUSE),
            mMaterials[4]->mTextureLayer[TT_DIFFUSE].mFilter,
            mMaterials[4]->mTextureLayer[TT_DIFFUSE].mModeU,
            mMaterials[4]->mTextureLayer[TT_DIFFUSE].mModeV);
        std::shared_ptr<Visual> visual =
            std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
        visual->UpdateModelBound();

        if (mMaterials[4]->Update(mBlendState))
            Renderer::Get()->Unbind(mBlendState);
        if (mMaterials[4]->Update(mDepthStencilState))
            Renderer::Get()->Unbind(mDepthStencilState);
        if (mMaterials[4]->Update(mRasterizerState))
            Renderer::Get()->Unbind(mRasterizerState);

        Renderer::Get()->SetBlendState(mBlendState);
        Renderer::Get()->SetDepthStencilState(mDepthStencilState);
        Renderer::Get()->SetRasterizerState(mRasterizerState);

        std::shared_ptr<ConstantBuffer> cbuffer;
        cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
        Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
        Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
        Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
        *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

        Renderer* renderer = Renderer::Get();
        renderer->Update(cbuffer);
        renderer->Update(vertexBuffer);
        renderer->Draw(visual);

        Renderer::Get()->SetDefaultBlendState();
        Renderer::Get()->SetDefaultDepthStencilState();
        Renderer::Get()->SetDefaultRasterizerState();
	}
}

void Sky::DrawStars(Scene* pScene, float wickedTimeOfDay)
{
	// Tune values so that stars first appear just after the sun
	// disappears over the horizon, and disappear just before the sun
	// appears over the horizon.
	// Also tune so that stars are at full brightness from time 20000
	// to time 4000.

	float tod = wickedTimeOfDay < 0.5f ? wickedTimeOfDay : (1.0f - wickedTimeOfDay);
	float starbrightness = (0.25f - fabsf(tod)) * 20.0f;
	mStarColor = mStarParams.starcolor;
	mStarColor.mAlpha *= std::clamp(starbrightness, 0.0f, 1.0f);
	if (mStarColor.mAlpha <= 0.0f) // Stars are only drawn when not fully transparent
		return;
	mMaterials[0]->mDiffuse = mMaterials[0]->mEmissive = mStarColor.ToArray();

    std::shared_ptr<StarsEffect> effect = std::make_shared<StarsEffect>(
        ProgramFactory::Get()->CreateFromProgram(mShader.visualProgram));
    std::shared_ptr<Visual> visual = std::make_shared<Visual>(
        mStars->GetVertice(), mStars->GetIndice(), effect);
    visual->UpdateModelBound();

    if (mMaterials[0]->Update(mBlendState))
        Renderer::Get()->Unbind(mBlendState);
    if (mMaterials[0]->Update(mDepthStencilState))
        Renderer::Get()->Unbind(mDepthStencilState);
    if (mMaterials[0]->Update(mRasterizerState))
        Renderer::Get()->Unbind(mRasterizerState);

    Renderer::Get()->SetBlendState(mBlendState);
    Renderer::Get()->SetDepthStencilState(mDepthStencilState);
    Renderer::Get()->SetRasterizerState(mRasterizerState);

    effect->SetStarColor(mMaterials[0]->mDiffuse);

    std::shared_ptr<ConstantBuffer> cbuffer;
    cbuffer = effect->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
    Matrix4x4<float> skyRotation = MakeRotationAxisRadians(
        2.f * GE_C_PI * (wickedTimeOfDay - 0.25f), Vector3<float>::Unit(AXIS_Z));
    Matrix4x4<float> worldMatrix = mWorldTransform * skyRotation;

    Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
    Matrix4x4<float> pvwMatrix = pvMatrix * worldMatrix;
#else
    Matrix4x4<float> pvwMatrix = worldMatrix * pvMatrix;
#endif
    *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

    Renderer* renderer = Renderer::Get();
    renderer->Update(cbuffer);
    renderer->Update(effect->GetStarColor());
    renderer->Update(mStars->GetVertice());
    renderer->Draw(visual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();
}

void Sky::DrawSkyBox(Scene* pScene)
{
    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    for (unsigned int j = 5; j < 11; j++)
    {
        SColorF c(1.f, 1.f, 1.f, 1.f);

        // Use 1.05 rather than 1.0 to avoid colliding with the
        // sun, moon and stars, as this is a background skybox.

        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill vertices
        vertices[0].position = Vector3<float>{ -1.05f, -1.05f, -1.05f };
        vertices[1].position = Vector3<float>{ 1.05f, -1.05f, -1.05f };
        vertices[2].position = Vector3<float>{ 1.05f, 1.05f, -1.05f };
        vertices[3].position = Vector3<float>{ -1.05f, 1.05f, -1.05f };

        vertices[0].tcoord = Vector2<float>{ 0.0f, 0.0f };
        vertices[1].tcoord = Vector2<float>{ 0.0f, 1.0f };
        vertices[2].tcoord = Vector2<float>{ 1.0f, 1.0f };
        vertices[3].tcoord = Vector2<float>{ 1.0f, 0.0f };

        vertices[0].color = c.ToArray();
        vertices[1].color = c.ToArray();
        vertices[2].color = c.ToArray();
        vertices[3].color = c.ToArray();

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

        for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
        {
            if (j == 5)
            {
                // Top texture
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateYZBy(90);

                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(90);
            }
            else if (j == 6)
            {
                // Bottom texture
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateYZBy(-90);

                tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(90);
            }
            else if (j == 7)
            {
                // Left texture
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(90);
            }
            else if (j == 8)
            {
                // Right texture
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(-90);
            }
            else if (j == 9)
            {
                // Front texture, do nothing
                // Game engine doesn't like it when vertexes are left
                // alone and not rotated for some reason.
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 0 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(0);
            }
            else
            {
                // Back texture
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(180);
            }
        }

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
            mMaterials[j]->GetTexture(TT_DIFFUSE),
            mMaterials[j]->mTextureLayer[TT_DIFFUSE].mFilter,
            mMaterials[j]->mTextureLayer[TT_DIFFUSE].mModeU,
            mMaterials[j]->mTextureLayer[TT_DIFFUSE].mModeV);
        std::shared_ptr<Visual> visual =
            std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
        visual->UpdateModelBound();

        if (mMaterials[j]->Update(mBlendState))
            Renderer::Get()->Unbind(mBlendState);
        if (mMaterials[j]->Update(mDepthStencilState))
            Renderer::Get()->Unbind(mDepthStencilState);
        if (mMaterials[j]->Update(mRasterizerState))
            Renderer::Get()->Unbind(mRasterizerState);

        Renderer::Get()->SetBlendState(mBlendState);
        Renderer::Get()->SetDepthStencilState(mDepthStencilState);
        Renderer::Get()->SetRasterizerState(mRasterizerState);

        std::shared_ptr<ConstantBuffer> cbuffer;
        cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
        Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
        Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
        Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
        *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

        Renderer* renderer = Renderer::Get();
        renderer->Update(cbuffer);
        renderer->Update(vertexBuffer);
        renderer->Draw(visual);

        Renderer::Get()->SetDefaultBlendState();
        Renderer::Get()->SetDefaultDepthStencilState();
        Renderer::Get()->SetDefaultRasterizerState();
    }
}

void Sky::DrawSunrise(Scene* pScene, float wickedTimeOfDay)
{
    float mid1 = 0.25f;
    float mid = wickedTimeOfDay < 0.5f ? mid1 : (1.f - mid1);
    float a1 = 1.0f - std::fabs(wickedTimeOfDay - mid) * 35.0f;
    float a = EaseCurve(std::max(0.f, std::min(1.f, a1)));
    //std::cerr<<"a_="<<a_<<" a="<<a<<std::endl;
    SColorF c(1.f, 1.f, 1.f, 1.f);
    float y = -(1.f - a) * 0.22f;

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    unsigned int numVertices = 4;
    std::shared_ptr<VertexBuffer> vertexBuffer =
        std::make_shared<VertexBuffer>(vformat, numVertices);
    vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
    Vertex* vertices = vertexBuffer->Get<Vertex>();
    memset(vertices, 0, numVertices * sizeof(Vertex));

    // fill vertices
    vertices[0].position = Vector3<float>{ -1, -0.05f + y, -1 };
    vertices[1].position = Vector3<float>{ 1, -0.05f + y, -1 };
    vertices[2].position = Vector3<float>{ 1, 0.2f + y, -1 };
    vertices[3].position = Vector3<float>{ -1, 0.2f + y, -1 };

    vertices[0].tcoord = Vector2<float>{ 0.0f, 0.0f };
    vertices[1].tcoord = Vector2<float>{ 0.0f, 1.0f };
    vertices[2].tcoord = Vector2<float>{ 1.0f, 1.0f };
    vertices[3].tcoord = Vector2<float>{ 1.0f, 0.0f };

    vertices[0].color = c.ToArray();
    vertices[1].color = c.ToArray();
    vertices[2].color = c.ToArray();
    vertices[3].color = c.ToArray();

    // fill indices
    unsigned int numTriangles = 2;
    std::shared_ptr<IndexBuffer> indexBuffer =
        std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
    indexBuffer->SetTriangle(0, 0, 1, 2);
    indexBuffer->SetTriangle(1, 2, 3, 0);

    for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
    {
        if (wickedTimeOfDay < 0.5)
        {
            // Switch from -Z (south) to +X (east)
            Quaternion<float> tgt = Rotation<3, float>(
                AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
            vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
            //vertices[i].position.rotateXZBy(90);
        }
        else
        {
            // Switch from -Z (south) to -X (west)
            Quaternion<float> tgt = Rotation<3, float>(
                AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
            vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
            //vertices[i].position.rotateXZBy(-90);
        }
    }

    std::vector<std::string> path;
#if defined(_OPENGL_)
    path.push_back("Effects/Texture2EffectVS.glsl");
    path.push_back("Effects/Texture2EffectPS.glsl");
#else
    path.push_back("Effects/Texture2EffectVS.hlsl");
    path.push_back("Effects/Texture2EffectPS.hlsl");
#endif
    std::shared_ptr<ResHandle> resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

    const std::shared_ptr<ShaderResourceExtraData>& extra =
        std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
    if (!extra->GetProgram())
        extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
        mMaterials[2]->GetTexture(TT_DIFFUSE),
        mMaterials[2]->mTextureLayer[TT_DIFFUSE].mFilter,
        mMaterials[2]->mTextureLayer[TT_DIFFUSE].mModeU,
        mMaterials[2]->mTextureLayer[TT_DIFFUSE].mModeV);
    std::shared_ptr<Visual> visual =
        std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
    visual->UpdateModelBound();

    if (mMaterials[2]->Update(mBlendState))
        Renderer::Get()->Unbind(mBlendState);
    if (mMaterials[2]->Update(mDepthStencilState))
        Renderer::Get()->Unbind(mDepthStencilState);
    if (mMaterials[2]->Update(mRasterizerState))
        Renderer::Get()->Unbind(mRasterizerState);

    Renderer::Get()->SetBlendState(mBlendState);
    Renderer::Get()->SetDepthStencilState(mDepthStencilState);
    Renderer::Get()->SetRasterizerState(mRasterizerState);

    std::shared_ptr<ConstantBuffer> cbuffer;
    cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
    Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
    Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
    Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
    *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

    Renderer* renderer = Renderer::Get();
    renderer->Update(cbuffer);
    renderer->Update(vertexBuffer);
    renderer->Draw(visual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();
}

void Sky::DrawCloudyFogBelow(Scene* pScene)
{
    struct Vertex
    {
        Vector3<float> position;
        Vector4<float> color;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    for (unsigned int j = 0; j < 4; j++)
    {
        SColorF c = mBGColor;

        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill vertices
        vertices[0].position = Vector3<float>{ -1, -1.0f, -1 };
        vertices[1].position = Vector3<float>{ 1, -1.0f, -1 };
        vertices[2].position = Vector3<float>{ 1, -0.02f, -1 };
        vertices[3].position = Vector3<float>{ -1, -0.02f, -1 };

        vertices[0].color = c.ToArray();
        vertices[1].color = c.ToArray();
        vertices[2].color = c.ToArray();
        vertices[3].color = c.ToArray();

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

        for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
        {
            if (j == 0)
            {
                // Don't switch
            }
            else if (j == 1)
            {
                // Switch from -Z (south) to +X (east)
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(90);
            }
            else if (j == 2)
            {
                // Switch from -Z (south) to -X (west)
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(-90);
            }
            else
            {
                // Switch from -Z (south) to +Z (north)
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -180 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(-180);
            }
        }

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/VertexColorEffectVS.glsl");
        path.push_back("Effects/VertexColorEffectPS.glsl");
#else
        path.push_back("Effects/VertexColorEffectVS.hlsl");
        path.push_back("Effects/VertexColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<ColorEffect> effect = std::make_shared<ColorEffect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
        std::shared_ptr<Visual> visual =
            std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
        visual->UpdateModelBound();

        if (mMaterials[1]->Update(mBlendState))
            Renderer::Get()->Unbind(mBlendState);
        if (mMaterials[1]->Update(mDepthStencilState))
            Renderer::Get()->Unbind(mDepthStencilState);
        if (mMaterials[1]->Update(mRasterizerState))
            Renderer::Get()->Unbind(mRasterizerState);

        Renderer::Get()->SetBlendState(mBlendState);
        Renderer::Get()->SetDepthStencilState(mDepthStencilState);
        Renderer::Get()->SetRasterizerState(mRasterizerState);

        std::shared_ptr<ConstantBuffer> cbuffer;
        cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
        Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
        Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
        Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
        *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

        Renderer* renderer = Renderer::Get();
        renderer->Update(cbuffer);
        renderer->Update(vertexBuffer);
        renderer->Draw(visual);

        Renderer::Get()->SetDefaultBlendState();
        Renderer::Get()->SetDefaultDepthStencilState();
        Renderer::Get()->SetDefaultRasterizerState();
    }

    // Draw bottom far cloudy fog thing in front of sun, moon and stars
    SColorF c = mBGColor;

    unsigned int numVertices = 4;
    std::shared_ptr<VertexBuffer> vertexBuffer =
        std::make_shared<VertexBuffer>(vformat, numVertices);
    vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
    Vertex* vertices = vertexBuffer->Get<Vertex>();
    memset(vertices, 0, numVertices * sizeof(Vertex));

    // fill vertices
    vertices[0].position = Vector3<float>{ -1, -1, -1 };
    vertices[1].position = Vector3<float>{ 1, -1, -1 };
    vertices[2].position = Vector3<float>{ 1, -1, -1 };
    vertices[3].position = Vector3<float>{ -1, -1, -1 };

    vertices[0].color = c.ToArray();
    vertices[1].color = c.ToArray();
    vertices[2].color = c.ToArray();
    vertices[3].color = c.ToArray();

    // fill indices
    unsigned int numTriangles = 2;
    std::shared_ptr<IndexBuffer> indexBuffer =
        std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
    indexBuffer->SetTriangle(0, 0, 1, 2);
    indexBuffer->SetTriangle(1, 2, 3, 0);

    std::vector<std::string> path;
#if defined(_OPENGL_)
    path.push_back("Effects/VertexColorEffectVS.glsl");
    path.push_back("Effects/VertexColorEffectPS.glsl");
#else
    path.push_back("Effects/VertexColorEffectVS.hlsl");
    path.push_back("Effects/VertexColorEffectPS.hlsl");
#endif
    std::shared_ptr<ResHandle> resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

    const std::shared_ptr<ShaderResourceExtraData>& extra =
        std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
    if (!extra->GetProgram())
        extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    std::shared_ptr<ColorEffect> effect = std::make_shared<ColorEffect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
    std::shared_ptr<Visual> visual =
        std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
    visual->UpdateModelBound();

    if (mMaterials[1]->Update(mBlendState))
        Renderer::Get()->Unbind(mBlendState);
    if (mMaterials[1]->Update(mDepthStencilState))
        Renderer::Get()->Unbind(mDepthStencilState);
    if (mMaterials[1]->Update(mRasterizerState))
        Renderer::Get()->Unbind(mRasterizerState);

    Renderer::Get()->SetBlendState(mBlendState);
    Renderer::Get()->SetDepthStencilState(mDepthStencilState);
    Renderer::Get()->SetRasterizerState(mRasterizerState);

    std::shared_ptr<ConstantBuffer> cbuffer;
    cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
    Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
    Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
    Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
    *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

    Renderer* renderer = Renderer::Get();
    renderer->Update(cbuffer);
    renderer->Update(vertexBuffer);
    renderer->Draw(visual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();
}

void Sky::DrawCloudyFog(Scene* pScene)
{
    struct Vertex
    {
        Vector3<float> position;
        Vector4<float> color;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    for (unsigned int j = 0; j < 4; j++)
    {
        unsigned int numVertices = 4;
        std::shared_ptr<VertexBuffer> vertexBuffer =
            std::make_shared<VertexBuffer>(vformat, numVertices);
        vertexBuffer->SetUsage(Resource::DYNAMIC_UPDATE);
        Vertex* vertices = vertexBuffer->Get<Vertex>();
        memset(vertices, 0, numVertices * sizeof(Vertex));

        // fill vertices
        vertices[0].position = Vector3<float>{ -1, -0.02f, -1 };
        vertices[1].position = Vector3<float>{ 1, -0.02f, -1 };
        vertices[2].position = Vector3<float>{ 1, 0.45f, -1 };
        vertices[3].position = Vector3<float>{ -1, 0.45f, -1 };

        vertices[0].color = mBGColor.ToArray();
        vertices[1].color = mBGColor.ToArray();
        vertices[2].color = mSkyColor.ToArray();
        vertices[3].color = mSkyColor.ToArray();

        // fill indices
        unsigned int numTriangles = 2;
        std::shared_ptr<IndexBuffer> indexBuffer =
            std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(unsigned int));
        indexBuffer->SetTriangle(0, 0, 1, 2);
        indexBuffer->SetTriangle(1, 2, 3, 0);

        for (unsigned int i = 0; i < vertexBuffer->GetNumElements(); i++)
        {
            if (j == 0)
            {
                // Don't switch
            }
            else if (j == 1)
            {
                // Switch from -Z (south) to +X (east)
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(90);
            }
            else if (j == 2)
            {
                // Switch from -Z (south) to -X (west)
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(-90);
            }
            else
            {
                // Switch from -Z (south) to +Z (north)
                Quaternion<float> tgt = Rotation<3, float>(
                    AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -180 * (float)GE_C_DEG_TO_RAD));
                vertices[i].position = HProject(Rotate(tgt, HLift(vertices[i].position, 0.f)));
                //vertices[i].position.rotateXZBy(-180);
            }
        }

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/VertexColorEffectVS.glsl");
        path.push_back("Effects/VertexColorEffectPS.glsl");
#else
        path.push_back("Effects/VertexColorEffectVS.hlsl");
        path.push_back("Effects/VertexColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<ColorEffect> effect = std::make_shared<ColorEffect>(
            ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
        std::shared_ptr<Visual> visual =
            std::make_shared<Visual>(vertexBuffer, indexBuffer, effect);
        visual->UpdateModelBound();

        if (mMaterials[1]->Update(mBlendState))
            Renderer::Get()->Unbind(mBlendState);
        if (mMaterials[1]->Update(mDepthStencilState))
            Renderer::Get()->Unbind(mDepthStencilState);
        if (mMaterials[1]->Update(mRasterizerState))
            Renderer::Get()->Unbind(mRasterizerState);

        Renderer::Get()->SetBlendState(mBlendState);
        Renderer::Get()->SetDepthStencilState(mDepthStencilState);
        Renderer::Get()->SetRasterizerState(mRasterizerState);

        std::shared_ptr<ConstantBuffer> cbuffer;
        cbuffer = visual->GetEffect()->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
        Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
#if defined(GE_USE_MAT_VEC)
        Matrix4x4<float> pvwMatrix = pvMatrix * mWorldTransform;
#else
        Matrix4x4<float> pvwMatrix = mWorldTransform * pvMatrix;
#endif
        *cbuffer->Get<Matrix4x4<float>>() = pvwMatrix;

        Renderer* renderer = Renderer::Get();
        renderer->Update(cbuffer);
        renderer->Update(vertexBuffer);
        renderer->Draw(visual);

        Renderer::Get()->SetDefaultBlendState();
        Renderer::Get()->SetDefaultDepthStencilState();
        Renderer::Get()->SetDefaultRasterizerState();
    }
}

void Sky::SetSunTexture(const std::string& sunTexture,
    const std::string& sunToneMap, BaseTextureSource* textureSrc)
{
	// Ignore matching textures (with modifiers) entirely,
	// but lets at least update the toneMap before hand.
	mSunParams.toneMap = sunToneMap;
	mSunToneMap = textureSrc->IsKnownSourceImage(mSunParams.toneMap) ?
        textureSrc->GetTexture(mSunParams.toneMap) : nullptr;
	mMaterials[3]->mLighting = !!mSunToneMap;

	if (mSunParams.texture == sunTexture)
		return;

	mSunParams.texture = sunTexture;
	if (sunTexture != "") 
    {
		// We want to ensure the texture exists first.
		mSunTexture = textureSrc->GetTextureForMesh(mSunParams.texture);

		if (mSunTexture) 
        {
			mMaterials[3] = BaseMaterial();
			mMaterials[3]->SetTexture(0, mSunTexture);
			mMaterials[3]->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

            mMaterials[3]->mBlendTarget.enable = true;
            mMaterials[3]->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
            mMaterials[3]->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
            mMaterials[3]->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
            mMaterials[3]->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

            mMaterials[3]->mDepthBuffer = true;
            mMaterials[3]->mDepthMask = DepthStencilState::MASK_ZERO;

            // Disables texture filtering
            //mMaterials[3]->mTextureLayer[0].mFilter
		}
	} 
    else 
    {
		mSunTexture = nullptr;
	}
}

void Sky::SetSunriseTexture(const std::string& sunglowTexture, BaseTextureSource* textureSrc)
{
	// Ignore matching textures (with modifiers) entirely.
	if (mSunParams.sunrise == sunglowTexture)
		return;

	mSunParams.sunrise = sunglowTexture;
	mMaterials[2]->SetTexture(0, textureSrc->GetTextureForMesh(
		sunglowTexture.empty() ? "sunrisebg.png" : sunglowTexture));
}

void Sky::SetMoonTexture(const std::string& moonTexture,
	const std::string& moonToneMap, BaseTextureSource* textureSrc)
{
	// Ignore matching textures (with modifiers) entirely,
	// but lets at least update the toneMap before hand.
	mMoonParams.toneMap = moonToneMap;
	mMoonToneMap = textureSrc->IsKnownSourceImage(mMoonParams.toneMap) ?
        textureSrc->GetTexture(mMoonParams.toneMap) : nullptr;
	mMaterials[4]->mLighting = !!mMoonToneMap;

	if (mMoonParams.texture == moonTexture)
		return;
	mMoonParams.texture = moonTexture;

	if (moonTexture != "") 
    {
		// We want to ensure the texture exists first.
		mMoonTexture = textureSrc->GetTextureForMesh(mMoonParams.texture);

		if (mMoonTexture) 
        {
			mMaterials[4] = BaseMaterial();
			mMaterials[4]->SetTexture(0, mMoonTexture);
			mMaterials[4]->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

            mMaterials[4]->mBlendTarget.enable = true;
            mMaterials[4]->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
            mMaterials[4]->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
            mMaterials[4]->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
            mMaterials[4]->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

            mMaterials[4]->mDepthBuffer = true;
            mMaterials[4]->mDepthMask = DepthStencilState::MASK_ZERO;

            // Disables texture filtering
            //mMaterials[4]->mTextureLayer[0].mFilter
		}
	} 
    else 
    {
		mMoonTexture = nullptr;
	}
}

void Sky::SetStarCount(unsigned int starCount, bool forceUpdate)
{
	// Allow force updating star count at game init.
	if (mStarParams.count != starCount || forceUpdate) 
    {
		mStarParams.count = starCount;
		UpdateStars();
	}
}

void Sky::UpdateStars()
{
	// Game engine doesn’t allow non-indexed rendering, and indexed quad
	// rendering is slow due to lack of hardware support. So as indices are
	// 16-bit and there are 4 vertices per star... the limit is 2^16/4 = 0x4000.
	// That should be well enough actually.
	if (mStarParams.count > 0x4000) 
    {
		LogWarning("Requested " + std::to_string(mStarParams.count) + 
            " stars but " + std::to_string(0x4000) + " is the max\n");
		mStarParams.count = 0x4000;
	}
    struct Vertex
    {
        Vector3<float> position;
    };

    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);

    mStars = std::make_shared<MeshBuffer>(vformat,
        4 * mStarParams.count, 2 * mStarParams.count, sizeof(unsigned int));

	//SColorF fallbackColor = mStarParams.starcolor; // used on GLES 2 “without shaders”
    mPcgRand = PcgRandom(mSeed);
	float d = (0.006f / 2) * mStarParams.scale;
    Vertex* vertices = mStars->GetVertice()->Get<Vertex>();
	for (uint16_t i = 0; i < mStarParams.count; i++) 
    {
        Vector3<float> from = Vector3<float>::Unit(AXIS_Y);
        Vector3<float> to{
            (float)mPcgRand.Range(-10000, 10000),
            (float)mPcgRand.Range(-10000, 10000),
            (float)mPcgRand.Range(-10000, 10000)};

        Matrix4x4<float> rotationMatrix = MakeRotateFromTo(from, to);

        Vector4<float> p, p1, p2, p3;
        rotationMatrix.Transformation(Vector4<float>{-d, 1, -d, 0}, p);
        rotationMatrix.Transformation(Vector4<float>{d, 1, -d, 0}, p1);
        rotationMatrix.Transformation(Vector4<float>{d, 1, d, 0}, p2);
        rotationMatrix.Transformation(Vector4<float>{-d, 1, d, 0}, p3);

        vertices[i*4].position = Vector3<float>{ p[0], p[1], p[2] };
        vertices[i*4+1].position = Vector3<float>{ p1[0], p1[1], p1[2] };
        vertices[i*4+2].position = Vector3<float>{ p2[0], p2[1], p2[2] };
        vertices[i*4+3].position = Vector3<float>{ p3[0], p3[1], p3[2] };
	}

    unsigned int numIndices = mStars->GetIndice()->GetNumElements();
    unsigned int* indices = mStars->GetIndice()->Get<unsigned int>();

    int idx = 0;
	for (uint16_t i = 0; i < mStarParams.count; i++) 
    {
        indices[idx++] = i * 4 + 0;
        indices[idx++] = i * 4 + 1;
        indices[idx++] = i * 4 + 2;

        indices[idx++] = i * 4 + 2;
        indices[idx++] = i * 4 + 3;
        indices[idx++] = i * 4 + 0;
	}
}

void Sky::SetSkyColors(const SkyColor& skyColor)
{
	mSkyParams.skyColor = skyColor;
}

void Sky::SetHorizonTint(SColor sunTint, SColor moonTint, const std::string& useSunTint)
{
	// Change sun and moon tinting:
	mSkyParams.fogSunTint = sunTint;
	mSkyParams.fogMoonTint = moonTint;
	// Faster than comparing strings every rendering frame
	if (useSunTint == "default")
		mDefaultTint = true;
	else if (useSunTint == "custom")
		mDefaultTint = false;
	else
		mDefaultTint = true;
}

void Sky::AddTextureToSkybox(const std::string& texture, int materialId, BaseTextureSource* textureSrc)
{
	// Sanity check for more than six textures.
	if (materialId + 5 >= SKY_MATERIAL_COUNT)
		return;
	// Keep a list of texture names handy.
	mSkyParams.textures.emplace_back(texture);
	std::shared_ptr<Texture2> result = textureSrc->GetTextureForMesh(texture);
	mMaterials[materialId + 5] = BaseMaterial();
	mMaterials[materialId + 5]->SetTexture(0, result);
	mMaterials[materialId + 5]->mType = MT_SOLID;
}

// To be called once at game init to setup default values.
void Sky::SetSkyDefaults()
{
	SkyboxDefaults skyDefaults;
	mSkyParams.skyColor = skyDefaults.GetSkyColorDefaults();
	mSunParams = skyDefaults.GetSunDefaults();
	mMoonParams = skyDefaults.GetMoonDefaults();
	mStarParams = skyDefaults.GetStarDefaults();
}
