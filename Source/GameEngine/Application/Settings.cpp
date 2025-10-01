//========================================================================
// Initialization.cpp : Defines utility functions for game initialization
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
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

#include "Settings.h"

#include "Core/IO/FileSystem.h"
#include "Core/IO/XmlResource.h"
#include "Core/Threading/MutexAutolock.h"


Settings* Settings::mSetting = NULL;

Settings* Settings::mSettingsLayers[SL_COUNT] = { 0 }; // Zeroed by compiler
std::unordered_map<std::string, const FlagDescription *> Settings::mSettingFlags;

Settings* Settings::Get(void)
{
    LogAssert(Settings::mSetting, "Game setting doesn't exist");
    return Settings::mSetting;
}


Settings* Settings::GetLayer(SettingsLayer sl)
{
    LogAssert((int)sl >= 0 && sl < SL_COUNT, "incorrect layer");
    return mSettingsLayers[(size_t)sl];
}


Settings* Settings::CreateLayer(SettingsLayer sl)
{
    if ((int)sl < 0 || sl >= SL_COUNT)
        throw BaseException("Invalid settings layer");

    Settings *&layer = mSettingsLayers[(size_t)sl];
    if (layer)
        throw BaseException("Setting layer " + std::to_string(sl) + " already exists");

    layer = new Settings();
    layer->mSettingsLayer = sl;

    if (sl == SL_GLOBAL)
        Settings::mSetting = layer;

    return layer;
}


Settings::Settings(const std::string& endTag) : mEndTag(endTag)
{
	// set all the options to decent default value
	mRoot = NULL;
}


Settings::~Settings()
{
    MutexAutoLock lock(mMutex);

    if (mSettingsLayer < SL_COUNT)
        mSettingsLayers[(size_t)mSettingsLayer] = nullptr;

    // Compatibility
    if (mSettingsLayer == SL_GLOBAL)
        Settings::mSetting = nullptr;

    for (SettingEntries::const_iterator it = mSettings.begin(); it != mSettings.end(); ++it)
        delete it->second.mGroup;
    mSettings.clear();
}


Settings& Settings::operator = (const Settings &other)
{
    if (&other == this)
        return *this;

    // TODO: Avoid copying Settings objects. Make this private.
    LogAssert(mSettingsLayer != SL_COUNT && other.mSettingsLayer != SL_COUNT, 
        "Tried to copy unique Setting layer " + std::to_string(mSettingsLayer));

    MutexAutoLock lock(mMutex);
    MutexAutoLock lock2(other.mMutex);

    for (SettingEntries::const_iterator it = mSettings.begin(); it != mSettings.end(); ++it)
        delete it->second.mGroup;
    mSettings.clear();

    mSettings = other.mSettings;
    //mCallbacks = other.mCallbacks;

    return *this;
}


SettingsParseEvent Settings::ParseConfigObject(
    const std::string& line, std::string& name, std::string& value)
{
    std::string trimmedLine = Trim(line);

    if (trimmedLine.empty())
        return SPE_NONE;
    if (trimmedLine[0] == '#')
        return SPE_COMMENT;
    if (trimmedLine == mEndTag)
        return SPE_END;

    size_t pos = trimmedLine.find('=');
    if (pos == std::string::npos)
        return SPE_INVALID;

    name = Trim(trimmedLine.substr(0, pos));
    value = Trim(trimmedLine.substr(pos + 1));

    if (value == "{")
        return SPE_GROUP;
    if (value == "\"\"\"")
        return SPE_MULTILINE;

    return SPE_KVPAIR;
}


bool Settings::UpdateConfigObject(std::istream& is, std::ostream& os, unsigned int tabDepth)
{
    SettingEntries::const_iterator it;
    std::set<std::string> presentEntries;
    std::string line, name, value;
    bool wasModified = false;
    bool endFound = false;

    // Add any settings that exist in the config file with the current value
    // in the object if existing
    while (is.good() && !endFound) 
    {
        std::getline(is, line);
        SettingsParseEvent evt = ParseConfigObject(line, name, value);

        switch (evt)
        {
            case SPE_END:
                // Skip end tag. Append later.
                endFound = true;
                break;
            case SPE_MULTILINE:
                value = GetMultiline(is);
                /* FALLTHROUGH */
            case SPE_KVPAIR:
                it = mSettings.find(name);
                if (it != mSettings.end() && 
                    (it->second.mGroup || it->second.mValue != value)) 
                {
                    PrintEntry(os, name, it->second, tabDepth);
                    wasModified = true;
                }
                else if (it == mSettings.end()) 
                {
                    // Remove by skipping
                    wasModified = true;
                    break;
                }
                else 
                {
                    os << line << "\n";
                    if (evt == SPE_MULTILINE)
                        os << value << "\n\"\"\"\n";
                }
                presentEntries.insert(name);
                break;
            case SPE_GROUP:
                it = mSettings.find(name);
                if (it != mSettings.end() && it->second.mGroup) 
                {
                    os << line << "\n";
                    LogAssert(it->second.mGroup, "invalid group");
                    wasModified |= it->second.mGroup->UpdateConfigObject(is, os, tabDepth + 1);
                }
                else if (it == mSettings.end()) 
                {
                    // Remove by skipping
                    wasModified = true;
                    Settings removedGroup("}"); // Move 'is' to group end
                    std::stringstream ss;
                    removedGroup.UpdateConfigObject(is, ss, tabDepth + 1);
                    break;
                }
                else 
                {
                    PrintEntry(os, name, it->second, tabDepth);
                    wasModified = true;
                }
                presentEntries.insert(name);
                break;
            default:
                os << line << (is.eof() ? "" : "\n");
                break;
        }
    }

    if (!line.empty() && is.eof())
        os << "\n";

    // Add any settings in the object that don't exist in the config file yet
    for (it = mSettings.begin(); it != mSettings.end(); ++it) 
    {
        if (presentEntries.find(it->first) != presentEntries.end())
            continue;

        PrintEntry(os, it->first, it->second, tabDepth);
        wasModified = true;
    }

    // Append ending tag
    if (!mEndTag.empty()) 
    {
        os << mEndTag << "\n";
        wasModified |= !endFound;
    }

    return wasModified;
}


bool Settings::ReadConfigFile(const char* filename)
{
    std::ifstream is(filename);
    if (!is.good())
        return false;

    return ParseConfigLines(is);
}


bool Settings::UpdateConfigFile(const char* filename)
{
    MutexAutoLock lock(mMutex);

    std::ifstream is(filename);
    std::ostringstream os(std::ios_base::binary);

    bool wasModified = UpdateConfigObject(is, os);
    is.close();

    if (!wasModified)
        return true;

    if (!FileSystem::Get()->SafeWriteToFile(filename, os.str())) 
    {
        LogError("Error writing configuration file: \"" + std::string(filename) + "\"");
        return false;
    }

    return true;
}


bool Settings::ParseConfigLines(std::istream& is)
{
    MutexAutoLock lock(mMutex);

    std::string line, name, value;

    while (is.good()) 
    {
        std::getline(is, line);
        SettingsParseEvent evt = ParseConfigObject(line, name, value);

        switch (evt)
        {
            case SPE_NONE:
            case SPE_INVALID:
            case SPE_COMMENT:
                break;
            case SPE_KVPAIR:
                mSettings[name] = SettingsEntry(value);
                break;
            case SPE_END:
                return true;
            case SPE_GROUP: 
            {
                Settings* group = new Settings("}");
                if (!group->ParseConfigLines(is)) 
                {
                    delete group;
                    return false;
                }
                mSettings[name] = SettingsEntry(group);
                break;
            }
            case SPE_MULTILINE:
                mSettings[name] = SettingsEntry(GetMultiline(is));
                break;
        }
    }

    // false (failure) if end tag not found
    return mEndTag.empty();
}


