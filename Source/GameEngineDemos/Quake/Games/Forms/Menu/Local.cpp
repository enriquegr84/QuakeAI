//========================================================================
// Local.cpp
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

#include "Core/Event/Event.h"
#include "Core/Event/EventManager.h"

#include "../../Subgames.h"

#include "../../../QuakeEvents.h"

//---------------------------------------------------------------------------------------------------------------------
// Local
//---------------------------------------------------------------------------------------------------------------------
Local::Local(std::wstring level) : BaseMenu(), mLevel(level)
{
    Settings::Get()->SetInt("mainmenu_last_selected_world", -1);
}

std::string Local::GetForm()
{
    std::string form = 
        "size[12.000000,5.400000,false]" + BaseMenu::GetForm() +
        "1;true;false]button[3.9,3.8;2.8,1;world_delete;Delete]"
        "button[6.55,3.8;2.8,1;world_configure;Select Mods]"
        "button[9.2,3.8;2.8,1;world_create;New]"
        "label[3.9,-0.05;Select World:]"
        "checkbox[0,-0.20;cb_creative_mode;Creative Mode;" +
        Settings::Get()->Get("creative_mode") + "]"
        "checkbox[0,0.25;cb_enable_damage;Enable Damage;" +
        Settings::Get()->Get("enable_damage") + "]"
        "checkbox[0,0.7;cb_server;Host Server;" +
        Settings::Get()->Get("enable_server") + "]"
        "textlist[3.9,0.4;7.9,3.45;sp_worlds;";

    unsigned int levelCount = 0;
    std::vector<Level*> levels = GameLogic::Get()->GetLevelManager()->GetLevels();
    for (std::vector<Level*>::iterator it = levels.begin(); it != levels.end(); ++it)
    {
        if ((*it)->GetName() == mLevel)
        {
            form += ToString((*it)->GetName()) + ",";
            levelCount++;
        }
    }
    if (levelCount > 0) form = form.substr(0, form.size() - 1);
    form += ";" + std::to_string(levelCount) + "]";

    if (Settings::Get()->GetBool("enable_server"))
    {
        form +=
            "button[7.9,4.75;4.1,1;play;Host Game]"
            "checkbox[0,1.15;cb_server_announce;Announce Server;" +
            Settings::Get()->Get("server_announce") + "]"
            "field[0.3,2.85;3.8,0.5;te_playername;Name;" +
            Settings::Get()->Get("name") + "]" +
            "pwdfield[0.3,4.05;3.8,0.5;te_passwd;Password]";

        std::string bindAddr = Settings::Get()->Get("bind_address");
        if (!bindAddr.empty())
        {
            form +=
                "field[0.3,5.25;2.5,0.5;te_serveraddr;Bind Address;" +
                Settings::Get()->Get("bind_address") + "]" +
                "field[2.85,5.25;1.25,0.5;te_serverport;Port;" +
                Settings::Get()->Get("port") + "]";
        }
        else
        {
            form +=
                "field[0.3,5.25;3.8,0.5;te_serverport;Server Port;" +
                Settings::Get()->Get("port") + "]";
        }
    }
    else form += "button[7.9,4.75;4.1,1;play;Play Game]";

    float offset = 0.0575f;
    RectangleShape<2, float> box;
    box.mCenter = Vector2<float>{ -0.3f, 5.9f };
    box.mExtent = Vector2<float>{ 12.4f, 1.15f };

    form += "box[" +
        std::to_string(box.mCenter[0]) + "," + std::to_string(box.mCenter[1]) + ";" +
        std::to_string(box.mExtent[0]) + "," + std::to_string(box.mExtent[1]) + ";" +
        "#000000]";

    box.mExtent[0] = box.mExtent[1];
    box.mCenter[0] += offset;
    box.mCenter[1] += offset;

    for (std::vector<Level*>::iterator it = levels.begin(); it != levels.end(); ++it)
    {
        //check for screenshot beeing available
        std::wstring screenshotFileName = (*it)->GetIconPath();

        std::wstring modScreenshot;
        if (FileSystem::Get()->ExistFile(screenshotFileName))
            modScreenshot = screenshotFileName;

        if (modScreenshot.empty())
            modScreenshot = L"art/noscreenshot.png";

        form += "image_button[" +
            std::to_string(box.mCenter[0]) + "," + std::to_string(box.mCenter[1]) + ";" +
            std::to_string(box.mExtent[0]) + "," + std::to_string(box.mExtent[1]) + ";" +
            ToString(modScreenshot) + ";game_btnbar_" + ToString((*it)->GetId()) +
            ";;true;false]tooltip[game_btnbar_" + ToString((*it)->GetId()) + ";" +
            ToString((*it)->GetName()) + "]";

        box.mCenter[0] += box.mExtent[0];
    }

    form += "image_button[" +
        std::to_string(box.mCenter[0]) + "," + std::to_string(box.mCenter[1]) + ";" +
        std::to_string(box.mExtent[0]) + "," + std::to_string(box.mExtent[1]) + ";" +
        "art/minecraft/textures/plus.png;game_open_cdb;;true;false]"
        "tooltip[game_open_cdb;Install games from ContentDB]";

    return form;
}

