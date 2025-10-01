//========================================================================
// MinecraftEvents.h : defines game-specific events for Minecraft
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
//    http://code.google.com/p/GameEngine/
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


#ifndef MINECRAFTEVENTS_H
#define MINECRAFTEVENTS_H

#include "Data/SkyParams.h"
#include "Data/ParticleParams.h"

#include "Games/Map/MapBlock.h"

#include "Core/Utility/Chat.h"

#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "Core/Utility/EnrichedString.h"
#include "Core/Utility/StringUtil.h"


//---------------------------------------------------------------------------------------------------------------------
// class EventDataOpenContentStore
//---------------------------------------------------------------------------------------------------------------------
class EventDataOpenContentStore : public EventData
{

public:
    static const BaseEventType skEventType;

    EventDataOpenContentStore(void) { }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataOpenContentStore());
    }

    virtual const char* GetName(void) const
    {
        return "EventDataOpenContentStore";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataDeleteContentStore
//---------------------------------------------------------------------------------------------------------------------
class EventDataDeleteContentStore : public EventData
{
    std::string mGameLevel;

public:
    static const BaseEventType skEventType;

    EventDataDeleteContentStore(void) { }
    EventDataDeleteContentStore(const std::string& level) : mGameLevel(level)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataDeleteContentStore(mGameLevel));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mGameLevel;
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mGameLevel;
    }

    const std::string& GetLevel(void) const
    {
        return mGameLevel;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataDeleteContentStore";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataOpenGameSelection
//---------------------------------------------------------------------------------------------------------------------
class EventDataOpenGameSelection : public EventData
{

public:
    static const BaseEventType skEventType;

    EventDataOpenGameSelection(void) { }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataOpenGameSelection());
    }

    virtual const char* GetName(void) const
    {
        return "EventDataOpenGameSelection";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataChangeGameSelection
//---------------------------------------------------------------------------------------------------------------------
class EventDataChangeGameSelection : public EventData
{
    std::string mGameId;
    std::string mGameName;

public:
    static const BaseEventType skEventType;

    EventDataChangeGameSelection(void) { }
    EventDataChangeGameSelection(const std::string& gameId, const std::string& game)
        : mGameName(game), mGameId(gameId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataChangeGameSelection(mGameId, mGameName));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mGameId;
        out << mGameName;
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mGameId;
        in >> mGameName;
    }

    const std::string& GetGameName(void) const
    {
        return mGameName;
    }

    const std::string& GetGameId(void) const
    {
        return mGameId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataChangeGameSelection";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataGameUIUpdate				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class EventDataGameUIUpdate : public EventData
{
    std::string mGameplayUiString;

public:
    static const BaseEventType skEventType;

    EventDataGameUIUpdate(void) { }
    EventDataGameUIUpdate(const std::string& gameplayUiString)
	: mGameplayUiString(gameplayUiString)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataGameUIUpdate(mGameplayUiString));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
		out << mGameplayUiString;
    }

    virtual void Deserialize(std::istrstream& in)
    {
		in >> mGameplayUiString;
    }

    const std::string& GetUiString(void) const
    {
        return mGameplayUiString;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataGameUIUpdate";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataSetActorController				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class EventDataSetActorController : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataSetActorController(void) { }
    EventDataSetActorController(ActorId actorId)
        : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataSetActorController(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId;
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataSetActorController";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudAdd
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudAdd : public EventData
{
    unsigned int mId;
    uint8_t mType;
    Vector2<float> mPosition;
    std::string mHudName;
    Vector2<float> mScale;
    std::string mText;
    unsigned int mNumber;
    unsigned int mItem;
    unsigned int mDirection;
    Vector2<float> mAlign;
    Vector2<float> mOffset;
    Vector3<float> mWorldPosition;
    Vector2<int> mSize;
    short mZIndex;
    std::string mText2;

public:
    static const BaseEventType skEventType;

    EventDataHudAdd(void) { }
    EventDataHudAdd(unsigned int id, uint8_t type, const Vector2<float>& position,
        const std::string& name, const Vector2<float>& scale, const std::string& text, 
        unsigned int number, unsigned int item, unsigned int direction, const Vector2<float>& align, 
        const Vector2<float>& offset, const Vector3<float>& worldPosition, const Vector2<int>& size, 
        short zIndex, const std::string& text2) : mId(id), mType(type), mPosition(position), 
        mHudName(name), mScale(scale), mText(text), mNumber(number), mItem(item), mDirection(direction), 
        mAlign(align), mOffset(offset), mWorldPosition(worldPosition), mSize(size), mZIndex(zIndex), mText2(text2)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHudAdd(mId, mType, mPosition, mHudName, mScale, mText, 
            mNumber, mItem, mDirection, mAlign, mOffset, mWorldPosition, mSize, mZIndex, mText2));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mType << " ";
        for (int i = 0; i < 2; ++i)
            out << mPosition[i] << " ";
        out << mHudName << " ";
        for (int i = 0; i < 2; ++i)
            out << mScale[i] << " ";
        out << mText << " ";
        out << mNumber << " ";
        out << mDirection << " ";
        for (int i = 0; i < 2; ++i)
            out << mAlign[i] << " ";
        for (int i = 0; i < 2; ++i)
            out << mOffset[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mWorldPosition[i] << " ";
        for (int i = 0; i < 2; ++i)
            out << mSize[i] << " ";
        out << mZIndex << " ";
        out << mText2 << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mType;
        for (int i = 0; i < 2; ++i)
            in >> mPosition[i];
        in >> mHudName;
        for (int i = 0; i < 2; ++i)
            in >> mScale[i];
        in >> mText;
        in >> mNumber;
        in >> mDirection;
        for (int i = 0; i < 2; ++i)
            in >> mAlign[i];
        for (int i = 0; i < 2; ++i)
            in >> mOffset[i];
        for (int i = 0; i < 3; ++i)
            in >> mWorldPosition[i];
        for (int i = 0; i < 2; ++i)
            in >> mSize[i];
        in >> mZIndex;
        in >> mText2;
    }

    const unsigned int GetId(void) const
    {
        return mId;
    }

    const uint8_t GetType(void) const
    {
        return mType;
    }

    const Vector2<float>& GetPosition(void) const
    {
        return mPosition;
    }

    const std::string& GetHudName(void) const
    {
        return mHudName;
    }

    const Vector2<float>& GetScale(void) const
    {
        return mScale;
    }

    const std::string& GetText(void) const
    {
        return mText;
    }

    const unsigned int GetNumber(void) const
    {
        return mNumber;
    }

    const unsigned int GetItem(void) const
    {
        return mItem;
    }

    const unsigned int GetDirection(void) const
    {
        return mDirection;
    }

    const Vector2<float>& GetAlign(void) const
    {
        return mAlign;
    }

    const Vector2<float>& GetOffset(void) const
    {
        return mOffset;
    }

    const Vector3<float>& GetWorldPosition(void) const
    {
        return mWorldPosition;
    }

    const Vector2<int>& GetSize(void) const
    {
        return mSize;
    }

    const short GetZIndex(void) const
    {
        return mZIndex;
    }

    const std::string& GetText2(void) const
    {
        return mText2;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudAdd";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudRemove
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudRemove : public EventData
{
    unsigned int mId;

public:
    static const BaseEventType skEventType;

    EventDataHudRemove(void) { }
    EventDataHudRemove(unsigned int id) : mId(id)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHudRemove(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const unsigned int GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudRemove";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudChange
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudChange : public EventData
{
    unsigned int mId;
    unsigned char mStat;
    void* mValue;

public:
    static const BaseEventType skEventType;

    EventDataHudChange(void) { }
    EventDataHudChange(unsigned int id, unsigned char stat, void* value) : mId(id), mStat(stat), mValue(value)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHudChange(mId, mStat, mValue));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mStat << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mStat;
    }

    const unsigned int GetId(void) const
    {
        return mId;
    }

    const unsigned int GetStat(void) const
    {
        return mStat;
    }

    const void* GetValue(void) const
    {
        return mValue;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudChange";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudSetFlags
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudSetFlags : public EventData
{
    unsigned int mMask;
    unsigned int mFlags;

public:
    static const BaseEventType skEventType;

    EventDataHudSetFlags(void) { }
    EventDataHudSetFlags(unsigned int mask, unsigned int flags) : mMask(mask), mFlags(flags)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHudSetFlags(mMask, mFlags));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mMask << " ";
        out << mFlags << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mMask;
        in >> mFlags;
    }

    const unsigned int GetMask(void) const
    {
        return mMask;
    }

    const unsigned int GetFlags(void) const
    {
        return mFlags;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudSetFlags";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudSetParam
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudSetParam : public EventData
{
    uint16_t mParam;
    std::string mValue;

public:
    static const BaseEventType skEventType;

    EventDataHudSetParam(void) { }
    EventDataHudSetParam(uint16_t param, std::string value) : mParam(param), mValue(value)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHudSetParam(mParam, mValue));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mParam << " ";
        out << mValue << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mParam;
        in >> mValue;
    }

    const uint16_t GetParam(void) const
    {
        return mParam;
    }

    const std::string GetValue(void) const
    {
        return mValue;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudSetParam";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudSetSky
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudSetSky : public EventData
{
    SColor mBGColor;
    std::string mType;
    std::vector<std::string> mTextures;
    bool mClouds;
    SkyColor mSkyColor;
    SColor mFogSunTint;
    SColor mFogMoonTint;
    std::string mFogTintType;

public:
    static const BaseEventType skEventType;

    EventDataHudSetSky(void) { }
    EventDataHudSetSky(const SColor& bgColor, const std::string& type, bool clouds,
        const SColor& fogSunTint, const SColor& fogMoonTint, const std::string& fogTintType, 
        const SkyColor& skyColor, const std::vector<std::string>& textures) :
        mBGColor(bgColor), mType(type), mClouds(clouds), mFogSunTint(fogSunTint), 
        mFogMoonTint(fogMoonTint), mFogTintType(fogTintType), mSkyColor(skyColor), mTextures(textures)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHudSetSky(
            mBGColor, mType, mClouds, mFogSunTint, mFogMoonTint, mFogTintType, mSkyColor, mTextures));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mBGColor.mColor << " ";
        out << mType << " ";

        out << mClouds << " ";
        out << mFogSunTint.mColor << " ";
        out << mFogMoonTint.mColor << " ";
        out << mFogTintType << " ";
        out << mSkyColor.daySky.mColor << " ";
        out << mSkyColor.dayHorizon.mColor << " ";
        out << mSkyColor.dawnSky.mColor << " ";
        out << mSkyColor.dawnHorizon.mColor << " ";
        out << mSkyColor.nightSky.mColor << " ";
        out << mSkyColor.nightHorizon.mColor << " ";
        out << mSkyColor.indoors.mColor << " ";
        for (std::string texture : mTextures)
            out << texture << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mBGColor.mColor;
        in >> mType;

        in >> mClouds;
        in >> mFogSunTint.mColor;
        in >> mFogMoonTint.mColor;
        in >> mFogTintType;
        in >> mSkyColor.daySky.mColor;
        in >> mSkyColor.dayHorizon.mColor;
        in >> mSkyColor.dawnSky.mColor;
        in >> mSkyColor.dawnHorizon.mColor;
        in >> mSkyColor.nightSky.mColor;
        in >> mSkyColor.nightHorizon.mColor;
        in >> mSkyColor.indoors.mColor;
        for (std::string texture : mTextures)
            in >> texture;
    }

    const SColor& GetBGColor(void) const
    {
        return mBGColor;
    }

    const std::string& GetType(void) const
    {
        return mType;
    }

    const bool GetClouds(void) const
    {
        return mClouds;
    }

    const SColor& GetFogSunTint(void) const
    {
        return mFogSunTint;
    }

    const SColor& GetFogMoonTint(void) const
    {
        return mFogMoonTint;
    }

    const std::string& GetFogTintType(void) const
    {
        return mFogTintType;
    }

    const SkyColor& GetSkyColor(void) const
    {
        return mSkyColor;
    }

    const std::vector<std::string>& GetTextures(void) const
    {
        return mTextures;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudSetSky";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudSetSun
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudSetSun : public EventData
{
    std::string mTexture;
    std::string mToneMap;
    std::string mSunrise;
    bool mSunriseVisible;
    bool mVisible;
    float mScale;

public:
    static const BaseEventType skEventType;

    EventDataHudSetSun(void) { }
    EventDataHudSetSun(const std::string& texture, const std::string& toneMap, 
        const std::string& sunrise, bool sunriseVisible, bool visible, float scale) :
        mTexture(texture), mToneMap(toneMap), mSunrise(sunrise), 
        mSunriseVisible(sunriseVisible), mVisible(visible), mScale(scale)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(
            new EventDataHudSetSun(mTexture, mToneMap, mSunrise, mSunriseVisible, mVisible, mScale));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mTexture << " ";
        out << mToneMap << " ";
        out << mSunrise << " ";

        out << mSunriseVisible << " ";
        out << mVisible << " ";
        out << mScale << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mTexture;
        in >> mToneMap;
        in >> mSunrise;

        in >> mSunriseVisible;
        in >> mVisible;
        in >> mScale;
    }

    const std::string& GetTexture(void) const
    {
        return mTexture;
    }

    const std::string& GetToneMap(void) const
    {
        return mToneMap;
    }

    const std::string& GetSunrise(void) const
    {
        return mSunrise;
    }

    const bool GetSunriseVisible(void) const
    {
        return mSunriseVisible;
    }

    const bool GetVisible(void) const
    {
        return mVisible;
    }

    const float GetScale(void) const
    {
        return mScale;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudSetSun";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudSetMoon
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudSetMoon : public EventData
{
    std::string mTexture;
    std::string mToneMap;
    bool mVisible;
    float mScale;

public:
    static const BaseEventType skEventType;

    EventDataHudSetMoon(void) { }
    EventDataHudSetMoon(const std::string& texture, const std::string& toneMap,  bool visible, float scale) :
        mTexture(texture), mToneMap(toneMap), mVisible(visible), mScale(scale)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(
            new EventDataHudSetMoon(mTexture, mToneMap, mVisible, mScale));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mTexture << " ";
        out << mToneMap << " ";

        out << mVisible << " ";
        out << mScale << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mTexture;
        in >> mToneMap;

        in >> mVisible;
        in >> mScale;
    }

    const std::string& GetTexture(void) const
    {
        return mTexture;
    }

    const std::string& GetToneMap(void) const
    {
        return mToneMap;
    }

    const bool GetVisible(void) const
    {
        return mVisible;
    }

    const float GetScale(void) const
    {
        return mScale;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudSetMoon";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHudSetStars
//---------------------------------------------------------------------------------------------------------------------
class EventDataHudSetStars : public EventData
{
    SColor mStarColor;
    unsigned int mCount;
    bool mVisible;
    float mScale;

public:
    static const BaseEventType skEventType;

    EventDataHudSetStars(void) { }
    EventDataHudSetStars(const SColor& color, unsigned int count, bool visible, float scale) :
        mStarColor(color), mCount(count), mVisible(visible), mScale(scale)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(
            new EventDataHudSetStars(mStarColor, mCount, mVisible, mScale));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mStarColor.mColor << " ";
        out << mCount << " ";

        out << mVisible << " ";
        out << mScale << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mStarColor.mColor;
        in >> mCount;

        in >> mVisible;
        in >> mScale;
    }

    const SColor& GetColor(void) const
    {
        return mStarColor;
    }

    const unsigned int GetCount(void) const
    {
        return mCount;
    }

    const bool GetVisible(void) const
    {
        return mVisible;
    }

    const float GetScale(void) const
    {
        return mScale;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHudSetStars";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataSetClouds
//---------------------------------------------------------------------------------------------------------------------
class EventDataSetClouds : public EventData
{
    SColor mColorBright;
    SColor mColorAmbient;

    Vector2<float> mSpeed;

    float mThickness;
    float mDensity;
    float mHeight;

public:
    static const BaseEventType skEventType;

    EventDataSetClouds(void) { }
    EventDataSetClouds(const SColor& colorBright, const SColor& colorAmbient,
        const Vector2<float>& speed, float thickness, float density, float height) :
        mColorBright(colorBright), mColorAmbient(colorAmbient), mSpeed(speed), 
        mThickness(thickness), mDensity(density), mHeight(height)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataSetClouds(
            mColorBright, mColorAmbient, mSpeed, mThickness, mDensity, mHeight));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mColorBright.mColor << " ";
        out << mColorAmbient.mColor << " ";
        for (int i = 0; i < 2; ++i)
            out << mSpeed[i] << " ";

        out << mThickness << " ";
        out << mDensity << " ";
        out << mHeight << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mColorBright.mColor;
        in >> mColorAmbient.mColor;
        for (int i = 0; i < 2; ++i)
            in >> mSpeed[i];

        in >> mThickness;
        in >> mDensity;
        in >> mHeight;
    }

    const SColor& GetBrightColor(void) const
    {
        return mColorBright;
    }

    const SColor& GetAmbientColor(void) const
    {
        return mColorAmbient;
    }

    const Vector2<float>& GetSpeed(void) const
    {
        return mSpeed;
    }

    const float GetThickness(void) const
    {
        return mThickness;
    }

    const float GetDensity(void) const
    {
        return mDensity;
    }

    const float GetHeight(void) const
    {
        return mHeight;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataSetClouds";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataTimeOfDay
//---------------------------------------------------------------------------------------------------------------------
class EventDataTimeOfDay : public EventData
{
    unsigned int mTimeDay; 
    float mTimeSpeed;

public:
    static const BaseEventType skEventType;

    EventDataTimeOfDay(void) { }
    EventDataTimeOfDay(unsigned int timeDay, float timeSpeed) : mTimeDay(timeDay), mTimeSpeed(timeSpeed)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataTimeOfDay(mTimeDay, mTimeSpeed));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mTimeDay;
        out << mTimeSpeed;
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mTimeDay;
        in >> mTimeSpeed;
    }

    const unsigned int GetTimeDay(void) const
    {
        return mTimeDay;
    }

    const float GetTimeSpeed(void) const
    {
        return mTimeSpeed;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataTimeOfDay";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataOverrideDayNightRatio
//---------------------------------------------------------------------------------------------------------------------
class EventDataOverrideDayNightRatio : public EventData
{
    bool mOverride;
    float mRatio;

public:
    static const BaseEventType skEventType;

    EventDataOverrideDayNightRatio(void) { }
    EventDataOverrideDayNightRatio(bool override, float ratio) : mOverride(override), mRatio(ratio)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataOverrideDayNightRatio(mOverride, mRatio));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mOverride << " ";
        out << mRatio << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mOverride;
        in >> mRatio;
    }

    const unsigned int GetOverride(void) const
    {
        return mOverride;
    }

    const float GetRatio(void) const
    {
        return mRatio;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataOverrideDayNightRatio";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataGameInit
//---------------------------------------------------------------------------------------------------------------------
class EventDataGameInit : public EventData
{

public:
    static const BaseEventType skEventType;

    EventDataGameInit(void) { }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataGameInit());
    }

    virtual const char* GetName(void) const
    {
        return "EventDataGameInit";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataGameReady
//---------------------------------------------------------------------------------------------------------------------
class EventDataGameReady : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataGameReady(void) { }
    EventDataGameReady(ActorId actorId) { mId = actorId; }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataGameReady(mId));
    }


    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataGameReady";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataMovement
//---------------------------------------------------------------------------------------------------------------------
class EventDataMovement : public EventData
{
    ActorId mId;
    float mGravity;
    float mAccelDefault, mAccelAir, mAccelFast;
    float mLiquidFluidity, mLiquidFluiditySmooth, mLiquidSink;
    float mSpeedWalk, mSpeedCrouch, mSpeedFast, mSpeedClimb, mSpeedJump;

public:
    static const BaseEventType skEventType;

    EventDataMovement(void) { }
    EventDataMovement(ActorId actorId, 
        float gravity, float accelDefault, float accelAir, float accelFast,
        float liquidFluidity, float liquidFluiditySmooth, float liquidSink,
        float speedWalk, float speedCrouch, float speedFast, float speedClimb, float speedJump) : 
        mId(actorId), mGravity(gravity), mAccelDefault(accelDefault), mAccelAir(accelAir), mAccelFast(accelFast),
        mLiquidFluidity(liquidFluidity), mLiquidFluiditySmooth(liquidFluiditySmooth), mLiquidSink(liquidSink),
        mSpeedWalk(speedWalk), mSpeedCrouch(speedCrouch), mSpeedFast(speedFast), mSpeedClimb(speedClimb), mSpeedJump(speedJump)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataMovement(mId, mGravity, mAccelDefault, mAccelAir, mAccelFast,
            mLiquidFluidity, mLiquidFluiditySmooth, mLiquidSink, mSpeedWalk, mSpeedCrouch, mSpeedFast, mSpeedClimb, mSpeedJump));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mGravity << " ";
        out << mAccelDefault << " ";
        out << mAccelAir << " ";
        out << mAccelFast << " ";
        out << mLiquidFluidity << " ";
        out << mLiquidFluiditySmooth << " ";
        out << mLiquidSink << " ";
        out << mSpeedWalk << " ";
        out << mSpeedCrouch << " ";
        out << mSpeedFast << " ";
        out << mSpeedClimb << " ";
        out << mSpeedJump << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mGravity;
        in >> mAccelDefault;
        in >> mAccelAir;
        in >> mAccelFast;
        in >> mLiquidFluidity;
        in >> mLiquidFluiditySmooth;
        in >> mLiquidSink;
        in >> mSpeedWalk;
        in >> mSpeedCrouch;
        in >> mSpeedFast;
        in >> mSpeedClimb;
        in >> mSpeedJump;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    float GetGravity(void) const
    {
        return mGravity;
    }

    float GetAccelDefault(void) const
    {
        return mAccelDefault;
    }

    float GetAccelAir(void) const
    {
        return mAccelAir;
    }

    float GetAccelFast(void) const
    {
        return mAccelFast;
    }

    float GetLiquidFluidity(void) const
    {
        return mLiquidFluidity;
    }

    float GetLiquidFluiditySmooth(void) const
    {
        return mLiquidFluiditySmooth;
    }

    float GetLiquidSink(void) const
    {
        return mLiquidSink;
    }

    float GetSpeedWalk(void) const
    {
        return mSpeedWalk;
    }

    float GetSpeedCrouch(void) const
    {
        return mSpeedCrouch;
    }

    float GetSpeedFast(void) const
    {
        return mSpeedFast;
    }

    float GetSpeedClimb(void) const
    {
        return mSpeedClimb;
    }

    float GetSpeedJump(void) const
    {
        return mSpeedJump;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataMovement";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataInteract
//---------------------------------------------------------------------------------------------------------------------
class EventDataInteract : public EventData
{
    ActorId mId;
    uint8_t mAction;
    unsigned int mWieldIndex;
    std::string mPointedThing;

    short mPitch, mYaw;
    unsigned int mKeyPressed;
    uint8_t mFov, mWantedRange;
    Vector3<int> mPosition, mSpeed;

public:
    static const BaseEventType skEventType;

    EventDataInteract(void) { }
    EventDataInteract(
        ActorId actorId, unsigned int keyPressed, uint8_t action, 
        uint16_t wieldIndex, const std::string& pointedThing,
        const Vector3<int>& position, const Vector3<int>& speed,
        short pitch, short yaw, uint8_t fov, uint8_t wantedRange) :
        mId(actorId), mAction(action), mWieldIndex(wieldIndex), mPointedThing(pointedThing), 
        mPosition(position), mSpeed(speed), mPitch(pitch), mYaw(yaw), mKeyPressed(keyPressed), 
        mFov(fov), mWantedRange(wantedRange)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataInteract(mId, mKeyPressed, mAction, 
            mWieldIndex, mPointedThing, mPosition, mSpeed, mPitch, mYaw, mFov, mWantedRange));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mAction << " ";
        out << mWieldIndex << " ";
        out << mPointedThing << " ";
        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mSpeed[i] << " ";

        out << mPitch << " ";
        out << mYaw << " ";
        out << mKeyPressed << " ";
        out << mFov << " ";
        out << mWantedRange << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mAction;
        in >> mWieldIndex;
        in >> mPointedThing;
        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];
        for (int i = 0; i < 3; ++i)
            in >> mSpeed[i];

        in >> mPitch;
        in >> mYaw;
        in >> mKeyPressed;
        in >> mFov;
        in >> mWantedRange;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }


    uint8_t GetAction(void) const
    {
        return mAction;
    }


    uint16_t GetWieldIndex(void) const
    {
        return mWieldIndex;
    }


    const std::string& GetPointedThing(void) const
    {
        return mPointedThing;
    }

    const Vector3<int>& GetPosition(void) const
    {
        return mPosition;
    }

    const Vector3<int>& GetSpeed(void) const
    {
        return mSpeed;
    }

    short GetPitch(void) const
    {
        return mPitch;
    }

    short GetYaw(void) const
    {
        return mYaw;
    }

    uint8_t GetFov(void) const
    {
        return mFov;
    }

    uint8_t GetWantedRange(void) const
    {
        return mWantedRange;
    }

    unsigned short GetKeyPressed(void) const
    {
        return mKeyPressed;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataInteract";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlaySoundAt
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlaySoundAt : public EventData
{
    std::string mSoundName;
    Vector3<float> mPosition;

    float mGain;
    float mPitch;

    bool mLoop;

public:
    static const BaseEventType skEventType;

    EventDataPlaySoundAt(void) { }
    EventDataPlaySoundAt(std::string name, Vector3<float> pos, 
        float gain, float pitch = 1.0f, bool loop = false) :
        mSoundName(name), mPosition(pos), mLoop(loop), mGain(gain), mPitch(pitch)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlaySoundAt(mSoundName, mPosition, mGain, mPitch, mLoop));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mSoundName << " ";

        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";

        out << mGain << " ";
        out << mPitch << " ";
        out << mLoop << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mSoundName;

        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];

        in >> mGain;
        in >> mPitch;
        in >> mLoop;
    }

    const std::string& GetSoundName(void) const
    {
        return mSoundName;
    }

    const Vector3<float>& GetPosition(void) const
    {
        return mPosition;
    }

    float GetGain(void) const
    {
        return mGain;
    }

    float GetPitch(void) const
    {
        return mPitch;
    }

    bool IsLoop(void) const
    {
        return mLoop;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlaySoundAt";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlaySoundType
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlaySoundType : public EventData
{
    int mId;
    std::string mSoundName;
    unsigned char mType; // 0=local, 1=positional, 2=object

    Vector3<float> mPosition;
    unsigned short mObjectId; 

    float mGain;
    float mFade;
    float mPitch;

    bool mEphemeral;
    bool mLoop;

public:
    static const BaseEventType skEventType;

    EventDataPlaySoundType(void) { }
    EventDataPlaySoundType(int id, std::string name, unsigned char type, Vector3<float> pos, 
        unsigned short objectId, float gain, float fade = 0.0f, float pitch = 1.0f, 
        bool ephemeral = false, bool loop = false) : mId(id), mSoundName(name), mType(type), mPosition(pos),
        mObjectId(objectId), mLoop(loop), mEphemeral(ephemeral), mGain(gain), mFade(fade), mPitch(pitch)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlaySoundType(
            mId, mSoundName, mType, mPosition, mObjectId, mGain, mFade, mPitch, mEphemeral, mLoop));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mType << " ";
        out << mSoundName << " ";

        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";
        out << mObjectId << " ";

        out << mGain << " ";
        out << mFade << " ";
        out << mPitch << " ";
        out << mEphemeral << " ";
        out << mLoop << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mType;
        in >> mSoundName;

        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];
        in >> mObjectId;

        in >> mGain;
        in >> mFade;
        in >> mPitch;
        in >> mEphemeral;
        in >> mLoop;
    }

    int GetId(void) const
    {
        return mId;
    }

    unsigned char GetType(void) const
    {
        return mType;
    }

    const std::string& GetSoundName(void) const
    {
        return mSoundName;
    }

    const Vector3<float>& GetPosition(void) const
    {
        return mPosition;
    }

    int GetObjectId(void) const
    {
        return mObjectId;
    }

    float GetGain(void) const
    {
        return mGain;
    }

    float GetFade(void) const
    {
        return mFade;
    }

    float GetPitch(void) const
    {
        return mPitch;
    }

    bool IsEphemeral(void) const
    {
        return mEphemeral;
    }

    bool IsLoop(void) const
    {
        return mLoop;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlaySoundType";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataStopSound
//---------------------------------------------------------------------------------------------------------------------
class EventDataStopSound : public EventData
{
    int mId;

public:
    static const BaseEventType skEventType;

    EventDataStopSound(void) { }
    EventDataStopSound(int id) : mId(id)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataStopSound(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    int GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataStopSound";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataFadeSound
//---------------------------------------------------------------------------------------------------------------------
class EventDataFadeSound : public EventData
{
    int mId;

    float mStep;
    float mGain;

public:
    static const BaseEventType skEventType;

    EventDataFadeSound(void) { }
    EventDataFadeSound(int id, float step, float gain) : mId(id), mStep(step), mGain(gain)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataFadeSound(mId, mStep, mGain));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mStep << " ";
        out << mGain << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mStep;
        in >> mGain;
    }

    int GetId(void) const
    {
        return mId;
    }

    float GetStep(void) const
    {
        return mStep;
    }

    float GetGain(void) const
    {
        return mGain;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataFadeSound";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataRemoveSounds
//---------------------------------------------------------------------------------------------------------------------
class EventDataRemoveSounds : public EventData
{
    std::vector<int> mSoundIds;

public:
    static const BaseEventType skEventType;

    EventDataRemoveSounds(void) { }
    EventDataRemoveSounds(const std::vector<int>& ids) : mSoundIds(ids)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataRemoveSounds(mSoundIds));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        for (unsigned int i = 0; i < mSoundIds.size(); ++i)
            out << mSoundIds[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        for (unsigned int i = 0; i < mSoundIds.size(); ++i)
            in >> mSoundIds[i];
    }

    const std::vector<int>& GetIds(void) const
    {
        return mSoundIds;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataRemoveSounds";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerEyeOffset
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerEyeOffset : public EventData
{
    ActorId mId;
    Vector3<float> mFirst, mThird;

public:
    static const BaseEventType skEventType;

    EventDataPlayerEyeOffset(void) { }
    EventDataPlayerEyeOffset(ActorId actorId, const Vector3<float>& first, const Vector3<float>& third) :
        mId(actorId), mFirst(first), mThird(third)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerEyeOffset(mId, mFirst, mThird));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        for (int i = 0; i < 3; ++i)
            out << mFirst[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mThird[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        for (int i = 0; i < 3; ++i)
            in >> mFirst[i];
        for (int i = 0; i < 3; ++i)
            in >> mThird[i];
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const Vector3<float>& GetFirst(void) const
    {
        return mFirst;
    }
    
    const Vector3<float>& GetThird(void) const
    {
        return mThird;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerEyeOffset";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerAnimations
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerAnimations : public EventData
{
    ActorId mId;
    float mSpeed;
    std::vector<Vector2<short>> mFrames;

public:
    static const BaseEventType skEventType;

    EventDataPlayerAnimations(void) { }
    EventDataPlayerAnimations(ActorId actorId, float speed, const std::vector<Vector2<short>>& frames) : 
        mId(actorId), mSpeed(speed), mFrames(frames)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerAnimations(mId, mSpeed, mFrames));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mSpeed << " ";

        for (Vector2<short> frame : mFrames)
            out << frame[0] << " " << frame[1] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mSpeed;

        for (Vector2<short> frame : mFrames)
            in >> frame[0] >> frame[1];
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const float GetSpeed(void) const
    {
        return mSpeed;
    }

    const std::vector<Vector2<short>>& GetFrames(void) const
    {
        return mFrames;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerAnimations";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerHP
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerHP : public EventData
{
    ActorId mId;
    unsigned short mHP;

public:
    static const BaseEventType skEventType;

    EventDataPlayerHP(void) { }
    EventDataPlayerHP(ActorId actorId, unsigned short hp) : mId(actorId), mHP(hp)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerHP(mId, mHP));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mHP << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mHP;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const unsigned int GetHP(void) const
    {
        return mHP;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerHP";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerBreath
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerBreath : public EventData
{
    ActorId mId;
    unsigned short mBreath;

public:
    static const BaseEventType skEventType;

    EventDataPlayerBreath(void) { }
    EventDataPlayerBreath(ActorId actorId, unsigned short hp) : mId(actorId), mBreath(hp)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerBreath(mId, mBreath));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mBreath << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mBreath;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const unsigned int GetBreath(void) const
    {
        return mBreath;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerBreath";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerItem
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerItem : public EventData
{
    ActorId mId;
    unsigned short mItem;

public:
    static const BaseEventType skEventType;

    EventDataPlayerItem(void) { }
    EventDataPlayerItem(ActorId actorId, unsigned short item) : mId(actorId), mItem(item)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerItem(mId, mItem));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mItem << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mItem;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const unsigned short GetItem(void) const
    {
        return mItem;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerItem";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerSpeed
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerSpeed : public EventData
{
    ActorId mId;
    Vector3<float> mVelocity;

public:
    static const BaseEventType skEventType;

    EventDataPlayerSpeed(void) { }
    EventDataPlayerSpeed(ActorId actorId, const Vector3<float>& vel) : mId(actorId), mVelocity(vel)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerSpeed(mId, mVelocity));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        for (int i = 0; i < 3; ++i)
            out << mVelocity[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        for (int i = 0; i < 3; ++i)
            in >> mVelocity[i];
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const Vector3<float>& GetVelocity(void) const
    {
        return mVelocity;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerSpeed";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerPosition
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerPosition : public EventData
{
    ActorId mId;

    int mPitch, mYaw;
    unsigned int mKeyPressed;
    Vector3<int> mPosition, mSpeed;
    uint8_t mFov, mWantedRange;

public:
    static const BaseEventType skEventType;

    EventDataPlayerPosition(void) { }
    EventDataPlayerPosition(ActorId actorId, unsigned int keyPressed,
        const Vector3<int>& position, const Vector3<int>& speed,
        int pitch, int yaw, uint8_t fov, uint8_t wantedRange) :
        mId(actorId), mPosition(position), mSpeed(speed), mPitch(pitch),
        mYaw(yaw), mKeyPressed(keyPressed), mFov(fov), mWantedRange(wantedRange)
    {

    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerPosition(
            mId, mKeyPressed, mPosition, mSpeed, mPitch, mYaw, mFov, mWantedRange));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mSpeed[i] << " ";

        out << mPitch << " ";
        out << mYaw << " ";
        out << mKeyPressed << " ";
        out << mFov << " ";
        out << mWantedRange << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];
        for (int i = 0; i < 3; ++i)
            in >> mSpeed[i];

        in >> mPitch;
        in >> mYaw;
        in >> mKeyPressed;
        in >> mFov;
        in >> mWantedRange;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const Vector3<int>& GetPosition(void) const
    {
        return mPosition;
    }

    const Vector3<int>& GetSpeed(void) const
    {
        return mSpeed;
    }

    int GetPitch(void) const
    {
        return mPitch;
    }

    int GetYaw(void) const
    {
        return mYaw;
    }

    uint8_t GetFov(void) const
    {
        return mFov;
    }

    uint8_t GetWantedRange(void) const
    {
        return mWantedRange;
    }

    unsigned int GetKeyPressed(void) const
    {
        return mKeyPressed;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerPosition";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerInventoryForm
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerInventoryForm : public EventData
{
    ActorId mId;
    std::string mInventoryForm;

public:
    static const BaseEventType skEventType;

    EventDataPlayerInventoryForm(void) { }
    EventDataPlayerInventoryForm(ActorId actorId, const std::string& inventoryForm) :
        mId(actorId), mInventoryForm(inventoryForm)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerInventoryForm(mId, mInventoryForm));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mInventoryForm << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mInventoryForm;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const std::string& GetInventoryForm(void) const
    {
        return mInventoryForm;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerInventoryForm";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// EventDataPlayerRespawn - sent when actors are spawned
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerRespawn : public EventData
{
    ActorId mId;
    std::string mMessage;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataPlayerRespawn(void) { }

    EventDataPlayerRespawn(const ActorId& id, const std::string& message="") : mId(id), mMessage(message)
    {

    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mMessage << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mMessage;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerRespawn(mId, mMessage));
    }

    const ActorId& GetId()
    {
        return mId;
    }

    const std::string& GetString(void) const
    {
        return mMessage;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerRespawn";
    }

};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerRegainGround
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerRegainGround : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataPlayerRegainGround(void) { }
    EventDataPlayerRegainGround(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerRegainGround(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerRegainGround";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerMove
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerMove : public EventData
{
    ActorId mId;
    Vector3<float> mPosition;
    EulerAngles<float> mRotation;

public:
    static const BaseEventType skEventType;

    EventDataPlayerMove(void) { }
    EventDataPlayerMove(ActorId actorId, const Vector3<float>& position, const EulerAngles<float>& rotation) 
        : mId(actorId), mPosition(position), mRotation(rotation)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerMove(mId, mPosition, mRotation));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mRotation.mAxis[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mRotation.mAngle[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];
        for (int i = 0; i < 3; ++i)
            in >> mRotation.mAxis[i];
        for (int i = 0; i < 3; ++i)
            in >> mRotation.mAngle[i];
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const Vector3<float>& GetPosition(void) const
    {
        return mPosition;
    }

    const EulerAngles<float>& GetRotation(void) const
    {
        return mRotation;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerMove";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerJump
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerJump : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataPlayerJump(void) { }
    EventDataPlayerJump(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerJump(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerJump";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerDamage
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerDamage : public EventData
{
    ActorId mId;
    uint16_t mDamage;

public:
    static const BaseEventType skEventType;

    EventDataPlayerDamage(void) { }
    EventDataPlayerDamage(ActorId actorId, uint16_t damage) : mId(actorId), mDamage(damage)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerDamage(mId, mDamage));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mDamage << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mDamage;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    uint16_t GetDamage(void) const
    {
        return mDamage;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerDamage";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataPlayerFallingDamage
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayerFallingDamage : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataPlayerFallingDamage(void) { }
    EventDataPlayerFallingDamage(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataPlayerFallingDamage(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataPlayerFallingDamage";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataDeleteParticleSpawner
//---------------------------------------------------------------------------------------------------------------------
class EventDataDeleteParticleSpawner : public EventData
{
    unsigned int mId;

public:
    static const BaseEventType skEventType;

    EventDataDeleteParticleSpawner(void) { }
    EventDataDeleteParticleSpawner(unsigned int id) : mId(id)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataDeleteParticleSpawner(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    unsigned int GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataDeleteParticleSpawner";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataAddParticleSpawner
//---------------------------------------------------------------------------------------------------------------------
class EventDataAddParticleSpawner : public EventData
{
    unsigned int mId, mAttachedId;
    bool mCollisionDetection;
    bool mCollisionRemoval;
    bool mObjectCollision;
    bool mVertical;
    std::string mTexture;
    std::string mAnimation;
    uint8_t mGlow;
    uint8_t mNodeTile;
    uint8_t mNodeParam2;
    uint16_t mNodeParam0;

    float mTime;
    uint16_t mAmount;
    Vector3<float> mMinPos, mMaxPos, mMinVel, mMaxVel, mMinAcc, mMaxAcc;
    float mMinExpTime, mMaxExpTime, mMinSize, mMaxSize;

public:
    static const BaseEventType skEventType;

    EventDataAddParticleSpawner(void) { }
    EventDataAddParticleSpawner(
        unsigned int id, unsigned int attachedId,
        const std::string& texture, const std::string& animation,
        bool collDetection, bool collRemoval, bool objectColl, bool vertical, uint8_t glow, 
        uint8_t nodeTile, uint8_t ndeParam2, uint16_t nodeParam0, float time, uint16_t amount,
        const Vector3<float>& minPos, const Vector3<float>& maxPos, const Vector3<float>& minVel, 
        const Vector3<float>& maxVel, const Vector3<float>& minAcc, const Vector3<float>& maxAcc,
        float minExpTime, float maxExpTime, float minSize, float maxSize) : mId(id), mTexture(texture), mAnimation(animation),
        mCollisionDetection(collDetection), mCollisionRemoval(collRemoval), mObjectCollision(objectColl), mVertical(vertical), 
        mGlow(glow), mNodeTile(nodeTile), mNodeParam2(ndeParam2), mNodeParam0(nodeParam0), mTime(time), mAmount(amount), 
        mMinPos(minPos), mMaxPos(maxPos), mMinVel(minVel),mMaxVel(maxVel), mMinAcc(minAcc), mMaxAcc(maxAcc), 
        mMinExpTime(minExpTime), mMaxExpTime(maxExpTime), mMinSize(minSize), mMaxSize(maxSize)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataAddParticleSpawner(mId, mAttachedId, mTexture, mAnimation,
            mCollisionDetection, mCollisionRemoval, mObjectCollision, mVertical, mGlow, mNodeTile, mNodeParam2, mNodeParam0, 
            mTime, mAmount, mMinPos, mMaxPos, mMinVel, mMaxVel, mMinAcc, mMaxAcc, mMinExpTime, mMaxExpTime, mMinSize, mMaxSize));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mAttachedId << " ";
        out << mTexture << " ";
        out << mAnimation << " ";

        out << mCollisionDetection << " ";
        out << mCollisionRemoval << " ";
        out << mObjectCollision << " ";
        out << mVertical << " ";
        out << mGlow << " ";
        out << mNodeTile << " ";
        out << mNodeParam2 << " ";
        out << mNodeParam0 << " ";
        out << mTime << " ";
        out << mAmount << " ";

        for (int i = 0; i < 3; ++i)
            out << mMinPos[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mMaxPos[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mMinVel[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mMaxVel[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mMinAcc[i] << " ";
        for (int i = 0; i < 3; ++i)
            out << mMaxAcc[i] << " ";

        out << mMinExpTime << " ";
        out << mMaxExpTime << " ";
        out << mMinSize << " ";
        out << mMaxSize << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mAttachedId;
        in >> mTexture;
        in >> mAnimation;

        in >> mCollisionDetection;
        in >> mCollisionRemoval;
        in >> mObjectCollision;
        in >> mVertical;
        in >> mGlow;
        in >> mNodeTile;
        in >> mNodeParam2;
        in >> mNodeParam0;
        in >> mTime;
        in >> mAmount;

        for (int i = 0; i < 3; ++i)
            in >> mMinPos[i];
        for (int i = 0; i < 3; ++i)
            in >> mMaxPos[i];
        for (int i = 0; i < 3; ++i)
            in >> mMinVel[i];
        for (int i = 0; i < 3; ++i)
            in >> mMaxVel[i];
        for (int i = 0; i < 3; ++i)
            in >> mMinAcc[i];
        for (int i = 0; i < 3; ++i)
            in >> mMaxAcc[i];

        in >> mMinExpTime;
        in >> mMaxExpTime;
        in >> mMinSize;
        in >> mMaxSize;
    }

    unsigned int GetId(void) const
    {
        return mId;
    }

    unsigned int GetAttachedId(void) const
    {
        return mAttachedId;
    }

    const std::string& GetTexture(void) const
    {
        return mTexture;
    }

    const std::string& GetAnimation(void) const
    {
        return mAnimation;
    }

    bool GetCollisionDetection(void) const
    {
        return mCollisionDetection;
    }

    bool GetCollisionRemoval(void) const
    {
        return mCollisionRemoval;
    }

    bool GetObjectCollision(void) const
    {
        return mObjectCollision;
    }

    bool GetVertical(void) const
    {
        return mVertical;
    }

    uint8_t GetGlow(void) const
    {
        return mGlow;
    }

    uint8_t GetNodeTile(void) const
    {
        return mNodeTile;
    }

    uint8_t GetNodeParam2(void) const
    {
        return mNodeParam2;
    }

    uint16_t GetNodeParam0(void) const
    {
        return mNodeParam0;
    }

    float GetTime(void) const
    {
        return mTime;
    }

    uint16_t GetAmount(void) const
    {
        return mAmount;
    }

    const Vector3<float>& GetMinPos(void) const
    {
        return mMinPos;
    }

    const Vector3<float>& GetMaxPos(void) const
    {
        return mMaxPos;
    }

    const Vector3<float>& GetMinVel(void) const
    {
        return mMinVel;
    }

    const Vector3<float>& GetMaxVel(void) const
    {
        return mMaxVel;
    }

    const Vector3<float>& GetMinAcc(void) const
    {
        return mMinAcc;
    }

    const Vector3<float>& GetMaxAcc(void) const
    {
        return mMaxAcc;
    }

    float GetMinExpTime(void) const
    {
        return mMinExpTime;
    }

    float GetMaxExpTime(void) const
    {
        return mMaxExpTime;
    }

    float GetMinSize(void) const
    {
        return mMinSize;
    }

    float GetMaxSize(void) const
    {
        return mMaxSize;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataAddParticleSpawner";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataSpawnParticle
//---------------------------------------------------------------------------------------------------------------------
class EventDataSpawnParticle : public EventData
{
    std::string mData;

public:
    static const BaseEventType skEventType;

    EventDataSpawnParticle(void) { }
    EventDataSpawnParticle(const std::string& data) : mData(data)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataSpawnParticle(mData));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mData << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
    }

    const std::string& GetData(void) const
    {
        return mData;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataSpawnParticle";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataViewBobbingStep
//---------------------------------------------------------------------------------------------------------------------
class EventDataViewBobbingStep : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataViewBobbingStep(void) { }
    EventDataViewBobbingStep(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataViewBobbingStep(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataViewBobbingStep";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataCameraPunchLeft
//---------------------------------------------------------------------------------------------------------------------
class EventDataCameraPunchLeft : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataCameraPunchLeft(void) { }
    EventDataCameraPunchLeft(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataCameraPunchLeft(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataCameraPunchLeft";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataCameraPunchRight
//---------------------------------------------------------------------------------------------------------------------
class EventDataCameraPunchRight : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataCameraPunchRight(void) { }
    EventDataCameraPunchRight(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataCameraPunchRight(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataCameraPunchRight";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataMapNodeRemove
//---------------------------------------------------------------------------------------------------------------------
class EventDataMapNodeRemove : public EventData
{
    Vector3<short> mPoint;

public:
    static const BaseEventType skEventType;

    EventDataMapNodeRemove(void) { }
    EventDataMapNodeRemove(const Vector3<short>& point) : mPoint(point)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataMapNodeRemove(mPoint));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        //out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        //in >> mId;
    }

    const Vector3<short>& GetPoint()
    {
        return mPoint;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataMapNodeRemove";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataMapNodeAdd
//---------------------------------------------------------------------------------------------------------------------
class EventDataMapNodeAdd : public EventData
{
    Vector3<short> mPoint;
    MapNode mNode;

public:
    static const BaseEventType skEventType;

    EventDataMapNodeAdd(void) { }
    EventDataMapNodeAdd(const Vector3<short>& point, const MapNode& node) :
        mPoint(point), mNode(node)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataMapNodeAdd(mPoint, mNode));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        //out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        //in >> mId;
    }

    const Vector3<short>& GetPoint()
    {
        return mPoint;
    }

    const MapNode& GetNode()
    {
        return mNode;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataMapNodeAdd";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataMapNodeDug
//---------------------------------------------------------------------------------------------------------------------
class EventDataMapNodeDug : public EventData
{
    Vector3<short> mPoint;
    MapNode mNode;

public:
    static const BaseEventType skEventType;

    EventDataMapNodeDug(void) { }
    EventDataMapNodeDug(const Vector3<short>& point, const MapNode& node) : 
        mPoint(point), mNode(node)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataMapNodeDug(mPoint, mNode));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        //out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        //in >> mId;
    }

    const MapNode& GetNode()
    {
        return mNode;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataMapNodeDug";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// EventDataActiveObjectRemoveAdd
//---------------------------------------------------------------------------------------------------------------------
class EventDataActiveObjectRemoveAdd : public EventData
{
    std::string mData;
    unsigned int mSize;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataActiveObjectRemoveAdd(void) { }

    EventDataActiveObjectRemoveAdd(const std::string& data, unsigned int size) : mData(data), mSize(size)
    {

    }

    virtual void Serialize(std::ostrstream &out) const
    {
        out << mData << " ";
        out << mSize << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
        in >> mSize;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataActiveObjectRemoveAdd(mData, mSize));
    }

    virtual const char* GetName(void) const
    {
        return "EventDataActiveObjectRemoveAdd";
    }

    const std::string& GetData(void) const
    {
        return mData;
    }

    unsigned int GetSize(void) const
    {
        return mSize;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// EventDataActiveObjectMessages
//---------------------------------------------------------------------------------------------------------------------
class EventDataActiveObjectMessages : public EventData
{
    std::string mData;
    unsigned int mSize;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataActiveObjectMessages(void) { }

    EventDataActiveObjectMessages(const std::string& data, unsigned int size) : mData(data), mSize(size)
    {

    }

    virtual void Serialize(std::ostrstream &out) const
    {
        out << mData << " ";
        out << mSize << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
        in >> mSize;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataActiveObjectMessages(mData, mSize));
    }

    virtual const char* GetName(void) const
    {
        return "EventDataActiveObjectMessages";
    }

    const std::string& GetData(void) const
    {
        return mData;
    }

    unsigned int GetSize(void) const
    {
        return mSize;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// EventDataShowForm
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowForm : public EventData
{
    std::string mForm;
    std::string mFormName;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataShowForm(void) { }

    EventDataShowForm(const std::string& form, const std::string& formName) : 
        mForm(form), mFormName(formName)
    {

    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mForm << " ";
        out << mFormName << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mForm;
        in >> mFormName;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataShowForm(mForm, mFormName));
    }

    const std::string& GetFormName(void) const
    {
        return mFormName;
    }

    const std::string& GetForm(void) const
    {
        return mForm;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataShowForm";
    }

};


//---------------------------------------------------------------------------------------------------------------------
// EventDataDeathScreen
//---------------------------------------------------------------------------------------------------------------------
class EventDataDeathScreen : public EventData
{
    bool mSetCameraPointTarget;
    Vector3<float> mCameraPointTarget;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataDeathScreen(void) { }

    EventDataDeathScreen(bool setPointTarget, const Vector3<float>& pointTarget) : 
        mSetCameraPointTarget(setPointTarget), mCameraPointTarget(pointTarget)
    {

    }

    virtual void Serialize(std::ostrstream &out) const
    {
        out << mSetCameraPointTarget << " ";
        for (int i = 0; i < 3; ++i)
            out << mCameraPointTarget[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mSetCameraPointTarget;
        for (int i = 0; i < 3; ++i)
            in >> mCameraPointTarget[i];
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataDeathScreen(mSetCameraPointTarget, mCameraPointTarget));
    }

    virtual const char* GetName(void) const
    {
        return "EventDataDeathScreen";
    }

    const Vector3<float>& GetCameraPointTarget(void) const
    {
        return mCameraPointTarget;
    }

    bool SetCameraPointTarget(void) const
    {
        return mSetCameraPointTarget;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// EventDataUpdateChat
//---------------------------------------------------------------------------------------------------------------------
class EventDataUpdateChat : public EventData
{
    unsigned int mLineCount;
    EnrichedString mChat;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataUpdateChat(void) { }

    EventDataUpdateChat(unsigned int lineCount, const EnrichedString& chat) : mLineCount(lineCount), mChat(chat)
    {

    }

    virtual void Serialize(std::ostrstream &out) const
    {
        out << mLineCount << " ";
        out << mChat.C_Str() << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mLineCount;
        in >> ToString(mChat.C_Str());
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataUpdateChat(mLineCount, mChat));
    }

    virtual const char* GetName(void) const
    {
        return "EventDataUpdateChat";
    }

    const EnrichedString& GetChat(void) const
    {
        return mChat;
    }

    unsigned int GetLineCount(void) const
    {
        return mLineCount;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// EventDataInitChat
//---------------------------------------------------------------------------------------------------------------------
class EventDataInitChat : public EventData
{
    ChatBackend* mChat;

public:
    static const BaseEventType skEventType;

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    EventDataInitChat(void) { }

    EventDataInitChat(ChatBackend* chat) : mChat(chat)
    {

    }

    virtual void Serialize(std::ostrstream &out) const
    {
    }

    virtual void Deserialize(std::istrstream& in)
    {
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataInitChat(mChat));
    }

    virtual const char* GetName(void) const
    {
        return "EventDataInitChat";
    }

    ChatBackend* GetChat(void) const
    {
        return mChat;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataChangePassword
//---------------------------------------------------------------------------------------------------------------------
class EventDataChangePassword : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataChangePassword(void) { }
    EventDataChangePassword(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataChangePassword(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataChangePassword";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataChangeVolume
//---------------------------------------------------------------------------------------------------------------------
class EventDataChangeVolume : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataChangeVolume(void) { }
    EventDataChangeVolume(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataChangeVolume(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataChangeVolume";
    }
};



//---------------------------------------------------------------------------------------------------------------------
// class EventDataChangeMenu
//---------------------------------------------------------------------------------------------------------------------
class EventDataChangeMenu : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

    EventDataChangeMenu(void) { }
    EventDataChangeMenu(ActorId actorId) : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataChangeMenu(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataChangeMenu";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataSaveBlockData
//---------------------------------------------------------------------------------------------------------------------
class EventDataSaveBlockData : public EventData
{
    MapBlock* mBlock;

public:
    static const BaseEventType skEventType;

    EventDataSaveBlockData(void) { }
    EventDataSaveBlockData(MapBlock* block) : mBlock(block)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataSaveBlockData(mBlock));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
    }

    virtual void Deserialize(std::istrstream& in)
    {
    }

    MapBlock* GetBlock() const
    {
        return mBlock;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataSaveBlockData";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleBlockData
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleBlockData : public EventData
{
    std::string mData;
    Vector3<short> mPosition;

public:
    static const BaseEventType skEventType;

    EventDataHandleBlockData(void) { }
    EventDataHandleBlockData(const std::string& data, const Vector3<short>& pos) : mData(data), mPosition(pos)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleBlockData(mData, mPosition));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mData << " ";
        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];
    }

    const std::string& GetData() const
    {
        return mData;
    }

   const Vector3<short>& GetPosition(void) const
    {
        return mPosition;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleBlockData";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataDeletedBlocks
//---------------------------------------------------------------------------------------------------------------------
class EventDataDeletedBlocks : public EventData
{
    std::vector<Vector3<short>> mBlocks;

public:
    static const BaseEventType skEventType;

    EventDataDeletedBlocks(void) { }
    EventDataDeletedBlocks(const std::vector<Vector3<short>>& blocks) : mBlocks(blocks)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataDeletedBlocks(mBlocks));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        for (auto block : mBlocks)
            for (int i = 0; i < 3; ++i)
                out << block[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        for (auto block : mBlocks)
            for (int i = 0; i < 3; ++i)
                in >> block[i];
    }

    const std::vector<Vector3<short>>& GetBlocks(void) const
    {
        return mBlocks;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataDeletedBlocks";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataGotBlocks
//---------------------------------------------------------------------------------------------------------------------
class EventDataGotBlocks : public EventData
{
    std::vector<Vector3<short>> mBlocks;

public:
    static const BaseEventType skEventType;

    EventDataGotBlocks(void) { }
    EventDataGotBlocks(const std::vector<Vector3<short>>& blocks) : mBlocks(blocks)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataGotBlocks(mBlocks));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        for (auto block : mBlocks)
            for (int i = 0; i < 3; ++i)
                out << block[i] << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        for (auto block : mBlocks)
            for (int i = 0; i < 3; ++i)
                in >> block[i];
    }

    const std::vector<Vector3<short>>& GetBlocks(void) const
    {
        return mBlocks;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataGotBlocks";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleNodes
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleNodes : public EventData
{
    std::string mData;

public:
    static const BaseEventType skEventType;

    EventDataHandleNodes(void) { }
    EventDataHandleNodes(const std::string& data) : mData(data) { }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleNodes());
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mData << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
    }

    const std::string& GetData(void) const
    {
        return mData;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleNodes";
    }
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleNodeMetaChanged
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleNodeMetaChanged : public EventData
{
    std::string mData;

public:
    static const BaseEventType skEventType;

    EventDataHandleNodeMetaChanged(void) { }
    EventDataHandleNodeMetaChanged(const std::string& data) : mData(data)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleNodeMetaChanged(mData));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mData << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
    }

    const std::string& GetData(void) const
    {
        return mData;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleNodeMetaChanged";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleMedia
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleMedia : public EventData
{
    std::unordered_map<std::wstring, std::wstring> mMedia;

public:
    static const BaseEventType skEventType;

    EventDataHandleMedia(void) { }
    EventDataHandleMedia(const std::unordered_map<std::wstring, std::wstring>& media) : mMedia(media)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleMedia(mMedia));
    }


    const std::unordered_map<std::wstring, std::wstring>& GetMedia(void) const
    {
        return mMedia;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleMedia";
    }
};



//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleNodeMetaFields
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleNodeMetaFields : public EventData
{
    StringMap mFields;
    std::string mForm;
    Vector3<short> mPosition;

public:
    static const BaseEventType skEventType;

    EventDataHandleNodeMetaFields(void) { }
    EventDataHandleNodeMetaFields(const Vector3<short>& pos, const std::string& form, const StringMap& fields) : 
        mPosition(pos), mForm(form), mFields(fields)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleNodeMetaFields(mPosition, mForm, mFields));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        for (int i = 0; i < 3; ++i)
            out << mPosition[i] << " ";

        out << mForm << " ";

        for (auto field : mFields)
        {
            out << field.first << " ";
            out << field.second << " ";
        }
    }

    virtual void Deserialize(std::istrstream& in)
    {
        for (int i = 0; i < 3; ++i)
            in >> mPosition[i];

        in >> mForm;
        /*
        for (auto field : mFields)
        {
            in >> F;
            in >> field.second;
        }
        */
    }

    const StringMap& GetFields(void) const
    {
        return mFields;
    }

    const Vector3<short>& GetPosition(void) const
    {
        return mPosition;
    }

    const std::string& GetForm(void) const
    {
        return mForm;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleNodeMetaFields";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleItems
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleItems : public EventData
{
    std::string mData;

public:
    static const BaseEventType skEventType;

    EventDataHandleItems(void) { }
    EventDataHandleItems(const std::string& data) : mData(data) { }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleItems(mData));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mData << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mData;
    }

    const std::string& GetData(void) const
    {
        return mData;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleItems";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleInventory
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleInventory : public EventData
{
    ActorId mId;
    std::string mInventory;

public:
    static const BaseEventType skEventType;

    EventDataHandleInventory(void) { }
    EventDataHandleInventory(ActorId id, const std::string& inventory) : mId(id), mInventory(inventory)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleInventory(mId, mInventory));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mInventory << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mInventory;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const std::string& GetInventory(void) const
    {
        return mInventory;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleInventory";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleInventoryFields
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleInventoryFields : public EventData
{
    StringMap mFields;
    std::string mForm;

public:
    static const BaseEventType skEventType;

    EventDataHandleInventoryFields(void) { }
    EventDataHandleInventoryFields(const std::string& form, const StringMap& fields) :
        mForm(form), mFields(fields)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleInventoryFields(mForm, mFields));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mForm << " ";

        for (auto field : mFields)
        {
            out << field.first << " ";
            out << field.second << " ";
        }
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mForm;
        /*
        for (auto field : mFields)
        {
            in >> field.first;
            in >> field.second;
        }
        */
    }

    const StringMap& GetFields(void) const
    {
        return mFields;
    }

    const std::string& GetForm(void) const
    {
        return mForm;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleInventoryFields";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleInventoryAction
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleInventoryAction : public EventData
{
    ActorId mId;
    std::string mAction;

public:
    static const BaseEventType skEventType;

    EventDataHandleInventoryAction(void) { }
    EventDataHandleInventoryAction(ActorId id, const std::string& action) : mId(id), mAction(action)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleInventoryAction(mId, mAction));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId << " ";
        out << mAction << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
        in >> mAction;
    }

    const ActorId& GetId(void) const
    {
        return mId;
    }

    const std::string& GetAction(void) const
    {
        return mAction;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleInventoryAction";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataHandleDetachedInventory
//---------------------------------------------------------------------------------------------------------------------
class EventDataHandleDetachedInventory : public EventData
{
    bool mKeepInventory;
    std::string mInventory;
    std::string mInventoryName;

public:
    static const BaseEventType skEventType;

    EventDataHandleDetachedInventory(void) { }
    EventDataHandleDetachedInventory(bool keepInventory, const std::string& name, const std::string& inventory) :
        mKeepInventory(keepInventory), mInventoryName(name), mInventory(inventory)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataHandleDetachedInventory(mKeepInventory, mInventoryName, mInventory));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mKeepInventory << " ";
        out << mInventoryName << " ";
        out << mInventory << " ";
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mKeepInventory;
        in >> mInventoryName;
        in >> mInventory;
    }

    const bool KeepInventory(void) const
    {
        return mKeepInventory;
    }

    const std::string& GetInventoryName(void) const
    {
        return mInventoryName;
    }

    const std::string& GetInventory(void) const
    {
        return mInventory;
    }

    virtual const char* GetName(void) const
    {
        return "EventDataHandleDetachedInventory";
    }
};

#endif