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

#include "../MinecraftEvents.h"

#include "PlayerCamera.h"
#include "WieldMesh.h"
#include "Node.h"

#include "Map/VisualMap.h"

#include "Actors/VisualPlayer.h"

#include "../Games/Actors/Player.h"
#include "../Games/Environment/VisualEnvironment.h"

#include "../Utils/Noise.h"

#include "Graphic/Scene/Scene.h"

#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Element/CameraNode.h"

#include "Application/GameApplication.h"

#define CAMERA_OFFSET_STEP 200
#define WIELDMESH_OFFSET_X 55.0f
#define WIELDMESH_OFFSET_Y -35.0f
#define WIELDMESH_AMPLITUDE_X 7.0f
#define WIELDMESH_AMPLITUDE_Y 10.0f

PlayerCamera::PlayerCamera(VisualEnvironment* env, Scene* pScene, BaseUI* ui, MapDrawControl* drawControl) :
	mDrawControl(drawControl), mEnvironment(env), mUI(ui)
{
	// note: making the camera node a child of the player node
	// would lead to unexpected behaviour, so we don't do that.
	mPlayerNode = pScene->AddEmptyNode(0);
	mHeadNode = pScene->AddEmptyNode(mPlayerNode);
	mCameraNode = pScene->AddCameraNode(0);
	//UpdateCameraRotation(mCameraNode, Vector4<float>::Zero(), Vector4<float>{0, 0, 100, 0});
	//mCameraNode->BindTargetAndRotation(true);

	// This needs to be in its own scene manager. It is drawn after
	// all other 3D scene nodes and before the GUI.
    mWieldMgr.reset(new Scene());
	mWieldMgr->AddCameraNode();
	UpdateCameraRotation(mWieldMgr->GetActiveCamera(), 
		Vector4<float>::Zero(), Vector4<float>{0, 0, 100, 0});
	mWieldMgr->GetRootNode()->SetCullingMode(CULL_NEVER);
	mWieldNode = std::make_shared<WieldMeshNode>(
		INVALID_ACTOR_ID, false, mEnvironment, &mWieldMgr->GetPVWUpdater());
	mWieldMgr->AddSceneNode(INVALID_ACTOR_ID, mWieldNode);
	mWieldNode->CreateMesh();
	mWieldNode->SetItem(ItemStack());

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change logic settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	mCacheFallBobbingAmount = Settings::Get()->GetFloat("fall_bobbing_amount");
	mCacheViewBobbingAmount = Settings::Get()->GetFloat("view_bobbing_amount");

	// 45 degrees is the lowest FOV that doesn't cause the logic to treat this
	// as a zoom FOV and load world beyond the set logic limits.
	mCacheFOV = std::fmax(Settings::Get()->GetFloat("fov"), 45.0f);
	mArmInertia = Settings::Get()->GetBool("arm_inertia");
	mNameTags.clear();
	mShowNameTagBackgrounds = Settings::Get()->GetBool("show_nametag_backgrounds");

    // Create a vertex buffer for a single triangle.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    std::vector<std::string> path;
#if defined(_OPENGL_)
    path.push_back("Effects/ColorEffectVS.glsl");
    path.push_back("Effects/ColorEffectPS.glsl");
#else
    path.push_back("Effects/ColorEffectVS.hlsl");
    path.push_back("Effects/ColorEffectPS.hlsl");
#endif
	std::shared_ptr<ResHandle> resHandle =
		ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

	const std::shared_ptr<ShaderResourceExtraData>& extra =
		std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
	if (!extra->GetProgram())
		extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    mEffect = std::make_shared<ColorEffect>(
		ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

PlayerCamera::~PlayerCamera()
{

}

void PlayerCamera::UpdateCameraRotation(std::shared_ptr<CameraNode> camera, 
	const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const
{
	EulerAngles<float> rotation;
	rotation.mAxis[1] = 1;
	rotation.mAxis[2] = 2;

	Quaternion<float> q(targetPos - cameraPos);
	rotation = Rotation<4, float>(q)(rotation.mAxis[0], rotation.mAxis[1], rotation.mAxis[2]);
	camera->GetRelativeTransform().SetRotation(rotation);
	camera->UpdateAbsoluteTransform();
}

void PlayerCamera::NotifyFovChange()
{
	VisualPlayer* player = mEnvironment->GetPlayer();
	LogAssert(player, "invalid");

	/*
	 * Update mOldFOVDegrees first - it serves as the starting point of the
	 * upcoming transition.
	 *
	 * If an FOV transition is already active, mark current FOV as the start of
	 * the new transition. If not, set it to the previous transition's target FOV.
	 */
	if (mFovTransitionActive)
		mOldFOVDegrees = mCurrentFOVDegrees;
	else
		mOldFOVDegrees = mUpdateFOV ? mTargetFOVDegrees : mCacheFOV;

	/*
	 * Update mUpdateFOV next - it corresponds to the target FOV of the
	 * upcoming transition.
	 *
	 * Set it to mCacheFOV, if logic-sent FOV is 0. Otherwise check if
	 * logic-sent FOV is a multiplier, and multiply it with mCacheFOV instead
	 * of overriding.
	 */
	if (player->GetFov().fov == 0.0f)
    {
		mUpdateFOV = false;
		mTargetFOVDegrees = mCacheFOV;
	} 
    else 
    {
		mUpdateFOV = true;
		mTargetFOVDegrees = player->GetFov().isMultiplier ? 
			mCacheFOV * player->GetFov().fov : player->GetFov().fov;
	}

	if (player->GetFov().transitionTime > 0.0f)
		mFovTransitionActive = true;

	// If FOV smooth transition is active, initialize required variables
	if (mFovTransitionActive) 
    {
		mTransitionTime = player->GetFov().transitionTime;
		mFovDiff = mTargetFOVDegrees - mOldFOVDegrees;
	}
}

bool PlayerCamera::SuccessfullyCreated()
{
	bool success = false;
	if (!mPlayerNode) 
    {
		LogError("Failed to create the player scene node");
	}
    else if (!mHeadNode) 
    {
		LogError("Failed to create the head scene node");
	} 
    else if (!mCameraNode) 
    {
		LogError("Failed to create the camera scene node");
	} 
    else if (!mWieldMgr) 
    {
		LogError("Failed to create the wielded item scene manager");
	}
    else if (!mWieldNode)
    {
		LogError("Failed to create the wielded item scene node");
	} 
    else 
    {
		success = true;
	}

    if (BaseGame::Get()->ModsLoaded())
        BaseGame::Get()->OnCameraReady(this);

	return success;
}

void PlayerCamera::Step(float dTime)
{
	if(mViewBobbingFall > 0)
	{
		mViewBobbingFall -= 3 * dTime;
		if(mViewBobbingFall <= 0)
			mViewBobbingFall = -1; // Mark the effect as finished
	}

	bool wasUnderZero = mWieldChangeTimer < 0;
	mWieldChangeTimer = std::min(mWieldChangeTimer + dTime, 0.125f);

	if (mWieldChangeTimer >= 0 && wasUnderZero)
		mWieldNode->SetItem(mWieldItemNext);

	if (mViewBobbingState != 0)
	{
		//float offset = deltaMs * mViewBobbingSpeed * 0.035f;
		float offset = dTime * mViewBobbingSpeed * 0.03f;
		if (mViewBobbingState == 2) 
        {
			// Animation is getting turned off
			if (mViewBobbingAnim < 0.25) 
            {
				mViewBobbingAnim -= offset;
			} 
            else if (mViewBobbingAnim > 0.75) 
            {
				mViewBobbingAnim += offset;
			}

			if (mViewBobbingAnim < 0.5) 
            {
				mViewBobbingAnim += offset;
				if (mViewBobbingAnim > 0.5)
					mViewBobbingAnim = 0.5;
			} 
            else
            {
				mViewBobbingAnim -= offset;
				if (mViewBobbingAnim < 0.5)
					mViewBobbingAnim = 0.5;
			}

			if (mViewBobbingAnim <= 0 || 
                mViewBobbingAnim >= 1 || 
                fabs(mViewBobbingAnim - 0.5) < 0.01) 
            {
				mViewBobbingAnim = 0;
				mViewBobbingState = 0;
			}
		}
		else 
        {
			float was = mViewBobbingAnim;
			mViewBobbingAnim = modf(mViewBobbingAnim + offset, &mViewBobbingAnim);
			bool step = (was == 0 ||
					(was < 0.5f && mViewBobbingAnim >= 0.5f) ||
					(was > 0.5f && mViewBobbingAnim <= 0.5f));
            if (step)
                BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataViewBobbingStep>());
		}
	}

	if (mDiggingButton != -1) 
    {
		float offset = dTime * 3.5f;
		float mDiggingAnim_was = mDiggingAnim;
		mDiggingAnim += offset;
		if (mDiggingAnim >= 1)
		{
			mDiggingAnim = 0;
			mDiggingButton = -1;
		}
		float lim = 0.15f;
		if(mDiggingAnim_was < lim && mDiggingAnim >= lim)
		{
			if (mDiggingButton == 0) 
                BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataCameraPunchLeft>());
            else if(mDiggingButton == 1)
                BaseEventManager::Get()->TriggerEvent(std::make_shared<EventDataCameraPunchRight>());
		}
	}
}

void PlayerCamera::AddArmInertia(float playerYaw)
{
	mCamVel[0] = std::fabs(std::clamp(mLastCamPos[0] - playerYaw, -100.0f, 100.0f) / 0.016f) * 0.01f;
	mCamVel[1] = std::fabs((mLastCamPos[1] - mCameraDirection[1]) / 0.016f);
	float gapX = std::fabs(WIELDMESH_OFFSET_X - mWieldMeshOffset[0]);
	float gapY = std::fabs(WIELDMESH_OFFSET_Y - mWieldMeshOffset[1]);

	if (mCamVel[0] > 1.0f || mCamVel[1] > 1.0f) 
    {
		/*
		    The arm moves relative to the camera speed,
		    with an acceleration factor.
		*/
		if (mCamVel[0] > 1.0f) 
        {
			if (mCamVel[0] > mCamVelOld[0])
				mCamVelOld[0] = mCamVel[0];

			float accX = 0.12f * (mCamVel[0] - (gapX * 0.1f));
			mWieldMeshOffset[0] += mLastCamPos[0] < playerYaw ? accX : -accX;

			if (mLastCamPos[0] != playerYaw)
				mLastCamPos[0] = playerYaw;

			mWieldMeshOffset[0] = std::clamp(mWieldMeshOffset[0],
				WIELDMESH_OFFSET_X - (WIELDMESH_AMPLITUDE_X * 0.5f),
				WIELDMESH_OFFSET_X + (WIELDMESH_AMPLITUDE_X * 0.5f));
		}

		if (mCamVel[1] > 1.0f) 
        {
			if (mCamVel[1] > mCamVelOld[1])
				mCamVelOld[1] = mCamVel[1];

			float accY = 0.12f * (mCamVel[1] - (gapY * 0.1f));
			mWieldMeshOffset[1] += mLastCamPos[1] > mCameraDirection[1] ? accY : -accY;

			if (mLastCamPos[1] != mCameraDirection[1])
				mLastCamPos[1] = mCameraDirection[1];

			mWieldMeshOffset[1] = std::clamp(mWieldMeshOffset[1],
				WIELDMESH_OFFSET_Y - (WIELDMESH_AMPLITUDE_Y * 0.5f),
				WIELDMESH_OFFSET_Y + (WIELDMESH_AMPLITUDE_Y * 0.5f));
		}

        float x = mWieldMeshOffset[0] - WIELDMESH_OFFSET_X;
        float y = mWieldMeshOffset[1] - WIELDMESH_OFFSET_Y;

        float xAbs = std::fabs(x);
        float yAbs = std::fabs(y);
        if (xAbs >= yAbs)
        {
            y *= (1.0f / xAbs);
            x /= xAbs;
        }

        if (yAbs >= xAbs)
        {
            x *= (1.0f / yAbs);
            y /= yAbs;
        }

        mArmDir = {std::fabs(x), std::fabs(y)};
	} 
    else 
    {
		/*
		    Now the arm gets back to its default position when the camera stops,
		    following a vector, with a smooth deceleration factor.
		*/
		float decX = 0.35f * (std::min(15.0f, mCamVelOld[0]) * (1.0f +
			(1.0f - mArmDir[0]))) * (gapX / 20.0f);

		float decY = 0.25f * (std::min(15.0f, mCamVelOld[1]) * (1.0f +
			(1.0f - mArmDir[1]))) * (gapY / 15.0f);

		if (gapX < 0.1f) mCamVelOld[0] = 0.0f;

		mWieldMeshOffset[0] -= mWieldMeshOffset[0] > WIELDMESH_OFFSET_X ? decX : -decX;

		if (gapY < 0.1f) mCamVelOld[1] = 0.0f;

		mWieldMeshOffset[1] -= mWieldMeshOffset[1] > WIELDMESH_OFFSET_Y ? decY : -decY;
	}
}

void PlayerCamera::Update(VisualPlayer* player, 
    float frametime, float busytime, float toolReloadRatio)
{
	// Get player position
	// Smooth the movement when walking up stairs
	Vector3<float> oldPlayerPosition = mPlayerNode->GetRelativeTransform().GetTranslation();
	Vector3<float> playerPosition = player->GetPosition();

	// This is worse than `VisualPlayer::GetPosition()` but
	// mods expect the player head to be at the parent's position
	// plus eye height.
	if (player->GetParent())
		playerPosition = player->GetParent()->GetPosition();

	// Smooth the camera movement when the player instantly moves upward due to stepheight.
	// To smooth the 'not mTouchingGround' stepheight, smoothing is necessary when jumping
	// or swimming (for when moving from liquid to land).
	// Disable smoothing if climbing or flying, to avoid upwards offset of player model
	// when seen in 3rd person view.
	bool flying = Settings::Get()->GetBool("free_move") && mEnvironment->CheckLocalPrivilege("fly");
	if (playerPosition[1] > oldPlayerPosition[1] && !player->mIsClimbing && !flying) 
    {
		float oldy = oldPlayerPosition[1];
		float newy = playerPosition[1];
		float t = std::exp(-23 * frametime);
		playerPosition[1] = oldy * t + newy * (1-t);
	}

	// Set player node transformation
	mPlayerNode->GetRelativeTransform().SetTranslation(playerPosition);

    Matrix4x4<float> yawRotation = Rotation<4, float>(
        AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), -1 * player->GetYaw() * (float)GE_C_DEG_TO_RAD));
    Matrix4x4<float> pitchRotation = Rotation<4, float>(
        AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), 0));
    Matrix4x4<float> rollRotation = Rotation<4, float>(
        AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), 0));

    Matrix4x4<float> rotation = yawRotation * pitchRotation * rollRotation;
    mPlayerNode->GetRelativeTransform().SetRotation(rotation);
    mPlayerNode->UpdateAbsoluteTransform();

	// Get camera tilt timer (hurt animation)
	float cameratilt = fabs(fabs(player->mHurtTiltTimer-0.75f) - 0.75f);

	// Fall bobbing animation
	float fallBobbing = 0;
	if(player->mCameraImpact >= 1 && mCameraMode < CAMERA_MODE_THIRD)
	{
		if(mViewBobbingFall == -1) // Effect took place and has finished
			player->mCameraImpact = mViewBobbingFall = 0;
		else if(mViewBobbingFall == 0) // Initialize effect
			mViewBobbingFall = 1;

		// Convert 0 -> 1 to 0 -> 1 -> 0
		fallBobbing = mViewBobbingFall < 0.5f ? mViewBobbingFall * 2 : -(mViewBobbingFall - 0.5f) * 2 + 1;
		// Smoothen and invert the above
		fallBobbing = sin(fallBobbing * 0.5f * (float)GE_C_PI) * -1;
		// Amplify according to the intensity of the impact
		fallBobbing *= (1 - std::clamp(50 / player->mCameraImpact, 0.f, 1.f)) * 5;

		fallBobbing *= mCacheFallBobbingAmount;
	}

	// Calculate and translate the head SceneNode offsets
	{
		Vector3<float> eyeOffset = player->GetEyeOffset();
		if (mCameraMode == CAMERA_MODE_FIRST)
			eyeOffset += player->mEyeOffsetFirst;
		else
			eyeOffset += player->mEyeOffsetThird;

		// Set head node transformation
		eyeOffset[1] += cameratilt * -player->mHurtTiltStrength + fallBobbing;
		mHeadNode->GetRelativeTransform().SetTranslation(eyeOffset);

        yawRotation = Rotation<4, float>(AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), 0.f));
		pitchRotation = Rotation<4, float>(AxisAngle<4, float>(
			-Vector4<float>::Unit(AXIS_X), player->GetPitch() * (float)GE_C_DEG_TO_RAD));
        rollRotation = Rotation<4, float>(AxisAngle<4, float>(
            -Vector4<float>::Unit(AXIS_Z), cameratilt * player->mHurtTiltStrength * (float)GE_C_DEG_TO_RAD));

		rotation = yawRotation * pitchRotation * rollRotation;
        mHeadNode->GetRelativeTransform().SetRotation(rotation);
		mHeadNode->UpdateAbsoluteTransform();
	}

	// Compute relative camera position and target
    Vector3<float> relCamPos = Vector3<float>{ 0,0,0 };
    Vector3<float> relCamTarget = Vector3<float>{ 0,0,1 };
    Vector3<float> relCamUp = Vector3<float>{ 0,1,0 };

	if (mCacheViewBobbingAmount != 0.0f && 
        mViewBobbingAnim != 0.0f &&
		mCameraMode < CAMERA_MODE_THIRD) 
    {
        float bobfrac;
		bobfrac = modf(mViewBobbingAnim * 2, &bobfrac);
		float bobdir = (mViewBobbingAnim < 0.5f) ? 1.f : -1.f;

		#if 1
		    float bobknob = 1.2f;
		    float bobtmp = (float)sin(pow(bobfrac, bobknob) * GE_C_PI);
		    //float bobtmp2 = cos(pow(bobfrac, bobknob) * GE_C_PI);

            Vector3<float> bobvec = Vector3<float>{
                0.3f * bobdir * (float)sin(bobfrac * GE_C_PI),
                -0.28f * bobtmp * bobtmp, 0.f };

		    //relCamPos += 0.2 * bobvec;
		    //relCamTarget += 0.03 * bobvec;
		    //relCamUp.rotateXYBy(0.02 * bobdir * bobtmp * M_PI);
		    float f = 1.0;
		    f *= mCacheViewBobbingAmount;
		    relCamPos += bobvec * f;
		    //relCamTarget += 0.995 * bobvec * f;
		    relCamTarget += bobvec * f;
		    relCamTarget[2] -= 0.005f * bobvec[2] * f;
		    //relCamTarget[0] -= 0.005 * bobvec[0] * f;
		    //relCamTarget[1] -= 0.005 * bobvec[1] * f;
            Quaternion<float> tgt = Rotation<3, float>(AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 
				0.03f * bobdir * bobtmp * (float)GE_C_PI * f * (float)GE_C_DEG_TO_RAD));
            relCamUp = HProject(Rotate(tgt, HLift(relCamUp, 0.f)));
            //relCamUp.rotateXYBy(-0.03 * bobdir * bobtmp * GE_C_PI * f);
		#else
		    float angleDeg = 1 * bobdir * sin(bobfrac * GE_C_PI);
		    float angleRad = angleDeg * GE_C_PI / 180;
		    float r = 0.05;
		    Vector3<float> off = Vector3<float>(
			    r * sin(angleRad),
			    r * (cos(angleRad) - 1),
			    0);
		    relCamPos += off;
		    //relCamTarget += off;

			Quaternion<float> tgt = Rotation<3, float>(AxisAngle<3, float>(
				-Vector3<float>::Unit(AXIS_Z), -angleDeg * (float)GE_C_DEG_TO_RAD));
			relCamUp = HProject(Rotate(tgt, HLift(relCamUp, 0.f)));
		    //relCamUp.rotateXYBy(angleDeg);
		#endif

	}

	// Compute absolute camera position and target
	Vector4<float> cameraPos = HLift(relCamPos, 0.f);
	mHeadNode->GetAbsoluteTransform().GetRotation().Transformation(cameraPos);
    cameraPos += mHeadNode->GetAbsoluteTransform().GetTranslationW0();
    mCameraPosition = HProject(cameraPos);

	Vector4<float> cameraDir = HLift(relCamTarget - relCamPos, 0.f);
    mHeadNode->GetAbsoluteTransform().GetRotation().Transformation(cameraDir);
    mCameraDirection = HProject(cameraDir);

	Vector4<float> absCamUp = HLift(relCamUp, 0.f);
	mHeadNode->GetAbsoluteTransform().GetRotation().Transformation(absCamUp);

	// Reposition the camera for third person view
	if (mCameraMode > CAMERA_MODE_FIRST)
	{
		if (mCameraMode == CAMERA_MODE_THIRD_FRONT)
			mCameraDirection *= -1.f;

		cameraPos[1] += 2;

		// Calculate new position
		bool abort = false;
		for (int i = (int)BS; i <= BS * 2.75f; i++) 
        {
			cameraPos[0] = mCameraPosition[0] + mCameraDirection[0] * -i;
			cameraPos[2] = mCameraPosition[2] + mCameraDirection[2] * -i;
			if (i > 12)
				cameraPos[1] = mCameraPosition[1] + (mCameraDirection[1] * -i);

			// Prevent camera positioned inside nodes
            Vector3<short> cp;
            cp[0] = (short)((cameraPos[0] + (cameraPos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
            cp[1] = (short)((cameraPos[1] + (cameraPos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
            cp[2] = (short)((cameraPos[2] + (cameraPos[2] > 0 ? BS / 2 : -BS / 2)) / BS);

            const NodeManager* nodeMgr = mEnvironment->GetNodeManager();
			MapNode node = mEnvironment->GetVisualMap()->GetNode(cp);

			const ContentFeatures& features = nodeMgr->Get(node);
			if (features.walkable) 
            {
				cameraPos[0] += mCameraDirection[0]*-1*-BS/2;
				cameraPos[2] += mCameraDirection[2]*-1*-BS/2;
				cameraPos[1] += mCameraDirection[1]*-1*-BS/2;
				abort = true;
				break;
			}
		}

		// If node blocks camera position don't move y to heigh
		if (abort && cameraPos[1] > playerPosition[1]+BS*2)
			cameraPos[1] = playerPosition[1]+BS*2;
	}

	// Update offset if too far away from the center of the map
	mCameraOffset[0] += CAMERA_OFFSET_STEP *
			(((short)(cameraPos[0]/BS) - mCameraOffset[0])/CAMERA_OFFSET_STEP);
	mCameraOffset[1] += CAMERA_OFFSET_STEP *
			(((short)(cameraPos[1]/BS) - mCameraOffset[1])/CAMERA_OFFSET_STEP);
	mCameraOffset[2] += CAMERA_OFFSET_STEP *
			(((short)(cameraPos[2]/BS) - mCameraOffset[2])/CAMERA_OFFSET_STEP);

	// Set camera node transformation
    Vector3<float> cameraOffset{ 
        (float)mCameraOffset[0], (float)mCameraOffset[1], (float)mCameraOffset[2] };
	mCameraNode->GetRelativeTransform().SetTranslation(
        HProject(cameraPos) - cameraOffset * (float)BS);

    // *100.0 helps in large map coordinates
	Vector4<float> target = Vector4<float>(
		cameraPos - HLift(cameraOffset * (float)BS, 0.f) + HLift(100.f * mCameraDirection, 0.f));
	Vector4<float> direction = target - mCameraNode->GetAbsoluteTransform().GetTranslationW0();
	Normalize(direction);

	yawRotation = Rotation<4, float>(AxisAngle<4, float>(
		Vector4<float>::Unit(AXIS_Y), atan2(direction[0], direction[2])));
	pitchRotation = Rotation<4, float>(AxisAngle<4, float>(
		Vector4<float>::Unit(AXIS_X), -asin(direction[1])));
	mCameraNode->GetRelativeTransform().SetRotation(yawRotation * pitchRotation);
	mCameraNode->UpdateAbsoluteTransform();

	Vector3<float> pos = mCameraNode->GetAbsoluteTransform().GetTranslation();

	// update the camera position in third-person mode to render blocks behind player
	// and correctly apply liquid post FX.
	if (mCameraMode != CAMERA_MODE_FIRST)
		mCameraPosition = HProject(cameraPos);

	/*
	 * Apply logic-sent FOV, instantaneous or smooth transition.
	 * If not, check for zoom and set to zoom FOV.
	 * Otherwise, default to mCacheFOV.
	 */
	if (mFovTransitionActive) 
    {
		// Smooth FOV transition
		// Dynamically calculate FOV delta based on frametimes
		float delta = (frametime / mTransitionTime) * mFovDiff;
		mCurrentFOVDegrees += delta;

		// Mark transition as complete if target FOV has been reached
		if ((mFovDiff > 0.0f && mCurrentFOVDegrees >= mTargetFOVDegrees) ||
            (mFovDiff < 0.0f && mCurrentFOVDegrees <= mTargetFOVDegrees)) 
        {
			mFovTransitionActive = false;
			mCurrentFOVDegrees = mTargetFOVDegrees;
		}
	} 
    else if (mUpdateFOV) 
    {
		// Instantaneous FOV change
		mCurrentFOVDegrees = mTargetFOVDegrees;
	} 
    else if (player->GetPlayerControl().zoom && player->GetZoomFOV() > 0.001f) 
    {
		// Player requests zoom, apply zoom FOV
		mCurrentFOVDegrees = player->GetZoomFOV();
	} 
    else 
    {
		// Set to visual's selected FOV
		mCurrentFOVDegrees = mCacheFOV;
	}
	mCurrentFOVDegrees = std::clamp(mCurrentFOVDegrees, 1.0f, 160.0f);

	// FOV and aspect ratio
    Renderer* renderer = Renderer::Get();
    Vector2<unsigned int> screenSize(renderer->GetScreenSize());
	mAspect = (float)screenSize[0] / (float)screenSize[1];
	mFovY = mCurrentFOVDegrees * (float)GE_C_DEG_TO_RAD;
	// Increase vertical FOV on lower aspect ratios (<16:10)
	mFovY *= std::clamp((float)sqrt(16./10. / mAspect), 1.f, 1.4f);
	mFovX = 2 * atan(mAspect * tan(0.5f * mFovY));
    mCameraNode->Get()->SetFrustum(mFovY * (float)GE_C_RAD_TO_DEG, mAspect, 
        mCameraNode->Get()->GetDMin(), mCameraNode->Get()->GetDMax());

	if (mArmInertia)
		AddArmInertia(player->GetYaw());

	// Position the wielded item
	//Vector3<float> wieldPosition = Vector3<float>(45, -35, 65);
    Vector3<float> wieldPosition = Vector3<float>{ mWieldMeshOffset[0], mWieldMeshOffset[1], 65 };
	//Vector3<float> wieldRotation = Vector3<float>(-100, 120, -100);
    Vector3<float> wieldRotation = Vector3<float>{ -100, 120, -100 };
	wieldPosition[1] += fabs(mWieldChangeTimer)*320 - 40;
	if(mDiggingAnim < 0.05f || mDiggingAnim > 0.5f)
	{
		float frac = 1.f;
		if(mDiggingAnim > 0.5f)
			frac = 2.f * (mDiggingAnim - 0.5f);
		// This value starts from 1 and settles to 0
		float ratiothing = std::pow((1.0f - toolReloadRatio), 0.5f);
		//float ratiothing2 = pow(ratiothing, 0.5f);
		float ratiothing2 = (EaseCurve(ratiothing*0.5f))*2.f;
		wieldPosition[1] -= frac * 25.f * pow(ratiothing2, 1.7f);
		//wieldPosition[2] += frac * 5.f * ratiothing2;
		wieldPosition[0] -= frac * 35.f * pow(ratiothing2, 1.1f);
		wieldRotation[1] += frac * 70.f * pow(ratiothing2, 1.4f);
		//wieldRotation[0] -= frac * 15.f * pow(ratiothing2, 1.4f);
		//wieldRotation[2] += frac * 15.f * pow(ratiothing2, 1.0f);
	}
    if (mDiggingButton != -1)
    {
        float digfrac = mDiggingAnim;
        wieldPosition[0] -= 50 * (float)sin(pow(digfrac, 0.8f) * (float)GE_C_PI);
        wieldPosition[1] += 24 * (float)sin(digfrac * 1.8 * GE_C_PI);
        wieldPosition[2] += 25 * 0.5f;

        // Euler angles are PURE EVIL, so why not use quaternions?
		float yaw = wieldRotation[2] * (float)GE_C_DEG_TO_RAD;
		float pitch = wieldRotation[1] * (float)GE_C_DEG_TO_RAD;
		float roll = wieldRotation[0] * (float)GE_C_DEG_TO_RAD;

		Matrix4x4<float> yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), yaw));
		Matrix4x4<float> pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), pitch));
		Matrix4x4<float> rollRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), roll));
		Quaternion<float> quatBegin = Rotation<4, float>(
			yawRotation * pitchRotation * rollRotation);

		yaw = 100 * (float)GE_C_DEG_TO_RAD;
		pitch = 30 * (float)GE_C_DEG_TO_RAD;
		roll = 80 * (float)GE_C_DEG_TO_RAD;
		yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), yaw));
		pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), pitch));
		rollRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), roll));
		Quaternion<float> quatEnd = Rotation<4, float>(
			yawRotation * pitchRotation * rollRotation);

        Quaternion<float> quatSlerp = Slerp((float)sin(digfrac * GE_C_PI), quatBegin, quatEnd);
        mWieldNode->GetRelativeTransform().SetRotation(quatSlerp);
	} 
    else 
    {
        float bobfrac;
		bobfrac = modf(mViewBobbingAnim, &bobfrac);
		wieldPosition[0] -= (float)sin(bobfrac * GE_C_PI * 2.0) * 3.f;
		wieldPosition[1] += (float)sin(modf(bobfrac * 2.f, &bobfrac) * GE_C_PI) * 3.f;

		float yaw = wieldRotation[2] * (float)GE_C_DEG_TO_RAD;
		float pitch = wieldRotation[1] * (float)GE_C_DEG_TO_RAD;
		float roll = wieldRotation[0] * (float)GE_C_DEG_TO_RAD;

		Matrix4x4<float> yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), yaw));
		Matrix4x4<float> pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), pitch));
		Matrix4x4<float> rollRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), roll));

		Quaternion<float> quat = Rotation<4, float>(
			yawRotation * pitchRotation * rollRotation);
		mWieldNode->GetRelativeTransform().SetRotation(quat);
	}
	mWieldNode->GetRelativeTransform().SetTranslation(wieldPosition);
	mWieldNode->UpdateAbsoluteTransform();

	mWieldNode->SetNodeLightColor(player->mLightColor);

	// Set render distance
	UpdateViewingRange();

	// If the player is walking, swimming, or climbing,
	// view bobbing is enabled and freeMove is off,
	// start (or continue) the view bobbing animation.
	const Vector3<float>& speed = player->GetSpeed();
	const bool movementXZ = hypot(speed[0], speed[2]) > BS;
	const bool movementY = fabs(speed[1]) > BS;

	const bool walking = movementXZ && player->mTouchingGround;
	const bool swimming = (movementXZ || player->mSwimmingVertical) && player->mInLiquid;
	const bool climbing = movementY && player->mIsClimbing;
	if ((walking || swimming || climbing) && !flying) 
    {
		// Start animation
		mViewBobbingState = 1;
		mViewBobbingSpeed = std::min(Length(speed), 70.f);
	} 
    else if (mViewBobbingState == 1) 
    {
		// Stop animation
		mViewBobbingState = 2;
		mViewBobbingSpeed = 60;
	}

	mWieldMgr->OnUpdate(0, 0);
}

