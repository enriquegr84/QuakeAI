//========================================================================
// MinecraftActorFactory.cpp : Creates actors from components
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
//    http://code.google.com/p/GameEngine/
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

 
#include "MinecraftActorFactory.h"

#include "Games/Games.h"

#include "Games/Environment/LogicEnvironment.h"

#include "Games/Actors/ABMComponent.h"
#include "Games/Actors/ItemComponent.h"
#include "Games/Actors/VisualComponent.h"
#include "Games/Actors/CraftingComponent.h"

#include "Game/Actor/TransformComponent.h"
#include "Game/Actor/ActorComponent.h"
#include "Game/GameLogic.h"

#include "Core/IO/XmlResource.h"

//---------------------------------------------------------------------------------------------------------------------
MinecraftActorFactory::MinecraftActorFactory(void) : ActorFactory()
{
    mComponentFactory.Register<ABMComponent>(ActorComponent::GetIdFromName(ABMComponent::Name));
    mComponentFactory.Register<ItemComponent>(ActorComponent::GetIdFromName(ItemComponent::Name));
    mComponentFactory.Register<VisualComponent>(ActorComponent::GetIdFromName(VisualComponent::Name));
    mComponentFactory.Register<CraftingComponent>(ActorComponent::GetIdFromName(CraftingComponent::Name));
}

std::shared_ptr<ActorComponent> MinecraftActorFactory::CreateComponent(
	std::shared_ptr<Actor> pActor, tinyxml2::XMLElement* pData)
{
	return ActorFactory::CreateComponent(pActor, pData);
}


