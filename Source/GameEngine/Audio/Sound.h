//========================================================================
// Sound.h : Defines a simple sound system.
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

#ifndef SOUND_H
#define SOUND_H

#include "GameEngineStd.h"

#include "Core/Utility/Serialize.h"


/*
	Game Sound System Architecture. The audio system presented is a class hierarchy implementation
	which allows to integrate any audio subsystem right for the game.

	The sound system inherits from BaseAudio. This object is responsible for the list of sounds
	currently active. The Audio class implements some implementation-generic routines, and the
	SoundOpenal class completes the implementation with APISound-specific calls.
*/


//////////////////////////////////////////////////////////////////////
// SoundType Description
//
// This is an enum that represents the different kinds of sound data
// streams the sound system can handle.
//
//////////////////////////////////////////////////////////////////////
enum SoundType
{
	SOUND_TYPE_FIRST,
	SOUND_TYPE_MP3 = SOUND_TYPE_FIRST,
	SOUND_TYPE_WAVE,
	SOUND_TYPE_MIDI,
	SOUND_TYPE_OGG,

	// This needs to be the last sound type
	SOUND_TYPE_COUNT,
	SOUND_TYPE_UNKNOWN,
};


struct SimpleSound
{
    SimpleSound(const std::string& name = "", 
        float gain = 1.0f, float fade = 0.0f, float pitch = 1.0f) :
        name(name), gain(gain), fade(fade), pitch(pitch)
    {
    }

    bool Exists() const { return !name.empty(); }

    // Take cfVersion from ContentFeatures::serialize to
    // keep in sync with item definitions
    void Serialize(std::ostream& os) const
    {
        os << SerializeString16(name);
        WriteFloat(os, gain);
        WriteFloat(os, pitch);
        WriteFloat(os, fade);
        // if (cfVersion < ?) return;
    }

    void Deserialize(std::istream& is)
    {
        name = DeserializeString16(is);
        gain = ReadFloat(is);
        pitch = ReadFloat(is);
        fade = ReadFloat(is);
    }

    std::string name;
    float gain = 1.0f;
    float fade = 0.0f;
    float pitch = 1.0f;
};

class OnDemandSoundFetcher
{
public:
	virtual void FetchSounds(const std::string& name, std::set<std::string>& dstPaths) = 0;
};


class SoundSystem
{

public:
	SoundSystem();

	~SoundSystem();

	virtual bool Init() = 0;

	// Getter for the main global sound system. This is the audio that is used by the majority of the 
	// engine, though you are free to define your own as long as you instantiate it.
	// It is not valid to have more than one global sound system.
	static SoundSystem* Get(void);

protected:

	// The unique audio object.
	static SoundSystem* mAudioSystem;
};


/*
	BaseSound interface class describes the public interface for a game's audio system. It
	encapsulate the system that manages the list of actie sounds and has three main purposes:
	create, manage and release audio buffers.
*/
class BaseSoundManager
{
public:
	BaseSoundManager();

	~BaseSoundManager();

	// Multiple sounds can be loaded per name; when played, the sound
	// should be chosen randomly from alternatives
	// Return value determines success/failure
	virtual bool LoadSound(const std::string& name, const std::string& filepath) = 0;

	virtual void UpdateListener(
        const Vector3<float>& pos, const Vector3<float>& vel, 
        const Vector3<float>& at, const Vector3<float>& up) = 0;
	virtual void SetListenerGain(float gain) = 0;

	// PlaySound functions return -1 on failure, otherwise a handle to the
	// sound. If name=="", call should be ignored without error.
	virtual int PlaySoundGlobal(const std::string& name, bool loop, float volume,
        float fade = 0.0f, float pitch = 1.0f) = 0;
	virtual int PlaySoundAt(const std::string& name, bool loop, float volume, 
        Vector3<float> pos, float pitch = 1.0f) = 0;
	virtual void StopSound(int id) = 0;
	virtual bool SoundExists(int id) = 0;
	virtual void UpdateSoundPosition(int id, Vector3<float> pos) = 0;
	virtual bool UpdateSoundGain(int id, float gain) = 0;
	virtual float GetSoundProgress(int id) = 0;
	virtual float GetSoundGain(int id) = 0;
	virtual void Step(float deltaMs) = 0;
	virtual void FadeSound(int id, float step, float gain) = 0;

	int PlaySoundGlobal(const SimpleSound& spec, bool loop)
	{
		return PlaySoundGlobal(spec.name, loop, spec.gain, spec.fade, spec.pitch);
	}
	int PlaySoundAt(const SimpleSound& spec, bool loop, const Vector3<float>& pos)
	{
		return PlaySoundAt(spec.name, loop, spec.gain, pos, spec.pitch);
	}

	static BaseSoundManager* Get(void);

protected:

	// The unique sound manager object.
	static BaseSoundManager* mSoundManager;
};

class SimpleSoundManager : public BaseSoundManager
{
public:
	virtual bool LoadSound(const std::string& name, const std::string& filepath)
	{
		return true;
	}
	void UpdateListener(
        const Vector3<float>& pos, const Vector3<float>& vel, 
        const Vector3<float>& at, const Vector3<float>& up)
	{
	}
	void SetListenerGain(float gain) {}
	int PlaySound(const std::string& name, bool loop, float volume, float fade, float pitch)
	{
		return 0;
	}
	int PlaySoundAt(const std::string& name, bool loop, float volume, Vector3<float> pos, float pitch)
	{
		return 0;
	}
	void StopSound(int id) {}
	bool SoundExists(int id) { return false; }
	void UpdateSoundPosition(int id, Vector3<float> pos) {}
	bool UpdateSoundGain(int id, float gain) { return false; }
	float GetSoundProgress(int id) { return 0; }
	float GetSoundGain(int id) { return 0; }
	void Step(float deltaMs) {}
	void FadeSound(int id, float step, float gain) {}
};

#endif