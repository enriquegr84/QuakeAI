//========================================================================
// QuakeEvents.h : defines game-specific events for Quake
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


#ifndef QUAKEEVENTS_H
#define QUAKEEVENTS_H

#include "Core/Utility/Chat.h"

#include "Core/Utility/EnrichedString.h"
#include "Core/Utility/StringUtil.h"

#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "AI/Pathing.h"


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataOpenContentStore
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataOpenContentStore : public EventData
{

public:
	static const BaseEventType skEventType;

	QuakeEventDataOpenContentStore(void) { }

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataOpenContentStore());
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataOpenContentStore";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataDeleteContentStore
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataDeleteContentStore : public EventData
{
	std::string mGameLevel;

public:
	static const BaseEventType skEventType;

	QuakeEventDataDeleteContentStore(void) { }
	QuakeEventDataDeleteContentStore(const std::string& level) : mGameLevel(level)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataDeleteContentStore(mGameLevel));
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
		return "QuakeEventDataDeleteContentStore";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataOpenGameSelection
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataOpenGameSelection : public EventData
{

public:
	static const BaseEventType skEventType;

	QuakeEventDataOpenGameSelection(void) { }

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataOpenGameSelection());
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataOpenGameSelection";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataChangeGameSelection
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataChangeGameSelection : public EventData
{
	std::string mGameId;
	std::string mGameName;

public:
	static const BaseEventType skEventType;

	QuakeEventDataChangeGameSelection(void) { }
	QuakeEventDataChangeGameSelection(const std::string& game, const std::string& gameId)
		: mGameName(game), mGameId(gameId)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataChangeGameSelection(mGameName, mGameId));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mGameName;
		out << mGameId;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mGameName;
		in >> mGameId;
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
		return "QuakeEventDataChangeGameSelection";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataShowForm
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataShowForm : public EventData
{
	std::string mForm;
	std::string mFormName;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataShowForm(void) { }

	QuakeEventDataShowForm(const std::string& form, const std::string& formName) :
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
		return BaseEventDataPtr(new QuakeEventDataShowForm(mForm, mFormName));
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
		return "QuakeEventDataShowForm";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataInitChat
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataInitChat : public EventData
{
	ChatBackend* mChat;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataInitChat(void) { }

	QuakeEventDataInitChat(ChatBackend* chat) : mChat(chat)
	{

	}

	virtual void Serialize(std::ostrstream& out) const
	{
	}

	virtual void Deserialize(std::istrstream& in)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataInitChat(mChat));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataInitChat";
	}

	ChatBackend* GetChat(void) const
	{
		return mChat;
	}
};


