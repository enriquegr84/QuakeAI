//========================================================================
// SoundOpenal.h : Defines the openal sound system.
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


#ifndef SOUNDOPENAL_H
#define SOUNDOPENAL_H

#if defined(_WIN32)
    #include <AL/al.h>
    #include <AL/alc.h>
    //#include <AL/alext.h>
#elif defined(__APPLE__)
    #define OPENAL_DEPRECATED
    #include <OpenAL/al.h>
    #include <OpenAL/alc.h>
    //#include <OpenAL/alext.h>
#else
    #include <AL/al.h>
    #include <AL/alc.h>
    #include <AL/alext.h>
#endif

#include <cmath>
#include <vorbis/vorbisfile.h>
#include <cassert>

#include "GameEngineStd.h"

#include "Sound.h"

struct PlayingSound
{
    ALuint sourceId;
    bool loop;
};

struct BufferSource
{
    const char* buf;
    size_t curOffset;
    size_t len;
};

struct SoundBuffer
{
    ALenum format;
    ALsizei freq;
    ALuint bufferId;
    std::vector<char> buffer;
};

typedef std::unique_ptr<ALCdevice, void(*)(ALCdevice* p)> unique_ptr_alcdevice;
typedef std::unique_ptr<ALCcontext, void(*)(ALCcontext* p)> unique_ptr_alccontext;

class OpenALSoundSystem : public SoundSystem
{
public:
    unique_ptr_alcdevice  mDevice;
    unique_ptr_alccontext mContext;

public:
    OpenALSoundSystem();

    ~OpenALSoundSystem();

    virtual bool Init();
};

class OpenALSoundManager : public BaseSoundManager
{
public:
    OpenALSoundManager(OpenALSoundSystem* ss, OnDemandSoundFetcher* fetcher);

    ~OpenALSoundManager();

    void Step(float dTime);

    void AddBuffer(const std::string& name, SoundBuffer* buf);

    SoundBuffer* GetBuffer(const std::string& name);

    PlayingSound* CreatePlayingSound(SoundBuffer* buf, bool loop, float volume, float pitch);
    PlayingSound* CreatePlayingSoundAt(SoundBuffer* buf, bool loop, float volume, Vector3<float> pos, float pitch);

    int PlaySoundRaw(SoundBuffer* buf, bool loop, float volume, float pitch);
    int PlaySoundRawAt(SoundBuffer* buf, bool loop, float volume, const Vector3<float>& pos, float pitch);

    void DeleteSound(int id);

    /* If buffer does not exist, consult the fetcher */
    SoundBuffer* GetFetchBuffer(const std::string& name);

    // Remove stopped sounds
    void Maintain();

    /* Interface */
    bool LoadSound(const std::string& name, const std::string& filepath);

    void UpdateListener(
        const Vector3<float>& pos, const Vector3<float>& vel,
        const Vector3<float>& at, const Vector3<float>& up);

    void SetListenerGain(float gain);

    int PlaySoundGlobal(const std::string& name, bool loop, float volume, float fade, float pitch);
    int PlaySoundAt(const std::string& name, bool loop,
        float volume, Vector3<float> pos, float pitch);

    void StopSound(int sound);
    void FadeSound(int soundId, float step, float gain);

    void DoFades(float deltaMs);

    bool SoundExists(int sound);

    void UpdateSoundPosition(int id, Vector3<float> pos);
    bool UpdateSoundGain(int id, float gain);

    float GetSoundGain(int id);
    float GetSoundProgress(int id);

private:
    OnDemandSoundFetcher* mFetcher;
    ALCdevice* mDevice;
    ALCcontext* mContext;
    int mNextId;
    std::unordered_map<std::string, std::vector<SoundBuffer*>> mBuffers;
    std::unordered_map<int, PlayingSound*> mSoundsPlaying;
    struct FadeState
    {
        FadeState() = default;

        FadeState(float step, float currentGain, float targetGain) :
            step(step), currentGain(currentGain), targetGain(targetGain) {}
        float step;
        float currentGain;
        float targetGain;
    };

    std::unordered_map<int, FadeState> mSoundsFading;

};

#endif