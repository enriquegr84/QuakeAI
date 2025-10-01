//========================================================================
// SoundOpenal.cpp : Defines the openal sound system.
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

#include "SoundOpenal.h"

#include "SoundResource.h"

#include "Core/OS/OS.h"
#include "Core/Utility/StringUtil.h"
#include "Core/Logger/Logger.h"

static void DeleteAlcDevice(ALCdevice* p)
{
	if (p)
		alcCloseDevice(p);
}

static void DeleteAlcContext(ALCcontext* p)
{
	if (p)
    {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(p);
	}
}

static const char* AlErrorString(ALenum err)
{
	switch (err) 
    {
	    case AL_NO_ERROR:
		    return "no error";
	    case AL_INVALID_NAME:
		    return "invalid name";
	    case AL_INVALID_ENUM:
		    return "invalid enum";
	    case AL_INVALID_VALUE:
		    return "invalid value";
	    case AL_INVALID_OPERATION:
		    return "invalid operation";
	    case AL_OUT_OF_MEMORY:
		    return "out of memory";
	    default:
		    return "<unknown OpenAL error>";
	}
}

static ALenum WarnIfError(ALenum err, const char *desc)
{
	if(err == AL_NO_ERROR)
		return err;
	LogWarning(std::string(desc) + ": " + AlErrorString(err));
	return err;
}

void Float3Set(ALfloat* f3, Vector3<float> v)
{
	f3[0] = v[0];
	f3[1] = v[1];
	f3[2] = v[2];
}

OpenALSoundSystem::OpenALSoundSystem() : SoundSystem(),
    mDevice(nullptr, DeleteAlcDevice), mContext(nullptr, DeleteAlcContext)
{

}

OpenALSoundSystem::~OpenALSoundSystem()
{

}

bool OpenALSoundSystem::Init()
{
	if (!(mDevice = unique_ptr_alcdevice(alcOpenDevice(nullptr), DeleteAlcDevice))) 
    {
		LogError("Audio: Global Initialization: Failed to open device");
		return false;
	}

	if (!(mContext = unique_ptr_alccontext(
        alcCreateContext(mDevice.get(), nullptr), DeleteAlcContext))) 
    {
		LogError("Audio: Global Initialization: Failed to create context");
		return false;
	}

	if (!alcMakeContextCurrent(mContext.get())) 
    {
		LogError("Audio: Global Initialization: Failed to make current context");
		return false;
	}

	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

	if (alGetError() != AL_NO_ERROR) 
    {
		LogError("Audio: Global Initialization: OpenAL Error " + alGetError());
		return false;
	}

	LogInformation("Audio: Global Initialized: OpenAL " + std::string(alGetString(AL_VERSION)) +
        ", using " + std::string(alcGetString(mDevice.get(), ALC_DEVICE_SPECIFIER)));

	return true;
}


OpenALSoundManager::OpenALSoundManager(OpenALSoundSystem* smg, OnDemandSoundFetcher* fetcher) :
	BaseSoundManager(), mFetcher(fetcher), mDevice(smg->mDevice.get()), mContext(smg->mContext.get()), mNextId(1)
{
	LogInformation("Audio: Initialized: OpenAL ");
}

OpenALSoundManager::~OpenALSoundManager()
{
	LogInformation("Audio: Deinitializing...");

	std::unordered_set<int> sourceDelList;
	for (auto const& sp : mSoundsPlaying)
		sourceDelList.insert(sp.first);

	for (auto const& id : sourceDelList)
		DeleteSound(id);

	for (auto& buffer : mBuffers) 
    {
		for (SoundBuffer* sb : buffer.second)
			delete sb;

		buffer.second.clear();
	}
	mBuffers.clear();

	LogInformation("Audio: Deinitialized.");
}

void OpenALSoundManager::Step(float dTime)
{
	DoFades(dTime);
}

void OpenALSoundManager::AddBuffer(const std::string& name, SoundBuffer* buf)
{
	std::unordered_map<std::string, std::vector<SoundBuffer*>>::iterator i = mBuffers.find(name);
	if(i != mBuffers.end())
    {
		i->second.push_back(buf);
		return;
	}
	std::vector<SoundBuffer*> bufs;
	bufs.push_back(buf);
	mBuffers[name] = bufs;
}

