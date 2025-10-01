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

#ifndef PLAYERCAMERA_H
#define PLAYERCAMERA_H

#include "WieldMesh.h"

#include "../Games/Actors/Inventory.h"

struct MapDrawControl;

class VisualPlayer;
class BaseUI;

struct Nametag
{
	Node* parentNode;
	std::string text;
	SColor textcolor;
	SColor bgcolor;
	Vector3<float> pos;

	Nametag(Node* aParentNode, const std::string& text,
			const SColor& textcolor, const SColor& bgcolor,
			const Vector3<float>& pos) :
		parentNode(aParentNode), text(text), textcolor(textcolor),
		bgcolor(bgcolor), pos(pos)
	{
	}

	SColor GetBGColor(bool useFallback) const
	{
		if (bgcolor != NULL)
			return bgcolor;
		else if (!useFallback)
			return SColor(0, 0, 0, 0);
		else if (textcolor.GetLuminance() > 186)
			// Dark background for light text
			return SColor(50, 50, 50, 50);
		else
			// Light background for dark text
			return SColor(50, 255, 255, 255);
	}
};

enum PlayerCameraMode 
{
    CAMERA_MODE_FIRST,
    CAMERA_MODE_THIRD, 
    CAMERA_MODE_THIRD_FRONT
};

/*
	Visual camera class, manages the player and camera scene nodes, the viewing distance
	and performs view bobbing etc. It also displays the wielded tool in front of the
	first-person camera.
*/
class PlayerCamera
{
public:
	PlayerCamera(VisualEnvironment* env, 
        Scene* pScene, BaseUI* ui, MapDrawControl* drawControl);
	~PlayerCamera();

	// Get camera scene node.
	// It has the eye transformation, pitch and view bobbing applied.
	inline std::shared_ptr<CameraNode> GetCameraNode() const
	{
		return mCameraNode;
	}

	// Get the camera position (in absolute scene coordinates).
	// This has view bobbing applied.
	inline Vector3<float> GetPosition() const
	{
		return mCameraPosition;
	}

	// Returns the absolute position of the head SceneNode in the world
	inline Vector3<float> GetHeadPosition() const
	{
		return mHeadNode->GetAbsoluteTransform().GetTranslation();
	}

	// Get the camera direction (in absolute camera coordinates).
	// This has view bobbing applied.
	inline Vector3<float> GetDirection() const
	{
		return mCameraDirection;
	}

	// Get the camera offset
	inline Vector3<short> GetOffset() const
	{
		return mCameraOffset;
	}

	// Horizontal field of view
	inline float GetFovX() const
	{
		return mFovX;
	}

	// Vertical field of view
	inline float GetFovY() const
	{
		return mFovY;
	}

	// Get maximum of GetFovX() and GetFovY()
	inline float GetFovMax() const
	{
		return std::max(mFovX, mFovY);
	}

	// Notify about new logic-sent FOV and initialize smooth FOV transition
	void NotifyFovChange();

	// Checks if the constructor was able to create the scene nodes
	bool SuccessfullyCreated();

	// Step the camera: updates the viewing range and view bobbing.
	void Step(float dTime);

	// Update the camera from the local player's position.
	// busytime is used to adjust the viewing range.
	void Update(VisualPlayer* player, float frametime, float busytime, float toolReloadRatio);

	// Update render distance
	void UpdateViewingRange();

	// Start digging animation
	// Pass 0 for left click, 1 for right click
	void SetDigging(int button);

	// Replace the wielded item mesh
	void Wield(const ItemStack& item);

	// Draw the wielded tool.
	// This has to happen *after* the main scene is drawn.
	// Warning: This clears the Z buffer.
	void DrawWieldedTool(Matrix4x4<float>* translation=NULL);