void Settings::WriteLines(std::ostream& os, unsigned int tabDepth) const
{
    MutexAutoLock lock(mMutex);

    for (auto const& setting : mSettings)
        PrintEntry(os, setting.first, setting.second, tabDepth);

    // For groups this must be "}" !
    if (!mEndTag.empty()) 
    {
        for (unsigned int i = 0; i < tabDepth; i++)
            os << "\t";

        os << mEndTag << "\n";
    }
}


bool Settings::CheckNameValid(const std::string &name)
{
    bool valid = name.find_first_of("=\"{}#") == std::string::npos;
    if (valid)
        valid = std::find_if(name.begin(), name.end(), ::isspace) == name.end();

    if (!valid) 
    {
        LogError("Invalid setting name \"" + name + "\"");
        return false;
    }
    return true;
}


bool Settings::CheckValueValid(const std::string &value)
{
    if (value.substr(0, 3) == "\"\"\"" ||
        value.find("\n\"\"\"") != std::string::npos) 
    {
        LogError("Invalid character sequence '\"\"\"' found in setting value!");
        return false;
    }
    return true;
}


std::string Settings::GetMultiline(std::istream& is, size_t* numLines)
{
    size_t lines = 1;
    std::string value;
    std::string line;

    while (is.good())
    {
        lines++;
        std::getline(is, line);
        if (line == "\"\"\"")
            break;
        value += line;
        value.push_back('\n');
    }

    size_t len = value.size();
    if (len)
        value.erase(len - 1);

    if (numLines)
        *numLines = lines;

    return value;
}


void Settings::PrintEntry(std::ostream& os, const std::string& name,
    const SettingsEntry& entry, unsigned int tabDepth)
{
    for (unsigned int i = 0; i != tabDepth; i++)
        os << "\t";

    if (entry.mGroup) 
    {
        os << name << " = {\n";

        entry.mGroup->WriteLines(os, tabDepth + 1);

        // Closing bracket handled by writeLines
    }
    else 
    {
        os << name << " = ";

        if (entry.mValue.find('\n') != std::string::npos)
            os << "\"\"\"\n" << entry.mValue << "\n\"\"\"\n";
        else
            os << entry.mValue << "\n";
    }
}


