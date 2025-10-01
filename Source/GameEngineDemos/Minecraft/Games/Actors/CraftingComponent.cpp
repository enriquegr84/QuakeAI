//========================================================================
// CraftingComponent.h - CraftingComponent
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

#include "GameEngineStd.h"

#include "CraftingComponent.h"
#include "Craft.h"

#include "../Games.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"
#include "Game/Actor/Actor.h"

const char* CraftingComponent::Name = "CraftingComponent";

//---------------------------------------------------------------------------------------------------------------------
// CraftingComponent
//---------------------------------------------------------------------------------------------------------------------
CraftingComponent::CraftingComponent(void)
{
}

bool CraftingComponent::Init(tinyxml2::XMLElement* pData)
{
    mData = pData;
	return true;
}

tinyxml2::XMLElement* CraftingComponent::GenerateXml(void)
{
	tinyxml2::XMLDocument doc;

	// component element
	tinyxml2::XMLElement* pComponentElement = doc.NewElement(GetName());
	return pComponentElement;
}


void CraftingComponent::Update(float deltaMs)
{

}


bool CraftingComponent::ReadCraftReplacements(tinyxml2::XMLElement* pRecipe, CraftReplacements& replacements)
{
    return true;
}

bool CraftingComponent::ReadCraftRecipeShapeless(tinyxml2::XMLElement* pRecipe, std::vector<std::string>& recipe)
{
    std::string recipes = Trim(pRecipe->FirstChild()->Value());
    for (auto recipeDefinition : StringSplit(recipes, ','))
        recipe.push_back(recipeDefinition);

    return true;
}

bool CraftingComponent::ReadCraftRecipeShaped(tinyxml2::XMLElement* pRecipe, int& width, std::vector<std::string>& recipe)
{
    std::string recipes = Trim(pRecipe->FirstChild()->Value());
    for (auto recipeDefinition : StringSplit(recipes, ','))
        recipe.push_back(recipeDefinition);

    width = !recipe.empty();
    return width;
}