	// Toggle the current camera mode
	void ToggleCameraMode() 
    {
		if (mCameraMode == CAMERA_MODE_FIRST)
			mCameraMode = CAMERA_MODE_THIRD;
		else if (mCameraMode == CAMERA_MODE_THIRD)
			mCameraMode = CAMERA_MODE_THIRD_FRONT;
		else
			mCameraMode = CAMERA_MODE_FIRST;
	}

	// Set the current camera mode
	inline void SetCameraMode(PlayerCameraMode mode)
	{
		mCameraMode = mode;
	}

	//read the current camera mode
	inline PlayerCameraMode GetCameraMode()
	{
		return mCameraMode;
	}

	Nametag* AddNameTag(Node* parentNode,
		const std::string& text, SColor textcolor,
		SColor bgcolor, const Vector3<float>& pos);

	void RemoveNameTag(Nametag *nametag);

	void DrawNametags();

	inline void AddArmInertia(float playerYaw);

protected:

	void UpdateCameraRotation(std::shared_ptr<CameraNode> camera,
		const Vector4<float>& cameraPos, const Vector4<float>& targetPos) const;

private:

    BaseUI* mUI;
    VisualEnvironment* mEnvironment;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;

	// Nodes
	std::shared_ptr<Node> mPlayerNode = nullptr;
	std::shared_ptr<Node> mHeadNode = nullptr;
	std::shared_ptr<CameraNode> mCameraNode = nullptr;

    std::shared_ptr<Scene> mWieldMgr = nullptr;
    std::shared_ptr<WieldMeshNode> mWieldNode = nullptr;

	// draw control
	MapDrawControl* mDrawControl;

	// Default Visual FOV (as defined by the "fov" setting)
	float mCacheFOV;

	// Absolute camera position
	Vector3<float> mCameraPosition = Vector3<float>::Zero();
	// Absolute camera direction
	Vector3<float> mCameraDirection = Vector3<float>::Zero();
	// PlayerCamera offset
	Vector3<short> mCameraOffset = Vector3<short>::Zero();

	// Logic-sent FOV variables
	bool mUpdateFOV = false;
	float mCurrentFOVDegrees, mOldFOVDegrees, mTargetFOVDegrees;

	// FOV transition variables
	bool mFovTransitionActive = false;
	float mFovDiff, mTransitionTime;

    Vector2<float> mWieldMeshOffset = Vector2<float>{ 55.0f, -35.0f };
	Vector2<float> mArmDir = Vector2<float>::Zero();
	Vector2<float> mCamVel = Vector2<float>::Zero();
	Vector2<float> mCamVelOld = Vector2<float>::Zero();
	Vector2<float> mLastCamPos = Vector2<float>::Zero();

	// Field of view and aspect ratio stuff
	float mAspect = 1.0f;
	float mFovX = 1.0f;
	float mFovY = 1.0f;

	// View bobbing animation frame (0 <= mViewBobbingAnim < 1)
	float mViewBobbingAnim = 0.0f;
	// If 0, view bobbing is off (e.g. player is standing).
	// If 1, view bobbing is on (player is walking).
	// If 2, view bobbing is getting switched off.
	int mViewBobbingState = 0;
	// Speed of view bobbing animation
	float mViewBobbingSpeed = 0.0f;
	// Fall view bobbing
	float mViewBobbingFall = 0.0f;

	// Digging animation frame (0 <= mDiggingAnim < 1)
	float mDiggingAnim = 0.0f;
	// If -1, no digging animation
	// If 0, left-click digging animation
	// If 1, right-click digging animation
	int mDiggingButton = -1;

	// Animation when changing wielded item
	float mWieldChangeTimer = 0.125f;
	ItemStack mWieldItemNext;

	PlayerCameraMode mCameraMode = CAMERA_MODE_FIRST;

	float mCacheFallBobbingAmount;
	float mCacheViewBobbingAmount;
	bool mArmInertia;

	std::list<Nametag*> mNameTags;
	bool mShowNameTagBackgrounds;
};

#endif