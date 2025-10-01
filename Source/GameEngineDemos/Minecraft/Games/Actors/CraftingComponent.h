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

#ifndef CRAFTINGCOMPONENT_H
#define CRAFTINGCOMPONENT_H

#include "Game/Actor/ActorComponent.h"

class Environment;
class Craft;
struct CraftReplacements;

//---------------------------------------------------------------------------------------------------------------------
// Crafting Component implementation class.
//---------------------------------------------------------------------------------------------------------------------
class CraftingComponent : public ActorComponent
{
public:

	static const char* Name;
	virtual const char* GetName() const { return Name; }

    CraftingComponent(void);

	virtual bool Init(tinyxml2::XMLElement* pData) override;
	virtual tinyxml2::XMLElement* GenerateXml(void) override;
	virtual void Update(float deltaMs) override;

    int RegisterCraft(Environment* env);

protected:

    bool ReadCraftReplacements(tinyxml2::XMLElement* pRecipe, CraftReplacements& replacements);
    bool ReadCraftRecipeShapeless(tinyxml2::XMLElement* pRecipe, std::vector<std::string>& recipe);
    bool ReadCraftRecipeShaped(tinyxml2::XMLElement* pRecipe, int& width, std::vector<std::string>& recipe);
    
private:

    tinyxml2::XMLElement* mData;
};

#endif