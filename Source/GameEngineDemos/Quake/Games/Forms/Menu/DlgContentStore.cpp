//========================================================================
// DlgContentStore.cpp
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

#include "Local.h"
#include "DlgContentStore.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"

#include "../../../QuakeEvents.h"

//---------------------------------------------------------------------------------------------------------------------
// DlgContentStore
//---------------------------------------------------------------------------------------------------------------------
DlgContentStore::DlgContentStore() : BaseMenu()
{

}

std::string DlgContentStore::GetForm()
{
    std::string form =
        "form_version[3]size[15.75,9.5]position[0.5,0.55]"
        "style[status,downloading,queued;border=false]container[0.375,0.375]"
        "field[0,0;7.225,0.8;search_string;;]field_close_on_enter[search_string;false]"
        "image_button[7.3,0;0.8,0.8;art/minecraft/textures/search.png;search;]"
        "image_button[8.125,0;0.8,0.8;art/minecraft/textures/clear.png;clear;]"
        "dropdown[9.6,0;2.4,0.8;type;All packages,Games,Mods,Texture packs;2]container_end[]"
        "container[0,8.325]button[0.375,0;4,0.8;back;Back to Main Menu]container[10.175,0]"
        "image_button[0,0;0.8,0.8;art/minecraft/textures/start_icon.png;pstart;]"
        "image_button[0.8,0;0.8,0.8;art/minecraft/textures/prev_icon.png;pback;]"
        "style[pagenum;border=false]button[1.6,0;2,0.8;pagenum;1 / 8]"
        "image_button[3.6,0;0.8,0.8;art/minecraft/textures/next_icon.png;pnext;]"
        "image_button[4.4,0;0.8,0.8;art/minecraft/textures/end_icon.png;pend;]"
        "container_end[]container_end[]";

    //download/queued tooltips always have the same message
    form +=
        "button[12.75,0.375;2.625,0.8;update_all;Update All\[1\]]"
        "tooltip[downloading;Downloading...;#dff6f5;#302c2e]"
        "tooltip[queued;Queued;#dff6f5;#302c2e]";

    form +=
        "container[0.375,1.55]image[0,0;1.5,1;art/minecraft/cache/cbd/game-siegment-loria.png]"
        "label[1.875,0.1;\x1b(c@#72FF63)Loria (alpha)\x1b(c@#ffffff)\x1b(c@#BFBFBF) "
        "by siegment\x1b(c@#ffffff)]container[15,0.1]style[uninstall_1;bgcolor=#a93b3b]"
        "image_button[-1.55,0;0.7,0.7;art/minecraft/textures/cdb_clear.png;uninstall_1;]"
        "tooltip[uninstall_1;Uninstall;#dff6f5;#302c2e]]"
        "image_button[-0.7,0;0.7,0.7;art/minecraft/textures/cdb_viewonline.png;view_1;]"
        "tooltip[view_1;View more information in a web browser;#dff6f5;#302c2e]"
        "container_end[]textarea[1.855,0.3;11.625,0.8;;;"
        "Survival in hardcore acid - alien sci - fi themed world]container_end[]";

    form +=
        "container[0.375,2.925]image[0,0;1.5,1;art/minecraft/cache/cbd/game-Wuzzy-mineclone2.jpg]"
        "label[1.875,0.1;\x1b(c@#72FF63)MineClone 2\x1b(c@#ffffff)\x1b(c@#BFBFBF) "
        "by Wuzzy\x1b(c@#ffffff)]container[15,0.1]style[uninstall_1;bgcolor=#a93b3b]"
        "image_button[-1.55,0;0.7,0.7;art/minecraft/textures/cdb_clear.png;uninstall_1;]"
        "tooltip[uninstall_1;Uninstall;#dff6f5;#302c2e]]"
        "image_button[-0.7,0;0.7,0.7;art/minecraft/textures/cdb_viewonline.png;view_1;]"
        "tooltip[view_1;View more information in a web browser;#dff6f5;#302c2e]"
        "container_end[]textarea[1.855,0.3;11.625,0.8;;;"
        "A survival sandbox game(work in progress!).Survive\, gather\, hunt\, build\, explore\, and do"
        "much more.An imitation of Minecraft.]container_end[]";

    form +=
        "container[0.375,4.3]image[0,0;1.5,1;art/minecraft/cache/cbd/game-Warr1024-nodecore.jpg]"
        "label[1.875,0.1;\x1b(c@#72FF63)NodeCore\x1b(c@#ffffff)\x1b(c@#BFBFBF) "
        "by Warr1024\x1b(c@#ffffff)]container[15,0.1]style[install_3;bgcolor=#28ccdf]"
        "image_button[-1.55,0;0.7,0.7;art/minecraft/textures/cdb_update.png;install_3;]"
        "tooltip[install_3;Update;#dff6f5;#302c2e]]"
        "image_button[-0.7,0;0.7,0.7;art/minecraft/textures/cdb_viewonline.png;view_1;]"
        "tooltip[view_1;View more information in a web browser;#dff6f5;#302c2e]"
        "container_end[]textarea[1.855,0.3;11.625,0.8;;;"
        "Minetest's top original voxel game about emergent mechanics and explorationd]container_end[]";

    form +=
        "container[0.375,5.675]image[0,0;1.5,1;art/minecraft/cache/cbd/game-Wuzzy-tutorial.png]"
        "label[1.875,0.1;\x1b(c@#72FF63)Tutorial\x1b(c@#ffffff)\x1b(c@#BFBFBF) "
        "by Wuzzy\x1b(c@#ffffff)]container[15,0.1]style[uninstall_1;bgcolor=#a93b3b]"
        "image_button[-1.55,0;0.7,0.7;art/minecraft/textures/cdb_clear.png;uninstall_1;]"
        "tooltip[uninstall_1;Uninstall;#dff6f5;#302c2e]]"
        "image_button[-0.7,0;0.7,0.7;art/minecraft/textures/cdb_viewonline.png;view_1;]"
        "tooltip[view_1;View more information in a web browser;#dff6f5;#302c2e]"
        "container_end[]textarea[1.855,0.3;11.625,0.8;;;"
        "Learn how to play!]container_end[]";

    form +=
        "container[0.375,7.05]image[0,0;1.5,1;art/minecraft/cache/cbd/game-kay27-mineclone5.png]"
        "label[1.875,0.1;\x1b(c@#72FF63)MineClone 5\x1b(c@#ffffff)\x1b(c@#BFBFBF) "
        "by kay27\x1b(c@#ffffff)]container[15,0.1]style[install_5;bgcolor=#71aa34]"
        "image_button[-1.55,0;0.7,0.7;art/minecraft/textures/cdb_add.png;install_5;]"
        "tooltip[install_5;Install;#dff6f5;#302c2e]]"
        "image_button[-0.7,0;0.7,0.7;art/minecraft/textures/cdb_viewonline.png;view_1;]"
        "tooltip[view_1;View more information in a web browser;#dff6f5;#302c2e]"
        "container_end[]textarea[1.855,0.3;11.625,0.8;;;"
        "Waiting for MineClone 2 update ? Try MineClone 5 with no release milestones\, "
        "rapid delivery\, merges by one approval and no MC target version limitations."
        "But technically it's still MineClone 2.]container_end[]";

    return form;
}

bool DlgContentStore::Handle(std::string name, BaseUIElement* element)
{
    if (name == "back")
    {
        std::shared_ptr<QuakeEventDataOpenGameSelection>
            pOpenGameSelection(new QuakeEventDataOpenGameSelection());
        EventManager::Get()->QueueEvent(pOpenGameSelection);
    }

    return true;
}