void PlayerCamera::UpdateViewingRange()
{
	float viewingRange = Settings::Get()->GetFloat("viewing_range");

	// Ignore near_plane setting on all other platforms to prevent abuse
    float upFov, aspectRatio, dMin, dMax;
    mCameraNode->Get()->GetFrustum(upFov, aspectRatio, dMin, dMax);

	mDrawControl->wantedRange = (float)std::fmin(AdjustDistance((short)viewingRange, GetFovMax()), 4000);
	if (mDrawControl->rangeAll) 
    {
        mCameraNode->Get()->SetFrustum(upFov, aspectRatio, 0.1f * BS, 100000.0);
		return;
	}
    mCameraNode->Get()->SetFrustum(upFov, aspectRatio, 0.1f * BS, 
        (viewingRange < 2000) ? 2000 * BS : viewingRange * BS);
}

void PlayerCamera::SetDigging(int button)
{
	if (mDiggingButton == -1)
		mDiggingButton = button;
}

void PlayerCamera::Wield(const ItemStack& item)
{
	if (item.name != mWieldItemNext.name ||
        item.metadata != mWieldItemNext.metadata)
    {
		mWieldItemNext = item;
		if (mWieldChangeTimer > 0)
			mWieldChangeTimer = -mWieldChangeTimer;
		else if (mWieldChangeTimer == 0)
			mWieldChangeTimer = -0.001f;
	}
}