void Settings::Init(SettingsLayer sl, const wchar_t* xmlFileName)
{
	// read the XML file
	// if needed, override the XML file with options passed in on the command line.
	mRoot = XmlResourceLoader::LoadAndReturnRootXMLElement(xmlFileName);
    if (!mRoot)
    {
        LogError(
			L"Failed to load game options from file: " + std::wstring(xmlFileName));
        return;
    }
	else
	{
        // Loop through each child element and load the component
		tinyxml2::XMLElement* pNode = NULL;
        pNode = mRoot->FirstChildElement("Keymap");
        if (pNode)
        {
            if (pNode->Attribute("keymap_forward"))
                GetLayer(sl)->Set("keymap_forward", pNode->Attribute("keymap_forward"));
            if (pNode->Attribute("keymap_autoforward"))
                GetLayer(sl)->Set("keymap_autoforward", pNode->Attribute("keymap_autoforward"));
            if (pNode->Attribute("keymap_backward"))
                GetLayer(sl)->Set("keymap_backward", pNode->Attribute("keymap_backward"));
            if (pNode->Attribute("keymap_left"))
                GetLayer(sl)->Set("keymap_left", pNode->Attribute("keymap_left"));
            if (pNode->Attribute("keymap_right"))
                GetLayer(sl)->Set("keymap_right", pNode->Attribute("keymap_right"));
            if (pNode->Attribute("keymap_jump"))
                GetLayer(sl)->Set("keymap_jump", pNode->Attribute("keymap_jump"));
            if (pNode->Attribute("keymap_sneak"))
                GetLayer(sl)->Set("keymap_sneak", pNode->Attribute("keymap_sneak"));
            if (pNode->Attribute("keymap_dig"))
                GetLayer(sl)->Set("keymap_dig", pNode->Attribute("keymap_dig"));
            if (pNode->Attribute("keymap_place"))
                GetLayer(sl)->Set("keymap_place", pNode->Attribute("keymap_place"));
            if (pNode->Attribute("keymap_drop"))
                GetLayer(sl)->Set("keymap_drop", pNode->Attribute("keymap_drop"));
            if (pNode->Attribute("keymap_zoom"))
                GetLayer(sl)->Set("keymap_zoom", pNode->Attribute("keymap_zoom"));
            if (pNode->Attribute("keymap_inventory"))
                GetLayer(sl)->Set("keymap_inventory", pNode->Attribute("keymap_inventory"));
            if (pNode->Attribute("keymap_aux1"))
                GetLayer(sl)->Set("keymap_aux1", pNode->Attribute("keymap_aux1"));
            if (pNode->Attribute("keymap_chat"))
                GetLayer(sl)->Set("keymap_chat", pNode->Attribute("keymap_chat"));
            if (pNode->Attribute("keymap_cmd"))
                GetLayer(sl)->Set("keymap_cmd", pNode->Attribute("keymap_cmd"));
            if (pNode->Attribute("keymap_cmd_local"))
                GetLayer(sl)->Set("keymap_cmd_local", pNode->Attribute("keymap_cmd_local"));
            if (pNode->Attribute("keymap_minimap"))
                GetLayer(sl)->Set("keymap_minimap", pNode->Attribute("keymap_minimap"));
            if (pNode->Attribute("keymap_console"))
                GetLayer(sl)->Set("keymap_console", pNode->Attribute("keymap_console"));
            if (pNode->Attribute("keymap_rangeselect"))
                GetLayer(sl)->Set("keymap_rangeselect", pNode->Attribute("keymap_rangeselect"));
            if (pNode->Attribute("keymap_freemove"))
                GetLayer(sl)->Set("keymap_freemove", pNode->Attribute("keymap_freemove"));
            if (pNode->Attribute("keymap_pitchmove"))
                GetLayer(sl)->Set("keymap_pitchmove", pNode->Attribute("keymap_pitchmove"));
            if (pNode->Attribute("keymap_fastmove"))
                GetLayer(sl)->Set("keymap_fastmove", pNode->Attribute("keymap_fastmove"));
            if (pNode->Attribute("keymap_noclip"))
                GetLayer(sl)->Set("keymap_noclip", pNode->Attribute("keymap_noclip"));
            if (pNode->Attribute("keymap_hotbar_next"))
                GetLayer(sl)->Set("keymap_hotbar_next", pNode->Attribute("keymap_hotbar_next"));
            if (pNode->Attribute("keymap_hotbar_previous"))
                GetLayer(sl)->Set("keymap_hotbar_previous", pNode->Attribute("keymap_hotbar_previous"));
            if (pNode->Attribute("keymap_mute"))
                GetLayer(sl)->Set("keymap_mute", pNode->Attribute("keymap_mute"));
            if (pNode->Attribute("keymap_increase_volume"))
                GetLayer(sl)->Set("keymap_increase_volume", pNode->Attribute("keymap_increase_volume"));
            if (pNode->Attribute("keymap_decrease_volume"))
                GetLayer(sl)->Set("keymap_decrease_volume", pNode->Attribute("keymap_decrease_volume"));
            if (pNode->Attribute("keymap_cinematic"))
                GetLayer(sl)->Set("keymap_cinematic", pNode->Attribute("keymap_cinematic"));
            if (pNode->Attribute("keymap_toggle_hud"))
                GetLayer(sl)->Set("keymap_toggle_hud", pNode->Attribute("keymap_toggle_hud"));
            if (pNode->Attribute("keymap_toggle_chat"))
                GetLayer(sl)->Set("keymap_toggle_chat", pNode->Attribute("keymap_toggle_chat"));
            if (pNode->Attribute("keymap_toggle_fog"))
                GetLayer(sl)->Set("keymap_toggle_fog", pNode->Attribute("keymap_toggle_fog"));
            if (pNode->Attribute("keymap_toggle_update_camera"))
                GetLayer(sl)->Set("keymap_toggle_update_camera", pNode->Attribute("keymap_toggle_update_camera"));
            if (pNode->Attribute("keymap_toggle_debug"))
                GetLayer(sl)->Set("keymap_toggle_debug", pNode->Attribute("keymap_toggle_debug"));
            if (pNode->Attribute("keymap_toggle_profiler"))
                GetLayer(sl)->Set("keymap_toggle_profiler", pNode->Attribute("keymap_toggle_profiler"));
            if (pNode->Attribute("keymap_camera_mode"))
                GetLayer(sl)->Set("keymap_camera_mode", pNode->Attribute("keymap_camera_mode"));
            if (pNode->Attribute("keymap_screenshot"))
                GetLayer(sl)->Set("keymap_screenshot", pNode->Attribute("keymap_screenshot"));
            if (pNode->Attribute("keymap_increase_viewing_range_min"))
                GetLayer(sl)->Set("keymap_increase_viewing_range_min", pNode->Attribute("keymap_increase_viewing_range_min"));
            if (pNode->Attribute("keymap_decrease_viewing_range_min"))
                GetLayer(sl)->Set("keymap_decrease_viewing_range_min", pNode->Attribute("keymap_decrease_viewing_range_min"));
            if (pNode->Attribute("keymap_slot1"))
                GetLayer(sl)->Set("keymap_slot1", pNode->Attribute("keymap_slot1"));
            if (pNode->Attribute("keymap_slot2"))
                GetLayer(sl)->Set("keymap_slot2", pNode->Attribute("keymap_slot2"));
            if (pNode->Attribute("keymap_slot3"))
                GetLayer(sl)->Set("keymap_slot3", pNode->Attribute("keymap_slot3"));
            if (pNode->Attribute("keymap_slot4"))
                GetLayer(sl)->Set("keymap_slot4", pNode->Attribute("keymap_slot4"));
            if (pNode->Attribute("keymap_slot5"))
                GetLayer(sl)->Set("keymap_slot5", pNode->Attribute("keymap_slot5"));
            if (pNode->Attribute("keymap_slot6"))
                GetLayer(sl)->Set("keymap_slot6", pNode->Attribute("keymap_slot6"));
            if (pNode->Attribute("keymap_slot7"))
                GetLayer(sl)->Set("keymap_slot7", pNode->Attribute("keymap_slot7"));
            if (pNode->Attribute("keymap_slot8"))
                GetLayer(sl)->Set("keymap_slot8", pNode->Attribute("keymap_slot8"));
            if (pNode->Attribute("keymap_slot9"))
                GetLayer(sl)->Set("keymap_slot9", pNode->Attribute("keymap_slot9"));
            if (pNode->Attribute("keymap_slot10"))
                GetLayer(sl)->Set("keymap_slot10", pNode->Attribute("keymap_slot10"));
        }

        pNode = mRoot->FirstChildElement("Game");
        if (pNode)
        {
            if (pNode->Attribute("disable_anticheat"))
                GetLayer(sl)->Set("disable_anticheat", pNode->Attribute("disable_anticheat"));
            if (pNode->Attribute("default_game"))
                GetLayer(sl)->Set("default_game", pNode->Attribute("default_game"));
            if (pNode->Attribute("selected_game"))
                GetLayer(sl)->Set("selected_game", pNode->Attribute("selected_game"));
            if (pNode->Attribute("motd"))
                GetLayer(sl)->Set("motd", pNode->Attribute("motd"));
            if (pNode->Attribute("num_ais"))
                GetLayer(sl)->Set("num_ais", pNode->Attribute("num_ais"));
            if (pNode->Attribute("expected_players"))
                GetLayer(sl)->Set("expected_players", pNode->Attribute("expected_players"));
            if (pNode->Attribute("max_ais"))
                GetLayer(sl)->Set("max_ais", pNode->Attribute("max_ais"));
            if (pNode->Attribute("max_players"))
                GetLayer(sl)->Set("max_players", pNode->Attribute("max_players"));
            if (pNode->Attribute("item_entity_ttl"))
                GetLayer(sl)->Set("item_entity_ttl", pNode->Attribute("item_entity_ttl"));
            if (pNode->Attribute("creative_mode"))
                GetLayer(sl)->Set("creative_mode", pNode->Attribute("creative_mode"));
            if (pNode->Attribute("enable_damage"))
                GetLayer(sl)->Set("enable_damage", pNode->Attribute("enable_damage"));
            if (pNode->Attribute("free_move"))
                GetLayer(sl)->Set("free_move", pNode->Attribute("free_move"));
            if (pNode->Attribute("pitch_move"))
                GetLayer(sl)->Set("pitch_move", pNode->Attribute("pitch_move"));
            if (pNode->Attribute("fast_move"))
                GetLayer(sl)->Set("fast_move", pNode->Attribute("fast_move"));
            if (pNode->Attribute("invert_mouse"))
                GetLayer(sl)->Set("invert_mouse", pNode->Attribute("invert_mouse"));
            if (pNode->Attribute("mouse_sensitivity"))
                GetLayer(sl)->Set("mouse_sensitivity", pNode->Attribute("mouse_sensitivity"));
            if (pNode->Attribute("repeat_place_time"))
                GetLayer(sl)->Set("repeat_place_time", pNode->Attribute("repeat_place_time"));
            if (pNode->Attribute("safe_dig_and_place"))
                GetLayer(sl)->Set("safe_dig_and_place", pNode->Attribute("safe_dig_and_place"));
            if (pNode->Attribute("random_input"))
                GetLayer(sl)->Set("random_input", pNode->Attribute("random_input"));
            if (pNode->Attribute("aux1_descends"))
                GetLayer(sl)->Set("aux1_descends", pNode->Attribute("aux1_descends"));
            if (pNode->Attribute("doubletap_jump"))
                GetLayer(sl)->Set("doubletap_jump", pNode->Attribute("doubletap_jump"));
            if (pNode->Attribute("always_fly_fast"))
                GetLayer(sl)->Set("always_fly_fast", pNode->Attribute("always_fly_fast"));
            if (pNode->Attribute("autojump"))
                GetLayer(sl)->Set("autojump", pNode->Attribute("autojump"));
            if (pNode->Attribute("continuous_forward"))
                GetLayer(sl)->Set("continuous_forward", pNode->Attribute("continuous_forward"));
            if (pNode->Attribute("noclip"))
                GetLayer(sl)->Set("noclip", pNode->Attribute("noclip"));
            if (pNode->Attribute("screenshot_path"))
                GetLayer(sl)->Set("screenshot_path", pNode->Attribute("screenshot_path"));
            if (pNode->Attribute("screenshot_format"))
                GetLayer(sl)->Set("screenshot_format", pNode->Attribute("screenshot_format"));
            if (pNode->Attribute("screenshot_quality"))
                GetLayer(sl)->Set("screenshot_quality", pNode->Attribute("screenshot_quality"));
            if (pNode->Attribute("client_unload_unused_data_timeout"))
                GetLayer(sl)->Set("client_unload_unused_data_timeout", pNode->Attribute("client_unload_unused_data_timeout"));
            if (pNode->Attribute("client_mapblock_limit"))
                GetLayer(sl)->Set("client_mapblock_limit", pNode->Attribute("client_mapblock_limit"));
            if (pNode->Attribute("enable_build_where_you_stand"))
                GetLayer(sl)->Set("enable_build_where_you_stand", pNode->Attribute("enable_build_where_you_stand"));
            if (pNode->Attribute("curl_timeout"))
                GetLayer(sl)->Set("curl_timeout", pNode->Attribute("curl_timeout"));
            if (pNode->Attribute("curl_parallel_limit"))
                GetLayer(sl)->Set("curl_parallel_limit", pNode->Attribute("curl_parallel_limit"));
            if (pNode->Attribute("curl_file_download_timeout"))
                GetLayer(sl)->Set("curl_file_download_timeout", pNode->Attribute("curl_file_download_timeout"));
            if (pNode->Attribute("curl_verify_cert"))
                GetLayer(sl)->Set("curl_verify_cert", pNode->Attribute("curl_verify_cert"));
            if (pNode->Attribute("enable_remote_media_server"))
                GetLayer(sl)->Set("enable_remote_media_server", pNode->Attribute("enable_remote_media_server"));
            if (pNode->Attribute("enable_modding"))
                GetLayer(sl)->Set("enable_modding", pNode->Attribute("enable_modding"));
            if (pNode->Attribute("max_out_chat_queue_size"))
                GetLayer(sl)->Set("max_out_chat_queue_size", pNode->Attribute("max_out_chat_queue_size"));
            if (pNode->Attribute("pause_on_lost_focus"))
                GetLayer(sl)->Set("pause_on_lost_focus", pNode->Attribute("pause_on_lost_focus"));
            if (pNode->Attribute("enable_register_confirmation"))
                GetLayer(sl)->Set("enable_register_confirmation", pNode->Attribute("enable_register_confirmation"));
            if (pNode->Attribute("max_clearobjects_extra_loaded_blocks"))
                GetLayer(sl)->Set("max_clearobjects_extra_loaded_blocks", pNode->Attribute("max_clearobjects_extra_loaded_blocks"));
            if (pNode->Attribute("time_speed"))
                GetLayer(sl)->Set("time_speed", pNode->Attribute("time_speed"));
            if (pNode->Attribute("world_start_time"))
                GetLayer(sl)->Set("world_start_time", pNode->Attribute("world_start_time"));
            if (pNode->Attribute("max_objects_per_block"))
                GetLayer(sl)->Set("max_objects_per_block", pNode->Attribute("max_objects_per_block"));
            if (pNode->Attribute("chat_message_max_size"))
                GetLayer(sl)->Set("chat_message_max_size", pNode->Attribute("chat_message_max_size"));
            if (pNode->Attribute("chat_message_format"))
                GetLayer(sl)->Set("chat_message_format", pNode->Attribute("chat_message_format"));
            if (pNode->Attribute("chat_message_limit_per_10sec"))
                GetLayer(sl)->Set("chat_message_limit_per_10sec", pNode->Attribute("chat_message_limit_per_10sec"));
            if (pNode->Attribute("chat_message_limit_trigger_kick"))
                GetLayer(sl)->Set("chat_message_limit_trigger_kick", pNode->Attribute("chat_message_limit_trigger_kick"));
            if (pNode->Attribute("active_block_mgmt_interval"))
                GetLayer(sl)->Set("active_block_mgmt_interval", pNode->Attribute("active_block_mgmt_interval"));
            if (pNode->Attribute("abm_interval"))
                GetLayer(sl)->Set("abm_interval", pNode->Attribute("abm_interval"));
            if (pNode->Attribute("abm_time_budget"))
                GetLayer(sl)->Set("abm_time_budget", pNode->Attribute("abm_time_budget"));
            if (pNode->Attribute("nodetimer_interval"))
                GetLayer(sl)->Set("nodetimer_interval", pNode->Attribute("nodetimer_interval"));
            if (pNode->Attribute("debug_log_level"))
                GetLayer(sl)->Set("debug_log_level", pNode->Attribute("debug_log_level"));
            if (pNode->Attribute("debug_log_size_max"))
                GetLayer(sl)->Set("debug_log_size_max", pNode->Attribute("debug_log_size_max"));
            if (pNode->Attribute("chat_log_level"))
                GetLayer(sl)->Set("chat_log_level", pNode->Attribute("chat_log_level"));
            if (pNode->Attribute("num_emerge_threads"))
                GetLayer(sl)->Set("num_emerge_threads", pNode->Attribute("num_emerge_threads"));
            if (pNode->Attribute("emergequeue_limit_total"))
                GetLayer(sl)->Set("emergequeue_limit_total", pNode->Attribute("emergequeue_limit_total"));
            if (pNode->Attribute("emergequeue_limit_diskonly"))
                GetLayer(sl)->Set("emergequeue_limit_diskonly", pNode->Attribute("emergequeue_limit_diskonly"));
            if (pNode->Attribute("emergequeue_limit_generate"))
                GetLayer(sl)->Set("emergequeue_limit_generate", pNode->Attribute("emergequeue_limit_generate"));
            if (pNode->Attribute("disable_escape_sequences"))
                GetLayer(sl)->Set("disable_escape_sequences", pNode->Attribute("disable_escape_sequences"));
            if (pNode->Attribute("strip_color_codes"))
                GetLayer(sl)->Set("strip_color_codes", pNode->Attribute("strip_color_codes"));
        }

		pNode = mRoot->FirstChildElement("Graphics"); 
		if (pNode)
		{
			if (pNode->Attribute("show_debug"))
                GetLayer(sl)->Set("show_debug", pNode->Attribute("show_debug"));
            if (pNode->Attribute("fsaa"))
                GetLayer(sl)->Set("fsaa", pNode->Attribute("fsaa"));
            if (pNode->Attribute("fps_max"))
                GetLayer(sl)->Set("fps_max", pNode->Attribute("fps_max"));
            if (pNode->Attribute("fps_max_unfocused"))
                GetLayer(sl)->Set("fps_max_unfocused", pNode->Attribute("fps_max_unfocused"));
            if (pNode->Attribute("viewing_range"))
                GetLayer(sl)->Set("viewing_range", pNode->Attribute("viewing_range"));
            if (pNode->Attribute("screen_width"))
                GetLayer(sl)->Set("screen_width", pNode->Attribute("screen_width"));
            if (pNode->Attribute("screen_height"))
                GetLayer(sl)->Set("screen_height", pNode->Attribute("screen_height"));
            if (pNode->Attribute("autosave_screensize"))
                GetLayer(sl)->Set("autosave_screensize", pNode->Attribute("autosave_screensize"));
            if (pNode->Attribute("fullscreen"))
                GetLayer(sl)->Set("fullscreen", pNode->Attribute("fullscreen"));
            if (pNode->Attribute("fullscreen_bpp"))
                GetLayer(sl)->Set("fullscreen_bpp", pNode->Attribute("fullscreen_bpp"));
            if (pNode->Attribute("vsync"))
                GetLayer(sl)->Set("vsync", pNode->Attribute("vsync"));
            if (pNode->Attribute("fov"))
                GetLayer(sl)->Set("fov", pNode->Attribute("fov"));
            if (pNode->Attribute("video_driver"))
                GetLayer(sl)->Set("video_driver", pNode->Attribute("video_driver"));
            if (pNode->Attribute("high_precision_fpu"))
                GetLayer(sl)->Set("high_precision_fpu", pNode->Attribute("high_precision_fpu"));
            if (pNode->Attribute("enable_console"))
                GetLayer(sl)->Set("enable_console", pNode->Attribute("enable_console"));
            if (pNode->Attribute("screen_dpi"))
                GetLayer(sl)->Set("screen_dpi", pNode->Attribute("screen_dpi"));
		}

        pNode = mRoot->FirstChildElement("Visual");
        if (pNode)
        {
            if (pNode->Attribute("undersampling"))
                GetLayer(sl)->Set("undersampling", pNode->Attribute("undersampling"));
            if (pNode->Attribute("world_aligned_mode"))
                GetLayer(sl)->Set("world_aligned_mode", pNode->Attribute("world_aligned_mode"));
            if (pNode->Attribute("autoscale_mode"))
                GetLayer(sl)->Set("autoscale_mode", pNode->Attribute("autoscale_mode"));
            if (pNode->Attribute("enable_fog"))
                GetLayer(sl)->Set("enable_fog", pNode->Attribute("enable_fog"));
            if (pNode->Attribute("fog_start"))
                GetLayer(sl)->Set("fog_start", pNode->Attribute("fog_start"));
            if (pNode->Attribute("mode3d"))
                GetLayer(sl)->Set("mode3d", pNode->Attribute("mode3d"));
            if (pNode->Attribute("paralax3d_strength"))
                GetLayer(sl)->Set("paralax3d_strength", pNode->Attribute("paralax3d_strength"));
            if (pNode->Attribute("tooltip_show_delay"))
                GetLayer(sl)->Set("tooltip_show_delay", pNode->Attribute("tooltip_show_delay"));
            if (pNode->Attribute("tooltip_append_itemname"))
                GetLayer(sl)->Set("tooltip_append_itemname", pNode->Attribute("tooltip_append_itemname"));
            if (pNode->Attribute("leaves_style"))
                GetLayer(sl)->Set("leaves_style", pNode->Attribute("leaves_style"));
            if (pNode->Attribute("connected_glass"))
                GetLayer(sl)->Set("connected_glass", pNode->Attribute("connected_glass"));
            if (pNode->Attribute("smooth_lighting"))
                GetLayer(sl)->Set("smooth_lighting", pNode->Attribute("smooth_lighting"));
            if (pNode->Attribute("lighting_alpha"))
                GetLayer(sl)->Set("lighting_alpha", pNode->Attribute("lighting_alpha"));
            if (pNode->Attribute("lighting_beta"))
                GetLayer(sl)->Set("lighting_beta", pNode->Attribute("lighting_beta"));
            if (pNode->Attribute("display_gamma"))
                GetLayer(sl)->Set("display_gamma", pNode->Attribute("display_gamma"));
            if (pNode->Attribute("lighting_boost"))
                GetLayer(sl)->Set("lighting_boost", pNode->Attribute("lighting_boost"));
            if (pNode->Attribute("lighting_boost_center"))
                GetLayer(sl)->Set("lighting_boost_center", pNode->Attribute("lighting_boost_center"));
            if (pNode->Attribute("lighting_boost_spread"))
                GetLayer(sl)->Set("lighting_boost_spread", pNode->Attribute("lighting_boost_spread"));
            if (pNode->Attribute("texture_path"))
                GetLayer(sl)->Set("texture_path", pNode->Attribute("texture_path"));
            if (pNode->Attribute("shader_path"))
                GetLayer(sl)->Set("shader_path", pNode->Attribute("shader_path"));
            if (pNode->Attribute("cinematic"))
                GetLayer(sl)->Set("cinematic", pNode->Attribute("cinematic"));
            if (pNode->Attribute("camera_smoothing"))
                GetLayer(sl)->Set("camera_smoothing", pNode->Attribute("camera_smoothing"));
            if (pNode->Attribute("cinematic_camera_smoothing"))
                GetLayer(sl)->Set("cinematic_camera_smoothing", pNode->Attribute("cinematic_camera_smoothing"));
            if (pNode->Attribute("enable_clouds"))
                GetLayer(sl)->Set("enable_clouds", pNode->Attribute("enable_clouds"));
            if (pNode->Attribute("view_bobbing_amount"))
                GetLayer(sl)->Set("view_bobbing_amount", pNode->Attribute("view_bobbing_amount"));
            if (pNode->Attribute("fall_bobbing_amount"))
                GetLayer(sl)->Set("fall_bobbing_amount", pNode->Attribute("fall_bobbing_amount"));
            if (pNode->Attribute("enable_3d_clouds"))
                GetLayer(sl)->Set("enable_3d_clouds", pNode->Attribute("enable_3d_clouds"));
            if (pNode->Attribute("cloud_radius"))
                GetLayer(sl)->Set("cloud_radius", pNode->Attribute("cloud_radius"));
            if (pNode->Attribute("menu_clouds"))
                GetLayer(sl)->Set("menu_clouds", pNode->Attribute("menu_clouds"));
            if (pNode->Attribute("opaque_water"))
                GetLayer(sl)->Set("opaque_water", pNode->Attribute("opaque_water"));
            if (pNode->Attribute("console_height"))
                GetLayer(sl)->Set("console_height", pNode->Attribute("console_height"));
            if (pNode->Attribute("console_color"))
                GetLayer(sl)->Set("console_color", pNode->Attribute("console_color"));
            if (pNode->Attribute("console_alpha"))
                GetLayer(sl)->Set("console_alpha", pNode->Attribute("console_alpha"));
            if (pNode->Attribute("form_fullscreen_bg_color"))
                GetLayer(sl)->Set("form_fullscreen_bg_color", pNode->Attribute("form_fullscreen_bg_color"));
            if (pNode->Attribute("form_default_bg_color"))
                GetLayer(sl)->Set("form_default_bg_color", pNode->Attribute("form_default_bg_color"));
            if (pNode->Attribute("selectionbox_color"))
                GetLayer(sl)->Set("selectionbox_color", pNode->Attribute("selectionbox_color"));
            if (pNode->Attribute("selectionbox_width"))
                GetLayer(sl)->Set("selectionbox_width", pNode->Attribute("selectionbox_width"));
            if (pNode->Attribute("node_highlighting"))
                GetLayer(sl)->Set("node_highlighting", pNode->Attribute("node_highlighting"));
            if (pNode->Attribute("crosshair_color"))
                GetLayer(sl)->Set("crosshair_color", pNode->Attribute("crosshair_color"));
            if (pNode->Attribute("crosshair_alpha"))
                GetLayer(sl)->Set("crosshair_alpha", pNode->Attribute("crosshair_alpha"));
            if (pNode->Attribute("recent_chat_messages"))
                GetLayer(sl)->Set("recent_chat_messages", pNode->Attribute("recent_chat_messages"));
            if (pNode->Attribute("chat_font_size"))
                GetLayer(sl)->Set("chat_font_size", pNode->Attribute("chat_font_size"));
            if (pNode->Attribute("hud_scaling"))
                GetLayer(sl)->Set("hud_scaling", pNode->Attribute("hud_scaling"));
            if (pNode->Attribute("gui_scaling"))
                GetLayer(sl)->Set("gui_scaling", pNode->Attribute("gui_scaling"));
            if (pNode->Attribute("gui_scaling_filter"))
                GetLayer(sl)->Set("gui_scaling_filter", pNode->Attribute("gui_scaling_filter"));
            if (pNode->Attribute("gui_scaling_filter_txr2img"))
                GetLayer(sl)->Set("gui_scaling_filter_txr2img", pNode->Attribute("gui_scaling_filter_txr2img"));
            if (pNode->Attribute("desynchronize_mapblock_texture_animation"))
                GetLayer(sl)->Set("desynchronize_mapblock_texture_animation", pNode->Attribute("desynchronize_mapblock_texture_animation"));
            if (pNode->Attribute("hud_hotbar_max_width"))
                GetLayer(sl)->Set("hud_hotbar_max_width", pNode->Attribute("hud_hotbar_max_width"));
            if (pNode->Attribute("enable_local_map_saving"))
                GetLayer(sl)->Set("enable_local_map_saving", pNode->Attribute("enable_local_map_saving"));
            if (pNode->Attribute("show_entity_selectionbox"))
                GetLayer(sl)->Set("show_entity_selectionbox", pNode->Attribute("show_entity_selectionbox"));
            if (pNode->Attribute("texture_clean_transparent"))
                GetLayer(sl)->Set("texture_clean_transparent", pNode->Attribute("texture_clean_transparent"));
            if (pNode->Attribute("texture_min_size"))
                GetLayer(sl)->Set("texture_min_size", pNode->Attribute("texture_min_size"));
            if (pNode->Attribute("ambient_occlusion_gamma"))
                GetLayer(sl)->Set("ambient_occlusion_gamma", pNode->Attribute("ambient_occlusion_gamma"));
            if (pNode->Attribute("enable_shaders"))
                GetLayer(sl)->Set("enable_shaders", pNode->Attribute("enable_shaders"));
            if (pNode->Attribute("enable_particles"))
                GetLayer(sl)->Set("enable_particles", pNode->Attribute("enable_particles"));
            if (pNode->Attribute("arm_inertia"))
                GetLayer(sl)->Set("arm_inertia", pNode->Attribute("arm_inertia"));
            if (pNode->Attribute("show_nametag_backgrounds"))
                GetLayer(sl)->Set("show_nametag_backgrounds", pNode->Attribute("show_nametag_backgrounds"));
            if (pNode->Attribute("enable_minimap"))
                GetLayer(sl)->Set("enable_minimap", pNode->Attribute("enable_minimap"));
            if (pNode->Attribute("minimap_shape_round"))
                GetLayer(sl)->Set("minimap_shape_round", pNode->Attribute("minimap_shape_round"));
            if (pNode->Attribute("minimap_double_scan_height"))
                GetLayer(sl)->Set("minimap_double_scan_height", pNode->Attribute("minimap_double_scan_height"));
            if (pNode->Attribute("directional_colored_fog"))
                GetLayer(sl)->Set("directional_colored_fog", pNode->Attribute("directional_colored_fog"));
            if (pNode->Attribute("inventory_items_animations"))
                GetLayer(sl)->Set("inventory_items_animations", pNode->Attribute("inventory_items_animations"));
            if (pNode->Attribute("mip_map"))
                GetLayer(sl)->Set("mip_map", pNode->Attribute("mip_map"));
            if (pNode->Attribute("anisotropic_filter"))
                GetLayer(sl)->Set("anisotropic_filter", pNode->Attribute("anisotropic_filter"));
            if (pNode->Attribute("bilinear_filter"))
                GetLayer(sl)->Set("bilinear_filter", pNode->Attribute("bilinear_filter"));
            if (pNode->Attribute("trilinear_filter"))
                GetLayer(sl)->Set("trilinear_filter", pNode->Attribute("trilinear_filter"));
            if (pNode->Attribute("tone_mapping"))
                GetLayer(sl)->Set("tone_mapping", pNode->Attribute("tone_mapping"));
            if (pNode->Attribute("enable_waving_water"))
                GetLayer(sl)->Set("enable_waving_water", pNode->Attribute("enable_waving_water"));
            if (pNode->Attribute("water_wave_length"))
                GetLayer(sl)->Set("water_wave_length", pNode->Attribute("water_wave_length"));
            if (pNode->Attribute("water_wave_speed"))
                GetLayer(sl)->Set("water_wave_speed", pNode->Attribute("water_wave_speed"));
            if (pNode->Attribute("enable_waving_leaves"))
                GetLayer(sl)->Set("enable_waving_leaves", pNode->Attribute("enable_waving_leaves"));
            if (pNode->Attribute("enable_waving_plants"))
                GetLayer(sl)->Set("enable_waving_plants", pNode->Attribute("enable_waving_plants"));
            if (pNode->Attribute("liquid_loop_max"))
                GetLayer(sl)->Set("liquid_loop_max", pNode->Attribute("liquid_loop_max"));
            if (pNode->Attribute("liquid_queue_purge_time"))
                GetLayer(sl)->Set("liquid_queue_purge_time", pNode->Attribute("liquid_queue_purge_time"));
            if (pNode->Attribute("liquid_update"))
                GetLayer(sl)->Set("liquid_update", pNode->Attribute("liquid_update"));
            if (pNode->Attribute("mg_name"))
                GetLayer(sl)->Set("mg_name", pNode->Attribute("mg_name"));
            if (pNode->Attribute("water_level"))
                GetLayer(sl)->Set("water_level", pNode->Attribute("water_level"));
            if (pNode->Attribute("mapgen_limit"))
                GetLayer(sl)->Set("mapgen_limit", pNode->Attribute("mapgen_limit"));
            if (pNode->Attribute("chunksize"))
                GetLayer(sl)->Set("chunksize", pNode->Attribute("chunksize"));
            if (pNode->Attribute("fixed_map_seed"))
                GetLayer(sl)->Set("fixed_map_seed", pNode->Attribute("fixed_map_seed"));
            if (pNode->Attribute("enable_mapgen_debug_info"))
                GetLayer(sl)->Set("enable_mapgen_debug_info", pNode->Attribute("enable_mapgen_debug_info"));
            if (pNode->Attribute("enable_mesh_cache"))
                GetLayer(sl)->Set("enable_mesh_cache", pNode->Attribute("enable_mesh_cache"));
            if (pNode->Attribute("mesh_generation_interval"))
                GetLayer(sl)->Set("mesh_generation_interval", pNode->Attribute("mesh_generation_interval"));
            if (pNode->Attribute("meshgen_block_cache_size"))
                GetLayer(sl)->Set("meshgen_block_cache_size", pNode->Attribute("meshgen_block_cache_size"));
            if (pNode->Attribute("enable_vbo"))
                GetLayer(sl)->Set("enable_vbo", pNode->Attribute("enable_vbo"));
        }

		pNode = mRoot->FirstChildElement("Sound"); 
		if (pNode)
		{
            if (pNode->Attribute("music_volume"))
                GetLayer(sl)->Set("music_volume", pNode->Attribute("music_volume"));
            if (pNode->Attribute("sfx_volume"))
                GetLayer(sl)->Set("sfx_volume", pNode->Attribute("sfx_volume"));
            if (pNode->Attribute("enable_sound"))
                GetLayer(sl)->Set("enable_sound", pNode->Attribute("enable_sound"));
            if (pNode->Attribute("sound_volume"))
                GetLayer(sl)->Set("sound_volume", pNode->Attribute("sound_volume"));
            if (pNode->Attribute("mute_sound"))
                GetLayer(sl)->Set("mute_sound", pNode->Attribute("mute_sound"));
		}

		pNode = mRoot->FirstChildElement("Network"); 
		if (pNode)
		{
            if (pNode->Attribute("name"))
                GetLayer(sl)->Set("name", pNode->Attribute("name"));
            if (pNode->Attribute("address"))
                GetLayer(sl)->Set("address", pNode->Attribute("address"));
            if (pNode->Attribute("bind_address"))
                GetLayer(sl)->Set("bind_address", pNode->Attribute("bind_address"));
            if (pNode->Attribute("remote_port"))
                GetLayer(sl)->Set("remote_port", pNode->Attribute("remote_port"));
            if (pNode->Attribute("port"))
                GetLayer(sl)->Set("port", pNode->Attribute("port"));
            if (pNode->Attribute("enable_server"))
                GetLayer(sl)->Set("enable_server", pNode->Attribute("enable_server"));
            if (pNode->Attribute("server_announce"))
                GetLayer(sl)->Set("server_announce", pNode->Attribute("server_announce"));

            if (pNode->Attribute("max_users"))
                GetLayer(sl)->Set("max_users", pNode->Attribute("max_users"));
            if (pNode->Attribute("max_simultaneous_block_sends_per_client"))
                GetLayer(sl)->Set("max_simultaneous_block_sends_per_client", pNode->Attribute("max_simultaneous_block_sends_per_client"));
            if (pNode->Attribute("full_block_send_enable_min_time_from_building"))
                GetLayer(sl)->Set("full_block_send_enable_min_time_from_building", pNode->Attribute("full_block_send_enable_min_time_from_building"));
            if (pNode->Attribute("profiler_print_interval"))
                GetLayer(sl)->Set("profiler_print_interval", pNode->Attribute("profiler_print_interval"));
            if (pNode->Attribute("max_block_send_distance"))
                GetLayer(sl)->Set("max_block_send_distance", pNode->Attribute("max_block_send_distance"));
            if (pNode->Attribute("block_send_optimize_distance"))
                GetLayer(sl)->Set("block_send_optimize_distance", pNode->Attribute("block_send_optimize_distance"));
            if (pNode->Attribute("max_block_generate_distance"))
                GetLayer(sl)->Set("max_block_generate_distance", pNode->Attribute("max_block_generate_distance"));
            if (pNode->Attribute("active_object_send_range_blocks"))
                GetLayer(sl)->Set("active_object_send_range_blocks", pNode->Attribute("active_object_send_range_blocks"));
            if (pNode->Attribute("active_block_range"))
                GetLayer(sl)->Set("active_block_range", pNode->Attribute("active_block_range"));

            if (pNode->Attribute("server_map_save_interval"))
                GetLayer(sl)->Set("server_map_save_interval", pNode->Attribute("server_map_save_interval"));
            if (pNode->Attribute("server_unload_unused_data_timeout"))
                GetLayer(sl)->Set("server_unload_unused_data_timeout", pNode->Attribute("server_unload_unused_data_timeout"));
            if (pNode->Attribute("server_side_occlusion_culling"))
                GetLayer(sl)->Set("server_side_occlusion_culling", pNode->Attribute("server_side_occlusion_culling"));
            if (pNode->Attribute("ignore_world_load_errors"))
                GetLayer(sl)->Set("ignore_world_load_errors", pNode->Attribute("ignore_world_load_errors"));
            if (pNode->Attribute("time_send_interval"))
                GetLayer(sl)->Set("time_send_interval", pNode->Attribute("time_send_interval"));
            if (pNode->Attribute("dedicated_server_step"))
                GetLayer(sl)->Set("dedicated_server_step", pNode->Attribute("dedicated_server_step"));
            if (pNode->Attribute("player_transfer_distance"))
                GetLayer(sl)->Set("player_transfer_distance", pNode->Attribute("player_transfer_distance"));

            if (pNode->Attribute("map_compression_level_disk"))
                GetLayer(sl)->Set("map_compression_level_disk", pNode->Attribute("map_compression_level_disk"));
            if (pNode->Attribute("map_compression_level_net"))
                GetLayer(sl)->Set("map_compression_level_net", pNode->Attribute("map_compression_level_net"));
		}

        pNode = mRoot->FirstChildElement("Physics");
        if (pNode)
        {
            if (pNode->Attribute("debug_draw_wireframe"))
                GetLayer(sl)->Set("debug_draw_wireframe", pNode->Attribute("debug_draw_wireframe"));
            if (pNode->Attribute("debug_draw_contactpoints"))
                GetLayer(sl)->Set("debug_draw_contactpoints", pNode->Attribute("debug_draw_contactpoints"));
            if (pNode->Attribute("movement_acceleration_default"))
                GetLayer(sl)->Set("movement_acceleration_default", pNode->Attribute("movement_acceleration_default"));
            if (pNode->Attribute("movement_acceleration_air"))
                GetLayer(sl)->Set("movement_acceleration_air", pNode->Attribute("movement_acceleration_air"));
            if (pNode->Attribute("movement_acceleration_fast"))
                GetLayer(sl)->Set("movement_acceleration_fast", pNode->Attribute("movement_acceleration_fast"));
            if (pNode->Attribute("movement_speed_walk"))
                GetLayer(sl)->Set("movement_speed_walk", pNode->Attribute("movement_speed_walk"));
            if (pNode->Attribute("movement_speed_crouch"))
                GetLayer(sl)->Set("movement_speed_crouch", pNode->Attribute("movement_speed_crouch"));
            if (pNode->Attribute("movement_speed_fast"))
                GetLayer(sl)->Set("movement_speed_fast", pNode->Attribute("movement_speed_fast"));
            if (pNode->Attribute("movement_speed_climb"))
                GetLayer(sl)->Set("movement_speed_climb", pNode->Attribute("movement_speed_climb"));
            if (pNode->Attribute("movement_speed_jump"))
                GetLayer(sl)->Set("movement_speed_jump", pNode->Attribute("movement_speed_jump"));
            if (pNode->Attribute("movement_liquid_fluidity"))
                GetLayer(sl)->Set("movement_liquid_fluidity", pNode->Attribute("movement_liquid_fluidity"));
            if (pNode->Attribute("movement_liquid_fluidity_smooth"))
                GetLayer(sl)->Set("movement_liquid_fluidity_smooth", pNode->Attribute("movement_liquid_fluidity_smooth"));
            if (pNode->Attribute("movement_liquid_sink"))
                GetLayer(sl)->Set("movement_liquid_sink", pNode->Attribute("movement_liquid_sink"));
            if (pNode->Attribute("movement_gravity"))
                GetLayer(sl)->Set("movement_gravity", pNode->Attribute("movement_gravity"));
            if (pNode->Attribute("default_gravity"))
                GetLayer(sl)->Set("default_gravity", pNode->Attribute("default_gravity"));
        }
	}
}