int CraftingComponent::RegisterCraft(Environment* env)
{
    BaseWritableCraftManager* craftManager =
        static_cast<BaseWritableCraftManager*>(env->GetCraftManager());

    tinyxml2::XMLNode* pNode = mData->Parent();
    tinyxml2::XMLElement* pActorElement = pNode->ToElement();

    /*
        CraftDefinitionShaped
    */
    if (pActorElement->Attribute("type") == std::string("shaped"))
    {
        std::string output = "";
        tinyxml2::XMLElement* pOutput = mData->FirstChildElement("Output");
        if (pOutput)
            output = pOutput->FirstChild()->Value();
        if (output.empty())
            LogError("Crafting definition is missing an output");

        int width = 0;
        std::vector<std::string> recipe;
        tinyxml2::XMLElement* pRecipe = mData->FirstChildElement("Recipe");
        if (!pRecipe)
            LogError("Crafting definition is missing a recipe (output=" + output + ")");

        if (!ReadCraftRecipeShaped(pRecipe, width, recipe))
            LogError("Invalid crafting recipe (output=" + output + ")");

        CraftReplacements replacements;
        tinyxml2::XMLElement* pReplacements = mData->FirstChildElement("Replacements");
        if (!pReplacements)
            if (!ReadCraftReplacements(pReplacements, replacements))
                LogError("Invalid replacements (output=" + output + ")");

        Craft* craftShaped = new CraftShaped(output, width, recipe, replacements);
        craftManager->RegisterCraft(craftShaped, env);
    }
    /*
        CraftDefinitionShapeless
    */
    else if (pActorElement->Attribute("type") == std::string("shapeless"))
    {
        std::string output = "";
        tinyxml2::XMLElement* pOutput = mData->FirstChildElement("Output");
        if (pOutput)
            output = pOutput->FirstChild()->Value();
        if (output.empty())
            LogError("Crafting definition (shapeless) is missing an output");

        std::vector<std::string> recipe;
        tinyxml2::XMLElement* pRecipe = mData->FirstChildElement("Recipe");
        if (!pRecipe)
            LogError("Crafting definition (shapeless) is missing a recipe (output=" + output + ")");

        if (!ReadCraftRecipeShapeless(pRecipe, recipe))
            LogError("Invalid crafting recipe (output=" + output + ")");

        CraftReplacements replacements;
        tinyxml2::XMLElement* pReplacements = mData->FirstChildElement("Replacements");
        if (!pReplacements)
            if (!ReadCraftReplacements(pReplacements, replacements))
                LogError("Invalid replacements (output=\"" + output + "\")");

        Craft* craftShapeless = new CraftShapeless(output, recipe, replacements);
        craftManager->RegisterCraft(craftShapeless, env);
    }
    /*
        CraftDefinitionToolRepair
    */
    else if (pActorElement->Attribute("type") == std::string("toolrepair"))
    {
        float additionalWear = 0.f;
        tinyxml2::XMLElement* pAdditionalWear = mData->FirstChildElement("AdditionalWear");
        if (pAdditionalWear)
            additionalWear = (float)atof(pAdditionalWear->FirstChild()->Value());

        Craft* craftToolRepair = new CraftToolRepair(additionalWear);
        craftManager->RegisterCraft(craftToolRepair, env);
    }
    /*
        CraftDefinitionCooking
    */
    else if (pActorElement->Attribute("type") == std::string("cooking"))
    {
        std::string output = "";
        tinyxml2::XMLElement* pOutput = mData->FirstChildElement("Output");
        if (pOutput)
            output = pOutput->FirstChild()->Value();
        if (output.empty())
            LogError("Crafting definition (cooking) is missing an output");

        std::string recipe = "";
        tinyxml2::XMLElement* pRecipe = mData->FirstChildElement("Recipe");
        if (pRecipe)
            recipe = pRecipe->FirstChild()->Value();
        if (recipe.empty())
            LogError("Crafting definition (cooking) is missing a recipe (output=" + recipe + ")");

        float cooktime = 3.f;
        tinyxml2::XMLElement* pCookTime = mData->FirstChildElement("CookTime");
        if (pCookTime)
            cooktime = (float)atof(pCookTime->FirstChild()->Value());

        CraftReplacements replacements;
        tinyxml2::XMLElement* pReplacements = mData->FirstChildElement("Replacements");
        if (!pReplacements)
            if (!ReadCraftReplacements(pReplacements, replacements))
                LogError("Invalid replacements (cooking output=" + output + ")");

        Craft* craftCooking = new CraftCooking(output, recipe, cooktime, replacements);
        craftManager->RegisterCraft(craftCooking, env);
    }
    /*
        CraftDefinitionFuel
    */
    else if (pActorElement->Attribute("type") == std::string("fuel"))
    {
        std::string recipe = "";
        tinyxml2::XMLElement* pRecipe = mData->FirstChildElement("Recipe");
        if (pRecipe)
            recipe = pRecipe->FirstChild()->Value();
        if (recipe.empty())
            LogError("Crafting definition (fuel) is missing a recipe");

        float burntime = 1.f;
        tinyxml2::XMLElement* pBurnTime = mData->FirstChildElement("BurnTime");
        if (pBurnTime)
            burntime = (float)atof(pBurnTime->FirstChild()->Value());


        CraftReplacements replacements;
        tinyxml2::XMLElement* pReplacements = mData->FirstChildElement("Replacements");
        if (!pReplacements)
            if (!ReadCraftReplacements(pReplacements, replacements))
                LogError("Invalid replacements (fuel recipe=" + recipe + ")");

        Craft* craftFuel = new CraftFuel(recipe, burntime, replacements);
        craftManager->RegisterCraft(craftFuel, env);
    }
    else
    {
        LogError("Unknown crafting definition type: " + 
            std::string(pActorElement->Attribute("type")));
    }

    return 0; /* number of results */
}