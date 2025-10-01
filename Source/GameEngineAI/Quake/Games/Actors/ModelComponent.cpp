 
//========================================================================
// ModelComponent.cpp : classes that define model components of actors.
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

#include "ModelComponent.h"

#include "Graphic/Graphic.h"
#include "Core/Logger/Logger.h"

#include "Core/Event/Event.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/TransformComponent.h"

#include "Application/GameApplication.h"

const char* ModelComponent::Name = "ModelComponent";

//---------------------------------------------------------------------------------------------------------------------
// ModelComponent
//---------------------------------------------------------------------------------------------------------------------
ModelComponent::ModelComponent(void)
{
	mMaterialType = 0;
	mAnimationType = 0;
	mAnimationSpeed = 0.f;
}

bool ModelComponent::Init(tinyxml2::XMLElement* pData)
{
	float temp = 0;

	tinyxml2::XMLElement* pMesh = pData->FirstChildElement("Mesh");
	if (pMesh)
	{
		tinyxml2::XMLElement* pWeapon = pMesh->FirstChildElement("Weapon");
		if (pWeapon)
			mMeshes["Weapon"] = pWeapon->FirstChild()->Value();
		tinyxml2::XMLElement* pWeaponFlash = pMesh->FirstChildElement("Flash");
		if (pWeaponFlash)
			mMeshes["Flash"] = pWeaponFlash->FirstChild()->Value();
		tinyxml2::XMLElement* pWeaponBarrel = pMesh->FirstChildElement("Barrel");
		if (pWeaponBarrel)
			mMeshes["Barrel"] = pWeaponBarrel->FirstChild()->Value();
		tinyxml2::XMLElement* pWeaponHand = pMesh->FirstChildElement("Hand");
		if (pWeaponHand)
			mMeshes["Hand"] = pWeaponHand->FirstChild()->Value();
	}

	tinyxml2::XMLElement* pSound = pData->FirstChildElement("Sound");
	if (pSound)
	{
		tinyxml2::XMLElement* pPickup = pSound->FirstChildElement("Pickup");
		if (pPickup)
			mSounds["Pickup"] = pPickup->FirstChild()->Value();
		tinyxml2::XMLElement* pFlash = pSound->FirstChildElement("Flash");
		if (pFlash)
			mSounds["Flash"] = pFlash->FirstChild()->Value();
		tinyxml2::XMLElement* pFiring = pSound->FirstChildElement("Firing");
		if (pFiring)
			mSounds["Firing"] = pFiring->FirstChild()->Value();
		tinyxml2::XMLElement* pHit = pSound->FirstChildElement("Hit");
		if (pHit)
			mSounds["Hit"] = pHit->FirstChild()->Value();
		tinyxml2::XMLElement* pReady = pSound->FirstChildElement("Ready");
		if (pReady)
			mSounds["Ready"] = pReady->FirstChild()->Value();
		tinyxml2::XMLElement* pMissile = pSound->FirstChildElement("Missile");
		if (pMissile)
			mSounds["Missile"] = pMissile->FirstChild()->Value();
	}

	tinyxml2::XMLElement* pColor = pData->FirstChildElement("FlashColor");
	if (pColor)
	{
		float temp = 0;

		temp = pColor->FloatAttribute("r", temp);
		mFlashColor[0] = temp;

		temp = pColor->FloatAttribute("g", temp);
		mFlashColor[1] = temp;

		temp = pColor->FloatAttribute("b", temp);
		mFlashColor[2] = temp;

		temp = pColor->FloatAttribute("a", temp);
		mFlashColor[3] = temp;
	}

	tinyxml2::XMLElement* pIcon = pSound->FirstChildElement("Icon");
	if (pIcon)
		mIcon = pIcon->FirstChild()->Value();

	tinyxml2::XMLElement* pMaterial = pData->FirstChildElement("Material");
	if (pMaterial)
	{
		unsigned int type = 0;
		mMaterialType = pMaterial->IntAttribute("type", type);
	}

	tinyxml2::XMLElement* pAnimation = pData->FirstChildElement("Animation");
	if (pAnimation)
	{
		unsigned int type = 0;
		mAnimationType = pAnimation->IntAttribute("type", type);
	}

    return true;
}