/***********
 * Getters *
 ***********/

Settings* Settings::GetParent() const
{
    // If the Settings object is within the hierarchy structure,
    // iterate towards the origin (0) to find the next fallback layer
    if (mSettingsLayer >= SL_COUNT)
        return nullptr;

    for (int i = (int)mSettingsLayer - 1; i >= 0; --i) 
        if (mSettingsLayers[i])
            return mSettingsLayers[i];

    // No parent
    return nullptr;
}

const SettingsEntry& Settings::GetEntry(const std::string& name) const
{
    MutexAutoLock lock(mMutex);

    SettingEntries::const_iterator n;
    if ((n = mSettings.find(name)) != mSettings.end())
        return n->second;

    if (auto parent = GetParent())
        return parent->GetEntry(name);

    throw SettingNotFoundException("Setting [" + name + "] not found.");
}

Settings* Settings::GetGroup(const std::string& name) const
{
    const SettingsEntry& entry = GetEntry(name);
    if (!entry.mGroup)
        throw SettingNotFoundException("Setting [" + name + "] is not a group.");
    return entry.mGroup;
}

const std::string& Settings::Get(const std::string& name) const
{
    const SettingsEntry& entry = GetEntry(name);
    if (entry.mGroup)
        throw SettingNotFoundException("Setting [" + name + "] is a group.");
    return entry.mValue;
}

