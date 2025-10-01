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
        "size[15.5,7,false]real_coordinates[true]" + BaseMenu::GetForm() +
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
        "table[0.25,1;9.25,5.75;servers;6,#4bdd42,Public Servers,,,0,0,,,4,,,#a1e587,33 / 50,0,2,#ffffff,Capture the Flag,1,,,#a1e587,20 / 52,0,2,#ffffff,Your Land,1,,,#a1e587,17 / 64,0,2,#ffffff,Nico's World,1,,,#ffba97,18 / 19,0,2,#ffffff,Versus : Minecraft!,1,,,#a1e587,10 / 20,0,2,#ffffff,\\[JMA\\] Capture the Flag (DE),1,,,#a1e587,5 / 40,0,2,#ffffff,*** SkyBlock! *** by Telesight,1,,,#a1e587,6 / 25,0,2,#ffffff,Francophonia \\[France\\],3,,,#a1e587,5 / 30,0,1,#ffffff,Tunnelers' Abyss,1,,,#a1e587,5 / 20,0,2,#ffffff,Pandorabox,1,,,#a1e587,3 / 85,0,1,#ffffff,Asia Thailand \\[v5\\] - LOW server lag - solar power,1,,,#a1e587,4 / 77,0,2,#ffffff,Nico's Survival Server,1,,,#a1e587,4 / 42,0,2,#ffffff,oysterity anarchy,1,,,#a1e587,4 / 20,0,2,#ffffff,WunderWelt \\[WuWe\\],1,,,#a1e587,3 / 10,0,2,#ffffff,Solo Un Altro Clone,1,,,#a1e587,3 / 24,0,2,#ffffff,SkyAnarchy \\[skyblock\\],3,,,#a1e587,19 / 40,0,2,#ffffff,Nico's Small Builder Server,1,,,#a1e587,3 / 32,0,1,#ffffff,NodeCore Community,1,,,#a1e587,3 / 25,0,2,#ffffff,*** FantasyWorld *** by Telesight,1,,,#a1e587,8 / 50,0,2,#ffffff,Cheers,3,,,#a1e587,3 / 100,0,2,#ffffff,Blocky Survival,1,,,#a1e587,3 / 25,0,1,#ffffff,Inside the Box,1,,,#a1e587,2 / 30,1,0,#ffffff,*** BuildersWorld *** by Telesight,1,,,#a1e587,2 / 30,0,1,#ffffff,\\[RU\\] MineClone 2,1,,,#a1e587,2 / 100,0,2,#ffffff,CitySim 2,2,,,#a1e587,2 / 20,0,2,#ffffff,Enyekala (Must Test),3,,,#a1e587,2 / 15,0,2,#ffffff,HANS (Hopefully A Nice Server),3,,,#a1e587,1 / 16,0,2,#ffffff,BRAZUCAS \\[Brasil\\] \\[pt_BR\\],1,,,#a1e587,1 / 149,0,2,#ffffff,* Bau-Deine-Welt * Build Your Land *,3,,,#a1e587,1 / 50,0,2,#ffffff,Centeria Survival Deluxe,1,,,#a1e587,1 / 15,0,2,#ffffff,Ancient Times,1,,,#a1e587,1 / 15,0,1,#ffffff,Zeitsprung,1,,,#a1e587;0]";

    return form;
}

bool Online::Handle(std::string name, BaseUIElement* element)
{
    return true;
}
