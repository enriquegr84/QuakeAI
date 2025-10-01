//========================================================================
// About.cpp
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

#include "About.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"

//---------------------------------------------------------------------------------------------------------------------
// About
//---------------------------------------------------------------------------------------------------------------------
About::About() : BaseMenu()
{

}

std::string About::GetForm()
{
    std::string logoFile = "art/quake/textures/logo.png";
    std::string versionProject = "Quake 1.0-dev";

    std::string form =
        "size[12.000000,5.400000,false]" + BaseMenu::GetForm() +
        "5;true;false]image[0.75,0.5;2.2,2.2;" + logoFile + "]"
        "style[label_button;border=false]"
        "button[0.5,2;2.5,2;label_button;" + versionProject + "]"
        "button[0.75,2.75;2,2;homepage;quake.net]"
        "tablecolumns[color;text]"
        "tableoptions[background=#00000000;highlight=#00000000;border=false]"
        "table[3.5,-0.25;8.5,6.05;list_credits;"
        "#FFFF00,Core Developers,,Enrique Gonzalez,;1]";

    //Render information
    form +=
        "label[0.75,4.9;Active renderer:\n" +
        Settings::Get()->Get("video_driver") + "]"
        "tooltip[userdata;Opens the directory that contains user-provided worlds, games, mods,\n" 
        "and texture packs in a file manager / explorer.]"
        "button[0,4;3.5,1;userdata;Open User Data Directory]";

    return form;
}

bool About::Handle(std::string name, BaseUIElement* element)
{
    return true;
}