std::vector<std::shared_ptr<Actor>> MinecraftActorFactory::CreateMods(LogicEnvironment* env,
    const wchar_t* modResource, const Transform* pInitialTransform)
{
    std::vector<std::shared_ptr<Actor>> actors;

	// Grab the root XML node
	tinyxml2::XMLElement* pRoot = XmlResourceLoader::LoadAndReturnRootXMLElement(modResource);
	if (!pRoot)
	{
		LogError(L"Failed to create mods from resource: " + std::wstring(modResource));
		return actors;
	}

    tinyxml2::XMLElement* pActorNode = pRoot->FirstChildElement();

    // Loop through each child element and load the component
    for (; pActorNode; pActorNode = pActorNode->NextSiblingElement())
    {
        bool initialTransformSet = false;
        tinyxml2::XMLElement* pNode = pActorNode->FirstChildElement();

        if (pActorNode->Attribute("resource"))
        {
            std::wstring actorResource = ToWideString(pActorNode->Attribute("resource"));
            pNode = XmlResourceLoader::LoadAndReturnRootXMLElement(actorResource.c_str());
            pNode = pNode->FirstChildElement();
        }

        // create the actor instance
        ActorId nextActorId = GameLogic::Get()->GetNewActorID();
        std::shared_ptr<Actor> pActor(new Actor(nextActorId));
        if (!pActor->Init(pActorNode))
        {
            LogError(L"Failed to initialize actor id: " + std::to_wstring(nextActorId) +
               L" in " + std::wstring(modResource));
            continue;
        }

        // Loop through each child element and load the component
        for (; pNode; pNode = pNode->NextSiblingElement())
        {
            std::shared_ptr<ActorComponent> pComponent(CreateComponent(pActor, pNode));
            if (!pComponent)
            {
                //	If an error occurs, we kill the actor and bail. We could keep going, 
                //	but the actor is will only be partially complete so it's not worth it.  
                //	Note that the pActor instance will be destroyed because it will fall 
                //	out of scope with nothing else pointing to it.
                nextActorId = INVALID_ACTOR_ID;
                break;
            }
        }

        if (nextActorId == INVALID_ACTOR_ID)
            continue;

        if (pActorNode->Attribute("resource"))
        {
            std::shared_ptr<ItemComponent> pItemComponent(
                pActor->GetComponent<ItemComponent>(ItemComponent::Name).lock());
            if (pItemComponent)
            {
                ContentFeatures cFeatures;
                pItemComponent->ParseContentFeatures(cFeatures);

                NodeManager* nodeMgr = env->GetNodeManager();
                uint16_t id = nodeMgr->Set(cFeatures.name, cFeatures);
            }

            tinyxml2::XMLElement* overrides = pActorNode->FirstChildElement();
            if (overrides)
                ModifyActor(pActor, pActorNode);
        }

        std::shared_ptr<ItemComponent> pItemComponent(
            pActor->GetComponent<ItemComponent>(ItemComponent::Name).lock());
        if (pItemComponent)
            pItemComponent->RegisterItem(env);

        std::shared_ptr<CraftingComponent> pCraftingComponent =
            pActor->GetComponent<CraftingComponent>(CraftingComponent::Name).lock();
        if (pCraftingComponent)
            pCraftingComponent->RegisterCraft(env);

        std::shared_ptr<ABMComponent> pABMComponent =
            pActor->GetComponent<ABMComponent>(ABMComponent::Name).lock();
        if (pABMComponent)
            pABMComponent->RegisterABM(env);

        //	This is a bit of a hack to get the initial transform of the transform component 
        //	set before the other components (like PhysicsComponent) read it.
        std::shared_ptr<TransformComponent> pTransformComponent(
            pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
        if (pInitialTransform && pTransformComponent)
        {
            pTransformComponent->SetRotation(pInitialTransform->GetRotation());
            pTransformComponent->SetPosition(pInitialTransform->GetTranslation());
        }

        // Now that the actor has been fully created, run the post init phase
        pActor->PostInit();

        actors.push_back(pActor);
    }

    return actors;
}

std::shared_ptr<LogicPlayer> MinecraftActorFactory::CreatePlayer(BaseItemManager* itemMgr, const char* name,
    const wchar_t* actorResource, tinyxml2::XMLElement* overrides, const Transform* pInitialTransform)
{
    // Grab the root XML node
    tinyxml2::XMLElement* pRoot = XmlResourceLoader::LoadAndReturnRootXMLElement(actorResource);
    if (!pRoot)
    {
        LogError(L"Failed to create actor from resource: " + std::wstring(actorResource));
        return std::shared_ptr<LogicPlayer>();
    }

    // create the actor instance
    std::shared_ptr<LogicPlayer> pActor(new LogicPlayer(name, itemMgr));
    if (!pActor->Init(pRoot))
    {
        LogError(L"Failed to initialize actor: " + std::wstring(actorResource));
        return std::shared_ptr<LogicPlayer>();
    }

    bool initialTransformSet = false;
    tinyxml2::XMLElement* pNode = pRoot->FirstChildElement();

    // Loop through each child element and load the component
    for (; pNode; pNode = pNode->NextSiblingElement())
    {
        std::shared_ptr<ActorComponent> pComponent(CreateComponent(pActor, pNode));
        if (!pComponent)
        {
            //	If an error occurs, we kill the actor and bail.  We could keep going, 
            //	but the actor is will only be partially complete so it's not worth it.  
            //	Note that the pActor instance will be destroyed because it will fall 
            //	out of scope with nothing else pointing to it.
            return std::shared_ptr<LogicPlayer>();
        }
    }

    if (overrides)
    {
        ModifyActor(pActor, overrides);
    }

    //	This is a bit of a hack to get the initial transform of the transform component 
    //	set before the other components (like PhysicsComponent) read it.
    std::shared_ptr<TransformComponent> pTransformComponent(
        pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
    if (pInitialTransform && pTransformComponent)
    {
        pTransformComponent->SetRotation(pInitialTransform->GetRotation());
        pTransformComponent->SetPosition(pInitialTransform->GetTranslation());
    }

    // Now that the actor has been fully created, run the post init phase
    pActor->PostInit();

    return pActor;
}