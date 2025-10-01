//========================================================================
// MenuSettings.h
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

#ifndef MENUSETTINGS_H
#define MENUSETTINGS_H

#include "BaseMenu.h"

//---------------------------------------------------------------------------------------------------------------------
// Settings implementation class.
//---------------------------------------------------------------------------------------------------------------------
class MenuSettings : public BaseMenu
{
public:

    MenuSettings();

    virtual std::string GetForm();

    virtual bool Handle(std::string name, BaseUIElement* element);

protected:

    std::string GetLeaves();
    std::string GetFilters();
    std::string GetMipmaps();
    std::string GetAntialiasings();
    std::string GetNodeHighlightings();

    unsigned int GetLeaveOption();
    unsigned int GetFilterOption();
    unsigned int GetMipmapOption();
    unsigned int GetAntialiasingOption();
    unsigned int GetNodeHighlightingOption();

private:

    std::vector<std::string> mLeaves;
    std::vector<std::string> mFilters;
    std::vector<std::string> mMipmap;
    std::vector<std::string> mAntialiasing;
    std::vector<std::string> mNodeHighlighting;

    std::map<std::string, std::string> mLeavesOptions;
    std::map<std::string, std::string> mNodeHighlightingOptions;
    std::map<std::string, std::string> mFiltersOptions;
    std::map<std::string, std::string> mMipmapOptions;
    std::map<std::string, std::string> mAntialiasingOptions;
};

#endif