void ModelComponent::PostInit(void)
{
	std::shared_ptr<EventDataNewRenderComponent> pEvent(
		new EventDataNewRenderComponent(mOwner->GetId(), GetSceneNode()));
	BaseEventManager::Get()->TriggerEvent(pEvent);
}


void ModelComponent::OnChanged(void)
{
	std::shared_ptr<EventDataModifiedRenderComponent> pEvent(
		new EventDataModifiedRenderComponent(mOwner->GetId()));
	BaseEventManager::Get()->TriggerEvent(pEvent);
}

tinyxml2::XMLElement* ModelComponent::GenerateXml(void)
{
	tinyxml2::XMLDocument doc;

	// component element
	tinyxml2::XMLElement* pComponentElement = doc.NewElement(GetName());

	// mesh
	tinyxml2::XMLElement* pMeshes = doc.NewElement("Mesh");
	for (auto const& mesh : mMeshes)
	{
		tinyxml2::XMLElement* pMesh = doc.NewElement(mesh.first.c_str());
		tinyxml2::XMLText* pMeshFile = doc.NewText(mesh.second.c_str());
		pMesh->LinkEndChild(pMeshFile);
	}
	pComponentElement->LinkEndChild(pMeshes);

	// sound
	tinyxml2::XMLElement* pSounds = doc.NewElement("Sound");
	for (auto const& sound : mSounds)
	{
		tinyxml2::XMLElement* pSound = doc.NewElement(sound.first.c_str());
		tinyxml2::XMLText* pSoundFile = doc.NewText(sound.second.c_str());
		pSound->LinkEndChild(pSoundFile);
	}
	pComponentElement->LinkEndChild(pSounds);

	tinyxml2::XMLElement* pIcon = doc.NewElement("Icon");
	tinyxml2::XMLText* pIconFile = doc.NewText(mIcon.c_str());
	pIcon->LinkEndChild(pIconFile);
	pComponentElement->LinkEndChild(pIcon);

	tinyxml2::XMLElement* pMaterial = doc.NewElement("Material");
	pMaterial->SetAttribute("type", std::to_string(mMaterialType).c_str());
	pComponentElement->LinkEndChild(pMaterial);

	// animations
	tinyxml2::XMLElement* pAnimation = doc.NewElement("Animation");
	pAnimation->SetAttribute("type", std::to_string(mAnimationType).c_str());
	pComponentElement->LinkEndChild(pAnimation);

	return pComponentElement;
}

void ModelComponent::Update(float deltaMs)
{

}

const std::shared_ptr<Node>& ModelComponent::GetSceneNode(void)
{
	if (!mSceneNode)
		mSceneNode = CreateSceneNode();
	return mSceneNode;
}