bool Settings::GetBool(const std::string& name) const
{
    return IsYes(Get(name));
}

uint16_t Settings::GetUInt16(const std::string& name) const
{
    return std::clamp(atoi(Get(name).c_str()), 0, 65535);
}

int16_t Settings::GetInt16(const std::string& name) const
{
    return std::clamp(atoi(Get(name).c_str()), -32768, 32767);
}

unsigned int Settings::GetUInt(const std::string& name) const
{
    return (unsigned int)atoi(Get(name).c_str());
}

int Settings::GetInt(const std::string& name) const
{
    return atoi(Get(name).c_str());
}

float Settings::GetFloat(const std::string& name) const
{
    return (float)atof(Get(name).c_str());
}

uint64_t Settings::GetUInt64(const std::string& name) const
{
    uint64_t value = 0;
    std::string str = Get(name);
    std::istringstream ss(str);
    ss >> value;
    return value;
}

Vector2<float> Settings::GetVector2(const std::string& name) const
{
    Vector2<float> value;

    Strfnd f(Get(name));
    f.Next("(");
    value[0] = (float)atof(f.Next(",").c_str());
    value[1] = (float)atof(f.Next(")").c_str());

    return value;
}

Vector3<float> Settings::GetVector3(const std::string& name) const
{
    Vector3<float> value;

    Strfnd f(Get(name));
    f.Next("(");
    value[0] = (float)atof(f.Next(",").c_str());
    value[1] = (float)atof(f.Next(",").c_str());
    value[2] = (float)atof(f.Next(")").c_str());

    return value;
}

