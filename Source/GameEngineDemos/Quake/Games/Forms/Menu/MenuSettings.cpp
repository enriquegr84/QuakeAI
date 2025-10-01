//========================================================================
// Settings.cpp
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

#include "MenuSettings.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"
#include "Game/Actor/Actor.h"

//---------------------------------------------------------------------------------------------------------------------
// Settings
//---------------------------------------------------------------------------------------------------------------------
MenuSettings::MenuSettings() : BaseMenu()
{
    mNodeHighlighting = { "Node Highlighting", "Node Outlining", "None" };
    mLeaves = { "Fancy Leaves", "Opaque Leaves", "Simple Leaves" };
    mFilters = { "No Filter", "Bilinear Filter", "Trilinear Filter" };
    mMipmap = { "No Mipmap", "Mipmap", "Mipmap + Aniso. Filter" };
    mAntialiasing = { "2x", "4x", "8x", "None" };

    mNodeHighlightingOptions["Node Outlining"] = "box";
    mNodeHighlightingOptions["Node Highlighting"] = "halo";
    mNodeHighlightingOptions["None"] = "none";
    mLeavesOptions["Opaque Leaves"] = "opaque";
    mLeavesOptions["Simple Leaves"] = "simple";
    mLeavesOptions["Fancy Leaves"] = "fancy";
    mFiltersOptions["No Filter"] = "";
    mFiltersOptions["Bilinear Filter"] = "bilinear_filter";
    mFiltersOptions["Trilinear Filter"] = "trilinear_filter";
    mMipmapOptions["No Mipmap"] = "";
    mMipmapOptions["Mipmap"] = "Mipmap";
    mMipmapOptions["Mipmap + Aniso. Filter"] = "anisotropic_filter";
    mAntialiasingOptions["2x"] = "2";
    mAntialiasingOptions["4x"] = "4";
    mAntialiasingOptions["8x"] = "8";
    mAntialiasingOptions["None"] = "0";
}

std::string MenuSettings::GetLeaves()
{
    std::string leaves;
    for (std::string leaf : mLeaves)
        leaves += leaf + ",";
    if (!leaves.empty()) leaves.pop_back();
    return leaves;
}

std::string MenuSettings::GetFilters()
{
    std::string filters;
    for (std::string filter : mFilters)
        filters += filter + ",";
    if (!filters.empty()) filters.pop_back();
    return filters;
}

std::string MenuSettings::GetMipmaps()
{
    std::string mipMaps;
    for (std::string mipMap : mMipmap)
        mipMaps += mipMap + ",";
    if (!mipMaps.empty()) mipMaps.pop_back();
    return mipMaps;
}

std::string MenuSettings::GetAntialiasings()
{
    std::string fsaa;
    for (std::string antialiasing : mAntialiasing)
        fsaa += antialiasing + ",";
    if (!fsaa.empty()) fsaa.pop_back();
    return fsaa;
}

std::string MenuSettings::GetNodeHighlightings()
{
    std::string nodes;
    for (std::string node : mNodeHighlighting)
        nodes += node + ",";
    if (!nodes.empty()) nodes.pop_back();
    return nodes;
}

unsigned int MenuSettings::GetLeaveOption()
{
    std::string leavesStyle = Settings::Get()->Get("leaves_style");
    for (auto it = mLeavesOptions.begin(); it != mLeavesOptions.end(); ++it)
        if (it->second == leavesStyle)
            return std::distance(mLeavesOptions.begin(), it) + 1;

    return 0;
}

unsigned int MenuSettings::GetFilterOption()
{
    if (Settings::Get()->Get("trilinear_filter") == "true")
    {
        auto it = std::find(mFilters.begin(), mFilters.end(), "Trilinear Filter");
        return std::distance(mFilters.begin(), it) + 1;
    }
    else if (Settings::Get()->Get("trilinear_filter") == "false" &&
            Settings::Get()->Get("bilinear_filter") == "true")
    {
        auto it = std::find(mFilters.begin(), mFilters.end(), "Bilinear Filter");
        return std::distance(mFilters.begin(), it) + 1;
    }
    else
    {
        auto it = std::find(mFilters.begin(), mFilters.end(), "No Filter");
        return std::distance(mFilters.begin(), it) + 1;
    }
}

