// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "NodeAnimatorRotation.h"

#include "Core/OS/OS.h"

//! constructor
NodeAnimatorRotation::NodeAnimatorRotation(unsigned int time, const Vector4<float>& axis, float rotationSpeed)
	: mRotationSpeed(rotationSpeed), mStartTime(time), mRotationAxis(axis)
{

}

//! animates a scene node
void NodeAnimatorRotation::AnimateNode(Scene* pScene, Node* node, unsigned int timeMs)
{
	if (node)
	{
		const unsigned int diffTime = timeMs - mStartTime;

		if (diffTime != 0)
		{
			// This is the child node's rotation.
			AxisAngle<4, float> localRotation;
			node->GetRelativeTransform().GetRotation(localRotation);
			localRotation.mAngle *= Dot(localRotation.mAxis, mRotationAxis) >= 0 ? 1 : -1;
			localRotation.mAngle += mRotationSpeed * 0.1f * diffTime * (float)GE_C_DEG_TO_RAD;

			Matrix4x4<float> rotation = Rotation<4, float>(AxisAngle<4, float>(mRotationAxis, localRotation.mAngle));
			node->GetRelativeTransform().SetRotation(rotation);
			node->UpdateAbsoluteTransform();

			mStartTime=timeMs;
		}
	}
}

NodeAnimator* NodeAnimatorRotation::CreateClone(Node* node)
{
	NodeAnimatorRotation* newAnimator = 
		new NodeAnimatorRotation(mStartTime, mRotationAxis, mRotationSpeed);

	return newAnimator;
}

