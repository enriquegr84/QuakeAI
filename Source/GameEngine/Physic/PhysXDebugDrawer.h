//========================================================================
// PhysXDebugDrawer.h - implements a physics debug drawer 
//
//========================================================================

#ifndef PHYSXDEBUGDRAWER_H
#define PHYSXDEBUGDRAWER_H

#include "GameEngineStd.h"

#include "Graphic/Scene/Hierarchy/Visual.h"

#include "PxPhysicsAPI.h"

using namespace physx;

//////////////////////////////////////////////////////////////////////////////
// class PhysXDebugDrawer
//
// PhysX uses this object to draw debug information.  This implementation
//   represent the current state of the physics simulation 
//
class PhysXDebugDrawer
{

public:

	PhysXDebugDrawer(PxScene* scene);

	// [mrmike] Added post press to read Settingss.xml to turn on physics debug options.
	void ReadSettings(tinyxml2::XMLElement *pRoot);

	void Render();

private:

	struct Vertex
	{
		Vector3<float> position;
		Vector4<float> color;
	};
	std::shared_ptr<VisualEffect> mEffect;

	PxScene* mScene = NULL;
};

#endif