unsigned int MenuSettings::GetMipmapOption()
{
    if (Settings::Get()->Get("anisotropic_filter") == "true")
    {
        auto it = std::find(mMipmap.begin(), mMipmap.end(), "Mipmap + Aniso. Filter");
        return std::distance(mMipmap.begin(), it) + 1;
    }
    else if (Settings::Get()->Get("anisotropic_filter") == "false" &&
            Settings::Get()->Get("mip_map") == "true")
    {
        auto it = std::find(mMipmap.begin(), mMipmap.end(), "Mipmap");
        return std::distance(mMipmap.begin(), it) + 1;
    }
    else
    {
        auto it = std::find(mMipmap.begin(), mMipmap.end(), "No Mipmap");
        return std::distance(mMipmap.begin(), it) + 1;
    }
}

unsigned int MenuSettings::GetAntialiasingOption()
{
    std::string antialiasing = Settings::Get()->Get("fsaa");
    for (auto it = mAntialiasingOptions.begin(); it != mAntialiasingOptions.end(); ++it)
        if (it->second == antialiasing)
            return std::distance(mAntialiasingOptions.begin(), it) + 1;

    return 0;
}

unsigned int MenuSettings::GetNodeHighlightingOption()
{
    std::string nodeHighlighting = Settings::Get()->Get("node_highlighting");
    for (auto it = mNodeHighlightingOptions.begin(); it != mNodeHighlightingOptions.end(); ++it)
        if (it->second == nodeHighlighting)
            return std::distance(mNodeHighlightingOptions.begin(), it) + 1;

    return 0;
}

std::string MenuSettings::GetForm()
{
    std::string form = 
        "size[12.000000,5.400000,false]" + BaseMenu::GetForm() +
        "4;true;false]box[0,0;3.75,4.5;#999999]"
        "checkbox[0.25,0;cb_smooth_lighting;Smooth Lighting;" +
        Settings::Get()->Get("smooth_lighting") + "]"
        "checkbox[0.25,0.5;cb_particles;Particles;" +
        Settings::Get()->Get("enable_particles") + "]"
        "checkbox[0.25,1;cb_3d_clouds;3D Clouds;" +
        Settings::Get()->Get("enable_3d_clouds") + "]"
        "checkbox[0.25,1.5;cb_opaque_water;Opaque Water;" +
        Settings::Get()->Get("opaque_water") + "]"
        "checkbox[0.25,2.0;cb_connected_glass;Connected Glass;" +
        Settings::Get()->Get("connected_glass") + "]"
        "dropdown[0.25,2.8;3.5;dd_node_highlighting;" + GetNodeHighlightings() + ";" +
        std::to_string(GetNodeHighlightingOption()) + "]"
        "dropdown[0.25,3.6;3.5;dd_leaves_style;" + GetLeaves() + ";" +
        std::to_string(GetLeaveOption()) + "]"
        "box[4,0;3.75,4.5;#999999]"
        "label[4.25,0.1;Texturing:]"
        "dropdown[4.25,0.55;3.5;dd_filters;" + GetFilters() + ";" +
        std::to_string(GetFilterOption()) + "]"
        "dropdown[4.25,1.35;3.5;dd_mipmap;" + GetMipmaps() + ";" +
        std::to_string(GetMipmapOption()) + "]"
        "label[4.25,2.15;Antialiasing:]"
        "dropdown[4.25,2.6;3.5;dd_antialiasing;" + GetAntialiasings() + ";" +
        std::to_string(GetAntialiasingOption()) + "]"
        "label[4.25,3.45;Screen:]"
        "checkbox[4.25,3.6;cb_autosave_screensize;Autosave Screen Size;" +
        Settings::Get()->Get("autosave_screensize") + "]"
        "box[8,0;3.75,4.5;#999999]";

    std::string videoDriver = Settings::Get()->Get("video_driver");
    bool shadersEnabled = Settings::Get()->GetBool("enable_shaders");
    if (videoDriver == "opengl")
    {
        form += "checkbox[8.25,0;cb_shaders;Shaders;";
        form += shadersEnabled ? "true]" : "false]";
    }
    else
    {
        form += "checkbox[8.25,0;cb_shaders;Shaders;";
        form += shadersEnabled ? "true]" : "false]";
    }

    form +=
        "button[8,4.75;3.95,1;btn_change_keys;Change Keys]"
        "button[0,4.75;3.95,1;btn_advanced_settings;All Settings]";

    if (shadersEnabled)
    {
        form +=
            "checkbox[8.25,0.5;cb_tonemapping;Tone Mapping;" +
            Settings::Get()->Get("tone_mapping") + "]" +
            "checkbox[8.25,1;cb_waving_water;Waving Liquids;" +
            Settings::Get()->Get("enable_waving_water") + "]"
            "checkbox[8.25,1.5;cb_waving_leaves;Waving Leaves;" +
            Settings::Get()->Get("enable_waving_leaves") + "]"
            "checkbox[8.25,2;cb_waving_plants;Waving Plants;" +
            Settings::Get()->Get("enable_waving_plants") + "]";
    }
    else
    {
        form +=
            "label[8.38,0.7;\x1b(c@#888888)Tone Mapping\x1b(c@#ffffff)]"
            "label[8.38,1.2;\x1b(c@#888888)Waving Liquids\x1b(c@#ffffff)]"
            "label[8.38,1.7;\x1b(c@#888888)Waving Leaves\x1b(c@#ffffff)]"
            "label[8.38,2.2;\x1b(c@#888888)Waving Plants\x1b(c@#ffffff)]";
    }
    return form;
}

