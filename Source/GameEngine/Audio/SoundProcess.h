/////////////////////////////////////////////////////////////////////////////
// File SoundProcess.h
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

#ifndef SOUNDPROCESS_H
#define SOUNDPROCESS_H

#include "GameEngineStd.h"

#include "SoundResource.h"

#include "Core/Process/Process.h"

/*
	SoundProcess
	This class provides a single object that manages individual sounds. Many of the methods 
	are re-implementations of some BaseAudioBuffer methods. This class can be used to manage 
	timing between sounds & animations.
	The parameters to initialize this object are a ResHandle and initial sound settings. 
	Every process has a type, and sound processes use this to distinguish themselves 
	into sound categories such as sound effects, music, ambient background effects, or 
	speech. This creates an easy way for a game to turn off or change the volume level
	of a particular type of sound.
*/
class SoundProcess : public Process
{
protected:

	// these hold the initial setting until the sound is actually launched.
	int mHandle;
	std::string mSound;

    bool mIsLooping;
	float mVolume;

public:
	SoundProcess(std::string sound, float volume = 100.f, bool looping=false);
	virtual ~SoundProcess();

    void Play(const float volume, const bool looping);
    void Stop();

    void SetVolume(float volume);
    float GetVolume();
	bool IsSoundValid() { return mHandle != -1; }
    bool IsPlaying();
	bool IsLooping() { return mIsLooping; }
    float GetProgress();

protected:
	virtual void OnInit();
    virtual void OnUpdate(unsigned long deltaMs);

    void InitializeVolume();
};

/////////////////////////////////////////////////////////////////////////////
// class ExplosionProcess						- Chapter 13, page 433
//
// This is an example of a process that uses a simple state machine
// to control itself. 
//
/////////////////////////////////////////////////////////////////////////////
class ExplosionProcess : public Process
{
protected:
    int mStage;
	std::shared_ptr<SoundProcess> mSound;

  public:
  	ExplosionProcess() { mStage=0; }

protected:   
    virtual void OnInit();
  	virtual void OnUpdate(unsigned long deltaMs);
};

/////////////////////////////////////////////////////////////////////////////
// class FadeProcess							- Chapter 13, page 435
//
// Fades sound volume in or out over time and then kills itself.
// This should be useful for groups of sound effects, too - such as when
//   an AI barks and it must be heard above the other effects like too much freaking thunder.
//
/////////////////////////////////////////////////////////////////////////////
class FadeProcess : public Process
{
protected:
	std::shared_ptr<SoundProcess> mSound;
	
	int mTotalFadeTime;
	int mElapsedTime;
	float mStartVolume;
	float mEndVolume;

public:
	FadeProcess(std::shared_ptr<SoundProcess> sound, int fadeTime, float endVolume);
	virtual void OnUpdate(unsigned long deltaMs);
};


#endif