Vector4<short> Settings::GetVector4(const std::string& name) const
{
    Vector4<short> value;

    Strfnd f(Get(name));
    f.Next("(");
    value[0] = (short)atoi(f.Next(",").c_str());
    value[1] = (short)atoi(f.Next(",").c_str());
    value[2] = (short)atoi(f.Next(",").c_str());
    value[3] = (short)atoi(f.Next(")").c_str());

    return value;
}

unsigned int Settings::GetFlagString(const std::string& name,
    const FlagDescription* flagdesc, unsigned int* flagmask) const
{
    unsigned int flags = 0;

    // Read default value (if there is any)
    if (auto parent = GetParent())
        flags = parent->GetFlagString(name, flagdesc, flagmask);

    // Apply custom flags "on top"
    if (mSettings.find(name) != mSettings.end()) 
    {
        std::string value = Get(name);
        unsigned int flagsUser;
        unsigned int maskUser = 0xFFFFFFFF;
        flagsUser = std::isdigit(value[0]) ? atoi(value.c_str()) // Override default
            : ReadFlagString(value, flagdesc, &maskUser);

        flags &= ~maskUser;
        flags |= flagsUser;
        if (flagmask)
            *flagmask |= maskUser;
    }

    return flags;
}


bool Settings::Exists(const std::string& name) const
{
    MutexAutoLock lock(mMutex);

    if (mSettings.find(name) != mSettings.end())
        return true;
    if (auto parent = GetParent())
        return parent->Exists(name);
    return false;
}

