//========================================================================
// VisualComponent.h - VisualComponent
//
// Part of the GameCode4 Application
//
// GameCode4 is the sample application that encapsulates much of the source code
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

#include "VisualComponent.h"

#include "../Games.h"

#include "../../Graphics/ObjectMesh.h"
#include "../../Graphics/AnimatedObjectMesh.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"
#include "Game/Actor/Actor.h"

const char* VisualComponent::Name = "VisualComponent";

VisualComponent::VisualComponent(void)
{
	mMaterialType = 0;
	mAnimationType = 0;
	mAnimationSpeed = 0.f;
}

bool VisualComponent::Init(tinyxml2::XMLElement* pData)
{
	float temp = 0;

	tinyxml2::XMLElement* pMesh = pData->FirstChildElement("Mesh");
	if (pMesh)
	{
		std::string meshes = pMesh->FirstChild()->Value();
		meshes.erase(std::remove(meshes.begin(), meshes.end(), '\r'), meshes.end());
		meshes.erase(std::remove(meshes.begin(), meshes.end(), '\n'), meshes.end());
		meshes.erase(std::remove(meshes.begin(), meshes.end(), '\t'), meshes.end());
		size_t meshBegin = 0, meshEnd = 0;
		do
		{
			meshEnd = meshes.find(',', meshBegin);
			mMeshes.push_back(meshes.substr(meshBegin, meshEnd - meshBegin));

			meshBegin = meshEnd + 1;
		} while (meshEnd != std::string::npos);
	}

	tinyxml2::XMLElement* pTexture = pData->FirstChildElement("Texture");
	if (pTexture)
	{
		std::string textures = pTexture->FirstChild()->Value();
		textures.erase(std::remove(textures.begin(), textures.end(), '\r'), textures.end());
		textures.erase(std::remove(textures.begin(), textures.end(), '\n'), textures.end());
		textures.erase(std::remove(textures.begin(), textures.end(), '\t'), textures.end());
		size_t textureBegin = 0, textureEnd = 0;
		do
		{
			textureEnd = textures.find(',', textureBegin);
			mTextures.push_back(textures.substr(textureBegin, textureEnd - textureBegin));

			textureBegin = textureEnd + 1;
		} while (textureEnd != std::string::npos);
	}

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

	tinyxml2::XMLElement* pAnimations = pData->FirstChildElement("Animations");
	if (pAnimations)
	{
		float speed = 0.f;
		mAnimationSpeed = pAnimations->FloatAttribute("speed", speed);

		tinyxml2::XMLElement* pAnimation = pAnimations->FirstChildElement("Animation");
		for (; pAnimation; pAnimation = pAnimation->NextSiblingElement())
		{
			unsigned int type = 0;
			std::string animName = pAnimation->Attribute("name");

			short start = 0, end = 0;
			start = pAnimation->IntAttribute("start", start);
			end = pAnimation->IntAttribute("end", end);
			mAnimationFrames[animName] = { start, end };
		}
	}

	return true;
}

tinyxml2::XMLElement* VisualComponent::GenerateXml(void)
{
	tinyxml2::XMLDocument doc;

	// component element
	tinyxml2::XMLElement* pComponentElement = doc.NewElement(GetName());
	return pComponentElement;
}


void VisualComponent::Update(float deltaMs)
{

}