bool Local::Handle(std::string name, BaseUIElement* element)
{
    if (name == "sp_worlds")
    {
        const UITable* table = reinterpret_cast<UITable*>(element);
        if (table)
        {
            int selection = table ? table->GetSelected() : 0;

            std::vector<WorldSpecification> worldSpecs = GetAvailableWorlds();
            if (selection >= 0 && selection < (int)worldSpecs.size())
                Settings::Get()->SetInt("mainmenu_last_selected_world", selection);
        }
    }

    if (name == "play")
    {
        const UIButton* button = reinterpret_cast<UIButton*>(element);
        if (button)
        {
            int selection = Settings::Get()->GetInt("mainmenu_last_selected_world");

            std::vector<WorldSpecification> worldSpecs = GetAvailableWorlds();
            if (selection >= 0 && selection < (int)worldSpecs.size())
            {
                auto& worldSpec = worldSpecs[selection];

                Settings conf;
                std::string confPath = worldSpec.mPath + "/world.mt";
                bool succeeded = conf.ReadConfigFile(confPath.c_str());
                if (!succeeded)
                    return false;

                Settings::Get()->SetInt("selected_world", selection);

                //Update last game
                Settings::Get()->Set("selected_game", ToString(mLevelId));

                if (conf.Exists("creative_mode"))
                    Settings::Get()->Set("creative_mode", conf.Get("creative_mode"));
                if (conf.Exists("enable_damage"))
                    Settings::Get()->Set("enable_damage", conf.Get("enable_damage"));

                std::shared_ptr<EventDataRequestStartGame>
                    pRequestStartGameEvent(new EventDataRequestStartGame());
                EventManager::Get()->QueueEvent(pRequestStartGameEvent);
                return true;
            }
        }
    }

    if (name == "game_open_cdb")
    {
        std::shared_ptr<QuakeEventDataOpenContentStore> pOpenContentStore(
            new QuakeEventDataOpenContentStore());
        EventManager::Get()->QueueEvent(pOpenContentStore);
        return true;
    }

    if (name == "world_create")
    {
        /*
        MinecraftMainMenuUI* mainMenu = reinterpret_cast<MinecraftMainMenuUI*>(mUI);
        mainMenu->UpdateMenuUI((*it));
        */
        return true;
    }

    if (name == "world_delete")
    {
        /*
        MinecraftMainMenuUI* mainMenu = reinterpret_cast<MinecraftMainMenuUI*>(mUI);
        mainMenu->UpdateMenuUI((*it));
        */
        return true;
    }

    if (name == "world_configure")
    {
        /*
        MinecraftMainMenuUI* mainMenu = reinterpret_cast<MinecraftMainMenuUI*>(mUI);
        mainMenu->UpdateMenuUI((*it));
        */
        return true;
    }

    if (name.find("game_btnbar_") != std::string::npos)
    {
        std::vector<Level*> levels = GameLogic::Get()->GetLevelManager()->GetLevels();
        for (std::vector<Level*>::iterator it = levels.begin(); it != levels.end(); ++it)
        {
            if (name == "game_btnbar_" + ToString((*it)->GetId()))
            {
                mLevel = (*it)->GetName();
                mLevelId = (*it)->GetId();

                Settings::Get()->SetInt("mainmenu_last_selected_world", 0);

                std::shared_ptr<QuakeEventDataChangeGameSelection> pChangeGameSelection(
                    new QuakeEventDataChangeGameSelection(ToString(mLevel), ToString(mLevelId)));
                EventManager::Get()->QueueEvent(pChangeGameSelection);
                return true;
            }
        }
    }

    if (name == "cb_creative_mode")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("creative_mode", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_enable_damage")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_damage", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_server")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_server", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_server_announce")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("server_announce", checkbox->IsChecked());
        return true;
    }

    return false;
}