void PlayerCamera::DrawWieldedTool(Matrix4x4<float>* translation)
{
	// Clear Z buffer so that the wielded tool stays in front of world geometry
    Renderer::Get()->ClearDepthBuffer();

	// Draw the wielded node (in a separate scene manager)
	std::shared_ptr<CameraNode> activeCamera = mWieldMgr->GetActiveCamera();
    activeCamera->Get()->SetFrustum(72.0, mAspect, 10, 1000);
	if (translation != NULL)
	{
        Transform cameraTransform;
        cameraTransform.SetMatrix(activeCamera->GetAbsoluteTransform() * *translation);
        activeCamera->GetRelativeTransform().SetTranslation(cameraTransform.GetTranslation());
        activeCamera->GetRelativeTransform().SetRotation(
            activeCamera->Get()->GetDVector() - activeCamera->GetAbsoluteTransform().GetTranslationW0());
		activeCamera->UpdateAbsoluteTransform();
	}
	mWieldMgr->OnRender();
}

void PlayerCamera::DrawNametags()
{
	Matrix4x4<float> transform = mCameraNode->Get()->GetProjectionViewMatrix();

    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	std::shared_ptr<BaseUIFont> font = skin->GetFont();

	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

	for (const Nametag* nametag : mNameTags) 
    {
		// Nametags are hidden in GenericVAO::updateNametag()
		Vector3<float> pos = 
            nametag->parentNode->GetAbsoluteTransform().GetTranslation() + nametag->pos * BS;
		Vector4<float> transformedPosition = HLift(pos, 1.f) * transform;
		if (transformedPosition[3] > 0) 
        {
			std::wstring nameTagColorless = ToWideString(nametag->text);
			Vector2<int> textSize = font->GetDimension(nameTagColorless);
			float zDiv = transformedPosition[3] == 0.0f ? 1.0f : 1.f / transformedPosition[3];
			Vector2<int> screenPos;
			screenPos[0] = (int)(screenSize[0] * (0.5f * transformedPosition[0] * zDiv + 0.5f) - textSize[0] / 2);
			screenPos[1] = (int)(screenSize[1] * (0.5f - transformedPosition[1] * zDiv * 0.5f) - textSize[1] / 2);
            RectangleShape<2, int> size;
            size.mExtent = textSize;
            size.mCenter = textSize / 2;
			size.mCenter += screenPos;
            RectangleShape<2, int> bgSize;
            bgSize.mExtent = Vector2<int>{textSize[0] + 4, textSize[1]} + screenPos;
            bgSize.mCenter = bgSize.mExtent / 2;
            bgSize.mCenter[0] -= 2;

            RectangleShape<2, int> clip;
            clip.mExtent = Vector2<int>{ (int)screenSize[0], (int)screenSize[1] };
            clip.mCenter = clip.mExtent / 2;

			auto bgcolor = nametag->GetBGColor(mShowNameTagBackgrounds);
			if (bgcolor.GetAlpha() != 0)
				skin->Draw2DRectangle(bgcolor, mVisual, bgSize, &clip);

			font->Draw(ToWideString(nametag->text), size, nametag->textcolor);
		}
	}
}

Nametag* PlayerCamera::AddNameTag(
    Node *parentNode, const std::string& text, 
    SColor textcolor, SColor bgcolor, const Vector3<float>& pos)
{
	Nametag *nametag = new Nametag(parentNode, text, textcolor, bgcolor, pos);
	mNameTags.push_back(nametag);
	return nametag;
}

void PlayerCamera::RemoveNameTag(Nametag *nametag)
{
	mNameTags.remove(nametag);
	delete nametag;
}
