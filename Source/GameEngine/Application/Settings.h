//========================================================================
// Settings.h : Defines utility functions for game options
//
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "GameEngineStd.h"

#include "Core/Utility/StringUtil.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"
#include "Mathematic/Algebra/Vector4.h"

#include <mutex>

// forward definition
class Settings;

// Type for a settings changed callback function
typedef void(*SettingsChangedCallback)(const std::string& name, void* data);
typedef std::vector<std::pair<SettingsChangedCallback, void *>> SettingsCallbackList;
typedef std::unordered_map<std::string, SettingsCallbackList> SettingsCallbackMap;

enum SettingsParseEvent 
{
    SPE_NONE,
    SPE_INVALID,
    SPE_COMMENT,
    SPE_KVPAIR,
    SPE_END,
    SPE_GROUP,
    SPE_MULTILINE
};

enum SettingsLayer 
{
    SL_DEFAULTS,
    SL_GAME,
    SL_GLOBAL,
    SL_MAP,
    SL_COUNT
};

struct SettingsEntry 
{
    SettingsEntry() = default;

    SettingsEntry(const std::string& value) : mValue(value)
    {}

    SettingsEntry(Settings* group) : mGroup(group)
    {}

    std::string mValue = "";
    Settings* mGroup = nullptr;
};

typedef std::unordered_map<std::string, SettingsEntry> SettingEntries;


class Settings
{
public:
    Settings(const std::string &endTag = "");
    ~Settings();

    Settings& operator = (const Settings& other);

	void Init(SettingsLayer sl, const wchar_t* xmlFilePath);

    static Settings* GetLayer(SettingsLayer sl);
    static Settings* CreateLayer(SettingsLayer sl);
    SettingsLayer GetLayerType() const { return mSettingsLayer; }


    /***********************
     * Reading and writing *
     ***********************/

     // Read configuration file.  Returns success.
    bool ReadConfigFile(const char* filename);
    //Updates configuration file.  Returns success.
    bool UpdateConfigFile(const char* filename);
    bool ParseConfigLines(std::istream& is);
    void WriteLines(std::ostream& os, unsigned int tabDepth = 0) const;


    /***********
     * Getters *
     ***********/

    Settings* GetParent() const;
    const SettingsEntry& GetEntry(const std::string& name) const;
    Settings* GetGroup(const std::string& name) const;
    const std::string& Get(const std::string& name) const;
    bool GetBool(const std::string& name) const;
    uint16_t GetUInt16(const std::string& name) const;
    int16_t GetInt16(const std::string& name) const;
    unsigned int GetUInt(const std::string& name) const;
    int GetInt(const std::string& name) const;
    uint64_t GetUInt64(const std::string& name) const;
    float GetFloat(const std::string& name) const;
    Vector2<float> GetVector2(const std::string& name) const;
    Vector3<float> GetVector3(const std::string& name) const;
    Vector4<short> GetVector4(const std::string& name) const;
    unsigned int GetFlagString(const std::string& name,
        const FlagDescription* flagdesc, unsigned int* flagmask) const;

    // return all keys used
    std::vector<std::string> GetNames() const;
    bool Exists(const std::string& name) const;


    /***********
     * Setters *
     ***********/

     // N.B. Groups not allocated with new must be set to NULL in the settings
     // tree before object destruction.
    bool SetEntry(const std::string& name, const void* entry, bool setGroup);
    bool Set(const std::string& name, const std::string& value);
    bool SetGroup(const std::string& name, const Settings& group);
    bool SetBool(const std::string& name, bool value);
    bool SetInt16(const std::string& name, short value);
    bool SetUInt16(const std::string& name, unsigned short value);
    bool SetInt(const std::string& name, int value);
    bool SetUInt64(const std::string& name, uint64_t value);
    bool SetFloat(const std::string& name, float value);
    bool SetVector2(const std::string& name, Vector2<float> value);
    bool SetVector3(const std::string& name, Vector3<float> value);
    bool SetVector4(const std::string& name, Vector4<short> value);
    bool SetFlagString(const std::string& name, unsigned int flags,
        const FlagDescription* flagdesc, unsigned int flagmask);

    // remove a setting
    bool Remove(const std::string& name);

    /**************
     * Miscellany *
     **************/

    void SetDefault(const std::string &name, const FlagDescription *flagdesc, unsigned int flags);
    const FlagDescription* GetFlagDescFallback(const std::string& name) const;

    void RegisterChangedCallback(const std::string& name, SettingsChangedCallback cbf, void* userdata = NULL);
    void DeregisterChangedCallback(const std::string& name, SettingsChangedCallback cbf, void* userdata = NULL);


    // Getter for the main global system. This is the system that is used by the majority of the 
    // engine, though you are free to define your own as long as you instantiate it.
    // It is not valid to have more than one global system.
    static Settings* Get(void);

    // XMLElement - look at this to find other options added by the developer
    tinyxml2::XMLElement *mRoot;


protected:

    static bool CheckNameValid(const std::string& name);
    static bool CheckValueValid(const std::string& value);
    static std::string GetMultiline(std::istream &is, size_t* numLines = NULL);
    static void PrintEntry(std::ostream& os, const std::string& name,
        const SettingsEntry &entry, unsigned int tabDepth = 0);

    /***********************
     * Reading and writing *
     ***********************/

    SettingsParseEvent ParseConfigObject(const std::string& line, std::string& name, std::string& value);
    bool UpdateConfigObject(std::istream& is, std::ostream& os, unsigned int tabDepth = 0);

    SettingEntries mSettings;
    //SettingsCallbackMap m_callbacks;
    std::string mEndTag;

    static Settings* mSettingsLayers[SL_COUNT];
    SettingsLayer mSettingsLayer = SL_COUNT;

    static Settings* mSetting;

private:

    void DoCallbacks(const std::string &name) const;

    SettingsCallbackMap mCallbacks;

    // All methods that access m_settings/m_defaults directly should lock this.
    mutable std::mutex mMutex;

    mutable std::mutex mCallbackMutex;

    static std::unordered_map<std::string, const FlagDescription *> mSettingFlags;
};


#endif