std::shared_ptr<Node> ModelComponent::CreateSceneNode(void)
{
    // get the transform component
    const std::shared_ptr<TransformComponent>& pTransformComponent(
		mOwner->GetComponent<TransformComponent>(TransformComponent::Name).lock());
	if (pTransformComponent)
	{
		const std::shared_ptr<ScreenElementScene>& pScene = GameApplication::Get()->GetHumanView()->mScene;
		Transform transform = pTransformComponent->GetTransform();

		std::shared_ptr<BaseMesh> mesh;

		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(mMeshes["Weapon"])));
		if (resHandle)
		{
			const std::shared_ptr<MeshResourceExtraData>& extra =
				std::static_pointer_cast<MeshResourceExtraData>(resHandle->GetExtra());
				
			std::shared_ptr<AnimateMeshMD3> meshMD3 = std::dynamic_pointer_cast<AnimateMeshMD3>(mesh);
			if (meshMD3)
			{
				std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
					std::dynamic_pointer_cast<AnimateMeshMD3>(extra->GetMesh());
				if (!animMeshMD3->GetMD3Mesh()->GetParent())
					meshMD3->GetMD3Mesh()->AttachChild(animMeshMD3->GetMD3Mesh());

				if (mMaterialType == MaterialType::MT_TRANSPARENT)
				{
					std::vector<std::shared_ptr<MD3Mesh>> animMeshes;
					animMeshMD3->GetMD3Mesh()->GetMeshes(animMeshes);

					for (std::shared_ptr<MD3Mesh> animMesh : animMeshes)
					{
						for (unsigned int i = 0; i < animMesh->GetMeshBufferCount(); ++i)
						{
							std::shared_ptr<Material> material = animMesh->GetMeshBuffer(i)->GetMaterial();
							material->mBlendTarget.enable = true;
							material->mBlendTarget.srcColor = BlendState::BM_ONE;
							material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_COLOR;
							material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
							material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

							material->mDepthBuffer = true;
							material->mDepthMask = DepthStencilState::MASK_ZERO;

							material->mFillMode = RasterizerState::FILL_SOLID;
							material->mCullMode = RasterizerState::CULL_NONE;

							material->mType = (MaterialType)mMaterialType;
						}
					}
				}
			}
			else
			{
				mesh = std::shared_ptr<BaseMesh>(extra->GetMesh());
				
				if (mMaterialType == MaterialType::MT_TRANSPARENT)
				{
					for (unsigned int i = 0; i < mesh->GetMeshBufferCount(); ++i)
					{
						std::shared_ptr<Material> material = mesh->GetMeshBuffer(i)->GetMaterial();
						material->mBlendTarget.enable = true;
						material->mBlendTarget.srcColor = BlendState::BM_ONE;
						material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_COLOR;
						material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
						material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

						material->mDepthBuffer = true;
						material->mDepthMask = DepthStencilState::MASK_ZERO;

						material->mFillMode = RasterizerState::FILL_SOLID;
						material->mCullMode = RasterizerState::CULL_NONE;

						material->mType = (MaterialType)mMaterialType;
					}
				}
			}
		}

		std::shared_ptr<Node> meshNode = nullptr;
		if (!mesh)
		{
			meshNode = std::shared_ptr<Node>();
			meshNode->GetRelativeTransform() = transform;
			meshNode->UpdateAbsoluteTransform();

			meshNode->SetMaterialType((MaterialType)mMaterialType);
		}
		else if (mesh->GetMeshType() == MT_STATIC)
		{
			// create an static mesh scene node with specified mesh.
			meshNode = pScene->AddStaticMeshNode(0, mesh, mOwner->GetId());
			if (meshNode)
			{
				meshNode->GetRelativeTransform() = transform;
				meshNode->UpdateAbsoluteTransform();

				meshNode->SetMaterialType((MaterialType)mMaterialType);
			}
		}
		else if (mesh->GetMeshType() == MT_NORMAL)
		{
			// create an mesh scene node with specified mesh.
			meshNode = pScene->AddMeshNode(0, mesh, mOwner->GetId());
			if (meshNode)
			{
				meshNode->GetRelativeTransform() = transform;
				meshNode->UpdateAbsoluteTransform();

				meshNode->SetMaterialType((MaterialType)mMaterialType);
			}
		}
        else if (mesh->GetMeshType() == MT_SKINNED || mesh->GetMeshType() == MT_ANIMATED)
		{
			// create an animated mesh scene node with specified animated mesh.
			meshNode = pScene->AddAnimatedMeshNode( 
				0, std::dynamic_pointer_cast<BaseAnimatedMesh>(mesh), mOwner->GetId());
			if (meshNode)
			{
				meshNode->GetRelativeTransform() = transform;
				meshNode->UpdateAbsoluteTransform();

				if (mAnimationType & NAT_ROTATION)
				{
					std::shared_ptr<NodeAnimator> anim = 0;
					anim = pScene->CreateRotationAnimator(Vector4<float>::Unit(2), 1.0f);
					meshNode->AttachAnimator(anim);
				}

				if (mAnimationType & NAT_FLY_STRAIGHT)
				{
					std::shared_ptr<NodeAnimator> anim = 0;
					anim = pScene->CreateFlyStraightAnimator(
						transform.GetTranslation() + Vector3<float>::Unit(2) * 5.f,
						transform.GetTranslation() - Vector3<float>::Unit(2) * 5.f,
						500, true, true);
					meshNode->AttachAnimator(anim);
				}

				meshNode->SetMaterialType((MaterialType)mMaterialType);
			}
		}
		return meshNode;
	}
	return std::shared_ptr<Node>();
}
