//========================================================================
// ABMComponent.h - ABMComponent
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

#include "ABMComponent.h"

#include "../Games.h"

#include "../Environment/LogicEnvironment.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"
#include "Game/Actor/Actor.h"

const char* ABMComponent::Name = "ABMComponent";

//---------------------------------------------------------------------------------------------------------------------
// ABMComponent
//---------------------------------------------------------------------------------------------------------------------
ABMComponent::ABMComponent(void)
{
}

bool ABMComponent::Init(tinyxml2::XMLElement* pData)
{
	mData = pData;
	return true;
}

tinyxml2::XMLElement* ABMComponent::GenerateXml(void)
{
	tinyxml2::XMLDocument doc;

	// component element
	tinyxml2::XMLElement* pComponentElement = doc.NewElement(GetName());
	return pComponentElement;
}


void ABMComponent::Update(float deltaMs)
{

}


int ABMComponent::RegisterABM(LogicEnvironment* env)
{
	std::vector<std::string> triggerContents;
	tinyxml2::XMLElement* pNodenames = mData->FirstChildElement("Nodenames");
	if (pNodenames)
		triggerContents = StringSplit(std::string(pNodenames->FirstChild()->Value()), ',');

	std::vector<std::string> requiredNeighbors;
	tinyxml2::XMLElement* pNeighbors = mData->FirstChildElement("Neighbors");
	if (pNeighbors)
		requiredNeighbors = StringSplit(std::string(pNeighbors->FirstChild()->Value()), ',');

	float triggerInterval = 10.0;
	tinyxml2::XMLElement* pInterval = mData->FirstChildElement("Interval");
	if (pInterval)
		triggerInterval = (float)atof(pInterval->FirstChild()->Value());

	int triggerChance = 50;
	tinyxml2::XMLElement* pChance = mData->FirstChildElement("Chance");
	if (pChance)
		triggerChance = atoi(pChance->FirstChild()->Value());

	bool simpleCatchUp = true;
	tinyxml2::XMLElement* pCatchup = mData->FirstChildElement("Catchup");
	if (pCatchup)
		simpleCatchUp = atoi(pCatchup->FirstChild()->Value());

	std::string action;
	tinyxml2::XMLElement* pAction = mData->FirstChildElement("Action");
	if (pAction)
		action = pAction->FirstChild()->Value();
	
	ABM* abm = new ABM(GameLogic::Get()->GetNewActorID(), triggerContents, 
		requiredNeighbors, triggerInterval, triggerChance, simpleCatchUp);
	env->AddActiveBlockModifier(abm);

    return 0; /* number of results */
}