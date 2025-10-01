//========================================================================
// DlgDeleteContent.cpp
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

#include "Content.h"
#include "DlgDeleteContent.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"

#include "../../../QuakeEvents.h"

//---------------------------------------------------------------------------------------------------------------------
// DlgDeleteContent
//---------------------------------------------------------------------------------------------------------------------
DlgDeleteContent::DlgDeleteContent(const std::wstring& content)
    : BaseMenu(), mContent(content)
{

}

std::string DlgDeleteContent::GetForm()
{
    std::string form =
        "size[11.5,4.5,true]label[2,2;Are you sure you want to delete " +
        ToString(mContent) + "?]"
        "style[dlg_delete_content_confirm;bgcolor=red]"
        "button[3.25,3.5;2.5,0.5;dlg_delete_content_confirm;Delete]"
        "button[5.75,3.5;2.5,0.5;dlg_delete_content_cancel;Cancel]";

    return form;
}

bool DlgDeleteContent::Handle(std::string name, BaseUIElement* element)
{

    if (name == "btn_mod_mgr_delete_mod")
    {
        //local adv_settings_dlg = create_adv_settings_dlg()
        //adv_settings_dlg:set_parent(this)
        //this : hide()
        //adv_settings_dlg : show()
        //mm_texture.update("singleplayer", current_game())
        return true;
    }

    if (name == "dlg_delete_content_cancel")
    {
        std::shared_ptr<EventDataOpenGameSelection>
            pOpenGameSelection(new EventDataOpenGameSelection());
        EventManager::Get()->QueueEvent(pOpenGameSelection);
        return true;
    }

    return true;
}
