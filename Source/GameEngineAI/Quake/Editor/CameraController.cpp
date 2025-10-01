// CameraController.cpp - Controller class for the player 
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/gamecode4/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#include "../QuakeEvents.h"
#include "../QuakeApp.h"

#include "Core/Logger/Logger.h"

#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

#include "CameraController.h"

////////////////////////////////////////////////////
// CameraController Implementation
////////////////////////////////////////////////////


CameraController::CameraController(const std::shared_ptr<CameraNode>& camera,
	float initialYaw, float initialPitch, bool rotateWhenRButtonDown)
	: mCamera(camera), mEnabled(true)
{
	mYaw = (float)GE_C_RAD_TO_DEG * initialYaw;
	mPitch = (float)GE_C_RAD_TO_DEG * initialPitch;

	mMaxMoveSpeed = 300.0f;
	mMaxRotateSpeed = 180.0f;
	mMoveSpeed = 0.0f;
	mRotateSpeed = 0.0f;

	//Point cursor;
	System* system = System::Get();
	Vector2<unsigned int> cursorPosition = system->GetCursorControl()->GetPosition();
	mLastMousePos = Vector2<int>{ (int)cursorPosition[0], (int)cursorPosition[1] };

	memset(mKey, 0x00, sizeof(mKey));

	mMouseUpdate = true;
	mMouseRButtonDown = false;
	mMouseLButtonDown = false;
	mWheelRollDown = false;
	mWheelRollUp = false;
	mRotateWhenRButtonDown = rotateWhenRButtonDown;
}

//
// CameraController::OnMouseButtonDown		- Chapter 10, page 282
//
bool CameraController::OnMouseButtonDown(
	const Vector2<int> &mousePos, const int radius, const std::string &buttonName)
{
	if (buttonName == "PointerLeft")
	{
		mMouseLButtonDown = true;

		// We want mouse movement to be relative to the position
		// the cursor was at when the user first presses down on
		// the left button
		mLastMousePos = mousePos;
		return true;
	}
	else if (buttonName == "PointerRight")
	{
		mMouseRButtonDown = true;

		// We want mouse movement to be relative to the position
		// the cursor was at when the user first presses down on
		// the right button
		mLastMousePos = mousePos;
		System::Get()->GetCursorControl()->SetVisible(false);
		System::Get()->GetCursorControl()->SetPosition(0.5f, 0.5f);
		return true;
	}
	return false;
}

bool CameraController::OnMouseButtonUp(
	const Vector2<int> &mousePos, const int radius, const std::string &buttonName)
{
	if (buttonName == "PointerLeft")
	{
		mMouseLButtonDown = false;
		return true;
	}
	else if (buttonName == "PointerRight")
	{
		mMouseRButtonDown = false;
		System::Get()->GetCursorControl()->SetVisible(true);
		System::Get()->GetCursorControl()->SetPosition(0.5f, 0.5f);
		return true;
	}
	return false;
}


//  class CameraController::OnMouseMove		- Chapter 10, page 282

bool CameraController::OnMouseMove(const Vector2<int> &mousePos, const int radius)
{
	// There are two modes supported by this controller.
	if (mMouseUpdate)
	{
		mRotateSpeed = mMaxRotateSpeed;

		// Rotate the view only when the right mouse button is down
		// Only look around if the right button is down
		if (mRotateWhenRButtonDown && mMouseRButtonDown)
		{
			Vector2<unsigned int> center(Renderer::Get()->GetScreenSize() / (unsigned int)2);
			Vector2<unsigned int> cursorPos = System::Get()->GetCursorControl()->GetPosition();
			Vector2<int> dist = { (int)center[0] - (int)cursorPos[0], (int)cursorPos[1] - (int)center[1] };

			mYaw += (dist[0] / (float)System::Get()->GetWidth()) * mRotateSpeed;
			mPitch += (dist[1] / (float)System::Get()->GetHeight()) * mRotateSpeed;

			if (dist[0] != 0 || dist[1] != 0)
				System::Get()->GetCursorControl()->SetPosition(0.5f, 0.5f);
		}
		else
		{
			Vector2<unsigned int> cursorPos = System::Get()->GetCursorControl()->GetPosition();
			mLastMousePos = Vector2<int>{ (int)cursorPos[0], (int)cursorPos[1] };
		}
	}

	return true;
}

//  class CameraController::OnUpdate			- Chapter 10, page 283

