 
//========================================================================
// AudioComponent.cpp : A component for attaching sounds to an actor
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


#include "AudioComponent.h"

#include "Application/GameApplication.h"

#include "Audio/SoundProcess.h"

const char* AudioComponent::Name = "AudioComponent";

AudioComponent::AudioComponent()
{
	mPlaySound = false;
	mLooping = false;
	mFadeTime = 0;
	mVolume = 80.f;
}

bool AudioComponent::Init(tinyxml2::XMLElement* pData)
{
	tinyxml2::XMLElement* pAudio = pData->FirstChildElement("Sound");
	if (pAudio)
	{
		std::string audios = Trim(pAudio->FirstChild()->Value());
		for (auto const& audio : StringSplit(audios, ','))
			mAudios.push_back(audio);
	}

	tinyxml2::XMLElement* pLooping = pData->FirstChildElement("Looping");
	if (pLooping)
	{
		std::string value = pLooping->FirstChild()->Value();
		mLooping = (value == "0") ? false : true;
	}

	tinyxml2::XMLElement* pPlaySound = pData->FirstChildElement("PlaySound");
	if (pPlaySound)
	{
		std::string value = pPlaySound->FirstChild()->Value();
		mPlaySound = (value == "0") ? false : true;
	}

	tinyxml2::XMLElement* pFadeIn = pData->FirstChildElement("FadeIn");
	if (pFadeIn)
	{
		std::string value = pFadeIn->FirstChild()->Value();
		mFadeTime = atoi(value.c_str());
	}

	tinyxml2::XMLElement* pVolume = pData->FirstChildElement("Volume");
	if (pVolume)
	{
		std::string value = pVolume->FirstChild()->Value();
		mVolume = (float)atof(value.c_str());
	}

	return true;
}

tinyxml2::XMLElement* AudioComponent::GenerateXml(void)
{
	tinyxml2::XMLDocument doc;

	// base element
	tinyxml2::XMLElement* pBaseElement = doc.NewElement(GetName());

	tinyxml2::XMLElement* pScound = doc.NewElement("Sound");
	for (std::string audio : mAudios)
	{
		tinyxml2::XMLText* pAudioText = doc.NewText(audio.c_str());
		pScound->LinkEndChild(pAudioText);
	}
	pBaseElement->LinkEndChild(pScound);

	tinyxml2::XMLElement* pLoopingNode = doc.NewElement("Looping");
	tinyxml2::XMLText* pLoopingText = doc.NewText(mLooping ? "1" : "0");
    pLoopingNode->LinkEndChild(pLoopingText);
    pBaseElement->LinkEndChild(pLoopingNode);

	tinyxml2::XMLElement* pPlaySoundNode = doc.NewElement("PlaySound");
	tinyxml2::XMLText* pPlaySoundText = doc.NewText(mPlaySound ? "1" : "0");
	pLoopingNode->LinkEndChild(pPlaySoundText);
	pBaseElement->LinkEndChild(pPlaySoundNode);

	tinyxml2::XMLElement* pFadeInNode = doc.NewElement("FadeIn");
	tinyxml2::XMLText* pFadeInText = doc.NewText(std::to_string(mFadeTime).c_str());
    pFadeInNode->LinkEndChild(pFadeInText);
    pBaseElement->LinkEndChild(pFadeInNode);

	tinyxml2::XMLElement* pVolumeNode = doc.NewElement("Volume");
	tinyxml2::XMLText* pVolumeText = doc.NewText(std::to_string(mVolume).c_str());
    pVolumeNode->LinkEndChild(pVolumeText);
    pBaseElement->LinkEndChild(pVolumeNode);

	return pBaseElement;
}

void AudioComponent::PostInit()
{
	HumanView *humanView = GameApplication::Get()->GetHumanView();
	if (!humanView)
	{
		LogError("Sounds need a human view to be heard!");
		return;
	}

	ProcessManager *processManager = humanView->GetProcessManager();
	if (!processManager)
	{
		LogError("Sounds need a process manager to attach!");
		return;
	}

	if (!GameApplication::Get()->IsEditorRunning())
	{
		// The editor can play sounds, but it shouldn't run them when AudioComponents are initialized.
		if (mPlaySound)
		{
			for (std::string audio : mAudios)
			{
				std::shared_ptr<SoundProcess> sound(new SoundProcess(audio, mVolume, mLooping));
				processManager->AttachProcess(sound);

				// fade process
				if (mFadeTime > 0)
				{
					std::shared_ptr<FadeProcess> fadeProc(new FadeProcess(sound, mFadeTime, mVolume));
					processManager->AttachProcess(fadeProc);
				}
			}
		}
	}
}