//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataUpdateChat
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataUpdateChat : public EventData
{
	unsigned int mLineCount;
	EnrichedString mChat;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataUpdateChat(void) { }

	QuakeEventDataUpdateChat(unsigned int lineCount, const EnrichedString& chat) : mLineCount(lineCount), mChat(chat)
	{

	}

	virtual void Serialize(std::ostrstream& out) const
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
		return BaseEventDataPtr(new QuakeEventDataUpdateChat(mLineCount, mChat));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataUpdateChat";
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
// class QuakeEventDataGameInit
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataGameInit : public EventData
{

public:
	static const BaseEventType skEventType;

	QuakeEventDataGameInit(void) { }

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataGameInit());
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataGameInit";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataGameReady
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataGameReady : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	QuakeEventDataGameReady(void) { }
	QuakeEventDataGameReady(ActorId actorId) { mId = actorId; }

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataGameReady(mId));
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
		return "QuakeEventDataGameReady";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataPlaySoundAt
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataPlaySoundAt : public EventData
{
	std::string mSoundName;
	Vector3<float> mPosition;

	float mGain;
	float mPitch;

	bool mLoop;

public:
	static const BaseEventType skEventType;

	QuakeEventDataPlaySoundAt(void) { }
	QuakeEventDataPlaySoundAt(std::string name, Vector3<float> pos,
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
		return BaseEventDataPtr(new QuakeEventDataPlaySoundAt(mSoundName, mPosition, mGain, mPitch, mLoop));
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
		return "QuakeEventDataPlaySoundAt";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataPlaySoundType
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataPlaySoundType : public EventData
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

	QuakeEventDataPlaySoundType(void) { }
	QuakeEventDataPlaySoundType(
		int id, std::string name, unsigned char type, Vector3<float> pos, unsigned short objectId, 
		float gain, float fade = 0.0f, float pitch = 1.0f, bool ephemeral = false, bool loop = false) : 
		mId(id), mSoundName(name), mType(type), mPosition(pos), mObjectId(objectId), mLoop(loop), 
		mEphemeral(ephemeral), mGain(gain), mFade(fade), mPitch(pitch)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataPlaySoundType(
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
		return "QuakeEventDataPlaySoundType";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataStopSound
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataStopSound : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	QuakeEventDataStopSound(void) { }
	QuakeEventDataStopSound(int id) : mId(id)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataStopSound(mId));
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
		return "QuakeEventDataStopSound";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataFadeSound
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataFadeSound : public EventData
{
	int mId;

	float mStep;
	float mGain;

public:
	static const BaseEventType skEventType;

	QuakeEventDataFadeSound(void) { }
	QuakeEventDataFadeSound(int id, float step, float gain) : mId(id), mStep(step), mGain(gain)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataFadeSound(mId, mStep, mGain));
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
		return "QuakeEventDataFadeSound";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataRemoveSounds
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataRemoveSounds : public EventData
{
	std::vector<int> mSoundIds;

public:
	static const BaseEventType skEventType;

	QuakeEventDataRemoveSounds(void) { }
	QuakeEventDataRemoveSounds(const std::vector<int>& ids) : mSoundIds(ids)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataRemoveSounds(mSoundIds));
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
		return "QuakeEventDataRemoveSounds";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataChangeVolume
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataChangeVolume : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	QuakeEventDataChangeVolume(void) { }
	QuakeEventDataChangeVolume(ActorId actorId) : mId(actorId)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataChangeVolume(mId));
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
		return "QuakeEventDataChangeVolume";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataChangeMenu
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataChangeMenu : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	QuakeEventDataChangeMenu(void) { }
	QuakeEventDataChangeMenu(ActorId actorId) : mId(actorId)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataChangeMenu(mId));
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
		return "QuakeEventDataChangeMenu";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataHandleMedia
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataHandleMedia : public EventData
{
	std::unordered_map<std::wstring, std::wstring> mMedia;

public:
	static const BaseEventType skEventType;

	QuakeEventDataHandleMedia(void) { }
	QuakeEventDataHandleMedia(const std::unordered_map<std::wstring, std::wstring>& media) : mMedia(media)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataHandleMedia(mMedia));
	}


	const std::unordered_map<std::wstring, std::wstring>& GetMedia(void) const
	{
		return mMedia;
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataHandleMedia";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataFireWeapon
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataFireWeapon : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;
    virtual const BaseEventType & GetEventType() const
    {
        return skEventType;
    }

	QuakeEventDataFireWeapon(void)
    {
        mId = INVALID_ACTOR_ID;
    }

	QuakeEventDataFireWeapon( ActorId id )
	: mId( id )
    {
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new QuakeEventDataFireWeapon(mId));
    }

    virtual void Serialize( std::ostrstream & out ) const
    {
        out << mId << " ";
    }

    virtual void Deserialize( std::istrstream & in )
    {
        in >> mId;
    }

    virtual const char* GetName(void) const
    {
        return "QuakeEventDataFireWeapon";
    }

	ActorId GetId(void) const
    {
        return mId;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataChangeWeapon
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataChangeWeapon : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;
	virtual const BaseEventType & GetEventType() const
	{
		return skEventType;
	}

	QuakeEventDataChangeWeapon(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataChangeWeapon(ActorId id)
		: mId(id)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataChangeWeapon(mId));
	}

	virtual void Serialize(std::ostrstream & out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream & in)
	{
		in >> mId;
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataChangeWeapon";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataSplashDamage
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataSplashDamage : public EventData
{
	ActorId mId;
	Vector3<float> mOrigin;

public:
	static const BaseEventType skEventType;
	virtual const BaseEventType & GetEventType() const
	{
		return skEventType;
	}

	QuakeEventDataSplashDamage(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataSplashDamage(ActorId id, const Vector3<float>& origin)
		: mId(id), mOrigin(origin)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataSplashDamage(mId, mOrigin));
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<3; ++i)
			out << mOrigin[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i<3; ++i)
			in >> mOrigin[i];
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataSplashDamage";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetOrigin(void) const
	{
		return mOrigin;
	}
};