void CameraController::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	// Special case, mouse is whipped outside of window before it can update.
	if (mEnabled)
	{
		System* system = System::Get();
		Vector2<unsigned int> cursorPosition = system->GetCursorControl()->GetPosition();
		Vector2<int> mousePosition{ (int)cursorPosition[0], (int)cursorPosition[1] };

		Renderer* renderer = Renderer::Get();
		Vector2<unsigned int> screenSize(renderer->GetScreenSize());
		RectangleShape<2, int> screenRectangle;
		screenRectangle.mCenter[0] = screenSize[0] / 2;
		screenRectangle.mCenter[1] = screenSize[1] / 2;
		screenRectangle.mExtent[0] = (int)screenSize[0];
		screenRectangle.mExtent[1] = (int)screenSize[1];

		// Only if we are moving outside quickly.
		bool reset = !screenRectangle.IsPointInside(mousePosition);

		if (reset)
		{
			// Force a reset.
			mMouseUpdate = false;
			cursorPosition = system->GetCursorControl()->GetPosition();
			mLastMousePos = Vector2<int>{ (int)cursorPosition[0], (int)cursorPosition[1] };
		}
		else mMouseUpdate = true;
	}

	//Handling rotation as a result of mouse position
	{
		mPitch = std::max(1.f, std::min(179.f, mPitch));

		// Calculate the new rotation matrix from the camera
		// yaw and pitch (zrotate and xrotate).
		Matrix4x4<float> yawRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), mYaw * (float)GE_C_DEG_TO_RAD));
		Matrix4x4<float> pitchRotation = Rotation<4, float>(
			AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), mPitch * (float)GE_C_DEG_TO_RAD));

		mAbsoluteTransform.SetRotation(yawRotation * pitchRotation);
		mAbsoluteTransform.SetTranslation(mCamera->GetAbsoluteTransform().GetTranslation());
	}

	bool isTranslating = false;
	Vector4<float> atWorld = Vector4<float>::Zero();
	Vector4<float> rightWorld = Vector4<float>::Zero();
	Vector4<float> upWorld = Vector4<float>::Zero();

	if (mKey[KEY_KEY_W] || mKey[KEY_KEY_S])
	{
		// This will give us the "look at" vector 
		// in world space - we'll use that to move
		// the camera.
		atWorld = Vector4<float>::Unit(AXIS_Y); // forward vector
#if defined(GE_USE_MAT_VEC)
		atWorld = mAbsoluteTransform * atWorld;
#else
		atWorld = atWorld * mAbsoluteTransform;
#endif

		if (mKey[KEY_KEY_S])
			atWorld *= -1.f;

		isTranslating = true;
	}

	if (mKey[KEY_KEY_A] || mKey[KEY_KEY_D])
	{
		// This will give us the "look right" vector 
		// in world space - we'll use that to move
		// the camera.
		rightWorld = -Vector4<float>::Unit(AXIS_X); // right vector
#if defined(GE_USE_MAT_VEC)
		rightWorld = mAbsoluteTransform * rightWorld;
#else
		rightWorld = rightWorld * mAbsoluteTransform;
#endif

		if (mKey[KEY_KEY_A])
			rightWorld *= -1.f;

		isTranslating = true;
	}


	if (mKey[KEY_SPACE] || mKey[KEY_KEY_C] || mKey[KEY_KEY_X])
	{
		//Unlike strafing, Up is always up no matter
		//which way you are looking
		upWorld = Vector4<float>::Unit(AXIS_Z); // up vector
#if defined(GE_USE_MAT_VEC)
		upWorld = mAbsoluteTransform * upWorld;
#else
		upWorld = upWorld * mAbsoluteTransform;
#endif

		if (!mKey[KEY_SPACE])
			upWorld *= -1.f;

		isTranslating = true;
	}

	if (mEnabled && isTranslating)
	{
		float elapsedTime = deltaMs / 1000.f;

		Vector4<float> direction = atWorld + rightWorld + upWorld;
		Normalize(direction);

		mMoveSpeed = mMaxMoveSpeed;
		direction *= mMoveSpeed * elapsedTime;
		Vector4<float> pos = mCamera->GetAbsoluteTransform().GetTranslationW0() + direction;
		mAbsoluteTransform.SetTranslation(pos);
	}

	// update transform matrix
	mCamera->GetRelativeTransform() = mAbsoluteTransform;

	mWheelRollDown = false;
	mWheelRollUp = false;
}