SoundBuffer* OpenALSoundManager::GetBuffer(const std::string& name)
{
	std::unordered_map<std::string, std::vector<SoundBuffer*>>::iterator i = mBuffers.find(name);
	if(i == mBuffers.end())
		return nullptr;
	std::vector<SoundBuffer*>& bufs = i->second;
	int j = Randomizer::Rand() % bufs.size();
	return bufs[j];
}

PlayingSound* OpenALSoundManager::CreatePlayingSound(SoundBuffer* buf, bool loop, float volume, float pitch)
{
	LogInformation("OpenALSoundManager: Creating playing sound");
	LogAssert(buf, "invalid sound buffer");
	PlayingSound* sound = new PlayingSound;
    LogAssert(sound, "invalid sound");
	WarnIfError(alGetError(), "before CreatePlayingSound");
	alGenSources(1, &sound->sourceId);
	alSourcei(sound->sourceId, AL_BUFFER, buf->bufferId);
	alSourcei(sound->sourceId, AL_SOURCE_RELATIVE, true);
	alSource3f(sound->sourceId, AL_POSITION, 0, 0, 0);
	alSource3f(sound->sourceId, AL_VELOCITY, 0, 0, 0);
	alSourcei(sound->sourceId, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	volume = std::fmax(0.0f, volume);
	alSourcef(sound->sourceId, AL_GAIN, volume);
	alSourcef(sound->sourceId, AL_PITCH, pitch);
	alSourcePlay(sound->sourceId);
	WarnIfError(alGetError(), "CreatePlayingSound");
	return sound;
}

PlayingSound* OpenALSoundManager::CreatePlayingSoundAt(SoundBuffer* buf, bool loop,
		float volume, Vector3<float> pos, float pitch)
{
	LogInformation("OpenALSoundManager: Creating positional playing sound");
	LogAssert(buf, "invalid sound buffer");
	PlayingSound* sound = new PlayingSound;
    LogAssert(sound, "invalid sound");
	WarnIfError(alGetError(), "before CreatePlayingSoundAt");
	alGenSources(1, &sound->sourceId);
	alSourcei(sound->sourceId, AL_BUFFER, buf->bufferId);
	alSourcei(sound->sourceId, AL_SOURCE_RELATIVE, false);
	alSource3f(sound->sourceId, AL_POSITION, pos[0], pos[1], pos[2]);
	alSource3f(sound->sourceId, AL_VELOCITY, 0, 0, 0);
	// Use alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED) and set reference
	// distance to clamp gain at <1 node distance, to avoid excessive
	// volume when closer
	alSourcef(sound->sourceId, AL_REFERENCE_DISTANCE, 10.0f);
	alSourcei(sound->sourceId, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	// Multiply by 3 to compensate for reducing AL_REFERENCE_DISTANCE from
	// the previous value of 30 to the new value of 10
	volume = std::fmax(0.0f, volume * 3.0f);
	alSourcef(sound->sourceId, AL_GAIN, volume);
	alSourcef(sound->sourceId, AL_PITCH, pitch);
	alSourcePlay(sound->sourceId);
	WarnIfError(alGetError(), "CreatePlayingSoundAt");
	return sound;
}

int OpenALSoundManager::PlaySoundRaw(SoundBuffer* buf, bool loop, float volume, float pitch)
{
	LogAssert(buf, "invalid sound buffer");
	PlayingSound* sound = CreatePlayingSound(buf, loop, volume, pitch);
	if(!sound)
		return -1;
	int id = mNextId++;
	mSoundsPlaying[id] = sound;
	return id;
}

int OpenALSoundManager::PlaySoundRawAt(SoundBuffer* buf, bool loop, float volume, const Vector3<float>& pos, float pitch)
{
	LogAssert(buf, "invalid sound buffer");
	PlayingSound* sound = CreatePlayingSoundAt(buf, loop, volume, pos, pitch);
	if(!sound)
		return -1;
	int id = mNextId++;
	mSoundsPlaying[id] = sound;
	return id;
}

void OpenALSoundManager::DeleteSound(int id)
{
	std::unordered_map<int, PlayingSound*>::iterator i = mSoundsPlaying.find(id);
	if(i == mSoundsPlaying.end())
		return;
	PlayingSound* sound = i->second;

	alDeleteSources(1, &sound->sourceId);

	delete sound;
	mSoundsPlaying.erase(id);
}

/* If buffer does not exist, consult the fetcher */
SoundBuffer* OpenALSoundManager::GetFetchBuffer(const std::string& name)
{
	SoundBuffer* buf = GetBuffer(name);
	if(buf)
		return buf;
	if(!mFetcher)
		return nullptr;

    std::set<std::string> paths;
	mFetcher->FetchSounds(name, paths);
	for (std::string const& path : paths) 
		LoadSound(name, path);

	return GetBuffer(name);
}

// Remove stopped sounds
void OpenALSoundManager::Maintain()
{
	if (!mSoundsPlaying.empty()) 
    {
		LogInformation("OpenALSoundManager::Maintain(): " + 
            std::to_string(mSoundsPlaying.size()) + " playing sounds, " +
            std::to_string(mBuffers.size()) + " sound names loaded");
	}

	std::unordered_set<int> delList;
	for (auto const& sp : mSoundsPlaying) 
    {
		int id = sp.first;
		PlayingSound *sound = sp.second;
		// If not playing, remove it
		{
			ALint state;
			alGetSourcei(sound->sourceId, AL_SOURCE_STATE, &state);
			if(state != AL_PLAYING)
				delList.insert(id);
		}
	}
    if (!delList.empty())
    {
        LogInformation("OpenALSoundManager::Maintain(): deleting " +
            std::to_string(delList.size()) + " playing sounds");
    }
	for (int delSound : delList)
		DeleteSound(delSound);
}

/* Interface */

bool OpenALSoundManager::LoadSound(const std::string& name, const std::string& filepath)
{
	std::shared_ptr<ResHandle> resHandle =
		ResCache::Get()->GetHandle(&BaseResource(ToWideString(filepath)));
	if (resHandle)
	{
		std::shared_ptr<SoundResourceExtraData> extra =
			std::static_pointer_cast<SoundResourceExtraData>(resHandle->GetExtra());

		/*
			It branches on the sound type, which signifies what kind of sound resource is about to play:
			WAV, MP3, OGG, or MIDI. To extend this system to play any kind of sound format, the new code
			will be hooked in right here, in this case we are looking at WAV data or OGG data that has
			been decompressed.
		*/
		switch (extra->GetSoundType())
		{
			case SOUND_TYPE_OGG:
			case SOUND_TYPE_WAVE:
				// We support WAVs and OGGs
				break;

			case SOUND_TYPE_MP3:
			case SOUND_TYPE_MIDI:
				//If it's a midi file, then do nothin at this time...
				//maybe we will support this in the future
				LogError("MP3s and MIDI are not supported");
				return false;
				break;

			default:
				LogError("Unknown sound type");
				return false;
		}//end switch

		long bytes = resHandle->Size();
		char* array = (char*)resHandle->WritableBuffer();

		SoundBuffer* snd = new SoundBuffer;

		// Check the number of channels... always use 16-bit samples
		if (extra->GetFormat()->nChannels == 1)
			snd->format = AL_FORMAT_MONO16;
		else
			snd->format = AL_FORMAT_STEREO16;

		// The frequency of the sampling rate
		snd->freq = extra->GetFormat()->nSamplesPerSec;

		// Append to end of buffer
		snd->buffer.insert(snd->buffer.end(), array, array + bytes);

		alGenBuffers(1, &snd->bufferId);
		alBufferData(snd->bufferId, snd->format,
			&(snd->buffer[0]), (unsigned int)snd->buffer.size(), snd->freq);

		ALenum error = alGetError();
		if (error != AL_NO_ERROR)
		{
			LogError("Audio: OpenAL error: " +
				std::string(AlErrorString(error)) + "preparing sound buffer");
		}

		AddBuffer(name, snd);
		return true;
	}

	return false;
}

void OpenALSoundManager::UpdateListener(
    const Vector3<float>& pos, const Vector3<float>& vel, 
    const Vector3<float>& at, const Vector3<float>& up)
{
	alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);
	alListener3f(AL_VELOCITY, vel[0], vel[1], vel[2]);
	ALfloat f[6];
	Float3Set(f, at);
	Float3Set(f+3, -up);
	alListenerfv(AL_ORIENTATION, f);
	WarnIfError(alGetError(), "UpdateListener");
}

void OpenALSoundManager::SetListenerGain(float gain)
{
	alListenerf(AL_GAIN, gain);
}

int OpenALSoundManager::PlaySoundGlobal(const std::string& name, bool loop, float volume, float fade, float pitch)
{
	Maintain();
	if (name.empty())
		return 0;
	SoundBuffer* buf = GetFetchBuffer(name);
	if(!buf)
    {
		LogWarning("OpenALSoundManager: \"" + name + "\" not found.");
		return -1;
	}
	int handle = -1;
	if (fade > 0) 
    {
		handle = PlaySoundRaw(buf, loop, 0.0f, pitch);
		FadeSound(handle, fade, volume);
	} 
    else 
    {
		handle = PlaySoundRaw(buf, loop, volume, pitch);
	}
	return handle;
}

int OpenALSoundManager::PlaySoundAt(const std::string& name, bool loop,
    float volume, Vector3<float> pos, float pitch)
{
	Maintain();
	if (name.empty())
		return 0;
	SoundBuffer* buf = GetFetchBuffer(name);
	if(!buf)
    {
		LogWarning("OpenALSoundManager: \"" + name + "\" not found.");
		return -1;
	}
	return PlaySoundRawAt(buf, loop, volume, pos, pitch);
}

void OpenALSoundManager::StopSound(int sound)
{
	Maintain();
	DeleteSound(sound);
}

void OpenALSoundManager::FadeSound(int soundId, float step, float gain)
{
	// Ignore the command if step isn't valid.
	if (step == 0)
		return;
	float currentGain = GetSoundGain(soundId);
	step = gain - currentGain > 0 ? abs(step) : -abs(step);
	if (mSoundsFading.find(soundId) != mSoundsFading.end()) 
    {
		auto current_fade = mSoundsFading[soundId];
		// Do not replace the fade if it's equivalent.
		if (current_fade.targetGain == gain && current_fade.step == step)
			return;
		mSoundsFading.erase(soundId);
	}
	gain = std::clamp(gain, 0.f, 1.f);
	mSoundsFading[soundId] = FadeState(step, currentGain, gain);
}

void OpenALSoundManager::DoFades(float dTime)
{
	for (auto it = mSoundsFading.begin(); it != mSoundsFading.end();)
    {
		FadeState& fade = it->second;
		assert(fade.step != 0);
		fade.currentGain += (fade.step * dTime);

		if (fade.step < 0.f)
			fade.currentGain = std::max(fade.currentGain, fade.targetGain);
		else
			fade.currentGain = std::min(fade.currentGain, fade.targetGain);

		if (fade.currentGain <= 0.f)
			StopSound(it->first);
		else
			UpdateSoundGain(it->first, fade.currentGain);

		// The increment must happen during the erase call, or else it'll segfault.
		if (fade.currentGain == fade.targetGain)
			mSoundsFading.erase(it++);
		else
			it++;
	}
}

bool OpenALSoundManager::SoundExists(int sound)
{
	Maintain();
	return (mSoundsPlaying.count(sound) != 0);
}

void OpenALSoundManager::UpdateSoundPosition(int id, Vector3<float> pos)
{
	auto i = mSoundsPlaying.find(id);
	if (i == mSoundsPlaying.end())
		return;
	PlayingSound* sound = i->second;

	alSourcei(sound->sourceId, AL_SOURCE_RELATIVE, false);
	alSource3f(sound->sourceId, AL_POSITION, pos[0], pos[1], pos[2]);
	alSource3f(sound->sourceId, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	alSourcef(sound->sourceId, AL_REFERENCE_DISTANCE, 10.0f);
}

bool OpenALSoundManager::UpdateSoundGain(int id, float gain)
{
	auto i = mSoundsPlaying.find(id);
	if (i == mSoundsPlaying.end())
		return false;

	PlayingSound* sound = i->second;
	alSourcef(sound->sourceId, AL_GAIN, gain);
	return true;
}

float OpenALSoundManager::GetSoundGain(int id)
{
	auto i = mSoundsPlaying.find(id);
	if (i == mSoundsPlaying.end())
		return 0;

	PlayingSound* sound = i->second;
	ALfloat gain;
	alGetSourcef(sound->sourceId, AL_GAIN, &gain);
	return gain;
}

float OpenALSoundManager::GetSoundProgress(int id)
{
	auto i = mSoundsPlaying.find(id);
	if (i == mSoundsPlaying.end())
		return 0;

	PlayingSound* sound = i->second;
	ALfloat offset, size;
	alGetSourcef(sound->sourceId, AL_BYTE_OFFSET, &offset);
	alGetSourcef(sound->sourceId, AL_SIZE, &size);
	
	return offset/size;
}