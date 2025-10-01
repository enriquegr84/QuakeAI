//========================================================================
// ModelComponent.h : classes that define model components of actors
// Author: David "Rez" Graham
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

#ifndef MODELCOMPONENT_H
#define MODELCOMPONENT_H

#include "GameEngineStd.h"

#include "Game/Actor/ActorComponent.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector4.h"
#include "Mathematic/Geometric/AlignedBox.h"

#include "Graphic/Resource/Color.h"

class Node;

//---------------------------------------------------------------------------------------------------------------------
// This class represents a model component.  
//---------------------------------------------------------------------------------------------------------------------
class ModelComponent : public ActorComponent
{
	std::map<std::string, std::string> mMeshes;
	std::map<std::string, std::string> mSounds;
	Vector4<float> mFlashColor;
	unsigned int mMaterialType;
	std::string mIcon;

	int mAnimationType;
	float mAnimationSpeed;

public:
	static const char *Name;
	virtual const char *GetName() const { return Name; }

	ModelComponent(void);

	virtual bool Init(tinyxml2::XMLElement* pData) override;
	virtual void PostInit(void) override;
	virtual void OnChanged(void) override;
	virtual tinyxml2::XMLElement* GenerateXml(void) override;
	virtual void Update(float deltaMs) override;

	std::map<std::string, std::string> GetMeshes() { return mMeshes; }
	std::map<std::string, std::string> GetSounds() { return mSounds; }
	std::string GetIcon() { return mIcon; }
	
	float GetAnimationSpeed() { return mAnimationSpeed; }
	void SetAnimationSpeed(float animSpeed) { mAnimationSpeed = animSpeed; }

protected:

	std::shared_ptr<Node> mSceneNode;

	const std::shared_ptr<Node>& GetSceneNode(void);

    std::shared_ptr<Node> CreateSceneNode(void);  // factory method to create the appropriate scene node

};

#endif
