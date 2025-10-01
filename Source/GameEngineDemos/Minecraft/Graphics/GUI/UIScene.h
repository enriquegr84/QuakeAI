/*
Minetest
Copyright (C) 2020 Jean-Patrick Guerrero <jeanpatrick.guerrero@gmail.com>

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

#ifndef UISCENE_H
#define UISCENE_H

#include "../../MinecraftStd.h"

#include "Graphic/UI/Element/UIElement.h"
#include "Graphic/UI/Style.h"

#include "Graphic/Scene/Scene.h"
#include "Graphic/Scene/Element/CameraNode.h"
#include "Graphic/Scene/Element/AnimatedMeshNode.h"
#include "Graphic/Scene/Scene.h"

#include "Graphic/Scene/Mesh/AnimatedMesh.h"

#include "Graphic/Resource/Color.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class UIScene : public BaseUIElement
{
public:

    //! constructor
    UIScene(Scene* pScene, BaseUI* ui, int id, RectangleShape<2, int> rectangle);

	~UIScene();

	std::shared_ptr<AnimatedMeshNode> SetMesh(std::shared_ptr<AnimatedMesh> mesh = nullptr);
	void SetTexture(unsigned int idx, std::shared_ptr<Texture2> texture);
	void SetBackgroundColor(const SColor& color) noexcept { mBGColor = color; };
	void SetFrameLoop(int begin, int end);
	void SetAnimationSpeed(float speed);
	void EnableMouseControl(bool enable) noexcept { mMouseCtrl = enable; };
	void SetRotation(Vector2<float> rot) noexcept { mCustomRot = rot; };
	void EnableContinuousRotation(bool enable) noexcept { mInfRot = enable; };
	void SetStyles(const std::array<Style, Style::NUM_STATES> &styles);

	virtual void Draw();
	virtual bool OnEvent(const Event& evt);

private:
	void CalculateOptimalDistance();
	void UpdateTargetPosition();
	void UpdateCamera(std::shared_ptr<Node> target);
	void SetCameraRotation(EulerAngles<float> rot);
	// return true indicates that the rotation was corrected
	bool CorrectBounds(EulerAngles<float>& rot);
	void CameraLoop();

	void UpdateCameraPosition();
    EulerAngles<float> GetCameraRotation() const;
	void RotateCamera(const EulerAngles<float>& rotation);

    BaseUI* mUI;
	Scene* mScene;

    std::shared_ptr<Visual> mVisual;
	std::shared_ptr<CameraNode> mCamera;
	std::shared_ptr<AnimatedMeshNode> mMeshNode = nullptr;

	float mCameraDistance = 50.f;

	unsigned int mLastTime = 0;

	Vector3<float> mCameraPos;
	Vector3<float> mTargetPos;
	Vector3<float> mLastTargetPos;
	// Cursor positions
	Vector2<float> mCursorPos;
	Vector2<float> mCursorLastPos;
	// Initial rotation
	Vector2<float> mCustomRot;

	bool mMouseCtrl = true;
	bool mUpdateCamera = false;
	bool mInfRot = false;
	bool mInitialRotation = true;

	SColor mBGColor = 0;
};


#endif