//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataDeadActor
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataDeadActor : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;
	virtual const BaseEventType & GetEventType() const
	{
		return skEventType;
	}

	QuakeEventDataDeadActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataDeadActor(ActorId id)
		: mId(id)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataDeadActor(mId));
	}

	virtual void Serialize(std::ostrstream & out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream & in)
	{
		in >> mId;
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataDeadActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataTeleportActor - sent when actors are teleported
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataTeleportActor : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataTeleportActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataTeleportActor(ActorId id)
		: mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataTeleportActor(mId));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataTeleportActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataSpawnActor - sent when actors are spawned
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataSpawnActor : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataSpawnActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataSpawnActor(ActorId id)
		: mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataSpawnActor(mId));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataSpawnActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataPushActor - sent when actor is pushed
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataPushActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataPushActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataPushActor(ActorId id, const Vector3<float>& dir)
		: mId(id), mDirection(dir)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<3; ++i)
			out << mDirection[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i<3; ++i)
			in >> mDirection[i];
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataPushActor(mId, mDirection));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataPushActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetDirection(void) const
	{
		return mDirection;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataJumpActor - sent when actor jumps
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataJumpActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataJumpActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataJumpActor(ActorId id, const Vector3<float>& dir)
		: mId(id), mDirection(dir)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<3; ++i)
			out << mDirection[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i<3; ++i)
			in >> mDirection[i];
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataJumpActor(mId, mDirection));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataJumpActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetDirection(void) const
	{
		return mDirection;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataMoveActor - sent when actors are moved
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataMoveActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataMoveActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataMoveActor(ActorId id, const Vector3<float>& dir)
		: mId(id), mDirection(dir)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<3; ++i)
			out << mDirection[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i<3; ++i)
			in >> mDirection[i];
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataMoveActor(mId, mDirection));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataMoveActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetDirection(void) const
	{
		return mDirection;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataFallActor - sent when actors are falling
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataFallActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataFallActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataFallActor(ActorId id, const Vector3<float>& dir)
		: mId(id), mDirection(dir)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<3; ++i)
			out << mDirection[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i<3; ++i)
			in >> mDirection[i];
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataFallActor(mId, mDirection));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataFallActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetDirection(void) const
	{
		return mDirection;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeEventDataRotateActor - sent when actors are moved
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataRotateActor : public EventData
{
	ActorId mId;
	Transform mTransform;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	QuakeEventDataRotateActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	QuakeEventDataRotateActor(ActorId id, const Transform& trans)
		: mId(id), mTransform(trans)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<4; ++i)
			for (int j = 0; j<4; ++j)
				out << mTransform.GetMatrix()(i, j) << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;

		Matrix4x4<float> transform = mTransform.GetMatrix();
		for (int i = 0; i<4; ++i)
			for (int j = 0; j<4; ++j)
				in >> transform(i, j);
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataRotateActor(mId, mTransform));
	}

	virtual const char* GetName(void) const
	{
		return "QuakeEventDataRotateActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Transform& GetTransform(void) const
	{
		return mTransform;
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataGameplayUIUpdate				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataGameplayUIUpdate : public EventData
{
    std::string mGameplayUiString;

public:
    static const BaseEventType skEventType;

	QuakeEventDataGameplayUIUpdate(void) { }
	QuakeEventDataGameplayUIUpdate(const std::string& gameplayUiString)
	: mGameplayUiString(gameplayUiString)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new QuakeEventDataGameplayUIUpdate(mGameplayUiString));
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
        return "QuakeEventDataGameplayUIUpdate";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataSetControlledActor				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataSetControlledActor : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

	QuakeEventDataSetControlledActor(void) { }
	QuakeEventDataSetControlledActor(ActorId actorId)
        : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new QuakeEventDataSetControlledActor(mId));
    }

    virtual void Serialize(std::ostrstream& out) const
    {
        out << mId;
    }

    virtual void Deserialize(std::istrstream& in)
    {
        in >> mId;
    }

    const ActorId& GetActorId(void) const
    {
        return mId;
    }

    virtual const char* GetName(void) const
    {
        return "QuakeEventDataSetControlledActor";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataHudAdd
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataHudAdd : public EventData
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

	QuakeEventDataHudAdd(void) { }
	QuakeEventDataHudAdd(unsigned int id, uint8_t type, const Vector2<float>& position,
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
		return BaseEventDataPtr(new QuakeEventDataHudAdd(mId, mType, mPosition, mHudName, mScale, mText,
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
		return "QuakeEventDataHudAdd";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataHudRemove
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataHudRemove : public EventData
{
	unsigned int mId;

public:
	static const BaseEventType skEventType;

	QuakeEventDataHudRemove(void) { }
	QuakeEventDataHudRemove(unsigned int id) : mId(id)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataHudRemove(mId));
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
		return "QuakeEventDataHudRemove";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataHudChange
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataHudChange : public EventData
{
	unsigned int mId;
	unsigned char mStat;
	void* mValue;

public:
	static const BaseEventType skEventType;

	QuakeEventDataHudChange(void) { }
	QuakeEventDataHudChange(unsigned int id, unsigned char stat, void* value) : 
		mId(id), mStat(stat), mValue(value)
	{

	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataHudChange(mId, mStat, mValue));
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
		return "QuakeEventDataHudChange";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataHudSetFlags
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataHudSetFlags : public EventData
{
	unsigned int mMask;
	unsigned int mFlags;

public:
	static const BaseEventType skEventType;

	QuakeEventDataHudSetFlags(void) { }
	QuakeEventDataHudSetFlags(unsigned int mask, unsigned int flags) : mMask(mask), mFlags(flags)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataHudSetFlags(mMask, mFlags));
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
		return "QuakeEventDataHudSetFlags";
	}
};


//---------------------------------------------------------------------------------------------------------------------
// class QuakeEventDataHudSetParam
//---------------------------------------------------------------------------------------------------------------------
class QuakeEventDataHudSetParam : public EventData
{
	uint16_t mParam;
	std::string mValue;

public:
	static const BaseEventType skEventType;

	QuakeEventDataHudSetParam(void) { }
	QuakeEventDataHudSetParam(uint16_t param, std::string value) : mParam(param), mValue(value)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new QuakeEventDataHudSetParam(mParam, mValue));
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
		return "QuakeEventDataHudSetParam";
	}
};


#endif