//========================================================================
// Online.cpp
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

#include "Online.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"

//---------------------------------------------------------------------------------------------------------------------
// Online
//---------------------------------------------------------------------------------------------------------------------
Online::Online() : BaseMenu()
{

}

std::string Online::GetForm()
{
    std::string form = 
        "size[12.000000,5.400000,false]real_coordinates[true]" + BaseMenu::GetForm() +
        "2;true;false]field[0.25,0.25;7,0.75;te_search;;]container[7.25,0.25]"
        "image_button[0,0;0.75,0.75;art/minecraft/textures/search.png;btn_mp_search;]"
        "image_button[0.75,0;0.75,0.75;art/minecraft/textures/clear.png;btn_mp_clear;]"
        "image_button[1.5,0;0.75,0.75;art/minecraft/textures/refresh.png;btn_mp_refresh;]"
        "tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
        "container_end[]";

    form += "container[9.75,0]box[0,0;5.75,7;#666666]";

    // Address / Port
    form +=
        "label[0.25,0.35;Address]"
        "label[4.25,0.35;Port]"
        "field[0.25,0.5;4,0.75;te_address;;" +
        Settings::Get()->Get("address") + "]"
        "field[4.25,0.5;1.25,0.75;te_port;;" +
        Settings::Get()->Get("remote_port") + "]";

    // Name / Password
    form +=
        "label[0.25,1.55;Name]"
        "label[3,1.55;Password]"
        "field[0.25,1.75;2.75,0.75;te_name;;" +
        Settings::Get()->Get("name") + "]"
        "pwdfield[3,1.75;2.5,0.75;te_pwd;]";

    // Description background
    form +=
        "label[0.25,2.75;Server Description]"
        "box[0.25,3;5.25,2.75;#999999]";

    // Connect
    form +=
        "button[3,6;2.5,0.75;btn_mp_connect;Connect]"
        "textarea[0.25,3;5.25,2.75;;;]";

    form += "container_end[]";

    // Table
    form += "tablecolumns["
        "image,tooltip=Ping,"
        "0=art/minecraft/textures/blank.png,"
        "1=art/minecraft/textures/server_ping_4.png,"
        "2=art/minecraft/textures/server_ping_3.png,"
        "3=art/minecraft/textures/server_ping_2.png,"
        "4=art/minecraft/textures/server_ping_1.png,"
        "5=art/minecraft/textures/server_favorite.png,"
        "6=art/minecraft/textures/server_public.png,"
        "7=art/minecraft/textures/server_incompatible.png;"
        "color,span=1;text,align=inline;color,span=1;"
        "text,align=inline,width=4.25;"
        "image,tooltip=Creative mode,"
        "0=art/minecraft/textures/blank.png,"
        "1=art/minecraft/textures/server_flags_creative.png,"
        "align=inline,padding=0.25,width=1.5;"
        "image,tooltip=Damage / PvP,"
        "0=art/minecraft/textures/blank.png,"
        "1=art/minecraft/textures/server_flags_damage.png,"
        "2=art/minecraft/textures/server_flags_pvp.png,"
        "align=inline,padding=0.25,width=1.5;"
        "color,align=inline,span=1;"
        "text,align=inline,padding=1]"
        "table[0.25,1;9.25,5.75;servers;"
        "#FFFF00,Core Developers,,Enrique Gonzalez,;1]";

    return form;
}

bool Online::Handle(std::string name, BaseUIElement* element)
{
    return true;
}