std::vector<std::string> Settings::GetNames() const
{
    MutexAutoLock lock(mMutex);

    std::vector<std::string> names;
    names.reserve(mSettings.size());
    for (auto const& setting : mSettings) 
        names.push_back(setting.first);
    return names;
}


/***********
 * Setters *
 ***********/

bool Settings::SetEntry(const std::string& name, const void* data, bool setGroup)
{
    if (!CheckNameValid(name))
        return false;
    if (!setGroup && !CheckValueValid(*(const std::string*)data))
        return false;

    Settings* oldGroup = NULL;
    MutexAutoLock lock(mMutex);

    SettingsEntry &entry = mSettings[name];
    oldGroup = entry.mGroup;

    entry.mValue = setGroup ? "" : *(const std::string *)data;
    entry.mGroup = setGroup ? *(Settings **)data : NULL;
    if (setGroup) entry.mGroup->mEndTag = "}";

    delete oldGroup;
    return true;
}

bool Settings::Set(const std::string& name, const std::string& value)
{
    if (!SetEntry(name, &value, false))
        return false;

    DoCallbacks(name);
    return true;
}

bool Settings::SetGroup(const std::string& name, const Settings& group)
{
    // Settings must own the group pointer
    // avoid double-free by copying the source
    Settings* copy = new Settings();
    *copy = group;
    return SetEntry(name, &copy, true);
}