bool MenuSettings::Handle(std::string name, BaseUIElement* element)
{
    if (name == "btn_advanced_settings")
    {
        //local adv_settings_dlg = create_adv_settings_dlg()
        //adv_settings_dlg:set_parent(this)
        //this : hide()
        //adv_settings_dlg : show()
        //mm_texture.update("singleplayer", current_game())
        return true;
    }
    
    if (name == "cb_smooth_lighting")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("smooth_lighting", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_particles")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_particles", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_3d_clouds")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_3d_clouds", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_opaque_water")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("opaque_water", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_connected_glass")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("connected_glass", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_autosave_screensize")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("autosave_screensize", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_shaders")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_shaders", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_tonemapping")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("tone_mapping", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_waving_water")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_waving_water", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_waving_leaves")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_waving_leaves", checkbox->IsChecked());
        return true;
    }

    if (name == "cb_waving_plants")
    {
        const BaseUICheckBox* checkbox = reinterpret_cast<BaseUICheckBox*>(element);
        Settings::Get()->SetBool("enable_waving_plants", checkbox->IsChecked());
        return true;
    }

    if (name == "btn_change_keys")
    {
        //core.show_keys_menu();
        return true;
    }

    if (name == "dd_leaves_style")
    {
        const BaseUIComboBox* comboBox = reinterpret_cast<BaseUIComboBox*>(element);
        Settings::Get()->Set(
            "leaves_style", ToString(comboBox->GetItem(comboBox->GetSelected())));
        return true;
    }

    if (name == "dd_node_highlighting")
    {
        const BaseUIComboBox* comboBox = reinterpret_cast<BaseUIComboBox*>(element);
        Settings::Get()->Set(
            "node_highlighting", ToString(comboBox->GetItem(comboBox->GetSelected())));
        return true;
    }

    if (name == "dd_antialiasing")
    {
        const BaseUIComboBox* comboBox = reinterpret_cast<BaseUIComboBox*>(element);
        Settings::Get()->Set(
            "fsaa", ToString(comboBox->GetItem(comboBox->GetSelected())));
        return true;
    }

    if (name == "dd_filters")
    {
        const BaseUIComboBox* comboBox = reinterpret_cast<BaseUIComboBox*>(element);
        switch (comboBox->GetSelected())
        {
            case 0:
                Settings::Get()->Set("bilinear_filter", "false");
                Settings::Get()->Set("trilinear_filter", "false");
                break;
            case 1:
                Settings::Get()->Set("bilinear_filter", "true");
                Settings::Get()->Set("trilinear_filter", "false");
                break;
            case 2:
                Settings::Get()->Set("bilinear_filter", "false");
                Settings::Get()->Set("trilinear_filter", "true");
                break;
        }
        return true;
    }

    if (name == "dd_mipmap")
    {
        const BaseUIComboBox* comboBox = reinterpret_cast<BaseUIComboBox*>(element);
        switch (comboBox->GetSelected())
        {
            case 0:
                Settings::Get()->Set("mip_map", "false");
                Settings::Get()->Set("anisotropic_filter", "false");
                break;
            case 1:
                Settings::Get()->Set("mip_map", "true");
                Settings::Get()->Set("anisotropic_filter", "false");
                break;
            case 2:
                Settings::Get()->Set("Mipap", "true");
                Settings::Get()->Set("anisotropic_filter", "true");
                break;
        }
        return true;
    }

    return false;
}
