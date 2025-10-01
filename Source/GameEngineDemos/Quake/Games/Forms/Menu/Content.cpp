//========================================================================
// Content.cpp
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
// Content
//---------------------------------------------------------------------------------------------------------------------
Content::Content() : BaseMenu(), mSelectedLevel(1)
{

}

std::string Content::GetForm()
{
    std::string form = 
        "size[12.000000,5.400000,false]" + BaseMenu::GetForm() +
        "3;true;false]label[0.05,-0.25;Installed Packages:]"
        "tablecolumns[color;tree;text]"
        "table[0,0.25;5.1,4.3;pkglist;";

    std::vector<Level*> levels = GameLogic::Get()->GetLevelManager()->GetLevels();
    for (std::vector<Level*>::iterator it = levels.begin(); it != levels.end(); ++it)
        form += "#6389FF,0," + ToString((*it)->GetName()) + ",";
    if (!levels.empty()) form.pop_back();

    form += ";" + std::to_string(mSelectedLevel) + "]"
        "button[0,4.85;5.25,0.5;btn_contentdb;Browse online content]";

    Level* selectedLevelInfo = nullptr;
    if (levels.size() >= mSelectedLevel-1)
        selectedLevelInfo = levels[mSelectedLevel-1];

    if (selectedLevelInfo)
    {
        //check for screenshot beeing available
        std::wstring screenshotFileName = selectedLevelInfo->GetPath() + L"\\screenshot.png";
        
        std::wstring modScreenshot;
        if (FileSystem::Get()->ExistFile(screenshotFileName))
            modScreenshot = screenshotFileName;

        if (modScreenshot.empty())
            modScreenshot = L"art/noscreenshot.png";

        std::wstring desc = L"No package description available";
        if (!selectedLevelInfo->GetDescription().empty())
            desc = selectedLevelInfo->GetDescription();

        form +=
            "image[5.5,0;3,2;" + ToString(modScreenshot) + "]"
            "label[8.25,0.6;" + ToString(selectedLevelInfo->GetName()) + "]"
            "box[5.5,2.2;6.15,2.35;#000]"
            "textarea[5.85,2.2;6.35,2.9;;"
            "Information:;" + ToString(desc) + "]"
            "button[5.5,4.65;3.25,1;btn_mod_mgr_delete_mod;Uninstall Package]";
    }
    return form;
}

bool Content::Handle(std::string name, BaseUIElement* element)
{
    if (name == "pkglist")
    {
        const UITable* table = reinterpret_cast<UITable*>(element);
        mSelectedLevel = table->GetSelected();
        return true;
    }

    if (name == "btn_mod_mgr_delete_mod")
    {
        Level* selectedLevel = GameLogic::Get()->GetLevelManager()->GetLevel(mSelectedLevel-1);

        std::shared_ptr<QuakeEventDataDeleteContentStore> pDeleteContentStore(
            new QuakeEventDataDeleteContentStore(ToString(selectedLevel->GetName())));
        EventManager::Get()->QueueEvent(pDeleteContentStore);
        return true;
    }

    return false;
}