bool Settings::SetBool(const std::string& name, bool value)
{
    return Set(name, value ? "true" : "false");
}

bool Settings::SetInt16(const std::string& name, short value)
{
    return Set(name, std::to_string(value));
}

bool Settings::SetUInt16(const std::string& name, unsigned short value)
{
    return Set(name, std::to_string(value));
}

bool Settings::SetInt(const std::string& name, int value)
{
    return Set(name, std::to_string(value));
}

bool Settings::SetUInt64(const std::string& name, uint64_t value)
{
    std::ostringstream os;
    os << value;
    return Set(name, os.str());
}

bool Settings::SetFloat(const std::string& name, float value)
{
    return Set(name, std::to_string(value));
}

bool Settings::SetVector2(const std::string& name, Vector2<float> value)
{
    std::ostringstream os;
    os << "(" << value[0] << "," << value[1] << ")";
    return Set(name, os.str());
}

bool Settings::SetVector3(const std::string& name, Vector3<float> value)
{
    std::ostringstream os;
    os << "(" << value[0] << "," << value[1] << "," << value[2] << ")";
    return Set(name, os.str());
}

bool Settings::SetVector4(const std::string& name, Vector4<short> value)
{
    std::ostringstream os;
    os << "(" << value[0] << "," << value[1] << "," << value[2] << "," << value[3] << ")";
    return Set(name, os.str());
}

bool Settings::SetFlagString(const std::string& name, unsigned int flags,
    const FlagDescription* flagdesc, unsigned int flagmask)
{
    if (!flagdesc) 
        if (!(flagdesc = GetFlagDescFallback(name)))
            return false; // Not found

    return Set(name, WriteFlagString(flags, flagdesc, flagmask));
}


bool Settings::Remove(const std::string& name)
{
    // Lock as short as possible, unlock before doCallbacks()
    mMutex.lock();

    SettingEntries::iterator it = mSettings.find(name);
    if (it != mSettings.end()) 
    {
        delete it->second.mGroup;
        mSettings.erase(it);
        mMutex.unlock();

        //DoCallbacks(name);
        return true;
    }

    mMutex.unlock();
    return false;
}

void Settings::SetDefault(const std::string& name, const FlagDescription* flagdesc, unsigned int flags)
{
    mSettingFlags[name] = flagdesc;
    GetLayer(SL_DEFAULTS)->Set(name, WriteFlagString(flags, flagdesc, 0xFFFFFFFF));
}

const FlagDescription* Settings::GetFlagDescFallback(const std::string& name) const
{
    auto it = mSettingFlags.find(name);
    return it == mSettingFlags.end() ? nullptr : it->second;
}

void Settings::RegisterChangedCallback(const std::string &name, SettingsChangedCallback cbf, void *userdata)
{
    MutexAutoLock lock(mCallbackMutex);
    mCallbacks[name].emplace_back(cbf, userdata);
}

void Settings::DeregisterChangedCallback(const std::string& name, SettingsChangedCallback cbf, void* userdata)
{
    MutexAutoLock lock(mCallbackMutex);
    SettingsCallbackMap::iterator itCbks = mCallbacks.find(name);

    if (itCbks != mCallbacks.end())
    {
        SettingsCallbackList& cbks = itCbks->second;

        SettingsCallbackList::iterator position =
            std::find(cbks.begin(), cbks.end(), std::make_pair(cbf, userdata));

        if (position != cbks.end())
            cbks.erase(position);
    }
}

void Settings::DoCallbacks(const std::string& name) const
{
    MutexAutoLock lock(mCallbackMutex);

    SettingsCallbackMap::const_iterator itCbks = mCallbacks.find(name);
    if (itCbks != mCallbacks.end())
    {
        SettingsCallbackList::const_iterator it;
        for (it = itCbks->second.begin(); it != itCbks->second.end(); ++it)
            (it->first)(name, it->second);
    }
}