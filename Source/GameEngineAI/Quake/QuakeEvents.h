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


#ifndef QUAKEEVENTS_H
#define QUAKEEVENTS_H

#include "Core/Utility/Chat.h"

#include "Core/Utility/EnrichedString.h"
#include "Core/Utility/StringUtil.h"

#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "AI/Pathing.h"


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
	std::string mGamePath;

public:
	static const BaseEventType skEventType;

	EventDataChangeGameSelection(void) { }
	EventDataChangeGameSelection(const std::string& gameId, 
		const std::string& gameName, const std::string& gamePath)
		: mGameId(gameId), mGameName(gameName), mGamePath(gamePath)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataChangeGameSelection(mGameId, mGameName, mGamePath));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mGameId;
		out << mGameName;
		out << mGamePath;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mGameId;
		in >> mGameName;
		in >> mGamePath;
	}

	const std::string& GetGamePath(void) const
	{
		return mGamePath;
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

	virtual void Serialize(std::ostrstream& out) const
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
	EventDataPlaySoundType(
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
// class EventDataSaveAll
//---------------------------------------------------------------------------------------------------------------------
class EventDataSaveAll : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	EventDataSaveAll(void) { }
	EventDataSaveAll(ActorId actorId) : mId(actorId)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSaveAll(mId));
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
		return "EventDataSaveAll";
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
// class EventDataFireWeapon
//---------------------------------------------------------------------------------------------------------------------
class EventDataFireWeapon : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;
    virtual const BaseEventType & GetEventType() const
    {
        return skEventType;
    }

	EventDataFireWeapon(void)
    {
        mId = INVALID_ACTOR_ID;
    }

	EventDataFireWeapon( ActorId id )
	: mId( id )
    {
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataFireWeapon(mId));
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
        return "EventDataFireWeapon";
    }

	ActorId GetId(void) const
    {
        return mId;
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataChangeWeapon
//---------------------------------------------------------------------------------------------------------------------
class EventDataChangeWeapon : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;
	virtual const BaseEventType & GetEventType() const
	{
		return skEventType;
	}

	EventDataChangeWeapon(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataChangeWeapon(ActorId id)
		: mId(id)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataChangeWeapon(mId));
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
		return "EventDataChangeWeapon";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataSplashDamage
//---------------------------------------------------------------------------------------------------------------------
class EventDataSplashDamage : public EventData
{
	ActorId mId;
	Vector3<float> mOrigin;

public:
	static const BaseEventType skEventType;
	virtual const BaseEventType & GetEventType() const
	{
		return skEventType;
	}

	EventDataSplashDamage(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataSplashDamage(ActorId id, const Vector3<float>& origin)
		: mId(id), mOrigin(origin)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSplashDamage(mId, mOrigin));
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
		return "EventDataSplashDamage";
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
// class EventDataDeadActor
//---------------------------------------------------------------------------------------------------------------------
class EventDataDeadActor : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;
	virtual const BaseEventType & GetEventType() const
	{
		return skEventType;
	}

	EventDataDeadActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataDeadActor(ActorId id)
		: mId(id)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataDeadActor(mId));
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
		return "EventDataDeadActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataTeleportActor - sent when actors are teleported
//---------------------------------------------------------------------------------------------------------------------
class EventDataTeleportActor : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataTeleportActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataTeleportActor(ActorId id)
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
		return BaseEventDataPtr(new EventDataTeleportActor(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataTeleportActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataSpawnActor - sent when actors are spawned
//---------------------------------------------------------------------------------------------------------------------
class EventDataSpawnActor : public EventData
{
	ActorId mId;
	Transform mTransform;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataSpawnActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataSpawnActor(ActorId id, const Transform& trans)
		: mId(id), mTransform(trans)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				out << mTransform.GetHMatrix()(i, j) << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;

		Matrix4x4<float> transform = mTransform.GetMatrix();
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				in >> transform(i, j);
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSpawnActor(mId, mTransform));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataSpawnActor";
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
// EventDataPushActor - sent when actor is pushed
//---------------------------------------------------------------------------------------------------------------------
class EventDataPushActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataPushActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataPushActor(ActorId id, const Vector3<float>& dir)
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
		return BaseEventDataPtr(new EventDataPushActor(mId, mDirection));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataPushActor";
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
// EventDataJumpActor - sent when actor jumps
//---------------------------------------------------------------------------------------------------------------------
class EventDataJumpActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;
	Vector3<float> mFallDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataJumpActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataJumpActor(ActorId id, const Vector3<float>& dir, const Vector3<float>& fallDir)
		: mId(id), mDirection(dir), mFallDirection(fallDir)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		for (int i = 0; i<3; ++i)
			out << mDirection[i] << " ";
		for (int i = 0; i < 3; ++i)
			out << mFallDirection[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i<3; ++i)
			in >> mDirection[i];
		for (int i = 0; i < 3; ++i)
			in >> mFallDirection[i];
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataJumpActor(mId, mDirection, mFallDirection));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataJumpActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetDirection(void) const
	{
		return mDirection;
	}

	const Vector3<float>& GetFallDirection(void) const
	{
		return mFallDirection;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataMoveActor - sent when actor moves
//---------------------------------------------------------------------------------------------------------------------
class EventDataMoveActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;
	Vector3<float> mFallDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataMoveActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataMoveActor(ActorId id, const Vector3<float>& dir, const Vector3<float>& fallDir)
		: mId(id), mDirection(dir), mFallDirection(fallDir)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
		for (int i = 0; i < 3; ++i)
			out << mDirection[i] << " ";
		for (int i = 0; i < 3; ++i)
			out << mFallDirection[i] << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		for (int i = 0; i < 3; ++i)
			in >> mDirection[i];
		for (int i = 0; i < 3; ++i)
			in >> mFallDirection[i];
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataMoveActor(mId, mDirection, mFallDirection));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataMoveActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	const Vector3<float>& GetDirection(void) const
	{
		return mDirection;
	}

	const Vector3<float>& GetFallDirection(void) const
	{
		return mFallDirection;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataFallActor - sent when actors are falling
//---------------------------------------------------------------------------------------------------------------------
class EventDataFallActor : public EventData
{
	ActorId mId;
	Vector3<float> mDirection;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataFallActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataFallActor(ActorId id, const Vector3<float>& dir)
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
		return BaseEventDataPtr(new EventDataFallActor(mId, mDirection));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataFallActor";
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
// EventDataRotateActor - sent when actors are moved
//---------------------------------------------------------------------------------------------------------------------
class EventDataRotateActor : public EventData
{
	ActorId mId;
	float mYaw, mPitch;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataRotateActor(void)
	{
		mId = INVALID_ACTOR_ID;
	}

	EventDataRotateActor(ActorId id, float yaw, float pitch)
		: mId(id), mYaw(yaw), mPitch(pitch)
	{
		//
	}

	virtual void Serialize(std::ostrstream &out) const
	{
		out << mId << " ";
		out << mYaw << " ";
		out << mPitch << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
		in >> mYaw;
		in >> mPitch;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataRotateActor(mId, mYaw, mPitch));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataRotateActor";
	}

	ActorId GetId(void) const
	{
		return mId;
	}

	float GetYaw(void) const
	{
		return mYaw;
	}

	float GetPitch(void) const
	{
		return mPitch;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataClear - sent for clearing displayed map info
//---------------------------------------------------------------------------------------------------------------------
class EventDataClear : public EventData
{
public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataClear(void)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{

	}

	virtual void Deserialize(std::istrstream& in)
	{

	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataClear());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataClear";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataRemoveNode - sent for removing pathing node
//---------------------------------------------------------------------------------------------------------------------
class EventDataRemoveNode : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataRemoveNode(void)
	{
		mId = -1;
	}

	EventDataRemoveNode(int id) : mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataRemoveNode(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataRemoveNode";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataRemoveArcType - sent for removing pathing arcs type
//---------------------------------------------------------------------------------------------------------------------
class EventDataRemoveArcType : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataRemoveArcType(void)
	{
		mId = -1;
	}

	EventDataRemoveArcType(int id) : mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataRemoveArcType(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataRemoveArcType";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataRemoveArc - sent for removing pathing arc
//---------------------------------------------------------------------------------------------------------------------
class EventDataRemoveArc : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataRemoveArc(void)
	{
		mId = -1;
	}

	EventDataRemoveArc(int id) : mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataRemoveArc(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataRemoveArc";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataHighlightNode - sent for highlighting pathing node
//---------------------------------------------------------------------------------------------------------------------
class EventDataHighlightNode : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataHighlightNode(void)
	{
		mId = -1;
	}

	EventDataHighlightNode(int id) : mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataHighlightNode(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataHighlightNode";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataEditMapNode - sent for editing pathing node map
//---------------------------------------------------------------------------------------------------------------------
class EventDataEditMapNode : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataEditMapNode(void)
	{
		mId = -1;
	}

	EventDataEditMapNode(int id) : mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataEditMapNode(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataEditMapNode";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowMapNode - sent for showing path nodes
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowMapNode : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowMapNode(void)
	{
		mId = -1;
	}

	EventDataShowMapNode(int id) : mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowMapNode(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowMapNode";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataChangeAnalysisFrame - change analysis frame
//---------------------------------------------------------------------------------------------------------------------
class EventDataChangeAnalysisFrame : public EventData
{
	unsigned short mFrame;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataChangeAnalysisFrame(void)
	{
		mFrame = 0;
	}

	EventDataChangeAnalysisFrame(int frame) : mFrame(frame)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFrame << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFrame;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataChangeAnalysisFrame(mFrame));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataChangeAnalysisFrame";
	}

	unsigned short GetFrame(void) const
	{
		return mFrame;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowGameSimulation - sent for showing ai game simulation
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowGameSimulation : public EventData
{
	unsigned short mFrame;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowGameSimulation(void)
	{
		mFrame = 0;
	}

	EventDataShowGameSimulation(int frame) : mFrame(frame)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFrame << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFrame;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowGameSimulation(mFrame));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowGameSimulation";
	}

	unsigned short GetFrame(void) const
	{
		return mFrame;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowGameState - sent for showing ai game state
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowGameState : public EventData
{
	unsigned short mFrame;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowGameState(void)
	{
		mFrame = 0;
	}

	EventDataShowGameState(int frame) : mFrame(frame)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFrame << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFrame;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowGameState(mFrame));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowGameState";
	}

	unsigned short GetFrame(void) const
	{
		return mFrame;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataSimulateExploring - ai pathing exploration
//---------------------------------------------------------------------------------------------------------------------
class EventDataSimulateExploring : public EventData
{
	ActorId mActorId;
	int mNodeId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataSimulateExploring(void)
	{
		mActorId = INVALID_ACTOR_ID;
		mNodeId = -1;
	}

	EventDataSimulateExploring(ActorId actorId, int nodeId)
		: mActorId(actorId), mNodeId(nodeId)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mActorId << " ";
		out << mNodeId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mActorId;
		in >> mNodeId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSimulateExploring(mActorId, mNodeId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataSimulateExploring";
	}

	ActorId GetActorId(void) const
	{
		return mActorId;
	}

	int GetNodeId(void) const
	{
		return mNodeId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataSimulatePathing - ai pathing simulation
//---------------------------------------------------------------------------------------------------------------------
class EventDataSimulatePathing : public EventData
{
	ActorId mActorId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataSimulatePathing(void)
	{
		mActorId = INVALID_ACTOR_ID;
	}

	EventDataSimulatePathing(ActorId actorId)
		: mActorId(actorId)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mActorId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mActorId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSimulatePathing(mActorId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataSimulatePathing";
	}

	ActorId GetActorId(void) const
	{
		return mActorId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataSimulateAIGame - ai simulation
//---------------------------------------------------------------------------------------------------------------------
class EventDataSimulateAIGame : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataSimulateAIGame(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSimulateAIGame());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataSimulateAIGame";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataAnalyzeAIGame - ai analysis
//---------------------------------------------------------------------------------------------------------------------
class EventDataAnalyzeAIGame : public EventData
{
	unsigned short mGameFrame;
	unsigned short mAnalysisFrame;
	unsigned short mPlayerIndex;
	unsigned short mTabIndex;
	std::string mEvaluationCluster;
	std::string mDecisionCluster;
	std::string mEvaluationFilter;
	std::string mDecisionFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataAnalyzeAIGame(void)
	{
		mGameFrame = 0;
		mAnalysisFrame = 0;
		mPlayerIndex = 1;
		mTabIndex = 1;
		mEvaluationCluster = std::string();
		mDecisionCluster = std::string();
		mEvaluationFilter = std::string();
		mDecisionFilter = std::string();
	}

	EventDataAnalyzeAIGame(unsigned short gameFrame, unsigned short analysisFrame, 
		unsigned short playerIndex, const std::string& evaluationCluster, const std::string& decisionCluster,
		const std::string& evaluationFilter, const std::string& decisionFilter, unsigned short tabIndex) :
		mGameFrame(gameFrame), mAnalysisFrame(analysisFrame), mPlayerIndex(playerIndex),
		mEvaluationCluster(evaluationCluster), mDecisionCluster(decisionCluster), 
		mEvaluationFilter(evaluationFilter), mDecisionFilter(decisionFilter), mTabIndex(tabIndex)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mGameFrame << " ";
		out << mAnalysisFrame << " ";
		out << mPlayerIndex << " ";
		out << mTabIndex << " ";
		out << mEvaluationCluster << " ";
		out << mDecisionCluster << " ";
		out << mEvaluationFilter << " ";
		out << mDecisionFilter << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mGameFrame;
		in >> mAnalysisFrame;
		in >> mPlayerIndex;
		in >> mTabIndex;
		in >> mEvaluationCluster;
		in >> mDecisionCluster;
		in >> mEvaluationFilter;
		in >> mDecisionFilter;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataAnalyzeAIGame(mGameFrame, mAnalysisFrame, mPlayerIndex, 
			mEvaluationCluster, mDecisionCluster, mEvaluationFilter, mDecisionFilter, mTabIndex));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataAnalyzeAIGame";
	}

	unsigned short GetGameFrame(void) const
	{
		return mGameFrame;
	}

	unsigned short GetAnalysisFrame(void) const
	{
		return mAnalysisFrame;
	}

	unsigned short GetPlayer(void) const
	{
		return mPlayerIndex;
	}

	const std::string& GetEvaluationCluster(void) const
	{
		return mEvaluationCluster;
	}

	const std::string& GetDecisionCluster(void) const
	{
		return mDecisionCluster;
	}

	const std::string& GetEvaluationFilter(void) const
	{
		return mEvaluationFilter;
	}

	const std::string& GetDecisionFilter(void) const
	{
		return mDecisionFilter;
	}

	unsigned short GetTab(void) const
	{
		return mTabIndex;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowAIGame - show ai game
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowAIGame : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowAIGame(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowAIGame());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowAIGame";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowAIGameAnalysis - show ai game analysis
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowAIGameAnalysis : public EventData
{
	unsigned short mGameFrame;
	unsigned short mAnalysisFrame;
	unsigned short mPlayerIndex;
	unsigned short mTabIndex;
	std::string mEvaluationCluster;
	std::string mDecisionCluster;
	std::string mEvaluationFilter;
	std::string mDecisionFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowAIGameAnalysis(void)
	{
		mGameFrame = 0;
		mAnalysisFrame = 0;
		mPlayerIndex = 1;
		mTabIndex = 1;
		mEvaluationCluster = -1;
		mDecisionCluster = -1;
		mEvaluationFilter = std::string();
		mDecisionFilter = std::string();
	}

	EventDataShowAIGameAnalysis(unsigned short gameFrame, unsigned short analysisFrame, 
		unsigned short playerIndex, const std::string& evaluationCluster, const std::string& decisionCluster,
		const std::string& evaluationFilter, const std::string& decisionFilter, unsigned short tabIndex) :
		mGameFrame(gameFrame), mAnalysisFrame(analysisFrame), mPlayerIndex(playerIndex),
		mEvaluationCluster(evaluationCluster), mDecisionCluster(decisionCluster),
		mEvaluationFilter(evaluationFilter), mDecisionFilter(decisionFilter), mTabIndex(tabIndex)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mGameFrame << " ";
		out << mAnalysisFrame << " ";
		out << mPlayerIndex << " ";
		out << mTabIndex << " ";
		out << mEvaluationCluster << " ";
		out << mDecisionCluster << " ";
		out << mEvaluationFilter << " ";
		out << mDecisionFilter << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mGameFrame;
		in >> mAnalysisFrame;
		in >> mPlayerIndex;
		in >> mTabIndex;
		in >> mEvaluationCluster;
		in >> mDecisionCluster;
		in >> mEvaluationFilter;
		in >> mDecisionFilter;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowAIGameAnalysis(mGameFrame, mAnalysisFrame, 
			mPlayerIndex, mEvaluationCluster, mDecisionCluster, mEvaluationFilter, mDecisionFilter, mTabIndex));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowAIGameAnalysis";
	}

	unsigned short GetGameFrame(void) const
	{
		return mGameFrame;
	}

	unsigned short GetAnalysisFrame(void) const
	{
		return mAnalysisFrame;
	}

	unsigned short GetPlayer(void) const
	{
		return mPlayerIndex;
	}

	const std::string& GetEvaluationCluster(void) const
	{
		return mEvaluationCluster;
	}

	const std::string& GetDecisionCluster(void) const
	{
		return mDecisionCluster;
	}

	const std::string& GetEvaluationFilter(void) const
	{
		return mEvaluationFilter;
	}

	const std::string& GetDecisionFilter(void) const
	{
		return mDecisionFilter;
	}

	unsigned short GetTab(void) const
	{
		return mTabIndex;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataSaveAIGame - save ai game
//---------------------------------------------------------------------------------------------------------------------
class EventDataSaveAIGame : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataSaveAIGame(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSaveAIGame());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataSaveAIGame";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataPlayDuelCombat - duel combat
//---------------------------------------------------------------------------------------------------------------------
class EventDataPlayDuelCombat : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataPlayDuelCombat(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataPlayDuelCombat());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataPlayDuelCombat";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataSaveMap - save map
//---------------------------------------------------------------------------------------------------------------------
class EventDataSaveMap : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataSaveMap(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataSaveMap());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataSaveMap";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataEditMap - edit map
//---------------------------------------------------------------------------------------------------------------------
class EventDataEditMap : public EventData
{
	std::string mFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataEditMap(void) { }
	EventDataEditMap(const std::string& filter) : mFilter(filter)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataEditMap(mFilter));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFilter;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFilter;
	}

	const std::string& GetFilter(void) const
	{
		return mFilter;
	}

	virtual const char* GetName(void) const
	{
		return "EventDataEditMap";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowMap - show map
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowMap : public EventData
{
	std::string mFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowMap(void) { }
	EventDataShowMap(const std::string& filter) : mFilter(filter)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowMap(mFilter));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFilter;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFilter;
	}

	const std::string& GetFilter(void) const
	{
		return mFilter;
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowMap";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataCreateMap - create map
//---------------------------------------------------------------------------------------------------------------------
class EventDataCreateMap : public EventData
{
	std::string mFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataCreateMap(void) { }
	EventDataCreateMap(const std::string& filter) : mFilter(filter)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataCreateMap(mFilter));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFilter;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFilter;
	}

	const std::string& GetFilter(void) const
	{
		return mFilter;
	}

	virtual const char* GetName(void) const
	{
		return "EventDataCreateMap";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataCreatePath - create path
//---------------------------------------------------------------------------------------------------------------------
class EventDataCreatePath : public EventData
{
	std::string mFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataCreatePath(void) { }
	EventDataCreatePath(const std::string& filter) : mFilter(filter)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataCreatePath(mFilter));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFilter;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFilter;
	}

	const std::string& GetFilter(void) const
	{
		return mFilter;
	}

	virtual const char* GetName(void) const
	{
		return "EventDataCreatePath";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataEditPathing - edit pathing
//---------------------------------------------------------------------------------------------------------------------
class EventDataEditPathing : public EventData
{
	std::string mFilter;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataEditPathing(void) { }
	EventDataEditPathing(const std::string& filter) : mFilter(filter)
	{
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataEditPathing(mFilter));
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mFilter;
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mFilter;
	}

	const std::string& GetFilter(void) const
	{
		return mFilter;
	}

	virtual const char* GetName(void) const
	{
		return "EventDataEditPathing";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataShowPathing - ai pathing graph
//---------------------------------------------------------------------------------------------------------------------
class EventDataShowPathing : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataShowPathing(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataShowPathing());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataShowPathing";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataCreatePathing - ai pathing creation through simulation
//---------------------------------------------------------------------------------------------------------------------
class EventDataCreatePathing : public EventData
{
	ActorId mActorId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataCreatePathing(void)
	{
		mActorId = INVALID_ACTOR_ID;
	}

	EventDataCreatePathing(ActorId actorId)
		: mActorId(actorId)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mActorId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mActorId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataCreatePathing(mActorId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataCreatePathing";
	}

	ActorId GetActorId(void) const
	{
		return mActorId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataCreatePathingMap - create pathing map
//---------------------------------------------------------------------------------------------------------------------
class EventDataCreatePathingMap : public EventData
{
	ActorId mActorId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataCreatePathingMap(void)
	{
		mActorId = INVALID_ACTOR_ID;
	}

	EventDataCreatePathingMap(ActorId actorId)
		: mActorId(actorId)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mActorId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mActorId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataCreatePathingMap(mActorId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataCreatePathingMap";
	}

	ActorId GetActorId(void) const
	{
		return mActorId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataCreatePathingNode - create pathingnode from player position
//---------------------------------------------------------------------------------------------------------------------
class EventDataCreatePathingNode : public EventData
{
	ActorId mActorId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataCreatePathingNode(void)
	{
		mActorId = INVALID_ACTOR_ID;
	}

	EventDataCreatePathingNode(ActorId actorId)
		: mActorId(actorId)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mActorId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mActorId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataCreatePathingNode(mActorId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataCreatePathingNode";
	}

	ActorId GetActorId(void) const
	{
		return mActorId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataValidateMap - validate map
//---------------------------------------------------------------------------------------------------------------------
class EventDataValidateMap : public EventData
{

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataValidateMap(void)
	{
		//
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataValidateMap());
	}

	virtual const char* GetName(void) const
	{
		return "EventDataValidateMap";
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataNodeVisibility - node visibility
//---------------------------------------------------------------------------------------------------------------------
class EventDataNodeVisibility : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataNodeVisibility(void)
	{
		mId = -1;
	}

	EventDataNodeVisibility(int id)
		: mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataNodeVisibility(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataNodeVisibility";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataNodeConnection - node visibility
//---------------------------------------------------------------------------------------------------------------------
class EventDataNodeConnection : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataNodeConnection(void)
	{
		mId = -1;
	}

	EventDataNodeConnection(int id)
		: mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataNodeConnection(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataNodeConnection";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// EventDataArcConnection - arc visibility
//---------------------------------------------------------------------------------------------------------------------
class EventDataArcConnection : public EventData
{
	int mId;

public:
	static const BaseEventType skEventType;

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	EventDataArcConnection(void)
	{
		mId = -1;
	}

	EventDataArcConnection(int id)
		: mId(id)
	{
		//
	}

	virtual void Serialize(std::ostrstream& out) const
	{
		out << mId << " ";
	}

	virtual void Deserialize(std::istrstream& in)
	{
		in >> mId;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataArcConnection(mId));
	}

	virtual const char* GetName(void) const
	{
		return "EventDataArcConnection";
	}

	int GetId(void) const
	{
		return mId;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// class EventDataGameplayUIUpdate				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class EventDataGameplayUIUpdate : public EventData
{
    std::string mGameplayUiString;

public:
    static const BaseEventType skEventType;

	EventDataGameplayUIUpdate(void) { }
	EventDataGameplayUIUpdate(const std::string& gameplayUiString)
	: mGameplayUiString(gameplayUiString)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataGameplayUIUpdate(mGameplayUiString));
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
        return "EventDataGameplayUIUpdate";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataSetControlledActor				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class EventDataSetControlledActor : public EventData
{
    ActorId mId;

public:
    static const BaseEventType skEventType;

	EventDataSetControlledActor(void) { }
	EventDataSetControlledActor(ActorId actorId)
        : mId(actorId)
    {
    }

    virtual const BaseEventType& GetEventType(void) const
    {
        return skEventType;
    }

    virtual BaseEventDataPtr Copy() const
    {
        return BaseEventDataPtr(new EventDataSetControlledActor(mId));
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
        return "EventDataSetControlledActor";
    }
};


//---------------------------------------------------------------------------------------------------------------------
// class EventDataRemoveControlledActor				- Chapter 10, 279
//---------------------------------------------------------------------------------------------------------------------
class EventDataRemoveControlledActor : public EventData
{
	ActorId mId;

public:
	static const BaseEventType skEventType;

	EventDataRemoveControlledActor(void) { }
	EventDataRemoveControlledActor(ActorId actorId)
		: mId(actorId)
	{
	}

	virtual const BaseEventType& GetEventType(void) const
	{
		return skEventType;
	}

	virtual BaseEventDataPtr Copy() const
	{
		return BaseEventDataPtr(new EventDataRemoveControlledActor(mId));
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
		return "EventDataRemoveControlledActor";
	}
};

#endif